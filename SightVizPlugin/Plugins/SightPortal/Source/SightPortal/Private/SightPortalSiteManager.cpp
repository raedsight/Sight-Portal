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

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsSpawning = false;
        return;
    }

    // 1. Scan attached children to recover any untracked valid Zone Managers (preserves them across editor rebuilds/reloads)
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ZoneManagerClass))
        {
            if (!ActiveZoneManagers.Contains(Attached))
            {
                ActiveZoneManagers.Add(Attached);
            }
        }
    }

    // 2. Clean up invalid/destroyed actors from our array
    for (int32 i = ActiveZoneManagers.Num() - 1; i >= 0; --i)
    {
        if (!IsValid(ActiveZoneManagers[i]))
        {
            ActiveZoneManagers.RemoveAt(i);
        }
    }

    // 3. If any actor in our array is NOT an instance of the current ZoneManagerClass, destroy and remove it
    for (int32 i = ActiveZoneManagers.Num() - 1; i >= 0; --i)
    {
        if (IsValid(ActiveZoneManagers[i]) && !ActiveZoneManagers[i]->IsA(ZoneManagerClass))
        {
            ActiveZoneManagers[i]->Destroy();
            ActiveZoneManagers.RemoveAt(i);
        }
    }

    // 4. If we have more than ZoneCount, destroy excess ones from the end
    while (ActiveZoneManagers.Num() > ZoneCount)
    {
        AActor* ExcessActor = ActiveZoneManagers.Last();
        if (IsValid(ExcessActor))
        {
            ExcessActor->Destroy();
        }
        ActiveZoneManagers.RemoveAt(ActiveZoneManagers.Num() - 1);
    }

    // 5. Spawn only the missing Zone Managers, keeping existing ones (and their custom transforms!) completely intact
    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    for (int32 Index = ActiveZoneManagers.Num(); Index < ZoneCount; ++Index)
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
        if (!World->IsGameWorld())
        {
            SpawnParams.ObjectFlags |= RF_Transient;
        }
        #endif

        AActor* NewZoneActor = World->SpawnActor<AActor>(ZoneManagerClass, SpawnLoc, ManagerRotation, SpawnParams);
        if (NewZoneActor)
        {
            // Attach SightPortalZoneManager to SightPortalSiteManager
            NewZoneActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
            ActiveZoneManagers.Add(NewZoneActor);
            UE_LOG(LogTemp, Log, TEXT("[SightPortal SiteManager] Successfully spawned and attached Zone Manager at location %s"), *SpawnLoc.ToString());
        }
    }

    bIsSpawning = false;
}
