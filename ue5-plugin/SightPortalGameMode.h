#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SightPortalPlayerController.h"
#include "SightPortalGameMode.generated.h"

/**
 * ASightPortalGameMode
 * Dedicated GameMode for SightPortal ArchViz architectural walkthroughs in Unreal Engine 5.
 * 
 * Configures:
 * - PlayerControllerClass -> ASightPortalPlayerController (Mouse click/tap picking & 2D detail HUD toggle)
 * - DefaultPawnClass -> Configurable spectator / archviz camera or pawn
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API ASightPortalGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASightPortalGameMode();

    virtual void StartPlay() override;
};
