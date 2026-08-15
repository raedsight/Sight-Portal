#include "SightPortal3DPropertyWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void USightPortal3DPropertyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ExploreButton)
    {
        ExploreButton->OnClicked.AddDynamic(this, &USightPortal3DPropertyWidget::OnExploreClicked);
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &USightPortal3DPropertyWidget::OnCloseClicked);
    }

    // Default to visible when constructed
    SetVisibility(ESlateVisibility::Visible);

    // Refresh UI elements with current cached property
    SetPropertyData(CachedProperty);
}

void USightPortal3DPropertyWidget::NativeDestruct()
{
    if (ExploreButton)
    {
        ExploreButton->OnClicked.RemoveDynamic(this, &USightPortal3DPropertyWidget::OnExploreClicked);
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.RemoveDynamic(this, &USightPortal3DPropertyWidget::OnCloseClicked);
    }

    Super::NativeDestruct();
}

void USightPortal3DPropertyWidget::SetPropertyData(const FSightPortalProperty& InProperty)
{
    CachedProperty = InProperty;

    if (SurfaceText)
    {
        FString SurfaceString = FString::Printf(TEXT("%.1f m²"), InProperty.Surface);
        SurfaceText->SetText(FText::FromString(SurfaceString));
    }

    if (BedroomsText)
    {
        FString BedroomsString = FString::Printf(TEXT("%d Beds"), InProperty.BedroomsCount);
        BedroomsText->SetText(FText::FromString(BedroomsString));
    }

    if (PropertyNameText)
    {
        PropertyNameText->SetText(FText::FromString(InProperty.Name.IsEmpty() ? TEXT("Property") : InProperty.Name));
    }
}

void USightPortal3DPropertyWidget::ShowWidget()
{
    SetVisibility(ESlateVisibility::Visible);
}

void USightPortal3DPropertyWidget::HideWidget()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void USightPortal3DPropertyWidget::SetWidgetVisibility(bool bVisible)
{
    SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void USightPortal3DPropertyWidget::OnExploreClicked()
{
    // Hide the 3D widget when Explore is clicked
    HideWidget();

    // Broadcast event to open full 2D detail popup
    OnExploreRequested.Broadcast(CachedProperty);
}

void USightPortal3DPropertyWidget::OnCloseClicked()
{
    // Hide the 3D widget when Close is clicked
    HideWidget();

    // Broadcast close event
    OnCloseRequested.Broadcast();
}
