#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "BlockSpline.generated.h"

/**
 * ABlockSpline
 * An actor that contains a spline component to guide property visualizer layout.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|BlockSpline")
class SIGHTPORTAL_API ABlockSpline : public AActor
{
    GENERATED_BODY()

public:
    ABlockSpline();

protected:
    virtual void BeginPlay() override;

public:
    virtual void OnConstruction(const FTransform& Transform) override;

    // Spline component contained in this actor
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|Spline", meta = (AllowPrivateAccess = "true"))
    USplineComponent* SplineComponent;

    // Spacing between visualizers along this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Spline")
    float VisualizerSpacing = 350.0f;

    // Extra rotation offset applied to visualizers on this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Spline")
    FRotator VisualizerRotationOffset = FRotator::ZeroRotator;

    // Scale applied to visualizers on this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Spline")
    FVector VisualizerScaleOffset = FVector(1.0f, 1.0f, 1.0f);
};
