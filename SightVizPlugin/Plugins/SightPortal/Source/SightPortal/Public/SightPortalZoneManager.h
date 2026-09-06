#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalZoneManager.generated.h"

class ASightPortalBlockManager;
class APropertyVisualizer;

/**
 * ASightPortalZoneManager
 * A dedicated class to handle each Zone in a construction site (residential city).
 * It will create SightPortalBlockManager actors based on BlockCount variable.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|ZoneManager")
class SIGHTPORTAL_API ASightPortalZoneManager : public AActor
{
    GENERATED_BODY()

public:
    ASightPortalZoneManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    // Number of Block Managers to spawn in this construction zone, exposed to be called/modified in editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|ZoneConfiguration", meta = (ClampMin = "0"))
    int32 BlockCount = 1;

    // Distance/spacing parameter between spawned block managers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|ZoneConfiguration")
    float BlockSpacing = 1000.0f;

    // Custom Zone Name identifier for property matching
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|ZoneConfiguration")
    FString ZoneName = TEXT("1");

    // Blueprint-editable Template class to spawn for each Block Manager
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|ZoneConfiguration")
    TSubclassOf<AActor> BlockManagerClass;

    // List of active spawned Block Managers representing blocks in the zone
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|State")
    TArray<AActor*> ActiveBlockManagers;

    // Track if this zone manager has been manually moved by the user in the editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|State")
    bool bHasBeenManuallyMoved = false;

    // --- Custom Block Spawn Parameters (Editor Exposed) ---

    // Optional custom name when spawning a custom block from editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Custom Block")
    FString NewBlockCustomName = TEXT("");

    // Optional custom world position/offset when spawning a custom block from editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Custom Block")
    FVector NewBlockCustomLocation = FVector::ZeroVector;

    // If true, uses NewBlockCustomLocation; otherwise calculates automatic forward-vector offset based on BlockSpacing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Custom Block")
    bool bUseCustomLocationForNewBlock = false;

#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
#endif

    // --- Operations ---

    // Spawns and arranges Block Managers dynamically
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations", meta = (CallInEditor = "true", DisplayName = "Spawn Block Managers"))
    void SpawnBlockManagers();

    // Adds a single new Block Manager without respawning or altering existing blocks
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations", meta = (CallInEditor = "true", DisplayName = "Add New Block"))
    ASightPortalBlockManager* AddNewBlock();

    // Adds a new Block using the configured NewBlockCustomName and NewBlockCustomLocation parameters without respawning existing blocks
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations", meta = (CallInEditor = "true", DisplayName = "Add Configured Block"))
    void AddConfiguredBlock();

    // Adds a new Block Manager with a custom name and world location without respawning existing blocks
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations", meta = (DisplayName = "Add Block With Parameters"))
    ASightPortalBlockManager* AddBlockWithParameters(const FString& CustomBlockName = TEXT(""), const FVector& CustomLocation = FVector::ZeroVector);

    // Cleans up all spawned Block Managers
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations", meta = (CallInEditor = "true", DisplayName = "Clear Block Managers"))
    void ClearBlockManagers();

    // Spawns/updates Property Visualizers for all child blocks (which in turn spawn for rows)
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnPropertyVisualizers();

    // Clears Property Visualizers for all child blocks (which in turn clear for rows)
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearPropertyVisualizers();

    // Replaces a single specific Property Visualizer class for a given property name dynamically
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    APropertyVisualizer* ChangeVisualizerClassForProperty(const FString& PropertyName, TSubclassOf<APropertyVisualizer> InNewClass);

    // Replaces a single specific Property Visualizer class in a named block at given row and index dynamically
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    APropertyVisualizer* ChangeVisualizerClassInBlock(const FString& InBlockName, int32 RowIndex, int32 VisualizerIndex, TSubclassOf<APropertyVisualizer> InNewClass);

    // Callback when full portfolio data is received from connector
    UFUNCTION()
    void HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Callback when a single property update is received from connector
    UFUNCTION()
    void HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

    // Dynamic Blueprint Implementable Event triggered when live real-estate data is pushed from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Dynamic Blueprint Implementable Event triggered when a property in this zone is updated from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalPropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

private:
    bool bIsSpawning = false;
};
