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
    bAutoManageSplinePoints = true;
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

    int32 PropertyCount = 0;
    for (AActor* Child : AttachedActors)
    {
        if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
        {
            PropertyCount++;
        }
    }

    if (bAutoManageSplinePoints && PropertyCount > 0)
    {
        float TargetSplineLength = PropertyCount * VisualizerSpacing;
        SplineComponent->ClearSplinePoints(false);
        SplineComponent->AddSplinePoint(FVector(0.f, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
        SplineComponent->AddSplinePoint(FVector(TargetSplineLength, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
        SplineComponent->UpdateSpline();
    }

    int32 Index = 0;
    for (AActor* Child : AttachedActors)
    {
        if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
        {
            APropertyVisualizer* PropVis = Cast<APropertyVisualizer>(Child);
            if (PropVis)
            {
                float Distance = Index * VisualizerSpacing;
                FVector SpawnLoc = SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator SpawnRot = SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

                if (!PropVis->bHasBeenManuallyMoved)
                {
                    PropVis->SetActorLocationAndRotation(SpawnLoc, SpawnRot + VisualizerRotationOffset);
                    PropVis->SetActorScale3D(VisualizerScaleOffset);
                }
                else
                {
                    FTransform DefaultTransform(SpawnRot + VisualizerRotationOffset, SpawnLoc, VisualizerScaleOffset);
                    FTransform FinalTransform = PropVis->ManualRelativeTransform * DefaultTransform;
                    PropVis->SetActorTransform(FinalTransform);
                }
            }
            Index++;
        }
    }
}

#if WITH_EDITOR
void ABlockSpline::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;
}
#endif
