#include "SightPortalSiteManager.h"
#include "SightPortalZoneManager.h"
#include "SightPortalConnector.h"
#include "PropertyVisualizer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

ASightPortalSiteManager::ASightPortalSiteManager()
{
    PrimaryActorTick.bCanEverTick = false;
    ZoneCount = 1;
    ZoneSpacing = 1500.0f;
    WebSocketURL = TEXT("wss://ais-pre-4wjcvfkjzt7ohntjrl7gk5-405891248157.europe-west3.run.app/ws/hyperion-vis");
    RemoteEndpointURL = TEXT("https://sight-portal-1127775803.europe-west2.run.app/api/health");

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void ASightPortalSiteManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Removed automatic spawning to prevent resetting manual transforms

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        // Bind callback to ensure we receive updates dynamically in the editor
        if (!Connector->OnRealEstateDataReceived.IsAlreadyBound(this, &ASightPortalSiteManager::HandleDataReceived))
        {
            Connector->OnRealEstateDataReceived.AddDynamic(this, &ASightPortalSiteManager::HandleDataReceived);
        }

        // Dynamically update property visualizers with latest cached data during construction
        if (Connector->CachedProperties.Num() > 0)
        {
            HandleDataReceived(Connector->CachedProperties);
        }
    }
}

void ASightPortalSiteManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Site Manager Booted."));

    // Sync URLs to Connector on start
    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->WebSocketURL = WebSocketURL;
        Connector->RemoteEndpointURL = RemoteEndpointURL;

        // Bind callback for when data is fetched/updated
        if (!Connector->OnRealEstateDataReceived.IsAlreadyBound(this, &ASightPortalSiteManager::HandleDataReceived))
        {
            Connector->OnRealEstateDataReceived.AddDynamic(this, &ASightPortalSiteManager::HandleDataReceived);
        }

        if (!Connector->OnPropertyUpdated.IsAlreadyBound(this, &ASightPortalSiteManager::HandleSinglePropertyUpdated))
        {
            Connector->OnPropertyUpdated.AddDynamic(this, &ASightPortalSiteManager::HandleSinglePropertyUpdated);
        }
        
        Connector->DisconnectWebSocket();
        Connector->ConnectWebSocket();
        Connector->FetchLatestSpreadsheetData();
    }
}

void ASightPortalSiteManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearZoneManagers();

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.RemoveDynamic(this, &ASightPortalSiteManager::HandleDataReceived);
        Connector->OnPropertyUpdated.RemoveDynamic(this, &ASightPortalSiteManager::HandleSinglePropertyUpdated);
    }

    Super::EndPlay(EndPlayReason);
}

void ASightPortalSiteManager::Destroyed()
{
    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.RemoveDynamic(this, &ASightPortalSiteManager::HandleDataReceived);
        Connector->OnPropertyUpdated.RemoveDynamic(this, &ASightPortalSiteManager::HandleSinglePropertyUpdated);
    }

    Super::Destroyed();
}

void ASightPortalSiteManager::ClearZoneManagers()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Clearing %d active spawned Zone Managers..."), ActiveZoneManagers.Num());

    ClearPropertyVisualizers();

    for (AActor* Actor : ActiveZoneManagers)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    ActiveZoneManagers.Empty();
}

void ASightPortalSiteManager::SpawnZoneManagers()
{
    if (bIsSpawning)
    {
        return;
    }

    // Use ZoneManagerClass if defined, otherwise log a warning
    if (!ZoneManagerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal SiteManager] ZoneManagerClass is not assigned in Site Manager! Define your Zone Manager blueprint to enable local 3D Zone spawning."));
        return;
    }

    // Safety check: Ensure the template is not this class itself
    if (ZoneManagerClass->IsChildOf(ASightPortalSiteManager::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("[SightPortal SiteManager] ERROR: ZoneManagerClass is set to ASightPortalSiteManager! This will cause a recursive crash. Aborting spawning."));
        return;
    }

    bIsSpawning = true;

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsSpawning = false;
        return;
    }

    // 1. Scan attached children to recover any untracked valid Zone Managers (preserves them across editor rebuilds/reloads)
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);

    TArray<ASightPortalZoneManager*> ExistingZones;
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ASightPortalZoneManager::StaticClass()))
        {
            ExistingZones.Add(Cast<ASightPortalZoneManager>(Attached));
        }
    }

    // 2. Destroy excess ones
    while (ExistingZones.Num() > ZoneCount)
    {
        ASightPortalZoneManager* Excess = ExistingZones.Last();
        if (IsValid(Excess))
        {
            Excess->Destroy();
        }
        ExistingZones.RemoveAt(ExistingZones.Num() - 1);
    }

    // 3. Spawn missing and arrange ALL of them correctly along the spacing vector
    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    for (int32 Index = 0; Index < ZoneCount; ++Index)
    {
        // Position alignment based on spacing parameter
        FVector TargetLocation = ManagerLocation + (GetActorRightVector() * (Index * ZoneSpacing));

        ASightPortalZoneManager* TargetZone = nullptr;
        if (Index < ExistingZones.Num())
        {
            TargetZone = ExistingZones[Index];
            if (IsValid(TargetZone) && !TargetZone->bHasBeenManuallyMoved)
            {
                TargetZone->SetActorLocation(TargetLocation);
            }
        }
        else
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            TargetZone = World->SpawnActor<ASightPortalZoneManager>(
                ZoneManagerClass,
                TargetLocation,
                ManagerRotation,
                SpawnParams
            );

            if (TargetZone)
            {
                // Attach SightPortalZoneManager to SightPortalSiteManager
                TargetZone->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                ExistingZones.Add(TargetZone);
                UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Successfully spawned and attached Zone Manager at location %s"), *TargetLocation.ToString());
            }
        }

        if (IsValid(TargetZone))
        {
            // Assign Zone Name (Z1, Z2, Z3...)
            TargetZone->ZoneName = FString::Printf(TEXT("Z%d"), Index + 1);
#if WITH_EDITOR
            TargetZone->SetActorLabel(TargetZone->ZoneName);
#endif
        }
    }

    ActiveZoneManagers.Empty();
    for (ASightPortalZoneManager* Zone : ExistingZones)
    {
        ActiveZoneManagers.Add(Zone);
    }

    bIsSpawning = false;
}

ASightPortalZoneManager* ASightPortalSiteManager::AddNewZone()
{
    // Clean up any stale pointers in ActiveZoneManagers
    ActiveZoneManagers.RemoveAll([](AActor* Actor) { return !IsValid(Actor); });

    // Also scan attached children to make sure we don't miss any valid Zone Managers
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ASightPortalZoneManager::StaticClass()))
        {
            ActiveZoneManagers.AddUnique(Attached);
        }
    }

    int32 NewIndex = ActiveZoneManagers.Num();
    FVector SpawnLocation = GetActorLocation();

    if (NewIndex > 0)
    {
        AActor* LastActor = ActiveZoneManagers.Last();
        if (IsValid(LastActor))
        {
            SpawnLocation = LastActor->GetActorLocation() + (GetActorRightVector() * ZoneSpacing);
        }
        else
        {
            SpawnLocation = GetActorLocation() + (GetActorRightVector() * (NewIndex * ZoneSpacing));
        }
    }
    else
    {
        SpawnLocation = GetActorLocation();
    }

    FString GeneratedZoneName = FString::Printf(TEXT("Z%d"), NewIndex + 1);
    return AddZoneWithParameters(GeneratedZoneName, SpawnLocation);
}

ASightPortalZoneManager* ASightPortalSiteManager::AddZoneWithParameters(const FString& CustomZoneName, const FVector& CustomLocation)
{
    // Validate ZoneManagerClass or fallback to default
    TSubclassOf<AActor> ClassToSpawn = ZoneManagerClass;
    if (!ClassToSpawn)
    {
        ClassToSpawn = ASightPortalZoneManager::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal SiteManager] ZoneManagerClass not assigned in Site Manager. Defaulting to ASightPortalZoneManager::StaticClass()."));
    }

    if (ClassToSpawn->IsChildOf(ASightPortalSiteManager::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("[SightPortal SiteManager] ERROR: ZoneManagerClass is set to ASightPortalSiteManager! Recursive spawning aborted."));
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Clean up any stale pointers in ActiveZoneManagers
    ActiveZoneManagers.RemoveAll([](AActor* Actor) { return !IsValid(Actor); });

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FRotator ManagerRotation = GetActorRotation();
    ASightPortalZoneManager* NewZone = World->SpawnActor<ASightPortalZoneManager>(
        ClassToSpawn,
        CustomLocation,
        ManagerRotation,
        SpawnParams
    );

    if (!NewZone)
    {
        UE_LOG(LogTemp, Error, TEXT("[SightPortal SiteManager] Failed to spawn new Zone Manager actor at %s."), *CustomLocation.ToString());
        return nullptr;
    }

    // Attach SightPortalZoneManager to SightPortalSiteManager preserving its world transform
    NewZone->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

    // Assign Zone identifier name
    int32 ZoneIndex = ActiveZoneManagers.Num() + 1;
    NewZone->ZoneName = !CustomZoneName.IsEmpty() ? CustomZoneName : FString::Printf(TEXT("Z%d"), ZoneIndex);

#if WITH_EDITOR
    NewZone->SetActorLabel(NewZone->ZoneName);
#endif

    // Track in ActiveZoneManagers
    ActiveZoneManagers.Add(NewZone);
    ZoneCount = FMath::Max(ZoneCount, ActiveZoneManagers.Num());

    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Successfully added new Zone '%s' at %s without respawning existing zones. Total active zones: %d"),
        *NewZone->ZoneName, *CustomLocation.ToString(), ActiveZoneManagers.Num());

    return NewZone;
}

void ASightPortalSiteManager::SpawnPropertyVisualizers()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Site-wide Spawning/Syncing of Property Visualizers initiated."));
    for (AActor* ZoneActor : ActiveZoneManagers)
    {
        ASightPortalZoneManager* Zone = Cast<ASightPortalZoneManager>(ZoneActor);
        if (IsValid(Zone))
        {
            Zone->SpawnPropertyVisualizers();
        }
    }
}

void ASightPortalSiteManager::ClearPropertyVisualizers()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Site-wide Clearing of Property Visualizers initiated."));
    for (AActor* ZoneActor : ActiveZoneManagers)
    {
        ASightPortalZoneManager* Zone = Cast<ASightPortalZoneManager>(ZoneActor);
        if (IsValid(Zone))
        {
            Zone->ClearPropertyVisualizers();
        }
    }
    RegisteredPropertyVisualizers.Empty();
}

void ASightPortalSiteManager::RegisterPropertyVisualizer(const FString& PropertyName, AActor* VisualizerActor)
{
    if (PropertyName.IsEmpty() || !IsValid(VisualizerActor)) return;
    RegisteredPropertyVisualizers.Add(PropertyName, VisualizerActor);
}

void ASightPortalSiteManager::UnregisterPropertyVisualizer(const FString& PropertyName)
{
    if (PropertyName.IsEmpty()) return;
    RegisteredPropertyVisualizers.Remove(PropertyName);
}

AActor* ASightPortalSiteManager::GetRegisteredPropertyVisualizer(const FString& PropertyName) const
{
    if (PropertyName.IsEmpty()) return nullptr;
    const AActor* const* FoundActorPtr = RegisteredPropertyVisualizers.Find(PropertyName);
    return FoundActorPtr ? const_cast<AActor*>(*FoundActorPtr) : nullptr;
}

void ASightPortalSiteManager::ForceFetchData()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Force fetch initiated. Syncing variables and connecting..."));

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->WebSocketURL = WebSocketURL;
        Connector->RemoteEndpointURL = RemoteEndpointURL;

        // Ensure we are bound to receive the update dynamically in the editor
        if (!Connector->OnRealEstateDataReceived.IsAlreadyBound(this, &ASightPortalSiteManager::HandleDataReceived))
        {
            Connector->OnRealEstateDataReceived.AddDynamic(this, &ASightPortalSiteManager::HandleDataReceived);
        }

        // Apply any cached properties immediately for instant visual feedback in the editor
        if (Connector->CachedProperties.Num() > 0)
        {
            HandleDataReceived(Connector->CachedProperties);
        }

        // Force disconnect and reconnect WebSocket using the new URL
        Connector->DisconnectWebSocket();
        Connector->ConnectWebSocket();

        // Trigger an HTTP poll to pull the latest spreadsheet data immediately
        Connector->FetchLatestSpreadsheetData();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal SiteManager] USightPortalConnector subsystem not found during ForceFetchData."));
    }
}

#if WITH_EDITOR
void ASightPortalSiteManager::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;
}
#endif

void ASightPortalSiteManager::HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio)
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Received spreadsheet data updates. Syncing %d property records..."), PropertyPortfolio.Num());

    for (const FSightPortalProperty& Prop : PropertyPortfolio)
    {
        APropertyVisualizer* TargetVis = nullptr;

        // 1. Try exact lookup in the directory map
        AActor* VisualizerActor = GetRegisteredPropertyVisualizer(Prop.Name);
        if (IsValid(VisualizerActor))
        {
            TargetVis = Cast<APropertyVisualizer>(VisualizerActor);
        }

        // 2. If not found by name, try finding by Zone, Block, and DoorNo across all registered visualizers
        if (!TargetVis)
        {
            for (auto& Elem : RegisteredPropertyVisualizers)
            {
                AActor* Actor = Elem.Value;
                if (IsValid(Actor))
                {
                    APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Actor);
                    if (PropVis && 
                        PropVis->PropertyDetails.Zone.Equals(Prop.Zone, ESearchCase::IgnoreCase) &&
                        PropVis->PropertyDetails.Block.Equals(Prop.Block, ESearchCase::IgnoreCase) &&
                        PropVis->PropertyDetails.DoorNo == Prop.DoorNo)
                    {
                        TargetVis = PropVis;
                        break;
                    }
                }
            }
        }

        // 3. If still not found, search by exact name match in the registered visualizers' properties
        if (!TargetVis)
        {
            for (auto& Elem : RegisteredPropertyVisualizers)
            {
                AActor* Actor = Elem.Value;
                if (IsValid(Actor))
                {
                    APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Actor);
                    if (PropVis && PropVis->PropertyDetails.Name.Equals(Prop.Name, ESearchCase::IgnoreCase))
                    {
                        TargetVis = PropVis;
                        break;
                    }
                }
            }
        }

        // If we found the visualizer, update its details!
        if (TargetVis)
        {
            FString OldName = TargetVis->PropertyDetails.Name;
            
            TargetVis->SetPropertyDetails(Prop);

#if WITH_EDITOR
            TargetVis->SetActorLabel(Prop.Name);
#endif

            // Keep the map registry updated if the name changes
            if (!OldName.Equals(Prop.Name, ESearchCase::IgnoreCase))
            {
                UnregisterPropertyVisualizer(OldName);
                RegisterPropertyVisualizer(Prop.Name, TargetVis);
            }

            UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Updated PropertyDetails for visualizer '%s'"), *Prop.Name);
        }
    }

    // Broadcast the callback to Blueprint listeners so they can handle the data elsewhere
    OnDataReceived.Broadcast(PropertyPortfolio);

    // Trigger Blueprint Implementable Event for full portfolio sync
    OnPortalDataReceived(PropertyPortfolio);
}

void ASightPortalSiteManager::HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData)
{
    // Find matching visualizer in registered properties
    APropertyVisualizer* TargetVis = Cast<APropertyVisualizer>(GetRegisteredPropertyVisualizer(PropertyName));

    if (!TargetVis)
    {
        for (auto& Elem : RegisteredPropertyVisualizers)
        {
            AActor* Actor = Elem.Value;
            if (IsValid(Actor))
            {
                APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Actor);
                if (PropVis)
                {
                    const bool bNameMatches = PropVis->PropertyDetails.Name.Equals(PropertyName, ESearchCase::IgnoreCase) ||
                                              PropVis->PropertyDetails.Name.Equals(PropertyData.Name, ESearchCase::IgnoreCase);
                    const bool bLocationMatches = !PropertyData.Zone.IsEmpty() &&
                                                  PropVis->PropertyDetails.Zone.Equals(PropertyData.Zone, ESearchCase::IgnoreCase) &&
                                                  PropVis->PropertyDetails.Block.Equals(PropertyData.Block, ESearchCase::IgnoreCase) &&
                                                  PropVis->PropertyDetails.DoorNo == PropertyData.DoorNo;

                    if (bNameMatches || bLocationMatches)
                    {
                        TargetVis = PropVis;
                        break;
                    }
                }
            }
        }
    }

    if (TargetVis)
    {
        TargetVis->SetPropertyDetails(PropertyData);
    }

    // Trigger Blueprint Implementable Event for single property updates
    OnPortalPropertyUpdated(PropertyName, PropertyData);
}
