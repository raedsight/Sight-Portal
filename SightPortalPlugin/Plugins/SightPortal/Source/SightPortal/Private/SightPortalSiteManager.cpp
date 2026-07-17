#include "SightPortalSiteManager.h"
#include "Engine/World.h"

ASightPortalSiteManager::ASightPortalSiteManager()
{
    PrimaryActorTick.bCanEverTick = false;
    ZoneCount = 1;
    ZoneSpacing = 1500.0f;
}

void ASightPortalSiteManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Spawns zones dynamically in response to parameter updates in the Editor viewport
    SpawnZoneManagers();
}

void ASightPortalSiteManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Site Manager Booted."));

    SpawnZoneManagers();
}

void ASightPortalSiteManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearZoneManagers();

    Super::EndPlay(EndPlayReason);
}

void ASightPortalSiteManager::ClearZoneManagers()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Clearing %d active spawned Zone Managers..."), ActiveZoneManagers.Num());

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

    // Clear out any old instances
    ClearZoneManagers();

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsSpawning = false;
        return;
    }

    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    for (int32 Index = 0; Index < ZoneCount; ++Index)
    {
        // Position grid/linear alignment based on spacing parameter
        FVector SpawnLoc = ManagerLocation + (GetActorRightVector() * (Index * ZoneSpacing));

        // Ensure SpawnLoc components are finite
        if (SpawnLoc.ContainsNaN() || SpawnLoc.Size() > 1e12)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SightPortal SiteManager] Spawn location %s is invalid. Skipping spawn for Zone index %d"), *SpawnLoc.ToString(), Index);
            continue;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        #if WITH_EDITOR
        if (World && !World->IsGameWorld())
        {
            SpawnParams.ObjectFlags |= RF_Transient;
        }
        #endif

        AActor* NewZoneActor = World->SpawnActor<AActor>(ZoneManagerClass, SpawnLoc, ManagerRotation, SpawnParams);
        if (NewZoneActor)
        {
            ActiveZoneManagers.Add(NewZoneActor);
            UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Successfully spawned Zone Manager at location %s"), *SpawnLoc.ToString());
        }
    }

    bIsSpawning = false;
}
