#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SightPortalConnector.h"
#include "SightPortal3DPropertyWidget.generated.h"

class UTextBlock;
class UButton;

// Delegate broadcasted when user clicks the "Explore" button on the 3D widget
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSightPortalExploreRequested, const FSightPortalProperty&, PropertyDetails);

// Delegate broadcasted when user clicks the "Close" button on the 3D widget
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortal3DWidgetClosed);

/**
 * USightPortal3DPropertyWidget
 * Small 3D World Space Widget attached to APropertyVisualizer actors in Unreal Engine 5.
 * Displays key summary parameters: property surface area, bedroom numbers, and interactive "Explore" and "Close" buttons.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortal3DPropertyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USightPortal3DPropertyWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Set and update property data for this 3D widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|3DWidget")
    void SetPropertyData(const FSightPortalProperty& InProperty);

    // Label/suffix used for bedroom or room count display (defaults to "Beds", e.g. "Beds", "Bedrooms", "Rooms", "Offices")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|3DWidget")
    FString RoomType;

    // Set custom room type label (e.g. "Beds", "Bedrooms", "Rooms", "Offices")
    UFUNCTION(BlueprintCallable, Category = "SightPortal|3DWidget")
    void SetRoomType(const FString& InRoomType) { RoomType = InRoomType; }

    // Get current room type label
    UFUNCTION(BlueprintPure, Category = "SightPortal|3DWidget")
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

    // Event delegate fired when "Explore" button is pressed
    UPROPERTY(BlueprintAssignable, Category = "SightPortal|3DWidget")
    FOnSightPortalExploreRequested OnExploreRequested;

    // Event delegate fired when "Close" button is pressed
    UPROPERTY(BlueprintAssignable, Category = "SightPortal|3DWidget")
    FOnSightPortal3DWidgetClosed OnCloseRequested;

    // Show this 3D widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|3DWidget")
    void ShowWidget();

    // Hide this 3D widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|3DWidget")
    void HideWidget();

    // Toggle or set visibility
    UFUNCTION(BlueprintCallable, Category = "SightPortal|3DWidget")
    void SetWidgetVisibility(bool bVisible);

    // Auto-discover and resolve unbound UMG child widgets by name and alias
    UFUNCTION(BlueprintCallable, Category = "SightPortal|3DWidget")
    void ResolveUnboundWidgets();

    // Get current cached property data
    UFUNCTION(BlueprintPure, Category = "SightPortal|3DWidget")
    FSightPortalProperty GetPropertyData() const { return CachedProperty; }

    // Dynamic Blueprint Implementable Event triggered when property data is updated from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPropertyDataUpdatedFromPortal(const FSightPortalProperty& UpdatedProperty);

protected:
    // Property record cached in widget
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|3DWidget")
    FSightPortalProperty CachedProperty;

    // --- UMG UI Widget Bindings (Optional so users can design custom UMG blueprints) ---

    // Property Surface text display (e.g. "185 m²")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* SurfaceText;

    // Bedroom count text display (e.g. "3 Beds")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* BedroomsText;

    // Property Name / Identifier text display
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* PropertyNameText;

    // Price text display (e.g. "$1,250,000")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* PriceText;

    // Availability status text display (e.g. "Available", "Under Offer", "Sold")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* AvailabilityText;

    // Door/Room No text display (e.g. "#101")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* DoorNoText;

    // Block text display (e.g. "Block A")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* BlockText;

    // Zone text display (e.g. "Zone 1")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* ZoneText;

    // Class text display (e.g. "Residential", "Villa")
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UTextBlock* ClassText;

    // Explore Button (opens 2D details and hides 3D widget)
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UButton* ExploreButton;

    // Close Button (hides 3D widget)
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UButton* CloseButton;

    // Internal click handler for ExploreButton
    UFUNCTION()
    void OnExploreClicked();

    // Internal click handler for CloseButton
    UFUNCTION()
    void OnCloseClicked();
};
