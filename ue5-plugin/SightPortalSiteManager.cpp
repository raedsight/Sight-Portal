#include "SightPortalSiteManager.h"
#include "SightPortalZoneManager.h"
#include "SightPortalConnector.h"
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
        
        Connector->DisconnectWebSocket();
        Connector->ConnectWebSocket();
        Connector->FetchLatestSpreadsheetData();
    }
}

void ASightPortalSiteManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearZoneManagers();

    Super::EndPlay(EndPlayReason);
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
