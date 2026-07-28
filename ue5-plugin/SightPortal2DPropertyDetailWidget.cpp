#include "SightPortal2DPropertyDetailWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void USightPortal2DPropertyDetailWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &USightPortal2DPropertyDetailWidget::OnCloseButtonClicked);
    }

    DisplayPropertyDetails(ActiveProperty);
}

void USightPortal2DPropertyDetailWidget::NativeDestruct()
{
    if (CloseButton)
    {
        CloseButton->OnClicked.RemoveDynamic(this, &USightPortal2DPropertyDetailWidget::OnCloseButtonClicked);
    }

    Super::NativeDestruct();
}

void USightPortal2DPropertyDetailWidget::DisplayPropertyDetails(const FSightPortalProperty& InProperty)
{
    ActiveProperty = InProperty;

    if (NameText)
    {
        NameText->SetText(FText::FromString(InProperty.Name.IsEmpty() ? TEXT("N/A") : InProperty.Name));
    }

    if (ZoneText)
    {
        ZoneText->SetText(FText::FromString(InProperty.Zone.IsEmpty() ? TEXT("N/A") : InProperty.Zone));
    }

    if (BlockText)
    {
        BlockText->SetText(FText::FromString(InProperty.Block.IsEmpty() ? TEXT("N/A") : InProperty.Block));
    }

    if (DoorNoText)
    {
        DoorNoText->SetText(FText::FromString(FString::Printf(TEXT("#%d"), InProperty.DoorNo)));
    }

    if (PriceText)
    {
        FString FormattedPrice = FString::Printf(TEXT("$%.2f"), InProperty.Price);
        PriceText->SetText(FText::FromString(FormattedPrice));
    }

    if (SurfaceText)
    {
        SurfaceText->SetText(FText::FromString(FString::Printf(TEXT("%.2f m²"), InProperty.Surface)));
    }

    if (BuildingSurfaceText)
    {
        BuildingSurfaceText->SetText(FText::FromString(FString::Printf(TEXT("%.2f m²"), InProperty.BuildingSurface)));
    }

    if (AvailabilityText)
    {
        AvailabilityText->SetText(FText::FromString(InProperty.Availability.IsEmpty() ? TEXT("Available") : InProperty.Availability));
    }

    if (BedroomsCountText)
    {
        BedroomsCountText->SetText(FText::FromString(FString::Printf(TEXT("%d Bedrooms"), InProperty.BedroomsCount)));
    }

    if (BathroomsCountText)
    {
        BathroomsCountText->SetText(FText::FromString(FString::Printf(TEXT("%d Bathrooms"), InProperty.BathroomsCount)));
    }

    if (ClassText)
    {
        ClassText->SetText(FText::FromString(InProperty.Class.IsEmpty() ? TEXT("Standard") : InProperty.Class));
    }
}

void USightPortal2DPropertyDetailWidget::DismissWidget()
{
    RemoveFromParent();
    OnDetailClosed.Broadcast();
}

void USightPortal2DPropertyDetailWidget::OnCloseButtonClicked()
{
    DismissWidget();
}
