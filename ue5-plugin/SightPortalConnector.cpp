#include "SightPortalConnector.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"

void USightPortalConnector::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Bespoke UE5 C++ Connector Initialized."));

    // Automatically trigger WebSocket direct persistent link on start of interactive demonstration session
    ConnectWebSocket();

    // Fallback/Cold Boot: Query standard HTTP channel instantly to populate viewports in case sockets handshake is blocked
    FetchLatestSpreadsheetData();
}

void USightPortalConnector::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Bespoke UE5 C++ Connector Shutting Down."));
    
    // Shut down the WebSocket client cleanly if active upon session teardown
    DisconnectWebSocket();

    Super::Deinitialize();
}

void USightPortalConnector::FetchLatestSpreadsheetData()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Querying cloud server `/api/health` directly for real-estate values..."));

    FHttpModule* Http = &FHttpModule::Get();
    if (!Http) return;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &USightPortalConnector::HandleHTTPResponse);
    
    // Connect to the configured remote endpoint to pull current spreadsheet records
    Request->SetURL(RemoteEndpointURL);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->ProcessRequest();
}

void USightPortalConnector::HandleHTTPResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal Bridge] HTTP fallback connection failed. Is the cloud server sleeping?"));
        return;
    }

    FString ResponseString = Response->GetContentAsString();
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Data packet received via HTTP: %s"), *ResponseString);

    ParseAndSyncJsonPayload(ResponseString);
}

void USightPortalConnector::ConnectWebSocket()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Initiating persistent real-time socket connection directly to: %s"), *WebSocketURL);
    
    if (WebSocketClient.IsValid() && WebSocketClient->IsConnected())
    {
        UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Socket already connected. Skipping initialization."));
        return;
    }

    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    // Connect with standard WebSocket headers (no subprotocol requirement to prevent 'HS: ACCEPT missing' errors across proxies/CDNs)
    WebSocketClient = FWebSocketsModule::Get().CreateWebSocket(WebSocketURL);

    WebSocketClient->OnConnected().AddUObject(this, &USightPortalConnector::OnWebsocketConnected);
    WebSocketClient->OnConnectionError().AddUObject(this, &USightPortalConnector::OnWebsocketConnectionError);
    WebSocketClient->OnMessage().AddUObject(this, &USightPortalConnector::OnWebsocketMessage);
    WebSocketClient->OnClosed().AddUObject(this, &USightPortalConnector::OnWebsocketClosed);

    WebSocketClient->Connect();
}

void USightPortalConnector::DisconnectWebSocket()
{
    // Clear reconnect timer since we are explicitly disconnecting
    FTimerManager* TimerManager = GetSafeTimerManager();
    if (TimerManager)
    {
        TimerManager->ClearTimer(ReconnectTimerHandle);
    }

    if (WebSocketClient.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Closing direct WebSocket connection..."));
        WebSocketClient->Close();
        WebSocketClient.Reset();
    }
}

bool USightPortalConnector::IsWebSocketConnected() const
{
    return WebSocketClient.IsValid() && WebSocketClient->IsConnected();
}

FTimerManager* USightPortalConnector::GetSafeTimerManager() const
{
    UWorld* World = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World())
            {
                World = Context.World();
                break;
            }
        }
    }
    return World ? &World->GetTimerManager() : nullptr;
}

void USightPortalConnector::AttemptReconnect()
{
    FTimerManager* TimerManager = GetSafeTimerManager();
    if (!TimerManager) return;

    // Avoid planning parallel reconnect timers
    if (TimerManager->IsTimerActive(ReconnectTimerHandle))
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[SightPortal Socket] Scheduling automatic reconnection in 5.0 seconds..."));
    
    TimerManager->SetTimer(
        ReconnectTimerHandle,
        this,
        &USightPortalConnector::ConnectWebSocket,
        5.0f,
        false
    );
}

void USightPortalConnector::OnWebsocketConnected()
{
    UE_LOG(LogTemp, Display, TEXT("[SightPortal Socket] Direct connection established successfully! Ready for real-time updates."));
    
    // Clear any pending reconnection timers upon success
    FTimerManager* TimerManager = GetSafeTimerManager();
    if (TimerManager)
    {
        TimerManager->ClearTimer(ReconnectTimerHandle);
    }
}

void USightPortalConnector::OnWebsocketConnectionError(const FString& Error)
{
    UE_LOG(LogTemp, Error, TEXT("[SightPortal Socket] Direct real-time link failed to connect: %s"), *Error);
    
    // Auto Reconnect
    AttemptReconnect();
}

void USightPortalConnector::OnWebsocketMessage(const FString& MessageString)
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Socket] Real-time message streaming packet arrived: %s"), *MessageString);
    ParseAndSyncJsonPayload(MessageString);
}

void USightPortalConnector::OnWebsocketClosed(int32 Status, const FString& Reason, bool bWasClean)
{
    UE_LOG(LogTemp, Warning, TEXT("[SightPortal Socket] Session closed by host. Code: %d, Reason: '%s'. Clean shutdown: %s"), Status, *Reason, bWasClean ? TEXT("Yes") : TEXT("No"));
    WebSocketClient.Reset();

    // Auto Reconnect if closed abnormally (not by a clean manual disconnect call)
    if (!bWasClean)
    {
        AttemptReconnect();
    }
}

static FString GetFirstMatchingFieldAsString(TSharedPtr<FJsonObject> Obj, const TArray<FString>& Keys)
{
    if (!Obj.IsValid()) return TEXT("");
    
    for (const FString& Key : Keys)
    {
        if (Obj->HasField(Key))
        {
            FString ValString;
            if (Obj->TryGetStringField(Key, ValString))
            {
                return ValString;
            }
            double ValDouble;
            if (Obj->TryGetNumberField(Key, ValDouble))
            {
                return FString::SanitizeFloat(ValDouble);
            }
            bool ValBool;
            if (Obj->TryGetBoolField(Key, ValBool))
            {
                return ValBool ? TEXT("true") : TEXT("false");
            }
        }
    }
    return TEXT("");
}

// Helper to extract a single FSightPortalProperty from a JSON object with exhaustive key synonym matching
static FSightPortalProperty ParsePropertyFromJsonObject(TSharedPtr<FJsonObject> RowObj)
{
    FSightPortalProperty Property;
    if (!RowObj.IsValid()) return Property;

    // Define comprehensive key lists covering all preset templates, user naming conventions, and custom schemas
    TArray<FString> NameKeys = { 
        TEXT("Name"), TEXT("name"), TEXT("ActorName"), TEXT("actor_name"), TEXT("PropID"), TEXT("prop_id"), 
        TEXT("ID"), TEXT("id"), TEXT("Title"), TEXT("title"), TEXT("Label"), TEXT("label"),
        TEXT("PropertyName"), TEXT("property_name")
    };
    TArray<FString> ZoneKeys = { 
        TEXT("Zone"), TEXT("zone"), TEXT("Region"), TEXT("region"), TEXT("Sector"), TEXT("sector"), 
        TEXT("AreaName"), TEXT("area_name") 
    };
    TArray<FString> BlockKeys = { 
        TEXT("Block"), TEXT("block"), TEXT("Building"), TEXT("building"), TEXT("Tower"), TEXT("tower"), 
        TEXT("Phase"), TEXT("phase") 
    };
    TArray<FString> DoorNoKeys = { 
        TEXT("Door No"), TEXT("door_no"), TEXT("DoorNo"), TEXT("doorno"), TEXT("Door Number"), TEXT("door_number"), 
        TEXT("DoorNumber"), TEXT("Door"), TEXT("door"), TEXT("Unit"), TEXT("unit"), TEXT("Unit No"), TEXT("unit_no"), 
        TEXT("UnitNo"), TEXT("UnitNumber"), TEXT("unit_number"), TEXT("Floor"), TEXT("floor"), 
        TEXT("Room No"), TEXT("room_no"), TEXT("RoomNo"), TEXT("Room Number"), TEXT("room_number"), 
        TEXT("Room"), TEXT("room") 
    };
    TArray<FString> PriceKeys = { 
        TEXT("Price"), TEXT("price"), TEXT("PriceUSD"), TEXT("price_usd"), TEXT("Price EUR"), TEXT("price_eur"), 
        TEXT("PriceGBP"), TEXT("price_gbp"), TEXT("Cost"), TEXT("cost"), TEXT("Valuation"), TEXT("valuation"), 
        TEXT("Amount"), TEXT("amount"), TEXT("Rate"), TEXT("rate"), TEXT("ListPrice"), TEXT("list_price") 
    };
    TArray<FString> SurfaceKeys = { 
        TEXT("Surface"), TEXT("surface"), TEXT("AreaSqM"), TEXT("area_sqm"), TEXT("Area"), TEXT("area"), 
        TEXT("Size"), TEXT("size"), TEXT("TotalSurface"), TEXT("total_surface"), TEXT("PlotArea"), TEXT("plot_area"), 
        TEXT("LotSize"), TEXT("lot_size"), TEXT("SquareMeters"), TEXT("square_meters") 
    };
    TArray<FString> AvailabilityKeys = { 
        TEXT("Availability"), TEXT("availability"), TEXT("Status"), TEXT("status"), TEXT("State"), TEXT("state"), 
        TEXT("IsAvailable"), TEXT("is_available"), TEXT("Condition"), TEXT("condition") 
    };
    TArray<FString> BuildingSurfaceKeys = { 
        TEXT("BuildingSurface"), TEXT("building_surface"), TEXT("BuildingArea"), TEXT("building_area"), 
        TEXT("BuiltArea"), TEXT("built_area"), TEXT("IndoorSurface"), TEXT("indoor_surface"), 
        TEXT("NetArea"), TEXT("net_area") 
    };
    TArray<FString> BedroomsCountKeys = { 
        TEXT("BedroomsCount"), TEXT("bedrooms_count"), TEXT("bedroom_count"), TEXT("Bedrooms"), TEXT("bedrooms"), 
        TEXT("Bedroom"), TEXT("bedroom"), TEXT("Rooms"), TEXT("rooms"), TEXT("RoomCount"), TEXT("room_count"), 
        TEXT("Room Number"), TEXT("room_number"), TEXT("Beds"), TEXT("beds") 
    };
    TArray<FString> BathroomsCountKeys = { 
        TEXT("BathroomsCount"), TEXT("bathrooms_count"), TEXT("bathroom_count"), TEXT("Bathrooms"), TEXT("bathrooms"), 
        TEXT("Bathroom"), TEXT("bathroom"), TEXT("Baths"), TEXT("baths") 
    };
    TArray<FString> ClassKeys = { 
        TEXT("Class"), TEXT("class"), TEXT("Type"), TEXT("type"), TEXT("Category"), TEXT("category"), 
        TEXT("PropertyType"), TEXT("property_type"), TEXT("Usage"), TEXT("usage"), TEXT("Model"), TEXT("model") 
    };

    Property.Name = GetFirstMatchingFieldAsString(RowObj, NameKeys);
    Property.Zone = GetFirstMatchingFieldAsString(RowObj, ZoneKeys);
    Property.Block = GetFirstMatchingFieldAsString(RowObj, BlockKeys);
    
    FString DoorNoStr = GetFirstMatchingFieldAsString(RowObj, DoorNoKeys);
    Property.DoorNo = FCString::Atoi(*DoorNoStr);

    FString PriceStr = GetFirstMatchingFieldAsString(RowObj, PriceKeys);
    Property.Price = FCString::Atof(*PriceStr);

    FString SurfaceStr = GetFirstMatchingFieldAsString(RowObj, SurfaceKeys);
    Property.Surface = FCString::Atof(*SurfaceStr);

    Property.Availability = GetFirstMatchingFieldAsString(RowObj, AvailabilityKeys);

    FString BldgSurfaceStr = GetFirstMatchingFieldAsString(RowObj, BuildingSurfaceKeys);
    Property.BuildingSurface = FCString::Atof(*BldgSurfaceStr);

    FString BedroomsStr = GetFirstMatchingFieldAsString(RowObj, BedroomsCountKeys);
    Property.BedroomsCount = FCString::Atoi(*BedroomsStr);

    FString BathroomsStr = GetFirstMatchingFieldAsString(RowObj, BathroomsCountKeys);
    Property.BathroomsCount = FCString::Atoi(*BathroomsStr);

    Property.Class = GetFirstMatchingFieldAsString(RowObj, ClassKeys);

    // Collect all raw/custom fields into CustomAttributes map so no portal columns are lost
    for (const auto& FieldPair : RowObj->Values)
    {
        const FString& Key = FieldPair.Key;
        FString ValStr = GetFirstMatchingFieldAsString(RowObj, { Key });
        Property.CustomAttributes.Add(Key, ValStr);
    }

    return Property;
}

void USightPortalConnector::ParseAndSyncJsonPayload(const FString& JsonContent)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        TArray<FSightPortalProperty> PropertyPortfolio;

        // Resolve payload container: check for payload wrapper or root
        TSharedPtr<FJsonObject> PayloadObj = JsonObject;
        if (JsonObject->HasField(TEXT("payload")) && JsonObject->GetObjectField(TEXT("payload")).IsValid())
        {
            PayloadObj = JsonObject->GetObjectField(TEXT("payload"));
        }

        // Case 1: Multiple records in attributes_matrix or data array
        const TArray<TSharedPtr<FJsonValue>>* RowsArray = nullptr;
        if (PayloadObj->TryGetArrayField(TEXT("attributes_matrix"), RowsArray) ||
            PayloadObj->TryGetArrayField(TEXT("data"), RowsArray) ||
            JsonObject->TryGetArrayField(TEXT("attributes_matrix"), RowsArray))
        {
            for (const auto& RowVal : *RowsArray)
            {
                TSharedPtr<FJsonObject> RowObj = RowVal->AsObject();
                if (!RowObj.IsValid()) continue;

                FSightPortalProperty Property = ParsePropertyFromJsonObject(RowObj);
                PropertyPortfolio.Add(Property);

                // Fire singular event for localized asset updates
                OnPropertyUpdated.Broadcast(Property.Name, Property);
            }

            // Cache the full portfolio locally for instant access during new level load / play instantiations
            CachedProperties = PropertyPortfolio;

            // Fire global multicast delegate trigger
            OnRealEstateDataReceived.Broadcast(PropertyPortfolio);

            // Update real world interactive structures in scene
            SyncWorldActorsWithPayload(PropertyPortfolio);
            return;
        }

        // Case 2: Single property update payload (e.g. FORCE_ROW_UPDATE, CELL_UPDATE, or updated_row)
        TSharedPtr<FJsonObject> SinglePropertyObj = nullptr;
        if (PayloadObj->HasField(TEXT("data")) && PayloadObj->GetObjectField(TEXT("data")).IsValid())
        {
            SinglePropertyObj = PayloadObj->GetObjectField(TEXT("data"));
        }
        else if (PayloadObj->HasField(TEXT("updated_row")) && PayloadObj->GetObjectField(TEXT("updated_row")).IsValid())
        {
            SinglePropertyObj = PayloadObj->GetObjectField(TEXT("updated_row"));
        }
        else if (JsonObject->HasField(TEXT("data")) && JsonObject->GetObjectField(TEXT("data")).IsValid())
        {
            SinglePropertyObj = JsonObject->GetObjectField(TEXT("data"));
        }

        if (SinglePropertyObj.IsValid())
        {
            FSightPortalProperty SingleProp = ParsePropertyFromJsonObject(SinglePropertyObj);
            if (SingleProp.Name.IsEmpty() && JsonObject->HasField(TEXT("property")))
            {
                SingleProp.Name = JsonObject->GetStringField(TEXT("property"));
            }

            UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Single property live update received: '%s' (Price: %0.2f, Status: %s)"),
                *SingleProp.Name, SingleProp.Price, *SingleProp.Availability);

            // Update or append inside CachedProperties
            bool bFoundInCache = false;
            for (int32 i = 0; i < CachedProperties.Num(); ++i)
            {
                if (CachedProperties[i].Name.Equals(SingleProp.Name, ESearchCase::IgnoreCase) ||
                    (!SingleProp.Zone.IsEmpty() && CachedProperties[i].Zone.Equals(SingleProp.Zone, ESearchCase::IgnoreCase) &&
                     CachedProperties[i].Block.Equals(SingleProp.Block, ESearchCase::IgnoreCase) &&
                     CachedProperties[i].DoorNo == SingleProp.DoorNo))
                {
                    CachedProperties[i] = SingleProp;
                    bFoundInCache = true;
                    break;
                }
            }

            if (!bFoundInCache && !SingleProp.Name.IsEmpty())
            {
                CachedProperties.Add(SingleProp);
            }

            // Broadcast single property update delegate
            OnPropertyUpdated.Broadcast(SingleProp.Name, SingleProp);

            // Broadcast full portfolio update delegate
            OnRealEstateDataReceived.Broadcast(CachedProperties);

            // Sync visualizers in the world
            SyncWorldActorsWithPayload(CachedProperties);
        }
    }
}

void USightPortalConnector::SyncWorldActorsWithPayload(const TArray<FSightPortalProperty>& Properties)
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Bridge] Synchronizing %d real-estate meshes in Unreal viewport room..."), Properties.Num());

    // Locate matching actors in active level and feed dimensions live
    // This allows real-time rendering adjustment based entirely on Google Sheet records!
    for (const FSightPortalProperty& Prop : Properties)
    {
        UE_LOG(LogTemp, Log, TEXT("[SightPortal Sync] Updating '%s' - Zone: %s - Price: %0.2f - Surface: %0.1f - Status: %s"), 
            *Prop.Name, *Prop.Zone, Prop.Price, Prop.Surface, *Prop.Availability);
        
        // Custom Blueprint code runs on OnPropertyUpdated broadcast to scale walls, load 
        // 3D image billboards, or assign catalog prices inline!
    }
}
