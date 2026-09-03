/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import { Client, MediaResource } from "../types";

export const DRIVE_ROOT_MEDIA_FOLDER_ID = "13fE2R_-qxOMlT2U7tY1NtK36xpwahMkg";
export const DRIVE_ROOT_FOLDER_URL =
  "https://drive.google.com/drive/folders/13fE2R_-qxOMlT2U7tY1NtK36xpwahMkg?usp=sharing";

export interface DriveUploadResult {
  fileId: string;
  webViewLink: string;
  thumbnailLink?: string;
  webContentLink?: string;
}

export interface DriveFolderInfo {
  folderId: string;
  folderUrl: string;
  folderName: string;
}

export interface DriveFileItem {
  id: string;
  name: string;
  mimeType: string;
  webViewLink?: string;
  thumbnailLink?: string;
  size?: string;
  createdTime?: string;
}

/**
 * Converts a base64 Data URL to a Blob safely without throwing
 */
export function dataUrlToBlob(dataUrl: string): { blob: Blob; mimeType: string } {
  if (!dataUrl || typeof dataUrl !== "string") {
    return { blob: new Blob([], { type: "image/jpeg" }), mimeType: "image/jpeg" };
  }
  if (!dataUrl.startsWith("data:")) {
    return { blob: new Blob([], { type: "image/jpeg" }), mimeType: "image/jpeg" };
  }
  try {
    const parts = dataUrl.split(",");
    if (parts.length < 2) {
      return { blob: new Blob([], { type: "image/jpeg" }), mimeType: "image/jpeg" };
    }
    const mimeMatch = parts[0].match(/:(.*?);/);
    const mimeType = mimeMatch ? mimeMatch[1] : "image/jpeg";
    const binary = atob(parts[1]);
    const array = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) {
      array[i] = binary.charCodeAt(i);
    }
    return { blob: new Blob([array], { type: mimeType }), mimeType };
  } catch (e) {
    console.warn("[dataUrlToBlob] Parse warning:", e);
    return { blob: new Blob([], { type: "image/jpeg" }), mimeType: "image/jpeg" };
  }
}

/**
 * Converts a Blob to a base64 string
 */
export async function blobToBase64(blob: Blob): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onloadend = () => {
      const res = (reader.result as string) || "";
      const comma = res.indexOf(",");
      resolve(comma !== -1 ? res.substring(comma + 1) : res);
    };
    reader.onerror = () => reject(new Error("Failed to read image blob"));
    reader.readAsDataURL(blob);
  });
}

/**
 * Searches for or creates a dedicated client media folder inside the shared root Google Drive folder
 */
export async function getOrProvisionClientFolder(
  client: Client,
  token: string
): Promise<DriveFolderInfo> {
  if (!token) {
    throw new Error("Google OAuth token is required to access Google Drive folders.");
  }

  // 1. Try server-side API proxy first (bypasses browser CORS & sandbox limitations)
  try {
    const proxyRes = await fetch("/api/drive/provision-folder", {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ client }),
    });

    if (proxyRes.ok) {
      const folderInfo = await proxyRes.json();
      return folderInfo;
    } else {
      const errJson = await proxyRes.json().catch(() => null);
      if (errJson?.error) {
        throw new Error(errJson.error);
      }
    }
  } catch (proxyErr: any) {
    if (proxyErr.message && !proxyErr.message.includes("Failed to fetch")) {
      throw proxyErr;
    }
    console.warn("[Google Drive] Server proxy provision notice, trying direct client API...", proxyErr);
  }

  // 2. Direct client fallback
  const targetFolderName = `${client.company || client.name} (${client.id})`;

  // 2a. If client already has a saved folder ID, check if it's valid
  if (client.driveFolderId) {
    try {
      const checkRes = await fetch(
        `https://www.googleapis.com/drive/v3/files/${client.driveFolderId}?fields=id,name,webViewLink,trashed`,
        {
          headers: {
            Authorization: `Bearer ${token}`,
          },
        }
      );
      if (checkRes.ok) {
        const fileData = await checkRes.json();
        if (!fileData.trashed) {
          return {
            folderId: fileData.id,
            folderUrl:
              fileData.webViewLink ||
              `https://drive.google.com/drive/folders/${fileData.id}`,
            folderName: fileData.name || targetFolderName,
          };
        }
      }
    } catch (e) {
      console.warn("[Google Drive] Saved folder lookup failed, searching parent...", e);
    }
  }

  // 2b. Query the shared root folder for existing client folder
  const query = encodeURIComponent(
    `'${DRIVE_ROOT_MEDIA_FOLDER_ID}' in parents and mimeType = 'application/vnd.google-apps.folder' and trashed = false`
  );

  const listRes = await fetch(
    `https://www.googleapis.com/drive/v3/files?q=${query}&fields=files(id,name,webViewLink)&pageSize=100`,
    {
      headers: {
        Authorization: `Bearer ${token}`,
      },
    }
  );

  if (!listRes.ok) {
    const errText = await listRes.text();
    throw new Error(`Failed to list folders in shared Google Drive: ${errText}`);
  }

  const listData = await listRes.json();
  const existingFolder = (listData.files || []).find(
    (f: any) =>
      f.name.toLowerCase() === targetFolderName.toLowerCase() ||
      f.name.toLowerCase().includes(`(${client.id.toLowerCase()})`) ||
      (client.driveFolderId && f.id === client.driveFolderId)
  );

  if (existingFolder) {
    return {
      folderId: existingFolder.id,
      folderUrl:
        existingFolder.webViewLink ||
        `https://drive.google.com/drive/folders/${existingFolder.id}`,
      folderName: existingFolder.name,
    };
  }

  // 2c. Create the dedicated client folder inside the root shared directory
  const createRes = await fetch(
    "https://www.googleapis.com/drive/v3/files?fields=id,name,webViewLink",
    {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        name: targetFolderName,
        mimeType: "application/vnd.google-apps.folder",
        parents: [DRIVE_ROOT_MEDIA_FOLDER_ID],
        description: `Dedicated media repository for client ${client.name} (${client.company})`,
      }),
    }
  );

  if (!createRes.ok) {
    const errText = await createRes.text();
    throw new Error(`Failed to create client folder in Google Drive: ${errText}`);
  }

  const newFolder = await createRes.json();
  return {
    folderId: newFolder.id,
    folderUrl:
      newFolder.webViewLink || `https://drive.google.com/drive/folders/${newFolder.id}`,
    folderName: newFolder.name,
  };
}

/**
 * Uploads a file directly to the client's dedicated Google Drive folder
 * First routes via server-side proxy to guarantee zero CORS/sandbox failures
 */
export async function uploadMediaToClientFolder(params: {
  blob?: Blob;
  dataUrl?: string;
  url?: string;
  fileName: string;
  mimeType: string;
  folderId: string;
  token: string;
  description?: string;
}): Promise<DriveUploadResult> {
  const { blob, dataUrl, url, fileName, mimeType, folderId, token, description } = params;

  if (!token) {
    throw new Error("Authentication token is missing. Please sign in with your Google account to upload directly to Google Drive.");
  }

  // 1. Try server-side API proxy first
  try {
    let base64Data: string | undefined = undefined;
    if (dataUrl && dataUrl.startsWith("data:")) {
      const comma = dataUrl.indexOf(",");
      if (comma !== -1) {
        base64Data = dataUrl.substring(comma + 1);
      }
    } else if (blob && blob.size > 0) {
      base64Data = await blobToBase64(blob);
    }

    const proxyRes = await fetch("/api/drive/upload", {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        fileName,
        mimeType: mimeType || "image/jpeg",
        base64Data,
        dataUrl: !base64Data ? dataUrl : undefined,
        url: !base64Data && url ? url : undefined,
        folderId,
        description,
      }),
    });

    if (proxyRes.ok) {
      const data = await proxyRes.json();
      return {
        fileId: data.fileId,
        webViewLink: data.webViewLink,
        thumbnailLink: data.thumbnailLink,
        webContentLink: data.webContentLink,
      };
    } else {
      const errJson = await proxyRes.json().catch(() => null);
      const errMsg = errJson?.error || `Server upload returned status ${proxyRes.status}`;
      if (proxyRes.status === 401) {
        throw new Error(`Google Drive authorization has expired. Please re-authenticate your Google account: ${errMsg}`);
      }
      throw new Error(errMsg);
    }
  } catch (proxyErr: any) {
    // If the proxy threw a definitive error with details, rethrow it
    if (proxyErr.message && !proxyErr.message.includes("Failed to fetch")) {
      throw proxyErr;
    }
    console.warn("[Google Drive] Server proxy upload notice, trying direct client fallback...", proxyErr);
  }

  // 2. Direct client fallback (multipart/related)
  try {
    let uploadBlob = blob;
    if ((!uploadBlob || uploadBlob.size === 0) && dataUrl) {
      uploadBlob = dataUrlToBlob(dataUrl).blob;
    }

    if (!uploadBlob || uploadBlob.size === 0) {
      throw new Error("No media payload available to upload to Google Drive.");
    }

    const metadata = {
      name: fileName,
      parents: [folderId],
      description: description || "Uploaded via Client Portal Media Center",
    };

    const boundary = `-------SightPortalDriveUpload_${Date.now()}`;
    const metadataPart = `--${boundary}\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n${JSON.stringify(
      metadata
    )}\r\n`;
    const fileHeader = `--${boundary}\r\nContent-Type: ${mimeType || "image/jpeg"}\r\n\r\n`;
    const closing = `\r\n--${boundary}--`;

    const multipartBlob = new Blob([metadataPart, fileHeader, uploadBlob, closing], {
      type: `multipart/related; boundary=${boundary}`,
    });

    const uploadRes = await fetch(
      "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id,name,webViewLink,thumbnailLink,webContentLink",
      {
        method: "POST",
        headers: {
          Authorization: `Bearer ${token}`,
        },
        body: multipartBlob,
      }
    );

    if (!uploadRes.ok) {
      const errText = await uploadRes.text();
      throw new Error(`Google Drive upload failed (${uploadRes.status}): ${errText}`);
    }

    const data = await uploadRes.json();
    return {
      fileId: data.id,
      webViewLink: data.webViewLink,
      thumbnailLink: data.thumbnailLink,
      webContentLink: data.webContentLink,
    };
  } catch (directErr: any) {
    console.error("[Google Drive Direct Upload Error]", directErr);
    if (directErr.message?.includes("Failed to fetch")) {
      throw new Error(
        "Google Drive upload could not reach the server. Please ensure you are signed in with your Google account and have network connectivity."
      );
    }
    throw directErr;
  }
}

/**
 * Lists files inside the client's dedicated Google Drive folder
 */
export async function fetchClientFolderFiles(
  folderId: string,
  token: string
): Promise<DriveFileItem[]> {
  if (!token || !folderId) return [];

  // Try proxy first
  try {
    const proxyRes = await fetch(`/api/drive/files?folderId=${encodeURIComponent(folderId)}`, {
      headers: { Authorization: `Bearer ${token}` },
    });
    if (proxyRes.ok) {
      const data = await proxyRes.json();
      return data.files || [];
    }
  } catch (e) {
    console.warn("[Google Drive] Proxy list files notice, falling back to direct...", e);
  }

  // Direct fallback
  try {
    const query = encodeURIComponent(`'${folderId}' in parents and trashed = false`);
    const res = await fetch(
      `https://www.googleapis.com/drive/v3/files?q=${query}&fields=files(id,name,mimeType,webViewLink,thumbnailLink,size,createdTime)&orderBy=createdTime desc&pageSize=100`,
      {
        headers: {
          Authorization: `Bearer ${token}`,
        },
      }
    );

    if (!res.ok) {
      const errText = await res.text();
      throw new Error(`Failed to list Google Drive files: ${errText}`);
    }

    const data = await res.json();
    return data.files || [];
  } catch (err: any) {
    console.warn("[Google Drive] Fetch files notice:", err);
    return [];
  }
}

/**
 * Grants specific client user permission to access their dedicated folder.
 */
export async function grantClientFolderAccess(
  folderId: string,
  clientEmail: string,
  token: string,
  role: "reader" | "writer" = "reader"
): Promise<void> {
  if (!token || !clientEmail || !folderId) return;

  // Try proxy first
  try {
    const proxyRes = await fetch("/api/drive/share-folder", {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ folderId, clientEmail, role }),
    });
    if (proxyRes.ok) return;
  } catch (e) {
    console.warn("[Google Drive] Proxy share notice, falling back to direct...", e);
  }

  // Direct client fallback
  try {
    const res = await fetch(
      `https://www.googleapis.com/drive/v3/files/${folderId}/permissions`,
      {
        method: "POST",
        headers: {
          Authorization: `Bearer ${token}`,
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          role,
          type: "user",
          emailAddress: clientEmail,
        }),
      }
    );

    if (!res.ok) {
      const errText = await res.text();
      console.warn(`[Google Drive] Could not grant permission to ${clientEmail}:`, errText);
    }
  } catch (err) {
    console.warn(`[Google Drive] Could not grant permission to ${clientEmail}:`, err);
  }
}
