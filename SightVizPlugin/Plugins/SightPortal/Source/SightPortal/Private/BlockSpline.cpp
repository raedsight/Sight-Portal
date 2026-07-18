#include "BlockSpline.h"

ABlockSpline::ABlockSpline()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create the spline component and establish it as the root component
    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    if (SplineComponent)
    {
        SetRootComponent(SplineComponent);
    }
}

void ABlockSpline::BeginPlay()
{
    Super::BeginPlay();
}
