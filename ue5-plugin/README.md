# SightPortal UE5 Plugin - Real-Estate Visualizer, UI Widgets & Player Controller

This plugin provides native C++ real-time synchronization between **Google Sheets / SightPortal Web Dashboard** and **Unreal Engine 5**. It includes site, zone, block, and property visualizers, complete with **3D World Space Widgets**, **2D Detail Screen Popups**, and an interactive **Player Controller**.

---

## 🛠️ Plugin Architecture & Components

### 1. 🎮 `ASightPortalPlayerController` (Interactive ArchViz Controller)
A specialized Unreal Engine 5 Player Controller enabling intuitive viewport exploration:
* **WASD Flying Navigation:**
  * **W / S / Up / Down Arrow:** Fly forward and backward.
  * **A / D / Left / Right Arrow:** Strafe left and right.
  * **E / Space:** Elevate upward.
  * **Q:** Descend downward.
  * **Left Shift:** Hold to sprint (2.5x speed multiplier).
  * **Mouse Wheel:** Scroll to dolly zoom or adjust positioning.
* **Mouse Look:**
  * **Hold Right Mouse Button (RMB):** Captures mouse and looks around (Pitch & Yaw).
  * **Release RMB:** Restores mouse cursor for UI picking.
* **Touchscreen Gestures:**
  * **1-Finger Short Tap:** Pick property to toggle details, or tap empty space to dismiss.
  * **1-Finger Drag:** Rotate camera view / look around.
  * **2-Finger Pinch:** Zoom forward / backward.
  * **2-Finger Drag:** Pan camera horizontally and vertically.
* **Click to Select & Fast Travel to LookAt Arrow:** Left-clicking or tapping any `APropertyVisualizer` (or its `SelectionCollisionBox`) selects/highlights the property, smoothly fast-travels the player controller camera to the `LookAtArrowComponent` location and rotation, displays the floating 3D widget, and locks player navigation movement.
* **Movement Lock & Static State:** While focused on a selected property, player navigation (WASD, mouse look, touch swipe/pinch) is locked so the camera remains perfectly static in front of the property.
* **Unlock on Close or Explore:** Clicking either the **Close** button or the **Explore** button on the 3D widget automatically unlocks player navigation movement and resets the camera's Roll rotation to 0.0° (level with the horizon).
* **Explore to Open 2D Details:** The full 2D detail popup (`USightPortal2DPropertyDetailWidget`) opens when clicking the **Explore** button on the floating 3D World Space Widget (`USightPortal3DPropertyWidget`).
* **Click Away to Dismiss:** Clicking on empty space, background terrain, or non-property geometry automatically dismisses / hides the active 2D detail popup, unlocks movement, and clears selection.
* **Selection State & Delegates:** Exposes `OnPropertySelected` and `OnPropertyDeselected` dynamic Blueprint delegates for custom camera framing, audio cues, or lighting highlights.


### 2. 🌐 `USightPortal3DPropertyWidget` (3D World Space Widget)
A lightweight 3D UMG Widget attached directly to `APropertyVisualizer` actors floating in world space.
* **Surface Area Display:** Reads `FSightPortalProperty::Surface` (e.g. `185.0 m²`).
* **Bedrooms Count:** Reads `FSightPortalProperty::BedroomsCount` (e.g. `3 Beds`).
* **Explore Button:** Interactive UButton triggering `OnExploreRequested` dynamic delegate and auto-hiding the 3D widget while opening the 2D detail popup.
* **Close Button:** Interactive UButton (`CloseButton` with `OnCloseRequested` delegate) allowing users to dismiss/hide the floating 3D widget.
* **Property Name:** Reads `FSightPortalProperty::Name` (e.g. `Z1B11`).

### 3. 🖥️ `USightPortal2DPropertyDetailWidget` (2D Full Detail HUD Widget)
A full 2D screen overlay modal that presents **all** attributes read directly from the `FSightPortalProperty` structure:
* **Property Name** (`Name`)
* **Zone & Block** (`Zone`, `Block`)
* **Door Number** (`DoorNo`)
* **Listing Price** (`Price`, formatted e.g. `$450,000.00`)
* **Living Surface Area** (`Surface`, formatted e.g. `145.00 m²`)
* **Total Building Surface Area** (`BuildingSurface`, formatted e.g. `210.00 m²`)
* **Availability Status** (`Availability`, e.g. `Available`, `Reserved`, `Sold`)
* **Bedrooms Count** (`BedroomsCount`)
* **Bathrooms Count** (`BathroomsCount`)
* **Property Class / Category** (`Class`, e.g. `Penthouse`, `Duplex`, `Villa`)
* **Close / Dismiss Button** (`CloseButton` with `OnDetailClosed` delegate)

### 4. 🏢 `ASightPortalSiteManager` & `ASightPortalZoneManager`
* **Non-Destructive Zone/Block Additions (`CallInEditor` Buttons in Details Panel):**
  * **`AddNewZone`**: Spawns and attaches a single new `ASightPortalZoneManager` actor positioned alongside current zones without destroying, resetting, or altering already placed zones or visualizers.
  * **`AddConfiguredZone`**: Spawns a new Zone using the editable parameters in the Details panel (`NewZoneCustomName`, `NewZoneCustomLocation`, and `bUseCustomLocationForNewZone`).
  * **`AddZoneWithParameters(CustomZoneName, CustomLocation)`**: Blueprint & C++ callable function to spawn a specific Zone with custom transform and identifier.
  * **`AddNewBlock` (on `ASightPortalZoneManager`)**: Spawns and attaches a single new `ASightPortalBlockManager` alongside current blocks without destroying or resetting existing blocks.
  * **`AddConfiguredBlock` (on `ASightPortalZoneManager`)**: Spawns a new Block using the editable parameters in the Details panel (`NewBlockCustomName`, `NewBlockCustomLocation`, and `bUseCustomLocationForNewBlock`).
* **Batch Operations (`CallInEditor` Buttons):**
  * **`SpawnZoneManagers` / `SpawnBlockManagers`**: Spawns and arranges the full configured count.
  * **`SpawnPropertyVisualizers`**: Propagates site-wide to spawn/update all visualizers from cached portal records.
  * **`ClearZoneManagers` / `ClearBlockManagers` / `ClearPropertyVisualizers`**: Safely clears spawned entities.

### 5. 〰️ `ABlockSpline` (Spline-Guided Property Layout)
An actor containing a `USplineComponent` capable of spawning, arranging, and managing `APropertyVisualizer` actors along custom curves:
* **Editor Actions (`CallInEditor`):**
  * **`SpawnPropertyVisualizers()` / `SpawnProperties()`:** Spawns and distributes properties along the spline at configured `VisualizerSpacing`, matching real-estate records from `USightPortalConnector` cache and registering them into the site registry.
  * **`ClearPropertyVisualizers()` / `ClearProperties()` / `ClearActiveSpawnedActors()`:** Safely cleans up and destroys all spawned property visualizers along this spline and unregisters them from the site manager.
* **Configurable Properties:**
  * **`PropertyCount`:** Number of properties to spawn along this spline (default: `3`).
  * **`VisualizerSpacing`:** Distance along the spline between consecutive visualizers (default: `350.0f`).
  * **`PropertyVisualizerClass`:** Blueprint class of `APropertyVisualizer` to spawn.
  * **`BlockName` & `ZoneName`:** Block and Zone identifiers for property attribute matching.
  * **`StartingDoorNumber`:** Starting door/unit number index.
  * **`bAutoManageSplinePoints`:** When enabled, automatically configures a straight line spline matching total property length (`PropertyCount * VisualizerSpacing`). When disabled, allows full manual shaping of custom spline curves in the editor.
  * **`VisualizerRotationOffset` & `VisualizerScaleOffset`:** Rotational and scale offsets applied along the curve.

---

## 🚀 Installation & Setup in Unreal Engine 5

### 1. `YourProject.Build.cs` Dependencies
Ensure your project's `.Build.cs` file includes UMG and WebSockets:
```csharp
PublicDependencyModuleNames.AddRange(new string[] { 
    "Core", 
    "CoreUObject", 
    "Engine", 
    "InputCore", 
    "UMG", 
    "Slate", 
    "SlateCore", 
    "HTTP", 
    "Json", 
    "JsonUtilities", 
    "WebSockets" 
});
```

### 2. Using `ASightPortalGameMode`
You can use the provided `ASightPortalGameMode` (or a Blueprint child of it):
1. In your Level's **World Settings** -> **GameMode Override** -> select `ASightPortalGameMode` (or your Blueprint child `BP_SightPortalGameMode`).
2. Alternatively, in **Project Settings** -> **Maps & Modes** -> **Default GameMode** -> select `ASightPortalGameMode`.
3. It automatically configures `PlayerControllerClass = ASightPortalPlayerController::StaticClass()`.

### 3. Creating UMG Blueprint Widgets (Optional Custom Styling)
You can derive UMG Widget Blueprints from these C++ classes in the Unreal Editor:
1. **3D Compact Widget:** Create a Widget Blueprint derived from `USightPortal3DPropertyWidget`.
   * Add `UTextBlock` elements named `SurfaceText`, `BedroomsText`, `PropertyNameText`.
   * Add a `UButton` element named `ExploreButton`.
2. **2D Detail Modal Widget:** Create a Widget Blueprint derived from `USightPortal2DPropertyDetailWidget`.
   * Add `UTextBlock` elements named `NameText`, `ZoneText`, `BlockText`, `DoorNoText`, `PriceText`, `SurfaceText`, `BuildingSurfaceText`, `AvailabilityText`, `BedroomsCountText`, `BathroomsCountText`, `ClassText`.
   * Add a `UButton` named `CloseButton`.

### 4. Placing in the Level
1. Drag an `ASightPortalSiteManager` actor into your scene.
2. In the Details Panel under the **SightPortal | Bridge** category, configure your URLs:
   * **`RemoteEndpointURL`**: `https://sightportal.ai.studio/api/health` (HTTP initial dataset fetch & polling fallback)
   * **`WebSocketURL`**: `wss://sightportal.ai.studio/ws` or `wss://sightportal.ai.studio/ws/<client-slug>` (Instant live bi-directional sync)
3. Press **Force Fetch Data** or click **Spawn Property Visualizers**.
4. Each `APropertyVisualizer` will automatically render its floating 3D Widget with surface & bedroom counts.
5. In PIE or standalone, clicking on any `APropertyVisualizer` directly toggles its full 2D detail card on screen, and clicking away anywhere in the world toggles it closed!

---

## ⚡ Blueprint Implementable Events (Responding to Live Changes)

When an update is pushed from the Sight Portal, custom **Blueprint Implementable Events** are dispatched across the UE5 actor and widget hierarchy, allowing you to trigger custom animations, sound effects, particle feedback, material recoloring, lighting shifts, or gameplay logic directly in Blueprints:

### 1. `APropertyVisualizer`
* **`Event OnPropertyDataUpdatedFromPortal(UpdatedProperty)`**:
  Fired whenever this specific property visualizer's real-estate details (Price, Availability, Surface, Bedrooms, Custom Attributes, etc.) are updated from the Portal.
  * *Use case:* Change material color based on availability (e.g. green for Available, red for Sold, orange for Reserved), play sound effect, or trigger a pulse/glow material animation.
* **`Event OnPortfolioSynchronizedFromPortal(FullPortfolio)`**:
  Fired when full portfolio real-estate data is synchronized from the Portal.

### 2. `USightPortal3DPropertyWidget`
* **`Event OnPropertyDataUpdatedFromPortal(UpdatedProperty)`**:
  Fired in your 3D Widget Blueprint whenever property data is updated.
  * *Use case:* Trigger custom UMG entry animations, update custom dynamic icons, or animate badge colors.

### 3. `USightPortal2DPropertyDetailWidget`
* **`Event OnPropertyDataUpdatedFromPortal(UpdatedProperty)`**:
  Fired in your 2D HUD Detail Widget Blueprint when details are refreshed live while viewing.
  * *Use case:* Animate price changes, refresh custom feature tag lists, or show live status badges.

### 4. `ASightPortalSiteManager`, `ASightPortalZoneManager`, `ASightPortalBlockManager`, `ABlockSpline`
* **`Event OnPortalPropertyUpdated(PropertyName, PropertyData)`**:
  Fired whenever a single property matching this site/zone/block/spline is updated.
* **`Event OnPortalDataReceived(PropertyPortfolio)`**:
  Fired whenever the full dataset is received or refreshed from the Portal.

### 5. `ASightPortalPlayerController`
* **`Event OnPortalPropertyUpdated(PropertyName, PropertyData)`**:
  Fired in your Player Controller Blueprint on property update.
* **`Event OnPortalDataReceived(PropertyPortfolio)`**:
  Fired in your Player Controller Blueprint on full portfolio sync.

---

## 🎨 Changing a Single Specific Property Visualizer Class

You can assign different custom Blueprint Visualizer classes (e.g. `BP_VillaVisualizer`, `BP_PenthouseVisualizer`, `BP_CommercialVisualizer`) to specific visualizer actors within the `SpawnedVisualizers` array without changing the entire row/spline default:

### Method 1: In the Unreal Editor Details Panel
1. Select the `ASightPortalBlockManager` or `ABlockSpline` actor in the level.
2. In the **Details Panel** under **SightPortal | Operations | Change Visualizer**:
   * Set **`TargetRowIndex`** (e.g. `0` for the first row) *(on Block Manager)*.
   * Set **`TargetVisualizerIndex`** (e.g. `2` for the 3rd visualizer in that row/spline).
   * Choose **`NewVisualizerClass`** (e.g., `BP_LuxuryVillaVisualizer`).
   * Click **Apply Visualizer Class Override**.
3. The specific visualizer actor at that index is replaced with an instance of the new class immediately while preserving its world position, orientation, scale, manual offsets, and `FSightPortalProperty` real-estate data.

### Method 2: Per-Index Override Map (Persistent)
In `ASightPortalBlockManager`'s **PropertyRowDetails** or `ABlockSpline`'s **PropertyVisualizerOverrides**:
* Add key-value entries to `PropertyVisualizerOverrides` (e.g. Key `0` -> `BP_StudioVisualizer`, Key `4` -> `BP_CornerVillaVisualizer`).
* When **Spawn Property Visualizers** runs, it automatically respects your per-index overrides!

### Method 3: In Blueprints & C++
Call the new runtime functions dynamically:
* `ChangeVisualizerClassAtIndex(RowIndex, VisualizerIndex, NewClass)` on `ASightPortalBlockManager` or `ABlockSpline`.
* `ChangeVisualizerClassForProperty(PropertyName, NewClass)` on `ASightPortalSiteManager`, `ASightPortalZoneManager`, `ASightPortalBlockManager`, or `ABlockSpline`.
* `ChangeVisualizerClassAtSite(ZoneName, BlockName, RowIndex, VisualizerIndex, NewClass)` on `ASightPortalSiteManager`.

---

## 🖥️ 2D Property Detail Screen Widget & 3D Widget Setup

### Setting Up Your 2D Detail Widget Blueprint (`WBP_PropertyDetail`)
1. Create a **Widget Blueprint** (e.g. `WBP_PropertyDetail`).
2. In the Blueprint editor, go to **File -> Reparent Blueprint** and select **`SightPortal2DPropertyDetailWidget`**.
3. In the **Hierarchy** panel, create your Text Blocks and Buttons. Ensure **"Is Variable"** is checked for each widget in the Details panel.
4. **Widget Names (Hierarchy IDs)**:
   The C++ code binds to widgets by name. The following names are supported directly and via built-in aliases:
   * **Name**: `NameText` (aliases: `Name`, `PropertyName`)
   * **Zone**: `ZoneText` (aliases: `Zone`, `ZoneName`)
   * **Block**: `BlockText` (aliases: `Block`, `BlockName`)
   * **Door / Unit #**: `DoorNoText` (aliases: `Door`, `DoorText`, `DoorNo`, `DoorNumber`)
   * **Price**: `PriceText` (aliases: `Price`, `PropertyPrice`)
   * **Surface Area**: `SurfaceText` (aliases: `Surface`, `SurfaceArea`, `Area`)
   * **Building Surface**: `BuildingSurfaceText` (aliases: `BldSurfaceText`, `Bld Surface`, `BldSurface`, `BuildingSurface`)
   * **Availability**: `AvailabilityText` (aliases: `Availability`, `Status`)
   * **Bedrooms**: `BedroomsCountText` (aliases: `Bedrooms`, `BedroomsText`, `Beds`, `BedroomsCount`)
   * **Bathrooms**: `BathroomsCountText` (aliases: `Bathrooms`, `BathroomsText`, `Baths`, `BathroomsCount`)
   * **Class / Category**: `ClassText` (aliases: `Class`, `Category`, `PropertyClass`)
   * **Close / Exit Button**: `CloseButton` (aliases: `Exit`, `ExitButton`, `CloseBtn`, `DismissButton`)

5. **Assigning the 2D Widget Class in Unreal**:
   * On your `APropertyVisualizer` (or `BP_PropertyVisualizer` blueprint): Set **`Detail 2D Widget Class`** to your `WBP_PropertyDetail`.
   * On your `ASightPortalPlayerController` (or `BP_SightPortalPlayerController`): Set **`Detail 2D Widget Class`** to your `WBP_PropertyDetail`.

---

## 📡 Live Real-Time WebSockets
When a field is edited in the web interface or Google Sheet:
1. The web backend dispatches a `ue5_push` payload over WebSocket.
2. `USightPortalConnector` parses `FSightPortalProperty`.
3. `APropertyVisualizer` / `ASightPortalSiteManager` receive the update and update their internal state, 3D world widget, and 2D detail screen widget.
4. The `OnPropertyDataUpdatedFromPortal` and `OnPortalPropertyUpdated` Blueprint Implementable Events execute immediately in Blueprints.
