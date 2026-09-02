/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import { Client, Log, SpreadsheetData } from "./types";

// Pre-packaged spreadsheets corresponding to real Unreal Engine 5 use-cases.
// This allows testing the application instantly even without entering a live Google Sheet ID!
export const SPREADSHEET_TEMPLATES: Record<string, SpreadsheetData> = {
  "ArchViz Real-Estate Portfolio": {
    headers: ["Name", "Zone", "Block", "Class", "Door No", "Price", "Surface", "Availability", "BuildingSurface", "BedroomsCount", "BathroomsCount"],
    rows: [
      { Name: "Z1B11", Zone: "Z1", Block: "Z1B1", Class: "Villa Type A", "Door No": "1", Price: "1250000", Surface: "450.0", Availability: "Available", BuildingSurface: "350.0", BedroomsCount: "5", BathroomsCount: "4" },
      { Name: "Z1B12", Zone: "Z1", Block: "Z1B1", Class: "Penthouse Luxury", "Door No": "2", Price: "2890000", Surface: "280.0", Availability: "Available", BuildingSurface: "220.0", BedroomsCount: "3", BathroomsCount: "3" },
      { Name: "Z1B21", Zone: "Z1", Block: "Z1B2", Class: "Townhouse Modern", "Door No": "1", Price: "680000", Surface: "185.0", Availability: "Under Offer", BuildingSurface: "150.0", BedroomsCount: "4", BathroomsCount: "2" },
      { Name: "Z2B11", Zone: "Z2", Block: "Z2B1", Class: "Duplex Garden", "Door No": "1", Price: "450000", Surface: "120.0", Availability: "Sold", BuildingSurface: "100.0", BedroomsCount: "2", BathroomsCount: "2" },
    ],
  },
  "Virtual Camera & Rig Parameters": {
    headers: ["ActorName", "FocalLength", "Aperture", "FocusDistance", "TrackerID", "StabilizationPower", "VignetteIntensity", "IsRecording"],
    rows: [
      { ActorName: "CineCamera_A", FocalLength: "35.0", Aperture: "2.8", FocusDistance: "120.5", TrackerID: "VIVE_TRACKER_01", StabilizationPower: "0.85", VignetteIntensity: "0.15", IsRecording: "true" },
      { ActorName: "CineCamera_B", FocalLength: "85.0", Aperture: "1.4", FocusDistance: "340.0", TrackerID: "VIVE_TRACKER_02", StabilizationPower: "0.95", VignetteIntensity: "0.0", IsRecording: "false" },
    ],
  },
  "Material Swapping & Props Catalog": {
    headers: ["PropID", "BlueprintClass", "MaterialAssetPath", "GlossIntensity", "MetallicValue", "RoughnessFactor", "EmissiveBoost", "PlaySoundOnSpawn"],
    rows: [
      { PropID: "Hero_Sofa", BlueprintClass: "BP_InteractiveFurniture_C", MaterialAssetPath: "/Game/Materials/M_Leather_Cognac", GlossIntensity: "0.75", MetallicValue: "0.05", RoughnessFactor: "0.45", EmissiveBoost: "0.0", PlaySoundOnSpawn: "true" },
    ],
  },
};

export const DEFAULT_CLIENTS: Client[] = [
  {
    id: "hyperion-vis",
    name: "Hyperion ArchVis Group",
    company: "Hyperion Real Estate & visualization LLC",
    sheetId: "1tZp-u4cEunWqS8C8F3uW98vF8nK393J_i3d_8hH8q7g",
    sheetTab: "ArchVisProps",
    ue5Endpoint: "http://localhost:8012/api/ue5-stage",
    webSocketEndpoint: "ws://127.0.0.1:8009/ws/hyperion-vis",
    branding: {
      logoText: "HYPERION DIRECT",
      primaryColor: "#f59e0b", // Gorgeous Gold/Amber
      accentColor: "#0f766e",  // Dark Teal
      bgStyle: "clean",
      fontFamily: "sans",
    },
    updatedAt: "2026-06-05T14:24:00Z",
    sheetData: SPREADSHEET_TEMPLATES["ArchViz Real-Estate Portfolio"],
    bugs: [
      {
        id: "bug-101",
        title: "Villa Bella Vista Light Leak at Master Bedroom window frame",
        description: "Ceiling junctions exhibit intense light leakage when loading properties containing heavy architectural volume inputs from sheet columns during noon cycles.",
        severity: "High",
        status: "In Progress",
        createdAt: "2026-06-05T08:30:00Z",
        updatedAt: "2026-06-06T07:15:00Z",
        mediaName: "MasterBedroomLeaks.png",
        mediaType: "image",
        mediaUrl: "https://images.unsplash.com/photo-1600210492486-724fe5c67fb0?auto=format&fit=crop&w=400&q=80",
        activities: [
          {
            id: "act-101a",
            timestamp: "2026-06-05T08:30:00Z",
            message: "New design bug tracked matching villa coordinates schema. Notification dispatched.",
            user: "raed.sight@gmail.com"
          },
          {
            id: "act-101b",
            timestamp: "2026-06-06T07:15:00Z",
            message: "Status updated from Open to In Progress. Checked light map coordinates for window meshes.",
            user: "raed.sight@gmail.com"
          }
        ]
      },
      {
        id: "bug-102",
        title: "Penthouse SkyLine door collision alignment issue",
        description: "The 3D actors list is blocked by door collision meshes at Wall Street Penthouse corridor during character sweep testing.",
        severity: "Medium",
        status: "Open",
        createdAt: "2026-06-06T04:10:00Z",
        updatedAt: "2026-06-06T04:10:00Z",
        mediaName: "Penthouse_Stuck.png",
        mediaType: "image",
        mediaUrl: "https://images.unsplash.com/photo-1600607687939-ce8a6c25118c?auto=format&fit=crop&w=400&q=80",
        activities: [
          {
            id: "act-102a",
            timestamp: "2026-06-06T04:10:00Z",
            message: "Initial bug registered with standard floor coordinate logs.",
            user: "Client (Hyperion Team)"
          }
        ]
      }
    ],
    mediaResources: [
      {
        id: "med-101",
        category: "project",
        title: "Masterplan Sunset Aerial View",
        description: "General high-fidelity 4K overview render of the entire master development and coastal waterfront perimeter.",
        url: "https://images.unsplash.com/photo-1545324418-cc1a3fa10c00?auto=format&fit=crop&w=1920&q=85",
        fileName: "Masterplan_Sunset_4K.jpg",
        fileSize: 3420000,
        dimensions: { width: 3840, height: 2160 },
        resolutionTag: "4K UHD",
        tags: ["Masterplan", "Aerial", "Sunset", "Overview"],
        uploadedAt: "2026-06-05T10:00:00Z"
      },
      {
        id: "med-102",
        category: "project",
        title: "Grand Entrance Boulevard & Water Cascade",
        description: "Main arrival gateway and architectural fountain plaza leading to residential sectors.",
        url: "https://images.unsplash.com/photo-1600585154340-be6161a56a0c?auto=format&fit=crop&w=1920&q=85",
        fileName: "Grand_Boulevard_Entrance.jpg",
        fileSize: 2890000,
        dimensions: { width: 3840, height: 2160 },
        resolutionTag: "4K UHD",
        tags: ["Entrance", "Boulevard", "Landscape"],
        uploadedAt: "2026-06-05T11:30:00Z"
      },
      {
        id: "med-103",
        category: "services",
        title: "Central Clubhouse & Infinity Pool Deck",
        description: "Service building facility housing the private members lounge, poolside cabanas, and indoor cafe.",
        serviceName: "Clubhouse & Leisure Hub",
        url: "https://images.unsplash.com/photo-1576013551627-0cc20b96c2a7?auto=format&fit=crop&w=1920&q=85",
        fileName: "Clubhouse_Infinity_Pool.jpg",
        fileSize: 2450000,
        dimensions: { width: 2560, height: 1440 },
        resolutionTag: "2K QHD",
        tags: ["Clubhouse", "Pool", "Service Facility"],
        uploadedAt: "2026-06-05T14:15:00Z"
      },
      {
        id: "med-104",
        category: "services",
        title: "Holistic Wellness Spa & Fitness Center",
        description: "Dedicated service building containing sauna suites, hydrotherapy pools, and gym floor.",
        serviceName: "Wellness Spa Pavilion",
        url: "https://images.unsplash.com/photo-1540555700478-4be289fbecef?auto=format&fit=crop&w=1920&q=85",
        fileName: "Wellness_Spa_Pavilion.jpg",
        fileSize: 2180000,
        dimensions: { width: 1920, height: 1080 },
        resolutionTag: "1080p FHD",
        tags: ["Spa", "Fitness", "Service Building"],
        uploadedAt: "2026-06-05T15:00:00Z"
      },
      {
        id: "med-105",
        category: "properties",
        title: "Villa Bella Vista Panoramic Living Salon",
        description: "Double-height living pavilion with floor-to-ceiling motorized glazing overlooking the private garden.",
        propertyClass: "Villa Type A",
        propertyName: "Z1B11",
        url: "https://images.unsplash.com/photo-1600210492486-724fe5c67fb0?auto=format&fit=crop&w=1920&q=85",
        fileName: "VillaTypeA_LivingRoom.jpg",
        fileSize: 3120000,
        dimensions: { width: 3840, height: 2160 },
        resolutionTag: "4K UHD",
        tags: ["Villa Type A", "Interior", "Living Room", "Luxury"],
        uploadedAt: "2026-06-06T08:00:00Z"
      },
      {
        id: "med-106",
        category: "properties",
        title: "Penthouse Horizon Sky Deck & Jacuzzi",
        description: "Rooftop entertainment terrace with glass balustrade and unobstructed 360-degree city views.",
        propertyClass: "Penthouse Luxury",
        propertyName: "Z1B12",
        url: "https://images.unsplash.com/photo-1600607687939-ce8a6c25118c?auto=format&fit=crop&w=1920&q=85",
        fileName: "Penthouse_SkyDeck.jpg",
        fileSize: 2950000,
        dimensions: { width: 2560, height: 1440 },
        resolutionTag: "2K QHD",
        tags: ["Penthouse Luxury", "Sky Deck", "Terrace"],
        uploadedAt: "2026-06-06T09:30:00Z"
      }
    ]
  },
  {
    id: "neon-nebula",
    name: "Neon Nebula Productions",
    company: "Nebula Cinematic Studios Ltd",
    sheetId: "1BxiMVs0XRA5nFMdKv1aM9ldm5i-YSgcbL1g6xGoS18A",
    sheetTab: "Sheet1",
    ue5Endpoint: "http://127.0.0.1:8008/remote/object/call",
    webSocketEndpoint: "ws://127.0.0.1:8009/ws/neon-nebula",
    branding: {
      logoText: "NEBULA V-STAGE",
      primaryColor: "#d946ef", // Vibrant Magenta
      accentColor: "#3b82f6",  // Blue
      bgStyle: "cyber",
      fontFamily: "grotesk",
    },
    updatedAt: "2026-06-06T07:00:00Z",
    sheetData: SPREADSHEET_TEMPLATES["Virtual Camera & Rig Parameters"],
    bugs: []
  },
  {
    id: "overlord-egames",
    name: "Overlord Stadium",
    company: "Overlord Interactive Esports Esports Ltd",
    sheetId: "1_8jK6u9KjS_sQfF99G-q6hVv8l2pEq5L_g48q61zZ0g",
    sheetTab: "MainSpawns",
    ue5Endpoint: "http://localhost:8008/remote/object/call",
    webSocketEndpoint: "ws://127.0.0.1:8009/ws/overlord-egames",
    branding: {
      logoText: "OVERLORD ENGAGE",
      primaryColor: "#06b6d4", // Sharp Cyan
      accentColor: "#10b981",  // Emerlad
      bgStyle: "dark",
      fontFamily: "mono",
    },
    updatedAt: "2026-06-06T06:45:12Z",
    sheetData: SPREADSHEET_TEMPLATES["Material Swapping & Props Catalog"],
    bugs: []
  },
];

export const DEFAULT_LOGS: Log[] = [
  {
    id: "log-1",
    clientId: "neon-nebula",
    clientName: "Neon Nebula Productions",
    timestamp: "2026-06-06T07:01:15Z",
    type: "fetch_sheet",
    status: "success",
    details: "Successfully parsed CineCamera Google Sheet with 4 coordinate datasets",
    payload: '{"cols":["ActorName","FocalLength","Aperture","FocusDistance"],"row_count":4}',
  },
  {
    id: "log-2",
    clientId: "neon-nebula",
    clientName: "Neon Nebula Productions",
    timestamp: "2026-06-06T07:01:17Z",
    type: "ue5_push",
    status: "success",
    details: "Dispatched HTTP sequence payload to Unreal Engine 5 CineCamera controller",
    payload: '{"endpoint":"http://127.0.0.1:8008/remote/object/call","bytes_sent":512,"status_code":200,"body":{"success":true,"triggered_actors":["CineCamera_A","CineCamera_B","CraneCamera_Main"]}}',
  },
  {
    id: "log-3",
    clientId: "hyperion-vis",
    clientName: "Hyperion ArchVis Group",
    timestamp: "2026-06-05T14:24:00Z",
    type: "config_change",
    status: "success",
    details: "Admin updated client theme and changed background style from Dark to Clean Slate Minimal",
    payload: '{"new_branding":{"primaryColor":"#f59e0b","accentColor":"#0f766e","bgStyle":"clean","fontFamily":"sans"}}',
  },
  {
    id: "log-4",
    clientId: "overlord-egames",
    clientName: "Overlord Stadium",
    timestamp: "2026-06-06T06:45:10Z",
    type: "fetch_sheet",
    status: "warning",
    details: "Importing Google sheet fell back to local editable cached snapshot (CORS warning / Private Sheet ID)",
    payload: '{"sheet_id":"1_8jK6u9KjS_sQfF99G-q6hVv8l2pEq5L_g48q61zZ0g","status":"warning_fallback"}',
  },
];

// LocalStorage helpers to allow state persistence across reloads/rebuilds
export function getStoredClients(): Client[] {
  try {
    const data = localStorage.getItem("ue5_bridge_clients");
    if (data) {
      return JSON.parse(data);
    }
  } catch (e) {
    console.error("Error reading clients from local storage", e);
  }
  return DEFAULT_CLIENTS;
}

export function saveStoredClients(clients: Client[]): void {
  try {
    localStorage.setItem("ue5_bridge_clients", JSON.stringify(clients));
  } catch (e) {
    console.error("Error writing clients to local storage", e);
  }
}

export function getStoredLogs(): Log[] {
  try {
    const data = localStorage.getItem("ue5_bridge_logs2");
    if (data) {
      return JSON.parse(data);
    }
  } catch (e) {
    console.error("Error reading logs from local storage", e);
  }
  return DEFAULT_LOGS;
}

export function saveStoredLogs(logs: Log[]): void {
  try {
    localStorage.setItem("ue5_bridge_logs2", JSON.stringify(logs));
  } catch (e) {
    console.error("Error writing logs to local storage", e);
  }
}

// Map spreadsheet client custom rows in local storage to prevent loss of editing state!
export function getStoredClientSheet(clientId: string, defaultPresetName: string): SpreadsheetData {
  try {
    const data = localStorage.getItem(`ue5_sheet_state_${clientId}`);
    if (data) {
      return JSON.parse(data);
    }
  } catch (e) {
    console.error("Error browsing custom sheets from local storage", e);
  }
  
  // Hand back appropriate default preset
  return SPREADSHEET_TEMPLATES[defaultPresetName] || SPREADSHEET_TEMPLATES["Virtual Camera & Rig Parameters"];
}

export function saveStoredClientSheet(clientId: string, data: SpreadsheetData): void {
  try {
    localStorage.setItem(`ue5_sheet_state_${clientId}`, JSON.stringify(data));
  } catch (e) {
    console.error("Error saving custom sheet", e);
  }
}

// Convert google sheet URLs into direct JSON export links or extract Spreadsheet IDs
export function extractSpreadsheetId(inputString: string): { sheetId: string; tabId: string | null } {
  let sheetId = inputString.trim();
  let tabId: string | null = null;
  
  // Check if it's a full Google Docs spreadsheet URL
  const sheetUrlRegex = /\/spreadsheets\/d\/([a-zA-Z0-9-_]+)/;
  const match = sheetId.match(sheetUrlRegex);
  if (match && match[1]) {
    sheetId = match[1];
  }
  
  // Extract custom tab identifier (e.g. #gid=12344)
  const gidRegex = /[#&]gid=([0-9]+)/;
  const gidMatch = inputString.match(gidRegex);
  if (gidMatch && gidMatch[1]) {
    tabId = gidMatch[1];
  }
  
  return { sheetId, tabId };
}
