#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SightPortalConnector.h"
#include "SightPortalUnitSearchWidget.generated.h"

class UButton;
class UTextBlock;
class UEditableTextBox;
class UComboBoxString;
class USlider;
class UScrollBox;
class UPanelWidget;
class UBorder;
class UImage;
class APropertyVisualizer;
class USightPortal2DPropertyDetailWidget;
class USightPortalUnitSearchWidget;

/**
 * Sort options for property search results
 */
UENUM(BlueprintType)
enum class ESightPortalUnitSortMode : uint8
{
    Default UMETA(DisplayName = "Default"),
    PriceAscending UMETA(DisplayName = "Price: Low to High"),
    PriceDescending UMETA(DisplayName = "Price: High to Low"),
    SurfaceAscending UMETA(DisplayName = "Surface: Small to Large"),
    SurfaceDescending UMETA(DisplayName = "Surface: Large to Small"),
    BedroomsAscending UMETA(DisplayName = "Bedrooms: Low to High"),
    BedroomsDescending UMETA(DisplayName = "Bedrooms: High to Low"),
    NameAscending UMETA(DisplayName = "Name: A to Z"),
    NameDescending UMETA(DisplayName = "Name: Z to A")
};

/**
 * Currency configuration with symbol, code, exchange rate, and display format
 */
USTRUCT(BlueprintType)
struct FSightPortalCurrencySetting
{
    GENERATED_BODY()

    FSightPortalCurrencySetting()
        : CurrencyCode(TEXT("IQD"))
        , CurrencySymbol(TEXT("د.ع"))
        , ExchangeRate(1.0f)
        , DisplayLabel(TEXT("IQD (د.ع)"))
        , bSymbolPrefix(false)
        , DecimalPlaces(2)
        , MinimumIntegerDigits(1)
        , bUseThousandSeparators(false)
        , bAbbreviateLargeNumbers(false)
    {
    }

    FSightPortalCurrencySetting(const FString& InCode, const FString& InSymbol, float InRate, const FString& InLabel = TEXT(""), bool bInPrefix = false, int32 InDecimals = 2, bool bInThousandSeparators = false, bool bInAbbreviate = false, int32 InMinIntDigits = 1)
        : CurrencyCode(InCode)
        , CurrencySymbol(InSymbol)
        , ExchangeRate(InRate)
        , DisplayLabel(InLabel.IsEmpty() ? InCode : InLabel)
        , bSymbolPrefix(bInPrefix)
        , DecimalPlaces(InDecimals)
        , MinimumIntegerDigits(InMinIntDigits)
        , bUseThousandSeparators(bInThousandSeparators)
        , bAbbreviateLargeNumbers(bInAbbreviate)
    {
    }

    // Currency code (e.g. "IQD", "USD", "EUR", "AED", "SAR", "TRY")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    FString CurrencyCode = TEXT("IQD");

    // Currency symbol (e.g. "د.ع", "$", "€", "AED", "SAR", "₺")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    FString CurrencySymbol = TEXT("د.ع");

    // Exchange rate relative to base price (Portal prices are in Iraqi Dinars, so IQD = 1.0)
    // ConvertedPrice = BasePriceInIQD * ExchangeRate
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    float ExchangeRate = 1.0f;

    // Display label in dropdown (e.g. "IQD (د.ع)", "USD ($)", "EUR (€)", "AED (AED)")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    FString DisplayLabel = TEXT("IQD (د.ع)");

    // Whether the currency symbol should be placed before the amount or after (e.g. 150000000.00 د.ع)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    bool bSymbolPrefix = false;

    // Number of decimal places to display (default: 2 for 0000000000.00 format)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    int32 DecimalPlaces = 2;

    // Minimum integer digits (default: 1 for standard 150000000.00, set to 10 for zero-padded 0000000000.00)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    int32 MinimumIntegerDigits = 1;

    // Whether to use comma thousand grouping separators (e.g. 150,000,000.00 vs 150000000.00)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    bool bUseThousandSeparators = false;

    // Whether to abbreviate large numbers with K / M (default: false to preserve full 0000000000.00 format)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    bool bAbbreviateLargeNumbers = false;
};

/**
 * Filter and query criteria structure for finding units
 */
USTRUCT(BlueprintType)
struct FSightPortalUnitFilterCriteria
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    FString SearchKeyword = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    FString SelectedZone = TEXT("All");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    FString SelectedBlock = TEXT("All");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    FString SelectedClass = TEXT("All");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    FString SelectedAvailability = TEXT("All");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    int32 MinBedrooms = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    int32 MinBathrooms = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    float MinPrice = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    float MaxPrice = 0.0f; // 0 = no upper limit

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    float MinSurface = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    float MaxSurface = 0.0f; // 0 = no upper limit

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Search")
    ESightPortalUnitSortMode SortMode = ESightPortalUnitSortMode::Default;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSightPortalUnitSelected, const FSightPortalProperty&, SelectedProperty, APropertyVisualizer*, VisualizerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSightPortalUnitExploreClicked, const FSightPortalProperty&, SelectedProperty, APropertyVisualizer*, VisualizerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortalUnitSearchClosed);

/**
 * USightPortalUnitSearchResultWidget
 * Individual interactive property card inside the search results list.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortalUnitSearchResultWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USightPortalUnitSearchResultWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;

    // Configure card visuals and data
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void SetupResultCard(const FSightPortalProperty& InProperty, APropertyVisualizer* InVisualizer, USightPortalUnitSearchWidget* InParent, const FString& InRoomType = TEXT("Bedrooms"), const FSightPortalCurrencySetting& InCurrency = FSightPortalCurrencySetting());

    // Update active currency and format of the card
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Currency")
    void SetCurrency(const FSightPortalCurrencySetting& InCurrency);

    // Active currency setting used for formatting price on this card
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    FSightPortalCurrencySetting ActiveCurrency;

    // Format price value using current currency settings
    UFUNCTION(BlueprintPure, Category = "SightPortal|Currency")
    FString FormatPrice(float InBasePrice) const;

    // Auto-discover child widgets
    void ResolveUnboundWidgets();

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UnitSearch")
    FSightPortalProperty PropertyData;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UnitSearch")
    TWeakObjectPtr<APropertyVisualizer> AssociatedVisualizer;

    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UnitSearch")
    USightPortalUnitSearchWidget* ParentSearchWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    FString RoomType;

    // --- UMG Bindings (Optional) ---
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* NameText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* ZoneText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* BlockText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* ZoneBlockText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* DoorNoText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* PriceText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* SurfaceText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* BuildingSurfaceText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* BedroomsText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* BathroomsText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* AvailabilityText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* ClassText;

    // Buttons
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* CardButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* LocateButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* ExploreButton;

protected:
    UFUNCTION()
    void OnCardClicked();

    UFUNCTION()
    void OnLocateClicked();

    UFUNCTION()
    void OnExploreClicked();
};

/**
 * USightPortalUnitSearchWidget
 * Full interactive Unit Search & Filter modal for Unreal Engine 5 archviz walkthroughs.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortalUnitSearchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USightPortalUnitSearchWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- Public Operations ---

    // Gathers all APropertyVisualizer actors in the world and initial spreadsheet properties, populating the search widget
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void InitializeData();

    // Set custom property portfolio directly
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void SetPropertyList(const TArray<FSightPortalProperty>& InProperties);

    // Apply active filter criteria and regenerate search result cards
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void ApplyFilters();

    // Reset all filter controls and search terms to defaults
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void ResetFilters();

    // Dismiss and remove search modal from viewport
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void DismissSearchWidget();

    // Invoked when a unit result card is selected (focuses camera in 3D)
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void HandleUnitSelected(const FSightPortalProperty& InProperty, APropertyVisualizer* InVisualizer);

    // Invoked when explore / details is clicked on a unit result card (opens full 2D detail modal)
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void HandleUnitExplore(const FSightPortalProperty& InProperty, APropertyVisualizer* InVisualizer);

    // Auto-discover and bind UMG child widgets by name and aliases
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void ResolveUnboundWidgets();

    // --- Configuration ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    TSubclassOf<USightPortalUnitSearchResultWidget> ResultCardWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    TSubclassOf<USightPortal2DPropertyDetailWidget> DetailWidgetClass;

    // Suffix/Label for bedroom count (e.g. "Bedrooms", "Beds", "Rooms", "Offices")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    FString RoomType;

    // Set custom room type label
    UFUNCTION(BlueprintCallable, Category = "SightPortal|UnitSearch")
    void SetRoomType(const FString& InRoomType) { RoomType = InRoomType; }

    // Get current room type label
    UFUNCTION(BlueprintPure, Category = "SightPortal|UnitSearch")
    FString GetRoomType() const { return RoomType; }

    // --- Currency & Exchange Rate Settings ---

    // List of configurable currencies and their exchange rates. Configurable directly in Widget Details panel.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    TArray<FSightPortalCurrencySetting> Currencies;

    // Currency code selected by default (e.g. "USD", "EUR", "AED")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Currency")
    FString DefaultCurrencyCode;

    // Active currency setting currently applied to search cards
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|Currency")
    FSightPortalCurrencySetting ActiveCurrency;

    // Set active currency by currency code (e.g. "USD", "AED", "EUR")
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Currency")
    void SetActiveCurrencyByCode(const FString& InCurrencyCode);

    // Get active currency setting
    UFUNCTION(BlueprintPure, Category = "SightPortal|Currency")
    FSightPortalCurrencySetting GetActiveCurrency() const { return ActiveCurrency; }

    // Add or update a currency setting with code, symbol, exchange rate, display label, and prefix
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Currency")
    void AddOrUpdateCurrency(const FSightPortalCurrencySetting& InCurrency);

    // Whether typing or changing dropdowns immediately filters results without clicking "Apply"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    bool bAutoFilterOnChange;

    // Automatically dismiss the search widget when a unit is located/selected so user can view the 3D scene
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    bool bCloseOnUnitSelected;

    // Automatically find all APropertyVisualizer actors in the active level
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    bool bAutoDiscoverLevelVisualizers;

    // --- Scene Isolation (3D Level Highlighting & Dimming) ---

    // Whether scene isolation is enabled (highlight matching units in 3D level, dim or hide non-matching)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|SceneIsolation")
    bool bEnableSceneIsolation;

    // If scene isolation is active, hide non-matching visualizers (true) or highlight matching via custom depth stencil (false)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|SceneIsolation")
    bool bHideNonMatchingUnits;

    // Apply scene isolation to all registered visualizers based on currently filtered properties
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SceneIsolation")
    void ApplySceneIsolation();

    // Reset scene isolation on all visualizers back to their standard appearance
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SceneIsolation")
    void ClearSceneIsolation();

    // Enable or disable scene isolation
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SceneIsolation")
    void SetSceneIsolationEnabled(bool bEnabled);

    // Toggle scene isolation on/off
    UFUNCTION(BlueprintCallable, Category = "SightPortal|SceneIsolation")
    void ToggleSceneIsolation();

    // Active filter criteria
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|UnitSearch")
    FSightPortalUnitFilterCriteria FilterCriteria;

    // Cached full portfolio data
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UnitSearch")
    TArray<FSightPortalProperty> AllProperties;

    // Filtered properties currently displayed
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UnitSearch")
    TArray<FSightPortalProperty> FilteredProperties;

    // Map of Property Name to APropertyVisualizer
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|UnitSearch")
    TMap<FString, APropertyVisualizer*> VisualizerMap;

    // --- Dynamic Event Delegates ---

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalUnitSelected OnUnitSelected;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalUnitExploreClicked OnUnitExploreClicked;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalUnitSearchClosed OnUnitSearchClosed;

    // --- UMG UI Widget Bindings (Optional) ---

    // Keyword Search
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UEditableTextBox* SearchInputBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* ClearSearchButton;

    // Dropdown Filters
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UComboBoxString* ZoneComboBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UComboBoxString* BlockComboBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UComboBoxString* ClassComboBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UComboBoxString* AvailabilityComboBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UComboBoxString* BedroomsComboBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UComboBoxString* BathroomsComboBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UComboBoxString* SortComboBox;

    // Currency Selector Dropdown (Optional UMG binding)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|Currency")
    UComboBoxString* CurrencyComboBox;

    // Sliders / Numerical inputs for Price & Surface
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    USlider* MinPriceSlider;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    USlider* MaxPriceSlider;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UEditableTextBox* MinPriceInputBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UEditableTextBox* MaxPriceInputBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    USlider* MinSurfaceSlider;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    USlider* MaxSurfaceSlider;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UEditableTextBox* MinSurfaceInputBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UEditableTextBox* MaxSurfaceInputBox;

    // Action Buttons
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* ResetFiltersButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* ApplyFiltersButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* CloseButton;

    // Scene Isolation UI Bindings (Optional)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    class UCheckBox* SceneIsolationCheckBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UButton* SceneIsolationButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* SceneIsolationText;

    // Results & Feedback
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UScrollBox* ResultsScrollBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UPanelWidget* ResultsContainer;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* ResultCountText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UTextBlock* NoResultsText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|UnitSearch")
    UWidget* EmptyStateWidget;

protected:
    // Event handlers
    UFUNCTION()
    void OnSearchTextChanged(const FText& InText);

    UFUNCTION()
    void OnSearchTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void OnClearSearchClicked();

    UFUNCTION()
    void OnZoneSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnBlockSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnClassSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnAvailabilitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnBedroomsSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnBathroomsSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnSortSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnCurrencySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnMinPriceSliderChanged(float Value);

    UFUNCTION()
    void OnMaxPriceSliderChanged(float Value);

    UFUNCTION()
    void OnMinSurfaceSliderChanged(float Value);

    UFUNCTION()
    void OnMaxSurfaceSliderChanged(float Value);

    UFUNCTION()
    void OnMinPriceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void OnMaxPriceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void OnMinSurfaceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void OnMaxSurfaceTextCommitted(const FText& InText, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void OnResetClicked();

    UFUNCTION()
    void OnApplyClicked();

    UFUNCTION()
    void OnCloseClicked();

    UFUNCTION()
    void OnSceneIsolationCheckChanged(bool bIsChecked);

    UFUNCTION()
    void OnSceneIsolationButtonClicked();

    UFUNCTION()
    void HandleDataReceivedFromConnector(const TArray<FSightPortalProperty>& InPortfolio);

    // Helpers
    void PopulateFilterDropdowns();
    void PopulateCurrencyDropdown();
    void BuildResultCards();
    void UpdateResultCardCurrencies();
    void SortFilteredProperties();
    bool MatchesFilterCriteria(const FSightPortalProperty& Property) const;

private:
    float MaxPriceFoundInDataset;
    float MaxSurfaceFoundInDataset;
    bool bIsPopulatingDropdowns;
};
