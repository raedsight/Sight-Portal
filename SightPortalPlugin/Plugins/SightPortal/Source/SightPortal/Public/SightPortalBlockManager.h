#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "SightPortalBlockManager.generated.h"

/**
 * FPropertyBlockRowDetails
 * Configuration details for a row of real-estate properties within a block.
 */
USTRUCT(BlueprintType)
struct FPropertyBlockRowDetails
{
    GENERATED_BODY()

    // Number of properties to spawn in this row
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    int32 PropertyCount = 3;

    // Blueprint template class to use for visual representation of this row's properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    TSubclassOf<AActor> PropertyVisualizer;

    // Spline component along which this row's properties should be positioned (Spline type)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    USplineComponent* BlockRowSpline = nullptr;

    // Spacing between spawned actors in this row (float GridSpacing)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    float GridSpacing = 350.0f;

    // Custom offset rotation to apply to properties in this row (Rotator type)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    FRotator PropertyRotation = FRotator::ZeroRotator;

    // Custom offset scale to apply to properties in this row (Vector3 type / FVector)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    FVector PropertyScale = FVector(1.0f, 1.0f, 1.0f);
};

/**
 * ASightPortalBlockManager
 * A class dedicated to handling block-level property layout.
 * Moves variables and functionality from SightPortalActorManager, adding multi-row spline and grid support.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|BlockManager")
class SIGHTPORTAL_API ASightPortalBlockManager : public AActor
{
    GENERATED_BODY()

public:
    ASightPortalBlockManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    // Blueprint-editable Template class to spawn for each spreadsheet real-estate entry (fallback)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    TSubclassOf<AActor> PropertyVisualizerTemplate;

    // Grid layout column spacing when positioning spawned actors automatically (fallback)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float GridSpacing = 350.0f;

    // Baseline height/Z-offset for spawned property visual structures (fallback)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float SpawnZOffset = 0.0f;

    // Spacing between different property rows if spawning on a linear grid (fallback layout mode)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float RowSpacing = 500.0f;

    // The desired number of property rows in this block
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration", meta = (ClampMin = "0"))
    int32 PropertyRowCount = 1;

    // Dynamic list of property rows, automatically resized to match PropertyRowCount
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    TArray<FPropertyBlockRowDetails> PropertyRowDetails;

    // List of active spawned actors representing the real-estate portfolio in the viewport
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|State")
    TArray<AActor*> ActiveSpawnedActors;

    // --- Operations ---

    // Cleans up all spawned property actors in the active viewport space
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    void ClearActiveSpawnedActors();

    // Spawns and arranges properties along the spline paths or fall back to grid rows
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    void SpawnRowsOfProperties();

private:
    bool bIsSpawning = false;
};
