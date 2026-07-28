# SightPortal UE5 Plugin - Real-Estate Visualizer & UI Widgets

This plugin provides native C++ real-time synchronization between **Google Sheets / SightPortal Web Dashboard** and **Unreal Engine 5**. It includes site, zone, block, and property visualizers, complete with **3D World Space Widgets** and **2D Detail Screen Popups**.

---

## 🛠️ Plugin Architecture & Widgets

### 1. 🌐 `USightPortal3DPropertyWidget` (3D World Space Widget)
A lightweight 3D UMG Widget attached directly to `APropertyVisualizer` actors floating in world space.
* **Surface Area Display:** Reads `FSightPortalProperty::Surface` (e.g. `185.0 m²`).
* **Bedrooms Count:** Reads `FSightPortalProperty::BedroomsCount` (e.g. `3 Beds`).
* **Explore Button:** Interactive UButton triggering `OnExploreRequested` dynamic delegate when clicked.
* **Property Name:** Reads `FSightPortalProperty::Name` (e.g. `Z1B11`).

### 2. 🖥️ `USightPortal2DPropertyDetailWidget` (2D Full Detail HUD Widget)
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

### 2. Creating UMG Blueprint Widgets (Optional Custom Styling)
You can derive UMG Widget Blueprints from these C++ classes in the Unreal Editor:
1. **3D Compact Widget:** Create a Widget Blueprint derived from `USightPortal3DPropertyWidget`.
   * Add `UTextBlock` elements named `SurfaceText`, `BedroomsText`, `PropertyNameText`.
   * Add a `UButton` element named `ExploreButton`.
2. **2D Detail Modal Widget:** Create a Widget Blueprint derived from `USightPortal2DPropertyDetailWidget`.
   * Add `UTextBlock` elements named `NameText`, `ZoneText`, `BlockText`, `DoorNoText`, `PriceText`, `SurfaceText`, `BuildingSurfaceText`, `AvailabilityText`, `BedroomsCountText`, `BathroomsCountText`, `ClassText`.
   * Add a `UButton` named `CloseButton`.

### 3. Placing in the Level
1. Drag an `ASightPortalSiteManager` actor into your scene.
2. In the Details Panel, configure your `WebSocketURL` and `RemoteEndpointURL`.
3. Press **Force Fetch Data** or click **Spawn Property Visualizers**.
4. Each `APropertyVisualizer` will automatically render its floating 3D Widget with surface & bedroom counts. Clicking **Explore** on any 3D widget in PIE or standalone opens the complete 2D Detail Modal displaying all real-estate fields.

---

## 📡 Live Real-Time WebSockets
When a field is edited in the web interface or Google Sheet:
1. The web backend dispatches a `ue5_push` payload over WebSocket.
2. `USightPortalConnector` parses `FSightPortalProperty`.
3. `ASightPortalSiteManager` updates the target `APropertyVisualizer`.
4. `APropertyVisualizer::SetPropertyDetails()` instantly updates both the **3D World Widget** and any open **2D Detail Screen Widget**.
