#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SightPortalConnector.h"
#include "SightPortal3DPropertyWidget.generated.h"

class UTextBlock;
class UButton;

// Delegate broadcasted when user clicks the "Explore" button on the 3D widget
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSightPortalExploreRequested, const FSightPortalProperty&, PropertyDetails);

/**
 * USightPortal3DPropertyWidget
 * Small 3D World Space Widget attached to APropertyVisualizer actors in Unreal Engine 5.
 * Displays key summary parameters: property surface area, bedroom numbers, and an interactive "Explore" button.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortal3DPropertyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Set and update property data for this 3D widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|3DWidget")
    void SetPropertyData(const FSightPortalProperty& InProperty);

    // Event delegate fired when "Explore" button is pressed
    UPROPERTY(BlueprintAssignable, Category = "SightPortal|3DWidget")
    FOnSightPortalExploreRequested OnExploreRequested;

    // Get current cached property data
    UFUNCTION(BlueprintPure, Category = "SightPortal|3DWidget")
    FSightPortalProperty GetPropertyData() const { return CachedProperty; }

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

    // Explore Button
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|3DWidget", meta = (BindWidgetOptional))
    UButton* ExploreButton;

    // Internal click handler for ExploreButton
    UFUNCTION()
    void OnExploreClicked();
};
