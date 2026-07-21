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
    SpawnPropertyVisualizers();
}

void ASightPortalBlockManager::BeginPlay()
{
    Super::BeginPlay();
}

void ASightPortalBlockManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearActiveSpawnedActors();

    Super::EndPlay(EndPlayReason);
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

                APropertyVisualizer* PropertyVis = Cast<APropertyVisualizer>(Row.SpawnedVisualizers[i]);
                if (!IsValid(PropertyVis))
                {
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.Owner = this;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    TSubclassOf<APropertyVisualizer> VisualizerClass = Row.PropertyVisualizer;
                    if (!VisualizerClass)
                    {
                        VisualizerClass = APropertyVisualizer::StaticClass();
                    }

                    PropertyVis = World->SpawnActor<APropertyVisualizer>(VisualizerClass, SpawnLoc, SpawnRot, SpawnParams);
                    if (PropertyVis)
                    {
                        PropertyVis->AttachToActor(RowSpline, FAttachmentTransformRules::KeepWorldTransform);
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
                    PropertyVis->PropertyDetails = AssignedProperty;

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

        // Re-trigger construction of the spline to finalize child visualizer alignment
        RowSpline->OnConstruction(RowSpline->GetActorTransform());
    }

    bIsSpawning = false;
}

#if WITH_EDITOR
void ASightPortalBlockManager::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;
}
#endif
