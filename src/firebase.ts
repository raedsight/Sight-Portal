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
  if (isQuotaExceeded()) {
    return () => {};
  }
  try {
    const q = query(collection(db, "themePresets"), orderBy("updatedAt", "desc"));
    return onSnapshot(q, (snap) => {
      const presets = snap.docs.map(d => d.data() as ThemePreset);
      onUpdate(presets);
    }, (err) => {
      handleFirestoreError(err, OperationType.LIST, "themePresets");
    });
  } catch (e) {
    console.warn("Failed to attach theme presets listener:", e);
    return () => {};
  }
}

export async function saveThemePreset(preset: ThemePreset): Promise<void> {
  if (isQuotaExceeded()) return;
  try {
    const cleaned = sanitizeForFirestore(preset);
    await setDoc(doc(db, "themePresets", preset.id), cleaned, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `themePresets/${preset.id}`);
  }
}

export async function deleteThemePreset(presetId: string): Promise<void> {
  if (isQuotaExceeded()) return;
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

export interface QuotaStatusInfo {
  exceeded: boolean;
  message: string;
  url: string;
}

const QUOTA_STORAGE_KEY = "sightportal_quota_exceeded_v1";

const quotaListeners = new Set<(info: QuotaStatusInfo) => void>();
let currentQuotaStatus: QuotaStatusInfo = {
  exceeded: false,
  message: "",
  url: `https://console.firebase.google.com/project/${firebaseConfig.projectId}/firestore/databases/${configuredDatabaseId}/data?openUpgradeDialog=true`
};

export function isQuotaExceeded(): boolean {
  if (currentQuotaStatus.exceeded) return true;
  try {
    const raw = localStorage.getItem(QUOTA_STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      // Quotas in Firestore reset at midnight PST. If marked within last 24 hours, keep flag active
      if (Date.now() - parsed.timestamp < 24 * 60 * 60 * 1000) {
        currentQuotaStatus.exceeded = true;
        currentQuotaStatus.message = parsed.message || "Daily free tier Firestore read quota reached (50,000 read units/day).";
        return true;
      } else {
        localStorage.removeItem(QUOTA_STORAGE_KEY);
      }
    }
  } catch (e) {}
  return false;
}

export function markQuotaExceeded(msg: string) {
  currentQuotaStatus = {
    exceeded: true,
    message: msg,
    url: `https://console.firebase.google.com/project/${firebaseConfig.projectId}/firestore/databases/${configuredDatabaseId}/data?openUpgradeDialog=true`
  };
  try {
    localStorage.setItem(QUOTA_STORAGE_KEY, JSON.stringify({
      timestamp: Date.now(),
      message: msg
    }));
  } catch (e) {}
  quotaListeners.forEach((l) => l(currentQuotaStatus));
}

export function resetQuotaStatus() {
  currentQuotaStatus = {
    exceeded: false,
    message: "",
    url: `https://console.firebase.google.com/project/${firebaseConfig.projectId}/firestore/databases/${configuredDatabaseId}/data?openUpgradeDialog=true`
  };
  try {
    localStorage.removeItem(QUOTA_STORAGE_KEY);
  } catch (e) {}
  quotaListeners.forEach((l) => l(currentQuotaStatus));
}

export function subscribeToQuotaStatus(listener: (info: QuotaStatusInfo) => void): () => void {
  quotaListeners.add(listener);
  // Trigger immediately if already exceeded
  if (isQuotaExceeded()) {
    listener(currentQuotaStatus);
  }
  return () => quotaListeners.delete(listener);
}

export function getQuotaStatus(): QuotaStatusInfo {
  isQuotaExceeded();
  return currentQuotaStatus;
}

// Client cache helpers
export function getCachedClients(): Client[] {
  try {
    const raw = localStorage.getItem("sightportal_cached_clients");
    if (raw) {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed) && parsed.length > 0) return parsed;
    }
  } catch (e) {}
  return DEFAULT_CLIENTS;
}

export function setCachedClients(clients: Client[]) {
  try {
    localStorage.setItem("sightportal_cached_clients", JSON.stringify(clients));
  } catch (e) {}
}

// Log cache helpers
export function getCachedLogs(): Log[] {
  try {
    const raw = localStorage.getItem("sightportal_cached_logs");
    if (raw) {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed) && parsed.length > 0) return parsed;
    }
  } catch (e) {}
  return DEFAULT_LOGS;
}

export function setCachedLogs(logs: Log[]) {
  try {
    localStorage.setItem("sightportal_cached_logs", JSON.stringify(logs.slice(0, 100)));
  } catch (e) {}
}

// User profile cache helpers
export function getCachedUsers(): UserProfile[] {
  try {
    const raw = localStorage.getItem("sightportal_cached_users");
    if (raw) {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed) && parsed.length > 0) return parsed;
    }
  } catch (e) {}
  return [
    {
      uid: "GDtCCZxWf2brJSU2aoamHTKmrrA3",
      email: "raed.sight@gmail.com",
      role: "owner",
      clientId: null
    }
  ];
}

export function setCachedUsers(users: UserProfile[]) {
  try {
    localStorage.setItem("sightportal_cached_users", JSON.stringify(users));
  } catch (e) {}
}

export function handleFirestoreError(error: unknown, operationType: OperationType, path: string | null) {
  const errMsg = error instanceof Error ? error.message : String(error);
  // Suppress verbose offline errors in dev mode to avoid cluttering UI logs
  if (errMsg.includes("the client is offline") || errMsg.includes("offline")) {
    console.warn(`[Firestore Offline: ${operationType} on ${path}]`);
    return;
  }

  // Detect and broadcast Firestore quota limit
  if (errMsg.includes("Quota limit exceeded") || errMsg.includes("Quota exceeded")) {
    markQuotaExceeded(errMsg);
    console.warn(`[Firestore Quota Notice: ${operationType} on ${path} paused. Active local cache engaged.]`);
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
  if (isQuotaExceeded()) {
    const cached = getCachedUsers().find(u => u.uid === uid || u.email === auth.currentUser?.email);
    if (cached) return cached;
    if (auth.currentUser?.email === "raed.sight@gmail.com") {
      return { uid, email: "raed.sight@gmail.com", role: "owner", clientId: null };
    }
    return null;
  }
  try {
    const snap = await getDoc(doc(db, "users", uid));
    if (snap.exists()) {
      const profile = snap.data() as UserProfile;
      const users = getCachedUsers();
      const idx = users.findIndex(u => u.uid === profile.uid);
      if (idx >= 0) users[idx] = profile;
      else users.push(profile);
      setCachedUsers(users);
      return profile;
    }
    return null;
  } catch (err: any) {
    const msg = err?.message || String(err);
    if (msg.includes("client is offline") || msg.includes("offline")) {
      console.warn(`[Firestore] Profile lookup offline for user: ${uid}`);
      return null;
    }
    handleFirestoreError(err, OperationType.GET, `users/${uid}`);
    const cached = getCachedUsers().find(u => u.uid === uid || u.email === auth.currentUser?.email);
    if (cached) return cached;
    if (auth.currentUser?.email === "raed.sight@gmail.com") {
      return { uid, email: "raed.sight@gmail.com", role: "owner", clientId: null };
    }
    return null;
  }
}

export async function saveUserProfile(profile: UserProfile): Promise<void> {
  const users = getCachedUsers();
  const idx = users.findIndex(u => u.uid === profile.uid);
  if (idx >= 0) users[idx] = profile;
  else users.push(profile);
  setCachedUsers(users);

  if (isQuotaExceeded()) return;
  try {
    const cleaned = sanitizeForFirestore(profile);
    await setDoc(doc(db, "users", profile.uid), cleaned, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `users/${profile.uid}`);
  }
}

export async function fetchAllUserProfiles(): Promise<UserProfile[]> {
  if (isQuotaExceeded()) {
    return getCachedUsers();
  }
  try {
    const q = query(collection(db, "users"));
    const snap = await getDocs(q);
    const users = snap.docs.map(d => d.data() as UserProfile);
    if (users.length > 0) {
      setCachedUsers(users);
    }
    return users;
  } catch (err) {
    handleFirestoreError(err, OperationType.LIST, "users");
    return getCachedUsers();
  }
}

export async function deleteUserProfile(uid: string): Promise<void> {
  const users = getCachedUsers().filter(u => u.uid !== uid);
  setCachedUsers(users);

  if (isQuotaExceeded()) return;
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
  if (isQuotaExceeded()) {
    onUpdate(getCachedClients());
    return () => {};
  }
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
      setCachedClients(clients);
      onUpdate(clients);
    }, (err) => {
      handleFirestoreError(err, OperationType.LIST, "clients");
      onUpdate(getCachedClients());
    });
  } catch (e) {
    console.warn("Failed to attach clients listener:", e);
    onUpdate(getCachedClients());
    return () => {};
  }
}

/**
 * Force fetch all clients directly from Firestore server, bypassing local offline cache
 */
export async function fetchClientsFromServer(): Promise<Client[]> {
  if (isQuotaExceeded()) {
    return getCachedClients();
  }
  try {
    let clients: Client[] = [];
    try {
      const snap = await getDocsFromServer(collection(db, "clients"));
      clients = snap.docs.map(d => normalizeClientDoc(d.id, d.data()));
    } catch (serverErr) {
      if (String(serverErr).includes("Quota")) {
        throw serverErr;
      }
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

    if (clients.length > 0) {
      setCachedClients(clients);
    }

    return clients;
  } catch (err) {
    handleFirestoreError(err, OperationType.LIST, "clients");
    return getCachedClients();
  }
}

/**
 * Delete default sample clients if they were seeded into Firestore by mistake
 */
export async function purgeDefaultSampleClients(): Promise<number> {
  if (isQuotaExceeded()) return 0;
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
  if (isQuotaExceeded()) {
    const cached = getCachedClients().find(c => c.id === clientId) || null;
    onUpdate(cached);
    return () => {};
  }
  try {
    return onSnapshot(doc(db, "clients", clientId), (snap) => {
      onUpdate(snap.exists() ? (snap.data() as Client) : null);
    }, (err) => {
      handleFirestoreError(err, OperationType.GET, `clients/${clientId}`);
      const cached = getCachedClients().find(c => c.id === clientId) || null;
      onUpdate(cached);
    });
  } catch (e) {
    console.warn("Failed to attach single client listener:", e);
    const cached = getCachedClients().find(c => c.id === clientId) || null;
    onUpdate(cached);
    return () => {};
  }
}

export async function createOrUpdateClient(client: Client): Promise<void> {
  const clients = getCachedClients();
  const idx = clients.findIndex(c => c.id === client.id);
  if (idx >= 0) clients[idx] = client;
  else clients.unshift(client);
  setCachedClients(clients);

  if (isQuotaExceeded()) return;
  try {
    const cleaned = sanitizeForFirestore(client);
    await setDoc(doc(db, "clients", client.id), cleaned, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `clients/${client.id}`);
  }
}

export async function saveClientSheetData(clientId: string, sheetData: import("./types").SpreadsheetData): Promise<void> {
  const clients = getCachedClients();
  const idx = clients.findIndex(c => c.id === clientId);
  if (idx >= 0) {
    clients[idx] = { ...clients[idx], sheetData, updatedAt: new Date().toISOString() };
    setCachedClients(clients);
  }

  if (isQuotaExceeded()) return;
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
  if (isQuotaExceeded()) {
    const cached = getCachedClients().find(c => c.id === clientId);
    return cached?.sheetData || null;
  }
  try {
    const snap = await getDoc(doc(db, "clients", clientId));
    if (snap.exists()) {
      const data = snap.data() as Client;
      return data.sheetData || null;
    }
    return null;
  } catch (err) {
    handleFirestoreError(err, OperationType.GET, `clients/${clientId}`);
    const cached = getCachedClients().find(c => c.id === clientId);
    return cached?.sheetData || null;
  }
}

export async function removeClient(clientId: string): Promise<void> {
  const clients = getCachedClients().filter(c => c.id !== clientId);
  setCachedClients(clients);

  if (isQuotaExceeded()) return;
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
  if (isQuotaExceeded()) {
    onUpdate(getCachedLogs());
    return () => {};
  }
  try {
    const q = query(collection(db, "logs"), orderBy("timestamp", "desc"), limit(100));
    return onSnapshot(q, (snap) => {
      const logs = snap.docs.map(d => d.data() as Log);
      setCachedLogs(logs);
      onUpdate(logs);
    }, (err) => {
      handleFirestoreError(err, OperationType.LIST, "logs");
      onUpdate(getCachedLogs());
    });
  } catch (e) {
    console.warn("Failed to attach logs listener:", e);
    onUpdate(getCachedLogs());
    return () => {};
  }
}

export async function writeLog(log: Log): Promise<void> {
  const logs = getCachedLogs();
  logs.unshift(log);
  setCachedLogs(logs);

  if (isQuotaExceeded()) return;
  try {
    const cleaned = sanitizeForFirestore(log);
    await setDoc(doc(db, "logs", log.id), cleaned);
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `logs/${log.id}`);
  }
}

export async function clearAllLogs(): Promise<void> {
  setCachedLogs([]);
  if (isQuotaExceeded()) return;
  try {
    const snap = await getDocs(collection(db, "logs"));
    const promises = snap.docs.map(d => deleteDoc(d.ref));
    await Promise.all(promises);
  } catch (err) {
    handleFirestoreError(err, OperationType.DELETE, "logs");
  }
}

export async function fetchLogsFromServer(): Promise<Log[]> {
  if (isQuotaExceeded()) {
    return getCachedLogs();
  }
  try {
    const q = query(collection(db, "logs"), orderBy("timestamp", "desc"), limit(100));
    const snap = await getDocs(q);
    const logs = snap.docs.map(d => d.data() as Log);
    setCachedLogs(logs);
    return logs;
  } catch (err) {
    handleFirestoreError(err, OperationType.LIST, "logs");
    return getCachedLogs();
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
