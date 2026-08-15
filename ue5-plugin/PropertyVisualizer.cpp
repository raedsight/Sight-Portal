#include "PropertyVisualizer.h"
#include "BlockSpline.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "SightPortalPlayerController.h"

APropertyVisualizer::APropertyVisualizer()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // Create selection collision box to support raycast picking & cursor click events
    SelectionCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionCollisionBox"));
    SelectionCollisionBox->SetupAttachment(RootComponent);
    SelectionCollisionBox->SetBoxExtent(FVector(100.0f, 100.0f, 120.0f));
    SelectionCollisionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
    SelectionCollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SelectionCollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SelectionCollisionBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);

    // Create and attach 3D World Space Widget Component
    Widget3DComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget3DComponent"));
    Widget3DComponent->SetupAttachment(RootComponent);
    Widget3DComponent->SetWidgetSpace(EWidgetSpace::World);
    Widget3DComponent->SetDrawSize(FVector2D(320.0f, 160.0f));
    Widget3DComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f)); // Floating above actor
    Widget3DComponent->SetTwoSided(true);

    // Default Widget Classes
    Widget3DClass = USightPortal3DPropertyWidget::StaticClass();
    Detail2DWidgetClass = USightPortal2DPropertyDetailWidget::StaticClass();

    ManualRelativeTransform = FTransform::Identity;
}

void APropertyVisualizer::BeginPlay()
{
    Super::BeginPlay();

    // Set 3D widget class if specified
    if (Widget3DComponent && Widget3DClass)
    {
        Widget3DComponent->SetWidgetClass(Widget3DClass);
        Widget3DComponent->InitWidget();

        // Bind Explore button event from 3D Widget instance
        if (USightPortal3DPropertyWidget* Widget3D = Cast<USightPortal3DPropertyWidget>(Widget3DComponent->GetUserWidgetObject()))
        {
            Widget3D->SetPropertyData(PropertyDetails);
            Widget3D->OnExploreRequested.AddUniqueDynamic(this, &APropertyVisualizer::OnExploreRequestedFrom3DWidget);
        }
    }
}

void APropertyVisualizer::SetPropertyDetails(const FSightPortalProperty& InDetails)
{
    PropertyDetails = InDetails;

    if (Widget3DComponent)
    {
        if (USightPortal3DPropertyWidget* Widget3D = Cast<USightPortal3DPropertyWidget>(Widget3DComponent->GetUserWidgetObject()))
        {
            Widget3D->SetPropertyData(InDetails);
        }
    }

    if (Active2DDetailWidget && Active2DDetailWidget->IsInViewport())
    {
        Active2DDetailWidget->DisplayPropertyDetails(InDetails);
    }
}

USightPortal2DPropertyDetailWidget* APropertyVisualizer::OpenPropertyDetail2DWidget()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC)
    {
        return nullptr;
    }

    // If active player controller is SightPortal Player Controller, delegate selection & display
    if (ASightPortalPlayerController* SightPC = Cast<ASightPortalPlayerController>(PC))
    {
        Active2DDetailWidget = SightPC->ShowPropertyDetailWidget(this);
        return Active2DDetailWidget;
    }

    if (!Detail2DWidgetClass)
    {
        Detail2DWidgetClass = USightPortal2DPropertyDetailWidget::StaticClass();
    }

    if (Detail2DWidgetClass)
    {
        // Remove existing detail widget if open
        if (Active2DDetailWidget && Active2DDetailWidget->IsInViewport())
        {
            Active2DDetailWidget->RemoveFromParent();
        }

        Active2DDetailWidget = CreateWidget<USightPortal2DPropertyDetailWidget>(PC, Detail2DWidgetClass);
        if (Active2DDetailWidget)
        {
            Active2DDetailWidget->AddToViewport(100);
            Active2DDetailWidget->DisplayPropertyDetails(PropertyDetails);

            // Set UI Input mode so user can interact with the 2D detail popup
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(Active2DDetailWidget->TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = true;
        }
    }

    return Active2DDetailWidget;
}

void APropertyVisualizer::OnExploreRequestedFrom3DWidget(const FSightPortalProperty& InProperty)
{
    OpenPropertyDetail2DWidget();
}

#if WITH_EDITOR
void APropertyVisualizer::PostEditMove(bool bFinished)
{
    Super::PostEditMove(bFinished);
    bHasBeenManuallyMoved = true;

    // Get the parent spline actor (RowSpline)
    AActor* AttachParent = GetAttachParentActor();
    if (AttachParent && AttachParent->IsA(ABlockSpline::StaticClass()))
    {
        ABlockSpline* BlockSpline = Cast<ABlockSpline>(AttachParent);
        if (BlockSpline && BlockSpline->SplineComponent)
        {
            TArray<AActor*> AttachedActors;
            BlockSpline->GetAttachedActors(AttachedActors);

            int32 MyIndex = -1;
            int32 Index = 0;
            for (AActor* Child : AttachedActors)
            {
                if (IsValid(Child) && Child->IsA(APropertyVisualizer::StaticClass()))
                {
                    if (Child == this)
                    {
                        MyIndex = Index;
                        break;
                    }
                    Index++;
                }
            }

            if (MyIndex != -1)
            {
                float Distance = MyIndex * BlockSpline->VisualizerSpacing;
                FVector DefaultLoc = BlockSpline->SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                FRotator DefaultRot = BlockSpline->SplineComponent->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + BlockSpline->VisualizerRotationOffset;
                FVector DefaultScale = BlockSpline->VisualizerScaleOffset;

                FTransform DefaultTransform(DefaultRot, DefaultLoc, DefaultScale);
                FTransform ActualTransform = GetActorTransform();

                ManualRelativeTransform = ActualTransform.GetRelativeTransform(DefaultTransform);
            }
        }
    }
}
#endif
