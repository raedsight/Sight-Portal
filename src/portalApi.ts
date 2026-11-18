import { auth } from "./firebase";

// Headers for the portal's own endpoints that change what Unreal shows (push data, push media, re-sync).
// The server only accepts those from a signed-in user who owns the client, so the sign-in proof goes along.
export async function portalJsonHeaders(): Promise<Record<string, string>> {
  const headers: Record<string, string> = { "Content-Type": "application/json" };
  const token = await auth.currentUser?.getIdToken();
  if (token) headers.Authorization = `Bearer ${token}`;
  return headers;
}

// The same sign-in proof for requests whose Authorization header is already taken by the Google Drive token
export async function portalAuthHeader(): Promise<Record<string, string>> {
  const token = await auth.currentUser?.getIdToken();
  return token ? { "X-Portal-Auth": `Bearer ${token}` } : {};
}
