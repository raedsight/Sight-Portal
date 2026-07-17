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

    for (FPropertyBlockRowDetails& Row : PropertyRowDetails)
    {
        if (IsValid(Row.BlockSplineActor))
        {
            Row.BlockSplineActor->Destroy();
            Row.BlockSplineActor = nullptr;
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
            if (Prop.Zone.Equals(ZoneName, ESearchCase::IgnoreCase) && Prop.Block.Equals(BlockName, ESearchCase::IgnoreCase))
            {
                MatchedProperties.Add(Prop);
            }
        }
    }

    // Scan attached child splines to recover any that are unassigned
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

    for (int32 RowIndex = 0; RowIndex < PropertyRowDetails.Num(); ++RowIndex)
    {
        FPropertyBlockRowDetails& Row = PropertyRowDetails[RowIndex];

        FVector SplineLocation = ManagerLocation + (GetActorForwardVector() * (RowIndex * RowSpacing)) + (SpawnOffset * RowIndex) + Row.PropertyLocation;

        // 1. Ensure spline actor exists
        if (!IsValid(Row.BlockSplineActor))
        {
            // Try assigning an existing unassigned spline first
            for (ABlockSpline* FSpline : FoundSplines)
            {
                bool bAlreadyAssigned = false;
                for (const FPropertyBlockRowDetails& OtherRow : PropertyRowDetails)
                {
                    if (OtherRow.BlockSplineActor == FSpline)
                    {
                        bAlreadyAssigned = true;
                        break;
                    }
                }
                if (!bAlreadyAssigned)
                {
                    Row.BlockSplineActor = FSpline;
                    break;
                }
            }
        }

        // If still null, spawn a new spline
        if (!IsValid(Row.BlockSplineActor))
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            TSubclassOf<ABlockSpline> SplineClassToSpawn = BlockSplineClass ? BlockSplineClass : ABlockSpline::StaticClass();

            ABlockSpline* NewSpline = World->SpawnActor<ABlockSpline>(SplineClassToSpawn, SplineLocation, ManagerRotation, SpawnParams);
            if (NewSpline)
            {
                NewSpline->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                Row.BlockSplineActor = NewSpline;
                ActiveSpawnedActors.Add(NewSpline);
                UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Spawned BlockSpline for Row %d"), RowIndex);
            }
        }
        else
        {
            // Position the existing spline according to current offsets
            Row.BlockSplineActor->SetActorLocation(SplineLocation);
        }

        if (!IsValid(Row.BlockSplineActor)) continue;

        // Apply visual settings to the spline
        Row.BlockSplineActor->VisualizerRotationOffset = Row.PropertyRotation;
        Row.BlockSplineActor->VisualizerScaleOffset = Row.PropertyScale;

        // 2. Clean up destroyed/invalid visualizer references
        for (int32 i = Row.SpawnedVisualizers.Num() - 1; i >= 0; --i)
        {
            if (!IsValid(Row.SpawnedVisualizers[i]))
            {
                Row.SpawnedVisualizers.RemoveAt(i);
            }
        }

        // 3. Destroy excess visualizers
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

        // 4. Ensure array size matches PropertyCount
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

        USplineComponent* SplineComp = Row.BlockSplineActor->SplineComponent;
        if (SplineComp)
        {
            float ActiveSpacing = Row.BlockSplineActor->VisualizerSpacing;

            for (int32 i = 0; i < Row.PropertyCount; ++i)
            {
                float Distance = i * ActiveSpacing;
                FVector SpawnLoc = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator SpawnRot = SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + Row.PropertyRotation;

                int32 VisualizerBlockIndex = CumulativeIndex + i;

                // Determine unique real-estate property data to load
                FSightPortalProperty AssignedProperty;
                if (MatchedProperties.Num() > 0)
                {
                    AssignedProperty = MatchedProperties[VisualizerBlockIndex % MatchedProperties.Num()];
                }
                else if (Connector && Connector->CachedProperties.Num() > 0)
                {
                    AssignedProperty = Connector->CachedProperties[VisualizerBlockIndex % Connector->CachedProperties.Num()];
                }
                else
                {
                    // Clean mock fallback if cache isn't loaded yet
                    AssignedProperty.Name = FString::Printf(TEXT("Prop_Z%s_B%s_R%d_%d"), *ZoneName, *BlockName, RowIndex + 1, i + 1);
                    AssignedProperty.Zone = ZoneName;
                    AssignedProperty.Block = BlockName;
                    AssignedProperty.DoorNo = i + 1;
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

                    PropertyVis = World->SpawnActor<APropertyVisualizer>(APropertyVisualizer::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
                    if (PropertyVis)
                    {
                        PropertyVis->AttachToActor(Row.BlockSplineActor, FAttachmentTransformRules::KeepWorldTransform);
                        Row.SpawnedVisualizers[i] = PropertyVis;
                    }
                }

                if (PropertyVis)
                {
                    PropertyVis->SetActorLocationAndRotation(SpawnLoc, SpawnRot);
                    PropertyVis->SetActorScale3D(Row.PropertyScale);
                    PropertyVis->PropertyDetails = AssignedProperty;

                    // Central Registry
                    if (SiteManager)
                    {
                        SiteManager->RegisterPropertyVisualizer(AssignedProperty.Name, PropertyVis);
                    }
                }
            }
        }

        // Re-trigger construction of the spline to finalize child visualizer alignment
        Row.BlockSplineActor->OnConstruction(Row.BlockSplineActor->GetActorTransform());
    }

    bIsSpawning = false;
}
