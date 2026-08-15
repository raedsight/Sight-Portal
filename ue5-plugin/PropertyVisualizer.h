#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalConnector.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "SightPortal3DPropertyWidget.h"
#include "SightPortal2DPropertyDetailWidget.h"
#include "PropertyVisualizer.generated.h"

/**
 * APropertyVisualizer
 * A dedicated property visualizer actor with a single variable of type FSightPortalProperty.
 * Integrates LookAt Arrow Component for camera framing, 3D World Space Widget (surface, bedrooms, explore/close button), 
 * 2D Detail HUD Widget, and raycast selection collision.
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

    // Collision box for mouse cursor picking and selection interaction
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|Collision")
    UBoxComponent* SelectionCollisionBox;

    // LookAt Arrow Component indicating target camera location and orientation when this property is selected
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|Camera")
    UArrowComponent* LookAtArrowComponent;

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

    // Show/display the 3D world space widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    void Show3DWidget();

    // Hide/collapse the 3D world space widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    void Hide3DWidget();

    // Set 3D widget visibility
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    void Set3DWidgetVisible(bool bVisible);

    // Get the world location of the LookAt Arrow component
    UFUNCTION(BlueprintPure, Category = "SightPortal|Camera")
    FVector GetLookAtLocation() const;

    // Get the world rotation (direction) of the LookAt Arrow component
    UFUNCTION(BlueprintPure, Category = "SightPortal|Camera")
    FRotator GetLookAtRotation() const;

    // Get the LookAt Arrow component
    UFUNCTION(BlueprintPure, Category = "SightPortal|Camera")
    UArrowComponent* GetLookAtArrowComponent() const { return LookAtArrowComponent; }

    // Open/Show the 2D Detail Widget on player viewport displaying full property details
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    USightPortal2DPropertyDetailWidget* OpenPropertyDetail2DWidget();

    // Callback bound to 3D widget's Explore button
    UFUNCTION()
    void OnExploreRequestedFrom3DWidget(const FSightPortalProperty& InProperty);

    // Callback bound to 3D widget's Close button
    UFUNCTION()
    void OnCloseRequestedFrom3DWidget();

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
