#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "PropertyVisualizer.h"
#include "BlockSpline.generated.h"

/**
 * ABlockSpline
 * An actor that contains a spline component to guide property visualizer layout.
 * Supports standalone and block-managed spawning and clearing of property visualizers along its path.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|BlockSpline")
class SIGHTPORTAL_API ABlockSpline : public AActor
{
    GENERATED_BODY()

public:
    ABlockSpline();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void OnConstruction(const FTransform& Transform) override;

    // Spline component contained in this actor
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|Spline", meta = (AllowPrivateAccess = "true"))
    USplineComponent* SplineComponent;

    // Spacing between visualizers along this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Spline")
    float VisualizerSpacing = 350.0f;

    // Extra rotation offset applied to visualizers on this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Spline")
    FRotator VisualizerRotationOffset = FRotator::ZeroRotator;

    // Scale applied to visualizers on this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Spline")
    FVector VisualizerScaleOffset = FVector(1.0f, 1.0f, 1.0f);

    // If true, the spline points will be automatically managed as a straight line of matching length.
    // Uncheck this to manually edit and shape the spline curve in the editor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Spline")
    bool bAutoManageSplinePoints = false;

    // Number of property visualizers to spawn along this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Properties", meta = (ClampMin = "0"))
    int32 PropertyCount = 3;

    // Blueprint template class to use for visual representation of properties along this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Properties")
    TSubclassOf<APropertyVisualizer> PropertyVisualizerClass;

    // Optional: Specific custom visualizer class overrides per property index along this spline (0-indexed)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Properties")
    TMap<int32, TSubclassOf<APropertyVisualizer>> PropertyVisualizerOverrides;

    // Custom Block Name identifier for property matching
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Properties")
    FString BlockName = TEXT("1");

    // Custom Zone Name identifier for property matching
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Properties")
    FString ZoneName = TEXT("1");

    // Starting door / room number index for properties along this spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Properties", meta = (ClampMin = "1"))
    int32 StartingDoorNumber = 1;

    // List of active spawned property visualizers along this spline
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|Properties")
    TArray<AActor*> SpawnedVisualizers;

    // Track if this block spline has been manually moved by the user in the editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|State")
    bool bHasBeenManuallyMoved = false;

    // --- Change Specific Visualizer Class Parameters (Editor Exposed) ---

    // Target visualizer index (0-indexed) along this spline when modifying a specific visualizer class from editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Change Visualizer", meta = (ClampMin = "0"))
    int32 TargetVisualizerIndex = 0;

    // New Property Visualizer blueprint class to assign to the targeted visualizer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Operations|Change Visualizer")
    TSubclassOf<APropertyVisualizer> NewVisualizerClass;

#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
#endif

    // --- Operations (Exposed to Editor & Blueprints) ---

    // Applies the NewVisualizerClass to the specific visualizer at TargetVisualizerIndex along this spline in the editor
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations|Change Visualizer", meta = (CallInEditor = "true", DisplayName = "Apply Visualizer Class Override"))
    void ApplyVisualizerClassOverride();

    // Replaces a single specific Property Visualizer class at a given index along this spline dynamically while preserving its transform and data
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    APropertyVisualizer* ChangeVisualizerClassAtIndex(int32 VisualizerIndex, TSubclassOf<APropertyVisualizer> InNewClass);

    // Replaces a single specific Property Visualizer class for a given property name along this spline dynamically
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    APropertyVisualizer* ChangeVisualizerClassForProperty(const FString& PropertyName, TSubclassOf<APropertyVisualizer> InNewClass);

    // Spawns and arranges Property Visualizers along this spline curve
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnPropertyVisualizers();

    // Alias to spawn properties along this spline
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void SpawnProperties();

    // Clears and destroys all property visualizers spawned along this spline
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearPropertyVisualizers();

    // Alias to clear properties along this spline
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearProperties();

    // Clears all active spawned actors and visualizers associated with this spline
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SightPortal|Operations")
    void ClearActiveSpawnedActors();

    // Callback when portfolio data is received from connector
    UFUNCTION()
    void HandleDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Callback when a property update is received from connector
    UFUNCTION()
    void HandleSinglePropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

    // Dynamic Blueprint Implementable Event triggered when live real-estate data is pushed from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalDataReceived(const TArray<FSightPortalProperty>& PropertyPortfolio);

    // Dynamic Blueprint Implementable Event triggered when a property along this spline is updated from the Portal
    UFUNCTION(BlueprintImplementableEvent, Category = "SightPortal|Events")
    void OnPortalPropertyUpdated(const FString& PropertyName, const FSightPortalProperty& PropertyData);

private:
    bool bIsSpawning = false;
};
