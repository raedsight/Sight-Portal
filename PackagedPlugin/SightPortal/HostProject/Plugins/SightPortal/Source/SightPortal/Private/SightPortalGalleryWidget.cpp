#include "SightPortalGalleryWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

// ----------------------------------------------------------------------------------
// USightPortalGalleryCardWidget Implementation
// ----------------------------------------------------------------------------------

void USightPortalGalleryCardWidget::ResolveUnboundWidgets()
{
    if (!CardButton)
    {
        CardButton = Cast<UButton>(GetWidgetFromName(FName(TEXT("CardButton"))));
        if (!CardButton) CardButton = Cast<UButton>(GetWidgetFromName(FName(TEXT("Button_Card"))));
        if (!CardButton) CardButton = Cast<UButton>(GetWidgetFromName(FName(TEXT("Btn_Card"))));
        if (!CardButton) CardButton = Cast<UButton>(GetWidgetFromName(FName(TEXT("ImageButton"))));
        if (!CardButton) CardButton = Cast<UButton>(GetWidgetFromName(FName(TEXT("Button_Image"))));
        if (!CardButton) CardButton = Cast<UButton>(GetWidgetFromName(FName(TEXT("Button"))));
        if (!CardButton && WidgetTree)
        {
            WidgetTree->ForEachWidget([this](UWidget* Widget)
            {
                if (!CardButton) CardButton = Cast<UButton>(Widget);
            });
        }
    }

    if (!CardImage)
    {
        CardImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("CardImage"))));
        if (!CardImage) CardImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("Image_Card"))));
        if (!CardImage) CardImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("Img_Card"))));
        if (!CardImage) CardImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("MainImage"))));
        if (!CardImage) CardImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("Image"))));
        if (!CardImage && WidgetTree)
        {
            WidgetTree->ForEachWidget([this](UWidget* Widget)
            {
                if (!CardImage) CardImage = Cast<UImage>(Widget);
            });
        }
    }

    if (!ReflectionImage)
    {
        ReflectionImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("ReflectionImage"))));
        if (!ReflectionImage) ReflectionImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("Reflection"))));
        if (!ReflectionImage) ReflectionImage = Cast<UImage>(GetWidgetFromName(FName(TEXT("Img_Reflection"))));
    }

    if (!BorderFrame)
    {
        BorderFrame = Cast<UImage>(GetWidgetFromName(FName(TEXT("BorderFrame"))));
        if (!BorderFrame) BorderFrame = Cast<UImage>(GetWidgetFromName(FName(TEXT("Frame"))));
        if (!BorderFrame) BorderFrame = Cast<UImage>(GetWidgetFromName(FName(TEXT("Border"))));
    }
}

void USightPortalGalleryCardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ResolveUnboundWidgets();

    if (CardButton)
    {
        CardButton->OnClicked.AddUniqueDynamic(this, &USightPortalGalleryCardWidget::OnCardClicked);
    }
}

void USightPortalGalleryCardWidget::SetupCard(int32 InIndex, UTexture2D* InTexture, USightPortalGalleryWidget* InParent)
{
    ItemIndex = InIndex;
    ParentGallery = InParent;

    ResolveUnboundWidgets();

    if (CardButton)
    {
        CardButton->OnClicked.AddUniqueDynamic(this, &USightPortalGalleryCardWidget::OnCardClicked);
    }

    if (CardImage && InTexture)
    {
        CardImage->SetBrushFromTexture(InTexture, true);
    }

    if (ReflectionImage && InTexture)
    {
        ReflectionImage->SetBrushFromTexture(InTexture, true);
        ReflectionImage->SetRenderTransformAngle(180.0f);
        ReflectionImage->SetRenderOpacity(0.25f);
    }
}

void USightPortalGalleryCardWidget::OnCardClicked()
{
    if (ParentGallery)
    {
        ParentGallery->SetActiveIndex(ItemIndex);
    }
}

// ----------------------------------------------------------------------------------
// USightPortalGalleryWidget Implementation
// ----------------------------------------------------------------------------------

USightPortalGalleryWidget::USightPortalGalleryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , PrevButton(nullptr)
    , NextButton(nullptr)
    , CloseButton(nullptr)
    , TitleText(nullptr)
    , CounterText(nullptr)
    , CarouselCanvas(nullptr)
    , CardWidgetClass(nullptr)
    , ActiveIndex(0)
    , CarouselAnimationSpeed(10.0f)
    , CardBaseSize(FVector2D(640.0f, 380.0f))
    , CardHorizontalSpacing(260.0f)
    , ScaleStepFactor(0.18f)
    , OpacityStepFactor(0.25f)
    , MinCardScale(0.45f)
    , MinCardOpacity(0.35f)
    , bLoopCarousel(false)
    , CurrentAnimatedIndex(0.0f)
{
}

void USightPortalGalleryWidget::ResolveUnboundWidgets()
{
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

    // 1. Navigation and Close Buttons
    if (!PrevButton)
    {
        PrevButton = FindButtonWithAliases({
            TEXT("PrevButton"), TEXT("LeftButton"), TEXT("LeftArrow"), TEXT("Btn_Left"),
            TEXT("Btn_Prev"), TEXT("Previous"), TEXT("TriangleLeft"), TEXT("Button_Left"),
            TEXT("LeftArrowButton"), TEXT("Prev"), TEXT("Left")
        });
    }

    if (!NextButton)
    {
        NextButton = FindButtonWithAliases({
            TEXT("NextButton"), TEXT("RightButton"), TEXT("RightArrow"), TEXT("Btn_Right"),
            TEXT("Btn_Next"), TEXT("Next"), TEXT("TriangleRight"), TEXT("Button_Right"),
            TEXT("RightArrowButton"), TEXT("Right")
        });
    }

    if (!CloseButton)
    {
        CloseButton = FindButtonWithAliases({
            TEXT("CloseButton"), TEXT("ExitButton"), TEXT("Exit"), TEXT("Close"),
            TEXT("DismissButton"), TEXT("Btn_Close"), TEXT("Btn_Exit"), TEXT("Button_Close"),
            TEXT("Button_Exit"), TEXT("XButton"), TEXT("Button_X"), TEXT("CloseBtn"), TEXT("ExitBtn")
        });
    }

    // 2. Text Readouts
    if (!TitleText)
    {
        TitleText = FindTextBlockWithAliases({
            TEXT("TitleText"), TEXT("Title"), TEXT("ImageTitle"), TEXT("TextBlock_Title"), TEXT("Text_Title")
        });
    }

    if (!CounterText)
    {
        CounterText = FindTextBlockWithAliases({
            TEXT("CounterText"), TEXT("Counter"), TEXT("PageCount"), TEXT("TextBlock_Counter"), TEXT("Text_Counter")
        });
    }

    // 3. Carousel Canvas
    if (!CarouselCanvas)
    {
        CarouselCanvas = Cast<UCanvasPanel>(GetWidgetFromName(FName(TEXT("CarouselCanvas"))));
        if (!CarouselCanvas) CarouselCanvas = Cast<UCanvasPanel>(GetWidgetFromName(FName(TEXT("Canvas_Carousel"))));
        if (!CarouselCanvas) CarouselCanvas = Cast<UCanvasPanel>(GetWidgetFromName(FName(TEXT("CardsCanvas"))));
        if (!CarouselCanvas) CarouselCanvas = Cast<UCanvasPanel>(GetWidgetFromName(FName(TEXT("CarouselBox"))));
        if (!CarouselCanvas) CarouselCanvas = Cast<UCanvasPanel>(GetWidgetFromName(FName(TEXT("MainCanvas"))));
        if (!CarouselCanvas && WidgetTree)
        {
            WidgetTree->ForEachWidget([this](UWidget* Widget)
            {
                if (!CarouselCanvas) CarouselCanvas = Cast<UCanvasPanel>(Widget);
            });
        }
    }
}

void USightPortalGalleryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetVisibility(ESlateVisibility::Visible);

    ResolveUnboundWidgets();

    // Bind Button Click Events
    if (PrevButton)
    {
        PrevButton->OnClicked.AddUniqueDynamic(this, &USightPortalGalleryWidget::OnPrevButtonClicked);
    }

    if (NextButton)
    {
        NextButton->OnClicked.AddUniqueDynamic(this, &USightPortalGalleryWidget::OnNextButtonClicked);
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &USightPortalGalleryWidget::OnCloseButtonClicked);
    }

    // Convert raw textures to GalleryItems if needed
    if (GalleryItems.Num() == 0 && GalleryTextures.Num() > 0)
    {
        for (int32 i = 0; i < GalleryTextures.Num(); ++i)
        {
            if (GalleryTextures[i])
            {
                FString ItemTitle = FString::Printf(TEXT("Perspective View %d"), i + 1);
                GalleryItems.Add(FSightPortalGalleryItem(GalleryTextures[i], ItemTitle));
            }
        }
    }

    // Build or link Carousel Cards
    RebuildCarousel();

    CurrentAnimatedIndex = (float)ActiveIndex;
    UpdateCarouselLayout(CurrentAnimatedIndex);
    UpdateHeaderReadouts();
}

void USightPortalGalleryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Smoothly interpolate towards ActiveIndex
    if (!FMath::IsNearlyEqual(CurrentAnimatedIndex, (float)ActiveIndex, 0.001f))
    {
        CurrentAnimatedIndex = FMath::FInterpTo(CurrentAnimatedIndex, (float)ActiveIndex, InDeltaTime, CarouselAnimationSpeed);
        UpdateCarouselLayout(CurrentAnimatedIndex);
    }
}

void USightPortalGalleryWidget::RebuildCarousel()
{
    if (!CarouselCanvas)
    {
        ResolveUnboundWidgets();
    }

    if (!CarouselCanvas) return;

    // Safely remove only previously generated card widgets so we don't delete buttons/headers
    for (USightPortalGalleryCardWidget* OldCard : CardWidgets)
    {
        if (OldCard && IsValid(OldCard))
        {
            CarouselCanvas->RemoveChild(OldCard);
        }
    }
    CardWidgets.Empty();

    if (GalleryItems.Num() == 0 && GalleryTextures.Num() > 0)
    {
        for (int32 i = 0; i < GalleryTextures.Num(); ++i)
        {
            if (GalleryTextures[i])
            {
                FString ItemTitle = FString::Printf(TEXT("Perspective View %d"), i + 1);
                GalleryItems.Add(FSightPortalGalleryItem(GalleryTextures[i], ItemTitle));
            }
        }
    }

    if (GalleryItems.Num() == 0) return;

    // Use default class if not set
    TSubclassOf<USightPortalGalleryCardWidget> ClassToSpawn = CardWidgetClass ? CardWidgetClass : TSubclassOf<USightPortalGalleryCardWidget>(USightPortalGalleryCardWidget::StaticClass());

    for (int32 i = 0; i < GalleryItems.Num(); ++i)
    {
        USightPortalGalleryCardWidget* NewCard = CreateWidget<USightPortalGalleryCardWidget>(this, ClassToSpawn);
        if (NewCard)
        {
            NewCard->SetVisibility(ESlateVisibility::Visible);
            NewCard->SetupCard(i, GalleryItems[i].Texture, this);

            UCanvasPanelSlot* CanvasSlot = CarouselCanvas->AddChildToCanvas(NewCard);
            if (CanvasSlot)
            {
                // Anchor to center
                CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                CanvasSlot->SetSize(CardBaseSize);
            }

            CardWidgets.Add(NewCard);
        }
    }

    UpdateCarouselLayout(CurrentAnimatedIndex);
}

void USightPortalGalleryWidget::UpdateCarouselLayout(float AnimatedIndex)
{
    const int32 TotalCards = CardWidgets.Num();
    if (TotalCards == 0) return;

    for (int32 i = 0; i < TotalCards; ++i)
    {
        USightPortalGalleryCardWidget* Card = CardWidgets[i];
        if (!Card || !IsValid(Card)) continue;

        // Calculate offset from animated center
        float Dist = (float)i - AnimatedIndex;

        if (bLoopCarousel && TotalCards > 1)
        {
            while (Dist > TotalCards * 0.5f) Dist -= TotalCards;
            while (Dist < -TotalCards * 0.5f) Dist += TotalCards;
        }

        float AbsDist = FMath::Abs(Dist);

        // Perspective Scale & Opacity Calculations
        float Scale = FMath::Clamp(1.0f - (AbsDist * ScaleStepFactor), MinCardScale, 1.0f);
        float Opacity = FMath::Clamp(1.0f - (AbsDist * OpacityStepFactor), MinCardOpacity, 1.0f);

        // Horizontal layout position with subtle perspective curve
        float HorizontalOffset = Dist * CardHorizontalSpacing;
        float VerticalOffset = AbsDist * 10.0f; // Subtle drop for background cards

        // Z-Order: Front center card has highest Z-Order
        int32 ZOrder = 200 - FMath::RoundToInt(AbsDist * 20.0f);

        // Apply to Canvas Slot
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Card->Slot))
        {
            CanvasSlot->SetPosition(FVector2D(HorizontalOffset, VerticalOffset));
            CanvasSlot->SetSize(CardBaseSize);
            CanvasSlot->SetZOrder(ZOrder);
        }

        // Apply Render Transform and Opacity
        Card->SetRenderScale(FVector2D(Scale, Scale));
        Card->SetRenderOpacity(Opacity);
    }
}

void USightPortalGalleryWidget::UpdateHeaderReadouts()
{
    if (GalleryItems.IsValidIndex(ActiveIndex))
    {
        if (TitleText)
        {
            TitleText->SetText(FText::FromString(GalleryItems[ActiveIndex].Title));
        }

        if (CounterText)
        {
            FString CountStr = FString::Printf(TEXT("%d / %d"), ActiveIndex + 1, GalleryItems.Num());
            CounterText->SetText(FText::FromString(CountStr));
        }
    }
}

void USightPortalGalleryWidget::PrevImage()
{
    if (GalleryItems.Num() == 0) return;

    int32 Target = ActiveIndex - 1;
    if (Target < 0)
    {
        Target = bLoopCarousel ? GalleryItems.Num() - 1 : 0;
    }

    SetActiveIndex(Target);
}

void USightPortalGalleryWidget::NextImage()
{
    if (GalleryItems.Num() == 0) return;

    int32 Target = ActiveIndex + 1;
    if (Target >= GalleryItems.Num())
    {
        Target = bLoopCarousel ? 0 : GalleryItems.Num() - 1;
    }

    SetActiveIndex(Target);
}

void USightPortalGalleryWidget::SetActiveIndex(int32 NewIndex, bool bInstant)
{
    if (GalleryItems.Num() == 0) return;

    ActiveIndex = FMath::Clamp(NewIndex, 0, GalleryItems.Num() - 1);

    if (bInstant)
    {
        CurrentAnimatedIndex = (float)ActiveIndex;
        UpdateCarouselLayout(CurrentAnimatedIndex);
    }

    UpdateHeaderReadouts();

    UTexture2D* ActiveTex = GalleryItems.IsValidIndex(ActiveIndex) ? GalleryItems[ActiveIndex].Texture : nullptr;
    OnIndexChanged.Broadcast(ActiveIndex, ActiveTex);
}

void USightPortalGalleryWidget::AddGalleryItem(UTexture2D* InTexture, const FString& InTitle)
{
    if (!InTexture) return;

    GalleryItems.Add(FSightPortalGalleryItem(InTexture, InTitle.IsEmpty() ? FString::Printf(TEXT("Item %d"), GalleryItems.Num() + 1) : InTitle));
    RebuildCarousel();
    UpdateHeaderReadouts();
}

void USightPortalGalleryWidget::ClearGalleryItems()
{
    GalleryItems.Empty();
    ActiveIndex = 0;
    CurrentAnimatedIndex = 0.0f;
    RebuildCarousel();
    UpdateHeaderReadouts();
}

void USightPortalGalleryWidget::DismissGallery()
{
    OnGalleryClosed.Broadcast();
    RemoveFromParent();
}

void USightPortalGalleryWidget::OnPrevButtonClicked()
{
    PrevImage();
}

void USightPortalGalleryWidget::OnNextButtonClicked()
{
    NextImage();
}

void USightPortalGalleryWidget::OnCloseButtonClicked()
{
    DismissGallery();
}
