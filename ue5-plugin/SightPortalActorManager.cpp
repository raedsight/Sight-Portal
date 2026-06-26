#include "SightPortalActorManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

ASightPortalActorManager::ASightPortalActorManager()
{
    PrimaryActorTick.bCanEverTick = false;
    ActiveSubsystem = nullptr;
    GridSpacing = 350.0f;
    SpawnZOffset = 0.0f;
}

void ASightPortalActorManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    #if WITH_EDITOR
    UWorld* World = GetWorld();
    if (World && !World->IsGameWorld())
    {
        UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Editor Construction active. Binding to Engine Subsystem..."));
        
        if (GEngine)
        {
            ActiveSubsystem = GEngine->GetEngineSubsystem<USightPortalConnector>();
            if (ActiveSubsystem)
            {
                ActiveSubsystem->RemoteEndpointURL = RemoteEndpointURL;
                ActiveSubsystem->WebSocketURL = WebSocketURL;
                ActiveSubsystem->OnRealEstateDataReceived.RemoveAll(this);
                ActiveSubsystem->OnPropertyUpdated.RemoveAll(this);

                ActiveSubsystem->OnRealEstateDataReceived.AddDynamic(this, &ASightPortalActorManager::OnDataReceived);
                ActiveSubsystem->OnPropertyUpdated.AddDynamic(this, &ASightPortalActorManager::OnPropertyDetailUpdated);

                UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Subsystem connection succeeded and listener events bound in-editor."));
                
                // Trigger instant layout from the cache if available, otherwise fetch
                if (ActiveSubsystem->CachedProperties.Num() > 0)
                {
                    OnDataReceived(ActiveSubsystem->CachedProperties);
                }
                else
                {
                    ActiveSubsystem->FetchLatestSpreadsheetData();
                }
            }
        }
    }
    #endif
}

void ASightPortalActorManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Actor Manager Booted. Binding to Engine Subsystem..."));

    if (GEngine)
    {
        ActiveSubsystem = GEngine->GetEngineSubsystem<USightPortalConnector>();
        if (ActiveSubsystem)
        {
            // Bind C++ methods to Dynamic Multicast Delegates
            ActiveSubsystem->OnRealEstateDataReceived.RemoveAll(this);
            ActiveSubsystem->OnPropertyUpdated.RemoveAll(this);

            ActiveSubsystem->OnRealEstateDataReceived.AddDynamic(this, &ASightPortalActorManager::OnDataReceived);
            ActiveSubsystem->OnPropertyUpdated.AddDynamic(this, &ASightPortalActorManager::OnPropertyDetailUpdated);
            
            UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Subsystem connection succeeded and listener events bound."));

            // If we have cached properties, instantly spawn them so the game session initializes with valid actors!
            if (ActiveSubsystem->CachedProperties.Num() > 0)
            {
                OnDataReceived(ActiveSubsystem->CachedProperties);
            }

            // Fresh background fetch/poll to verify we have the absolute freshest data on boot
            ActiveSubsystem->FetchLatestSpreadsheetData();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[SightPortal Manager] USightPortalConnector subsystem is missing from GEngine."));
        }
    }
}

void ASightPortalActorManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean-up delegate listeners if Subsystem is active
    if (ActiveSubsystem)
    {
        ActiveSubsystem->OnRealEstateDataReceived.RemoveDynamic(this, &ASightPortalActorManager::OnDataReceived);
        ActiveSubsystem->OnPropertyUpdated.RemoveDynamic(this, &ASightPortalActorManager::OnPropertyDetailUpdated);
    }

    ClearActiveSpawnedActors();

    Super::EndPlay(EndPlayReason);
}

void ASightPortalActorManager::OnDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio)
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Received a full portfolio callback with %d items. Running layout loop..."), PropertyPortfolio.Num());

    // Call default spawning implementation
    SpawnAndArrangePortfolio(PropertyPortfolio);

    // Call the Blueprint Implementable Event
    OnPortfolioSynchronized(PropertyPortfolio);
}

void ASightPortalActorManager::OnPropertyDetailUpdated(const FString& Name, const FSightPortalProperty& PropertyDetails)
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Property Event triggered: '%s' pricing updated to $%0.2f"), *Name, PropertyDetails.Price);

    // Trigger Blueprint graph customization branch
    OnPropertySyncUpdated(Name, PropertyDetails);
}

void ASightPortalActorManager::ClearActiveSpawnedActors()
{
    UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Clearing %d active spawned property visualizers..."), ActiveSpawnedActors.Num());

    for (AActor* Actor : ActiveSpawnedActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    ActiveSpawnedActors.Empty();
}

void ASightPortalActorManager::SpawnAndArrangePortfolio(const TArray<FSightPortalProperty>& Properties)
{
    // Clear out any old instances
    ClearActiveSpawnedActors();

    UWorld* World = GetWorld();
    if (!World) return;

    // Use PropertyVisualizerTemplate if defined, otherwise log a warning
    if (!PropertyVisualizerTemplate)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SightPortal Manager] PropertyVisualizerTemplate is not assigned in Actor Manager! Define your visualizer Blueprint Template to enable local 3D spawner grids."));
        return;
    }

    FVector ManagerLocation = GetActorLocation();
    FRotator ManagerRotation = GetActorRotation();

    for (int32 Index = 0; Index < Properties.Num(); ++Index)
    {
        const FSightPortalProperty& PropertyData = Properties[Index];

        // Position grid alignment based on spacing parameter
        FVector SpawnLoc = ManagerLocation + (GetActorRightVector() * (Index * GridSpacing));
        SpawnLoc.Z += SpawnZOffset;

        // Ensure SpawnLoc components are finite and within double limits to prevent DoubleFloat.cpp crashes (Large World Coordinates limits)
        if (SpawnLoc.ContainsNaN() || SpawnLoc.Size() > 1e12)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SightPortal Manager] Spawn location %s is invalid, contains NaN, or exceeds safe boundaries. Skipping spawn for '%s' to safeguard engine runtime stability! Check GridSpacing or Actor placement."), *SpawnLoc.ToString(), *PropertyData.Name);
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

        AActor* NewPropertyActor = World->SpawnActor<AActor>(PropertyVisualizerTemplate, SpawnLoc, ManagerRotation, SpawnParams);
        if (NewPropertyActor)
        {
            ActiveSpawnedActors.Add(NewPropertyActor);
            
            UE_LOG(LogTemp, Log, TEXT("[SightPortal Manager] Successfully spawned visual actor for '%s' at vector %s"), 
                *PropertyData.Name, *SpawnLoc.ToString());
        }
    }
}
