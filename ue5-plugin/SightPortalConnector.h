#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SightPortalConnector.generated.h"

// Custom struct representing the real-estate attributes synced from the Google Sheet
USTRUCT(BlueprintType)
struct FSightPortalProperty
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    FString Zone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    FString Block;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    int32 DoorNo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    float Price = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    float Surface = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    FString Availability;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    float BuildingSurface = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    int32 BedroomsCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    int32 BathroomsCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    FString Class;

    // Any additional custom or dynamically defined column attributes from the portal/spreadsheet
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|RealEstate")
    TMap<FString, FString> CustomAttributes;

    // Helper to retrieve any custom attribute value
    FString GetCustomAttribute(const FString& Key, const FString& DefaultValue = TEXT("")) const
    {
        const FString* Found = CustomAttributes.Find(Key);
        return Found ? *Found : DefaultValue;
    }
};

// Blueprint multicast delegates to notify levels when property records are pulled
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSightPortalDataReceived, const TArray<FSightPortalProperty>&, PropertyPortfolio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSightPortalPropertyUpdated, const FString&, PropertyName, const FSightPortalProperty&, PropertyDetails);

/**
 * Bespoke C++ Subsystem that boots up with the SightPortal simulation session.
 * Connects directly to the UE5 Sheet Bridge without requiring complex external middleware.
 */
UCLASS()
class SIGHTPORTAL_API USightPortalConnector : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Trigger a manual poll check from inside Blueprints
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Bridge")
    void FetchLatestSpreadsheetData();

    // The Multicast delegates for level visualizers, camera rigs, and HUD charts
    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Bridge")
    FOnSightPortalDataReceived OnRealEstateDataReceived;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Bridge")
    FOnSightPortalPropertyUpdated OnPropertyUpdated;

    // Locally cached properties structure to support instant viewport spawns
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|Bridge")
    TArray<FSightPortalProperty> CachedProperties;

    // Helper functions to spawn property actors dynamically or map them to template assets
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Bridge")
    void SyncWorldActorsWithPayload(const TArray<FSightPortalProperty>& Properties);

    // Target endpoint URL to fetch spreadsheet datasets (Configure to your live production cloud URL)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Bridge")
    FString RemoteEndpointURL = TEXT("https://sightportal.ai.studio/api/health");

    // Live Streaming WebSocket URL for direct, persistent update delivery bypassing localhost entirely
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Bridge")
    FString WebSocketURL = TEXT("wss://sightportal.ai.studio/ws");

    // Establish persistent, bi-directional live socket connection to the cloud server directly
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Bridge")
    void ConnectWebSocket();

    // Terminate current socket stream cleanly
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Bridge")
    void DisconnectWebSocket();

    // Check if the real-time link is active
    UFUNCTION(BlueprintPure, Category = "SightPortal|Bridge")
    bool IsWebSocketConnected() const;

private:
    void HandleHTTPResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    // Socket state tracking and callback bindings
    TSharedPtr<class IWebSocket> WebSocketClient;

    FTimerHandle ReconnectTimerHandle;
    void AttemptReconnect();
    class FTimerManager* GetSafeTimerManager() const;

    void OnWebsocketConnected();
    void OnWebsocketConnectionError(const FString& Error);
    void OnWebsocketMessage(const FString& MessageString);
    void OnWebsocketClosed(int32 Status, const FString& Reason, bool bWasClean);

    // Main parser helper shared across HTTP and Socket responses
    void ParseAndSyncJsonPayload(const FString& JsonContent);
};
