import express from "express";
import path from "path";
import { createServer as createViteServer } from "vite";
import { WebSocketServer, WebSocket } from "ws";
import { 
  sendBugNotificationEmail, 
  DEFAULT_ADMIN_EMAIL, 
  getEmailAuditHistory 
} from "./src/serverEmailNotifier";
import { BugNotificationPayload } from "./src/types";

async function startServer() {
  const app = express();
  const PORT = 3000;

  // JSON parsing and large coordinate matrices and image base64 payloads helper setting
  app.use(express.json({ limit: "50mb" }));
  app.use(express.urlencoded({ extended: true, limit: "50mb" }));

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
          Name: "Z1B11",
          Zone: "Z1",
          Block: "Z1B1",
          "Door No": "1",
          Price: "1250000",
          Surface: "450.0",
          Availability: "Available",
          BuildingSurface: "350.0",
          BedroomsCount: "5",
          BathroomsCount: "4",
          Class: "Residential"
        },
        {
          Name: "Z1B12",
          Zone: "Z1",
          Block: "Z1B1",
          "Door No": "2",
          Price: "2890000",
          Surface: "280.0",
          Availability: "Available",
          BuildingSurface: "220.0",
          BedroomsCount: "3",
          BathroomsCount: "3",
          Class: "Residential"
        },
        {
          Name: "Z1B21",
          Zone: "Z1",
          Block: "Z1B2",
          "Door No": "1",
          Price: "680000",
          Surface: "185.0",
          Availability: "Under Offer",
          BuildingSurface: "150.0",
          BedroomsCount: "4",
          BathroomsCount: "2",
          Class: "Residential"
        },
        {
          Name: "Z2B11",
          Zone: "Z2",
          Block: "Z2B1",
          "Door No": "1",
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
  const broadcastSync = (event: "init" | "update", client_slug: string, extraPayload?: any) => {
    const data = clientsData[client_slug] || clientsData["hyperion-vis"];
    const payloadObject: Record<string, any> = {
      event,
      timestamp: new Date().toISOString(),
      payload: {
        client_slug: data.client_slug,
        target_class: data.target_class,
        attributes_matrix: data.attributes_matrix
      }
    };

    if (extraPayload) {
      payloadObject.updated_row = extraPayload;
      payloadObject.data = extraPayload.data || extraPayload;
      payloadObject.property = extraPayload.property || extraPayload.Name || extraPayload.name;
    }

    const payloadString = JSON.stringify(payloadObject);

    let clientsCount = 0;
    wss.clients.forEach((client: any) => {
      if (client.readyState === WebSocket.OPEN) {
        // Broadcast to matching client_slug or default instances
        if (
          !client.client_slug ||
          client.client_slug === client_slug || 
          client.client_slug === "hyperion-vis" ||
          client_slug === "hyperion-vis"
        ) {
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
        if (!client.client_slug || client.client_slug === slug || client.client_slug === "hyperion-vis") {
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
    const { client_slug, target_class, attributes_matrix, updated_row } = req.body;
    
    if (attributes_matrix && Array.isArray(attributes_matrix)) {
      const slug = client_slug || "hyperion-vis";
      clientsData[slug] = {
        client_slug: slug,
        target_class: target_class || "ArchViz Real-Estate Portfolio",
        attributes_matrix
      };
      
      console.log(`[Full-Stack API] State synchronized for client '${slug}' - ${attributes_matrix.length} rows updated.`);
      
      // Real-time Push to all active WebSockets/Connected Plugin threads directly
      broadcastSync("update", slug, updated_row);

      return res.json({ success: true, message: `Spreadsheet records successfully cached on backend server for client '${slug}'` });
    }
    
    return res.status(400).json({ error: "Invalid payload layout structure: missing 'attributes_matrix' array" });
  });

  // API Endpoint 3b: Dedicated single property real-time update endpoint
  app.post("/api/update-property", (req, res) => {
    const { client_slug, property_name, property_data } = req.body;
    const slug = client_slug || "hyperion-vis";

    if (!clientsData[slug]) {
      clientsData[slug] = {
        client_slug: slug,
        target_class: "ArchViz Real-Estate Portfolio",
        attributes_matrix: []
      };
    }

    const matrix = clientsData[slug].attributes_matrix;
    const targetName = property_name || property_data?.Name || property_data?.name;
    
    const idx = matrix.findIndex((r: any) => 
      (r.Name && r.Name === targetName) || 
      (r.ActorName && r.ActorName === targetName) || 
      (r.PropID && r.PropID === targetName)
    );

    if (idx !== -1) {
      matrix[idx] = { ...matrix[idx], ...property_data };
    } else if (targetName) {
      matrix.push({ Name: targetName, ...property_data });
    }

    console.log(`[Full-Stack API] Property '${targetName}' updated live for client '${slug}'. Broadcasting update to Unreal Engine...`);
    broadcastSync("update", slug, { property: targetName, data: property_data });

    res.json({
      success: true,
      message: `Property '${targetName}' updated and instantly pushed to Unreal Engine.`,
      payload: matrix
    });
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

  // -------------------------------------------------------------
  // AUTOMATED ADMIN EMAIL NOTIFICATION DISPATCHER FOR BUGS
  // Sends notification email to admin (raed.sight@gmail.com) on bug report or update
  // -------------------------------------------------------------
  app.post("/api/notify-bug", async (req, res) => {
    try {
      const payload = req.body as BugNotificationPayload;
      if (!payload || !payload.client || !payload.bug) {
        return res.status(400).json({ error: "Missing required 'client' or 'bug' payload object." });
      }

      // Infer current app URL if not provided
      const host = req.get("host") || "localhost:3000";
      const protocol = req.protocol === "https" || req.headers["x-forwarded-proto"] === "https" ? "https" : "http";
      const currentAppUrl = process.env.APP_URL || `${protocol}://${host}`;
      payload.appUrl = payload.appUrl || currentAppUrl;

      console.log(`[Bug Notification Endpoint] Action: ${payload.action} on Bug #${payload.bug.id} (${payload.bug.title}) for client '${payload.client.company}'`);

      const result = await sendBugNotificationEmail(payload);
      return res.json(result);
    } catch (err: any) {
      console.error("[Bug Notification Endpoint Error]", err);
      return res.status(500).json({ 
        success: false, 
        error: err.message || "Failed to dispatch bug notification email",
        timestamp: new Date().toISOString()
      });
    }
  });

  // Endpoint to send a test email to admin's address
  app.post("/api/notify-test-email", async (req, res) => {
    try {
      const { targetEmail } = req.body;
      const recipient = targetEmail || DEFAULT_ADMIN_EMAIL;

      const host = req.get("host") || "localhost:3000";
      const protocol = req.protocol === "https" || req.headers["x-forwarded-proto"] === "https" ? "https" : "http";
      const currentAppUrl = process.env.APP_URL || `${protocol}://${host}`;

      const testPayload: BugNotificationPayload = {
        action: "created",
        client: {
          id: "hyperion-vis",
          name: "Hyperion Real Estate Test",
          company: "Hyperion Luxury Living",
        },
        bug: {
          id: `test-${Date.now().toString().slice(-4)}`,
          title: "Sample ArchViz Test Bug Notification",
          description: "This is a live test notification verifying that the automated bug email dispatcher is properly connected and functioning for Sight Portal.",
          severity: "High",
          status: "Open",
          createdAt: new Date().toISOString(),
          updatedAt: new Date().toISOString(),
          activities: [
            {
              id: "act-test",
              timestamp: new Date().toISOString(),
              message: "System self-test executed by administrator.",
              user: "Sight Portal QA Automation",
            }
          ]
        },
        adminEmails: [recipient],
        appUrl: currentAppUrl
      };

      const result = await sendBugNotificationEmail(testPayload);
      return res.json({
        ...result,
        test: true,
        message: `Test email successfully dispatched to ${recipient}`,
      });
    } catch (err: any) {
      return res.status(500).json({ error: err.message || "Test email dispatch failed" });
    }
  });

  // Endpoint to fetch email audit history
  app.get("/api/notify-audit-history", (req, res) => {
    return res.json({
      history: getEmailAuditHistory(),
      defaultAdminEmail: DEFAULT_ADMIN_EMAIL,
    });
  });

  // Endpoint to inspect notification configuration
  app.get("/api/notify-config", (req, res) => {
    return res.json({
      adminEmail: DEFAULT_ADMIN_EMAIL,
      hasSmtp: Boolean(process.env.SMTP_HOST && process.env.SMTP_USER),
      hasResend: Boolean(process.env.RESEND_API_KEY),
      mode: process.env.SMTP_HOST ? "smtp" : (process.env.RESEND_API_KEY ? "resend" : "test/ethereal"),
    });
  });

  // -------------------------------------------------------------
  // GOOGLE DRIVE FULL-STACK API PROXY ENDPOINTS
  // Proxies client-side requests server-to-server to avoid browser CORS and iframe sandbox restrictions
  // -------------------------------------------------------------
  const ROOT_DRIVE_FOLDER_ID = "13fE2R_-qxOMlT2U7tY1NtK36xpwahMkg";

  // Helper to extract bearer token from request
  const getAuthToken = (req: express.Request): string | null => {
    const authHeader = req.headers.authorization;
    if (authHeader && authHeader.startsWith("Bearer ")) {
      return authHeader.slice(7).trim();
    }
    return null;
  };

  // Helper to parse Google Drive API error responses cleanly and detect disabled API
  const parseDriveApiError = (status: number, rawText: string) => {
    let parsed: any = null;
    try {
      parsed = typeof rawText === "string" ? JSON.parse(rawText) : rawText;
    } catch (e) {
      // raw text
    }

    const rawMsg = parsed?.error?.message || (typeof rawText === "string" ? rawText : "");
    const isServiceDisabled =
      status === 403 &&
      (rawMsg.includes("Google Drive API has not been used in project") ||
        rawMsg.includes("SERVICE_DISABLED") ||
        rawMsg.includes("accessNotConfigured") ||
        parsed?.error?.details?.some((d: any) => d.reason === "SERVICE_DISABLED" || d.metadata?.reason === "SERVICE_DISABLED"));

    // Extract project ID from error message or activation link if available
    let detectedProject = "sodium-icon-v8gvj";
    const projectMatch = rawMsg.match(/project[=\s/]+([0-9a-zA-Z\-_]+)/i);
    if (projectMatch && projectMatch[1]) {
      detectedProject = projectMatch[1];
    }

    let activationUrl = `https://console.developers.google.com/apis/api/drive.googleapis.com/overview?project=${detectedProject}`;

    if (parsed?.error?.details) {
      const detailObj = parsed.error.details.find((d: any) => d.metadata?.activationUrl || d.links?.some((l: any) => l.url));
      if (detailObj?.metadata?.activationUrl) {
        activationUrl = detailObj.metadata.activationUrl;
      } else if (detailObj?.links?.[0]?.url) {
        activationUrl = detailObj.links[0].url;
      }
    }

    if (activationUrl.includes("project=")) {
      const urlProj = activationUrl.split("project=")[1]?.split("&")[0];
      if (urlProj) detectedProject = urlProj;
    }

    if (!activationUrl && rawMsg) {
      const match = rawMsg.match(/https:\/\/console\.developers\.google\.com\/apis\/api\/drive\.googleapis\.com\/[^\s]+/);
      if (match) {
        activationUrl = match[0].replace(/[.,;]+$/, "");
      }
    }

    const cleanMessage = isServiceDisabled
      ? `Google Drive API is disabled in your Google Cloud project (${detectedProject}). Please enable it in the Google Cloud Console.`
      : (parsed?.error?.message || rawMsg || `Google Drive API error (${status})`);

    return {
      status,
      isServiceDisabled,
      activationUrl,
      message: cleanMessage,
      projectId: detectedProject,
    };
  };

  // Endpoint: Provision or locate client dedicated folder
  app.post("/api/drive/provision-folder", async (req, res) => {
    const token = getAuthToken(req);
    if (!token) {
      return res.status(401).json({ error: "Google OAuth access token missing from Authorization header" });
    }

    const { client } = req.body;
    if (!client) {
      return res.status(400).json({ error: "Missing client parameter in request body" });
    }

    const targetFolderName = `${client.company || client.name} (${client.id})`;

    try {
      // 1. Check if existing folder ID is valid
      if (client.driveFolderId) {
        try {
          const checkRes = await fetch(
            `https://www.googleapis.com/drive/v3/files/${client.driveFolderId}?fields=id,name,webViewLink,trashed`,
            { headers: { Authorization: `Bearer ${token}` } }
          );
          if (checkRes.ok) {
            const fileData = (await checkRes.json()) as any;
            if (!fileData.trashed) {
              return res.json({
                folderId: fileData.id,
                folderUrl: fileData.webViewLink || `https://drive.google.com/drive/folders/${fileData.id}`,
                folderName: fileData.name || targetFolderName,
              });
            }
          } else {
            const checkErrText = await checkRes.text();
            const errInfo = parseDriveApiError(checkRes.status, checkErrText);
            if (errInfo.isServiceDisabled) {
              return res.status(403).json({
                error: errInfo.message,
                isServiceDisabled: true,
                activationUrl: errInfo.activationUrl,
                projectId: errInfo.projectId,
              });
            }
          }
        } catch (checkErr) {
          console.warn("[Drive Proxy] Existing folder check notice:", checkErr);
        }
      }

      // 2. Query shared root folder
      const query = encodeURIComponent(
        `'${ROOT_DRIVE_FOLDER_ID}' in parents and mimeType = 'application/vnd.google-apps.folder' and trashed = false`
      );
      const listRes = await fetch(
        `https://www.googleapis.com/drive/v3/files?q=${query}&fields=files(id,name,webViewLink)&pageSize=100`,
        { headers: { Authorization: `Bearer ${token}` } }
      );

      if (listRes.ok) {
        const listData = (await listRes.json()) as any;
        const existingFolder = (listData.files || []).find(
          (f: any) =>
            f.name.toLowerCase() === targetFolderName.toLowerCase() ||
            f.name.toLowerCase().includes(`(${client.id.toLowerCase()})`) ||
            (client.driveFolderId && f.id === client.driveFolderId)
        );

        if (existingFolder) {
          return res.json({
            folderId: existingFolder.id,
            folderUrl: existingFolder.webViewLink || `https://drive.google.com/drive/folders/${existingFolder.id}`,
            folderName: existingFolder.name,
          });
        }
      } else {
        const listErrText = await listRes.text();
        const errInfo = parseDriveApiError(listRes.status, listErrText);
        if (errInfo.isServiceDisabled) {
          return res.status(403).json({
            error: errInfo.message,
            isServiceDisabled: true,
            activationUrl: errInfo.activationUrl,
            projectId: errInfo.projectId,
          });
        }
      }

      // 3. Create dedicated folder inside root directory
      const createRes = await fetch("https://www.googleapis.com/drive/v3/files?fields=id,name,webViewLink", {
        method: "POST",
        headers: {
          Authorization: `Bearer ${token}`,
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          name: targetFolderName,
          mimeType: "application/vnd.google-apps.folder",
          parents: [ROOT_DRIVE_FOLDER_ID],
          description: `Dedicated media repository for client ${client.name} (${client.company})`,
        }),
      });

      if (!createRes.ok) {
        const errText = await createRes.text();
        const errInfo = parseDriveApiError(createRes.status, errText);
        return res.status(createRes.status).json({
          error: errInfo.message,
          isServiceDisabled: errInfo.isServiceDisabled,
          activationUrl: errInfo.activationUrl,
          projectId: errInfo.projectId,
        });
      }

      const newFolder = (await createRes.json()) as any;
      return res.json({
        folderId: newFolder.id,
        folderUrl: newFolder.webViewLink || `https://drive.google.com/drive/folders/${newFolder.id}`,
        folderName: newFolder.name,
      });
    } catch (err: any) {
      console.error("[Drive Proxy Provision Error]", err);
      return res.status(500).json({ error: err.message || "Internal server error provisioning Google Drive folder" });
    }
  });

  // Endpoint: Upload file directly to Google Drive
  app.post("/api/drive/upload", async (req, res) => {
    const token = getAuthToken(req);
    if (!token) {
      return res.status(401).json({ error: "Google OAuth access token missing from Authorization header" });
    }

    const { fileName, mimeType, dataUrl, base64Data, url, folderId, description } = req.body;

    if (!fileName) {
      return res.status(400).json({ error: "Missing required 'fileName'" });
    }

    try {
      let fileBuffer: Buffer | null = null;
      let effectiveMime = mimeType || "image/jpeg";

      if (base64Data) {
        fileBuffer = Buffer.from(base64Data, "base64");
      } else if (dataUrl && dataUrl.startsWith("data:")) {
        const commaIdx = dataUrl.indexOf(",");
        if (commaIdx !== -1) {
          const header = dataUrl.substring(0, commaIdx);
          const mimeMatch = header.match(/:(.*?);/);
          if (mimeMatch) effectiveMime = mimeMatch[1];
          fileBuffer = Buffer.from(dataUrl.substring(commaIdx + 1), "base64");
        }
      } else if (url && (url.startsWith("http://") || url.startsWith("https://"))) {
        const imgRes = await fetch(url);
        if (imgRes.ok) {
          fileBuffer = Buffer.from(await imgRes.arrayBuffer());
          const cType = imgRes.headers.get("content-type");
          if (cType) effectiveMime = cType.split(";")[0];
        } else {
          return res.status(400).json({ error: `Could not fetch remote media from URL: ${url}` });
        }
      }

      if (!fileBuffer || fileBuffer.length === 0) {
        return res.status(400).json({ error: "No valid image payload provided (base64Data, dataUrl, or URL required)" });
      }

      const parentFolder = folderId || ROOT_DRIVE_FOLDER_ID;
      const metadata = {
        name: fileName,
        parents: [parentFolder],
        description: description || "Uploaded via Client Portal Media Center",
      };

      const boundary = `-------SightPortalUpload_${Date.now()}`;
      const delimiter = `\r\n--${boundary}\r\n`;
      const closeDelimiter = `\r\n--${boundary}--`;

      const metadataPart = Buffer.from(
        delimiter + "Content-Type: application/json; charset=UTF-8\r\n\r\n" + JSON.stringify(metadata) + "\r\n"
      );
      const fileHeader = Buffer.from(delimiter + `Content-Type: ${effectiveMime}\r\n\r\n`);
      const closing = Buffer.from(closeDelimiter);

      const multipartBody = Buffer.concat([metadataPart, fileHeader, fileBuffer, closing]);

      const driveRes = await fetch(
        "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id,name,webViewLink,thumbnailLink,webContentLink",
        {
          method: "POST",
          headers: {
            Authorization: `Bearer ${token}`,
            "Content-Type": `multipart/related; boundary=${boundary}`,
            "Content-Length": String(multipartBody.length),
          },
          body: multipartBody,
        }
      );

      if (!driveRes.ok) {
        const errText = await driveRes.text();
        const errInfo = parseDriveApiError(driveRes.status, errText);
        console.error(`[Drive Proxy Upload Failed] HTTP ${driveRes.status}:`, errInfo.message);
        return res.status(driveRes.status).json({
          error: errInfo.message,
          isServiceDisabled: errInfo.isServiceDisabled,
          activationUrl: errInfo.activationUrl,
          projectId: errInfo.projectId,
        });
      }

      const data = (await driveRes.json()) as any;
      return res.json({
        fileId: data.id,
        webViewLink: data.webViewLink,
        thumbnailLink: data.thumbnailLink,
        webContentLink: data.webContentLink,
      });
    } catch (err: any) {
      console.error("[Drive Proxy Upload Error]", err);
      return res.status(500).json({ error: err.message || "Failed to upload file to Google Drive" });
    }
  });

  // Endpoint: Query folder files
  app.get("/api/drive/files", async (req, res) => {
    const token = getAuthToken(req);
    if (!token) {
      return res.status(401).json({ error: "Google OAuth access token missing from Authorization header" });
    }

    const folderId = req.query.folderId as string;
    if (!folderId) {
      return res.status(400).json({ error: "Missing required 'folderId' parameter" });
    }

    try {
      const query = encodeURIComponent(`'${folderId}' in parents and trashed = false`);
      const listRes = await fetch(
        `https://www.googleapis.com/drive/v3/files?q=${query}&fields=files(id,name,mimeType,webViewLink,thumbnailLink,size,createdTime)&orderBy=createdTime desc&pageSize=100`,
        { headers: { Authorization: `Bearer ${token}` } }
      );

      if (!listRes.ok) {
        const errText = await listRes.text();
        const errInfo = parseDriveApiError(listRes.status, errText);
        return res.status(listRes.status).json({
          error: errInfo.message,
          isServiceDisabled: errInfo.isServiceDisabled,
          activationUrl: errInfo.activationUrl,
          projectId: errInfo.projectId,
        });
      }

      const data = (await listRes.json()) as any;
      return res.json({ files: data.files || [] });
    } catch (err: any) {
      console.error("[Drive Proxy Files Error]", err);
      return res.status(500).json({ error: err.message || "Failed to list Google Drive files" });
    }
  });

  // Endpoint: Grant folder permissions to client email
  app.post("/api/drive/share-folder", async (req, res) => {
    const token = getAuthToken(req);
    if (!token) {
      return res.status(401).json({ error: "Google OAuth access token missing from Authorization header" });
    }

    const { folderId, clientEmail, role } = req.body;
    if (!folderId || !clientEmail) {
      return res.status(400).json({ error: "Missing required 'folderId' or 'clientEmail'" });
    }

    try {
      const shareRes = await fetch(`https://www.googleapis.com/drive/v3/files/${folderId}/permissions`, {
        method: "POST",
        headers: {
          Authorization: `Bearer ${token}`,
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          role: role || "reader",
          type: "user",
          emailAddress: clientEmail,
        }),
      });

      if (!shareRes.ok) {
        const errText = await shareRes.text();
        console.warn(`[Drive Proxy Share Notice] HTTP ${shareRes.status}:`, errText);
        return res.status(shareRes.status).json({ error: errText });
      }

      return res.json({ success: true });
    } catch (err: any) {
      console.warn("[Drive Proxy Share Error]", err);
      return res.status(500).json({ error: err.message || "Failed to share Google Drive folder" });
    }
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
