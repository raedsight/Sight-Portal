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

ASightPortalBlockManager* ASightPortalZoneManager::AddNewBlock()
{
    // Clean up any stale pointers in ActiveBlockManagers
    ActiveBlockManagers.RemoveAll([](AActor* Actor) { return !IsValid(Actor); });

    // Scan attached children to make sure we don't miss any valid Block Managers
    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);
    for (AActor* Attached : AttachedActors)
    {
        if (IsValid(Attached) && Attached->IsA(ASightPortalBlockManager::StaticClass()))
        {
            ActiveBlockManagers.AddUnique(Attached);
        }
    }

    int32 NewIndex = ActiveBlockManagers.Num();
    FVector SpawnLocation = GetActorLocation();

    if (NewIndex > 0)
    {
        AActor* LastActor = ActiveBlockManagers.Last();
        if (IsValid(LastActor))
        {
            SpawnLocation = LastActor->GetActorLocation() + (GetActorForwardVector() * BlockSpacing);
        }
        else
        {
            SpawnLocation = GetActorLocation() + (GetActorForwardVector() * (NewIndex * BlockSpacing));
        }
    }
    else
    {
        SpawnLocation = GetActorLocation();
    }

    FString GeneratedBlockName = FString::Printf(TEXT("%sB%d"), *ZoneName, NewIndex + 1);
    return AddBlockWithParameters(GeneratedBlockName, SpawnLocation);
}

void ASightPortalZoneManager::AddConfiguredBlock()
{
    FVector TargetLocation = GetActorLocation();

    if (bUseCustomLocationForNewBlock && !NewBlockCustomLocation.IsZero())
    {
        TargetLocation = NewBlockCustomLocation;
    }
    else
    {
        // Clean up any stale pointers in ActiveBlockManagers
        ActiveBlockManagers.RemoveAll([](AActor* Actor) { return !IsValid(Actor); });

        int32 NewIndex = ActiveBlockManagers.Num();
        if (NewIndex > 0)
        {
            AActor* LastActor = ActiveBlockManagers.Last();
            if (IsValid(LastActor))
            {
                TargetLocation = LastActor->GetActorLocation() + (GetActorForwardVector() * BlockSpacing);
            }
            else
            {
                TargetLocation = GetActorLocation() + (GetActorForwardVector() * (NewIndex * BlockSpacing));
            }
        }
    }

    FString TargetBlockName = NewBlockCustomName;
    if (TargetBlockName.IsEmpty())
    {
        int32 NewIndex = ActiveBlockManagers.Num();
        TargetBlockName = FString::Printf(TEXT("%sB%d"), *ZoneName, NewIndex + 1);
    }

    AddBlockWithParameters(TargetBlockName, TargetLocation);
}

ASightPortalBlockManager* ASightPortalZoneManager::AddBlockWithParameters(const FString& CustomBlockName, const FVector& CustomLocation)
{
    TSubclassOf<AActor> ClassToSpawn = BlockManagerClass;
    if (!ClassToSpawn)
    {
        ClassToSpawn = ASightPortalBlockManager::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal ZoneManager] BlockManagerClass not assigned in Zone Manager. Defaulting to ASightPortalBlockManager::StaticClass()."));
    }

    if (ClassToSpawn->IsChildOf(ASightPortalZoneManager::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("[SightPortal ZoneManager] ERROR: BlockManagerClass is set to ASightPortalZoneManager! Recursive spawning aborted."));
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Clean up any stale pointers in ActiveBlockManagers
    ActiveBlockManagers.RemoveAll([](AActor* Actor) { return !IsValid(Actor); });

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FRotator ManagerRotation = GetActorRotation();
    ASightPortalBlockManager* NewBlock = World->SpawnActor<ASightPortalBlockManager>(
        ClassToSpawn,
        CustomLocation,
        ManagerRotation,
        SpawnParams
    );

    if (!NewBlock)
    {
        UE_LOG(LogTemp, Error, TEXT("[SightPortal ZoneManager] Failed to spawn new Block Manager actor at %s."), *CustomLocation.ToString());
        return nullptr;
    }

    // Attach SightPortalBlockManager to SightPortalZoneManager preserving its world transform
    NewBlock->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

    // Assign Block identifier name
    int32 BlockIndex = ActiveBlockManagers.Num() + 1;
    NewBlock->BlockName = !CustomBlockName.IsEmpty() ? CustomBlockName : FString::Printf(TEXT("%sB%d"), *ZoneName, BlockIndex);

#if WITH_EDITOR
    NewBlock->SetActorLabel(NewBlock->BlockName);
#endif

    // Track in ActiveBlockManagers
    ActiveBlockManagers.Add(NewBlock);
    BlockCount = FMath::Max(BlockCount, ActiveBlockManagers.Num());

    UE_LOG(LogTemp, Log, TEXT("[SightPortal ZoneManager] Successfully added new Block '%s' at %s without respawning existing blocks. Total active blocks: %d"),
        *NewBlock->BlockName, *CustomLocation.ToString(), ActiveBlockManagers.Num());

    return NewBlock;
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
