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

    // Refresh UI elements with current cached property
    SetPropertyData(CachedProperty);
}

void USightPortal3DPropertyWidget::NativeDestruct()
{
    if (ExploreButton)
    {
        ExploreButton->OnClicked.RemoveDynamic(this, &USightPortal3DPropertyWidget::OnExploreClicked);
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

void USightPortal3DPropertyWidget::OnExploreClicked()
{
    OnExploreRequested.Broadcast(CachedProperty);
}
