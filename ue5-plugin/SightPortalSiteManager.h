#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalConnector.h"
#include "SightPortalSiteManager.generated.h"

class ASightPortalZoneManager;

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
    virtual void Destroyed() override;

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

    // Track if this site manager has been manually moved by the user in the editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|State")
    bool bHasBeenManuallyMoved = false;

#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
#endif

    // --- Operations ---

    // Spawns and arranges Zone Managers dynamically
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnZoneManagers();

    // Adds a single new Zone Manager without respawning or altering existing zones
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    ASightPortalZoneManager* AddNewZone();

    // Adds a new Zone Manager with a custom name and world location without respawning existing zones
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    ASightPortalZoneManager* AddZoneWithParameters(const FString& CustomZoneName, const FVector& CustomLocation);

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

    // Delegate/callback to handle data received events elsewhere in Blueprints
    UPROPERTY(BlueprintAssignable, Category = "SightPortal|SiteManager")
    FOnSightPortalDataReceived OnDataReceived;

    // Callback when new spreadsheet data is fetched
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SiteManager")
    void HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Callback when a single property is updated
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SiteManager")
    void HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

    // Blueprint Implementable Event triggered when live real-estate data portfolio is pushed from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Blueprint Implementable Event triggered when a single property update is pushed from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalPropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

private:
    bool bIsSpawning = false;
};
