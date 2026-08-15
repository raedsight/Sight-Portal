#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SightPortalConnector.h"
#include "SightPortal2DPropertyDetailWidget.h"
#include "SightPortalPlayerController.generated.h"

class APropertyVisualizer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSightPortalPropertySelected, APropertyVisualizer*, SelectedVisualizer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortalPropertyDeselected);

/**
 * ASightPortalPlayerController
 * Interactive Player Controller for SightPortal archviz walkthroughs in Unreal Engine 5.
 * 
 * Capabilities:
 * - Left-click on any APropertyVisualizer actor to toggle show the 2DPropertyDetailWidget HUD.
 * - Clicking away (on the ground, other scene geometry, or empty space) will toggle hide / dismiss the 2DPropertyDetailWidget.
 * - Full mouse cursor management, touch/tap support, and dynamic Blueprint event dispatchers.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API ASightPortalPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASightPortalPlayerController();

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // --- Configuration Properties ---

    // 2D Widget Class to spawn for property details (Defaults to USightPortal2DPropertyDetailWidget)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UI")
    TSubclassOf<USightPortal2DPropertyDetailWidget> Detail2DWidgetClass;

    // Collision channel used to trace and pick Property Visualizer actors under mouse cursor (Default: ECC_Visibility)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Interaction")
    TEnumAsByte<ECollisionChannel> ClickTraceChannel;

    // Automatically enable mouse cursor and click events on BeginPlay
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Interaction")
    bool bAutoEnableMouseInteraction;

    // --- State Accessors ---

    // Active 2D Detail Widget instance currently displayed on viewport
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UI")
    USightPortal2DPropertyDetailWidget* Active2DDetailWidget;

    // Currently selected Property Visualizer actor
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|Selection")
    APropertyVisualizer* CurrentSelectedVisualizer;

    // --- Interactive Functions ---

    /**
     * Primary click handler bound to Left Mouse Button and Touch input.
     * Evaluates hit result under cursor:
     * - If clicking on a PropertyVisualizer: toggles detail popup (show if new/hidden, hide if already active).
     * - If clicking away on background/other actors/empty space: toggles hide detail popup.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Interaction")
    void HandleClickInteraction();

    /**
     * Touch input handler bound to screen tap on touch devices.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Interaction")
    void HandleTouchInteraction(ETouchIndex::Type FingerIndex, FVector Location);

    /**
     * Process click on a specific candidate actor.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Interaction")
    void HandleActorClicked(AActor* ClickedActor);

    /**
     * Toggle display of the 2D Property Detail widget for the specified visualizer.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    void TogglePropertyDetailWidget(APropertyVisualizer* TargetVisualizer);

    /**
     * Show/open the 2D Property Detail widget for the given visualizer.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    USightPortal2DPropertyDetailWidget* ShowPropertyDetailWidget(APropertyVisualizer* TargetVisualizer);

    /**
     * Hide/dismiss the 2D Property Detail widget and clear selection.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    void HidePropertyDetailWidget();

    /**
     * Checks if the 2D Detail widget is currently visible on screen.
     */
    UFUNCTION(BlueprintPure, Category = "SightPortal|UI")
    bool IsPropertyDetailWidgetOpen() const;

    // --- Dynamic Event Delegates ---

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalPropertySelected OnPropertySelected;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalPropertyDeselected OnPropertyDeselected;

protected:
    // Internal callback when detail widget is closed via its internal close button
    UFUNCTION()
    void OnDetailWidgetClosed();

    // Helper to find APropertyVisualizer from any hit actor or its hierarchy
    APropertyVisualizer* ResolvePropertyVisualizerFromActor(AActor* InActor) const;
};
