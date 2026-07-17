#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightPortalSiteManager.generated.h"

/**
 * ASightPortalSiteManager
 * A highly customizable C++ Site Manager class that handles the construction site as a whole
 * and spawns SightPortalZoneManager actors.
 */
UCLASS(Blueprintable, BlueprintType, Category = "SightPortal|SiteManager")
class SIGHTPORTAL_API ASightPortalSiteManager : public AActor
{
    GENERATED_BODY()

public:
    ASightPortalSiteManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void OnConstruction(const FTransform& Transform) override;

public:
    // Number of Zones to spawn in this construction site, exposed to be called/modified in editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration", meta = (ClampMin = "0"))
    int32 ZoneCount = 1;

    // Distance/spacing parameter between spawned zone managers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    float ZoneSpacing = 1500.0f;

    // Blueprint-editable Template class to spawn for each Zone Manager
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightPortal|Configuration")
    TSubclassOf<AActor> ZoneManagerClass;

    // List of active spawned Zone Managers representing zones in the level
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightPortal|State")
    TArray<AActor*> ActiveZoneManagers;

    // --- Operations ---

    // Spawns and arranges Zone Managers dynamically
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    void SpawnZoneManagers();

    // Cleans up all spawned Zone Managers
    UFUNCTION(BlueprintCallable, Category = "SightPortal|Operations")
    void ClearZoneManagers();

private:
    bool bIsSpawning = false;
};
