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
    // Spline component contained in this actor
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|Spline", meta = (AllowPrivateAccess = "true"))
    USplineComponent* SplineComponent;
};
