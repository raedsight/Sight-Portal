#include "SightPortalPlayerController.h"
#include "SightPortalHUDWidget.h"
#include "PropertyVisualizer.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/PlayerCameraManager.h"

ASightPortalPlayerController::ASightPortalPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;

    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bEnableTouchEvents = true;
    bEnableTouchOverEvents = true;

    ClickTraceChannel = ECC_Visibility;
    bAutoEnableMouseInteraction = true;

    // Movement defaults
    MoveSpeed = 1500.0f;
    SprintSpeedMultiplier = 2.5f;
    MouseLookSensitivity = 1.0f;
    TouchLookSensitivity = 0.35f;
    TouchMoveSensitivity = 8.0f;
    TouchTapMaxDistance = 15.0f;
    bInvertLookPitch = false;
    CameraTransitionSpeed = 10.0f;

    CurrentMovementInput = FVector::ZeroVector;
    bIsSprinting = false;
    bIsMouseLooking = false;
    bIsMovementLocked = false;
    bIsTransitioningCamera = false;
    TargetFocusLocation = FVector::ZeroVector;
    TargetFocusRotation = FRotator::ZeroRotator;

    // God Mode defaults (Services POI Exploration - fly high and look down)
    GodModeLocation = FVector(0.0f, 0.0f, 15000.0f);
    GodModeRotation = FRotator(-65.0f, 0.0f, 0.0f);
    GodModeTransitionSpeed = 5.0f;
    bIsInGodMode = false;
    bIsTransitioningToGodMode = false;
    GodModeTransitionTimeElapsed = 0.0f;
    bAutoRevealServicePOIsInGodMode = true;
    ServicePOITag = FName(TEXT("ServicePOI"));
    PreGodModeLocation = FVector::ZeroVector;
    PreGodModeRotation = FRotator::ZeroRotator;

    Detail2DWidgetClass = USightPortal2DPropertyDetailWidget::StaticClass();
    Active2DDetailWidget = nullptr;
    MainHUDWidgetClass = nullptr;
    ActiveMainHUDWidget = nullptr;
    bAutoSpawnMainHUD = true;
    CurrentSelectedVisualizer = nullptr;
}

void ASightPortalPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoSpawnMainHUD && MainHUDWidgetClass)
    {
        ShowMainHUD();
    }

    if (bAutoEnableMouseInteraction)
    {
        bShowMouseCursor = true;
        bEnableClickEvents = true;
        bEnableMouseOverEvents = true;
        bEnableTouchEvents = true;
        bEnableTouchOverEvents = true;

        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }

    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.AddUniqueDynamic(this, &ASightPortalPlayerController::HandleDataReceived);
        Connector->OnPropertyUpdated.AddUniqueDynamic(this, &ASightPortalPlayerController::HandleSinglePropertyUpdated);
    }
}

void ASightPortalPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    USightPortalConnector* Connector = GEngine ? GEngine->GetEngineSubsystem<USightPortalConnector>() : nullptr;
    if (Connector)
    {
        Connector->OnRealEstateDataReceived.RemoveDynamic(this, &ASightPortalPlayerController::HandleDataReceived);
        Connector->OnPropertyUpdated.RemoveDynamic(this, &ASightPortalPlayerController::HandleSinglePropertyUpdated);
    }

    Super::EndPlay(EndPlayReason);
}

void ASightPortalPlayerController::HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio)
{
    OnPortalDataReceived(PropertyPortfolio);
}

void ASightPortalPlayerController::HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData)
{
    // If the currently open 2D detail popup is for this property, refresh its content immediately
    if (Active2DDetailWidget && Active2DDetailWidget->IsInViewport())
    {
        if (Active2DDetailWidget->GetActiveProperty().Name.Equals(PropertyName, ESearchCase::IgnoreCase) ||
            Active2DDetailWidget->GetActiveProperty().Name.Equals(PropertyData.Name, ESearchCase::IgnoreCase))
        {
            Active2DDetailWidget->DisplayPropertyDetails(PropertyData);
        }
    }

    OnPortalPropertyUpdated(PropertyName, PropertyData);
}

void ASightPortalPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    // --- Fast Camera Travel to LookAt Point ---
    if (bIsTransitioningCamera)
    {
        AActor* TargetActor = GetPawn();
        if (!TargetActor)
        {
            TargetActor = GetSpectatorPawn();
        }
        if (!TargetActor)
        {
            TargetActor = GetViewTarget();
        }

        if (TargetActor)
        {
            const FVector CurrentLoc = TargetActor->GetActorLocation();
            const FRotator CurrentRot = GetControlRotation();

            const FVector NewLoc = FMath::VInterpTo(CurrentLoc, TargetFocusLocation, DeltaTime, CameraTransitionSpeed);
            const FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetFocusRotation, DeltaTime, CameraTransitionSpeed);

            TargetActor->SetActorLocation(NewLoc, true);
            SetControlRotation(NewRot);

            const float DistSq = FVector::DistSquared(NewLoc, TargetFocusLocation);
            const float RotDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Pitch, TargetFocusRotation.Pitch)) +
                                  FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, TargetFocusRotation.Yaw));

            // Snap when sufficiently close
            if (DistSq < 64.0f && RotDiff < 0.5f)
            {
                TargetActor->SetActorLocation(TargetFocusLocation, true);
                SetControlRotation(TargetFocusRotation);
                bIsTransitioningCamera = false;
            }
        }
        else
        {
            bIsTransitioningCamera = false;
        }
    }

    // --- God Mode Camera Travel (Fly High and Look Down) ---
    if (bIsTransitioningToGodMode)
    {
        AActor* TargetActor = GetPawn();
        if (!TargetActor)
        {
            TargetActor = GetSpectatorPawn();
        }
        if (!TargetActor)
        {
            TargetActor = GetViewTarget();
        }

        if (TargetActor)
        {
            GodModeTransitionTimeElapsed += DeltaTime;

            const FVector TargetLoc = bIsInGodMode ? GodModeLocation : PreGodModeLocation;
            const FRotator TargetRot = bIsInGodMode ? GodModeRotation : PreGodModeRotation;

            const FVector CurrentLoc = TargetActor->GetActorLocation();
            const FRotator CurrentRot = GetControlRotation();

            const FVector NewLoc = FMath::VInterpTo(CurrentLoc, TargetLoc, DeltaTime, GodModeTransitionSpeed);
            const FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, GodModeTransitionSpeed);

            TargetActor->SetActorLocation(NewLoc, false);
            SetControlRotation(NewRot);

            const float DistSq = FVector::DistSquared(NewLoc, TargetLoc);
            const float RotDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Pitch, TargetRot.Pitch)) +
                                  FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, TargetRot.Yaw));

            // Snap when sufficiently close OR if transition watchdog timer expires
            if ((DistSq < 10000.0f && RotDiff < 2.0f) || GodModeTransitionTimeElapsed > 4.0f)
            {
                TargetActor->SetActorLocation(TargetLoc, false);
                SetControlRotation(TargetRot);
                bIsTransitioningToGodMode = false;
                GodModeTransitionTimeElapsed = 0.0f;

                if (bIsInGodMode && bAutoRevealServicePOIsInGodMode)
                {
                    RevealServicePOIs();
                }
            }
        }
        else
        {
            bIsTransitioningToGodMode = false;
            GodModeTransitionTimeElapsed = 0.0f;
        }
    }

    // If movement is locked (static at LookAt point) or animating into God Mode, skip WASD navigation polling
    if (bIsMovementLocked || bIsTransitioningToGodMode)
    {
        CurrentMovementInput = FVector::ZeroVector;
        return;
    }

    // --- Direct Keyboard Polling for WASD, Arrows, Elevation & Sprint ---
    FVector KeyMovement = FVector::ZeroVector;

    // Forward / Backward (X axis)
    if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up))
    {
        KeyMovement.X += 1.0f;
    }
    if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down))
    {
        KeyMovement.X -= 1.0f;
    }

    // Right / Left Strafe (Y axis)
    if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right))
    {
        KeyMovement.Y += 1.0f;
    }
    if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left))
    {
        KeyMovement.Y -= 1.0f;
    }

    // Up / Down Elevation (Z axis)
    if (IsInputKeyDown(EKeys::E) || IsInputKeyDown(EKeys::SpaceBar))
    {
        KeyMovement.Z += 1.0f;
    }
    if (IsInputKeyDown(EKeys::Q) || IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::C))
    {
        KeyMovement.Z -= 1.0f;
    }

    // Sprint modifier
    bIsSprinting = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);

    // Combine keyboard input with any external movement input
    CurrentMovementInput = KeyMovement;

    ApplyDirectMovement(DeltaTime);
}

void ASightPortalPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!InputComponent)
    {
        return;
    }

    // --- Mouse Click Selection ---
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);

    // --- Mouse Look (Hold Right Mouse Button) ---
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ASightPortalPlayerController::StartMouseLook);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ASightPortalPlayerController::StopMouseLook);

    // --- Mouse Look Axes ---
    InputComponent->BindAxisKey(EKeys::MouseX, this, &ASightPortalPlayerController::OnMouseMoveX);
    InputComponent->BindAxisKey(EKeys::MouseY, this, &ASightPortalPlayerController::OnMouseMoveY);
    InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &ASightPortalPlayerController::OnMouseWheelZoom);

    // --- Touch Gestures ---
    InputComponent->BindTouch(IE_Pressed, this, &ASightPortalPlayerController::HandleTouchStarted);
    InputComponent->BindTouch(IE_Repeat, this, &ASightPortalPlayerController::HandleTouchMoved);
    InputComponent->BindTouch(IE_Released, this, &ASightPortalPlayerController::HandleTouchEnded);

    // --- Optional Named Action / Axis Bindings ---
    InputComponent->BindAction("SightPortalClick", IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);
    InputComponent->BindAction("Interact", IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);
}

// --- Movement Implementation ---

void ASightPortalPlayerController::SetMovementInput(FVector Direction)
{
    CurrentMovementInput = Direction;
}

void ASightPortalPlayerController::StartSprint()
{
    bIsSprinting = true;
}

void ASightPortalPlayerController::StopSprint()
{
    bIsSprinting = false;
}

void ASightPortalPlayerController::StartMouseLook()
{
    if (bIsMovementLocked)
    {
        return;
    }

    bIsMouseLooking = true;
    bShowMouseCursor = false;

    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}

void ASightPortalPlayerController::StopMouseLook()
{
    bIsMouseLooking = false;
    bShowMouseCursor = true;

    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
}

void ASightPortalPlayerController::OnMouseMoveX(float Value)
{
    if (!bIsMovementLocked && bIsMouseLooking && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        AddYawInput(Value * MouseLookSensitivity);
    }
}

void ASightPortalPlayerController::OnMouseMoveY(float Value)
{
    if (!bIsMovementLocked && bIsMouseLooking && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        const float PitchFactor = bInvertLookPitch ? 1.0f : -1.0f;
        AddPitchInput(Value * MouseLookSensitivity * PitchFactor);
    }
}

void ASightPortalPlayerController::OnMouseWheelZoom(float Value)
{
    if (!bIsMovementLocked && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        // Adjust movement speed or move forward along camera direction
        if (APawn* TargetPawn = GetPawn())
        {
            const FRotator ControlRot = GetControlRotation();
            const FVector ForwardDir = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
            TargetPawn->AddActorWorldOffset(ForwardDir * Value * 100.0f, true);
        }
    }
}

void ASightPortalPlayerController::ApplyDirectMovement(float DeltaTime)
{
    if (CurrentMovementInput.IsNearlyZero())
    {
        return;
    }

    AActor* TargetActor = GetPawn();
    if (!TargetActor)
    {
        TargetActor = GetSpectatorPawn();
    }
    if (!TargetActor)
    {
        TargetActor = GetViewTarget();
    }

    if (!TargetActor)
    {
        return;
    }

    const FRotator ControlRot = GetControlRotation();
    const FRotationMatrix RotMatrix(ControlRot);

    const FVector ForwardVector = RotMatrix.GetUnitAxis(EAxis::X);
    const FVector RightVector = RotMatrix.GetUnitAxis(EAxis::Y);
    const FVector UpVector = FVector::UpVector;

    FVector MoveDirection = (ForwardVector * CurrentMovementInput.X) +
                           (RightVector * CurrentMovementInput.Y) +
                           (UpVector * CurrentMovementInput.Z);

    if (!MoveDirection.IsNearlyZero())
    {
        MoveDirection.Normalize();

        const float CurrentSpeed = MoveSpeed * (bIsSprinting ? SprintSpeedMultiplier : 1.0f);
        const FVector MoveDelta = MoveDirection * CurrentSpeed * DeltaTime;

        TargetActor->AddActorWorldOffset(MoveDelta, true);
    }
}

// --- Touch Handling ---

void ASightPortalPlayerController::HandleTouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
    const int32 FingerID = static_cast<int32>(FingerIndex);
    FTouchData& Data = ActiveTouches.FindOrAdd(FingerID);

    Data.bIsActive = true;
    Data.StartLocation = FVector2D(Location.X, Location.Y);
    Data.CurrentLocation = Data.StartLocation;
    Data.PreviousLocation = Data.StartLocation;
    Data.StartTime = FPlatformTime::Seconds();
    Data.bMovedBeyondTap = false;
}

void ASightPortalPlayerController::HandleTouchMoved(ETouchIndex::Type FingerIndex, FVector Location)
{
    const int32 FingerID = static_cast<int32>(FingerIndex);
    FTouchData* Data = ActiveTouches.Find(FingerID);
    if (!Data || !Data->bIsActive)
    {
        return;
    }

    Data->PreviousLocation = Data->CurrentLocation;
    Data->CurrentLocation = FVector2D(Location.X, Location.Y);

    const FVector2D TotalDeltaFromStart = Data->CurrentLocation - Data->StartLocation;
    if (TotalDeltaFromStart.Size() > TouchTapMaxDistance)
    {
        Data->bMovedBeyondTap = true;
    }

    // If movement is locked (focusing on property), do not rotate or pan camera
    if (bIsMovementLocked)
    {
        return;
    }

    const int32 ActiveFingerCount = ActiveTouches.Num();

    if (ActiveFingerCount == 1)
    {
        // 1-Finger drag -> Rotate / Look around
        if (Data->bMovedBeyondTap)
        {
            const FVector2D FrameDelta = Data->CurrentLocation - Data->PreviousLocation;
            AddYawInput(FrameDelta.X * TouchLookSensitivity);
            const float PitchFactor = bInvertLookPitch ? -1.0f : 1.0f;
            AddPitchInput(-FrameDelta.Y * TouchLookSensitivity * PitchFactor);
        }
    }
    else if (ActiveFingerCount >= 2)
    {
        // 2-Finger gesture -> Pan and Zoom
        FTouchData* Finger0 = ActiveTouches.Find(static_cast<int32>(ETouchIndex::Touch1));
        FTouchData* Finger1 = ActiveTouches.Find(static_cast<int32>(ETouchIndex::Touch2));

        if (Finger0 && Finger1 && Finger0->bIsActive && Finger1->bIsActive)
        {
            const float CurrentDistance = FVector2D::Distance(Finger0->CurrentLocation, Finger1->CurrentLocation);
            const float PreviousDistance = FVector2D::Distance(Finger0->PreviousLocation, Finger1->PreviousLocation);
            const float PinchDelta = CurrentDistance - PreviousDistance;

            APawn* TargetPawn = GetPawn();
            if (TargetPawn)
            {
                // Pinch -> Move Forward/Backward
                const FRotator ControlRot = GetControlRotation();
                const FVector ForwardVector = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
                const FVector RightVector = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);

                if (FMath::Abs(PinchDelta) > 1.0f)
                {
                    TargetPawn->AddActorWorldOffset(ForwardVector * PinchDelta * TouchMoveSensitivity, true);
                }

                // Two-finger Pan -> Move Left/Right/Up
                const FVector2D AvgDelta = ((Finger0->CurrentLocation - Finger0->PreviousLocation) +
                                            (Finger1->CurrentLocation - Finger1->PreviousLocation)) * 0.5f;

                const FVector PanOffset = (-RightVector * AvgDelta.X + FVector::UpVector * AvgDelta.Y) * (TouchMoveSensitivity * 0.5f);
                TargetPawn->AddActorWorldOffset(PanOffset, true);
            }
        }
    }
}

void ASightPortalPlayerController::HandleTouchEnded(ETouchIndex::Type FingerIndex, FVector Location)
{
    const int32 FingerID = static_cast<int32>(FingerIndex);
    FTouchData TouchInfo;
    if (ActiveTouches.RemoveAndCopyValue(FingerID, TouchInfo))
    {
        // If the touch did not drag beyond threshold, treat as tap selection
        if (!TouchInfo.bMovedBeyondTap)
        {
            FHitResult HitResult;
            const bool bHit = GetHitResultUnderFinger(FingerIndex, ClickTraceChannel, false, HitResult);

            if (bHit && HitResult.bBlockingHit && HitResult.GetActor())
            {
                HandleActorClicked(HitResult.GetActor());
            }
            else
            {
                HandleActorClicked(nullptr);
            }
        }
    }
}

// --- Selection & Click Interaction ---

void ASightPortalPlayerController::HandleClickInteraction()
{
    FHitResult HitResult;
    const bool bHit = GetHitResultUnderCursor(ClickTraceChannel, false, HitResult);

    if (bHit && HitResult.bBlockingHit && HitResult.GetActor())
    {
        HandleActorClicked(HitResult.GetActor());
    }
    else
    {
        HandleActorClicked(nullptr);
    }
}

void ASightPortalPlayerController::HandleActorClicked(AActor* ClickedActor)
{
    APropertyVisualizer* TargetVisualizer = ResolvePropertyVisualizerFromActor(ClickedActor);

    if (TargetVisualizer)
    {
        // Select the clicked property visualizer (does NOT pop up the 2D detail widget)
        SelectPropertyVisualizer(TargetVisualizer);
    }
    else
    {
        // Clicked on empty space or non-property geometry
        if (IsPropertyDetailWidgetOpen())
        {
            HidePropertyDetailWidget();
        }
        else if (CurrentSelectedVisualizer)
        {
            DeselectPropertyVisualizer();
        }
    }
}

void ASightPortalPlayerController::FocusOnPropertyVisualizer(APropertyVisualizer* TargetVisualizer)
{
    if (!TargetVisualizer)
    {
        return;
    }

    // If currently in God Mode, cleanly exit God Mode so camera smoothly swoops to property
    if (bIsInGodMode || bIsTransitioningToGodMode)
    {
        bIsInGodMode = false;
        bIsTransitioningToGodMode = false;
        GodModeTransitionTimeElapsed = 0.0f;
        HideServicePOIs();
        OnGodModeToggled.Broadcast(false);
        OnGodModeStateChanged(false);
    }

    TargetFocusLocation = TargetVisualizer->GetLookAtLocation();
    TargetFocusRotation = TargetVisualizer->GetLookAtRotation();
    TargetFocusRotation.Roll = 0.0f; // Keep camera upright and level with horizon

    bIsTransitioningCamera = true;
    bIsMovementLocked = true;

    // Stop mouse look if active and clear current movement input
    if (bIsMouseLooking)
    {
        StopMouseLook();
    }
    CurrentMovementInput = FVector::ZeroVector;
}

void ASightPortalPlayerController::LockMovement()
{
    bIsMovementLocked = true;
    CurrentMovementInput = FVector::ZeroVector;
}

void ASightPortalPlayerController::UnlockMovement()
{
    bIsMovementLocked = false;
    bIsTransitioningCamera = false;

    // Reset Roll rotation to 0.0f so the camera is upright and level with horizon
    FRotator ControlRot = GetControlRotation();
    ControlRot.Roll = 0.0f;
    SetControlRotation(ControlRot);

    AActor* TargetActor = GetPawn();
    if (!TargetActor)
    {
        TargetActor = GetSpectatorPawn();
    }
    if (!TargetActor)
    {
        TargetActor = GetViewTarget();
    }

    if (TargetActor)
    {
        FRotator ActorRot = TargetActor->GetActorRotation();
        ActorRot.Roll = 0.0f;
        TargetActor->SetActorRotation(ActorRot);
    }

    // Restore standard Game & UI input mode
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
}

void ASightPortalPlayerController::SelectPropertyVisualizer(APropertyVisualizer* TargetVisualizer)
{
    if (CurrentSelectedVisualizer == TargetVisualizer)
    {
        // If clicking the same visualizer again, ensure its 3D widget is visible and re-focus camera
        if (TargetVisualizer)
        {
            TargetVisualizer->Show3DWidget();
            FocusOnPropertyVisualizer(TargetVisualizer);
        }
        return;
    }

    // Hide previous visualizer's 3D widget if another property was already selected
    if (CurrentSelectedVisualizer)
    {
        CurrentSelectedVisualizer->Hide3DWidget();
    }

    CurrentSelectedVisualizer = TargetVisualizer;

    if (TargetVisualizer)
    {
        TargetVisualizer->Show3DWidget();
        FocusOnPropertyVisualizer(TargetVisualizer);
        OnPropertySelected.Broadcast(TargetVisualizer);
    }
    else
    {
        UnlockMovement();
        OnPropertyDeselected.Broadcast();
    }
}

void ASightPortalPlayerController::DeselectPropertyVisualizer()
{
    if (CurrentSelectedVisualizer)
    {
        CurrentSelectedVisualizer->Hide3DWidget();
        CurrentSelectedVisualizer = nullptr;
        UnlockMovement();
        OnPropertyDeselected.Broadcast();
    }
}

APropertyVisualizer* ASightPortalPlayerController::ResolvePropertyVisualizerFromActor(AActor* InActor) const
{
    if (!InActor)
    {
        return nullptr;
    }

    if (APropertyVisualizer* DirectVisualizer = Cast<APropertyVisualizer>(InActor))
    {
        return DirectVisualizer;
    }

    if (APropertyVisualizer* OwnerVisualizer = Cast<APropertyVisualizer>(InActor->GetOwner()))
    {
        return OwnerVisualizer;
    }

    if (APropertyVisualizer* ParentVisualizer = Cast<APropertyVisualizer>(InActor->GetAttachParentActor()))
    {
        return ParentVisualizer;
    }

    return nullptr;
}

void ASightPortalPlayerController::TogglePropertyDetailWidget(APropertyVisualizer* TargetVisualizer)
{
    if (!TargetVisualizer)
    {
        HidePropertyDetailWidget();
        return;
    }

    if (IsPropertyDetailWidgetOpen() && CurrentSelectedVisualizer == TargetVisualizer)
    {
        HidePropertyDetailWidget();
    }
    else
    {
        ShowPropertyDetailWidget(TargetVisualizer);
    }
}

USightPortal2DPropertyDetailWidget* ASightPortalPlayerController::ShowPropertyDetailWidget(APropertyVisualizer* TargetVisualizer)
{
    if (!TargetVisualizer)
    {
        return nullptr;
    }

    // Prioritize visualizer-specific Detail2DWidgetClass (e.g. custom Blueprint WBP_PropertyDetail), fallback to player controller default
    TSubclassOf<USightPortal2DPropertyDetailWidget> ClassToSpawn = (TargetVisualizer && TargetVisualizer->Detail2DWidgetClass)
        ? TargetVisualizer->Detail2DWidgetClass
        : Detail2DWidgetClass;

    if (!ClassToSpawn)
    {
        ClassToSpawn = USightPortal2DPropertyDetailWidget::StaticClass();
    }

    // Recreate widget if it doesn't exist, is invalid, or is an instance of a different class
    if (!Active2DDetailWidget || !IsValid(Active2DDetailWidget) || Active2DDetailWidget->GetClass() != ClassToSpawn)
    {
        if (Active2DDetailWidget && Active2DDetailWidget->IsInViewport())
        {
            Active2DDetailWidget->RemoveFromParent();
        }

        Active2DDetailWidget = CreateWidget<USightPortal2DPropertyDetailWidget>(this, ClassToSpawn);
        if (Active2DDetailWidget)
        {
            Active2DDetailWidget->OnDetailClosed.AddUniqueDynamic(this, &ASightPortalPlayerController::OnDetailWidgetClosed);
        }
    }

    if (Active2DDetailWidget)
    {
        // Hide floating 3D world widget when 2D detail popup is opened
        TargetVisualizer->Hide3DWidget();

        if (!Active2DDetailWidget->IsInViewport())
        {
            Active2DDetailWidget->AddToViewport(100);
        }

        Active2DDetailWidget->DisplayPropertyDetails(TargetVisualizer->PropertyDetails);
        CurrentSelectedVisualizer = TargetVisualizer;

        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(Active2DDetailWidget->TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
        bShowMouseCursor = true;

        OnPropertySelected.Broadcast(TargetVisualizer);
    }

    return Active2DDetailWidget;
}

void ASightPortalPlayerController::HidePropertyDetailWidget()
{
    if (Active2DDetailWidget && Active2DDetailWidget->IsInViewport())
    {
        Active2DDetailWidget->RemoveFromParent();
    }

    if (CurrentSelectedVisualizer)
    {
        CurrentSelectedVisualizer->Hide3DWidget();
    }

    CurrentSelectedVisualizer = nullptr;
    UnlockMovement();
    OnPropertyDeselected.Broadcast();
}

bool ASightPortalPlayerController::IsPropertyDetailWidgetOpen() const
{
    return (Active2DDetailWidget != nullptr) && Active2DDetailWidget->IsInViewport();
}

void ASightPortalPlayerController::OnDetailWidgetClosed()
{
    if (CurrentSelectedVisualizer)
    {
        CurrentSelectedVisualizer->Hide3DWidget();
    }

    CurrentSelectedVisualizer = nullptr;
    UnlockMovement();
    OnPropertyDeselected.Broadcast();
}

USightPortalHUDWidget* ASightPortalPlayerController::ShowMainHUD()
{
    if (!MainHUDWidgetClass)
    {
        MainHUDWidgetClass = USightPortalHUDWidget::StaticClass();
    }

    if (!ActiveMainHUDWidget || !IsValid(ActiveMainHUDWidget) || ActiveMainHUDWidget->GetClass() != MainHUDWidgetClass)
    {
        if (ActiveMainHUDWidget && ActiveMainHUDWidget->IsInViewport())
        {
            ActiveMainHUDWidget->RemoveFromParent();
        }

        ActiveMainHUDWidget = CreateWidget<USightPortalHUDWidget>(this, MainHUDWidgetClass);
    }

    if (ActiveMainHUDWidget && !ActiveMainHUDWidget->IsInViewport())
    {
        ActiveMainHUDWidget->AddToViewport(10);
    }

    if (ActiveMainHUDWidget)
    {
        ActiveMainHUDWidget->OnServicesButtonClicked.AddUniqueDynamic(this, &ASightPortalPlayerController::OnHUDServicesClicked);
        ActiveMainHUDWidget->OnHomeNavigationTriggered.AddUniqueDynamic(this, &ASightPortalPlayerController::OnHUDHomeTriggered);
    }

    return ActiveMainHUDWidget;
}

void ASightPortalPlayerController::HideMainHUD()
{
    if (ActiveMainHUDWidget && ActiveMainHUDWidget->IsInViewport())
    {
        ActiveMainHUDWidget->RemoveFromParent();
    }
}

void ASightPortalPlayerController::ToggleMainHUD()
{
    if (ActiveMainHUDWidget && ActiveMainHUDWidget->IsInViewport())
    {
        HideMainHUD();
    }
    else
    {
        ShowMainHUD();
    }
}

// --- God Mode (Services POI Exploration) Implementation ---

void ASightPortalPlayerController::EnterGodMode()
{
    AActor* CurrentActor = GetPawn();
    if (!CurrentActor)
    {
        CurrentActor = GetSpectatorPawn();
    }
    if (!CurrentActor)
    {
        CurrentActor = GetViewTarget();
    }

    if (CurrentActor)
    {
        const FVector CurrentLoc = CurrentActor->GetActorLocation();
        // Save current location and rotation before flying up to God Mode (if on ground or below God Mode altitude)
        if (!bIsInGodMode || CurrentLoc.Z < (GodModeLocation.Z - 2000.0f))
        {
            PreGodModeLocation = CurrentLoc;
            PreGodModeRotation = GetControlRotation();
        }
    }

    // Dismiss any active property detail 2D widget
    if (IsPropertyDetailWidgetOpen())
    {
        HidePropertyDetailWidget();
    }

    bIsInGodMode = true;
    bIsTransitioningToGodMode = true;
    GodModeTransitionTimeElapsed = 0.0f;
    bIsTransitioningCamera = false; // Cancel any active property LookAt transition
    bIsMovementLocked = false;

    // Ensure cursor and interaction events are active
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    // Reveal Service POIs immediately so they are visible during flight
    if (bAutoRevealServicePOIsInGodMode)
    {
        RevealServicePOIs();
    }

    OnGodModeToggled.Broadcast(true);
    OnGodModeStateChanged(true);
}

void ASightPortalPlayerController::ExitGodMode()
{
    bIsInGodMode = false;
    bIsTransitioningToGodMode = true;
    GodModeTransitionTimeElapsed = 0.0f;

    // If PreGodModeLocation was never set or is near zero, safely fallback to HomeLocation
    if (PreGodModeLocation.IsNearlyZero())
    {
        if (ActiveMainHUDWidget && ActiveMainHUDWidget->IsHomeLocationInitialized() && !ActiveMainHUDWidget->HomeLocation.IsNearlyZero())
        {
            PreGodModeLocation = ActiveMainHUDWidget->HomeLocation;
            PreGodModeRotation = ActiveMainHUDWidget->HomeRotation;
        }
        else
        {
            PreGodModeLocation = FVector(GodModeLocation.X, GodModeLocation.Y, 200.0f);
            PreGodModeRotation = FRotator(0.0f, GodModeRotation.Yaw, 0.0f);
        }
    }

    // Hide service POI 3D widgets
    HideServicePOIs();

    OnGodModeToggled.Broadcast(false);
    OnGodModeStateChanged(false);
}

void ASightPortalPlayerController::ToggleGodMode()
{
    const double CurrentTime = FPlatformTime::Seconds();
    static double LastToggleTime = 0.0;
    if (CurrentTime - LastToggleTime < 0.2)
    {
        return; // Debounce rapid double-calls within 200ms
    }
    LastToggleTime = CurrentTime;

    if (bIsInGodMode)
    {
        ExitGodMode();
    }
    else
    {
        EnterGodMode();
    }
}

void ASightPortalPlayerController::SetGodModeTransform(FVector InLocation, FRotator InRotation)
{
    GodModeLocation = InLocation;
    GodModeRotation = InRotation;

    if (bIsInGodMode)
    {
        bIsTransitioningToGodMode = true;
    }
}

void ASightPortalPlayerController::RevealServicePOIs()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<APropertyVisualizer> It(World); It; ++It)
    {
        APropertyVisualizer* Visualizer = *It;
        if (Visualizer && IsValid(Visualizer))
        {
            // If ServicePOITag is specified, only reveal visualizers tagged with it
            if (ServicePOITag.IsNone() || ServicePOITag.IsEqual(NAME_None) || Visualizer->ActorHasTag(ServicePOITag))
            {
                Visualizer->Show3DWidget();
            }
        }
    }
}

void ASightPortalPlayerController::HideServicePOIs()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<APropertyVisualizer> It(World); It; ++It)
    {
        APropertyVisualizer* Visualizer = *It;
        if (Visualizer && IsValid(Visualizer))
        {
            if (ServicePOITag.IsNone() || ServicePOITag.IsEqual(NAME_None) || Visualizer->ActorHasTag(ServicePOITag))
            {
                Visualizer->Hide3DWidget();
            }
        }
    }
}

void ASightPortalPlayerController::OnHUDServicesClicked()
{
    ToggleGodMode();
}

void ASightPortalPlayerController::OnHUDHomeTriggered(FVector InLocation, FRotator InRotation)
{
    if (bIsInGodMode || bIsTransitioningToGodMode)
    {
        bIsInGodMode = false;
        bIsTransitioningToGodMode = false;
        GodModeTransitionTimeElapsed = 0.0f;
        HideServicePOIs();
        OnGodModeToggled.Broadcast(false);
        OnGodModeStateChanged(false);
    }
    PreGodModeLocation = InLocation;
    PreGodModeRotation = InRotation;
}


