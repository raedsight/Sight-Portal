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

  // Shared server-side state repository holding compiled client-specific real-estate attributes
  const clientsData: Record<string, {
    client_slug: string;
    target_class: string;
    attributes_matrix: any[];
  }> = {
    "hyperion-vis": {
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
          BathroomsCount: "4",
          Class: "Residential"
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
          BathroomsCount: "3",
          Class: "Residential"
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
          BathroomsCount: "2",
          Class: "Residential"
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
          BathroomsCount: "2",
          Class: "Residential"
        }
      ]
    }
  };

  // Helper function to broadcast updates to all active WebSocket clients connected to a specific client_slug
  const broadcastSync = (event: "init" | "update", client_slug: string) => {
    const data = clientsData[client_slug] || clientsData["hyperion-vis"];
    const payloadString = JSON.stringify({
      event,
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: data.client_slug,
        target_class: data.target_class,
        attributes_matrix: data.attributes_matrix
      }
    });

    let clientsCount = 0;
    wss.clients.forEach((client: any) => {
      if (client.readyState === WebSocket.OPEN) {
        if (client.client_slug === client_slug || client.client_slug === "hyperion-vis") {
          client.send(payloadString);
          clientsCount++;
        }
      }
    });
    console.log(`[WebSocket Broadcast] Sent live '${event}' data to ${clientsCount} connected Unreal Engine sockets for client '${client_slug}'`);
  };

  // API Endpoint 1: Healthcheck & Dynamic Unreal Compatibility Fallback Route
  // This mirrors what the Unreal Engine C++ plugin queries by default, responding with compiled datasets
  app.get("/api/health", (req, res) => {
    const slug = (req.query.client_slug as string) || "hyperion-vis";
    const data = clientsData[slug] || clientsData["hyperion-vis"];

    let ue5Alive = false;
    wss.clients.forEach((client: any) => {
      if (client.readyState === WebSocket.OPEN) {
        if (client.client_slug === slug || client.client_slug === "hyperion-vis") {
          ue5Alive = true;
        }
      }
    });

    res.json({
      status: "ok",
      timestamp: new Date().toISOString(),
      ue5Alive,
      payload: {
        client_slug: data.client_slug,
        target_class: data.target_class,
        attributes_matrix: data.attributes_matrix
      }
    });
  });

  // API Endpoint 2: Fetch spreadsheet datasets currently cached in memory
  app.get("/api/sheet-data", (req, res) => {
    const slug = (req.query.client_slug as string) || "hyperion-vis";
    const data = clientsData[slug] || clientsData["hyperion-vis"];
    res.json({
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: data.client_slug,
        target_class: data.target_class,
        attributes_matrix: data.attributes_matrix
      }
    });
  });

  // API Endpoint 3: Receive real-time sheet-data updates from the React Portal Workspace UI
  app.post("/api/sheet-data", (req, res) => {
    const { client_slug, target_class, attributes_matrix } = req.body;
    
    if (attributes_matrix && Array.isArray(attributes_matrix)) {
      const slug = client_slug || "hyperion-vis";
      clientsData[slug] = {
        client_slug: slug,
        target_class: target_class || "ArchViz Real-Estate Portfolio",
        attributes_matrix
      };
      
      console.log(`[Full-Stack API] State synchronized for client '${slug}' - ${attributes_matrix.length} rows updated.`);
      
      // Real-time Push to all active WebSockets/Connected Plugin threads directly
      broadcastSync("update", slug);

      return res.json({ success: true, message: `Spreadsheet records successfully cached on backend server for client '${slug}'` });
    }
    
    return res.status(400).json({ error: "Invalid payload layout structure: missing 'attributes_matrix' array" });
  });

  // API Endpoint 4: Force manual re-sync broadcast to connected Unreal Engine clients
  app.post("/api/force-sync", (req, res) => {
    const slug = (req.body.client_slug as string) || (req.query.client_slug as string) || "hyperion-vis";
    const data = clientsData[slug] || clientsData["hyperion-vis"];
    
    console.log(`[Full-Stack API] Force re-sync requested for client '${slug}'. Broadcasting state to all active WebSockets...`);
    
    // Broadcast latest state to all active sockets connected for this client
    broadcastSync("update", slug);
    
    res.json({
      success: true,
      message: `Successfully triggered manual re-sync for client '${slug}'. Broadcast sent!`,
      payload: {
        client_slug: data.client_slug,
        target_class: data.target_class,
        attributes_matrix: data.attributes_matrix
      }
    });
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

  const liveUrl = process.env.APP_URL || `http://localhost:3000`;
  const server = app.listen(PORT, "0.0.0.0", () => {
    console.log(`[SightPortal Server] Live Access URL: ${liveUrl}`);
    console.log(`[SightPortal Server] Internal container binding active on http://0.0.0.0:${PORT}`);
    console.log(`[WebSocket Server] Socket handshaking available via ws://localhost:${PORT}/ws/:client_slug`);
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
      // Strip query parameters to match paths simply, or use full URL parser
      const urlObj = new URL(rawUrl, "http://localhost");
      const pathname = urlObj.pathname;
      console.log(`[WebSocket Upgrade] Received upgrade request at path: "${pathname}" (full: "${rawUrl}")`);

      // Match paths like /ws, /ws/, /ws/<client_slug>, /ws/<client_slug>/
      const wsPathPattern = /^\/ws(?:\/([^\/]+))?\/?$/;
      const match = pathname.match(wsPathPattern);

      if (match) {
        // extract client_slug from path, e.g. /ws/neon-nebula -> "neon-nebula"
        // or extract from query param `client` or `client_slug`, e.g. /ws?client=neon-nebula
        let client_slug = match[1] || urlObj.searchParams.get("client") || urlObj.searchParams.get("client_slug") || "hyperion-vis";
        
        console.log(`[WebSocket Upgrade] Upgrade handshake routing to client_slug: "${client_slug}"`);

        wss.handleUpgrade(request, sckt, head, (ws: any) => {
          ws.client_slug = client_slug; // Attach the client_slug metadata to this connection
          wss.emit("connection", ws, request);
        });
      } else {
        console.log(`[WebSocket Upgrade] Ignored upgrade path "${pathname}"`);
      }
    } catch (e) {
      console.error("[WebSocket Upgrade Error]", e);
    }
  });

  wss.on("connection", (ws: any) => {
    const client_slug = ws.client_slug || "hyperion-vis";
    console.log(`[WebSocket connection] Direct Unreal Plugin client linked successfully for client: '${client_slug}'! Sending initial payload attributes...`);
    
    // Ensure this client has an entry in clientsData
    if (!clientsData[client_slug]) {
      clientsData[client_slug] = {
        client_slug,
        target_class: "ArchViz Real-Estate Portfolio",
        attributes_matrix: clientsData["hyperion-vis"]?.attributes_matrix || []
      };
    }

    const data = clientsData[client_slug];

    // Immediately stream latest spreadsheet properties matching the active portfolio state (the client initiates, the server responds)
    ws.send(JSON.stringify({
      event: "init",
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: data.client_slug,
        target_class: data.target_class,
        attributes_matrix: data.attributes_matrix
      }
    }));

    // Listen for peer pings to keep connections alive and logs interactive
    ws.on("message", (rawMsg: any) => {
      try {
        const msgStr = rawMsg.toString();
        if (msgStr === "ping") {
          ws.send("pong");
          return;
        }

        const msgObj = JSON.parse(msgStr);
        console.log(`[WebSocket Received - ${client_slug}] Got socket event:`, msgObj);
        if (msgObj && msgObj.event === "ping") {
          ws.send(JSON.stringify({ event: "pong" }));
        }
      } catch (err) {
        // Fallback for simple message formats
      }
    });

    ws.on("close", () => {
      console.log(`[WebSocket disconnection] Plugin client link terminated for client: '${client_slug}'.`);
    });

    ws.on("error", (err: any) => {
      console.error(`[WebSocket error - ${client_slug}]`, err);
    });
  });
}

startServer();
