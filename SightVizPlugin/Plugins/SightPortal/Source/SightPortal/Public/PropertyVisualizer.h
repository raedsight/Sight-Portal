#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalConnector.h"
#include "Components/WidgetComponent.h"
#include "SightPortal3DPropertyWidget.h"
#include "SightPortal2DPropertyDetailWidget.h"
#include "PropertyVisualizer.generated.h"

/**
 * APropertyVisualizer
 * A dedicated property visualizer actor with a single variable of type FSightPortalProperty.
 * Integrates 3D World Space Widget (surface, bedrooms, explore button) and 2D Detail HUD Widget.
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

    // 3D Widget Component attached to this actor in world space
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|UI")
    UWidgetComponent* Widget3DComponent;

    // Subclass for 3D Property Widget (Defaults to USightPortal3DPropertyWidget)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UI")
    TSubclassOf<USightPortal3DPropertyWidget> Widget3DClass;

    // Subclass for 2D Property Detail Screen Widget (Defaults to USightPortal2DPropertyDetailWidget)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UI")
    TSubclassOf<USightPortal2DPropertyDetailWidget> Detail2DWidgetClass;

    // Active 2D Detail Widget instance when Explore is opened
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UI")
    USightPortal2DPropertyDetailWidget* Active2DDetailWidget;

    // Set and synchronize property data, updating the 3D widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Property")
    void SetPropertyDetails(const FSightPortalProperty& InDetails);

    // Open/Show the 2D Detail Widget on player viewport displaying full property details
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    USightPortal2DPropertyDetailWidget* OpenPropertyDetail2DWidget();

    // Callback bound to 3D widget's Explore button
    UFUNCTION()
    void OnExploreRequestedFrom3DWidget(const FSightPortalProperty& InProperty);

    // Track if this visualizer has been manually moved by the user in the editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|State")
    bool bHasBeenManuallyMoved = false;

    // Relative transform offset from its default spline position when manually moved in the editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|State")
    FTransform ManualRelativeTransform;

#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
#endif
};
