import { initializeApp } from "firebase/app";
import { 
  getAuth, 
  signInWithPopup, 
  GoogleAuthProvider, 
  onAuthStateChanged, 
  signOut,
  signInWithEmailAndPassword,
  createUserWithEmailAndPassword,
  User 
} from "firebase/auth";
import { 
  getFirestore, 
  doc, 
  getDoc, 
  getDocs, 
  getDocsFromServer,
  setDoc, 
  query, 
  collection, 
  deleteDoc, 
  orderBy, 
  limit, 
  onSnapshot, 
  getDocFromServer,
  writeBatch
} from "firebase/firestore";
import { Client, Log, ThemePreset } from "./types";
import { DEFAULT_CLIENTS, DEFAULT_LOGS } from "./data";
import firebaseConfig from "../firebase-applet-config.json";
import { setCachedAccessToken } from "./firebaseAuth";

// Initialize Firebase with Authentication & Firestore support using designated Database ID
const app = initializeApp(firebaseConfig);
export const auth = getAuth(app);
export const configuredDatabaseId: string = (firebaseConfig as Record<string, any>).firestoreDatabaseId || "(default)";
export const db = configuredDatabaseId && configuredDatabaseId !== "(default)"
  ? getFirestore(app, configuredDatabaseId)
  : getFirestore(app);

/**
 * Recursively cleans objects to remove 'undefined' fields before passing them to Firestore.
 * Firestore strictly rejects undefined field values with 'Unsupported field value: undefined'.
 */
export function sanitizeForFirestore<T>(data: T): T {
  if (data === undefined) return null as any;
  if (data === null || typeof data !== "object") return data;
  if (Array.isArray(data)) {
    return data.map((item) => sanitizeForFirestore(item)) as any;
  }
  const cleaned: Record<string, any> = {};
  for (const [key, value] of Object.entries(data as Record<string, any>)) {
    if (value !== undefined) {
      cleaned[key] = sanitizeForFirestore(value);
    }
  }
  return cleaned as T;
}

// -------------------------------------------------------------
// THEME PRESET OPERATIONS (REAL FIRESTORE)
// -------------------------------------------------------------

export async function syncThemePresets(onUpdate: (presets: ThemePreset[]) => void): Promise<() => void> {
  try {
    const q = query(collection(db, "themePresets"), orderBy("updatedAt", "desc"));
    return onSnapshot(q, (snap) => {
      const presets = snap.docs.map(d => d.data() as ThemePreset);
      onUpdate(presets);
    }, (err) => {
      handleFirestoreError(err, OperationType.LIST, "themePresets");
    });
  } catch (e) {
    console.error("Failed to attach theme presets listener:", e);
    return () => {};
  }
}

export async function saveThemePreset(preset: ThemePreset): Promise<void> {
  try {
    const cleaned = sanitizeForFirestore(preset);
    await setDoc(doc(db, "themePresets", preset.id), cleaned, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `themePresets/${preset.id}`);
  }
}

export async function deleteThemePreset(presetId: string): Promise<void> {
  try {
    await deleteDoc(doc(db, "themePresets", presetId));
  } catch (err) {
    handleFirestoreError(err, OperationType.DELETE, `themePresets/${presetId}`);
  }
}

// 8-Pillar Error Handling conforming strictly to standard integration skills
export enum OperationType {
  CREATE = "create",
  UPDATE = "update",
  DELETE = "delete",
  LIST = "list",
  GET = "get",
  WRITE = "write",
}

export interface FirestoreErrorInfo {
  error: string;
  operationType: OperationType;
  path: string | null;
  authInfo: {
    userId?: string | null;
    email?: string | null;
    emailVerified?: boolean | null;
    isAnonymous?: boolean | null;
    tenantId?: string | null;
    providerInfo?: {
      providerId?: string | null;
      email?: string | null;
    }[];
  };
}

export function handleFirestoreError(error: unknown, operationType: OperationType, path: string | null) {
  const errMsg = error instanceof Error ? error.message : String(error);
  // Suppress verbose offline errors in dev mode to avoid cluttering UI logs
  if (errMsg.includes("the client is offline") || errMsg.includes("offline")) {
    console.warn(`[Firestore Offline: ${operationType} on ${path}]`);
    return;
  }
  const errInfo: FirestoreErrorInfo = {
    error: errMsg,
    authInfo: {
      userId: auth.currentUser?.uid,
      email: auth.currentUser?.email,
      emailVerified: auth.currentUser?.emailVerified,
      isAnonymous: auth.currentUser?.isAnonymous,
      tenantId: auth.currentUser?.tenantId,
      providerInfo: auth.currentUser?.providerData?.map(provider => ({
        providerId: provider.providerId,
        email: provider.email,
      })) || []
    },
    operationType,
    path
  };
  console.error("Firestore Error Detailed Payload: ", JSON.stringify(errInfo));
  throw new Error(JSON.stringify(errInfo));
}

// User Role Definition
export interface UserProfile {
  uid: string;
  email: string;
  role: "owner" | "admin" | "client";
  clientId?: string | null; // Null if developer/admin, holds assigned client portal slug if client group
}

// -------------------------------------------------------------
// USER OPERATIONS (REAL FIRESTORE)
// -------------------------------------------------------------

export async function fetchUserProfile(uid: string): Promise<UserProfile | null> {
  try {
    const snap = await getDoc(doc(db, "users", uid));
    return snap.exists() ? (snap.data() as UserProfile) : null;
  } catch (err: any) {
    const msg = err?.message || String(err);
    if (msg.includes("client is offline") || msg.includes("offline")) {
      console.warn(`[Firestore] Profile lookup offline for user: ${uid}`);
      return null;
    }
    handleFirestoreError(err, OperationType.GET, `users/${uid}`);
    return null;
  }
}

export async function saveUserProfile(profile: UserProfile): Promise<void> {
  try {
    const cleaned = sanitizeForFirestore(profile);
    await setDoc(doc(db, "users", profile.uid), cleaned, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `users/${profile.uid}`);
  }
}

export async function fetchAllUserProfiles(): Promise<UserProfile[]> {
  try {
    const q = query(collection(db, "users"));
    const snap = await getDocs(q);
    return snap.docs.map(d => d.data() as UserProfile);
  } catch (err) {
    handleFirestoreError(err, OperationType.LIST, "users");
    return [];
  }
}

export async function deleteUserProfile(uid: string): Promise<void> {
  try {
    await deleteDoc(doc(db, "users", uid));
  } catch (err) {
    handleFirestoreError(err, OperationType.DELETE, `users/${uid}`);
  }
}

// -------------------------------------------------------------
// CLIENT OPERATIONS (REAL FIRESTORE)
// -------------------------------------------------------------

function normalizeClientDoc(id: string, rawData: any): Client {
  const data = rawData || {};
  return {
    id: id,
    name: data.name || id.replace(/[-_]/g, ' ').replace(/\b\w/g, (l: string) => l.toUpperCase()),
    company: data.company || "Sight Real Estate & Production",
    sheetId: data.sheetId || "1BxiMVs0XRA5nFMdKv1aM9ldm5i-YSgcbL1g6xGoS18A",
    sheetTab: data.sheetTab || "MainSpawns",
    ue5Endpoint: data.ue5Endpoint || "http://localhost:8008/remote/object/call",
    webSocketEndpoint: data.webSocketEndpoint || `ws://127.0.0.1:8009/ws/${id}`,
    branding: {
      primaryColor: data.branding?.primaryColor || "#d97706",
      accentColor: data.branding?.accentColor || "#f59e0b",
      logoText: data.branding?.logoText || (data.name || id).toUpperCase(),
      bgStyle: data.branding?.bgStyle || "dark",
      fontFamily: data.branding?.fontFamily || "sans",
      logoUrl: data.branding?.logoUrl || undefined,
    },
    updatedAt: data.updatedAt || new Date().toISOString(),
    sheetData: data.sheetData,
    bugs: data.bugs || [],
    mediaResources: data.mediaResources || []
  };
}

export async function syncClients(onUpdate: (clients: Client[]) => void): Promise<() => void> {
  try {
    // Avoid orderBy in Firestore query so documents lacking updatedAt field are NOT dropped
    const q = query(collection(db, "clients"));
    return onSnapshot(q, (snap) => {
      const clients = snap.docs.map(d => normalizeClientDoc(d.id, d.data()));
      // Sort in-memory safely by updatedAt descending, fallback to name
      clients.sort((a, b) => {
        const timeA = a.updatedAt ? new Date(a.updatedAt).getTime() : 0;
        const timeB = b.updatedAt ? new Date(b.updatedAt).getTime() : 0;
        if (timeB !== timeA) return timeB - timeA;
        return (a.name || "").localeCompare(b.name || "");
      });
      onUpdate(clients);
    }, (err) => {
      console.warn("[Firestore] Sync clients error notice:", err);
      try {
        handleFirestoreError(err, OperationType.LIST, "clients");
      } catch (e) {
        // Handled to protect event loop
      }
    });
  } catch (e) {
    console.error("Failed to attach clients listener:", e);
    return () => {};
  }
}

/**
 * Force fetch all clients directly from Firestore server, bypassing local offline cache
 */
export async function fetchClientsFromServer(): Promise<Client[]> {
  try {
    let clients: Client[] = [];
    try {
      const snap = await getDocsFromServer(collection(db, "clients"));
      clients = snap.docs.map(d => normalizeClientDoc(d.id, d.data()));
    } catch (serverErr) {
      console.warn("[Firestore] Direct server read notice, trying fallback getDocs:", serverErr);
      const snap = await getDocs(collection(db, "clients"));
      clients = snap.docs.map(d => normalizeClientDoc(d.id, d.data()));
    }

    // Check alias collections if main collection is empty
    if (clients.length === 0) {
      const altCollections = ["clientPortals", "client_portals", "portals"];
      for (const collName of altCollections) {
        try {
          const altSnap = await getDocs(collection(db, collName));
          if (altSnap.docs.length > 0) {
            console.log(`[Firestore] Discovered ${altSnap.docs.length} clients in alias collection '${collName}'`);
            clients = altSnap.docs.map(d => normalizeClientDoc(d.id, d.data()));
            break;
          }
        } catch (e) {
          // ignore alias lookup
        }
      }
    }

    clients.sort((a, b) => {
      const timeA = a.updatedAt ? new Date(a.updatedAt).getTime() : 0;
      const timeB = b.updatedAt ? new Date(b.updatedAt).getTime() : 0;
      if (timeB !== timeA) return timeB - timeA;
      return (a.name || "").localeCompare(b.name || "");
    });

    return clients;
  } catch (err) {
    handleFirestoreError(err, OperationType.LIST, "clients");
    return [];
  }
}

/**
 * Delete default sample clients if they were seeded into Firestore by mistake
 */
export async function purgeDefaultSampleClients(): Promise<number> {
  const defaultSampleIds = ["neon-nebula", "overlord-stadium", "overlord-egames", "hyperion-vis"];
  let purgedCount = 0;
  for (const id of defaultSampleIds) {
    try {
      await deleteDoc(doc(db, "clients", id));
      purgedCount++;
    } catch (e) {
      console.warn(`[Firestore] Note on deleting default client ${id}:`, e);
    }
  }
  return purgedCount;
}

export async function syncSingleClient(clientId: string, onUpdate: (client: Client | null) => void): Promise<() => void> {
  try {
    return onSnapshot(doc(db, "clients", clientId), (snap) => {
      onUpdate(snap.exists() ? (snap.data() as Client) : null);
    }, (err) => {
      console.warn(`[Firestore] Sync client (${clientId}) error notice:`, err);
      try {
        handleFirestoreError(err, OperationType.GET, `clients/${clientId}`);
      } catch (e) {
        // Handled to protect event loop
      }
    });
  } catch (e) {
    console.error("Failed to attach single client listener:", e);
    return () => {};
  }
}

export async function createOrUpdateClient(client: Client): Promise<void> {
  try {
    const cleaned = sanitizeForFirestore(client);
    await setDoc(doc(db, "clients", client.id), cleaned, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `clients/${client.id}`);
  }
}

export async function saveClientSheetData(clientId: string, sheetData: import("./types").SpreadsheetData): Promise<void> {
  try {
    const cleaned = sanitizeForFirestore(sheetData);
    await setDoc(doc(db, "clients", clientId), { 
      sheetData: cleaned, 
      updatedAt: new Date().toISOString() 
    }, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `clients/${clientId}`);
  }
}

export async function fetchClientSheetData(clientId: string): Promise<import("./types").SpreadsheetData | null> {
  try {
    const snap = await getDoc(doc(db, "clients", clientId));
    if (snap.exists()) {
      const data = snap.data() as Client;
      return data.sheetData || null;
    }
    return null;
  } catch (err) {
    handleFirestoreError(err, OperationType.GET, `clients/${clientId}`);
    return null;
  }
}

export async function removeClient(clientId: string): Promise<void> {
  try {
    await deleteDoc(doc(db, "clients", clientId));
  } catch (err) {
    handleFirestoreError(err, OperationType.DELETE, `clients/${clientId}`);
  }
}

// -------------------------------------------------------------
// LOG OPERATIONS (REAL FIRESTORE)
// -------------------------------------------------------------

export async function syncLogs(onUpdate: (logs: Log[]) => void): Promise<() => void> {
  try {
    const q = query(collection(db, "logs"), orderBy("timestamp", "desc"), limit(100));
    return onSnapshot(q, (snap) => {
      const logs = snap.docs.map(d => d.data() as Log);
      onUpdate(logs);
    }, (err) => {
      console.warn("[Firestore] Sync logs error notice:", err);
      try {
        handleFirestoreError(err, OperationType.LIST, "logs");
      } catch (e) {
        // Handled to protect event loop
      }
    });
  } catch (e) {
    console.error("Failed to attach logs listener:", e);
    return () => {};
  }
}

export async function writeLog(log: Log): Promise<void> {
  try {
    const cleaned = sanitizeForFirestore(log);
    await setDoc(doc(db, "logs", log.id), cleaned);
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `logs/${log.id}`);
  }
}

export async function clearAllLogs(): Promise<void> {
  try {
    const snap = await getDocs(collection(db, "logs"));
    const promises = snap.docs.map(d => deleteDoc(d.ref));
    await Promise.all(promises);
  } catch (err) {
    handleFirestoreError(err, OperationType.DELETE, "logs");
  }
}

export async function fetchLogsFromServer(): Promise<Log[]> {
  try {
    const q = query(collection(db, "logs"), orderBy("timestamp", "desc"), limit(100));
    const snap = await getDocs(q);
    return snap.docs.map(d => d.data() as Log);
  } catch (err) {
    console.warn("[Firestore] fetchLogsFromServer error notice:", err);
    try {
      const snap = await getDocs(collection(db, "logs"));
      const logs = snap.docs.map(d => d.data() as Log);
      logs.sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime());
      return logs;
    } catch (fallbackErr) {
      handleFirestoreError(fallbackErr, OperationType.LIST, "logs");
      return [];
    }
  }
}

// Interactive Google Account Connection (requires Sheets + Drive scopes)
export async function triggerGoogleAuthPopup(): Promise<{ user: User; accessToken: string }> {
  try {
    const provider = new GoogleAuthProvider();
    provider.addScope("https://www.googleapis.com/auth/spreadsheets");
    provider.addScope("https://www.googleapis.com/auth/drive");
    provider.addScope("https://www.googleapis.com/auth/drive.file");
    const result = await signInWithPopup(auth, provider);
    const credential = GoogleAuthProvider.credentialFromResult(result);
    if (!credential?.accessToken) {
      throw new Error("Missing Google API access token in verified Google response");
    }
    setCachedAccessToken(credential.accessToken);
    return { user: result.user, accessToken: credential.accessToken };
  } catch (error: any) {
    console.error("[Google Auth Popup error]", error);
    const msg = error.message || String(error);
    if (
      msg.includes("Pending promise was never set") ||
      msg.includes("popup-blocked") ||
      msg.includes("cancelled-popup-request") ||
      error.code === "auth/popup-blocked" ||
      error.code === "auth/cancelled-popup-request"
    ) {
      throw new Error(
        "Google Sign-In popup was blocked or interrupted by the iframe preview environment. Please click 'Open in New Tab' to run outside the sandbox."
      );
    }
    throw error;
  }
}

// Standard Google Sign-In for general authentication
export async function triggerGoogleLogin(): Promise<User> {
  try {
    const provider = new GoogleAuthProvider();
    provider.addScope("https://www.googleapis.com/auth/spreadsheets");
    provider.addScope("https://www.googleapis.com/auth/drive");
    provider.addScope("https://www.googleapis.com/auth/drive.file");
    const result = await signInWithPopup(auth, provider);
    const credential = GoogleAuthProvider.credentialFromResult(result);
    if (credential?.accessToken) {
      setCachedAccessToken(credential.accessToken);
    }
    return result.user;
  } catch (error: any) {
    console.error("[Google Login error]", error);
    const msg = error.message || String(error);
    if (
      msg.includes("Pending promise was never set") ||
      msg.includes("popup-blocked") ||
      msg.includes("cancelled-popup-request") ||
      error.code === "auth/popup-blocked" ||
      error.code === "auth/cancelled-popup-request"
    ) {
      throw new Error(
        "Google Sign-In popup was blocked or interrupted by the iframe preview environment. Please click 'Open in New Tab' to sign in smoothly outside the sandbox."
      );
    }
    throw error;
  }
}
