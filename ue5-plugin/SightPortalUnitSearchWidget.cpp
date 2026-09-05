#include "SightPortalUnitSearchWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "PropertyVisualizer.h"
#include "SightPortalPlayerController.h"
#include "SightPortal2DPropertyDetailWidget.h"
#include "SightPortalSiteManager.h"
#include "Engine/World.h"

// ----------------------------------------------------------------------------------
// USightPortalUnitSearchResultWidget Implementation
// ----------------------------------------------------------------------------------

USightPortalUnitSearchResultWidget::USightPortalUnitSearchResultWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , ParentSearchWidget(nullptr)
    , RoomType(TEXT("Bedrooms"))
    , NameText(nullptr)
    , ZoneText(nullptr)
    , BlockText(nullptr)
    , ZoneBlockText(nullptr)
    , DoorNoText(nullptr)
    , PriceText(nullptr)
    , SurfaceText(nullptr)
    , BuildingSurfaceText(nullptr)
    , BedroomsText(nullptr)
    , BathroomsText(nullptr)
    , AvailabilityText(nullptr)
    , ClassText(nullptr)
    , CardButton(nullptr)
    , LocateButton(nullptr)
    , ExploreButton(nullptr)
{
}

void USightPortalUnitSearchResultWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ResolveUnboundWidgets();

    if (CardButton)
    {
        CardButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchResultWidget::OnCardClicked);
    }
    if (LocateButton)
    {
        LocateButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchResultWidget::OnLocateClicked);
    }
    if (ExploreButton)
    {
        ExploreButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchResultWidget::OnExploreClicked);
    }
}

void USightPortalUnitSearchResultWidget::ResolveUnboundWidgets()
{
    auto FindTextBlockWithAliases = [this](const TArray<FString>& Aliases) -> UTextBlock*
    {
        for (const FString& Name : Aliases)
        {
            if (UTextBlock* Found = Cast<UTextBlock>(GetWidgetFromName(FName(*Name))))
            {
                return Found;
            }
        }
        return nullptr;
    };

    auto FindButtonWithAliases = [this](const TArray<FString>& Aliases) -> UButton*
    {
        for (const FString& Name : Aliases)
        {
            if (UButton* Found = Cast<UButton>(GetWidgetFromName(FName(*Name))))
            {
                return Found;
            }
        }
        return nullptr;
    };

    if (!NameText)
    {
        NameText = FindTextBlockWithAliases({ TEXT("NameText"), TEXT("UnitNameText"), TEXT("TitleText"), TEXT("PropertyNameText"), TEXT("Txt_Name") });
    }
    if (!ZoneText)
    {
        ZoneText = FindTextBlockWithAliases({ TEXT("ZoneText"), TEXT("Txt_Zone"), TEXT("ZoneLabel") });
    }
    if (!BlockText)
    {
        BlockText = FindTextBlockWithAliases({ TEXT("BlockText"), TEXT("Txt_Block"), TEXT("BlockLabel") });
    }
    if (!ZoneBlockText)
    {
        ZoneBlockText = FindTextBlockWithAliases({ TEXT("ZoneBlockText"), TEXT("Txt_ZoneBlock"), TEXT("SubtitleText"), TEXT("LocationText") });
    }
    if (!DoorNoText)
    {
        DoorNoText = FindTextBlockWithAliases({ TEXT("DoorNoText"), TEXT("Txt_DoorNo"), TEXT("DoorText"), TEXT("UnitNumberText") });
    }
    if (!PriceText)
    {
        PriceText = FindTextBlockWithAliases({ TEXT("PriceText"), TEXT("Txt_Price"), TEXT("CostText") });
    }
    if (!SurfaceText)
    {
        SurfaceText = FindTextBlockWithAliases({ TEXT("SurfaceText"), TEXT("Txt_Surface"), TEXT("AreaText"), TEXT("SizeText") });
    }
    if (!BuildingSurfaceText)
    {
        BuildingSurfaceText = FindTextBlockWithAliases({ TEXT("BuildingSurfaceText"), TEXT("Txt_BuildingSurface"), TEXT("BuiltAreaText") });
    }
    if (!BedroomsText)
    {
        BedroomsText = FindTextBlockWithAliases({ TEXT("BedroomsText"), TEXT("BedroomsCountText"), TEXT("Txt_Bedrooms"), TEXT("BedsText") });
    }
    if (!BathroomsText)
    {
        BathroomsText = FindTextBlockWithAliases({ TEXT("BathroomsText"), TEXT("BathroomsCountText"), TEXT("Txt_Bathrooms"), TEXT("BathsText") });
    }
    if (!AvailabilityText)
    {
        AvailabilityText = FindTextBlockWithAliases({ TEXT("AvailabilityText"), TEXT("Txt_Availability"), TEXT("StatusText"), TEXT("StateText") });
    }
    if (!ClassText)
    {
        ClassText = FindTextBlockWithAliases({ TEXT("ClassText"), TEXT("Txt_Class"), TEXT("CategoryText"), TEXT("PropertyType") });
    }

    if (!CardButton)
    {
        CardButton = FindButtonWithAliases({ TEXT("CardButton"), TEXT("Btn_Card"), TEXT("ItemButton"), TEXT("RowButton"), TEXT("Button_Card") });
    }
    if (!LocateButton)
    {
        LocateButton = FindButtonWithAliases({ TEXT("LocateButton"), TEXT("Btn_Locate"), TEXT("FocusButton"), TEXT("FlyToButton"), TEXT("ViewIn3DButton") });
    }
    if (!ExploreButton)
    {
        ExploreButton = FindButtonWithAliases({ TEXT("ExploreButton"), TEXT("Btn_Explore"), TEXT("DetailsButton"), TEXT("InfoButton"), TEXT("ViewDetailsButton") });
    }
}

void USightPortalUnitSearchResultWidget::SetupResultCard(const FSightPortalProperty& InProperty, APropertyVisualizer* InVisualizer, USightPortalUnitSearchWidget* InParent, const FString& InRoomType, const FSightPortalCurrencySetting& InCurrency)
{
    PropertyData = InProperty;
    AssociatedVisualizer = InVisualizer;
    ParentSearchWidget = InParent;
    RoomType = InRoomType.IsEmpty() ? TEXT("Bedrooms") : InRoomType;
    ActiveCurrency = InCurrency;

    ResolveUnboundWidgets();

    if (NameText)
    {
        NameText->SetText(FText::FromString(InProperty.Name));
    }
    if (ZoneText)
    {
        ZoneText->SetText(FText::FromString(InProperty.Zone));
    }
    if (BlockText)
    {
        BlockText->SetText(FText::FromString(InProperty.Block));
    }
    if (ZoneBlockText)
    {
        FString ZB = TEXT("");
        if (!InProperty.Zone.IsEmpty() && !InProperty.Block.IsEmpty())
        {
            ZB = FString::Printf(TEXT("%s - %s"), *InProperty.Zone, *InProperty.Block);
        }
        else if (!InProperty.Zone.IsEmpty())
        {
            ZB = InProperty.Zone;
        }
        else
        {
            ZB = InProperty.Block;
        }
        ZoneBlockText->SetText(FText::FromString(ZB));
    }
    if (DoorNoText)
    {
        DoorNoText->SetText(FText::FromString(FString::Printf(TEXT("Door %d"), InProperty.DoorNo)));
    }
    if (PriceText)
    {
        PriceText->SetText(FText::FromString(FormatPrice(InProperty.Price)));
    }
    if (SurfaceText)
    {
        SurfaceText->SetText(FText::FromString(FString::Printf(TEXT("%.0f m²"), InProperty.Surface)));
    }
    if (BuildingSurfaceText)
    {
        BuildingSurfaceText->SetText(FText::FromString(FString::Printf(TEXT("%.0f m²"), InProperty.BuildingSurface)));
    }
    if (BedroomsText)
    {
        BedroomsText->SetText(FText::FromString(FString::Printf(TEXT("%d %s"), InProperty.BedroomsCount, *RoomType)));
    }
    if (BathroomsText)
    {
        BathroomsText->SetText(FText::FromString(FString::Printf(TEXT("%d Baths"), InProperty.BathroomsCount)));
    }
    if (AvailabilityText)
    {
        AvailabilityText->SetText(FText::FromString(InProperty.Availability));
    }
    if (ClassText)
    {
        ClassText->SetText(FText::FromString(InProperty.Class));
    }

    if (CardButton)
    {
        CardButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchResultWidget::OnCardClicked);
    }
    if (LocateButton)
    {
        LocateButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchResultWidget::OnLocateClicked);
    }
    if (ExploreButton)
    {
        ExploreButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchResultWidget::OnExploreClicked);
    }
}

void USightPortalUnitSearchResultWidget::SetCurrency(const FSightPortalCurrencySetting& InCurrency)
{
    ActiveCurrency = InCurrency;
    if (PriceText)
    {
        PriceText->SetText(FText::FromString(FormatPrice(PropertyData.Price)));
    }
}

FString USightPortalUnitSearchResultWidget::FormatPrice(float InBasePrice) const
{
    const float Rate = ActiveCurrency.ExchangeRate > 0.0f ? ActiveCurrency.ExchangeRate : 1.0f;
    const double ConvertedPrice = (double)InBasePrice * (double)Rate;
    const FString Symbol = ActiveCurrency.CurrencySymbol.IsEmpty() ? TEXT("د.ع") : ActiveCurrency.CurrencySymbol;

    FString FormattedNumber;
    if (ActiveCurrency.bAbbreviateLargeNumbers)
    {
        if (ConvertedPrice >= 1000000.0)
        {
            FormattedNumber = FString::Printf(TEXT("%.2fM"), ConvertedPrice / 1000000.0);
        }
        else if (ConvertedPrice >= 1000.0)
        {
            FormattedNumber = FString::Printf(TEXT("%.0fK"), ConvertedPrice / 1000.0);
        }
        else
        {
            FormattedNumber = FString::Printf(TEXT("%.*f"), ActiveCurrency.DecimalPlaces, ConvertedPrice);
        }
    }
    else
    {
        // Default format: 0000000000.00 (two decimal places)
        if (ActiveCurrency.bUseThousandSeparators)
        {
            FNumberFormattingOptions Options;
            Options.SetUseGrouping(true);
            Options.SetMinimumFractionalDigits(ActiveCurrency.DecimalPlaces);
            Options.SetMaximumFractionalDigits(ActiveCurrency.DecimalPlaces);
            if (ActiveCurrency.MinimumIntegerDigits > 1)
            {
                Options.SetMinimumIntegralDigits(ActiveCurrency.MinimumIntegerDigits);
            }
            FormattedNumber = FText::AsNumber(ConvertedPrice, &Options).ToString();
        }
        else
        {
            if (ActiveCurrency.MinimumIntegerDigits > 1)
            {
                const int64 IntPart = (int64)ConvertedPrice;
                const double FracPart = FMath::Abs(ConvertedPrice - (double)IntPart);
                const FString IntStr = FString::Printf(TEXT("%0*lld"), ActiveCurrency.MinimumIntegerDigits, IntPart);
                const FString FracStr = FString::Printf(TEXT("%.*f"), ActiveCurrency.DecimalPlaces, FracPart);
                int32 DotIndex = INDEX_NONE;
                if (FracStr.FindChar(TEXT('.'), DotIndex))
                {
                    FormattedNumber = FString::Printf(TEXT("%s.%s"), *IntStr, *FracStr.Mid(DotIndex + 1));
                }
                else
                {
                    FormattedNumber = IntStr;
                }
            }
            else
            {
                FormattedNumber = FString::Printf(TEXT("%.*f"), ActiveCurrency.DecimalPlaces, ConvertedPrice);
            }
        }
    }

    if (ActiveCurrency.bSymbolPrefix)
    {
        return FString::Printf(TEXT("%s%s"), *Symbol, *FormattedNumber);
    }
    else
    {
        return FString::Printf(TEXT("%s %s"), *FormattedNumber, *Symbol);
    }
}

void USightPortalUnitSearchResultWidget::OnCardClicked()
{
    if (ParentSearchWidget)
    {
        ParentSearchWidget->HandleUnitSelected(PropertyData, AssociatedVisualizer.Get());
    }
}

void USightPortalUnitSearchResultWidget::OnLocateClicked()
{
    if (ParentSearchWidget)
    {
        ParentSearchWidget->HandleUnitSelected(PropertyData, AssociatedVisualizer.Get());
    }
}

void USightPortalUnitSearchResultWidget::OnExploreClicked()
{
    if (ParentSearchWidget)
    {
        ParentSearchWidget->HandleUnitExplore(PropertyData, AssociatedVisualizer.Get());
    }
}

// ----------------------------------------------------------------------------------
// USightPortalUnitSearchWidget Implementation
// ----------------------------------------------------------------------------------

USightPortalUnitSearchWidget::USightPortalUnitSearchWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , ResultCardWidgetClass(USightPortalUnitSearchResultWidget::StaticClass())
    , DetailWidgetClass(nullptr)
    , RoomType(TEXT("Bedrooms"))
    , DefaultCurrencyCode(TEXT("IQD"))
    , bAutoFilterOnChange(true)
    , bCloseOnUnitSelected(false)
    , bAutoDiscoverLevelVisualizers(true)
    , bEnableSceneIsolation(false)
    , bHideNonMatchingUnits(false)
    , SearchInputBox(nullptr)
    , ClearSearchButton(nullptr)
    , ZoneComboBox(nullptr)
    , BlockComboBox(nullptr)
    , ClassComboBox(nullptr)
    , AvailabilityComboBox(nullptr)
    , BedroomsComboBox(nullptr)
    , BathroomsComboBox(nullptr)
    , SortComboBox(nullptr)
    , CurrencyComboBox(nullptr)
    , MinPriceSlider(nullptr)
    , MaxPriceSlider(nullptr)
    , MinPriceInputBox(nullptr)
    , MaxPriceInputBox(nullptr)
    , MinSurfaceSlider(nullptr)
    , MaxSurfaceSlider(nullptr)
    , MinSurfaceInputBox(nullptr)
    , MaxSurfaceInputBox(nullptr)
    , ResetFiltersButton(nullptr)
    , ApplyFiltersButton(nullptr)
    , CloseButton(nullptr)
    , SceneIsolationCheckBox(nullptr)
    , SceneIsolationButton(nullptr)
    , SceneIsolationText(nullptr)
    , ResultsScrollBox(nullptr)
    , ResultsContainer(nullptr)
    , ResultCountText(nullptr)
    , NoResultsText(nullptr)
    , EmptyStateWidget(nullptr)
    , MaxPriceFoundInDataset(1000000000.0f)
    , MaxSurfaceFoundInDataset(1000.0f)
    , bIsPopulatingDropdowns(false)
{
    // Portal prices are in Iraqi Dinars (IQD) as the base currency
    Currencies.Add(FSightPortalCurrencySetting(TEXT("IQD"), TEXT("د.ع"), 1.0f, TEXT("IQD (د.ع)"), false, 2, false, false));
    // Converted currencies relative to 1 Iraqi Dinar (e.g. 1 USD ≈ 1,310 IQD)
    Currencies.Add(FSightPortalCurrencySetting(TEXT("USD"), TEXT("$"), 1.0f / 1310.0f, TEXT("USD ($)"), true, 2, false, false));
    Currencies.Add(FSightPortalCurrencySetting(TEXT("EUR"), TEXT("€"), 1.0f / 1420.0f, TEXT("EUR (€)"), true, 2, false, false));
    Currencies.Add(FSightPortalCurrencySetting(TEXT("GBP"), TEXT("£"), 1.0f / 1670.0f, TEXT("GBP (£)"), true, 2, false, false));
    Currencies.Add(FSightPortalCurrencySetting(TEXT("AED"), TEXT("AED"), 1.0f / 356.7f, TEXT("AED (AED)"), false, 2, false, false));
    Currencies.Add(FSightPortalCurrencySetting(TEXT("SAR"), TEXT("SAR"), 1.0f / 349.3f, TEXT("SAR (SAR)"), false, 2, false, false));
    Currencies.Add(FSightPortalCurrencySetting(TEXT("TRY"), TEXT("₺"), 1.0f / 38.5f, TEXT("TRY (₺)"), true, 2, false, false));
    Currencies.Add(FSightPortalCurrencySetting(TEXT("JPY"), TEXT("¥"), 1.0f / 8.5f, TEXT("JPY (¥)"), true, 2, false, false));

    if (Currencies.Num() > 0)
    {
        ActiveCurrency = Currencies[0];
    }
}

void USightPortalUnitSearchWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ResolveUnboundWidgets();

    // Hook search text box
    if (SearchInputBox)
    {
        SearchInputBox->OnTextChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnSearchTextChanged);
        SearchInputBox->OnTextCommitted.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnSearchTextCommitted);
    }
    if (ClearSearchButton)
    {
        ClearSearchButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnClearSearchClicked);
    }

    // Hook combobox filters
    if (ZoneComboBox)
    {
        ZoneComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnZoneSelectionChanged);
    }
    if (BlockComboBox)
    {
        BlockComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnBlockSelectionChanged);
    }
    if (ClassComboBox)
    {
        ClassComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnClassSelectionChanged);
    }
    if (AvailabilityComboBox)
    {
        AvailabilityComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnAvailabilitySelectionChanged);
    }
    if (BedroomsComboBox)
    {
        BedroomsComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnBedroomsSelectionChanged);
    }
    if (BathroomsComboBox)
    {
        BathroomsComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnBathroomsSelectionChanged);
    }
    if (SortComboBox)
    {
        SortComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnSortSelectionChanged);
    }
    if (CurrencyComboBox)
    {
        CurrencyComboBox->OnSelectionChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnCurrencySelectionChanged);
    }

    // Initialize currency selector dropdown
    PopulateCurrencyDropdown();

    // Hook sliders and text boxes
    if (MinPriceSlider)
    {
        MinPriceSlider->OnValueChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMinPriceSliderChanged);
    }
    if (MaxPriceSlider)
    {
        MaxPriceSlider->OnValueChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMaxPriceSliderChanged);
    }
    if (MinPriceInputBox)
    {
        MinPriceInputBox->OnTextCommitted.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMinPriceTextCommitted);
    }
    if (MaxPriceInputBox)
    {
        MaxPriceInputBox->OnTextCommitted.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMaxPriceTextCommitted);
    }
    if (MinSurfaceSlider)
    {
        MinSurfaceSlider->OnValueChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMinSurfaceSliderChanged);
    }
    if (MaxSurfaceSlider)
    {
        MaxSurfaceSlider->OnValueChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMaxSurfaceSliderChanged);
    }
    if (MinSurfaceInputBox)
    {
        MinSurfaceInputBox->OnTextCommitted.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMinSurfaceTextCommitted);
    }
    if (MaxSurfaceInputBox)
    {
        MaxSurfaceInputBox->OnTextCommitted.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnMaxSurfaceTextCommitted);
    }

    // Hook action buttons
    if (ResetFiltersButton)
    {
        ResetFiltersButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnResetClicked);
    }
    if (ApplyFiltersButton)
    {
        ApplyFiltersButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnApplyClicked);
    }
    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnCloseClicked);
    }

    // Hook scene isolation widgets
    if (SceneIsolationCheckBox)
    {
        SceneIsolationCheckBox->SetIsChecked(bEnableSceneIsolation);
        SceneIsolationCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnSceneIsolationCheckChanged);
    }
    if (SceneIsolationButton)
    {
        SceneIsolationButton->OnClicked.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::OnSceneIsolationButtonClicked);
    }

    // Subscribe to connector real-time events
    if (GEngine)
    {
        if (USightPortalConnector* Connector = GEngine->GetEngineSubsystem<USightPortalConnector>())
        {
            Connector->OnRealEstateDataReceived.AddUniqueDynamic(this, &USightPortalUnitSearchWidget::HandleDataReceivedFromConnector);
        }
    }

    // Auto-populate data
    InitializeData();
}

void USightPortalUnitSearchWidget::NativeDestruct()
{
    ClearSceneIsolation();

    if (GEngine)
    {
        if (USightPortalConnector* Connector = GEngine->GetEngineSubsystem<USightPortalConnector>())
        {
            Connector->OnRealEstateDataReceived.RemoveDynamic(this, &USightPortalUnitSearchWidget::HandleDataReceivedFromConnector);
        }
    }

    Super::NativeDestruct();
}

void USightPortalUnitSearchWidget::ResolveUnboundWidgets()
{
    auto FindTextBoxWithAliases = [this](const TArray<FString>& Aliases) -> UEditableTextBox*
    {
        for (const FString& Name : Aliases)
        {
            if (UEditableTextBox* Found = Cast<UEditableTextBox>(GetWidgetFromName(FName(*Name))))
            {
                return Found;
            }
        }
        return nullptr;
    };

    auto FindComboBoxWithAliases = [this](const TArray<FString>& Aliases) -> UComboBoxString*
    {
        for (const FString& Name : Aliases)
        {
            if (UComboBoxString* Found = Cast<UComboBoxString>(GetWidgetFromName(FName(*Name))))
            {
                return Found;
            }
        }
        return nullptr;
    };

    auto FindSliderWithAliases = [this](const TArray<FString>& Aliases) -> USlider*
    {
        for (const FString& Name : Aliases)
        {
            if (USlider* Found = Cast<USlider>(GetWidgetFromName(FName(*Name))))
            {
                return Found;
            }
        }
        return nullptr;
    };

    auto FindButtonWithAliases = [this](const TArray<FString>& Aliases) -> UButton*
    {
        for (const FString& Name : Aliases)
        {
            if (UButton* Found = Cast<UButton>(GetWidgetFromName(FName(*Name))))
            {
                return Found;
            }
        }
        return nullptr;
    };

    auto FindTextBlockWithAliases = [this](const TArray<FString>& Aliases) -> UTextBlock*
    {
        for (const FString& Name : Aliases)
        {
            if (UTextBlock* Found = Cast<UTextBlock>(GetWidgetFromName(FName(*Name))))
            {
                return Found;
            }
        }
        return nullptr;
    };

    if (!SearchInputBox)
    {
        SearchInputBox = FindTextBoxWithAliases({ TEXT("SearchInputBox"), TEXT("SearchBox"), TEXT("Txt_Search"), TEXT("EditableTextBox_Search"), TEXT("InputSearch") });
    }
    if (!ClearSearchButton)
    {
        ClearSearchButton = FindButtonWithAliases({ TEXT("ClearSearchButton"), TEXT("Btn_ClearSearch"), TEXT("ClearSearch"), TEXT("ResetSearchButton") });
    }

    if (!ZoneComboBox)
    {
        ZoneComboBox = FindComboBoxWithAliases({ TEXT("ZoneComboBox"), TEXT("ComboBox_Zone"), TEXT("ZoneFilter"), TEXT("FilterZone") });
    }
    if (!BlockComboBox)
    {
        BlockComboBox = FindComboBoxWithAliases({ TEXT("BlockComboBox"), TEXT("ComboBox_Block"), TEXT("BlockFilter"), TEXT("FilterBlock") });
    }
    if (!ClassComboBox)
    {
        ClassComboBox = FindComboBoxWithAliases({ TEXT("ClassComboBox"), TEXT("ComboBox_Class"), TEXT("TypeComboBox"), TEXT("ClassFilter"), TEXT("PropertyTypeFilter") });
    }
    if (!AvailabilityComboBox)
    {
        AvailabilityComboBox = FindComboBoxWithAliases({ TEXT("AvailabilityComboBox"), TEXT("ComboBox_Availability"), TEXT("StatusFilter"), TEXT("FilterAvailability") });
    }
    if (!BedroomsComboBox)
    {
        BedroomsComboBox = FindComboBoxWithAliases({ TEXT("BedroomsComboBox"), TEXT("ComboBox_Bedrooms"), TEXT("BedsFilter"), TEXT("FilterBedrooms") });
    }
    if (!BathroomsComboBox)
    {
        BathroomsComboBox = FindComboBoxWithAliases({ TEXT("BathroomsComboBox"), TEXT("ComboBox_Bathrooms"), TEXT("BathsFilter"), TEXT("FilterBathrooms") });
    }
    if (!SortComboBox)
    {
        SortComboBox = FindComboBoxWithAliases({ TEXT("SortComboBox"), TEXT("ComboBox_Sort"), TEXT("SortBy"), TEXT("FilterSort") });
    }
    if (!CurrencyComboBox)
    {
        CurrencyComboBox = FindComboBoxWithAliases({ TEXT("CurrencyComboBox"), TEXT("ComboBox_Currency"), TEXT("CurrencyDropdown"), TEXT("Dropdown_Currency"), TEXT("SelectCurrency"), TEXT("CurrencyFilter") });
    }

    if (!MinPriceSlider)
    {
        MinPriceSlider = FindSliderWithAliases({ TEXT("MinPriceSlider"), TEXT("Slider_MinPrice"), TEXT("PriceMinSlider") });
    }
    if (!MaxPriceSlider)
    {
        MaxPriceSlider = FindSliderWithAliases({ TEXT("MaxPriceSlider"), TEXT("Slider_MaxPrice"), TEXT("PriceMaxSlider") });
    }
    if (!MinPriceInputBox)
    {
        MinPriceInputBox = FindTextBoxWithAliases({ TEXT("MinPriceInputBox"), TEXT("Txt_MinPrice"), TEXT("InputMinPrice") });
    }
    if (!MaxPriceInputBox)
    {
        MaxPriceInputBox = FindTextBoxWithAliases({ TEXT("MaxPriceInputBox"), TEXT("Txt_MaxPrice"), TEXT("InputMaxPrice") });
    }

    if (!MinSurfaceSlider)
    {
        MinSurfaceSlider = FindSliderWithAliases({ TEXT("MinSurfaceSlider"), TEXT("Slider_MinSurface"), TEXT("SurfaceMinSlider") });
    }
    if (!MaxSurfaceSlider)
    {
        MaxSurfaceSlider = FindSliderWithAliases({ TEXT("MaxSurfaceSlider"), TEXT("Slider_MaxSurface"), TEXT("SurfaceMaxSlider") });
    }
    if (!MinSurfaceInputBox)
    {
        MinSurfaceInputBox = FindTextBoxWithAliases({ TEXT("MinSurfaceInputBox"), TEXT("Txt_MinSurface"), TEXT("InputMinSurface") });
    }
    if (!MaxSurfaceInputBox)
    {
        MaxSurfaceInputBox = FindTextBoxWithAliases({ TEXT("MaxSurfaceInputBox"), TEXT("Txt_MaxSurface"), TEXT("InputMaxSurface") });
    }

    if (!ResetFiltersButton)
    {
        ResetFiltersButton = FindButtonWithAliases({ TEXT("ResetFiltersButton"), TEXT("Btn_Reset"), TEXT("ResetButton"), TEXT("ClearFiltersButton"), TEXT("Btn_Clear") });
    }
    if (!ApplyFiltersButton)
    {
        ApplyFiltersButton = FindButtonWithAliases({ TEXT("ApplyFiltersButton"), TEXT("Btn_Apply"), TEXT("ApplyButton"), TEXT("FilterButton") });
    }
    if (!CloseButton)
    {
        CloseButton = FindButtonWithAliases({ TEXT("CloseButton"), TEXT("Btn_Close"), TEXT("DismissButton"), TEXT("ExitButton"), TEXT("BackButton") });
    }

    if (!SceneIsolationCheckBox)
    {
        auto FindCheckBoxWithAliases = [this](const TArray<FString>& Aliases) -> UCheckBox*
        {
            for (const FString& Name : Aliases)
            {
                if (UCheckBox* Found = Cast<UCheckBox>(GetWidgetFromName(FName(*Name))))
                {
                    return Found;
                }
            }
            return nullptr;
        };
        SceneIsolationCheckBox = FindCheckBoxWithAliases({ TEXT("SceneIsolationCheckBox"), TEXT("CheckBox_SceneIsolation"), TEXT("CheckBox_Isolate"), TEXT("Toggle_Isolate"), TEXT("SceneIsolateCheckBox"), TEXT("IsolateCheckBox") });
    }
    if (!SceneIsolationButton)
    {
        SceneIsolationButton = FindButtonWithAliases({ TEXT("SceneIsolationButton"), TEXT("Btn_SceneIsolation"), TEXT("Btn_Isolate"), TEXT("IsolateButton"), TEXT("SceneIsolateButton") });
    }
    if (!SceneIsolationText)
    {
        SceneIsolationText = FindTextBlockWithAliases({ TEXT("SceneIsolationText"), TEXT("Txt_SceneIsolation"), TEXT("Txt_Isolate"), TEXT("IsolateLabel"), TEXT("SceneIsolationLabel") });
    }

    if (!ResultsScrollBox)
    {
        ResultsScrollBox = Cast<UScrollBox>(GetWidgetFromName(FName(TEXT("ResultsScrollBox"))));
        if (!ResultsScrollBox) ResultsScrollBox = Cast<UScrollBox>(GetWidgetFromName(FName(TEXT("ScrollBox_Results"))));
        if (!ResultsScrollBox) ResultsScrollBox = Cast<UScrollBox>(GetWidgetFromName(FName(TEXT("ResultsScroll"))));
    }
    if (!ResultsContainer)
    {
        ResultsContainer = Cast<UPanelWidget>(GetWidgetFromName(FName(TEXT("ResultsContainer"))));
        if (!ResultsContainer) ResultsContainer = Cast<UPanelWidget>(GetWidgetFromName(FName(TEXT("ResultsBox"))));
        if (!ResultsContainer) ResultsContainer = Cast<UPanelWidget>(GetWidgetFromName(FName(TEXT("VerticalBox_Results"))));
    }

    if (!ResultCountText)
    {
        ResultCountText = FindTextBlockWithAliases({ TEXT("ResultCountText"), TEXT("Txt_Count"), TEXT("CountText"), TEXT("TotalResultsText"), TEXT("PropertiesFoundText") });
    }
    if (!NoResultsText)
    {
        NoResultsText = FindTextBlockWithAliases({ TEXT("NoResultsText"), TEXT("Txt_NoResults"), TEXT("EmptyText"), TEXT("NotFoundText") });
    }
    if (!EmptyStateWidget)
    {
        EmptyStateWidget = GetWidgetFromName(FName(TEXT("EmptyStateWidget")));
        if (!EmptyStateWidget) EmptyStateWidget = GetWidgetFromName(FName(TEXT("EmptyState")));
    }
}

void USightPortalUnitSearchWidget::InitializeData()
{
    AllProperties.Empty();
    VisualizerMap.Empty();

    // 1. Gather all APropertyVisualizer actors registered in ASightPortalSiteManager instances in the world
    if (GetWorld())
    {
        TArray<AActor*> FoundSiteManagers;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASightPortalSiteManager::StaticClass(), FoundSiteManagers);
        for (AActor* SM : FoundSiteManagers)
        {
            if (ASightPortalSiteManager* SiteManager = Cast<ASightPortalSiteManager>(SM))
            {
                for (const auto& Pair : SiteManager->RegisteredPropertyVisualizers)
                {
                    if (APropertyVisualizer* PV = Cast<APropertyVisualizer>(Pair.Value))
                    {
                        if (!Pair.Key.IsEmpty())
                        {
                            VisualizerMap.Add(Pair.Key, PV);
                        }
                        if (!PV->PropertyDetails.Name.IsEmpty())
                        {
                            VisualizerMap.Add(PV->PropertyDetails.Name, PV);
                        }
                    }
                }
            }
        }
    }

    // 2. Discover all level APropertyVisualizer actors
    if (bAutoDiscoverLevelVisualizers && GetWorld())
    {
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APropertyVisualizer::StaticClass(), FoundActors);

        for (AActor* Actor : FoundActors)
        {
            if (APropertyVisualizer* PV = Cast<APropertyVisualizer>(Actor))
            {
                if (!PV->PropertyDetails.Name.IsEmpty())
                {
                    if (!VisualizerMap.Contains(PV->PropertyDetails.Name))
                    {
                        VisualizerMap.Add(PV->PropertyDetails.Name, PV);
                    }
                }
            }
        }
    }

    // 3. Ingest properties directly from USightPortalConnector cached portfolio (live cloud / spreadsheet dataset)
    bool bLoadedFromConnector = false;
    if (GEngine)
    {
        if (USightPortalConnector* Connector = GEngine->GetEngineSubsystem<USightPortalConnector>())
        {
            if (Connector->CachedProperties.Num() > 0)
            {
                AllProperties = Connector->CachedProperties;
                bLoadedFromConnector = true;
            }
        }
    }

    // If connector cached portfolio is not yet populated, populate from discovered visualizers
    if (!bLoadedFromConnector || AllProperties.Num() == 0)
    {
        TSet<FString> AddedNames;
        for (const auto& Pair : VisualizerMap)
        {
            if (Pair.Value && !AddedNames.Contains(Pair.Value->PropertyDetails.Name) && !Pair.Value->PropertyDetails.Name.IsEmpty())
            {
                AllProperties.Add(Pair.Value->PropertyDetails);
                AddedNames.Add(Pair.Value->PropertyDetails.Name);
            }
        }
    }
    else
    {
        // Ensure any visualizers in the level that might not be in connector cached portfolio are also merged
        TSet<FString> ExistingNames;
        for (const FSightPortalProperty& Prop : AllProperties)
        {
            ExistingNames.Add(Prop.Name);
        }
        for (const auto& Pair : VisualizerMap)
        {
            if (Pair.Value && !Pair.Value->PropertyDetails.Name.IsEmpty() && !ExistingNames.Contains(Pair.Value->PropertyDetails.Name))
            {
                AllProperties.Add(Pair.Value->PropertyDetails);
                ExistingNames.Add(Pair.Value->PropertyDetails.Name);
            }
        }
    }

    // Determine dataset limits
    float MaxPrice = 0.0f;
    float MaxSurf = 0.0f;
    for (const FSightPortalProperty& Prop : AllProperties)
    {
        if (Prop.Price > MaxPrice) MaxPrice = Prop.Price;
        if (Prop.Surface > MaxSurf) MaxSurf = Prop.Surface;
    }
    MaxPriceFoundInDataset = MaxPrice > 0.0f ? MaxPrice : 10000000.0f;
    MaxSurfaceFoundInDataset = MaxSurf > 0.0f ? MaxSurf : 1000.0f;

    PopulateFilterDropdowns();
    ApplyFilters();
}

void USightPortalUnitSearchWidget::SetPropertyList(const TArray<FSightPortalProperty>& InProperties)
{
    AllProperties = InProperties;

    // Scan site managers and level to update visualizer map
    VisualizerMap.Empty();
    if (GetWorld())
    {
        TArray<AActor*> FoundSiteManagers;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASightPortalSiteManager::StaticClass(), FoundSiteManagers);
        for (AActor* SM : FoundSiteManagers)
        {
            if (ASightPortalSiteManager* SiteManager = Cast<ASightPortalSiteManager>(SM))
            {
                for (const auto& Pair : SiteManager->RegisteredPropertyVisualizers)
                {
                    if (APropertyVisualizer* PV = Cast<APropertyVisualizer>(Pair.Value))
                    {
                        if (!Pair.Key.IsEmpty())
                        {
                            VisualizerMap.Add(Pair.Key, PV);
                        }
                    }
                }
            }
        }

        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APropertyVisualizer::StaticClass(), FoundActors);
        for (AActor* Actor : FoundActors)
        {
            if (APropertyVisualizer* PV = Cast<APropertyVisualizer>(Actor))
            {
                if (!PV->PropertyDetails.Name.IsEmpty() && !VisualizerMap.Contains(PV->PropertyDetails.Name))
                {
                    VisualizerMap.Add(PV->PropertyDetails.Name, PV);
                }
            }
        }
    }

    PopulateFilterDropdowns();
    ApplyFilters();
}

void USightPortalUnitSearchWidget::PopulateFilterDropdowns()
{
    bIsPopulatingDropdowns = true;

    TSet<FString> UniqueZones;
    TSet<FString> UniqueBlocks;
    TSet<FString> UniqueClasses;
    TSet<FString> UniqueAvailabilities;

    for (const FSightPortalProperty& Prop : AllProperties)
    {
        if (!Prop.Zone.IsEmpty()) UniqueZones.Add(Prop.Zone);
        if (!Prop.Block.IsEmpty()) UniqueBlocks.Add(Prop.Block);
        if (!Prop.Class.IsEmpty()) UniqueClasses.Add(Prop.Class);
        if (!Prop.Availability.IsEmpty()) UniqueAvailabilities.Add(Prop.Availability);
    }

    if (ZoneComboBox)
    {
        ZoneComboBox->ClearOptions();
        ZoneComboBox->AddOption(TEXT("All"));
        TArray<FString> SortedZones = UniqueZones.Array();
        SortedZones.Sort();
        for (const FString& Z : SortedZones)
        {
            ZoneComboBox->AddOption(Z);
        }
        ZoneComboBox->SetSelectedOption(FilterCriteria.SelectedZone);
    }

    if (BlockComboBox)
    {
        BlockComboBox->ClearOptions();
        BlockComboBox->AddOption(TEXT("All"));
        TArray<FString> SortedBlocks = UniqueBlocks.Array();
        SortedBlocks.Sort();
        for (const FString& B : SortedBlocks)
        {
            BlockComboBox->AddOption(B);
        }
        BlockComboBox->SetSelectedOption(FilterCriteria.SelectedBlock);
    }

    if (ClassComboBox)
    {
        ClassComboBox->ClearOptions();
        ClassComboBox->AddOption(TEXT("All"));
        TArray<FString> SortedClasses = UniqueClasses.Array();
        SortedClasses.Sort();
        for (const FString& C : SortedClasses)
        {
            ClassComboBox->AddOption(C);
        }
        ClassComboBox->SetSelectedOption(FilterCriteria.SelectedClass);
    }

    if (AvailabilityComboBox)
    {
        AvailabilityComboBox->ClearOptions();
        AvailabilityComboBox->AddOption(TEXT("All"));
        TArray<FString> SortedAvail = UniqueAvailabilities.Array();
        SortedAvail.Sort();
        for (const FString& A : SortedAvail)
        {
            AvailabilityComboBox->AddOption(A);
        }
        AvailabilityComboBox->SetSelectedOption(FilterCriteria.SelectedAvailability);
    }

    if (BedroomsComboBox)
    {
        BedroomsComboBox->ClearOptions();
        BedroomsComboBox->AddOption(TEXT("Any"));
        BedroomsComboBox->AddOption(FString::Printf(TEXT("1+ %s"), *RoomType));
        BedroomsComboBox->AddOption(FString::Printf(TEXT("2+ %s"), *RoomType));
        BedroomsComboBox->AddOption(FString::Printf(TEXT("3+ %s"), *RoomType));
        BedroomsComboBox->AddOption(FString::Printf(TEXT("4+ %s"), *RoomType));
        BedroomsComboBox->AddOption(FString::Printf(TEXT("5+ %s"), *RoomType));
        if (FilterCriteria.MinBedrooms > 0)
        {
            BedroomsComboBox->SetSelectedOption(FString::Printf(TEXT("%d+ %s"), FilterCriteria.MinBedrooms, *RoomType));
        }
        else
        {
            BedroomsComboBox->SetSelectedOption(TEXT("Any"));
        }
    }

    if (BathroomsComboBox)
    {
        BathroomsComboBox->ClearOptions();
        BathroomsComboBox->AddOption(TEXT("Any"));
        BathroomsComboBox->AddOption(TEXT("1+ Baths"));
        BathroomsComboBox->AddOption(TEXT("2+ Baths"));
        BathroomsComboBox->AddOption(TEXT("3+ Baths"));
        BathroomsComboBox->AddOption(TEXT("4+ Baths"));
        if (FilterCriteria.MinBathrooms > 0)
        {
            BathroomsComboBox->SetSelectedOption(FString::Printf(TEXT("%d+ Baths"), FilterCriteria.MinBathrooms));
        }
        else
        {
            BathroomsComboBox->SetSelectedOption(TEXT("Any"));
        }
    }

    if (SortComboBox)
    {
        SortComboBox->ClearOptions();
        SortComboBox->AddOption(TEXT("Default"));
        SortComboBox->AddOption(TEXT("Price: Low to High"));
        SortComboBox->AddOption(TEXT("Price: High to Low"));
        SortComboBox->AddOption(TEXT("Surface: Small to Large"));
        SortComboBox->AddOption(TEXT("Surface: Large to Small"));
        SortComboBox->AddOption(TEXT("Bedrooms: Low to High"));
        SortComboBox->AddOption(TEXT("Bedrooms: High to Low"));
        SortComboBox->AddOption(TEXT("Name: A to Z"));
        SortComboBox->AddOption(TEXT("Name: Z to A"));
        SortComboBox->SetSelectedIndex((int32)FilterCriteria.SortMode);
    }

    PopulateCurrencyDropdown();

    bIsPopulatingDropdowns = false;
}

void USightPortalUnitSearchWidget::PopulateCurrencyDropdown()
{
    if (!CurrencyComboBox) return;

    CurrencyComboBox->ClearOptions();

    if (Currencies.Num() == 0)
    {
        Currencies.Add(FSightPortalCurrencySetting(TEXT("IQD"), TEXT("د.ع"), 1.0f, TEXT("IQD (د.ع)"), false, 2, false, false));
    }

    int32 SelectedIndex = 0;
    for (int32 i = 0; i < Currencies.Num(); ++i)
    {
        const FSightPortalCurrencySetting& Cur = Currencies[i];
        const FString Label = Cur.DisplayLabel.IsEmpty() ? Cur.CurrencyCode : Cur.DisplayLabel;
        CurrencyComboBox->AddOption(Label);

        if (!DefaultCurrencyCode.IsEmpty() && Cur.CurrencyCode.Equals(DefaultCurrencyCode, ESearchCase::IgnoreCase))
        {
            SelectedIndex = i;
        }
        else if (Cur.CurrencyCode.Equals(ActiveCurrency.CurrencyCode, ESearchCase::IgnoreCase))
        {
            SelectedIndex = i;
        }
    }

    if (Currencies.IsValidIndex(SelectedIndex))
    {
        ActiveCurrency = Currencies[SelectedIndex];
        CurrencyComboBox->SetSelectedIndex(SelectedIndex);
    }
}

void USightPortalUnitSearchWidget::UpdateResultCardCurrencies()
{
    UPanelWidget* TargetPanel = ResultsScrollBox ? Cast<UPanelWidget>(ResultsScrollBox) : ResultsContainer;
    if (!TargetPanel) return;

    const int32 NumChildren = TargetPanel->GetChildrenCount();
    for (int32 i = 0; i < NumChildren; ++i)
    {
        if (USightPortalUnitSearchResultWidget* Card = Cast<USightPortalUnitSearchResultWidget>(TargetPanel->GetChildAt(i)))
        {
            Card->SetCurrency(ActiveCurrency);
        }
    }
}

void USightPortalUnitSearchWidget::SetActiveCurrencyByCode(const FString& InCurrencyCode)
{
    for (int32 i = 0; i < Currencies.Num(); ++i)
    {
        if (Currencies[i].CurrencyCode.Equals(InCurrencyCode, ESearchCase::IgnoreCase))
        {
            ActiveCurrency = Currencies[i];
            if (CurrencyComboBox)
            {
                CurrencyComboBox->SetSelectedIndex(i);
            }
            UpdateResultCardCurrencies();
            return;
        }
    }
}

void USightPortalUnitSearchWidget::AddOrUpdateCurrency(const FSightPortalCurrencySetting& InCurrency)
{
    for (int32 i = 0; i < Currencies.Num(); ++i)
    {
        if (Currencies[i].CurrencyCode.Equals(InCurrency.CurrencyCode, ESearchCase::IgnoreCase))
        {
            Currencies[i] = InCurrency;
            PopulateCurrencyDropdown();
            if (ActiveCurrency.CurrencyCode.Equals(InCurrency.CurrencyCode, ESearchCase::IgnoreCase))
            {
                ActiveCurrency = InCurrency;
                UpdateResultCardCurrencies();
            }
            return;
        }
    }

    Currencies.Add(InCurrency);
    PopulateCurrencyDropdown();
}

bool USightPortalUnitSearchWidget::MatchesFilterCriteria(const FSightPortalProperty& Prop) const
{
    // Keyword search
    if (!FilterCriteria.SearchKeyword.IsEmpty())
    {
        const FString LowerKeyword = FilterCriteria.SearchKeyword.ToLower();
        const bool bMatchesName = Prop.Name.ToLower().Contains(LowerKeyword);
        const bool bMatchesZone = Prop.Zone.ToLower().Contains(LowerKeyword);
        const bool bMatchesBlock = Prop.Block.ToLower().Contains(LowerKeyword);
        const bool bMatchesClass = Prop.Class.ToLower().Contains(LowerKeyword);
        const bool bMatchesDoor = FString::FromInt(Prop.DoorNo).Contains(LowerKeyword);

        if (!bMatchesName && !bMatchesZone && !bMatchesBlock && !bMatchesClass && !bMatchesDoor)
        {
            return false;
        }
    }

    // Zone filter
    if (!FilterCriteria.SelectedZone.IsEmpty() && FilterCriteria.SelectedZone != TEXT("All"))
    {
        if (!Prop.Zone.Equals(FilterCriteria.SelectedZone, ESearchCase::IgnoreCase))
        {
            return false;
        }
    }

    // Block filter
    if (!FilterCriteria.SelectedBlock.IsEmpty() && FilterCriteria.SelectedBlock != TEXT("All"))
    {
        if (!Prop.Block.Equals(FilterCriteria.SelectedBlock, ESearchCase::IgnoreCase))
        {
            return false;
        }
    }

    // Class filter
    if (!FilterCriteria.SelectedClass.IsEmpty() && FilterCriteria.SelectedClass != TEXT("All"))
    {
        if (!Prop.Class.Equals(FilterCriteria.SelectedClass, ESearchCase::IgnoreCase))
        {
            return false;
        }
    }

    // Availability filter
    if (!FilterCriteria.SelectedAvailability.IsEmpty() && FilterCriteria.SelectedAvailability != TEXT("All"))
    {
        if (!Prop.Availability.Equals(FilterCriteria.SelectedAvailability, ESearchCase::IgnoreCase))
        {
            return false;
        }
    }

    // Bedrooms filter
    if (FilterCriteria.MinBedrooms > 0 && Prop.BedroomsCount < FilterCriteria.MinBedrooms)
    {
        return false;
    }

    // Bathrooms filter
    if (FilterCriteria.MinBathrooms > 0 && Prop.BathroomsCount < FilterCriteria.MinBathrooms)
    {
        return false;
    }

    // Price range
    if (FilterCriteria.MinPrice > 0.0f && Prop.Price < FilterCriteria.MinPrice)
    {
        return false;
    }
    if (FilterCriteria.MaxPrice > 0.0f && Prop.Price > FilterCriteria.MaxPrice)
    {
        return false;
    }

    // Surface range
    if (FilterCriteria.MinSurface > 0.0f && Prop.Surface < FilterCriteria.MinSurface)
    {
        return false;
    }
    if (FilterCriteria.MaxSurface > 0.0f && Prop.Surface > FilterCriteria.MaxSurface)
    {
        return false;
    }

    return true;
}

void USightPortalUnitSearchWidget::SortFilteredProperties()
{
    switch (FilterCriteria.SortMode)
    {
    case ESightPortalUnitSortMode::PriceAscending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.Price < B.Price; });
        break;
    case ESightPortalUnitSortMode::PriceDescending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.Price > B.Price; });
        break;
    case ESightPortalUnitSortMode::SurfaceAscending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.Surface < B.Surface; });
        break;
    case ESightPortalUnitSortMode::SurfaceDescending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.Surface > B.Surface; });
        break;
    case ESightPortalUnitSortMode::BedroomsAscending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.BedroomsCount < B.BedroomsCount; });
        break;
    case ESightPortalUnitSortMode::BedroomsDescending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.BedroomsCount > B.BedroomsCount; });
        break;
    case ESightPortalUnitSortMode::NameAscending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.Name < B.Name; });
        break;
    case ESightPortalUnitSortMode::NameDescending:
        FilteredProperties.Sort([](const FSightPortalProperty& A, const FSightPortalProperty& B) { return A.Name > B.Name; });
        break;
    default:
        break;
    }
}

void USightPortalUnitSearchWidget::BuildResultCards()
{
    if (!ResultsScrollBox && !ResultsContainer)
    {
        ResolveUnboundWidgets();
    }

    UPanelWidget* TargetPanel = ResultsScrollBox ? Cast<UPanelWidget>(ResultsScrollBox) : ResultsContainer;
    if (!TargetPanel) return;

    TargetPanel->ClearChildren();

    TSubclassOf<USightPortalUnitSearchResultWidget> ClassToSpawn = ResultCardWidgetClass ? ResultCardWidgetClass : TSubclassOf<USightPortalUnitSearchResultWidget>(USightPortalUnitSearchResultWidget::StaticClass());

    for (const FSightPortalProperty& Prop : FilteredProperties)
    {
        USightPortalUnitSearchResultWidget* Card = CreateWidget<USightPortalUnitSearchResultWidget>(this, ClassToSpawn);
        if (Card)
        {
            Card->SetVisibility(ESlateVisibility::Visible);
            APropertyVisualizer** FoundVis = VisualizerMap.Find(Prop.Name);
            APropertyVisualizer* Vis = FoundVis ? *FoundVis : nullptr;
            Card->SetupResultCard(Prop, Vis, this, RoomType, ActiveCurrency);
            TargetPanel->AddChild(Card);
        }
    }

    const int32 Count = FilteredProperties.Num();
    const int32 Total = AllProperties.Num();

    if (ResultCountText)
    {
        ResultCountText->SetText(FText::FromString(FString::Printf(TEXT("Showing %d of %d Units"), Count, Total)));
    }

    const bool bHasResults = Count > 0;
    if (NoResultsText)
    {
        NoResultsText->SetVisibility(bHasResults ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
    if (EmptyStateWidget)
    {
        EmptyStateWidget->SetVisibility(bHasResults ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
}

void USightPortalUnitSearchWidget::ApplyFilters()
{
    FilteredProperties.Empty();

    for (const FSightPortalProperty& Prop : AllProperties)
    {
        if (MatchesFilterCriteria(Prop))
        {
            FilteredProperties.Add(Prop);
        }
    }

    SortFilteredProperties();
    BuildResultCards();

    // Scene isolation in 3D level
    if (bEnableSceneIsolation)
    {
        ApplySceneIsolation();
    }
}

void USightPortalUnitSearchWidget::ResetFilters()
{
    FilterCriteria = FSightPortalUnitFilterCriteria();

    if (SearchInputBox)
    {
        SearchInputBox->SetText(FText::GetEmpty());
    }

    PopulateFilterDropdowns();

    if (MinPriceSlider) MinPriceSlider->SetValue(0.0f);
    if (MaxPriceSlider) MaxPriceSlider->SetValue(1.0f);
    if (MinPriceInputBox) MinPriceInputBox->SetText(FText::FromString(TEXT("0")));
    if (MaxPriceInputBox) MaxPriceInputBox->SetText(FText::GetEmpty());

    if (MinSurfaceSlider) MinSurfaceSlider->SetValue(0.0f);
    if (MaxSurfaceSlider) MaxSurfaceSlider->SetValue(1.0f);
    if (MinSurfaceInputBox) MinSurfaceInputBox->SetText(FText::FromString(TEXT("0")));
    if (MaxSurfaceInputBox) MaxSurfaceInputBox->SetText(FText::GetEmpty());

    ClearSceneIsolation();
    ApplyFilters();
}

void USightPortalUnitSearchWidget::DismissSearchWidget()
{
    ClearSceneIsolation();
    OnUnitSearchClosed.Broadcast();
    RemoveFromParent();
}

void USightPortalUnitSearchWidget::HandleUnitSelected(const FSightPortalProperty& InProperty, APropertyVisualizer* InVisualizer)
{
    OnUnitSelected.Broadcast(InProperty, InVisualizer);

    // If a valid visualizer was found, focus player camera onto it
    if (InVisualizer && IsValid(InVisualizer))
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            if (ASightPortalPlayerController* SightPC = Cast<ASightPortalPlayerController>(PC))
            {
                SightPC->FocusOnPropertyVisualizer(InVisualizer);
            }
        }
    }

    if (bCloseOnUnitSelected)
    {
        DismissSearchWidget();
    }
}

void USightPortalUnitSearchWidget::HandleUnitExplore(const FSightPortalProperty& InProperty, APropertyVisualizer* InVisualizer)
{
    OnUnitExploreClicked.Broadcast(InProperty, InVisualizer);

    // Open the full 2D Property Detail modal for this property
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ASightPortalPlayerController* SightPC = Cast<ASightPortalPlayerController>(PC))
        {
            if (InVisualizer && IsValid(InVisualizer))
            {
                USightPortal2DPropertyDetailWidget* Detail = SightPC->ShowPropertyDetailWidget(InVisualizer);
                if (Detail)
                {
                    Detail->CurrencySymbol = ActiveCurrency.CurrencySymbol;
                    Detail->bSymbolPrefix = ActiveCurrency.bSymbolPrefix;
                    Detail->DecimalPlaces = ActiveCurrency.DecimalPlaces;
                    Detail->ExchangeRate = ActiveCurrency.ExchangeRate;
                    Detail->DisplayPropertyDetails(InVisualizer->PropertyDetails);
                }
            }
            else
            {
                // Spawn detail modal directly if visualizer is not in the level
                TSubclassOf<USightPortal2DPropertyDetailWidget> ClassToSpawn = DetailWidgetClass ? DetailWidgetClass : SightPC->Detail2DWidgetClass;
                if (!ClassToSpawn)
                {
                    ClassToSpawn = USightPortal2DPropertyDetailWidget::StaticClass();
                }

                USightPortal2DPropertyDetailWidget* DetailWidget = CreateWidget<USightPortal2DPropertyDetailWidget>(SightPC, ClassToSpawn);
                if (DetailWidget)
                {
                    DetailWidget->CurrencySymbol = ActiveCurrency.CurrencySymbol;
                    DetailWidget->bSymbolPrefix = ActiveCurrency.bSymbolPrefix;
                    DetailWidget->DecimalPlaces = ActiveCurrency.DecimalPlaces;
                    DetailWidget->ExchangeRate = ActiveCurrency.ExchangeRate;
                    DetailWidget->SetRoomType(RoomType);
                    DetailWidget->DisplayPropertyDetails(InProperty);
                    DetailWidget->AddToViewport(30);
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------------
// UI Event Handlers
// ----------------------------------------------------------------------------------

void USightPortalUnitSearchWidget::OnSearchTextChanged(const FText& InText)
{
    FilterCriteria.SearchKeyword = InText.ToString();
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnSearchTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    FilterCriteria.SearchKeyword = InText.ToString();
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnClearSearchClicked()
{
    FilterCriteria.SearchKeyword.Empty();
    if (SearchInputBox)
    {
        SearchInputBox->SetText(FText::GetEmpty());
    }
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnZoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;
    FilterCriteria.SelectedZone = SelectedItem;
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnBlockSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;
    FilterCriteria.SelectedBlock = SelectedItem;
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnClassSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;
    FilterCriteria.SelectedClass = SelectedItem;
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnAvailabilitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;
    FilterCriteria.SelectedAvailability = SelectedItem;
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnBedroomsSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;
    if (SelectedItem.IsEmpty() || SelectedItem == TEXT("Any"))
    {
        FilterCriteria.MinBedrooms = 0;
    }
    else
    {
        // Extract leading integer
        FilterCriteria.MinBedrooms = FCString::Atoi(*SelectedItem);
    }
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnBathroomsSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;
    if (SelectedItem.IsEmpty() || SelectedItem == TEXT("Any"))
    {
        FilterCriteria.MinBathrooms = 0;
    }
    else
    {
        FilterCriteria.MinBathrooms = FCString::Atoi(*SelectedItem);
    }
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnSortSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;
    if (SortComboBox)
    {
        const int32 Index = SortComboBox->GetSelectedIndex();
        if (Index >= 0 && Index <= (int32)ESightPortalUnitSortMode::NameDescending)
        {
            FilterCriteria.SortMode = (ESightPortalUnitSortMode)Index;
        }
    }
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnCurrencySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bIsPopulatingDropdowns) return;

    if (CurrencyComboBox)
    {
        const int32 Index = CurrencyComboBox->GetSelectedIndex();
        if (Currencies.IsValidIndex(Index))
        {
            ActiveCurrency = Currencies[Index];
            UpdateResultCardCurrencies();
            return;
        }
    }

    // Fallback lookup by DisplayLabel or CurrencyCode
    for (const FSightPortalCurrencySetting& Cur : Currencies)
    {
        if (Cur.DisplayLabel.Equals(SelectedItem, ESearchCase::IgnoreCase) || Cur.CurrencyCode.Equals(SelectedItem, ESearchCase::IgnoreCase))
        {
            ActiveCurrency = Cur;
            UpdateResultCardCurrencies();
            break;
        }
    }
}

void USightPortalUnitSearchWidget::OnMinPriceSliderChanged(float Value)
{
    FilterCriteria.MinPrice = Value * MaxPriceFoundInDataset;
    if (MinPriceInputBox)
    {
        MinPriceInputBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), FilterCriteria.MinPrice)));
    }
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnMaxPriceSliderChanged(float Value)
{
    FilterCriteria.MaxPrice = Value >= 1.0f ? 0.0f : (Value * MaxPriceFoundInDataset);
    if (MaxPriceInputBox)
    {
        if (FilterCriteria.MaxPrice > 0.0f)
        {
            MaxPriceInputBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), FilterCriteria.MaxPrice)));
        }
        else
        {
            MaxPriceInputBox->SetText(FText::FromString(TEXT("Max")));
        }
    }
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnMinSurfaceSliderChanged(float Value)
{
    FilterCriteria.MinSurface = Value * MaxSurfaceFoundInDataset;
    if (MinSurfaceInputBox)
    {
        MinSurfaceInputBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), FilterCriteria.MinSurface)));
    }
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnMaxSurfaceSliderChanged(float Value)
{
    FilterCriteria.MaxSurface = Value >= 1.0f ? 0.0f : (Value * MaxSurfaceFoundInDataset);
    if (MaxSurfaceInputBox)
    {
        if (FilterCriteria.MaxSurface > 0.0f)
        {
            MaxSurfaceInputBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), FilterCriteria.MaxSurface)));
        }
        else
        {
            MaxSurfaceInputBox->SetText(FText::FromString(TEXT("Max")));
        }
    }
    if (bAutoFilterOnChange)
    {
        ApplyFilters();
    }
}

void USightPortalUnitSearchWidget::OnMinPriceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    FilterCriteria.MinPrice = FCString::Atof(*InText.ToString());
    if (MinPriceSlider && MaxPriceFoundInDataset > 0.0f)
    {
        MinPriceSlider->SetValue(FMath::Clamp(FilterCriteria.MinPrice / MaxPriceFoundInDataset, 0.0f, 1.0f));
    }
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnMaxPriceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    const FString Str = InText.ToString();
    if (Str.Equals(TEXT("Max"), ESearchCase::IgnoreCase) || Str.IsEmpty())
    {
        FilterCriteria.MaxPrice = 0.0f;
    }
    else
    {
        FilterCriteria.MaxPrice = FCString::Atof(*Str);
    }
    if (MaxPriceSlider && MaxPriceFoundInDataset > 0.0f)
    {
        MaxPriceSlider->SetValue(FilterCriteria.MaxPrice > 0.0f ? FMath::Clamp(FilterCriteria.MaxPrice / MaxPriceFoundInDataset, 0.0f, 1.0f) : 1.0f);
    }
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnMinSurfaceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    FilterCriteria.MinSurface = FCString::Atof(*InText.ToString());
    if (MinSurfaceSlider && MaxSurfaceFoundInDataset > 0.0f)
    {
        MinSurfaceSlider->SetValue(FMath::Clamp(FilterCriteria.MinSurface / MaxSurfaceFoundInDataset, 0.0f, 1.0f));
    }
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnMaxSurfaceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
    const FString Str = InText.ToString();
    if (Str.Equals(TEXT("Max"), ESearchCase::IgnoreCase) || Str.IsEmpty())
    {
        FilterCriteria.MaxSurface = 0.0f;
    }
    else
    {
        FilterCriteria.MaxSurface = FCString::Atof(*Str);
    }
    if (MaxSurfaceSlider && MaxSurfaceFoundInDataset > 0.0f)
    {
        MaxSurfaceSlider->SetValue(FilterCriteria.MaxSurface > 0.0f ? FMath::Clamp(FilterCriteria.MaxSurface / MaxSurfaceFoundInDataset, 0.0f, 1.0f) : 1.0f);
    }
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnResetClicked()
{
    ResetFilters();
}

void USightPortalUnitSearchWidget::OnApplyClicked()
{
    ApplyFilters();
}

void USightPortalUnitSearchWidget::OnCloseClicked()
{
    DismissSearchWidget();
}

void USightPortalUnitSearchWidget::HandleDataReceivedFromConnector(const TArray<FSightPortalProperty>& InPortfolio)
{
    SetPropertyList(InPortfolio);
}

// ----------------------------------------------------------------------------------
// Scene Isolation
// ----------------------------------------------------------------------------------

void USightPortalUnitSearchWidget::ApplySceneIsolation()
{
    if (!bEnableSceneIsolation)
    {
        ClearSceneIsolation();
        return;
    }

    TSet<FString> MatchingNames;
    for (const FSightPortalProperty& Prop : FilteredProperties)
    {
        MatchingNames.Add(Prop.Name);
    }

    for (const auto& Pair : VisualizerMap)
    {
        if (APropertyVisualizer* PV = Pair.Value)
        {
            const bool bIsMatching = MatchingNames.Contains(Pair.Key) || (PV && MatchingNames.Contains(PV->PropertyDetails.Name));
            PV->SetSceneIsolationState(bIsMatching, true, bHideNonMatchingUnits);
        }
    }
}

void USightPortalUnitSearchWidget::ClearSceneIsolation()
{
    for (const auto& Pair : VisualizerMap)
    {
        if (APropertyVisualizer* PV = Pair.Value)
        {
            PV->ResetSceneIsolation();
        }
    }
}

void USightPortalUnitSearchWidget::SetSceneIsolationEnabled(bool bEnabled)
{
    bEnableSceneIsolation = bEnabled;
    if (SceneIsolationCheckBox)
    {
        SceneIsolationCheckBox->SetIsChecked(bEnabled);
    }
    if (bEnableSceneIsolation)
    {
        ApplySceneIsolation();
    }
    else
    {
        ClearSceneIsolation();
    }
}

void USightPortalUnitSearchWidget::ToggleSceneIsolation()
{
    SetSceneIsolationEnabled(!bEnableSceneIsolation);
}

void USightPortalUnitSearchWidget::OnSceneIsolationCheckChanged(bool bIsChecked)
{
    bEnableSceneIsolation = bIsChecked;
    if (bEnableSceneIsolation)
    {
        ApplySceneIsolation();
    }
    else
    {
        ClearSceneIsolation();
    }
}

void USightPortalUnitSearchWidget::OnSceneIsolationButtonClicked()
{
    ToggleSceneIsolation();
}

