#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalConnector.h"
#include "PropertyVisualizer.generated.h"

/**
 * APropertyVisualizer
 * A dedicated property visualizer actor with a single variable of type FSightPortalProperty.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|PropertyVisualizer")
class SIGHTPORTAL_API APropertyVisualizer : public AActor
{
    GENERATED_BODY()

public:
    APropertyVisualizer();

protected:
    virtual void BeginPlay() override;

public:
    // A single variable of type FSightPortalProperty containing the real-estate data
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Property")
    FSightPortalProperty PropertyDetails;
};
