#include "BlockSpline.h"
#include "PropertyVisualizer.h"
#include "SightPortalBlockManager.h"
#include "SightPortalZoneManager.h"
#include "SightPortalSiteManager.h"
#include "SightPortalConnector.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"

ABlockSpline::ABlockSpline()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create the spline component and establish it as the root component
    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    if (SplineComponent)
    {
        SetRootComponent(SplineComponent);
    }
    
    VisualizerSpacing = 350.0f;
    VisualizerRotationOffset = FRotator::ZeroRotator;
    VisualizerScaleOffset = FVector(1.0f, 1.0f, 1.0f);
    bAutoManageSplinePoints = false;
    PropertyCount = 3;
    BlockName = TEXT("1");
    ZoneName = TEXT("1");
    StartingDoorNumber = 1;
}

void ABlockSpline::BeginPlay()
{
    Super::BeginPlay();

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.AddUniqueDynamic(this, &ABlockSpline::HandleDataReceived);
        Connector->OnPropertyUpdated.AddUniqueDynamic(this, &ABlockSpline::HandleSinglePropertyUpdated);
    }
}

void ABlockSpline::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPropertyVisualizers();

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.RemoveDynamic(this, &ABlockSpline::HandleDataReceived);
        Connector->OnPropertyUpdated.RemoveDynamic(this, &ABlockSpline::HandleSinglePropertyUpdated);
    }

    Super::EndPlay(EndPlayReason);
}

void ABlockSpline::HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio)
{
    OnPortalDataReceived(PropertyPortfolio);
}

void ABlockSpline::HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData)
{
    // Check if the property belongs to this block/spline
    if (PropertyData.Block.Equals(BlockName, ESearchCase::IgnoreCase) ||
        PropertyName.StartsWith(BlockName, ESearchCase::IgnoreCase))
    {
        OnPortalPropertyUpdated(PropertyName, PropertyData);
    }
}

void ABlockSpline::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!SplineComponent) return;

    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);

    int32 AttachedPropertyCount = 0;
    for (AActor* Child : AttachedActors)
    {
        if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
        {
            AttachedPropertyCount++;
        }
    }

    if (bAutoManageSplinePoints && AttachedPropertyCount > 0)
    {
        float TargetSplineLength = AttachedPropertyCount * VisualizerSpacing;
        SplineComponent->ClearSplinePoints(false);
        SplineComponent->AddSplinePoint(FVector(0.f, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
        SplineComponent->AddSplinePoint(FVector(TargetSplineLength, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
        SplineComponent->UpdateSpline();
    }

    int32 Index = 0;
    for (AActor* Child : AttachedActors)
    {
        if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
        {
            APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Child);
            if (PropVis)
            {
                float Distance = Index * VisualizerSpacing;
                FVector SpawnLoc = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator SpawnRot = SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

                if (!PropVis->bHasBeenManuallyMoved)
                {
                    PropVis->SetActorLocationAndRotation(SpawnLoc, SpawnRot + VisualizerRotationOffset);
                    PropVis->SetActorScale3D(VisualizerScaleOffset);
                }
                else
                {
                    FTransform DefaultTransform(SpawnRot + VisualizerRotationOffset, SpawnLoc, VisualizerScaleOffset);
                    FTransform FinalTransform = PropVis->ManualRelativeTransform * DefaultTransform;
                    PropVis->SetActorTransform(FinalTransform);
                }
            }
            Index++;
        }
    }
}

void ABlockSpline::ClearActiveSpawnedActors()
{
    ClearPropertyVisualizers();
}

void ABlockSpline::ClearProperties()
{
    ClearPropertyVisualizers();
}

void ABlockSpline::ClearPropertyVisualizers()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockSpline] Clearing property visualizers..."));

    // Recover Site Manager reference for unregistering
    ASightPortalSiteManager* SiteManager = nullptr;
    AActor* CurrentParent = GetAttachParentActor();
    while (CurrentParent)
    {
        if (CurrentParent->IsA(ASightPortalSiteManager::StaticClass()))
        {
            SiteManager = Cast<ASightPortalSiteManager>(CurrentParent);
            break;
        }
        CurrentParent = CurrentParent->GetAttachParentActor();
    }

    // Destroy all tracked visualizers
    for (AActor* VisActor : SpawnedVisualizers)
    {
        if (IsValid(VisActor))
        {
            if (SiteManager)
            {
                APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(VisActor);
                if (PropVis)
                {
                    SiteManager->UnregisterPropertyVisualizer(PropVis->PropertyDetails.Name);
                }
            }
            VisActor->Destroy();
        }
    }
    SpawnedVisualizers.Empty();

    // Also scan any attached visualizers that may not be in SpawnedVisualizers array
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    for (AActor* Child : AttachedActors)
    {
        if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
        {
            if (SiteManager)
            {
                APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Child);
                if (PropVis)
                {
                    SiteManager->UnregisterPropertyVisualizer(PropVis->PropertyDetails.Name);
                }
            }
            Child->Destroy();
        }
    }
}

void ABlockSpline::SpawnProperties()
{
    SpawnPropertyVisualizers();
}

void ABlockSpline::SpawnPropertyVisualizers()
{
    if (bIsSpawning)
    {
        return;
    }

    bIsSpawning = true;

    UWorld* World = GetWorld();
    if (!World || !SplineComponent)
    {
        bIsSpawning = false;
        return;
    }

    // Auto-discover parent SiteManager, ZoneManager, and BlockManager
    ASightPortalSiteManager* SiteManager = nullptr;
    AActor* CurrentParent = GetAttachParentActor();
    while (CurrentParent)
    {
        if (CurrentParent->IsA(ASightPortalSiteManager::StaticClass()))
        {
            SiteManager = Cast<ASightPortalSiteManager>(CurrentParent);
            break;
        }
        CurrentParent = CurrentParent->GetAttachParentActor();
    }

    AActor* DirectParent = GetAttachParentActor();
    if (DirectParent)
    {
        if (DirectParent->IsA(ASightPortalBlockManager::StaticClass()))
        {
            ASightPortalBlockManager* ParentBlock = Cast<ASightPortalBlockManager>(DirectParent);
            BlockName = ParentBlock->BlockName;
            AActor* ParentZone = ParentBlock->GetAttachParentActor();
            if (ParentZone && ParentZone->IsA(ASightPortalZoneManager::StaticClass()))
            {
                ZoneName = Cast<ASightPortalZoneManager>(ParentZone)->ZoneName;
            }
        }
        else if (DirectParent->IsA(ASightPortalZoneManager::StaticClass()))
        {
            ZoneName = Cast<ASightPortalZoneManager>(DirectParent)->ZoneName;
        }
    }

    // Clean up invalid references from SpawnedVisualizers
    for (int32 i = SpawnedVisualizers.Num() - 1; i >= 0; --i)
    {
        if (!IsValid(SpawnedVisualizers[i]))
        {
            SpawnedVisualizers.RemoveAt(i);
        }
    }

    // Destroy excess visualizers if PropertyCount has been reduced
    while (SpawnedVisualizers.Num() > PropertyCount)
    {
        AActor* Excess = SpawnedVisualizers.Last();
        if (IsValid(Excess))
        {
            if (SiteManager)
            {
                APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Excess);
                if (PropVis)
                {
                    SiteManager->UnregisterPropertyVisualizer(PropVis->PropertyDetails.Name);
                }
            }
            Excess->Destroy();
        }
        SpawnedVisualizers.RemoveAt(SpawnedVisualizers.Num() - 1);
    }

    // Ensure array size matches PropertyCount
    if (SpawnedVisualizers.Num() != PropertyCount)
    {
        SpawnedVisualizers.SetNum(PropertyCount);
    }

    if (bAutoManageSplinePoints && PropertyCount > 0)
    {
        float TargetSplineLength = PropertyCount * VisualizerSpacing;
        SplineComponent->ClearSplinePoints(false);
        SplineComponent->AddSplinePoint(FVector(0.f, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
        SplineComponent->AddSplinePoint(FVector(TargetSplineLength, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
        SplineComponent->UpdateSpline();
    }

    // Recover cached properties from central connector
    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    TArray<FSightPortalProperty> MatchedProperties;
    if (Connector)
    {
        for (const FSightPortalProperty& Prop : Connector->CachedProperties)
        {
            if (Prop.Name.StartsWith(BlockName, ESearchCase::IgnoreCase))
            {
                MatchedProperties.Add(Prop);
            }
        }
    }

    for (int32 i = 0; i < PropertyCount; ++i)
    {
        float Distance = i * VisualizerSpacing;
        FVector SpawnLoc = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator SpawnRot = SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + VisualizerRotationOffset;

        int32 DoorNum = StartingDoorNumber + i;
        FString ExpectedName = FString::Printf(TEXT("%s%d"), *BlockName, DoorNum);

        // Determine unique real-estate property data to load
        FSightPortalProperty AssignedProperty;
        bool bFoundMatch = false;

        if (Connector)
        {
            // 1. Exact match by Name
            for (const FSightPortalProperty& Prop : Connector->CachedProperties)
            {
                if (Prop.Name.Equals(ExpectedName, ESearchCase::IgnoreCase))
                {
                    AssignedProperty = Prop;
                    bFoundMatch = true;
                    break;
                }
            }

            // 2. Match by Zone, Block, and DoorNo
            if (!bFoundMatch)
            {
                for (const FSightPortalProperty& Prop : Connector->CachedProperties)
                {
                    if (Prop.Zone.Equals(ZoneName, ESearchCase::IgnoreCase) &&
                        Prop.Block.Equals(BlockName, ESearchCase::IgnoreCase) &&
                        Prop.DoorNo == DoorNum)
                    {
                        AssignedProperty = Prop;
                        bFoundMatch = true;
                        break;
                    }
                }
            }

            // 3. Fallback to matched properties for this block
            if (!bFoundMatch && MatchedProperties.Num() > 0)
            {
                AssignedProperty = MatchedProperties[i % MatchedProperties.Num()];
                bFoundMatch = true;
            }

            // 4. Global index fallback
            if (!bFoundMatch && Connector->CachedProperties.Num() > 0)
            {
                AssignedProperty = Connector->CachedProperties[i % Connector->CachedProperties.Num()];
                bFoundMatch = true;
            }
        }

        // 5. Generate clean fallback if not found in cache
        if (!bFoundMatch)
        {
            AssignedProperty.Name = ExpectedName;
            AssignedProperty.Zone = ZoneName;
            AssignedProperty.Block = BlockName;
            AssignedProperty.DoorNo = DoorNum;
            AssignedProperty.Price = 250000.0f + (i * 15000.0f);
            AssignedProperty.Surface = 120.0f + (i * 10.0f);
            AssignedProperty.Availability = TEXT("Available");
        }

        // Determine the target class to use (check per-index override first, then spline default)
        TSubclassOf<APropertyVisualizer> VisualizerClass = PropertyVisualizerClass;
        if (PropertyVisualizerOverrides.Contains(i) && PropertyVisualizerOverrides[i])
        {
            VisualizerClass = PropertyVisualizerOverrides[i];
        }
        if (!VisualizerClass)
        {
            VisualizerClass = APropertyVisualizer::StaticClass();
        }

        APropertyVisualizer* PropertyVis = Cast<APropertyVisualizer>(SpawnedVisualizers[i]);
        
        // If actor is invalid OR if actor's class does not match the desired VisualizerClass, respawn it
        if (!IsValid(PropertyVis) || PropertyVis->GetClass() != VisualizerClass)
        {
            bool bHadManualMove = false;
            FTransform PrevManualTransform = FTransform::Identity;
            if (IsValid(PropertyVis))
            {
                bHadManualMove = PropertyVis->bHasBeenManuallyMoved;
                PrevManualTransform = PropertyVis->ManualRelativeTransform;
                if (SiteManager)
                {
                    SiteManager->UnregisterPropertyVisualizer(PropertyVis->PropertyDetails.Name);
                }
                PropertyVis->Destroy();
            }

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            PropertyVis = World->SpawnActor<APropertyVisualizer>(VisualizerClass, SpawnLoc, SpawnRot, SpawnParams);
            if (PropertyVis)
            {
                PropertyVis->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                PropertyVis->bHasBeenManuallyMoved = bHadManualMove;
                PropertyVis->ManualRelativeTransform = PrevManualTransform;
                SpawnedVisualizers[i] = PropertyVis;
            }
        }

        if (PropertyVis)
        {
            if (!PropertyVis->bHasBeenManuallyMoved)
            {
                PropertyVis->SetActorLocationAndRotation(SpawnLoc, SpawnRot);
                PropertyVis->SetActorScale3D(VisualizerScaleOffset);
            }
            else
            {
                FTransform DefaultTransform(SpawnRot, SpawnLoc, VisualizerScaleOffset);
                FTransform FinalTransform = PropertyVis->ManualRelativeTransform * DefaultTransform;
                PropertyVis->SetActorTransform(FinalTransform);
            }

            PropertyVis->SetPropertyDetails(AssignedProperty);

#if WITH_EDITOR
            PropertyVis->SetActorLabel(AssignedProperty.Name);
#endif

            // Register in Site Manager if present
            if (SiteManager)
            {
                SiteManager->RegisterPropertyVisualizer(AssignedProperty.Name, PropertyVis);
            }
        }
    }

    bIsSpawning = false;
}

void ABlockSpline::ApplyVisualizerClassOverride()
{
    if (!NewVisualizerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockSpline] Cannot apply visualizer override: NewVisualizerClass is null."));
        return;
    }

    ChangeVisualizerClassAtIndex(TargetVisualizerIndex, NewVisualizerClass);
}

APropertyVisualizer* ABlockSpline::ChangeVisualizerClassAtIndex(int32 VisualizerIndex, TSubclassOf<APropertyVisualizer> InNewClass)
{
    if (!InNewClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockSpline] ChangeVisualizerClassAtIndex failed: InNewClass is null."));
        return nullptr;
    }

    if (VisualizerIndex < 0 || VisualizerIndex >= PropertyCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockSpline] ChangeVisualizerClassAtIndex: Invalid VisualizerIndex %d (PropertyCount: %d)"), VisualizerIndex, PropertyCount);
        return nullptr;
    }

    // Save override in spline configuration map
    PropertyVisualizerOverrides.Add(VisualizerIndex, InNewClass);

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Discover Site Manager
    ASightPortalSiteManager* SiteManager = nullptr;
    AActor* CurrentParent = GetAttachParentActor();
    while (CurrentParent)
    {
        if (CurrentParent->IsA(ASightPortalSiteManager::StaticClass()))
        {
            SiteManager = Cast<ASightPortalSiteManager>(CurrentParent);
            break;
        }
        CurrentParent = CurrentParent->GetAttachParentActor();
    }

    APropertyVisualizer* OldVis = nullptr;
    if (SpawnedVisualizers.IsValidIndex(VisualizerIndex) && IsValid(SpawnedVisualizers[VisualizerIndex]))
    {
        OldVis = Cast<APropertyVisualizer>(SpawnedVisualizers[VisualizerIndex]);
    }

    FSightPortalProperty OldPropertyData;
    FTransform OldTransform = GetActorTransform();
    bool bWasManuallyMoved = false;
    FTransform OldManualRelTransform = FTransform::Identity;
    FString OldLabel = TEXT("");

    if (OldVis)
    {
        OldPropertyData = OldVis->PropertyDetails;
        OldTransform = OldVis->GetActorTransform();
        bWasManuallyMoved = OldVis->bHasBeenManuallyMoved;
        OldManualRelTransform = OldVis->ManualRelativeTransform;
#if WITH_EDITOR
        OldLabel = OldVis->GetActorLabel();
#endif
        if (SiteManager)
        {
            SiteManager->UnregisterPropertyVisualizer(OldVis->PropertyDetails.Name);
        }
        OldVis->Destroy();
    }
    else if (SplineComponent)
    {
        float Distance = VisualizerIndex * VisualizerSpacing;
        FVector SpawnLoc = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        FRotator SpawnRot = SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + VisualizerRotationOffset;
        OldTransform = FTransform(SpawnRot, SpawnLoc, VisualizerScaleOffset);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APropertyVisualizer* NewVis = World->SpawnActor<APropertyVisualizer>(InNewClass, OldTransform, SpawnParams);
    if (NewVis)
    {
        NewVis->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
        NewVis->bHasBeenManuallyMoved = bWasManuallyMoved;
        NewVis->ManualRelativeTransform = OldManualRelTransform;
        NewVis->SetActorTransform(OldTransform);
        NewVis->SetPropertyDetails(OldPropertyData);

#if WITH_EDITOR
        if (!OldLabel.IsEmpty())
        {
            NewVis->SetActorLabel(OldLabel);
        }
#endif

        if (SpawnedVisualizers.IsValidIndex(VisualizerIndex))
        {
            SpawnedVisualizers[VisualizerIndex] = NewVis;
        }
        else
        {
            SpawnedVisualizers.SetNum(PropertyCount);
            SpawnedVisualizers[VisualizerIndex] = NewVis;
        }

        if (SiteManager && !OldPropertyData.Name.IsEmpty())
        {
            SiteManager->RegisterPropertyVisualizer(OldPropertyData.Name, NewVis);
        }

        UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockSpline] Successfully replaced visualizer at Index %d with class %s"), VisualizerIndex, *InNewClass->GetName());
    }

    return NewVis;
}

APropertyVisualizer* ABlockSpline::ChangeVisualizerClassForProperty(const FString& PropertyName, TSubclassOf<APropertyVisualizer> InNewClass)
{
    if (!InNewClass || PropertyName.IsEmpty())
    {
        return nullptr;
    }

    for (int32 VisIdx = 0; VisIdx < SpawnedVisualizers.Num(); ++VisIdx)
    {
        APropertyVisualizer* Vis = Cast<APropertyVisualizer>(SpawnedVisualizers[VisIdx]);
        if (IsValid(Vis) && Vis->PropertyDetails.Name.Equals(PropertyName, ESearchCase::IgnoreCase))
        {
            return ChangeVisualizerClassAtIndex(VisIdx, InNewClass);
        }
    }

    return nullptr;
}

#if WITH_EDITOR
void ABlockSpline::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;
}
#endif
