#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalSiteManager.generated.h"

/**
 * ASightPortalSiteManager
 * A highly customizable C++ Site Manager class that handles the construction site as a whole
 * and spawns SightPortalZoneManager actors.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|SiteManager")
class SIGHTPORTAL_API ASightPortalSiteManager : public AActor
{
    GENERATED_BODY()

public:
    ASightPortalSiteManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    // Number of Zones to spawn in this construction site, exposed to be called/modified in editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration", meta = (ClampMin = "0"))
    int32 ZoneCount = 1;

    // Distance/spacing parameter between spawned zone managers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float ZoneSpacing = 1500.0f;

    // Blueprint-editable Template class to spawn for each Zone Manager
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    TSubclassOf<AActor> ZoneManagerClass;

    // Web Socket URL to stream live real-estate updates
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Network")
    FString WebSocketURL;

    // Remote Endpoint URL to poll current spreadsheet records
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Network")
    FString RemoteEndpointURL;

    // List of active spawned Zone Managers representing zones in the level
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|State")
    TArray<AActor*> ActiveZoneManagers;

    // Map tracking active spawned property visualizers in the site, mapping Property Name to the Actor
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|SiteManager")
    TMap<FString, AActor*> RegisteredPropertyVisualizers;

    // --- Operations ---

    // Spawns and arranges Zone Managers dynamically
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnZoneManagers();

    // Cleans up all spawned Zone Managers
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearZoneManagers();

    // Spawns/updates Property Visualizers for all child zones (which in turn spawn for blocks)
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnPropertyVisualizers();

    // Clears Property Visualizers for all child zones (which in turn clear for blocks)
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearPropertyVisualizers();

    // Force fetch spreadsheet data and update websocket connection
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ForceFetchData();

    // Register a spawned property visualizer in the site-wide directory
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SiteManager")
    void RegisterPropertyVisualizer(const FString& PropertyName, AActor* VisualizerActor);

    // Unregister a property visualizer from the site-wide directory
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SiteManager")
    void UnregisterPropertyVisualizer(const FString& PropertyName);

    // Get a registered property visualizer by name
    UFUNCTION(BlueprintPure, Category = "SightPortal|SiteManager")
    AActor* GetRegisteredPropertyVisualizer(const FString& PropertyName) const;

private:
    bool bIsSpawning = false;
};
