#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalConnector.h"
#include "SightPortalActorManager.generated.h"

/**
 * ASightPortalActorManager
 * A highly customizable C++ Actor Manager class that connects directly to the
 * USightPortalConnector GameInstance Subsystem.
 *
 * This class is designed to be fully inherited by Unreal Engine Blueprints
 * (Blueprintable, BlueprintType) so that technical artists and level designers 
 * can override behavior like actor spawning, material updates, structural mesh scaling,
 * and 3D UI billboard values in real-time when Google Sheets data updates.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|ActorManager")
class SIGHTPORTAL_API ASightPortalActorManager : public AActor
{
    GENERATED_BODY()

public:
    ASightPortalActorManager();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    // Target endpoint URL to fetch spreadsheet datasets (Configure to your live production cloud URL)
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    FString RemoteEndpointURL = TEXT("https://ais-pre-4wjcvfkjzt7ohntjrl7gk5-405891248157.europe-west3.run.app/api/health");

    // Live Streaming WebSocket URL for direct, persistent update delivery bypassing localhost entirely
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    FString WebSocketURL = TEXT("wss://ais-pre-4wjcvfkjzt7ohntjrl7gk5-405891248157.europe-west3.run.app/ws");

    // Blueprint-editable Template class to spawn for each spreadsheet real-estate entry
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    TSubclassOf<AActor> PropertyVisualizerTemplate;

    // Grid layout column spacing when positioning spawned actors automatically
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float GridSpacing = 350.0f;

    // Baseline height/Z-offset for spawned property visual structures
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float SpawnZOffset = 0.0f;

    // List of active spawned actors representing the real-estate portfolio in the viewport
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|State")
    TArray<AActor*> ActiveSpawnedActors;

    // The list of currently synchronized properties from the spreadsheet
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|State")
    TArray<FSightPortalProperty> SyncedProperties;

    // Subsystem reference for calling manual polling or checking network parameters
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|State")
    USightPortalConnector* ActiveSubsystem;

    // --- C++ Event Delegates hooks ---
    
    // Core callback bound to USightPortalConnector's OnRealEstateDataReceived delegate
    UFUNCTION()
    virtual void OnDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Core callback bound to USightPortalConnector's OnPropertyUpdated delegate
    UFUNCTION()
    virtual void OnPropertyDetailUpdated(const FString& Name, const FSightPortalProperty& PropertyDetails);

    // --- Blueprint Inheritable Events (Implementable in Blueprint graphs) ---

    // Fired in Blueprints when the full portfolio data comes in. Perfect for spawning & arranging layout
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events", meta = (DisplayName = "On Portfolio Synchronized"))
    void OnPortfolioSynchronized(const TArray<FSightPortalProperty>& PropertiesList);

    // Fired in Blueprints when a singular property is added/updated. Use to live-refresh 3D text UI or materials
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events", meta = (DisplayName = "On Property Sync Updated"))
    void OnPropertySyncUpdated(const FString& UpdatedPropertyName, const FSightPortalProperty& UpdatedPropertyDetails);

    // --- Blueprint Callable Helpers ---

    // Cleans up all spawned property actors in the active viewport space
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    void ClearActiveSpawnedActors();

    // Spawns and arranges the entire portfolio on a grid layout in the level
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    void SpawnAndArrangePortfolio(const TArray<FSightPortalProperty>& Properties);

private:
    bool bIsSpawning = false;
};
