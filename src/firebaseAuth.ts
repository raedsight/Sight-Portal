import { 
  signInWithPopup, 
  GoogleAuthProvider, 
  onAuthStateChanged, 
  User 
} from "firebase/auth";
import { auth } from "./firebase";

const provider = new GoogleAuthProvider();
// Google Sheets and Google Drive access for client media storage and the spreadsheet bridge. This provider is
// only used by googleSignIn() below, when someone uses a Drive or Sheets action; the portal's own sign-in
// (triggerGoogleLogin in firebase.ts) asks for none of this.
provider.addScope("https://www.googleapis.com/auth/spreadsheets");
provider.addScope("https://www.googleapis.com/auth/drive");
provider.addScope("https://www.googleapis.com/auth/drive.file");

let isSigningIn = false;
let cachedAccessToken: string | null = null;

export const setCachedAccessToken = (token: string | null) => {
  cachedAccessToken = token;
};

// Listen for Auth changes and restore or clear cached tokens
export const initAuth = (
  onAuthSuccess?: (user: User, token: string) => void,
  onAuthFailure?: () => void
) => {
  return onAuthStateChanged(auth, async (user: User | null) => {
    if (user) {
      if (cachedAccessToken) {
        if (onAuthSuccess) onAuthSuccess(user, cachedAccessToken);
      } else if (!isSigningIn) {
        // If the user refreshed or is already logged in, we fetch a fresh token or await user gesture
        if (onAuthSuccess) onAuthSuccess(user, "");
      }
    } else {
      cachedAccessToken = null;
      if (onAuthFailure) onAuthFailure();
    }
  });
};

export const googleSignIn = async (): Promise<{ user: User; accessToken: string } | null> => {
  try {
    isSigningIn = true;
    // Suggest the Google account already signed in to the portal, so connecting Drive does not switch accounts by accident
    const currentEmail = auth.currentUser?.email;
    provider.setCustomParameters(currentEmail ? { login_hint: currentEmail } : {});
    const result = await signInWithPopup(auth, provider);
    const credential = GoogleAuthProvider.credentialFromResult(result);
    if (!credential?.accessToken) {
      throw new Error("Failed to get access token from Firebase Auth");
    }

    cachedAccessToken = credential.accessToken;
    return { user: result.user, accessToken: cachedAccessToken };
  } catch (error: any) {
    console.error("Sign in error:", error);
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
  } finally {
    isSigningIn = false;
  }
};

export const getAccessToken = async (): Promise<string | null> => {
  return cachedAccessToken;
};

export const logout = async () => {
  await auth.signOut();
  cachedAccessToken = null;
};
