import express from "express";
import path from "path";
import { createServer as createViteServer } from "vite";
import { WebSocketServer, WebSocket } from "ws";

async function startServer() {
  const app = express();
  const PORT = 3000;

  // JSON parsing and large coordinate matrices helper setting
  app.use(express.json({ limit: "15mb" }));
  app.use(express.urlencoded({ extended: true, limit: "15mb" }));

  // Shared server-side state repository holding compiled real-estate attributes
  let latestSheetData = {
    client_slug: "hyperion-vis",
    target_class: "ArchViz Real-Estate Portfolio",
    attributes_matrix: [
      {
        Name: "Villa Bella Vista",
        Zone: "Luxury West",
        Block: "A",
        "Door No": "101",
        Price: "1250000",
        Surface: "450.0",
        Availability: "Available",
        BuildingSurface: "350.0",
        BedroomsCount: "5",
        BathroomsCount: "4"
      },
      {
        Name: "Penthouse SkyLine",
        Zone: "Downtown",
        Block: "B",
        "Door No": "3202",
        Price: "2890000",
        Surface: "280.0",
        Availability: "Available",
        BuildingSurface: "220.0",
        BedroomsCount: "3",
        BathroomsCount: "3"
      },
      {
        Name: "LakeSide Cottage",
        Zone: "North Basin",
        Block: "C",
        "Door No": "24",
        Price: "680000",
        Surface: "185.0",
        Availability: "Under Offer",
        BuildingSurface: "150.0",
        BedroomsCount: "4",
        BathroomsCount: "2"
      },
      {
        Name: "Minimalist Loft",
        Zone: "Arts District",
        Block: "D",
        "Door No": "408",
        Price: "450000",
        Surface: "120.0",
        Availability: "Sold",
        BuildingSurface: "100.0",
        BedroomsCount: "2",
        BathroomsCount: "2"
      }
    ]
  };

  // Helper function to broadcast updates to all active WebSocket clients
  const broadcastSync = (event: "init" | "update") => {
    const payloadString = JSON.stringify({
      event,
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: latestSheetData.client_slug,
        target_class: latestSheetData.target_class,
        attributes_matrix: latestSheetData.attributes_matrix
      }
    });

    let clientsCount = 0;
    wss.clients.forEach((client) => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(payloadString);
        clientsCount++;
      }
    });
    console.log(`[WebSocket Broadcast] Sent live '${event}' data to ${clientsCount} connected Unreal Engine sockets`);
  };

  // API Endpoint 1: Healthcheck & Dynamic Unreal Compatibility Fallback Route
  // This mirrors what the Unreal Engine C++ plugin queries by default, responding with compiled datasets
  app.get("/api/health", (req, res) => {
    res.json({
      status: "ok",
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: latestSheetData.client_slug,
        target_class: latestSheetData.target_class,
        attributes_matrix: latestSheetData.attributes_matrix
      }
    });
  });

  // API Endpoint 2: Fetch spreadsheet datasets currently cached in memory
  app.get("/api/sheet-data", (req, res) => {
    res.json({
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: latestSheetData.client_slug,
        target_class: latestSheetData.target_class,
        attributes_matrix: latestSheetData.attributes_matrix
      }
    });
  });

  // API Endpoint 3: Receive real-time sheet-data updates from the React Portal Workspace UI
  app.post("/api/sheet-data", (req, res) => {
    const { client_slug, target_class, attributes_matrix } = req.body;
    
    if (attributes_matrix && Array.isArray(attributes_matrix)) {
      latestSheetData = {
        client_slug: client_slug || "hyperion-vis",
        target_class: target_class || "ArchViz Real-Estate Portfolio",
        attributes_matrix
      };
      
      console.log(`[Full-Stack API] State synchronized for '${latestSheetData.client_slug}' - ${attributes_matrix.length} rows updated.`);
      
      // Real-time Push to all active active WebSockets/Connected Plugin threads directly
      broadcastSync("update");

      return res.json({ success: true, message: "Spreadsheet records successfully cached on backend server" });
    }
    
    return res.status(400).json({ error: "Invalid payload layout structure: missing 'attributes_matrix' array" });
  });

  // Attach Vite development server or production static distribution layer
  if (process.env.NODE_ENV !== "production") {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: "spa",
    });
    app.use(vite.middlewares);
  } else {
    const distPath = path.join(process.cwd(), "dist");
    app.use(express.static(distPath));
    app.get("*", (req, res) => {
      res.sendFile(path.join(distPath, "index.html"));
    });
  }

  const liveUrl = process.env.APP_URL || `http://localhost:${PORT}`;
  const server = app.listen(PORT, "0.0.0.0", () => {
    console.log(`[SightPortal Server] Live Access URL: ${liveUrl}`);
    console.log(`[SightPortal Server] Internal container binding active on http://0.0.0.0:${PORT}`);
    console.log(`[WebSocket Server] Socket handshaking available via ws://${PORT}/ws (or wss:///ws behind SSL proxy)`);
  });

  // Setup WebSocket Server bound to the express platform wrapper with strict route path matching to prevent conflict with Vite Dev Sockets
  // Configure handleProtocols to dynamically echo requested subprotocols (like 'ws'), making handshake highly compatible with Unreal Engine's native WebSocket module
  const wss = new WebSocketServer({
    noServer: true,
    handleProtocols: (protocols) => {
      if (protocols && protocols.size > 0) {
        return Array.from(protocols)[0];
      }
      return false;
    }
  });

  server.on("upgrade", (request, sckt, head) => {
    try {
      const rawUrl = request.url || "";
      const pathname = rawUrl.split("?")[0];
      console.log(`[WebSocket Upgrade] Received upgrade request at path: "${pathname}" (full: "${rawUrl}")`);

      if (pathname === "/ws" || pathname === "/ws/") {
        wss.handleUpgrade(request, sckt, head, (ws) => {
          wss.emit("connection", ws, request);
        });
      } else {
        console.log(`[WebSocket Upgrade] Ignored upgrade path "${pathname}"`);
      }
    } catch (e) {
      // Catch exceptions gracefully
      console.error("[WebSocket Upgrade Error]", e);
    }
  });

  wss.on("connection", (ws: WebSocket) => {
    console.log("[WebSocket connection] Direct Unreal Plugin client linked successfully! Sending initial payload attributes...");
    
    // Immediately stream latest spreadsheet properties matching the active portfolio state (the client initiates, the server responds)
    ws.send(JSON.stringify({
      event: "init",
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: latestSheetData.client_slug,
        target_class: latestSheetData.target_class,
        attributes_matrix: latestSheetData.attributes_matrix
      }
    }));

    // Listen for peer pings to keep connections alive and logs interactive
    ws.on("message", (rawMsg) => {
      try {
        const msgStr = rawMsg.toString();
        if (msgStr === "ping") {
          ws.send("pong");
          return;
        }

        const data = JSON.parse(msgStr);
        console.log(`[WebSocket Received] Got socket event:`, data);
        if (data && data.event === "ping") {
          ws.send(JSON.stringify({ event: "pong" }));
        }
      } catch (err) {
        // Fallback for simple message formats
      }
    });

    ws.on("close", () => {
      console.log("[WebSocket disconnection] Plugin client link terminated.");
    });

    ws.on("error", (err) => {
      console.error("[WebSocket error]", err);
    });
  });
}

startServer();
