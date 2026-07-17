#include "SightPortalBlockManager.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"

ASightPortalBlockManager::ASightPortalBlockManager()
{
    PrimaryActorTick.bCanEverTick = false;
    GridSpacing = 350.0f;
    SpawnZOffset = 0.0f;
    RowSpacing = 500.0f;
    PropertyRowCount = 1;
}

void ASightPortalBlockManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Keep PropertyRowDetails array synchronized with PropertyRowCount
    if (PropertyRowCount < 0)
    {
        PropertyRowCount = 0;
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
    UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Clearing %d active spawned property visualizers..."), ActiveSpawnedActors.Num());

    for (AActor* Actor : ActiveSpawnedActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    ActiveSpawnedActors.Empty();
}

void ASightPortalBlockManager::SpawnRowsOfProperties()
{
    if (bIsSpawning)
    {
        return;
    }

    bIsSpawning = true;

    // Clear out any old instances
    ClearActiveSpawnedActors();

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsSpawning = false;
        return;
    }

    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    for (int32 RowIndex = 0; RowIndex < PropertyRowDetails.Num(); ++RowIndex)
    {
        const FPropertyBlockRowDetails& Row = PropertyRowDetails[RowIndex];
        
        // Determine the class to spawn (per-row override or fallback)
        TSubclassOf<AActor> VisualizerClass = Row.PropertyVisualizer ? Row.PropertyVisualizer : PropertyVisualizerTemplate;
        if (!VisualizerClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockManager] Row %d lacks a valid PropertyVisualizer and no fallback template is set!"), RowIndex);
            continue;
        }

        // Safety check to prevent infinite recursion
        if (VisualizerClass->IsChildOf(ASightPortalBlockManager::StaticClass()))
        {
            UE_LOG(LogTemp, Error, TEXT("[SightPortal BlockManager] Row %d visualizer class cannot be ASightPortalBlockManager itself!"), RowIndex);
            continue;
        }

        float ActiveSpacing = Row.GridSpacing > 0.0f ? Row.GridSpacing : GridSpacing;

        // Mode A: Spline Spawning
        if (Row.BlockRowSpline)
        {
            float SplineLength = Row.BlockRowSpline->GetSplineLength();
            UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Spawning Row %d along Spline with %d properties..."), RowIndex, Row.PropertyCount);

            for (int32 i = 0; i < Row.PropertyCount; ++i)
            {
                float Distance = i * ActiveSpacing;
                if (Distance > SplineLength)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockManager] Spawning aborted at property index %d because spline distance %0.1f exceeds spline length %0.1f."), i, Distance, SplineLength);
                    break;
                }

                FVector SpawnLoc = Row.BlockRowSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator SpawnRot = Row.BlockRowSpline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

                // Apply custom row-based rotational offset
                FRotator FinalRot = SpawnRot + Row.PropertyRotation;

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                #if WITH_EDITOR
                if (World && !World->IsGameWorld())
                {
                    SpawnParams.ObjectFlags |= RF_Transient;
                }
                #endif

                AActor* NewPropertyActor = World->SpawnActor<AActor>(VisualizerClass, SpawnLoc, FinalRot, SpawnParams);
                if (NewPropertyActor)
                {
                    NewPropertyActor->SetActorScale3D(Row.PropertyScale);
                    ActiveSpawnedActors.Add(NewPropertyActor);
                }
            }
        }
        // Mode B: Standard Linear Spawning (Fallback)
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[SightPortal BlockManager] Spawning Row %d using fallback linear layout..."), RowIndex);

            for (int32 i = 0; i < Row.PropertyCount; ++i)
            {
                // Align rows orthogonally along Forward Vector, columns spaced out along Right Vector
                FVector SpawnLoc = ManagerLocation + (GetActorForwardVector() * (RowIndex * RowSpacing)) + (GetActorRightVector() * (i * ActiveSpacing));
                SpawnLoc.Z += SpawnZOffset;

                // Ensure SpawnLoc components are finite
                if (SpawnLoc.ContainsNaN() || SpawnLoc.Size() > 1e12)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SightPortal BlockManager] Spawn location is out of bounds. Skipping index %d"), i);
                    continue;
                }

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                #if WITH_EDITOR
                if (World && !World->IsGameWorld())
                {
                    SpawnParams.ObjectFlags |= RF_Transient;
                }
                #endif

                // Apply custom rotation
                FRotator FinalRot = ManagerRotation + Row.PropertyRotation;

                AActor* NewPropertyActor = World->SpawnActor<AActor>(VisualizerClass, SpawnLoc, FinalRot, SpawnParams);
                if (NewPropertyActor)
                {
                    NewPropertyActor->SetActorScale3D(Row.PropertyScale);
                    ActiveSpawnedActors.Add(NewPropertyActor);
                }
            }
        }
    }

    bIsSpawning = false;
}
