#include "SightPortalHUDWidget.h"
#include "SightPortalGalleryWidget.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

USightPortalHUDWidget::USightPortalHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , CompassText(nullptr)
    , CompassNeedleImage(nullptr)
    , CompassRoot(nullptr)
    , TrueNorthOffsetDegrees(0.0f)
    , TimeOfDaySlider(nullptr)
    , TimeOfDayText(nullptr)
    , CurrentTimeHours(9.0f)
    , bUse24HourFormat(true)
    , SunLightActorTag(TEXT("SunLight"))
    , SunLightActor(nullptr)
    , HomeButton(nullptr)
    , GalleryButton(nullptr)
    , ServicesButton(nullptr)
    , UnitSearchButton(nullptr)
    , HomeLocation(FVector::ZeroVector)
    , HomeRotation(FRotator(-30.0f, -45.0f, 0.0f))
    , bAutoCaptureStartLocationAsHome(true)
    , HomeTransitionSpeed(8.0f)
    , GalleryWidgetClass(USightPortalGalleryWidget::StaticClass())
    , bIsSettingTimeSlider(false)
    , bIsHomeLocationInitialized(false)
    , bIsTransitioningToHome(false)
    , HomeTransitionAlpha(0.0f)
{
}

void USightPortalHUDWidget::ResolveUnboundWidgets()
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

    auto FindSliderWithAliases = [this](const TArray<FString>& Aliases) -> USlider*
    {
        for (const FString& Alias : Aliases)
        {
            if (UWidget* FoundWidget = GetWidgetFromName(FName(*Alias)))
            {
                if (USlider* FoundSlider = Cast<USlider>(FoundWidget))
                {
                    return FoundSlider;
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

    auto FindImageWithAliases = [this](const TArray<FString>& Aliases) -> UImage*
    {
        for (const FString& Alias : Aliases)
        {
            if (UWidget* FoundWidget = GetWidgetFromName(FName(*Alias)))
            {
                if (UImage* FoundImage = Cast<UImage>(FoundWidget))
                {
                    return FoundImage;
                }
            }
        }
        return nullptr;
    };

    // 1. Compass Widgets
    if (!CompassText)
    {
        CompassText = FindTextBlockWithAliases({
            TEXT("CompassText"), TEXT("Compass"), TEXT("HeadingText"), TEXT("DirectionText"),
            TEXT("TextBlock_Compass"), TEXT("Text_Compass"), TEXT("Compass_Text"), TEXT("Heading")
        });
    }

    if (!CompassNeedleImage)
    {
        CompassNeedleImage = FindImageWithAliases({
            TEXT("CompassNeedleImage"), TEXT("CompassNeedle"), TEXT("CompassDisc"), TEXT("CompassImage"),
            TEXT("CompassIcon"), TEXT("Image_Compass"), TEXT("Img_Compass"), TEXT("NeedleImage")
        });
    }

    if (!CompassRoot)
    {
        CompassRoot = GetWidgetFromName(FName(TEXT("CompassRoot")));
        if (!CompassRoot) CompassRoot = GetWidgetFromName(FName(TEXT("CompassContainer")));
        if (!CompassRoot) CompassRoot = GetWidgetFromName(FName(TEXT("CompassBox")));
    }

    // 2. Time of Day Widgets
    if (!TimeOfDaySlider)
    {
        TimeOfDaySlider = FindSliderWithAliases({
            TEXT("TimeOfDaySlider"), TEXT("TimeSlider"), TEXT("SunSlider"), TEXT("Slider_Time"),
            TEXT("Slider_TimeOfDay"), TEXT("Time_Slider"), TEXT("DayNightSlider")
        });
    }

    if (!TimeOfDayText)
    {
        TimeOfDayText = FindTextBlockWithAliases({
            TEXT("TimeOfDayText"), TEXT("TimeText"), TEXT("SunTimeText"), TEXT("TextBlock_Time"),
            TEXT("Text_Time"), TEXT("Time_Text"), TEXT("ClockText")
        });
    }

    // 3. Navigation Buttons
    if (!HomeButton)
    {
        HomeButton = FindButtonWithAliases({
            TEXT("HomeButton"), TEXT("Home"), TEXT("Btn_Home"), TEXT("Button_Home"),
            TEXT("HomeBtn"), TEXT("Home_Btn"), TEXT("ButtonHome")
        });
    }

    if (!GalleryButton)
    {
        GalleryButton = FindButtonWithAliases({
            TEXT("GalleryButton"), TEXT("Gallery"), TEXT("Btn_Gallery"), TEXT("Button_Gallery"),
            TEXT("GalleryBtn"), TEXT("Gallary"), TEXT("GallaryButton"), TEXT("Btn_Gallary"), TEXT("Button_Gallary")
        });
    }

    if (!ServicesButton)
    {
        ServicesButton = FindButtonWithAliases({
            TEXT("ServicesButton"), TEXT("Services"), TEXT("Btn_Services"), TEXT("Button_Services"),
            TEXT("ServicesBtn"), TEXT("Surroundings"), TEXT("SurroundingsButton"), TEXT("Surrounding"),
            TEXT("Btn_Surroundings"), TEXT("Button_Surroundings")
        });
    }

    if (!UnitSearchButton)
    {
        UnitSearchButton = FindButtonWithAliases({
            TEXT("UnitSearchButton"), TEXT("UnitSearch"), TEXT("Btn_UnitSearch"), TEXT("Button_UnitSearch"),
            TEXT("UnitSearchBtn"), TEXT("SearchButton"), TEXT("SearchBtn"), TEXT("Search"),
            TEXT("Units"), TEXT("UnitsButton"), TEXT("Btn_Units")
        });
    }
}

void USightPortalHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ResolveUnboundWidgets();

    // Bind Time Slider Event
    if (TimeOfDaySlider)
    {
        TimeOfDaySlider->OnValueChanged.AddUniqueDynamic(this, &USightPortalHUDWidget::OnTimeSliderValueChanged);
        
        // Match slider value to CurrentTimeHours
        // If slider range is 0-1, map hours/24; if 0-24, set directly
        if (TimeOfDaySlider->GetMaxValue() <= 1.0f)
        {
            TimeOfDaySlider->SetValue(FMath::Clamp(CurrentTimeHours / 24.0f, 0.0f, 1.0f));
        }
        else
        {
            TimeOfDaySlider->SetValue(FMath::Clamp(CurrentTimeHours, 0.0f, 24.0f));
        }
    }

    // Bind Button Click Events
    if (HomeButton)
    {
        HomeButton->OnClicked.AddUniqueDynamic(this, &USightPortalHUDWidget::OnHomeClicked);
    }

    if (GalleryButton)
    {
        GalleryButton->OnClicked.AddUniqueDynamic(this, &USightPortalHUDWidget::OnGalleryClicked);
    }

    if (ServicesButton)
    {
        ServicesButton->OnClicked.AddUniqueDynamic(this, &USightPortalHUDWidget::OnServicesClicked);
    }

    if (UnitSearchButton)
    {
        UnitSearchButton->OnClicked.AddUniqueDynamic(this, &USightPortalHUDWidget::OnUnitSearchClicked);
    }

    // Auto-capture initial position if not specified
    APlayerController* PC = GetOwningPlayer();
    if (PC && bAutoCaptureStartLocationAsHome && HomeLocation.IsZero())
    {
        APawn* ControlledPawn = PC->GetPawn();
        if (ControlledPawn)
        {
            HomeLocation = ControlledPawn->GetActorLocation();
            HomeRotation = PC->GetControlRotation();
            bIsHomeLocationInitialized = true;
        }
    }

    // Find Sun Directional Light in level if not assigned
    if (!SunLightActor && GetWorld())
    {
        // First try finding by tag
        if (!SunLightActorTag.IsNone())
        {
            TArray<AActor*> TaggedActors;
            UGameplayStatics::GetAllActorsWithTag(GetWorld(), SunLightActorTag, TaggedActors);
            if (TaggedActors.Num() > 0)
            {
                SunLightActor = TaggedActors[0];
            }
        }

        // Fallback: search for ADirectionalLight in level
        if (!SunLightActor)
        {
            for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
            {
                SunLightActor = *It;
                break;
            }
        }
    }

    // Initialize display state
    SetTimeOfDay(CurrentTimeHours);
    UpdateCompassHeading();
}

void USightPortalHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Update real-time compass based on camera heading
    UpdateCompassHeading();

    // Handle smooth transition to Home viewpoint
    if (bIsTransitioningToHome)
    {
        APlayerController* PC = GetOwningPlayer();
        if (PC)
        {
            APawn* ControlledPawn = PC->GetPawn();
            if (ControlledPawn && HomeTransitionSpeed > 0.0f)
            {
                FVector CurrentLoc = ControlledPawn->GetActorLocation();
                FRotator CurrentRot = PC->GetControlRotation();

                FVector NewLoc = FMath::VInterpTo(CurrentLoc, HomeLocation, InDeltaTime, HomeTransitionSpeed);
                FRotator NewRot = FMath::RInterpTo(CurrentRot, HomeRotation, InDeltaTime, HomeTransitionSpeed);

                ControlledPawn->SetActorLocation(NewLoc);
                PC->SetControlRotation(NewRot);

                if (FVector::DistSquared(NewLoc, HomeLocation) < 4.0f && CurrentRot.Equals(HomeRotation, 0.5f))
                {
                    ControlledPawn->SetActorLocation(HomeLocation);
                    PC->SetControlRotation(HomeRotation);
                    bIsTransitioningToHome = false;
                }
            }
            else
            {
                bIsTransitioningToHome = false;
            }
        }
        else
        {
            bIsTransitioningToHome = false;
        }
    }
}

void USightPortalHUDWidget::UpdateCompassHeading()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    float CameraYaw = 0.0f;
    if (PC->PlayerCameraManager)
    {
        CameraYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;
    }
    else
    {
        CameraYaw = PC->GetControlRotation().Yaw;
    }

    // Apply level true-north offset
    float AdjustedYaw = FRotator::NormalizeAxis(CameraYaw + TrueNorthOffsetDegrees);

    // Update Compass Text (e.g. "N", "NE", "E", "SE", "S", "SW", "W", "NW")
    if (CompassText)
    {
        FString Cardinal = YawToCardinalDirection(AdjustedYaw);
        CompassText->SetText(FText::FromString(Cardinal));
    }

    // Update Compass Needle / Disc visual rotation
    if (CompassNeedleImage)
    {
        // Rotating the needle opposite to camera yaw keeps needle pointing North
        CompassNeedleImage->SetRenderTransformAngle(-AdjustedYaw);
    }
}

FString USightPortalHUDWidget::YawToCardinalDirection(float YawDegrees)
{
    // Normalize to 0 - 360
    float Deg = FRotator::NormalizeAxis(YawDegrees);
    if (Deg < 0.0f) Deg += 360.0f;

    // 8 Cardinal directions (45 deg sectors centered on 0, 45, 90, etc.)
    if (Deg >= 337.5f || Deg < 22.5f) return TEXT("N");
    if (Deg >= 22.5f && Deg < 67.5f) return TEXT("NE");
    if (Deg >= 67.5f && Deg < 112.5f) return TEXT("E");
    if (Deg >= 112.5f && Deg < 157.5f) return TEXT("SE");
    if (Deg >= 157.5f && Deg < 202.5f) return TEXT("S");
    if (Deg >= 202.5f && Deg < 247.5f) return TEXT("SW");
    if (Deg >= 247.5f && Deg < 292.5f) return TEXT("W");
    if (Deg >= 292.5f && Deg < 337.5f) return TEXT("NW");

    return TEXT("N");
}

void USightPortalHUDWidget::OnTimeSliderValueChanged(float NewValue)
{
    if (bIsSettingTimeSlider) return;

    // Detect whether slider is 0.0 to 1.0 or 0.0 to 24.0
    float TargetHours = NewValue;
    if (TimeOfDaySlider)
    {
        float MaxVal = TimeOfDaySlider->GetMaxValue();
        float MinVal = TimeOfDaySlider->GetMinValue();
        if (FMath::IsNearlyEqual(MaxVal, 1.0f, 0.01f) && FMath::IsNearlyEqual(MinVal, 0.0f, 0.01f))
        {
            TargetHours = NewValue * 24.0f;
        }
        else if (MaxVal > 1.0f)
        {
            TargetHours = NewValue;
        }
    }

    SetTimeOfDay(TargetHours);
}

void USightPortalHUDWidget::SetTimeOfDay(float InHours)
{
    CurrentTimeHours = FMath::Clamp(InHours, 0.0f, 24.0f);

    FString FormattedTime = FormatTimeString(CurrentTimeHours);

    // Update Slider Position
    if (TimeOfDaySlider)
    {
        bIsSettingTimeSlider = true;
        float MaxVal = TimeOfDaySlider->GetMaxValue();
        float MinVal = TimeOfDaySlider->GetMinValue();

        if (FMath::IsNearlyEqual(MaxVal, 1.0f, 0.01f) && FMath::IsNearlyEqual(MinVal, 0.0f, 0.01f))
        {
            TimeOfDaySlider->SetValue(FMath::Clamp(CurrentTimeHours / 24.0f, 0.0f, 1.0f));
        }
        else if (MaxVal > 1.0f)
        {
            TimeOfDaySlider->SetValue(FMath::Clamp(CurrentTimeHours, MinVal, MaxVal));
        }
        else
        {
            TimeOfDaySlider->SetValue(FMath::Clamp(CurrentTimeHours / 24.0f, 0.0f, 1.0f));
        }
        bIsSettingTimeSlider = false;
    }

    // Update Time Text Readout
    if (TimeOfDayText)
    {
        TimeOfDayText->SetText(FText::FromString(FormattedTime));
    }

    // Update Sun Directional Light Actor in Level
    UpdateSunLighting(CurrentTimeHours);

    // Broadcast event
    OnTimeOfDayChanged.Broadcast(CurrentTimeHours, FormattedTime);
}

void USightPortalHUDWidget::UpdateSunLighting(float InHours)
{
    if (!SunLightActor || !IsValid(SunLightActor)) return;

    // Map 0 to 24 hours to sun pitch:
    // 6:00 (Sunrise) -> Pitch 0
    // 12:00 (Solar Noon) -> Pitch -90 (or +90 looking down)
    // 18:00 (Sunset) -> Pitch -180 / 0
    // 24:00 (Midnight) -> Pitch +90
    float SunPitch = ((InHours - 6.0f) / 12.0f) * 180.0f;
    float SunYaw = 45.0f; // Typical sun trajectory offset

    FRotator NewSunRot = FRotator(-SunPitch, SunYaw, 0.0f);
    SunLightActor->SetActorRotation(NewSunRot);
}

FString USightPortalHUDWidget::FormatTimeString(float InHours) const
{
    int32 TotalMinutes = FMath::RoundToInt(InHours * 60.0f) % (24 * 60);
    int32 Hours = TotalMinutes / 60;
    int32 Minutes = TotalMinutes % 60;

    if (bUse24HourFormat)
    {
        return FString::Printf(TEXT("%02d:%02d"), Hours, Minutes);
    }
    else
    {
        FString Period = (Hours >= 12) ? TEXT("PM") : TEXT("AM");
        int32 DisplayHours = Hours % 12;
        if (DisplayHours == 0) DisplayHours = 12;
        return FString::Printf(TEXT("%d:%02d %s"), DisplayHours, Minutes, *Period);
    }
}

void USightPortalHUDWidget::SetHomeTransform(FVector InLocation, FRotator InRotation)
{
    HomeLocation = InLocation;
    HomeRotation = InRotation;
    bIsHomeLocationInitialized = true;
}

void USightPortalHUDWidget::OnHomeClicked()
{
    GoToHomeLocation();
}

void USightPortalHUDWidget::GoToHomeLocation()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    APawn* ControlledPawn = PC->GetPawn();
    if (!ControlledPawn) return;

    OnHomeNavigationTriggered.Broadcast(HomeLocation, HomeRotation);

    if (HomeTransitionSpeed <= 0.0f)
    {
        ControlledPawn->SetActorLocation(HomeLocation);
        PC->SetControlRotation(HomeRotation);
        bIsTransitioningToHome = false;
    }
    else
    {
        bIsTransitioningToHome = true;
    }
}

void USightPortalHUDWidget::OnGalleryClicked()
{
    TriggerGalleryAction();
}

void USightPortalHUDWidget::TriggerGalleryAction()
{
    OnGalleryButtonClicked.Broadcast();

    if (GalleryWidgetClass)
    {
        UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), GalleryWidgetClass);
        if (CreatedWidget)
        {
            CreatedWidget->AddToViewport(20);
        }
    }
}

void USightPortalHUDWidget::OnServicesClicked()
{
    TriggerServicesAction();
}

void USightPortalHUDWidget::TriggerServicesAction()
{
    OnServicesButtonClicked.Broadcast();

    if (ServicesWidgetClass)
    {
        UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), ServicesWidgetClass);
        if (CreatedWidget)
        {
            CreatedWidget->AddToViewport(20);
        }
    }
}

void USightPortalHUDWidget::OnUnitSearchClicked()
{
    TriggerUnitSearchAction();
}

void USightPortalHUDWidget::TriggerUnitSearchAction()
{
    OnUnitSearchButtonClicked.Broadcast();

    if (UnitSearchWidgetClass)
    {
        UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), UnitSearchWidgetClass);
        if (CreatedWidget)
        {
            CreatedWidget->AddToViewport(20);
        }
    }
}
