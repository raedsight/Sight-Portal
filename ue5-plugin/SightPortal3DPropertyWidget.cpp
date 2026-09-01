#include "SightPortal3DPropertyWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"

void USightPortal3DPropertyWidget::ResolveUnboundWidgets()
{
    auto FindTextBlockWithAliases = [this](const TArray<FString>& Aliases) -> UTextBlock*
    {
        for (const FString& Alias : Aliases)
        {
            if (UWidget* FoundWidget = GetWidgetFromName(FName(*Alias)))
            {
                if (UTextBlock* FoundText = Cast<UTextBlock>(FoundWidget))
                {
                    return FoundText;
                }
            }
        }
        return nullptr;
    };

    auto FindButtonWithAliases = [this](const TArray<FString>& Aliases) -> UButton*
    {
        for (const FString& Alias : Aliases)
        {
            if (UWidget* FoundWidget = GetWidgetFromName(FName(*Alias)))
            {
                if (UButton* FoundBtn = Cast<UButton>(FoundWidget))
                {
                    return FoundBtn;
                }
            }
        }
        return nullptr;
    };

    if (!SurfaceText) SurfaceText = FindTextBlockWithAliases({ TEXT("SurfaceText"), TEXT("Surface"), TEXT("SurfaceArea"), TEXT("Area") });
    if (!BedroomsText) BedroomsText = FindTextBlockWithAliases({ TEXT("BedroomsText"), TEXT("Bedrooms"), TEXT("BedroomsCount"), TEXT("Beds"), TEXT("BedsText") });
    if (!PropertyNameText) PropertyNameText = FindTextBlockWithAliases({ TEXT("PropertyNameText"), TEXT("PropertyName"), TEXT("NameText"), TEXT("Name") });
    if (!PriceText) PriceText = FindTextBlockWithAliases({ TEXT("PriceText"), TEXT("Price"), TEXT("PropertyPrice") });
    if (!AvailabilityText) AvailabilityText = FindTextBlockWithAliases({ TEXT("AvailabilityText"), TEXT("Availability"), TEXT("Status") });
    if (!DoorNoText) DoorNoText = FindTextBlockWithAliases({ TEXT("DoorNoText"), TEXT("DoorText"), TEXT("Door"), TEXT("DoorNo") });
    if (!BlockText) BlockText = FindTextBlockWithAliases({ TEXT("BlockText"), TEXT("Block") });
    if (!ZoneText) ZoneText = FindTextBlockWithAliases({ TEXT("ZoneText"), TEXT("Zone") });
    if (!ClassText) ClassText = FindTextBlockWithAliases({ TEXT("ClassText"), TEXT("Class") });

    if (!ExploreButton)
    {
        ExploreButton = FindButtonWithAliases({ TEXT("ExploreButton"), TEXT("Explore"), TEXT("ExploreBtn"), TEXT("Btn_Explore"), TEXT("Button_Explore") });
    }

    if (!CloseButton)
    {
        CloseButton = FindButtonWithAliases({ TEXT("CloseButton"), TEXT("Close"), TEXT("CloseBtn"), TEXT("Exit"), TEXT("ExitButton"), TEXT("Btn_Close"), TEXT("Button_Close") });
    }
}

void USightPortal3DPropertyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ResolveUnboundWidgets();

    if (ExploreButton)
    {
        ExploreButton->OnClicked.AddUniqueDynamic(this, &USightPortal3DPropertyWidget::OnExploreClicked);
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &USightPortal3DPropertyWidget::OnCloseClicked);
    }

    // Default to collapsed when constructed until explicitly shown on selection
    SetVisibility(ESlateVisibility::Collapsed);

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

    ResolveUnboundWidgets();

    if (ExploreButton)
    {
        ExploreButton->OnClicked.AddUniqueDynamic(this, &USightPortal3DPropertyWidget::OnExploreClicked);
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &USightPortal3DPropertyWidget::OnCloseClicked);
    }

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

    if (PriceText)
    {
        FString PriceString = FString::Printf(TEXT("$%.2f"), InProperty.Price);
        PriceText->SetText(FText::FromString(PriceString));
    }

    if (AvailabilityText)
    {
        AvailabilityText->SetText(FText::FromString(InProperty.Availability.IsEmpty() ? TEXT("Available") : InProperty.Availability));
    }

    if (DoorNoText)
    {
        DoorNoText->SetText(FText::FromString(FString::Printf(TEXT("#%d"), InProperty.DoorNo)));
    }

    if (BlockText)
    {
        BlockText->SetText(FText::FromString(InProperty.Block.IsEmpty() ? TEXT("") : InProperty.Block));
    }

    if (ZoneText)
    {
        ZoneText->SetText(FText::FromString(InProperty.Zone.IsEmpty() ? TEXT("") : InProperty.Zone));
    }

    if (ClassText)
    {
        ClassText->SetText(FText::FromString(InProperty.Class.IsEmpty() ? TEXT("") : InProperty.Class));
    }

    // Trigger Blueprint Implementable Event for custom UMG widget styling/animations
    OnPropertyDataUpdatedFromPortal(InProperty);
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
