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

    // Default Blueprint template class to use for visual representation of this row's properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    TSubclassOf<APropertyVisualizer> PropertyVisualizer;

    // Optional: Specific custom visualizer class overrides per property index (e.g. Index 0 -> BP_Villa, Index 2 -> BP_Penthouse)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|BlockRow")
    TMap<int32, TSubclassOf<APropertyVisualizer>> PropertyVisualizerOverrides;

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

    // Track if this block manager has been manually moved by the user in the editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|State")
    bool bHasBeenManuallyMoved = false;

#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
#endif

    // --- Change Specific Visualizer Class Parameters (Editor Exposed) ---

    // Target row index (0-indexed) when modifying a specific visualizer class from editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Change Visualizer", meta = (ClampMin = "0"))
    int32 TargetRowIndex = 0;

    // Target visualizer index in the row (0-indexed) when modifying a specific visualizer class from editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Change Visualizer", meta = (ClampMin = "0"))
    int32 TargetVisualizerIndex = 0;

    // New Property Visualizer blueprint class to assign to the targeted visualizer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Change Visualizer")
    TSubclassOf<APropertyVisualizer> NewVisualizerClass;

    // --- Operations ---

    // Applies the NewVisualizerClass to the specific visualizer at TargetRowIndex and TargetVisualizerIndex in the editor
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations|Change Visualizer", meta = (CallInEditor = "true", DisplayName = "Apply Visualizer Class Override"))
    void ApplyVisualizerClassOverride();

    // Replaces a single specific Property Visualizer class at a given row and index dynamically while preserving its transform and data
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    APropertyVisualizer* ChangeVisualizerClassAtIndex(int32 RowIndex, int32 VisualizerIndex, TSubclassOf<APropertyVisualizer> InNewClass);

    // Replaces a single specific Property Visualizer class for a given property name dynamically
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    APropertyVisualizer* ChangeVisualizerClassForProperty(const FString& PropertyName, TSubclassOf<APropertyVisualizer> InNewClass);

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

    // Callback when full portfolio data is received from connector
    UFUNCTION()
    void HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Callback when a single property is updated from connector
    UFUNCTION()
    void HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

    // Dynamic Blueprint Implementable Event triggered when live real-estate data is pushed from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Dynamic Blueprint Implementable Event triggered when a single property in this block is updated from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalPropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

private:
    bool bIsSpawning = false;
};
