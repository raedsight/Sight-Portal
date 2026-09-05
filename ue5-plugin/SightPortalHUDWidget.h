#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SightPortalHUDWidget.generated.h"

class UTextBlock;
class USlider;
class UButton;
class UImage;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSightPortalTimeOfDayChanged, float, NewTimeHours, const FString&, FormattedTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSightPortalHomeTriggered, FVector, TargetLocation, FRotator, TargetRotation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortalGalleryClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortalServicesClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortalUnitSearchClicked);

/**
 * USightPortalHUDWidget
 * Comprehensive ArchViz Main HUD & Navigation Widget for Unreal Engine 5.
 * 
 * Features:
 * 1. Top-Left Compass: Real-time heading display ("N", "NE", "E", "SE", "S", "SW", "W", "NW") and optional rotating compass needle/disc image.
 * 2. Top-Middle Time of Day Slider: Horizontal slider (0:00 - 24:00) with formatted time readout ("9:00") and sun/directional light pitch control.
 * 3. Bottom Navigation Bar:
 *    - Home Button: Smoothly transports the player pawn/camera to a user-configurable 3D location & rotation.
 *    - Gallery Button: Opens/dispatches gallery view.
 *    - Services Button: Opens/dispatches project surroundings and services view.
 *    - Unit Search Button: Opens/dispatches property filter & unit search view.
 * 
 * Includes auto-discovery and alias resolution for UMG designers.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortalHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USightPortalHUDWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // --- Compass Configuration & UMG Bindings ---

    // Text block displaying the current cardinal direction (e.g., "NE", "N", "SSW")
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Compass")
    UTextBlock* CompassText;

    // Image for compass needle or rotating bezel/disc
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Compass")
    UImage* CompassNeedleImage;

    // Compass root widget or container
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Compass")
    UWidget* CompassRoot;

    // True north rotation offset in level degrees (Default: 0.0f)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Compass")
    float TrueNorthOffsetDegrees;

    // --- Time of Day Configuration & UMG Bindings ---

    // Slider to control time of day (0.0 to 24.0 hours)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|TimeOfDay")
    USlider* TimeOfDaySlider;

    // Text block displaying the formatted time (e.g. "9:00", "14:30")
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|TimeOfDay")
    UTextBlock* TimeOfDayText;

    // Current time of day in decimal hours (0.0 - 24.0, e.g. 9.0 = 9:00 AM, 14.5 = 2:30 PM)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|TimeOfDay", meta = (ClampMin = "0.0", ClampMax = "24.0"))
    float CurrentTimeHours;

    // If true, format time as 24-hour clock (e.g., "14:00"), otherwise 12-hour AM/PM ("2:00 PM")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|TimeOfDay")
    bool bUse24HourFormat;

    // Tag to automatically find the directional light actor in the level for sun control
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|TimeOfDay")
    FName SunLightActorTag;

    // Optional direct reference to the sun directional light actor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|TimeOfDay")
    AActor* SunLightActor;

    // --- Bottom Navigation Bar: Buttons & Targets ---

    // Home Button: Return to default 3D viewpoint
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Navigation")
    UButton* HomeButton;

    // Gallery Button: Open gallery modal/widget
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Navigation")
    UButton* GalleryButton;

    // Services Button: Open services modal/widget
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Navigation")
    UButton* ServicesButton;

    // Unit Search Button: Open unit search & filter modal/widget
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Navigation")
    UButton* UnitSearchButton;

    // User-specified 3D Home Location for the player pawn/camera
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation", meta = (MakeEditWidget = true))
    FVector HomeLocation;

    // User-specified 3D Home Rotation for the player pawn/camera
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation")
    FRotator HomeRotation;

    // If true, automatically captures the pawn's initial position on start if HomeLocation is left at Zero
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation")
    bool bAutoCaptureStartLocationAsHome;

    // Indicates whether the Home coordinates have been initialized (via auto-capture or SetHomeTransform)
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|Navigation")
    bool bIsHomeLocationInitialized;

    // Smooth camera transition duration when hitting the Home button (0 = instantaneous)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation")
    float HomeTransitionSpeed;

    // Optional gallery widget class to spawn dynamically (defaults to USightPortalGalleryWidget)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation")
    TSubclassOf<class USightPortalGalleryWidget> GalleryWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation")
    TSubclassOf<UUserWidget> ServicesWidgetClass;

    // If true, clicking the Services button triggers God Mode (flying high and looking down) on the SightPortal Player Controller
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation")
    bool bTriggerGodModeOnServicesClick;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Navigation")
    TSubclassOf<UUserWidget> UnitSearchWidgetClass;

    // Active Unit Search Widget instance in viewport
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|Navigation")
    UUserWidget* ActiveUnitSearchWidget;

    // --- Dynamic Event Delegates ---

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalTimeOfDayChanged OnTimeOfDayChanged;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalHomeTriggered OnHomeNavigationTriggered;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalGalleryClicked OnGalleryButtonClicked;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalServicesClicked OnServicesButtonClicked;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalUnitSearchClicked OnUnitSearchButtonClicked;

    // --- Public Operations ---

    // Programmatically set time of day (0.0 to 24.0)
    UFUNCTION(BlueprintCallable, Category = "SightPortal|TimeOfDay")
    void SetTimeOfDay(float InHours);

    // Set custom 3D home coordinates
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void SetHomeTransform(FVector InLocation, FRotator InRotation);

    // Trigger navigation to the default 3D home position
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void GoToHomeLocation();

    // Trigger gallery action
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void TriggerGalleryAction();

    // Trigger services action
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void TriggerServicesAction();

    // Trigger Services exploration in God Mode (fly high and look down at POIs)
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void ExploreServicesInGodMode();

    // Trigger unit search action
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void TriggerUnitSearchAction();

    // Toggle unit search modal/drawer open or closed
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void ToggleUnitSearch();

    // Open unit search modal/drawer
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void OpenUnitSearch();

    // Close unit search modal/drawer if open
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Navigation")
    void CloseUnitSearch();

    // Check if unit search modal/drawer is open
    UFUNCTION(BlueprintPure, Category = "SightPortal|Navigation")
    bool IsUnitSearchOpen() const;

    // Check if Home coordinates have been initialized
    UFUNCTION(BlueprintPure, Category = "SightPortal|Navigation")
    bool IsHomeLocationInitialized() const { return bIsHomeLocationInitialized; }

    // Auto-discover and resolve unbound UMG child widgets by name and alias
    UFUNCTION(BlueprintCallable, Category = "SightPortal|HUD")
    void ResolveUnboundWidgets();

protected:
    // Internal button & slider callbacks
    UFUNCTION()
    void OnTimeSliderValueChanged(float NewValue);

    UFUNCTION()
    void OnHomeClicked();

    UFUNCTION()
    void OnGalleryClicked();

    UFUNCTION()
    void OnServicesClicked();

    UFUNCTION()
    void OnUnitSearchClicked();

    // Update compass heading and visual needle rotation
    void UpdateCompassHeading();

    // Update sun lighting rotation in level
    void UpdateSunLighting(float InHours);

    // Convert decimal hours into human readable text ("9:00", "09:00", etc.)
    FString FormatTimeString(float InHours) const;

    // Convert angle in degrees (-180 to 180) to 8-point or 16-point cardinal text
    static FString YawToCardinalDirection(float YawDegrees);

private:
    bool bIsSettingTimeSlider;
    bool bIsTransitioningToHome;
    FVector HomeTransitionStartLocation;
    FRotator HomeTransitionStartRotation;
    float HomeTransitionAlpha;
};
