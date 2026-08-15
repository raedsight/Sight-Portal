#include "SightPortalPlayerController.h"
#include "PropertyVisualizer.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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

    CurrentMovementInput = FVector::ZeroVector;
    bIsSprinting = false;
    bIsMouseLooking = false;

    Detail2DWidgetClass = USightPortal2DPropertyDetailWidget::StaticClass();
    Active2DDetailWidget = nullptr;
    CurrentSelectedVisualizer = nullptr;
}

void ASightPortalPlayerController::BeginPlay()
{
    Super::BeginPlay();

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
}

void ASightPortalPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    ApplyDirectMovement(DeltaTime);
}

void ASightPortalPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!InputComponent)
    {
        return;
    }

    // --- Mouse & Touch Clicks ---
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);

    // --- Mouse Look (Hold Right Mouse Button) ---
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ASightPortalPlayerController::StartMouseLook);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ASightPortalPlayerController::StopMouseLook);

    // --- Mouse Axes ---
    InputComponent->BindAxisKey(EKeys::MouseX, this, &ASightPortalPlayerController::OnMouseMoveX);
    InputComponent->BindAxisKey(EKeys::MouseY, this, &ASightPortalPlayerController::OnMouseMoveY);
    InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &ASightPortalPlayerController::OnMouseWheelZoom);

    // --- Keyboard WASD & Arrow Movement ---
    InputComponent->BindAxisKey(EKeys::W, this, &ASightPortalPlayerController::MoveForward);
    InputComponent->BindAxisKey(EKeys::S, this, &ASightPortalPlayerController::MoveForward);
    InputComponent->BindAxisKey(EKeys::Up, this, &ASightPortalPlayerController::MoveForward);
    InputComponent->BindAxisKey(EKeys::Down, this, &ASightPortalPlayerController::MoveForward);

    InputComponent->BindAxisKey(EKeys::D, this, &ASightPortalPlayerController::MoveRight);
    InputComponent->BindAxisKey(EKeys::A, this, &ASightPortalPlayerController::MoveRight);
    InputComponent->BindAxisKey(EKeys::Right, this, &ASightPortalPlayerController::MoveRight);
    InputComponent->BindAxisKey(EKeys::Left, this, &ASightPortalPlayerController::MoveRight);

    InputComponent->BindAxisKey(EKeys::E, this, &ASightPortalPlayerController::MoveUp);
    InputComponent->BindAxisKey(EKeys::Q, this, &ASightPortalPlayerController::MoveUp);
    InputComponent->BindAxisKey(EKeys::SpaceBar, this, &ASightPortalPlayerController::MoveUp);

    // --- Sprint ---
    InputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ASightPortalPlayerController::StartSprint);
    InputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ASightPortalPlayerController::StopSprint);

    // --- Touch Gestures ---
    InputComponent->BindTouch(IE_Pressed, this, &ASightPortalPlayerController::HandleTouchStarted);
    InputComponent->BindTouch(IE_Repeat, this, &ASightPortalPlayerController::HandleTouchMoved);
    InputComponent->BindTouch(IE_Released, this, &ASightPortalPlayerController::HandleTouchEnded);

    // --- Optional Named Action / Axis Bindings ---
    InputComponent->BindAction("SightPortalClick", IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);
    InputComponent->BindAction("Interact", IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);
}

// --- Movement Implementation ---

void ASightPortalPlayerController::MoveForward(float Value)
{
    CurrentMovementInput.X = Value;
}

void ASightPortalPlayerController::MoveRight(float Value)
{
    CurrentMovementInput.Y = Value;
}

void ASightPortalPlayerController::MoveUp(float Value)
{
    CurrentMovementInput.Z = Value;
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
    if (bIsMouseLooking && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        AddYawInput(Value * MouseLookSensitivity);
    }
}

void ASightPortalPlayerController::OnMouseMoveY(float Value)
{
    if (bIsMouseLooking && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
    {
        const float PitchFactor = bInvertLookPitch ? 1.0f : -1.0f;
        AddPitchInput(Value * MouseLookSensitivity * PitchFactor);
    }
}

void ASightPortalPlayerController::OnMouseWheelZoom(float Value)
{
    if (FMath::Abs(Value) > KINDA_SMALL_NUMBER)
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

    APawn* TargetPawn = GetPawn();
    if (!TargetPawn)
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

        if (UPawnMovementComponent* MoveComp = TargetPawn->GetMovementComponent())
        {
            TargetPawn->AddMovementInput(MoveDirection, (bIsSprinting ? SprintSpeedMultiplier : 1.0f));
        }
        else
        {
            TargetPawn->AddActorWorldOffset(MoveDirection * CurrentSpeed * DeltaTime, true);
        }
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
        TogglePropertyDetailWidget(TargetVisualizer);
    }
    else
    {
        if (IsPropertyDetailWidgetOpen())
        {
            HidePropertyDetailWidget();
        }
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

    if (!Detail2DWidgetClass)
    {
        Detail2DWidgetClass = USightPortal2DPropertyDetailWidget::StaticClass();
    }

    if (!Active2DDetailWidget || !IsValid(Active2DDetailWidget))
    {
        Active2DDetailWidget = CreateWidget<USightPortal2DPropertyDetailWidget>(this, Detail2DWidgetClass);
        if (Active2DDetailWidget)
        {
            Active2DDetailWidget->OnDetailClosed.AddUniqueDynamic(this, &ASightPortalPlayerController::OnDetailWidgetClosed);
        }
    }

    if (Active2DDetailWidget)
    {
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

    CurrentSelectedVisualizer = nullptr;
    OnPropertyDeselected.Broadcast();
}

bool ASightPortalPlayerController::IsPropertyDetailWidgetOpen() const
{
    return (Active2DDetailWidget != nullptr) && Active2DDetailWidget->IsInViewport();
}

void ASightPortalPlayerController::OnDetailWidgetClosed()
{
    CurrentSelectedVisualizer = nullptr;
    OnPropertyDeselected.Broadcast();
}

