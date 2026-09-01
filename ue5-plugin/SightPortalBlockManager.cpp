#include "SightPortalBlockManager.h"
#include "SightPortalZoneManager.h"
#include "SightPortalSiteManager.h"
#include "SightPortalConnector.h"
#include "BlockSpline.h"
#include "PropertyVisualizer.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"

ASightPortalBlockManager::ASightPortalBlockManager()
{
    PrimaryActorTick.bCanEverTick = false;
    SpawnOffset = FVector::ZeroVector;
    RowSpacing = 500.0f;
    PropertyRowCount = 1;
    BlockName = TEXT("1");

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void ASightPortalBlockManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (PropertyRowCount < 0)
    {
        PropertyRowCount = 0;
    }

    if (PropertyRowDetails.Num() != PropertyRowCount)
    {
        PropertyRowDetails.SetNum(PropertyRowCount);
    }

    // Call SpawnPropertyVisualizers to allow live interactive updates of properties in real time
    //SpawnPropertyVisualizers();
}

void ASightPortalBlockManager::BeginPlay()
{
    Super::BeginPlay();

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.AddUniqueDynamic(this, &ASightPortalBlockManager::HandleDataReceived);
        Connector->OnPropertyUpdated.AddUniqueDynamic(this, &ASightPortalBlockManager::HandleSinglePropertyUpdated);
    }
}

void ASightPortalBlockManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearActiveSpawnedActors();

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.RemoveDynamic(this, &ASightPortalBlockManager::HandleDataReceived);
        Connector->OnPropertyUpdated.RemoveDynamic(this, &ASightPortalBlockManager::HandleSinglePropertyUpdated);
    }

    Super::EndPlay(EndPlayReason);
}

void ASightPortalBlockManager::HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio)
{
    OnPortalDataReceived(PropertyPortfolio);
}

void ASightPortalBlockManager::HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData)
{
    // Check if the property belongs to this block
    if (PropertyData.Block.Equals(BlockName, ESearchCase::IgnoreCase) ||
        PropertyName.StartsWith(BlockName, ESearchCase::IgnoreCase))
    {
        OnPortalPropertyUpdated(PropertyName, PropertyData);
    }
}

void ASightPortalBlockManager::ClearActiveSpawnedActors()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Clearing all spawned visualizers and splines..."));

    ClearPropertyVisualizers();

    // Destroy all attached child splines
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ABlockSpline::StaticClass()))
        {
            Attached->Destroy();
        }
    }
    ActiveSpawnedActors.Empty();
}

void ASightPortalBlockManager::ClearPropertyVisualizers()
{
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

    // Clear tracked visualizers
    for (FPropertyBlockRowDetails& Row : PropertyRowDetails)
    {
        for (AActor* Vis : Row.SpawnedVisualizers)
        {
            if (IsValid(Vis))
            {
                if (SiteManager)
                {
                    APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Vis);
                    if (PropVis)
                    {
                        SiteManager->UnregisterPropertyVisualizer(PropVis->PropertyDetails.Name);
                    }
                }
                Vis->Destroy();
            }
        }
        Row.SpawnedVisualizers.Empty();
    }

    // Also scan for any untracked child visualizers on splines or on us
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached))
        {
            if (Attached->IsA(APropertyVisualizer::StaticClass()))
            {
                if (SiteManager)
                {
                    APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Attached);
                    if (PropVis)
                    {
                        SiteManager->UnregisterPropertyVisualizer(PropVis->PropertyDetails.Name);
                    }
                }
                Attached->Destroy();
            }
            else if (Attached->IsA(ABlockSpline::StaticClass()))
            {
                TArray<AActor*> SplineAttached;
                Attached->GetAttachedActors(SplineAttached);
                for (AActor* SplineChild : SplineAttached)
                {
                    if (IsValid(SplineChild) && SplineChild->IsA(APropertyVisualizer::StaticClass()))
                    {
                        if (SiteManager)
                        {
                            APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(SplineChild);
                            if (PropVis)
                            {
                                SiteManager->UnregisterPropertyVisualizer(PropVis->PropertyDetails.Name);
                            }
                        }
                        SplineChild->Destroy();
                    }
                }
            }
        }
    }
}

void ASightPortalBlockManager::SpawnRowsOfProperties()
{
    // Redirect legacy call to modern function
    SpawnPropertyVisualizers();
}

void ASightPortalBlockManager::SpawnPropertyVisualizers()
{
    if (bIsSpawning)
    {
        return;
    }

    bIsSpawning = true;

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsSpawning = false;
        return;
    }

    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    // Recover Site Manager reference
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

    // Recover Zone Name
    FString ZoneName = TEXT("1");
    AActor* ParentZone = GetAttachParentActor();
    if (ParentZone && ParentZone->IsA(ASightPortalZoneManager::StaticClass()))
    {
        ZoneName = Cast<ASightPortalZoneManager>(ParentZone)->ZoneName;
    }

    // Recover properties from central cache
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

    // Scan attached child splines to recover existing splines
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);

    TArray<ABlockSpline*> FoundSplines;
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ABlockSpline::StaticClass()))
        {
            FoundSplines.Add(Cast<ABlockSpline>(Attached));
        }
    }

    // Destroy any excess splines (e.g. if PropertyRowCount was decreased in Editor)
    while (FoundSplines.Num() > PropertyRowDetails.Num())
    {
        ABlockSpline* ExcessSpline = FoundSplines.Last();
        if (IsValid(ExcessSpline))
        {
            TArray<AActor*> SplineAttached;
            ExcessSpline->GetAttachedActors(SplineAttached);
            for (AActor* SplineChild : SplineAttached)
            {
                if (IsValid(SplineChild))
                {
                    if (SiteManager)
                    {
                        APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(SplineChild);
                        if (PropVis)
                        {
                            SiteManager->UnregisterPropertyVisualizer(PropVis->PropertyDetails.Name);
                        }
                    }
                    SplineChild->Destroy();
                }
            }
            ExcessSpline->Destroy();
        }
        FoundSplines.RemoveAt(FoundSplines.Num() - 1);
    }

    for (int32 RowIndex = 0; RowIndex < PropertyRowDetails.Num(); ++RowIndex)
    {
        FPropertyBlockRowDetails& Row = PropertyRowDetails[RowIndex];

        FVector SplineLocation = ManagerLocation + (GetActorForwardVector() * (RowIndex * RowSpacing)) + (SpawnOffset * RowIndex);

        ABlockSpline* RowSpline = nullptr;
        if (RowIndex < FoundSplines.Num())
        {
            RowSpline = FoundSplines[RowIndex];
            if (IsValid(RowSpline) && !RowSpline->bHasBeenManuallyMoved)
            {
                RowSpline->SetActorLocation(SplineLocation);
            }
        }
        else
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            TSubclassOf<ABlockSpline> SplineClassToSpawn = BlockSplineClass;
            if (!SplineClassToSpawn)
            {
                SplineClassToSpawn = ABlockSpline::StaticClass();
            }

            RowSpline = World->SpawnActor<ABlockSpline>(SplineClassToSpawn, SplineLocation, ManagerRotation, SpawnParams);
            if (RowSpline)
            {
                RowSpline->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                FoundSplines.Add(RowSpline);
                ActiveSpawnedActors.Add(RowSpline);
                UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Spawned BlockSpline for Row %d"), RowIndex);
            }
        }

        if (!IsValid(RowSpline)) continue;

#if WITH_EDITOR
        RowSpline->SetActorLabel(FString::Printf(TEXT("%s_Spline_R%d"), *BlockName, RowIndex + 1));
#endif

        // Clean up destroyed/invalid visualizer references from the tracking array
        for (int32 i = Row.SpawnedVisualizers.Num() - 1; i >= 0; --i)
        {
            if (!IsValid(Row.SpawnedVisualizers[i]))
            {
                Row.SpawnedVisualizers.RemoveAt(i);
            }
        }

        // Destroy excess visualizers
        while (Row.SpawnedVisualizers.Num() > Row.PropertyCount)
        {
            AActor* Excess = Row.SpawnedVisualizers.Last();
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
            Row.SpawnedVisualizers.RemoveAt(Row.SpawnedVisualizers.Num() - 1);
        }

        // Ensure array size matches PropertyCount
        if (Row.SpawnedVisualizers.Num() != Row.PropertyCount)
        {
            Row.SpawnedVisualizers.SetNum(Row.PropertyCount);
        }

        // Calculate cumulative index to match unique properties
        int32 CumulativeIndex = 0;
        for (int32 PreRow = 0; PreRow < RowIndex; ++PreRow)
        {
            CumulativeIndex += PropertyRowDetails[PreRow].PropertyCount;
        }

        USplineComponent* SplineComp = RowSpline->SplineComponent;
        if (SplineComp)
        {
            float ActiveSpacing = RowSpline->VisualizerSpacing;

            if (RowSpline->bAutoManageSplinePoints)
            {
                // Ensure the length of the spline matches the Property Count multiplied by the spacing number
                float TargetSplineLength = Row.PropertyCount * ActiveSpacing;
                SplineComp->ClearSplinePoints(false);
                SplineComp->AddSplinePoint(FVector(0.f, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
                SplineComp->AddSplinePoint(FVector(TargetSplineLength, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
                SplineComp->UpdateSpline();
            }

            for (int32 i = 0; i < Row.PropertyCount; ++i)
            {
                float Distance = i * ActiveSpacing;
                FVector SpawnLoc = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator SpawnRot = SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + RowSpline->VisualizerRotationOffset;

                int32 VisualizerBlockIndex = CumulativeIndex + i;

                FString ExpectedName = FString::Printf(TEXT("%s%d"), *BlockName, VisualizerBlockIndex + 1);

                // Determine unique real-estate property data to load
                FSightPortalProperty AssignedProperty;
                bool bFoundMatch = false;

                if (Connector)
                {
                    // 1. Try to find an exact match by Name (e.g. Z1B11) in CachedProperties
                    for (const FSightPortalProperty& Prop : Connector->CachedProperties)
                    {
                        if (Prop.Name.Equals(ExpectedName, ESearchCase::IgnoreCase))
                        {
                            AssignedProperty = Prop;
                            bFoundMatch = true;
                            break;
                        }
                    }

                    // 2. If not found, try to find by Zone, Block, and DoorNo
                    if (!bFoundMatch)
                    {
                        for (const FSightPortalProperty& Prop : Connector->CachedProperties)
                        {
                            if (Prop.Zone.Equals(ZoneName, ESearchCase::IgnoreCase) &&
                                Prop.Block.Equals(BlockName, ESearchCase::IgnoreCase) &&
                                Prop.DoorNo == (VisualizerBlockIndex + 1))
                            {
                                AssignedProperty = Prop;
                                bFoundMatch = true;
                                break;
                            }
                        }
                    }

                    // 3. If still not found, but we have some matched properties for this block, use index-based selection
                    if (!bFoundMatch && MatchedProperties.Num() > 0)
                    {
                        AssignedProperty = MatchedProperties[VisualizerBlockIndex % MatchedProperties.Num()];
                        bFoundMatch = true;
                    }

                    // 4. Global index fallback as a last resort if cache has items but no specific match
                    if (!bFoundMatch && Connector->CachedProperties.Num() > 0)
                    {
                        AssignedProperty = Connector->CachedProperties[VisualizerBlockIndex % Connector->CachedProperties.Num()];
                        bFoundMatch = true;
                    }
                }

                // 5. If still no match or no cache, generate clean mock fallback with correct default name
                if (!bFoundMatch)
                {
                    AssignedProperty.Name = ExpectedName;
                    AssignedProperty.Zone = ZoneName;
                    AssignedProperty.Block = BlockName;
                    AssignedProperty.DoorNo = VisualizerBlockIndex + 1;
                    AssignedProperty.Price = 250000.0f + (VisualizerBlockIndex * 15000.0f);
                    AssignedProperty.Surface = 120.0f + (VisualizerBlockIndex * 10.0f);
                    AssignedProperty.Availability = TEXT("Available");
                }

                // Determine the target class to use (check per-index override first, then row default)
                TSubclassOf<APropertyVisualizer> VisualizerClass = Row.PropertyVisualizer;
                if (Row.PropertyVisualizerOverrides.Contains(i) && Row.PropertyVisualizerOverrides[i])
                {
                    VisualizerClass = Row.PropertyVisualizerOverrides[i];
                }
                if (!VisualizerClass)
                {
                    VisualizerClass = APropertyVisualizer::StaticClass();
                }

                APropertyVisualizer* PropertyVis = Cast<APropertyVisualizer>(Row.SpawnedVisualizers[i]);
                
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
                        PropertyVis->AttachToActor(RowSpline, FAttachmentTransformRules::KeepWorldTransform);
                        PropertyVis->bHasBeenManuallyMoved = bHadManualMove;
                        PropertyVis->ManualRelativeTransform = PrevManualTransform;
                        Row.SpawnedVisualizers[i] = PropertyVis;
                    }
                }

                if (PropertyVis)
                {
                    if (!PropertyVis->bHasBeenManuallyMoved)
                    {
                        PropertyVis->SetActorLocationAndRotation(SpawnLoc, SpawnRot);
                        PropertyVis->SetActorScale3D(RowSpline->VisualizerScaleOffset);
                    }
                    else
                    {
                        FTransform DefaultTransform(SpawnRot, SpawnLoc, RowSpline->VisualizerScaleOffset);
                        FTransform FinalTransform = PropertyVis->ManualRelativeTransform * DefaultTransform;
                        PropertyVis->SetActorTransform(FinalTransform);
                    }
                    PropertyVis->SetPropertyDetails(AssignedProperty);

#if WITH_EDITOR
                    PropertyVis->SetActorLabel(AssignedProperty.Name);
#endif

                    // Central Registry
                    if (SiteManager)
                    {
                        SiteManager->RegisterPropertyVisualizer(AssignedProperty.Name, PropertyVis);
                    }
                }
            }
        }

        // Configure RowSpline parameters
        RowSpline->PropertyCount = Row.PropertyCount;
        RowSpline->PropertyVisualizerClass = Row.PropertyVisualizer;
        RowSpline->PropertyVisualizerOverrides = Row.PropertyVisualizerOverrides;
        RowSpline->BlockName = BlockName;
        RowSpline->ZoneName = ZoneName;
        RowSpline->StartingDoorNumber = CumulativeIndex + 1;
        RowSpline->SpawnedVisualizers = Row.SpawnedVisualizers;

        // Re-trigger construction of the spline to finalize child visualizer alignment
        RowSpline->OnConstruction(RowSpline->GetActorTransform());
    }

    bIsSpawning = false;
}

void ASightPortalBlockManager::ApplyVisualizerClassOverride()
{
    if (!NewVisualizerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockManager] Cannot apply visualizer override: NewVisualizerClass is null."));
        return;
    }

    ChangeVisualizerClassAtIndex(TargetRowIndex, TargetVisualizerIndex, NewVisualizerClass);
}

APropertyVisualizer* ASightPortalBlockManager::ChangeVisualizerClassAtIndex(int32 RowIndex, int32 VisualizerIndex, TSubclassOf<APropertyVisualizer> InNewClass)
{
    if (!InNewClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockManager] ChangeVisualizerClassAtIndex failed: InNewClass is null."));
        return nullptr;
    }

    if (!PropertyRowDetails.IsValidIndex(RowIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockManager] ChangeVisualizerClassAtIndex: Invalid RowIndex %d (Max: %d)"), RowIndex, PropertyRowDetails.Num() - 1);
        return nullptr;
    }

    FPropertyBlockRowDetails& Row = PropertyRowDetails[RowIndex];
    if (VisualizerIndex < 0 || VisualizerIndex >= Row.PropertyCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockManager] ChangeVisualizerClassAtIndex: Invalid VisualizerIndex %d for Row %d (PropertyCount: %d)"), VisualizerIndex, RowIndex, Row.PropertyCount);
        return nullptr;
    }

    // Save override in row configuration map
    Row.PropertyVisualizerOverrides.Add(VisualizerIndex, InNewClass);

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Locate SiteManager if present
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

    // Find parent RowSpline
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    TArray<ABlockSpline*> FoundSplines;
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ABlockSpline::StaticClass()))
        {
            FoundSplines.Add(Cast<ABlockSpline>(Attached));
        }
    }

    ABlockSpline* RowSpline = FoundSplines.IsValidIndex(RowIndex) ? FoundSplines[RowIndex] : nullptr;

    APropertyVisualizer* OldVis = nullptr;
    if (Row.SpawnedVisualizers.IsValidIndex(VisualizerIndex) && IsValid(Row.SpawnedVisualizers[VisualizerIndex]))
    {
        OldVis = Cast<APropertyVisualizer>(Row.SpawnedVisualizers[VisualizerIndex]);
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

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APropertyVisualizer* NewVis = World->SpawnActor<APropertyVisualizer>(InNewClass, OldTransform, SpawnParams);
    if (NewVis)
    {
        if (RowSpline)
        {
            NewVis->AttachToActor(RowSpline, FAttachmentTransformRules::KeepWorldTransform);
        }
        else
        {
            NewVis->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
        }

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

        if (Row.SpawnedVisualizers.IsValidIndex(VisualizerIndex))
        {
            Row.SpawnedVisualizers[VisualizerIndex] = NewVis;
        }
        else
        {
            Row.SpawnedVisualizers.SetNum(Row.PropertyCount);
            Row.SpawnedVisualizers[VisualizerIndex] = NewVis;
        }

        if (RowSpline)
        {
            RowSpline->PropertyVisualizerOverrides = Row.PropertyVisualizerOverrides;
            if (RowSpline->SpawnedVisualizers.IsValidIndex(VisualizerIndex))
            {
                RowSpline->SpawnedVisualizers[VisualizerIndex] = NewVis;
            }
        }

        if (SiteManager && !OldPropertyData.Name.IsEmpty())
        {
            SiteManager->RegisterPropertyVisualizer(OldPropertyData.Name, NewVis);
        }

        UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Successfully replaced visualizer at Row %d Index %d with class %s"), RowIndex, VisualizerIndex, *InNewClass->GetName());
    }

    return NewVis;
}

APropertyVisualizer* ASightPortalBlockManager::ChangeVisualizerClassForProperty(const FString& PropertyName, TSubclassOf<APropertyVisualizer> InNewClass)
{
    if (!InNewClass || PropertyName.IsEmpty())
    {
        return nullptr;
    }

    for (int32 RowIdx = 0; RowIdx < PropertyRowDetails.Num(); ++RowIdx)
    {
        FPropertyBlockRowDetails& Row = PropertyRowDetails[RowIdx];
        for (int32 VisIdx = 0; VisIdx < Row.SpawnedVisualizers.Num(); ++VisIdx)
        {
            APropertyVisualizer* Vis = Cast<APropertyVisualizer>(Row.SpawnedVisualizers[VisIdx]);
            if (IsValid(Vis) && Vis->PropertyDetails.Name.Equals(PropertyName, ESearchCase::IgnoreCase))
            {
                return ChangeVisualizerClassAtIndex(RowIdx, VisIdx, InNewClass);
            }
        }
    }

    return nullptr;
}

#if WITH_EDITOR
void ASightPortalBlockManager::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;
}
#endif
