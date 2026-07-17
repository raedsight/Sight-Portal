/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

export type BgStyleType = "dark" | "light" | "cyber" | "clean";

export interface BrandingTheme {
  logoUrl?: string;
  logoText: string;
  primaryColor: string; // e.g. "#ec4899"
  accentColor: string;  // e.g. "#a855f7"
  bgStyle: BgStyleType;
  fontFamily: "sans" | "mono" | "grotesk";
}

export interface ThemePreset {
  id: string;
  name: string;
  branding: BrandingTheme;
  updatedAt: string;
}

export interface BugActivity {
  id: string;
  timestamp: string;
  message: string;
  user: string;
}

export interface BugIssue {
  id: string;
  title: string;
  description: string;
  severity: "Low" | "Medium" | "High" | "Critical";
  status: "Open" | "In Progress" | "Resolved" | "Closed";
  createdAt: string;
  updatedAt: string;
  mediaUrl?: string; // base64 payload or custom path
  mediaType?: "image" | "video";
  mediaName?: string;
  activities: BugActivity[];
}

export interface Client {
  id: string;
  name: string;
  company: string;
  sheetId: string;       // Google Sheet ID or Full URL
  sheetTab: string;      // E.g., GridId/TabName "Sheet1"
  ue5Endpoint: string;   // E.g. "http://localhost:8008/remote/object/call"
  branding: BrandingTheme;
  updatedAt: string;     // ISO timestamp
  bugs?: BugIssue[];     // Optional list of tracked bug issues
}

export interface Log {
  id: string;
  clientId: string;
  clientName: string;
  timestamp: string;     // ISO timestamp
  type: "fetch_sheet" | "ue5_push" | "config_change" | "error";
  status: "success" | "warning" | "error";
  details: string;
  payload?: string;      // Detailed CSV, JSON or Response data preview
}

export interface SheetRow {
  [key: string]: string;
}

export interface SpreadsheetData {
  headers: string[];
  rows: SheetRow[];
}
