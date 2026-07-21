#include "BlockSpline.h"
#include "PropertyVisualizer.h"

ABlockSpline::ABlockSpline()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create the spline component and establish it as the root component
    SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
    if (SplineComponent)
    {
        SetRootComponent(SplineComponent);
    }
    
    VisualizerSpacing = 350.0f;
    VisualizerRotationOffset = FRotator::ZeroRotator;
    VisualizerScaleOffset = FVector(1.0f, 1.0f, 1.0f);
}

void ABlockSpline::BeginPlay()
{
    Super::BeginPlay();
}

void ABlockSpline::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!SplineComponent) return;

    TArray<AActor*> AttachedActors;
    GetAttachedActors(AttachedActors);

    int32 Index = 0;
    for (AActor* Child : AttachedActors)
    {
        if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
        {
            float Distance = Index * VisualizerSpacing;
            FVector SpawnLoc = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
            FRotator SpawnRot = SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

            Child->SetActorLocationAndRotation(SpawnLoc, SpawnRot + VisualizerRotationOffset);
            Child->SetActorScale3D(VisualizerScaleOffset);
            Index++;
        }
    }
}
