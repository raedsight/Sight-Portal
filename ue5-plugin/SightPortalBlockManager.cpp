#include "SightPortalBlockManager.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"

ASightPortalBlockManager::ASightPortalBlockManager()
{
    PrimaryActorTick.bCanEverTick = false;
    SpawnOffset = FVector::ZeroVector;
    RowSpacing = 500.0f;
    PropertyRowCount = 1;
}

void ASightPortalBlockManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (PropertyRowCount < 0)
    {
        PropertyRowCount = 0;
    }

    // Clean up excess rows before resizing array
    while (PropertyRowDetails.Num() > PropertyRowCount)
    {
        FPropertyBlockRowDetails& ExcessRow = PropertyRowDetails.Last();
        for (AActor* Vis : ExcessRow.SpawnedVisualizers)
        {
            if (IsValid(Vis))
            {
                Vis->Destroy();
            }
        }
        if (IsValid(ExcessRow.BlockSplineActor))
        {
            ExcessRow.BlockSplineActor->Destroy();
        }
        PropertyRowDetails.RemoveAt(PropertyRowDetails.Num() - 1);
    }

    if (PropertyRowDetails.Num() != PropertyRowCount)
    {
        PropertyRowDetails.SetNum(PropertyRowCount);
    }

    SpawnRowsOfProperties();
}

void ASightPortalBlockManager::BeginPlay()
{
    Super::BeginPlay();

    SpawnRowsOfProperties();
}

void ASightPortalBlockManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearActiveSpawnedActors();

    Super::EndPlay(EndPlayReason);
}

void ASightPortalBlockManager::ClearActiveSpawnedActors()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Clearing %d active spawned actors (splines and visualizers)..."), ActiveSpawnedActors.Num());

    for (AActor* Actor : ActiveSpawnedActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    ActiveSpawnedActors.Empty();

    for (FPropertyBlockRowDetails& Row : PropertyRowDetails)
    {
        Row.BlockSplineActor = nullptr;
        Row.SpawnedVisualizers.Empty();
    }
}

void ASightPortalBlockManager::SpawnRowsOfProperties()
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

    // 1. Scan attached children to recover any untracked ABlockSpline actors
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

    // 2. Clean up invalid/destroyed actors from ActiveSpawnedActors list
    for (int32 i = ActiveSpawnedActors.Num() - 1; i >= 0; --i)
    {
        if (!IsValid(ActiveSpawnedActors[i]))
        {
            ActiveSpawnedActors.RemoveAt(i);
        }
    }

    for (int32 RowIndex = 0; RowIndex < PropertyRowDetails.Num(); ++RowIndex)
    {
        FPropertyBlockRowDetails& Row = PropertyRowDetails[RowIndex];

        // Ensure we have a valid BlockSplineActor for this row
        if (!IsValid(Row.BlockSplineActor))
        {
            // Attempt to assign from recovered unassigned splines first
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

        // If still no valid BlockSplineActor, spawn a new one
        if (!IsValid(Row.BlockSplineActor))
        {
            FVector SplineLocation = ManagerLocation + (GetActorForwardVector() * (RowIndex * RowSpacing)) + (SpawnOffset * RowIndex) + Row.PropertyLocation;

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            #if WITH_EDITOR
            if (!World->IsGameWorld())
            {
                SpawnParams.ObjectFlags |= RF_Transient;
            }
            #endif

            ABlockSpline* NewSpline = World->SpawnActor<ABlockSpline>(ABlockSpline::StaticClass(), SplineLocation, ManagerRotation, SpawnParams);
            if (NewSpline)
            {
                // Attach BlockSpline to ASightPortalBlockManager
                NewSpline->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                Row.BlockSplineActor = NewSpline;
                ActiveSpawnedActors.Add(NewSpline);
                UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Successfully spawned and attached BlockSpline for Row %d"), RowIndex);
            }
        }

        // Recover any existing visualizer child actors attached to this spline
        if (IsValid(Row.BlockSplineActor))
        {
            TArray<AActor*> SplineAttachedActors;
            Row.BlockSplineActor->GetAttachedActors(SplineAttachedActors);

            for (AActor* SplineChild : SplineAttachedActors)
            {
                if (IsValid(SplineChild))
                {
                    if (!Row.SpawnedVisualizers.Contains(SplineChild))
                    {
                        Row.SpawnedVisualizers.Add(SplineChild);
                    }
                    if (!ActiveSpawnedActors.Contains(SplineChild))
                    {
                        ActiveSpawnedActors.Add(SplineChild);
                    }
                }
            }
        }

        // Clean up invalid visualizer pointers
        for (int32 i = Row.SpawnedVisualizers.Num() - 1; i >= 0; --i)
        {
            if (!IsValid(Row.SpawnedVisualizers[i]))
            {
                Row.SpawnedVisualizers.RemoveAt(i);
            }
        }

        // Destroy excess visualizers if PropertyCount was reduced
        while (Row.SpawnedVisualizers.Num() > Row.PropertyCount)
        {
            AActor* Excess = Row.SpawnedVisualizers.Last();
            if (IsValid(Excess))
            {
                ActiveSpawnedActors.Remove(Excess);
                Excess->Destroy();
            }
            Row.SpawnedVisualizers.RemoveAt(Row.SpawnedVisualizers.Num() - 1);
        }

        // Ensure array size matches PropertyCount
        if (Row.SpawnedVisualizers.Num() != Row.PropertyCount)
        {
            Row.SpawnedVisualizers.SetNum(Row.PropertyCount);
        }

        // Position and/or Spawn visualizers along the spline path
        if (IsValid(Row.BlockSplineActor) && Row.BlockSplineActor->SplineComponent)
        {
            USplineComponent* SplineComp = Row.BlockSplineActor->SplineComponent;
            float ActiveSpacing = Row.GridSpacing > 0.0f ? Row.GridSpacing : 350.0f;

            for (int32 i = 0; i < Row.PropertyCount; ++i)
            {
                float Distance = i * ActiveSpacing;
                FVector SpawnLoc = SplineComp->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator SpawnRot = SplineComp->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

                FRotator FinalRot = SpawnRot + Row.PropertyRotation;

                if (IsValid(Row.SpawnedVisualizers[i]))
                {
                    // Update transform of existing visualizer along the spline path (keeps customized edits like scale/rot offsets intact via Row settings)
                    AActor* ExistingVisualizer = Row.SpawnedVisualizers[i];
                    ExistingVisualizer->SetActorLocationAndRotation(SpawnLoc, FinalRot);
                    ExistingVisualizer->SetActorScale3D(Row.PropertyScale);

                    // Re-assert attachment in case it got broken
                    ExistingVisualizer->AttachToActor(Row.BlockSplineActor, FAttachmentTransformRules::KeepWorldTransform);
                }
                else
                {
                    // Spawn new visualizer
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.Owner = this;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    #if WITH_EDITOR
                    if (!World->IsGameWorld())
                    {
                        SpawnParams.ObjectFlags |= RF_Transient;
                    }
                    #endif

                    TSubclassOf<AActor> VisualizerClass = Row.PropertyVisualizer;
                    if (!VisualizerClass)
                    {
                        VisualizerClass = APropertyVisualizer::StaticClass();
                    }

                    AActor* NewPropertyActor = World->SpawnActor<AActor>(VisualizerClass, SpawnLoc, FinalRot, SpawnParams);
                    if (NewPropertyActor)
                    {
                        NewPropertyActor->SetActorScale3D(Row.PropertyScale);
                        // Attach PropertyVisualizer to its BlockSpline
                        NewPropertyActor->AttachToActor(Row.BlockSplineActor, FAttachmentTransformRules::KeepWorldTransform);

                        Row.SpawnedVisualizers[i] = NewPropertyActor;
                        ActiveSpawnedActors.Add(NewPropertyActor);
                    }
                }
            }
        }
    }

    bIsSpawning = false;
}
