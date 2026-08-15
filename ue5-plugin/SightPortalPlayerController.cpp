#include "SightPortalPlayerController.h"
#include "PropertyVisualizer.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"

ASightPortalPlayerController::ASightPortalPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bEnableTouchEvents = true;
    bEnableTouchOverEvents = true;

    ClickTraceChannel = ECC_Visibility;
    bAutoEnableMouseInteraction = true;

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

void ASightPortalPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (InputComponent)
    {
        // Bind primary mouse click
        InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);

        // Bind touch / tap support for mobile or tablet ArchViz
        InputComponent->BindTouch(IE_Pressed, this, &ASightPortalPlayerController::HandleTouchInteraction);

        // Also bind custom action mapping if configured in Project Settings
        InputComponent->BindAction("SightPortalClick", IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);
        InputComponent->BindAction("Interact", IE_Pressed, this, &ASightPortalPlayerController::HandleClickInteraction);
    }
}

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
        // Clicked into empty space / skybox
        HandleActorClicked(nullptr);
    }
}

void ASightPortalPlayerController::HandleTouchInteraction(ETouchIndex::Type FingerIndex, FVector Location)
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

void ASightPortalPlayerController::HandleActorClicked(AActor* ClickedActor)
{
    APropertyVisualizer* TargetVisualizer = ResolvePropertyVisualizerFromActor(ClickedActor);

    if (TargetVisualizer)
    {
        // Clicked directly on a Property Visualizer actor -> toggle details
        TogglePropertyDetailWidget(TargetVisualizer);
    }
    else
    {
        // Clicked away on ground, background environment, or another scene actor -> toggle hide
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

    // Direct cast check
    if (APropertyVisualizer* DirectVisualizer = Cast<APropertyVisualizer>(InActor))
    {
        return DirectVisualizer;
    }

    // Check actor owner hierarchy
    if (APropertyVisualizer* OwnerVisualizer = Cast<APropertyVisualizer>(InActor->GetOwner()))
    {
        return OwnerVisualizer;
    }

    // Check attachment parent hierarchy
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

    // If already open showing this exact property visualizer -> toggle hide
    if (IsPropertyDetailWidgetOpen() && CurrentSelectedVisualizer == TargetVisualizer)
    {
        HidePropertyDetailWidget();
    }
    else
    {
        // Show or switch to new property visualizer
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

        // Ensure mouse cursor is visible and interaction mode active
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
