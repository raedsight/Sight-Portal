#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlockSpline.h"
#include "PropertyVisualizer.h"
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
    TSubclassOf<APropertyVisualizer> PropertyVisualizer;

    // Custom offset location to apply to properties in this row (Property Location)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    FVector PropertyLocation = FVector::ZeroVector;

    // Custom offset rotation to apply to properties in this row
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    FRotator PropertyRotation = FRotator::ZeroRotator;

    // Custom offset scale to apply to properties in this row
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    FVector PropertyScale = FVector(1.0f, 1.0f, 1.0f);

    // List of active spawned property visualizers for this row
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|BlockRow")
    TArray<AActor*> SpawnedVisualizers;
};

/**
 * ASightPortalBlockManager
 * A class dedicated to handling block-level property layout using spawned BlockSpline actors.
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
    // Custom Block Name identifier for property matching
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    FString BlockName = TEXT("1");

    // Row layout offset vector when positioning spawned row splines dynamically
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    FVector SpawnOffset = FVector::ZeroVector;

    // Spacing between different property rows
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float RowSpacing = 500.0f;

    // The desired number of property rows in this block
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration", meta = (ClampMin = "0"))
    int32 PropertyRowCount = 1;

    // Class of BlockSpline actor to spawn dynamically if none is custom-selected
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    TSubclassOf<ABlockSpline> BlockSplineClass;

    // Dynamic list of property rows, automatically resized to match PropertyRowCount
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    TArray<FPropertyBlockRowDetails> PropertyRowDetails;

    // List of all active spawned actors (splines and visualizers) representing the block
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|State")
    TArray<AActor*> ActiveSpawnedActors;

    // --- Operations ---

    // Cleans up all spawned property actors in the active viewport space
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearActiveSpawnedActors();

    // Spawns and arranges properties along the spline paths of each row's BlockSpline actor
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnRowsOfProperties();

    // Spawns/updates Property Visualizers along rows
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnPropertyVisualizers();

    // Clears Property Visualizers for all rows
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearPropertyVisualizers();

private:
    bool bIsSpawning = false;
};
