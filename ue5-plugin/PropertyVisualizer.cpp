#include "PropertyVisualizer.h"

APropertyVisualizer::APropertyVisualizer()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void APropertyVisualizer::BeginPlay()
{
    Super::BeginPlay();
}

#if WITH_EDITOR
void APropertyVisualizer::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;
}
#endif
