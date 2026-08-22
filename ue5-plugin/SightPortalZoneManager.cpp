#include "SightPortalZoneManager.h"
#include "SightPortalBlockManager.h"
#include "SightPortalConnector.h"
#include "Engine/World.h"

ASightPortalZoneManager::ASightPortalZoneManager()
{
    PrimaryActorTick.bCanEverTick = false;
    BlockCount = 1;
    BlockSpacing = 1000.0f;
    ZoneName = TEXT("1");

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void ASightPortalZoneManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Removed automatic spawning to prevent resetting manual transforms
}

void ASightPortalZoneManager::BeginPlay()
{
    Super::BeginPlay();

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.AddUniqueDynamic(this, &ASightPortalZoneManager::HandleDataReceived);
        Connector->OnPropertyUpdated.AddUniqueDynamic(this, &ASightPortalZoneManager::HandleSinglePropertyUpdated);
    }
}

void ASightPortalZoneManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearBlockManagers();

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.RemoveDynamic(this, &ASightPortalZoneManager::HandleDataReceived);
        Connector->OnPropertyUpdated.RemoveDynamic(this, &ASightPortalZoneManager::HandleSinglePropertyUpdated);
    }

    Super::EndPlay(EndPlayReason);
}

void ASightPortalZoneManager::HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio)
{
    OnPortalDataReceived(PropertyPortfolio);
}

void ASightPortalZoneManager::HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData)
{
    if (PropertyData.Zone.Equals(ZoneName, ESearchCase::IgnoreCase))
    {
        OnPortalPropertyUpdated(PropertyName, PropertyData);
    }
}

void ASightPortalZoneManager::ClearBlockManagers()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal ZoneManager] Clearing %d active spawned Block Managers..."), ActiveBlockManagers.Num());

    ClearPropertyVisualizers();

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

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsSpawning = false;
        return;
    }

    // 1. Scan attached children to recover any untracked valid Block Managers (preserves them across editor rebuilds/reloads)
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);

    TArray<ASightPortalBlockManager*> ExistingBlocks;
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ASightPortalBlockManager::StaticClass()))
        {
            ExistingBlocks.Add(Cast<ASightPortalBlockManager>(Attached));
        }
    }

    // 2. Destroy excess ones
    while (ExistingBlocks.Num() > BlockCount)
    {
        ASightPortalBlockManager* Excess = ExistingBlocks.Last();
        if (IsValid(Excess))
        {
            Excess->Destroy();
        }
        ExistingBlocks.RemoveAt(ExistingBlocks.Num() - 1);
    }

    // 3. Spawn missing and arrange ALL of them correctly along the spacing vector
    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    for (int32 Index = 0; Index < BlockCount; ++Index)
    {
        // Spacing aligned orthogonally along Forward Vector of the Zone Manager
        FVector TargetLocation = ManagerLocation + (GetActorForwardVector() * (Index * BlockSpacing));

        ASightPortalBlockManager* BlockManager = nullptr;
        if (Index < ExistingBlocks.Num())
        {
            BlockManager = ExistingBlocks[Index];
            if (IsValid(BlockManager) && !BlockManager->bHasBeenManuallyMoved)
            {
                BlockManager->SetActorLocation(TargetLocation);
            }
        }
        else
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            BlockManager = World->SpawnActor<ASightPortalBlockManager>(
                BlockManagerClass,
                TargetLocation,
                ManagerRotation,
                SpawnParams
            );

            if (BlockManager)
            {
                // Attach SightPortalBlockManager to SightPortalZoneManager
                BlockManager->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                ExistingBlocks.Add(BlockManager);
                UE_LOG(LogTemp, Log, TEXT("[SightPortal ZoneManager] Successfully spawned and attached Block Manager at location %s"), *TargetLocation.ToString());
            }
        }

        if (BlockManager)
        {
            // Assign Block Name based on parent zone name and index (e.g. Z1B1, Z1B2...)
            BlockManager->BlockName = FString::Printf(TEXT("%sB%d"), *ZoneName, Index + 1);
#if WITH_EDITOR
            BlockManager->SetActorLabel(BlockManager->BlockName);
#endif
        }
    }

    ActiveBlockManagers.Empty();
    for (ASightPortalBlockManager* Block : ExistingBlocks)
    {
        ActiveBlockManagers.Add(Block);
    }

    bIsSpawning = false;
}

void ASightPortalZoneManager::SpawnPropertyVisualizers()
{
    for (AActor* BlockActor : ActiveBlockManagers)
    {
        ASightPortalBlockManager* Block = Cast<ASightPortalBlockManager>(BlockActor);
        if (IsValid(Block))
        {
            Block->SpawnPropertyVisualizers();
        }
    }
}

void ASightPortalZoneManager::ClearPropertyVisualizers()
{
    for (AActor* BlockActor : ActiveBlockManagers)
    {
        ASightPortalBlockManager* Block = Cast<ASightPortalBlockManager>(BlockActor);
        if (IsValid(Block))
        {
            Block->ClearPropertyVisualizers();
        }
    }
}

#if WITH_EDITOR
void ASightPortalZoneManager::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;
}
#endif
