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

export interface BugNotificationPayload {
  action: "created" | "updated" | "commented";
  client: {
    id: string;
    name: string;
    company: string;
  };
  bug: BugIssue;
  updateDetails?: {
    user?: string;
    message?: string;
    previousStatus?: string;
    newStatus?: string;
  };
  adminEmails?: string[];
  appUrl?: string;
}

export interface BugNotificationResult {
  success: boolean;
  message: string;
  recipient: string;
  subject: string;
  previewUrl?: string;
  mode: "smtp" | "resend" | "ethereal" | "logged";
  timestamp: string;
}

export type MediaCategory = "project" | "services" | "properties";

export interface MediaResource {
  id: string;
  category: MediaCategory;
  title: string;
  description?: string;
  url: string; // Base64 data URI or HTTP URL
  fileName: string;
  fileSize?: number; // In bytes
  dimensions?: {
    width: number;
    height: number;
  };
  resolutionTag?: string; // E.g., "4K UHD", "2K QHD", "1080p FHD"
  propertyClass?: string; // Automatically selected Property Class from the Sheet
  propertyName?: string;  // Optional specific property unit / name
  serviceName?: string;   // For 'services' category: service building name
  tags?: string[];
  uploadedAt: string;     // ISO timestamp
  driveFileId?: string;   // Google Drive File ID
  driveWebViewLink?: string; // Direct Google Drive View Link
  driveThumbnailLink?: string; // Google Drive thumbnail
}

export interface Client {
  id: string;
  name: string;
  company: string;
  sheetId: string;       // Google Sheet ID or Full URL
  sheetTab: string;      // E.g., GridId/TabName "Sheet1"
  ue5Endpoint: string;   // E.g. "http://localhost:8008/remote/object/call"
  webSocketEndpoint?: string; // E.g. "ws://127.0.0.1:8009"
  branding: BrandingTheme;
  updatedAt: string;     // ISO timestamp
  sheetData?: SpreadsheetData; // Live property portfolio spreadsheet rows and columns saved in Firebase
  bugs?: BugIssue[];     // Optional list of tracked bug issues
  mediaResources?: MediaResource[]; // Categorized media resources (Project, Services, Properties)
  driveFolderId?: string; // Dedicated Google Drive folder ID under shared root
  driveFolderUrl?: string; // Direct link to client's dedicated Google Drive folder
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
