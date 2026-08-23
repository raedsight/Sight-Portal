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
    await setDoc(doc(db, "themePresets", preset.id), preset, { merge: true });
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

// Initialize Firebase with Authentication & Firestore support
const app = initializeApp(firebaseConfig);
export const auth = getAuth(app);
export const db = firebaseConfig.firestoreDatabaseId && firebaseConfig.firestoreDatabaseId !== "(default)"
  ? getFirestore(app, firebaseConfig.firestoreDatabaseId)
  : getFirestore(app);

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
  const errInfo: FirestoreErrorInfo = {
    error: error instanceof Error ? error.message : String(error),
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

// Ensure database connection is online on startup
export async function validateDbConnection() {
  try {
    await getDocFromServer(doc(db, "test", "connection"));
    console.log("[Firestore] Connection test verified.");
  } catch (error) {
    if (error instanceof Error && error.message.includes("the client is offline")) {
      console.error("Please check your Firebase configuration or network status.");
    }
  }
}
validateDbConnection();

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
  } catch (err) {
    handleFirestoreError(err, OperationType.GET, `users/${uid}`);
    return null;
  }
}

export async function saveUserProfile(profile: UserProfile): Promise<void> {
  try {
    await setDoc(doc(db, "users", profile.uid), profile, { merge: true });
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

export async function syncClients(onUpdate: (clients: Client[]) => void): Promise<() => void> {
  try {
    const q = query(collection(db, "clients"), orderBy("updatedAt", "desc"));
    return onSnapshot(q, (snap) => {
      const clients = snap.docs.map(d => d.data() as Client);
      onUpdate(clients);
    }, (err) => {
      handleFirestoreError(err, OperationType.LIST, "clients");
    });
  } catch (e) {
    console.error("Failed to attach clients listener:", e);
    return () => {};
  }
}

export async function syncSingleClient(clientId: string, onUpdate: (client: Client | null) => void): Promise<() => void> {
  try {
    return onSnapshot(doc(db, "clients", clientId), (snap) => {
      onUpdate(snap.exists() ? (snap.data() as Client) : null);
    }, (err) => {
      handleFirestoreError(err, OperationType.GET, `clients/${clientId}`);
    });
  } catch (e) {
    console.error("Failed to attach single client listener:", e);
    return () => {};
  }
}

export async function createOrUpdateClient(client: Client): Promise<void> {
  try {
    await setDoc(doc(db, "clients", client.id), client, { merge: true });
  } catch (err) {
    handleFirestoreError(err, OperationType.WRITE, `clients/${client.id}`);
  }
}

export async function saveClientSheetData(clientId: string, sheetData: import("./types").SpreadsheetData): Promise<void> {
  try {
    await setDoc(doc(db, "clients", clientId), { 
      sheetData, 
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
      handleFirestoreError(err, OperationType.LIST, "logs");
    });
  } catch (e) {
    console.error("Failed to attach logs listener:", e);
    return () => {};
  }
}

export async function writeLog(log: Log): Promise<void> {
  try {
    await setDoc(doc(db, "logs", log.id), log);
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

// Interactive Google Account Connection (requires Sheets scope)
export async function triggerGoogleAuthPopup(): Promise<{ user: User; accessToken: string }> {
  try {
    const provider = new GoogleAuthProvider();
    provider.addScope("https://www.googleapis.com/auth/spreadsheets");
    const result = await signInWithPopup(auth, provider);
    const credential = GoogleAuthProvider.credentialFromResult(result);
    if (!credential?.accessToken) {
      throw new Error("Missing Sheets API access token in verified Google response");
    }
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

// Standard Google Sign-In for general authentication (no extra scopes)
export async function triggerGoogleLogin(): Promise<User> {
  try {
    const provider = new GoogleAuthProvider();
    const result = await signInWithPopup(auth, provider);
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
        "Google Sign-In popup was blocked or interrupted by the iframe preview environment. Please click 'Open in New Tab' at the top, or use Email/Password login."
      );
    }
    throw error;
  }
}
