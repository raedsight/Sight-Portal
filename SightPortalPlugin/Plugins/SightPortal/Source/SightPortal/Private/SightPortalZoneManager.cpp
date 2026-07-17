#include "SightPortalZoneManager.h"
#include "Engine/World.h"

ASightPortalZoneManager::ASightPortalZoneManager()
{
    PrimaryActorTick.bCanEverTick = false;
    BlockCount = 1;
    BlockSpacing = 1000.0f;
}

void ASightPortalZoneManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Spawns block managers dynamically in response to parameter updates in the Editor viewport
    SpawnBlockManagers();
}

void ASightPortalZoneManager::BeginPlay()
{
    Super::BeginPlay();

    SpawnBlockManagers();
}

void ASightPortalZoneManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearBlockManagers();

    Super::EndPlay(EndPlayReason);
}

void ASightPortalZoneManager::ClearBlockManagers()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal ZoneManager] Clearing %d active spawned Block Managers..."), ActiveBlockManagers.Num());

    for (AActor* Actor : ActiveBlockManagers)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    ActiveBlockManagers.Empty();
}

void ASightPortalZoneManager::SpawnBlockManagers()
{
    if (bIsSpawning)
    {
        return;
    }

    // Use BlockManagerClass if defined, otherwise log a warning
    if (!BlockManagerClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal ZoneManager] BlockManagerClass is not assigned in Zone Manager! Define your Block Manager blueprint to enable local 3D Block spawning."));
        return;
    }

    // Safety check: Ensure the template is not this class itself
    if (BlockManagerClass->IsChildOf(ASightPortalZoneManager::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("[SightPortal ZoneManager] ERROR: BlockManagerClass is set to ASightPortalZoneManager! This will cause a recursive crash. Aborting spawning."));
        return;
    }

    bIsSpawning = true;

    // Clear out any old instances
    ClearBlockManagers();

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsSpawning = false;
        return;
    }

    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    for (int32 Index = 0; Index < BlockCount; ++Index)
    {
        // Spacing aligned orthogonally along Forward Vector of the Zone Manager
        FVector SpawnLoc = ManagerLocation + (GetActorForwardVector() * (Index * BlockSpacing));

        // Ensure SpawnLoc components are finite
        if (SpawnLoc.ContainsNaN() || SpawnLoc.Size() > 1e12)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SightPortal ZoneManager] Spawn location %s is invalid. Skipping spawn for Block index %d"), *SpawnLoc.ToString(), Index);
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

        AActor* NewBlockActor = World->SpawnActor<AActor>(BlockManagerClass, SpawnLoc, ManagerRotation, SpawnParams);
        if (NewBlockActor)
        {
            ActiveBlockManagers.Add(NewBlockActor);
            UE_LOG(LogTemp, Log, TEXT("[SightPortal ZoneManager] Successfully spawned Block Manager at location %s"), *SpawnLoc.ToString());
        }
    }

    bIsSpawning = false;
}
