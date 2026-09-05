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
    USightPortal2DPropertyDetailWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Main update method to read all values from FSightPortalProperty structure and refresh UMG controls
    UFUNCTION(BlueprintCallable, Category = "SightPortal|2DWidget")
    void DisplayPropertyDetails(const FSightPortalProperty& InProperty);

    // Label/suffix used for room count display (defaults to "Bedrooms", e.g. "Bedrooms", "Rooms", "Offices", "Suites")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|2DWidget")
    FString RoomType;

    // Set custom room type label (e.g. "Bedrooms", "Rooms", "Offices", "Suites")
    UFUNCTION(BlueprintCallable, Category = "SightPortal|2DWidget")
    void SetRoomType(const FString& InRoomType) { RoomType = InRoomType; }

    // Get current room type label
    UFUNCTION(BlueprintPure, Category = "SightPortal|2DWidget")
    FString GetRoomType() const { return RoomType; }

    // Currency symbol (defaults to Iraqi Dinars "د.ع")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    FString CurrencySymbol;

    // Whether the currency symbol should be placed before the amount or after (e.g. 150000000.00 د.ع)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    bool bSymbolPrefix;

    // Number of decimal places (default 2 for 0000000000.00 format)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    int32 DecimalPlaces;

    // Exchange rate relative to base price (Portal prices are in Iraqi Dinars, so IQD = 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    float ExchangeRate;

    // Dynamic delegate fired when user closes or dismisses the detail modal
    UPROPERTY(BlueprintAssignable, Category = "SightPortal|2DWidget")
    FOnSightPortalDetailClosed OnDetailClosed;

    // Helper to close and remove this widget from viewport
    UFUNCTION(BlueprintCallable, Category = "SightPortal|2DWidget")
    void DismissWidget();

    // Auto-discover and resolve unbound UMG child widgets by name and alias
    UFUNCTION(BlueprintCallable, Category = "SightPortal|2DWidget")
    void ResolveUnboundWidgets();

    // Get active property data being viewed
    UFUNCTION(BlueprintPure, Category = "SightPortal|2DWidget")
    FSightPortalProperty GetActiveProperty() const { return ActiveProperty; }

    // Dynamic Blueprint Implementable Event triggered when property details are refreshed/updated from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPropertyDataUpdatedFromPortal(const FSightPortalProperty& UpdatedProperty);

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
