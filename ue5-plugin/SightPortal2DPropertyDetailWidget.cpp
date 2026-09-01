#include "SightPortal2DPropertyDetailWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"

void USightPortal2DPropertyDetailWidget::ResolveUnboundWidgets()
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

    if (!NameText) NameText = FindTextBlockWithAliases({ TEXT("NameText"), TEXT("Name"), TEXT("PropertyName"), TEXT("PropertyNameText"), TEXT("Name_Text"), TEXT("TextBlock_Name") });
    if (!ZoneText) ZoneText = FindTextBlockWithAliases({ TEXT("ZoneText"), TEXT("Zone"), TEXT("ZoneName"), TEXT("Zone_Text"), TEXT("TextBlock_Zone") });
    if (!BlockText) BlockText = FindTextBlockWithAliases({ TEXT("BlockText"), TEXT("Block"), TEXT("BlockName"), TEXT("Block_Text"), TEXT("TextBlock_Block") });
    if (!DoorNoText) DoorNoText = FindTextBlockWithAliases({ TEXT("DoorNoText"), TEXT("DoorText"), TEXT("Door"), TEXT("DoorNo"), TEXT("DoorNum"), TEXT("DoorNumber"), TEXT("Door_Text"), TEXT("TextBlock_Door") });
    if (!PriceText) PriceText = FindTextBlockWithAliases({ TEXT("PriceText"), TEXT("Price"), TEXT("Price_Text"), TEXT("PropertyPrice"), TEXT("TextBlock_Price") });
    if (!SurfaceText) SurfaceText = FindTextBlockWithAliases({ TEXT("SurfaceText"), TEXT("Surface"), TEXT("SurfaceArea"), TEXT("Area"), TEXT("Surface_Text"), TEXT("TextBlock_Surface") });
    if (!BuildingSurfaceText) BuildingSurfaceText = FindTextBlockWithAliases({ TEXT("BuildingSurfaceText"), TEXT("BldSurfaceText"), TEXT("Bld Surface"), TEXT("BldSurface"), TEXT("Bld_Surface"), TEXT("BuildingSurface"), TEXT("Building_Surface"), TEXT("BuiltSurface"), TEXT("Built_Surface"), TEXT("TextBlock_BldSurface") });
    if (!AvailabilityText) AvailabilityText = FindTextBlockWithAliases({ TEXT("AvailabilityText"), TEXT("Availability"), TEXT("Status"), TEXT("StatusText"), TEXT("Availability_Text"), TEXT("TextBlock_Availability") });
    if (!BedroomsCountText) BedroomsCountText = FindTextBlockWithAliases({ TEXT("BedroomsCountText"), TEXT("BedroomsText"), TEXT("Bedrooms"), TEXT("BedroomsCount"), TEXT("Beds"), TEXT("BedsText"), TEXT("BedCount"), TEXT("Bedrooms_Text"), TEXT("TextBlock_Bedrooms") });
    if (!BathroomsCountText) BathroomsCountText = FindTextBlockWithAliases({ TEXT("BathroomsCountText"), TEXT("BathroomsText"), TEXT("Bathrooms"), TEXT("BathroomsCount"), TEXT("Baths"), TEXT("BathsText"), TEXT("BathCount"), TEXT("Bathrooms_Text"), TEXT("TextBlock_Bathrooms") });
    if (!ClassText) ClassText = FindTextBlockWithAliases({ TEXT("ClassText"), TEXT("Class"), TEXT("Category"), TEXT("CategoryText"), TEXT("PropertyClass"), TEXT("Class_Text"), TEXT("TextBlock_Class") });

    if (!CloseButton)
    {
        CloseButton = FindButtonWithAliases({ TEXT("CloseButton"), TEXT("Exit"), TEXT("ExitButton"), TEXT("ExitBtn"), TEXT("CloseBtn"), TEXT("Close_Button"), TEXT("Exit_Button"), TEXT("Button_Exit"), TEXT("Button_Close"), TEXT("Btn_Exit"), TEXT("Btn_Close"), TEXT("DismissButton") });
    }
}

void USightPortal2DPropertyDetailWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ResolveUnboundWidgets();

    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &USightPortal2DPropertyDetailWidget::OnCloseButtonClicked);
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

    ResolveUnboundWidgets();

    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &USightPortal2DPropertyDetailWidget::OnCloseButtonClicked);
    }

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

    // Trigger Blueprint Implementable Event so Blueprint UMG widgets can run custom logic/animations
    OnPropertyDataUpdatedFromPortal(InProperty);
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
