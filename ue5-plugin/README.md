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
* **Click to Select:** Left-clicking or tapping any `APropertyVisualizer` (or its `SelectionCollisionBox`) selects/highlights the property visualizer and broadcasts `OnPropertySelected`.
* **Explore to Open 2D Details:** The full 2D detail popup (`USightPortal2DPropertyDetailWidget`) opens when clicking the **Explore** button on the floating 3D World Space Widget (`USightPortal3DPropertyWidget`).
* **Click Away to Dismiss:** Clicking on empty space, background terrain, or non-property geometry automatically dismisses / hides the active 2D detail popup and clears selection.
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
2. In the Details Panel, configure your `WebSocketURL` and `RemoteEndpointURL`.
3. Press **Force Fetch Data** or click **Spawn Property Visualizers**.
4. Each `APropertyVisualizer` will automatically render its floating 3D Widget with surface & bedroom counts.
5. In PIE or standalone, clicking on any `APropertyVisualizer` directly toggles its full 2D detail card on screen, and clicking away anywhere in the world toggles it closed!

---

## 📡 Live Real-Time WebSockets
When a field is edited in the web interface or Google Sheet:
1. The web backend dispatches a `ue5_push` payload over WebSocket.
2. `USightPortalConnector` parses `FSightPortalProperty`.
3. `ASightPortalSiteManager` updates the target `APropertyVisualizer`.
4. `APropertyVisualizer::SetPropertyDetails()` instantly updates both the **3D World Widget** and any open **2D Detail Screen Widget**.
