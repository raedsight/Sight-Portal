#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SightPortalConnector.h"
#include "SightPortal2DPropertyDetailWidget.generated.h"

class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortalDetailClosed);

/**
 * USightPortal2DPropertyDetailWidget
 * Full 2D HUD / Screen Widget displaying all fields read directly from the FSightPortalProperty structure in UE5.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortal2DPropertyDetailWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Main update method to read all values from FSightPortalProperty structure and refresh UMG controls
    UFUNCTION(BlueprintCallable, Category = "SightPortal|2DWidget")
    void DisplayPropertyDetails(const FSightPortalProperty& InProperty);

    // Dynamic delegate fired when user closes or dismisses the detail modal
    UPROPERTY(BlueprintAssignable, Category = "SightPortal|2DWidget")
    FOnSightPortalDetailClosed OnDetailClosed;

    // Helper to close and remove this widget from viewport
    UFUNCTION(BlueprintCallable, Category = "SightPortal|2DWidget")
    void DismissWidget();

    // Get active property data being viewed
    UFUNCTION(BlueprintPure, Category = "SightPortal|2DWidget")
    FSightPortalProperty GetActiveProperty() const { return ActiveProperty; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|2DWidget")
    FSightPortalProperty ActiveProperty;

    // --- UMG UI Widget Bindings for all FSightPortalProperty attributes ---

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* NameText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* ZoneText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* BlockText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* DoorNoText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* PriceText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* SurfaceText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* BuildingSurfaceText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* AvailabilityText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* BedroomsCountText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* BathroomsCountText;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UTextBlock* ClassText;

    // Close button
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|2DWidget", meta = (BindWidgetOptional))
    UButton* CloseButton;

    UFUNCTION()
    void OnCloseButtonClicked();
};
