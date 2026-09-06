#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SightPortalGalleryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UCanvasPanel;
class UOverlay;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSightPortalGalleryIndexChanged, int32, NewIndex, UTexture2D*, ActiveTexture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSightPortalGalleryClosed);

/**
 * Single Gallery item metadata
 */
USTRUCT(BlueprintType)
struct FSightPortalGalleryItem
{
    GENERATED_BODY()

    // Image texture for this gallery item
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Gallery")
    UTexture2D* Texture;

    // Optional caption or title
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Gallery")
    FString Title;

    // Optional description or specs
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Gallery")
    FString Description;

    FSightPortalGalleryItem()
        : Texture(nullptr)
        , Title(TEXT(""))
        , Description(TEXT(""))
    {}

    FSightPortalGalleryItem(UTexture2D* InTexture, const FString& InTitle = TEXT(""))
        : Texture(InTexture)
        , Title(InTitle)
        , Description(TEXT(""))
    {}
};

/**
 * USightPortalGalleryCardWidget
 * Individual interactive image card inside the cover-flow carousel.
 * Handles clicking directly on background cards to bring them to the front.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortalGalleryCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryCard")
    UButton* CardButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryCard")
    UImage* CardImage;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryCard")
    UImage* ReflectionImage;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryCard")
    UImage* BorderFrame;

    // Index of this item in the gallery dataset
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|GalleryCard")
    int32 ItemIndex;

    // Pointer to parent gallery widget
    UPROPERTY(BlueprintReadOnly, Category = "SightPortal|GalleryCard")
    class USightPortalGalleryWidget* ParentGallery;

    // Configure card visuals and index
    UFUNCTION(BlueprintCallable, Category = "SightPortal|GalleryCard")
    void SetupCard(int32 InIndex, UTexture2D* InTexture, class USightPortalGalleryWidget* InParent);

    // Auto-discover child widgets
    void ResolveUnboundWidgets();

protected:
    UFUNCTION()
    void OnCardClicked();
};

/**
 * USightPortalGalleryWidget
 * Interactive 3D Cover-Flow / Carousel Image Gallery for Unreal Engine 5.
 * 
 * Features:
 * 1. 3D Cover-Flow Carousel with smooth interpolation & perspective depth.
 * 2. Left & Right Triangle Navigation Buttons to browse images.
 * 3. Interactive Background Cards: Clicking any card in the background brings it front and center.
 * 4. Top-Right "X" Close Button to dismiss/exit the gallery.
 * 5. Blueprint-configurable list of images & auto-discovery of UMG elements.
 */
UCLASS(Blueprintable, BlueprintType)
class SIGHTPORTAL_API USightPortalGalleryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USightPortalGalleryWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // --- UMG Bound Widgets ---

    // Left navigation arrow button (Triangle button)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryUI")
    UButton* PrevButton;

    // Right navigation arrow button (Triangle button)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryUI")
    UButton* NextButton;

    // Top-right exit / close button (X button)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryUI")
    UButton* CloseButton;

    // Optional active item title text
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryUI")
    UTextBlock* TitleText;

    // Optional active item counter (e.g., "3 / 8")
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryUI")
    UTextBlock* CounterText;

    // Canvas panel holding dynamic carousel card widgets
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SightPortal|GalleryUI")
    UCanvasPanel* CarouselCanvas;

    // Optional card widget class to instantiate inside the carousel canvas
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|GalleryUI")
    TSubclassOf<USightPortalGalleryCardWidget> CardWidgetClass;

    // --- Gallery Configuration & Data ---

    // Array of gallery items
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|GalleryData")
    TArray<FSightPortalGalleryItem> GalleryItems;

    // Convenience list of raw textures (auto-converted to GalleryItems if GalleryItems is empty)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|GalleryData")
    TArray<UTexture2D*> GalleryTextures;

    // Current active center item index
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    int32 ActiveIndex;

    // Cover-Flow Animation Speed (Higher = snappier)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float CarouselAnimationSpeed;

    // Center Card Base Dimensions (Width, Height)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    FVector2D CardBaseSize;

    // Horizontal spacing/stagger offset between consecutive cards (in pixels)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    float CardHorizontalSpacing;

    // Scale reduction per step away from center (e.g. 0.2 means next card is 0.8x, then 0.6x)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    float ScaleStepFactor;

    // Opacity reduction per step away from center
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    float OpacityStepFactor;

    // Minimum scale for distant cards
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    float MinCardScale;

    // Minimum opacity for distant cards
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    float MinCardOpacity;

    // Whether carousel loops infinitely or stops at ends
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Carousel")
    bool bLoopCarousel;

    // --- Dynamic Event Delegates ---

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalGalleryIndexChanged OnIndexChanged;

    UPROPERTY(BlueprintAssignable, Category = "SightPortal|Events")
    FOnSightPortalGalleryClosed OnGalleryClosed;

    // --- Public Operations ---

    // Navigate to previous image (Left triangle)
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void PrevImage();

    // Navigate to next image (Right triangle)
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void NextImage();

    // Jump / smoothly slide to specific image index
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void SetActiveIndex(int32 NewIndex, bool bInstant = false);

    // Add a new texture or item to the gallery at runtime
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void AddGalleryItem(UTexture2D* InTexture, const FString& InTitle = TEXT(""));

    // Clear all gallery items
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void ClearGalleryItems();

    // Close and remove the gallery from viewport
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void DismissGallery();

    // Rebuild the carousel cards in the canvas
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void RebuildCarousel();

    // Auto-discover and resolve unbound UMG child widgets by name and alias
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Gallery")
    void ResolveUnboundWidgets();

protected:
    UFUNCTION()
    void OnPrevButtonClicked();

    UFUNCTION()
    void OnNextButtonClicked();

    UFUNCTION()
    void OnCloseButtonClicked();

    // Update layout transformations of all card widgets based on CurrentAnimatedIndex
    void UpdateCarouselLayout(float AnimatedIndex);

    // Update title and counter labels
    void UpdateHeaderReadouts();

private:
    // Animated floating position (smoothly interpolates towards ActiveIndex)
    float CurrentAnimatedIndex;

    // Array of instantiated card widget references
    UPROPERTY()
    TArray<USightPortalGalleryCardWidget*> CardWidgets;
};
