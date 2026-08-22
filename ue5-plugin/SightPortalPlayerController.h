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
 * - Full WASD / Arrow / Space / Shift navigation and smooth flying spectator camera movement.
 * - Mouse look (Hold Right Mouse Button to look around, Left-click to pick/interact with properties).
 * - Multi-touch gestures (1-finger tap to select, 1-finger drag to rotate view, 2-finger pan/pinch to move and zoom).
 * - Left-click on any APropertyVisualizer actor to toggle show the 2DPropertyDetailWidget HUD.
 * - Clicking away (on the ground, other scene geometry, or empty space) will toggle hide / dismiss the 2DPropertyDetailWidget.
 * - Dynamic Blueprint event dispatchers for custom camera animations and highlight effects.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API ASightPortalPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASightPortalPlayerController();

    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void SetupInputComponent() override;

    // --- Movement & Camera Configuration ---

    // Base flying / walking movement speed in unreal units per second
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Movement")
    float MoveSpeed;

    // Speed multiplier applied when holding Left Shift (Sprint)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Movement")
    float SprintSpeedMultiplier;

    // Mouse look sensitivity rate
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Camera")
    float MouseLookSensitivity;

    // Touch swipe rotation sensitivity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Touch")
    float TouchLookSensitivity;

    // Touch two-finger pinch zoom & pan sensitivity
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Touch")
    float TouchMoveSensitivity;

    // Max distance in pixels to consider a touch as a tap rather than a drag
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Touch")
    float TouchTapMaxDistance;

    // Invert vertical look pitch
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Camera")
    bool bInvertLookPitch;

    // Interpolation speed for fast camera travel to LookAt point (Default: 10.0f)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Camera")
    float CameraTransitionSpeed;

    // Whether player movement (WASD, mouse look, touch pan) is currently locked
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|State")
    bool bIsMovementLocked;

    // Whether camera is currently animating towards a property's LookAt point
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|State")
    bool bIsTransitioningCamera;

    // --- UI & Interaction Properties ---

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

    // --- Keyboard & Mouse Movement Input Functions ---

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Movement")
    void SetMovementInput(FVector Direction);

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Movement")
    void StartSprint();

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Movement")
    void StopSprint();

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Camera")
    void OnMouseMoveX(float Value);

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Camera")
    void OnMouseMoveY(float Value);

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Camera")
    void OnMouseWheelZoom(float Value);

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Camera")
    void StartMouseLook();

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Camera")
    void StopMouseLook();

    // --- Touch Input Functions ---

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Touch")
    void HandleTouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Touch")
    void HandleTouchMoved(ETouchIndex::Type FingerIndex, FVector Location);

    UFUNCTION(BlueprintCallable, Category = "SightPortal|Touch")
    void HandleTouchEnded(ETouchIndex::Type FingerIndex, FVector Location);

    // --- Interactive Selection & Focus Functions ---

    /**
     * Fast travel camera to property's LookAt Arrow location and rotation, locking movement.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Camera")
    void FocusOnPropertyVisualizer(APropertyVisualizer* TargetVisualizer);

    /**
     * Lock player WASD and mouse/touch navigation movement.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Movement")
    void LockMovement();

    /**
     * Unlock player WASD and mouse/touch navigation movement.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Movement")
    void UnlockMovement();

    /**
     * Check if player navigation movement is currently locked.
     */
    UFUNCTION(BlueprintPure, Category = "SightPortal|Movement")
    bool IsMovementLocked() const { return bIsMovementLocked; }

    /**
     * Check if camera is currently transitioning to a LookAt target.
     */
    UFUNCTION(BlueprintPure, Category = "SightPortal|Camera")
    bool IsTransitioningCamera() const { return bIsTransitioningCamera; }

    /**
     * Set the selected property visualizer (focuses/highlights without opening 2D detail popup).
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Selection")
    void SelectPropertyVisualizer(APropertyVisualizer* TargetVisualizer);

    /**
     * Clear and deselect the currently active property visualizer.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Selection")
    void DeselectPropertyVisualizer();

    /**
     * Primary click handler bound to Left Mouse Button.
     * Evaluates hit result under cursor:
     * - If clicking on a PropertyVisualizer: selects/highlights the property visualizer.
     * - If clicking away on background/empty space: dismisses open 2D detail popup and clears selection.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Interaction")
    void HandleClickInteraction();

    /**
     * Process click/tap on a specific candidate actor.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Interaction")
    void HandleActorClicked(AActor* ClickedActor);

    /**
     * Toggle display of the 2D Property Detail widget for the specified visualizer.
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    void TogglePropertyDetailWidget(APropertyVisualizer* TargetVisualizer);

    /**
     * Show/open the 2D Property Detail widget for the given visualizer (called when Explore is clicked).
     */
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UI")
    USightPortal2DPropertyDetailWidget* ShowPropertyDetailWidget(APropertyVisualizer* TargetVisualizer);

    /**
     * Hide/dismiss the 2D Property Detail widget.
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

    // Apply movement input to possessed Pawn or Spectator Pawn
    void ApplyDirectMovement(float DeltaTime);

private:
    // Movement state
    FVector CurrentMovementInput;
    bool bIsSprinting;
    bool bIsMouseLooking;

    // Camera Focus Target state
    FVector TargetFocusLocation;
    FRotator TargetFocusRotation;

    // Touch tracking state
    struct FTouchData
    {
        bool bIsActive = false;
        FVector2D StartLocation = FVector2D::ZeroVector;
        FVector2D CurrentLocation = FVector2D::ZeroVector;
        FVector2D PreviousLocation = FVector2D::ZeroVector;
        double StartTime = 0.0;
        bool bMovedBeyondTap = false;
    };

    TMap<int32, FTouchData> ActiveTouches;
};

