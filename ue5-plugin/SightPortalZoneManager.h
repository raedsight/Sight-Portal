#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalZoneManager.generated.h"

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

#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
#endif

    // --- Operations ---

    // Spawns and arranges Block Managers dynamically
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnBlockManagers();

    // Cleans up all spawned Block Managers
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearBlockManagers();

    // Spawns/updates Property Visualizers for all child blocks (which in turn spawn for rows)
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnPropertyVisualizers();

    // Clears Property Visualizers for all child blocks (which in turn clear for rows)
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearPropertyVisualizers();

private:
    bool bIsSpawning = false;
};
