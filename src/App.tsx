/**
 * @license
 * SPDX-License-Identifier: Apache-2.5
 */

import { useState, useEffect, useRef, FormEvent } from "react";
import { 
  auth, 
  db, 
  fetchUserProfile, 
  saveUserProfile, 
  fetchAllUserProfiles, 
  deleteUserProfile, 
  syncClients, 
  syncSingleClient,
  createOrUpdateClient, 
  removeClient, 
  syncLogs, 
  writeLog, 
  clearAllLogs,
  UserProfile,
  triggerGoogleAuthPopup,
  triggerGoogleLogin,
  fetchClientsFromServer,
  fetchLogsFromServer,
  purgeDefaultSampleClients,
  configuredDatabaseId,
  subscribeToQuotaStatus,
  resetQuotaStatus,
  QuotaStatusInfo
} from "./firebase";
import { 
  onAuthStateChanged, 
  signOut, 
  User,
  signInWithEmailAndPassword,
  createUserWithEmailAndPassword
} from "firebase/auth";
import { Client, Log } from "./types";
import { DEFAULT_CLIENTS, DEFAULT_LOGS, extractSpreadsheetId } from "./data";
import AdminConsole from "./components/AdminConsole";
import ClientDashboard from "./components/ClientDashboard";
import { 
  Building2, 
  Sparkles, 
  Cpu, 
  Server, 
  Compass, 
  Users, 
  CheckCircle2, 
  Settings,
  Monitor,
  Wifi,
  WifiOff,
  ShieldAlert,
  LogOut,
  ChevronRight,
  ShieldCheck,
  UserCheck,
  Database,
  Trash2,
  RefreshCw,
  AlertTriangle,
  ExternalLink,
  X
} from "lucide-react";

export default function App() {
  const [activeView, setActiveView] = useState<"admin" | "client">("admin");
  const [clients, setClients] = useState<Client[]>([]);
  const hasSeededInitialRef = useRef<boolean>(false);
  const [logs, setLogs] = useState<Log[]>([]);
  const [selectedClient, setSelectedClient] = useState<Client | null>(null);
  const [isSyncingDb, setIsSyncingDb] = useState<boolean>(false);
  const [syncDbStatus, setSyncDbStatus] = useState<string | null>(null);
  const [quotaInfo, setQuotaInfo] = useState<QuotaStatusInfo>({
    exceeded: false,
    message: "",
    url: `https://console.firebase.google.com/project/sight-portal-adc29/firestore/databases/${configuredDatabaseId}/data?openUpgradeDialog=true`
  });
  const [quotaBannerDismissed, setQuotaBannerDismissed] = useState<boolean>(false);

  // Subscribe to real-time quota status notifications
  useEffect(() => {
    const unsub = subscribeToQuotaStatus((info) => {
      setQuotaInfo(info);
    });
    return () => unsub();
  }, []);

  const handleForceSyncDatabase = async () => {
    setIsSyncingDb(true);
    setSyncDbStatus(`Querying Firestore (${configuredDatabaseId})...`);
    try {
      const [serverClients, serverUsers, serverLogs] = await Promise.all([
        fetchClientsFromServer(),
        fetchAllUserProfiles(),
        fetchLogsFromServer(),
      ]);
      setClients(serverClients);
      setUserProfilesList(serverUsers);
      if (serverLogs && serverLogs.length > 0) {
        setLogs(serverLogs);
      }
      setSyncDbStatus(`Synchronized database: ${serverClients.length} client(s), ${serverUsers.length} user(s), ${serverLogs.length} log(s).`);
      setTimeout(() => setSyncDbStatus(null), 5000);
    } catch (err: any) {
      setSyncDbStatus(`Sync failed: ${err?.message || "Check network/permissions"}`);
      setTimeout(() => setSyncDbStatus(null), 6000);
    } finally {
      setIsSyncingDb(false);
    }
  };

  const handlePurgeDefaultClients = async () => {
    setIsSyncingDb(true);
    setSyncDbStatus("Purging sample template clients from Firestore...");
    try {
      const purged = await purgeDefaultSampleClients();
      const serverClients = await fetchClientsFromServer();
      setClients(serverClients);
      setSyncDbStatus(`Purged ${purged} template client(s). Active clients in database: ${serverClients.length}.`);
      setTimeout(() => setSyncDbStatus(null), 5000);
    } catch (err: any) {
      setSyncDbStatus(`Purge notice: ${err?.message || String(err)}`);
      setTimeout(() => setSyncDbStatus(null), 6000);
    } finally {
      setIsSyncingDb(false);
    }
  };

  // Auth & RBAC State
  const [currentUser, setCurrentUser] = useState<User | null>(null);
  const [userProfile, setUserProfile] = useState<UserProfile | null>(null);
  const [userProfilesList, setUserProfilesList] = useState<UserProfile[]>([]);
  const [authLoading, setAuthLoading] = useState<boolean>(true);

  // Email/Password Auth Form state
  const [authMode, setAuthMode] = useState<"signin" | "signup">("signin");
  const [email, setEmail] = useState<string>("");
  const [password, setPassword] = useState<string>("");
  const [authError, setAuthError] = useState<string | null>(null);
  const [suggestGoogleAuth, setSuggestGoogleAuth] = useState<boolean>(false);
  const [formLoading, setFormLoading] = useState<boolean>(false);

  // Probe states for real-time connection status
  const [backendAlive, setBackendAlive] = useState<boolean>(true);
  const [ue5Alive, setUe5Alive] = useState<boolean>(false);

  // URL Query deep link: ?portal=id-slug
  const [deepLinkPortal, setDeepLinkPortal] = useState<string | null>(null);
  const [accessDeniedError, setAccessDeniedError] = useState<string | null>(null);

  // Parse deep link portal parameter on startup
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    const portalId = params.get("portal") || window.location.hash.replace("#", "").split("?")[0];
    if (portalId) {
      setDeepLinkPortal(portalId);
    }
  }, []);

  // Listen for Authentication state changes with safety timeout and non-blocking recovery
  useEffect(() => {
    let isMounted = true;

    // Safety timeout: If Firebase auth observer is delayed by iframe sandboxing, release the loading gate
    const fallbackTimer = setTimeout(() => {
      if (isMounted) {
        setAuthLoading((loading) => {
          if (loading) {
            console.warn("[Auth] Firebase initial observer timed out in sandbox; clearing loading screen.");
            return false;
          }
          return false;
        });
      }
    }, 1800);

    const unsubscribe = onAuthStateChanged(auth, async (user) => {
      clearTimeout(fallbackTimer);
      try {
        if (user) {
          setCurrentUser(user);
          
          // Determine immediate verified role
          const isOwnerEmail = user.email === "raed.sight@gmail.com";
          const defaultRole = isOwnerEmail ? "owner" : "client";
          
          const defaultProfile: UserProfile = {
            uid: user.uid,
            email: user.email || "",
            role: defaultRole,
            clientId: null
          };

          // Try fetching stored Firestore profile safely
          let profile: UserProfile = defaultProfile;
          try {
            const fetched = await fetchUserProfile(user.uid);
            if (fetched) {
              profile = fetched;
              if (isOwnerEmail && profile.role !== "owner") {
                profile.role = "owner";
                saveUserProfile(profile).catch((e) => console.warn("[Auth] Update owner profile notice:", e));
              }
            } else {
              saveUserProfile(defaultProfile).catch((e) => console.warn("[Auth] Save profile notice:", e));
            }
          } catch (profileErr) {
            console.warn("[Auth] Profile lookup non-fatal notice, using active profile:", profileErr);
          }

          if (isMounted) {
            setUserProfile(profile);
          }

          // Asynchronously load user list for Admin directory without blocking UI
          if (profile.role === "owner" || profile.role === "admin") {
            fetchAllUserProfiles()
              .then((list) => {
                if (isMounted && list.length > 0) setUserProfilesList(list);
              })
              .catch((err) => console.warn("[Auth] Directory list notice:", err));
          }

          // Asynchronously record audit log without blocking UI
          const initialLog: Log = {
            id: `log-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
            clientId: "system",
            clientName: "Identity Management",
            timestamp: new Date().toISOString(),
            type: "config_change",
            status: "success",
            details: `User session active: ${user.email || user.uid} (${profile.role.toUpperCase()} GRP)`
          };
          writeLog(initialLog).catch((e) => console.warn("[Auth] Audit log notice:", e));

        } else {
          if (isMounted) {
            setCurrentUser(null);
            setUserProfile(null);
            setUserProfilesList([]);
          }
        }
      } catch (authErr) {
        console.error("[Auth] Unexpected error during authentication resolution:", authErr);
      } finally {
        if (isMounted) {
          setAuthLoading(false);
        }
      }
    });

    return () => {
      isMounted = false;
      clearTimeout(fallbackTimer);
      unsubscribe();
    };
  }, []);

  // Synchronize dynamic Client Portals list from Firestore based on auth state and role
  useEffect(() => {
    if (authLoading || !userProfile) {
      setClients([]);
      return;
    }

    let unsubscribe: (() => void) | null = null;

    const setupSync = async () => {
      if (userProfile.role === "owner" || userProfile.role === "admin") {
        // Query Firestore directly from server first
        fetchClientsFromServer()
          .then((directClients) => {
            if (directClients.length > 0) {
              setClients(directClients);
            }
          })
          .catch((e) => console.warn("[Firestore] Direct fetch notice:", e));

        unsubscribe = await syncClients((updatedClients) => {
          setClients(updatedClients);
          setSelectedClient(prev => {
            if (!prev) return null;
            const found = updatedClients.find(c => c.id === prev.id);
            return found || prev;
          });
        });
      } else if (userProfile.role === "client" && userProfile.clientId) {
        unsubscribe = await syncSingleClient(userProfile.clientId, (updatedClient) => {
          if (updatedClient) {
            setClients([updatedClient]);
            setSelectedClient(prev => {
              if (prev && prev.id === updatedClient.id) return updatedClient;
              return prev;
            });
          } else {
            setClients([]);
          }
        });
      }
    };

    setupSync();
    return () => {
      if (unsubscribe) unsubscribe();
    };
  }, [authLoading, userProfile]);

  // Synchronize System Central logs from Firestore in real-time (Owner/Admin only)
  useEffect(() => {
    if (authLoading || !userProfile || userProfile.role === "client") {
      setLogs([]);
      return;
    }

    let unsubscribe: (() => void) | null = null;

    const setupSync = async () => {
      let firstRun = true;
      unsubscribe = await syncLogs((updatedLogs) => {
        if (updatedLogs.length === 0 && firstRun && !localStorage.getItem("sightportal_seeded_logs")) {
          firstRun = false;
          localStorage.setItem("sightportal_seeded_logs", "true");
          DEFAULT_LOGS.forEach(async (l) => {
            await writeLog(l);
          });
        } else {
          firstRun = false;
          setLogs(updatedLogs);
        }
      });
    };

    setupSync();
    return () => {
      if (unsubscribe) unsubscribe();
    };
  }, [authLoading, userProfile]);

  // Resolve client routing based on user profile roles & deep links
  useEffect(() => {
    if (authLoading || !userProfile) return;

    // Evaluate Deep Routing Parameter (?portal=xxx)
    if (deepLinkPortal) {
      const matchedClient = clients.find(c => c.id === deepLinkPortal);
      if (matchedClient) {
        if (userProfile.role === "owner" || userProfile.role === "admin") {
          setSelectedClient(matchedClient);
          setActiveView("client");
          setAccessDeniedError(null);
        } else if (userProfile.role === "client" && userProfile.clientId === deepLinkPortal) {
          setSelectedClient(matchedClient);
          setActiveView("client");
          setAccessDeniedError(null);
        } else {
          setAccessDeniedError(`Access Denied: Your client account group is unauthorized to access portal '${deepLinkPortal}'.`);
          setSelectedClient(null);
          setActiveView("client");
        }
      } else if (clients.length > 0) {
        setAccessDeniedError(`Portal channel '${deepLinkPortal}' was not found or has been removed from the Stage registry.`);
        setSelectedClient(null);
        setActiveView("client");
      }
      return;
    }

    // Role-based redirect if accessing root view without specific URL param
    if (userProfile.role === "client") {
      if (userProfile.clientId) {
        const clientObj = clients.find(c => c.id === userProfile.clientId);
        if (clientObj) {
          setSelectedClient(clientObj);
          setActiveView("client");
          setAccessDeniedError(null);
        } else {
          setAccessDeniedError("Staging portal configuration issue: Your assigned Portal Slug was not found in active registries.");
        }
      } else {
        setAccessDeniedError("Awaiting Portal Allocation: Your account belongs to the Client Group, but has not yet been assigned to a designated portal. Please contact the project Owner/Admin.");
      }
    } else {
      // Admins and owners default to the Admin Console
      setAccessDeniedError(null);
    }
  }, [deepLinkPortal, userProfile, clients, authLoading]);

  // Sync state & helper function to trigger a manual refresh on demand
  const [syncing, setSyncing] = useState<boolean>(false);

  const handleForceSync = async () => {
    if (syncing) return;
    setSyncing(true);
    const slug = selectedClient?.id || "hyperion-vis";
    const name = selectedClient?.name || "Hyperion Vis";
    try {
      const res = await fetch("/api/force-sync", {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify({ client_slug: slug })
      });
      const data = await res.json();
      if (data.success) {
        await handleRecordLog({
          clientId: slug,
          clientName: name,
          type: "config_change",
          status: "success",
          details: `Manual Force Refresh triggered. Dispatched client '${slug}' spreadsheet records to UE5 WebSocket receiver.`
        });
        alert(`Unreal Sync Completed! Re-synchronized parameters for client '${name}' to the active Unreal Engine session.`);
      } else {
        alert(`Sync failed: ${data.error || "Unknown error"}`);
      }
    } catch (err) {
      console.error("[Force Sync] Failed:", err);
      alert("Failed to communicate with Server. Verify internet connectivity.");
    } finally {
      setSyncing(false);
    }
  };

  // Poll connection health and Unreal presence
  useEffect(() => {
    const probeEndpoints = async () => {
      // 1. Probe the main server health route
      let serverUeAlive = false;
      try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 2000);
        const slugQuery = selectedClient ? `?client_slug=${selectedClient.id}` : "";
        const res = await fetch(`/api/health${slugQuery}`, { signal: controller.signal });
        clearTimeout(timeoutId);
        setBackendAlive(res.ok);
        if (res.ok) {
          const data = await res.json();
          if (data && data.ue5Alive) {
            serverUeAlive = true;
          }
        }
      } catch (err) {
        setBackendAlive(false);
      }

      // 2. Discover running native Unreal Engine Remote Control API (which runs directly on localhost)
      if (serverUeAlive) {
        setUe5Alive(true);
      } else {
        const targetUE5Urls = [
          selectedClient?.ue5Endpoint || "http://localhost:8008/remote/object/call",
          "http://127.0.0.1:8008/remote/object/call"
        ];

        let ueResponsive = false;
        for (const url of targetUE5Urls) {
          if (!url) continue;
          try {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), 1200);
            await fetch(url, { method: "GET", mode: "no-cors", signal: controller.signal });
            clearTimeout(timeoutId);
            ueResponsive = true;
            break;
          } catch (e) {
            // Unreachable port
          }
        }
        setUe5Alive(ueResponsive);
      }
    };

    probeEndpoints();
    const interval = setInterval(probeEndpoints, 8000);
    return () => clearInterval(interval);
  }, [selectedClient, clients]);

  // Handle Client operations mapped to Firestore
  const handleAddClient = async (newClient: Client) => {
    await createOrUpdateClient(newClient);
    await handleRecordLog({
      clientId: "system",
      clientName: "System Central",
      type: "config_change",
      status: "success",
      details: `Created new client portal profile: '${newClient.name}' [Slug: ${newClient.id}]`,
      payload: JSON.stringify(newClient),
    });
  };

  const handleUpdateClient = async (updatedClient: Client) => {
    await createOrUpdateClient(updatedClient);
    if (selectedClient && selectedClient.id === updatedClient.id) {
      setSelectedClient(updatedClient);
    }
    await handleRecordLog({
      clientId: updatedClient.id,
      clientName: updatedClient.name,
      type: "config_change",
      status: "success",
      details: `Updated workspace settings and theme styling for client: ${updatedClient.name}`,
      payload: JSON.stringify(updatedClient),
    });
  };

  const handleDeleteClient = async (id: string) => {
    // Only Owner or Admin GRP can delete portals/clients
    if (userProfile?.role !== "owner" && userProfile?.role !== "admin") {
      alert("Unauthorized Operation: Only members of the Owner or Admin GRP can delete portal configurations.");
      return;
    }

    const target = clients.find(c => c.id === id);

    // Optimistically remove from local state
    setClients(prev => prev.filter(c => c.id !== id));

    if (selectedClient && selectedClient.id === id) {
      setSelectedClient(null);
      setActiveView("admin");
    }

    try {
      await removeClient(id);
    } catch (err) {
      console.error("Failed to remove client from database:", err);
    }

    await handleRecordLog({
      clientId: "system",
      clientName: "System Central",
      type: "config_change",
      status: "success",
      details: `Removed client portal portfolio: '${target?.name || id}'`,
    });
  };

  const handleRecordLog = async (newLogFields: Omit<Log, "id" | "timestamp">) => {
    const fullLog: Log = {
      ...newLogFields,
      id: `log-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
      timestamp: new Date().toISOString(),
    };
    await writeLog(fullLog);
  };

  const handleClearLogs = async () => {
    if (userProfile?.role !== "owner") {
      alert("Unauthorized Operation: Only members of the Owner GRP can flush database system audit logs.");
      return;
    }
    if (confirm("Are you sure you want to flush and erase all system activity logs in the Cloud database?")) {
      try {
        localStorage.setItem("sightportal_seeded_logs", "true");
        await clearAllLogs();
        await handleRecordLog({
          clientId: "system",
          clientName: "System Central",
          type: "config_change",
          status: "success",
          details: "Owner flushed central database audit log streams.",
        });
        alert("System central audit logs have been successfully cleared.");
      } catch (err: any) {
        console.error("Failed to clear logs:", err);
        alert(`Failed to clear logs: ${err.message || String(err)}`);
      }
    }
  };

  // User RBAC Management Actions
  const handleUpdateUserProfile = async (updatedProfile: UserProfile) => {
    if (userProfile?.role !== "owner" && userProfile?.role !== "admin") {
      alert("Unauthorized operation.");
      return;
    }
    await saveUserProfile(updatedProfile);
    
    // Refresh directory list
    const list = await fetchAllUserProfiles();
    setUserProfilesList(list);

    await handleRecordLog({
      clientId: "system",
      clientName: "Access Control",
      type: "config_change",
      status: "success",
      details: `Updated user permissions: ${updatedProfile.email} configured as role group ${updatedProfile.role.toUpperCase()}`
    });
  };

  const handleDeleteUserProfile = async (uid: string) => {
    if (userProfile?.role !== "owner") {
      alert("Unauthorized: Only the project Owner can delete user records.");
      return;
    }
    const targetProf = userProfilesList.find(u => u.uid === uid);
    if (!targetProf) return;

    if (confirm(`Revoke all access and delete user profile record for '${targetProf.email}'?`)) {
      await deleteUserProfile(uid);
      const list = await fetchAllUserProfiles();
      setUserProfilesList(list);

      await handleRecordLog({
        clientId: "system",
        clientName: "Access Control",
        type: "config_change",
        status: "warning",
        details: `Deleted user profile from system: ${targetProf.email}`
      });
    }
  };

  const handleLaunchClientWorkspace = (client: Client) => {
    setSelectedClient(client);
    setActiveView("client");
  };

  const handleEmailAuth = async (e: FormEvent) => {
    e.preventDefault();
    setAuthError(null);
    setSuggestGoogleAuth(false);
    setFormLoading(true);

    if (!email || !password) {
      setAuthError("Please fill in both email and password.");
      setFormLoading(false);
      return;
    }

    try {
      if (authMode === "signin") {
        await signInWithEmailAndPassword(auth, email, password);
      } else {
        await createUserWithEmailAndPassword(auth, email, password);
      }
    } catch (err: any) {
      const code = err?.code || "";
      if (code === "auth/operation-not-allowed") {
        console.warn("[Firebase Auth] Email/password provider is not enabled in Firebase Console for this project. Prompting Google Sign-In.");
        setSuggestGoogleAuth(true);
        setAuthError(
          "Email & Password authentication is disabled in this Firebase project's console. Please use 'Continue with Google' (configured with Google Sheets & Drive permissions)."
        );
      } else {
        console.warn("[Firebase Auth] Authentication notice:", code || err?.message);
        let errMsg = err.message || "Authentication failed.";
        if (code === "auth/invalid-credential" || code === "auth/user-not-found" || code === "auth/wrong-password") {
          errMsg = "Invalid email or password. Please verify your credentials or try Google Sign-In.";
        } else if (code === "auth/email-already-in-use") {
          errMsg = "This email is already registered. Try signing in instead.";
        } else if (code === "auth/weak-password") {
          errMsg = "Password is too weak. Must be at least 6 characters.";
        } else if (code === "auth/invalid-email") {
          errMsg = "Please enter a valid email address.";
        } else if (code === "auth/network-request-failed") {
          errMsg = "Network request failed. Please check your connection.";
        }
        setAuthError(errMsg);
      }
    } finally {
      setFormLoading(false);
    }
  };

  const handleGoogleLogin = async () => {
    setAuthError(null);
    setSuggestGoogleAuth(false);
    setFormLoading(true);
    try {
      await triggerGoogleLogin();
    } catch (e: any) {
      console.warn("[Google Authentication notice]:", e.message || e);
      setAuthError(e.message || "Google Sign-In popup was closed or interrupted.");
    } finally {
      setFormLoading(false);
    }
  };

  const handleLogout = async () => {
    await signOut(auth);
    setSelectedClient(null);
    setActiveView("admin");
    setAccessDeniedError(null);
    setDeepLinkPortal(null);
  };

  // -------------------------------------------------------------
  // RENDERING CONTROLLER
  // -------------------------------------------------------------

  // Loading state with failsafe controls
  if (authLoading) {
    return (
      <div className="min-h-screen bg-[#050505] flex flex-col items-center justify-center font-mono p-4 text-center select-none">
        <div className="relative mb-5">
          <Compass className="h-10 w-10 text-amber-500 animate-spin" />
          <div className="absolute inset-0 h-10 w-10 bg-amber-500/20 blur-lg rounded-full animate-pulse"></div>
        </div>
        <span className="text-xs text-amber-400 font-bold tracking-widest uppercase mb-1">
          Initializing Secure Stage Channels...
        </span>
        <p className="text-[11px] text-gray-500 max-w-xs font-sans">
          Synchronizing identity channels and stage parameters
        </p>

        <div className="flex flex-wrap items-center justify-center gap-3 mt-8">
          <button
            type="button"
            onClick={() => setAuthLoading(false)}
            className="px-4 py-2 bg-white/5 hover:bg-white/10 border border-white/10 text-gray-300 text-xs rounded-xl transition cursor-pointer flex items-center gap-2"
          >
            <span>Continue to Sign In</span>
            <ChevronRight className="h-3.5 w-3.5 text-amber-400" />
          </button>
          {typeof window !== "undefined" && window.self !== window.top && (
            <a
              href={window.location.href}
              target="_blank"
              rel="noopener noreferrer"
              className="px-4 py-2 bg-amber-500/10 hover:bg-amber-500/20 border border-amber-500/30 text-amber-400 text-xs rounded-xl transition flex items-center gap-2"
            >
              <Monitor className="h-3.5 w-3.5" />
              <span>Open in New Tab</span>
            </a>
          )}
        </div>
      </div>
    );
  }

  // Not Logged In View: Render Beautiful Glass Authentication Card with Email/Password Form
  if (!currentUser) {
    return (
      <div className="min-h-screen bg-[#050505] text-[#e5e5e5] flex flex-col font-sans relative overflow-hidden selection:bg-amber-500 selection:text-black">
        {/* Dynamic ambient backgrounds */}
        <div className="absolute top-0 left-1/4 w-[500px] h-[500px] bg-amber-500/5 rounded-full blur-3xl pointer-events-none"></div>
        <div className="absolute bottom-1/4 right-1/4 w-[600px] h-[600px] bg-orange-600/5 rounded-full blur-3xl pointer-events-none"></div>

        <div className="flex-1 flex items-center justify-center p-4 z-10">
          <div className="w-full max-w-md bg-black/60 border border-white/10 p-8 rounded-2xl relative shadow-2xl backdrop-blur-xl">
            <div className="absolute top-0 right-0 w-24 h-24 bg-gradient-to-br from-amber-500/20 to-transparent blur-xl pointer-events-none"></div>
            
            <div className="text-center space-y-4">
              <div className="inline-flex h-12 w-12 bg-amber-500 rounded-xl items-center justify-center shadow-lg shadow-amber-500/20 border border-amber-400/30 mx-auto">
                <Cpu className="h-6 w-6 text-black" />
              </div>

              <div>
                <h1 className="text-2xl font-bold tracking-tight text-white uppercase font-sans">
                  SightPortal Stage Hub
                </h1>
                <p className="text-xs text-gray-400 mt-1.5 font-mono">
                  UE5 Multi-Bridge & Spreadsheets Dispatcher
                </p>
              </div>

              {/* Error Box with Action Resolution */}
              {authError && (
                <div className="bg-rose-500/10 border border-rose-500/30 p-3.5 rounded-xl text-left text-xs text-rose-300 space-y-2.5 font-mono">
                  <div className="flex items-start gap-2">
                    <ShieldAlert className="h-4 w-4 shrink-0 mt-0.5 text-rose-400" />
                    <span className="leading-relaxed">{authError}</span>
                  </div>
                  {suggestGoogleAuth && (
                    <button
                      type="button"
                      onClick={handleGoogleLogin}
                      disabled={formLoading}
                      className="w-full py-2.5 px-3 bg-gradient-to-r from-amber-500 to-amber-400 hover:from-amber-400 hover:to-amber-300 text-black font-bold text-xs rounded-lg transition flex items-center justify-center gap-2 cursor-pointer shadow-lg font-sans"
                    >
                      <span>Sign In with Google Account</span>
                      <ChevronRight className="h-3.5 w-3.5" />
                    </button>
                  )}
                </div>
              )}

              {/* Primary Authentication: Google Sign-In */}
              <div className="space-y-2 pt-1">
                <button
                  type="button"
                  onClick={handleGoogleLogin}
                  disabled={formLoading}
                  className="w-full py-3 px-4 bg-white hover:bg-gray-100 text-gray-900 font-bold text-xs rounded-xl shadow-lg transition flex items-center justify-center gap-3 cursor-pointer disabled:opacity-50 border border-white/20 active:scale-[0.99]"
                >
                  {formLoading ? (
                    <Compass className="h-4 w-4 text-gray-900 animate-spin" />
                  ) : (
                    <svg className="h-4 w-4 shrink-0" viewBox="0 0 24 24">
                      <path
                        fill="#4285F4"
                        d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z"
                      />
                      <path
                        fill="#34A853"
                        d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"
                      />
                      <path
                        fill="#FBBC05"
                        d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.06H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.94l2.85-2.22.81-.63z"
                      />
                      <path
                        fill="#EA4335"
                        d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.06l3.66 2.84c.87-2.6 3.3-4.52 6.16-4.52z"
                      />
                    </svg>
                  )}
                  <span>Continue with Google (Recommended)</span>
                </button>

                <div className="flex items-center justify-between text-[10px] text-gray-400 px-1 font-mono">
                  <span>Owner & Google Drive Access</span>
                  <span className="text-emerald-400 font-semibold">Active Provider</span>
                </div>
              </div>

              {/* Iframe Preview Helper */}
              {typeof window !== "undefined" && window.self !== window.top && (
                <div className="p-3 bg-amber-500/10 border border-amber-500/20 rounded-xl text-left text-[11px] text-amber-400 space-y-2">
                  <p className="font-sans leading-normal">
                    ⚠️ <strong>Preview Iframe Notice:</strong> Browser security blocks popups inside embedded preview frames.
                  </p>
                  <div className="flex gap-2">
                    <a
                      href={window.location.href}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="inline-flex items-center gap-1.5 px-3 py-1.5 bg-amber-500 hover:bg-amber-600 text-black font-bold rounded-lg text-[10px] uppercase tracking-wider font-mono transition-colors"
                    >
                      <Monitor className="h-3 w-3" />
                      Open in New Tab to Sign In
                    </a>
                  </div>
                </div>
              )}

              {/* Secondary Option Divider */}
              <div className="relative flex py-2 items-center">
                <div className="flex-grow border-t border-white/10"></div>
                <span className="flex-shrink mx-4 text-[9px] text-gray-500 font-mono uppercase tracking-wider">
                  Or email & password
                </span>
                <div className="flex-grow border-t border-white/10"></div>
              </div>

              {/* Toggle Tab for Email */}
              <div className="grid grid-cols-2 bg-white/5 p-1 rounded-xl border border-white/5">
                <button
                  type="button"
                  onClick={() => {
                    setAuthMode("signin");
                    setAuthError(null);
                    setSuggestGoogleAuth(false);
                  }}
                  className={`py-2 rounded-lg text-xs font-mono font-bold uppercase transition ${
                    authMode === "signin"
                      ? "bg-amber-500 text-black shadow"
                      : "text-gray-400 hover:text-white"
                  }`}
                >
                  Sign In
                </button>
                <button
                  type="button"
                  onClick={() => {
                    setAuthMode("signup");
                    setAuthError(null);
                    setSuggestGoogleAuth(false);
                  }}
                  className={`py-2 rounded-lg text-xs font-mono font-bold uppercase transition ${
                    authMode === "signup"
                      ? "bg-amber-500 text-black shadow"
                      : "text-gray-400 hover:text-white"
                  }`}
                >
                  Sign Up
                </button>
              </div>

              {/* Credential Form */}
              <form onSubmit={handleEmailAuth} className="space-y-4 text-left">
                <div>
                  <label className="block text-[10px] font-mono uppercase tracking-wider text-gray-400 mb-1.5">
                    Email Address
                  </label>
                  <input
                    type="email"
                    value={email}
                    onChange={(e) => setEmail(e.target.value)}
                    placeholder="name@example.com"
                    required
                    className="w-full px-4 py-3 bg-white/5 border border-white/10 rounded-xl text-xs font-mono text-white placeholder-gray-500 focus:outline-none focus:ring-1 focus:ring-amber-500/50 focus:border-amber-500/50 transition-all"
                  />
                </div>

                <div>
                  <label className="block text-[10px] font-mono uppercase tracking-wider text-gray-400 mb-1.5">
                    Password
                  </label>
                  <input
                    type="password"
                    value={password}
                    onChange={(e) => setPassword(e.target.value)}
                    placeholder="••••••••"
                    required
                    className="w-full px-4 py-3 bg-white/5 border border-white/10 rounded-xl text-xs font-mono text-white placeholder-gray-500 focus:outline-none focus:ring-1 focus:ring-amber-500/50 focus:border-amber-500/50 transition-all"
                  />
                </div>

                <button
                  type="submit"
                  disabled={formLoading}
                  className="w-full py-3 px-4 bg-amber-500/20 hover:bg-amber-500/30 text-amber-300 hover:text-amber-200 border border-amber-500/30 active:scale-[0.98] transition-all rounded-xl font-bold text-xs font-mono uppercase tracking-wider flex items-center justify-center gap-2 cursor-pointer disabled:opacity-50"
                >
                  {formLoading ? (
                    <Compass className="h-4 w-4 text-amber-300 animate-spin" />
                  ) : authMode === "signin" ? (
                    <>
                      Sign In with Password
                      <ChevronRight className="h-3.5 w-3.5" />
                    </>
                  ) : (
                    <>
                      Create New Account
                      <ChevronRight className="h-3.5 w-3.5" />
                    </>
                  )}
                </button>
              </form>
            </div>
          </div>
        </div>

        <footer className="py-6 text-center text-[10px] text-gray-600 font-mono z-10">
          SightPortal Connection Infrastructure Secured via Zero-Trust ABAC • v1.4.2
        </footer>
      </div>
    );
  }

  // Handle Access Denied warning state for Restricted Client groups
  if (accessDeniedError) {
    return (
      <div className="min-h-screen bg-[#050505] text-[#e5e5e5] flex flex-col font-sans items-center justify-center p-4">
        <div className="w-full max-w-lg bg-black/40 border border-rose-500/20 rounded-2xl p-8 shadow-2xl backdrop-blur-md">
          <div className="text-center space-y-4">
            <div className="h-12 w-12 bg-rose-500/10 rounded-full border border-rose-500/20 flex items-center justify-center mx-auto text-rose-400">
              <ShieldAlert className="h-6 w-6" />
            </div>
            <h2 className="text-lg font-bold text-white uppercase">Portal Restriced</h2>
            <p className="text-xs text-gray-400 leading-relaxed font-mono px-4">
              {accessDeniedError}
            </p>
            <div className="pt-4 flex justify-center gap-3">
              <button
                onClick={handleLogout}
                className="px-4 py-2 bg-white/5 border border-white/10 rounded-lg text-xs font-mono text-gray-400 hover:text-white transition cursor-pointer"
              >
                Sign Out
              </button>
              {deepLinkPortal && (
                <button
                  onClick={() => {
                    setAccessDeniedError(null);
                    setDeepLinkPortal(null);
                    window.location.search = "";
                  }}
                  className="px-4 py-2 bg-amber-500 hover:bg-amber-600 rounded-lg text-xs font-mono text-black font-bold transition cursor-pointer"
                >
                  Return Home
                </button>
              )}
            </div>
          </div>
        </div>
      </div>
    );
  }

  const isDeveloperUser = userProfile?.role === "owner" || userProfile?.role === "admin";

  return (
    <div className="min-h-screen bg-[#050505] text-[#e5e5e5] flex flex-col font-sans selection:bg-amber-500 selection:text-black" id="app-wrapper">
      
      {/* Universal Sticky Glass Top Bar */}
      <header className="bg-[var(--surface)] border-b border-[var(--ink-faint)] sticky top-0 z-55 flex-none" id="app-nav-bar">
        <div className="w-full px-6 py-3 flex items-center justify-between gap-4">
          
          {/* Logo & Platform Name */}
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 bg-[var(--accent)] text-[#1e2025] grid place-items-center rounded select-none">
              <span className="font-['Syne'] font-extrabold text-sm">U</span>
            </div>
            <div>
              <div className="h-display text-[1.1rem] text-[var(--accent)]">Multi-Bridge</div>
              <div className="label text-[0.55rem]">Stage Hub Control</div>
            </div>
          </div>

          {/* Quick View Swap Buttons (Hidden from restricted clients) */}
          {isDeveloperUser && (
            <div className="flex items-center justify-center gap-3">
              <button
                id="admin-view-toggle"
                onClick={() => setActiveView("admin")}
                className={`btn-ghost ${activeView === "admin" ? "border-[var(--accent)] text-[var(--ink)]" : ""}`}
              >
                Admin Console
              </button>

              <select
                id="portal-quick-selector"
                value={activeView === "client" && selectedClient ? selectedClient.id : ""}
                onChange={(e) => {
                  const clientObj = clients.find(c => c.id === e.target.value);
                  if (clientObj) {
                    handleLaunchClientWorkspace(clientObj);
                  } else {
                    setActiveView("admin");
                  }
                }}
                className="portal-select w-[200px] sm:w-[240px]"
              >
                <option value="">Select Portal...</option>
                {clients.map((c) => (
                  <option key={c.id} value={c.id}>
                    Portal: {c.name}
                  </option>
                ))}
              </select>
            </div>
          )}

          {/* Dynamic User Profile Menu & Log Out */}
          <div className="flex items-center gap-4">
            <div className="text-right">
              <div className="font-semibold text-[0.75rem] text-[var(--ink)] truncate max-w-[160px]" title={currentUser.email || ""}>
                {currentUser.email}
              </div>
              <div className="label text-[var(--accent)] text-[0.55rem]">
                {userProfile?.role?.toUpperCase()} GRP
              </div>
            </div>

            <button 
              onClick={handleLogout}
              className="btn-ghost px-3 py-1.5"
              title="Sign Out of Stage Hub"
            >
              LOGOUT
            </button>
          </div>

        </div>
      </header>

      {/* Firestore Quota Exceeded Alert Banner */}
      {quotaInfo.exceeded && !quotaBannerDismissed && (
        <div className="bg-amber-950/80 border-b border-amber-500/30 px-6 py-3 text-xs text-amber-200 flex flex-col md:flex-row items-start md:items-center justify-between gap-3 shadow-lg z-50 backdrop-blur-md">
          <div className="flex items-start gap-2.5">
            <AlertTriangle className="h-4 w-4 text-amber-400 shrink-0 mt-0.5" />
            <div>
              <div className="font-semibold text-amber-300 tracking-wide font-mono flex flex-wrap items-center gap-2">
                <span>FIRESTORE DAILY READ QUOTA EXCEEDED (FREE TIER)</span>
                <span className="bg-amber-500/20 text-amber-300 text-[10px] px-2 py-0.5 rounded border border-amber-500/30">Local Cache Fallback Active</span>
              </div>
              <p className="text-[11px] text-amber-200/80 mt-0.5 leading-relaxed">
                The free daily read quota (50,000 read units/day) for this Firestore database has been reached. Quotas reset automatically every 24 hours (at midnight PST). The portal is operating smoothly using locally cached client configurations.
              </p>
            </div>
          </div>
          <div className="flex items-center gap-2 shrink-0 self-end md:self-auto">
            <button
              type="button"
              onClick={() => {
                resetQuotaStatus();
                handleForceSyncDatabase();
              }}
              className="px-2.5 py-1.5 bg-amber-500/20 hover:bg-amber-500/30 text-amber-200 border border-amber-500/40 font-mono text-[11px] rounded transition flex items-center gap-1.5 cursor-pointer"
              title="Reset quota guard and attempt direct database sync"
            >
              <RefreshCw className="h-3 w-3" />
              <span>Retry Sync</span>
            </button>
            <a
              href={quotaInfo.url}
              target="_blank"
              rel="noopener noreferrer"
              className="px-3 py-1.5 bg-amber-500 hover:bg-amber-400 text-black font-bold font-mono text-[11px] rounded transition flex items-center gap-1.5 shadow"
            >
              <span>Upgrade in Firebase Console</span>
              <ExternalLink className="h-3 w-3" />
            </a>
            <button
              type="button"
              onClick={() => setQuotaBannerDismissed(true)}
              className="p-1.5 hover:bg-amber-500/20 rounded text-amber-300 transition cursor-pointer"
              title="Dismiss notice"
            >
              <X className="h-3.5 w-3.5" />
            </button>
          </div>
        </div>
      )}

      {/* Main Container */}
      <main className="flex-1 w-full" id="app-main-content">
        {activeView === "admin" && isDeveloperUser ? (
          <AdminConsole
            clients={clients}
            logs={logs}
            onAddClient={handleAddClient}
            onUpdateClient={handleUpdateClient}
            onDeleteClient={handleDeleteClient}
            onClearLogs={handleClearLogs}
            onSelectClientView={handleLaunchClientWorkspace}
            currentUserProfile={userProfile}
            userProfiles={userProfilesList}
            onUpdateUserProfile={handleUpdateUserProfile}
            onDeleteUserProfile={handleDeleteUserProfile}
            onForceSyncDatabase={handleForceSyncDatabase}
            onPurgeDefaultClients={handlePurgeDefaultClients}
            isSyncing={isSyncingDb}
            syncStatus={syncDbStatus}
            databaseId={configuredDatabaseId}
          />
        ) : selectedClient ? (
          <ClientDashboard
            client={selectedClient}
            onBackToAdmin={() => setActiveView("admin")}
            onRecordLog={handleRecordLog}
            onUpdateClient={handleUpdateClient}
            showBackToAdmin={isDeveloperUser}
          />
        ) : (
          <div className="text-center py-24 glass rounded-2xl border border-[var(--ink-faint)] shadow-xl max-w-lg mx-auto my-12">
            <Compass className="h-12 w-12 text-[var(--accent)] mx-auto mb-4 animate-spin-slow" />
            <h2 className="h-display text-lg text-white">No Staging Portal Assigned</h2>
            <p className="text-[var(--ink-muted)] text-xs mt-2 px-4 font-mono">
              Awaiting Developer configurations. If your profile was recently registered, ask the Owner or Admin to delegate a portal slug.
            </p>
          </div>
        )}
      </main>

      {/* Universal Footer */}
      <footer className="bg-[var(--surface)] border-t border-[var(--ink-faint)] px-8 py-3 text-xs flex-none flex flex-col md:flex-row items-center justify-between gap-4">
        <div className="flex items-center text-[0.7rem]">
          <span className="status-dot bg-pulse"></span>
          <span className="label text-[var(--ink)] tracking-wide">SightPortal v1.4.2</span>
          <span className="ml-6 opacity-40 font-mono text-[0.65rem]">CLOUD_DB_INTEGRATION: ACTIVE</span>
        </div>

        {/* Real-Time Connectivity Diagnostic Indicators */}
        <div className="flex items-center gap-6">
          <div className="label text-[0.6rem]">
            Pipeline:{" "}
            <span className={ue5Alive ? "text-emerald-400" : "text-amber-400"}>
              {ue5Alive ? "Linked Engine" : "Awaiting Engine"}
            </span>
          </div>

          <button
            onClick={handleForceSync}
            disabled={syncing}
            title="Force trigger a manual re-sync of active client parameters to the Unreal Engine 5 instance"
            className="btn-primary py-1.5 px-4 text-[0.65rem] flex items-center gap-2"
          >
            <RefreshCw className={`h-3 w-3 ${syncing ? "animate-spin" : ""}`} />
            <span>{syncing ? "Syncing..." : "Force Refresh"}</span>
          </button>
        </div>
      </footer>
    </div>
  );
}
