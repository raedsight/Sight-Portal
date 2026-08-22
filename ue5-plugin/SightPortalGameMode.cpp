#include "SightPortalGameMode.h"
#include "SightPortalPlayerController.h"
#include "GameFramework/DefaultPawn.h"

ASightPortalGameMode::ASightPortalGameMode()
{
    // Set default PlayerController class to SightPortal's interactive controller
    PlayerControllerClass = ASightPortalPlayerController::StaticClass();

    // Default flying spectator pawn for seamless ArchViz walkthroughs (can be overridden in BP)
    DefaultPawnClass = ADefaultPawn::StaticClass();
}

void ASightPortalGameMode::StartPlay()
{
    Super::StartPlay();

    UE_LOG(LogTemp, Log, TEXT("[SightPortalGameMode] Initialized with PlayerControllerClass: %s, DefaultPawnClass: %s"), 
        PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("None"),
        DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("None"));
}

