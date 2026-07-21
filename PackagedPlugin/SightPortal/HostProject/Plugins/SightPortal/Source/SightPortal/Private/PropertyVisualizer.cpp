#include "PropertyVisualizer.h"
#include "BlockSpline.h"

APropertyVisualizer::APropertyVisualizer()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    ManualRelativeTransform = FTransform::Identity;
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

    // Get the parent spline actor (RowSpline)
    AActor* AttachParent = GetAttachParentActor();
    if (AttachParent && AttachParent->IsA(ABlockSpline::StaticClass()))
    {
        ABlockSpline* BlockSpline = Cast<ABlockSpline>(AttachParent);
        if (BlockSpline && BlockSpline->SplineComponent)
        {
            TArray<AActor*> AttachedActors;
            BlockSpline->GetAttachedActors(AttachedActors);

            int32 MyIndex = -1;
            int32 Index = 0;
            for (AActor* Child : AttachedActors)
            {
                if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
                {
                    if (Child == this)
                    {
                        MyIndex = Index;
                        break;
                    }
                    Index++;
                }
            }

            if (MyIndex != -1)
            {
                float Distance = MyIndex * BlockSpline->VisualizerSpacing;
                FVector DefaultLoc = BlockSpline->SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator DefaultRot = BlockSpline->SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + BlockSpline->VisualizerRotationOffset;
                FVector DefaultScale = BlockSpline->VisualizerScaleOffset;

                FTransform DefaultTransform(DefaultRot, DefaultLoc, DefaultScale);
                FTransform ActualTransform = GetActorTransform();

                ManualRelativeTransform = ActualTransform.GetRelativeTransform(DefaultTransform);
            }
        }
    }
}
#endif
