#include "SightPortalGameMode.h"
#include "SightPortalPlayerController.h"

ASightPortalGameMode::ASightPortalGameMode()
{
    // Set default PlayerController class to SightPortal's interactive controller
    PlayerControllerClass = ASightPortalPlayerController::StaticClass();

    // Default spectator or pawn class (can be customized or overridden in BP)
    DefaultPawnClass = nullptr;
}

void ASightPortalGameMode::StartPlay()
{
    Super::StartPlay();

    UE_LOG(LogTemp, Log, TEXT("[SightPortalGameMode] Initialized with PlayerControllerClass: %s"), 
        PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("None"));
}
