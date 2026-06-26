/**
 * @license
 * SPDX-License-Identifier: Apache-2.5
 */

import { useState, useEffect, FormEvent } from "react";
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
  triggerGoogleLogin
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
  Trash2
} from "lucide-react";

export default function App() {
  const [activeView, setActiveView] = useState<"admin" | "client">("admin");
  const [clients, setClients] = useState<Client[]>([]);
  const [logs, setLogs] = useState<Log[]>([]);
  const [selectedClient, setSelectedClient] = useState<Client | null>(null);

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

  // Listen for Authentication state changes
  useEffect(() => {
    const unsubscribe = onAuthStateChanged(auth, async (user) => {
      setAuthLoading(true);
      if (user) {
        setCurrentUser(user);
        
        // Retrieve or bootstrap User Profile Role
        let profile = await fetchUserProfile(user.uid);
        
        // Auto-bootstrap Owner account based on metadata verified email
        const isOwnerEmail = user.email === "raed.sight@gmail.com";
        
        if (!profile) {
          // New user defaults to client unless matching owner email
          const defaultRole = isOwnerEmail ? "owner" : "client";
          profile = {
            uid: user.uid,
            email: user.email || "",
            role: defaultRole,
            clientId: null
          };
          await saveUserProfile(profile);
        } else if (isOwnerEmail && profile.role !== "owner") {
          // Enforce owner credentials state integrity
          profile.role = "owner";
          await saveUserProfile(profile);
        }

        setUserProfile(profile);

        // Fetch user profiles list for User Directory Tab if Owner/Admin
        if (profile.role === "owner" || profile.role === "admin") {
          const list = await fetchAllUserProfiles();
          setUserProfilesList(list);
        }

        // Record a sign in audit log
        const initialLog: Log = {
          id: `log-${Date.now()}-${Math.random().toString(36).substring(2, 6)}`,
          clientId: "system",
          clientName: "Identity Management",
          timestamp: new Date().toISOString(),
          type: "config_change",
          status: "success",
          details: `User logged in successfully: ${user.email} (${profile.role.toUpperCase()} GRP)`
        };
        await writeLog(initialLog);

      } else {
        setCurrentUser(null);
        setUserProfile(null);
        setUserProfilesList([]);
      }
      setAuthLoading(false);
    });

    return () => unsubscribe();
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
        unsubscribe = await syncClients((updatedClients) => {
          // Seed initial presets if collection is empty
          if (updatedClients.length === 0) {
            console.log("[Firestore] Seeding initial client portal profiles...");
            DEFAULT_CLIENTS.forEach(async (c) => {
              await createOrUpdateClient(c);
            });
          } else {
            setClients(updatedClients);
          }
        });
      } else if (userProfile.role === "client" && userProfile.clientId) {
        unsubscribe = await syncSingleClient(userProfile.clientId, (updatedClient) => {
          if (updatedClient) {
            setClients([updatedClient]);
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
      unsubscribe = await syncLogs((updatedLogs) => {
        if (updatedLogs.length === 0) {
          DEFAULT_LOGS.forEach(async (l) => {
            await writeLog(l);
          });
        } else {
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

  // Poll connection health and Unreal presence
  useEffect(() => {
    const probeEndpoints = async () => {
      // 1. Probe the main server health route
      try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 2000);
        const res = await fetch("/api/health", { signal: controller.signal });
        clearTimeout(timeoutId);
        setBackendAlive(res.ok);
      } catch (err) {
        setBackendAlive(false);
      }

      // 2. Discover running native Unreal Engine Remote Control API (which runs directly on localhost)
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
    // Only Owner GRP can delete portals/clients
    if (userProfile?.role !== "owner") {
      alert("Unauthorized Operation: Only members of the Owner GRP can delete portal configurations.");
      return;
    }

    const target = clients.find(c => c.id === id);
    await removeClient(id);

    if (selectedClient && selectedClient.id === id) {
      setSelectedClient(null);
      setActiveView("admin");
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
      await clearAllLogs(logs);
      await handleRecordLog({
        clientId: "system",
        clientName: "System Central",
        type: "config_change",
        status: "success",
        details: "Owner flushed central database audit log streams.",
      });
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
      console.error("Email authentication failed:", err);
      let errMsg = err.message || "Authentication failed.";
      if (err.code === "auth/invalid-credential" || err.code === "auth/user-not-found" || err.code === "auth/wrong-password") {
        errMsg = "Invalid email or password. Please try again.";
      } else if (err.code === "auth/email-already-in-use") {
        errMsg = "This email is already registered. Try signing in instead.";
      } else if (err.code === "auth/weak-password") {
        errMsg = "Password is too weak. Must be at least 6 characters.";
      } else if (err.code === "auth/invalid-email") {
        errMsg = "Please enter a valid email address.";
      }
      setAuthError(errMsg);
    } finally {
      setFormLoading(false);
    }
  };

  const handleGoogleLogin = async () => {
    setAuthError(null);
    setFormLoading(true);
    try {
      await triggerGoogleLogin();
    } catch (e: any) {
      console.error("Google Authentication failed:", e);
      setAuthError(e.message || "Google Sign-In popup was closed or failed.");
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

  // Loading state
  if (authLoading) {
    return (
      <div className="min-h-screen bg-[#050505] flex flex-col items-center justify-center font-mono">
        <Compass className="h-10 w-10 text-blue-500 animate-spin mb-4" />
        <span className="text-xs text-gray-400 tracking-widest uppercase">Initializing Secure Stage Channels...</span>
      </div>
    );
  }

  // Not Logged In View: Render Beautiful Glass Authentication Card with Email/Password Form
  if (!currentUser) {
    return (
      <div className="min-h-screen bg-[#050505] text-[#e5e5e5] flex flex-col font-sans relative overflow-hidden selection:bg-blue-600">
        {/* Dynamic ambient backgrounds */}
        <div className="absolute top-0 left-1/4 w-[500px] h-[500px] bg-blue-600/5 rounded-full blur-3xl pointer-events-none"></div>
        <div className="absolute bottom-1/4 right-1/4 w-[600px] h-[600px] bg-purple-600/5 rounded-full blur-3xl pointer-events-none"></div>

        <div className="flex-1 flex items-center justify-center p-4 z-10">
          <div className="w-full max-w-md bg-black/60 border border-white/10 p-8 rounded-2xl relative shadow-2xl backdrop-blur-xl">
            <div className="absolute top-0 right-0 w-24 h-24 bg-gradient-to-br from-blue-500/20 to-transparent blur-xl pointer-events-none"></div>
            
            <div className="text-center space-y-4">
              <div className="inline-flex h-12 w-12 bg-blue-600 rounded-xl items-center justify-center shadow-lg shadow-blue-500/20 border border-blue-400/30 mx-auto">
                <Cpu className="h-6 w-6 text-white" />
              </div>

              <div>
                <h1 className="text-2xl font-bold tracking-tight text-white uppercase font-sans">
                  SightPortal Stage Hub
                </h1>
                <p className="text-xs text-gray-400 mt-1.5 font-mono">
                  UE5 Multi-Bridge & Spreadsheets Dispatcher
                </p>
              </div>

              {/* Toggle Tab */}
              <div className="grid grid-cols-2 bg-white/5 p-1 rounded-xl border border-white/5">
                <button
                  type="button"
                  onClick={() => {
                    setAuthMode("signin");
                    setAuthError(null);
                  }}
                  className={`py-2 rounded-lg text-xs font-mono font-bold uppercase transition ${
                    authMode === "signin"
                      ? "bg-blue-600 text-white shadow"
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
                  }}
                  className={`py-2 rounded-lg text-xs font-mono font-bold uppercase transition ${
                    authMode === "signup"
                      ? "bg-blue-600 text-white shadow"
                      : "text-gray-400 hover:text-white"
                  }`}
                >
                  Sign Up
                </button>
              </div>

              {/* Error Box */}
              {authError && (
                <div className="bg-rose-500/10 border border-rose-500/30 p-3 rounded-lg text-left text-xs text-rose-400 flex items-start gap-2 animate-pulse font-mono">
                  <ShieldAlert className="h-4 w-4 shrink-0 mt-0.5" />
                  <span>{authError}</span>
                </div>
              )}

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
                    className="w-full px-4 py-3 bg-white/5 border border-white/10 rounded-xl text-xs font-mono text-white placeholder-gray-500 focus:outline-none focus:ring-1 focus:ring-blue-500/50 focus:border-blue-500/50 transition-all"
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
                    className="w-full px-4 py-3 bg-white/5 border border-white/10 rounded-xl text-xs font-mono text-white placeholder-gray-500 focus:outline-none focus:ring-1 focus:ring-blue-500/50 focus:border-blue-500/50 transition-all"
                  />
                </div>

                <button
                  type="submit"
                  disabled={formLoading}
                  className="w-full py-3.5 px-4 bg-blue-600 hover:bg-blue-500 hover:shadow-lg hover:shadow-blue-600/20 active:scale-[0.98] transition-all rounded-xl text-white font-bold text-xs font-mono uppercase tracking-wider flex items-center justify-center gap-2 cursor-pointer border border-blue-400/20 disabled:opacity-50"
                >
                  {formLoading ? (
                    <Compass className="h-4 w-4 text-white animate-spin" />
                  ) : authMode === "signin" ? (
                    <>
                      Sign In Account
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

              {/* Decorative line */}
              <div className="relative flex py-2 items-center">
                <div className="flex-grow border-t border-white/10"></div>
                <span className="flex-shrink mx-4 text-[9px] text-gray-500 font-mono uppercase tracking-wider">
                  Or use corporate login
                </span>
                <div className="flex-grow border-t border-white/10"></div>
              </div>

              {/* Google Account Button */}
              <button
                type="button"
                onClick={handleGoogleLogin}
                disabled={formLoading}
                className="w-full py-2.5 px-4 bg-white/5 hover:bg-white/10 transition-all rounded-xl text-gray-300 font-bold text-xs font-mono uppercase tracking-wider flex items-center justify-center gap-2 cursor-pointer border border-white/10 disabled:opacity-50"
              >
                Sign In with Google
              </button>
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
                  className="px-4 py-2 bg-blue-600 hover:bg-blue-500 rounded-lg text-xs font-mono text-white transition cursor-pointer"
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
    <div className="min-h-screen bg-[#050505] text-[#e5e5e5] flex flex-col font-sans selection:bg-blue-600 selection:text-white" id="app-wrapper">
      
      {/* Universal Sticky Glass Top Bar */}
      <header className="bg-[#0A0A0A] border-b border-white/10 sticky top-0 z-55 flex-none" id="app-nav-bar">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 h-16 flex items-center justify-between gap-4">
          
          {/* Logo & Platform Name */}
          <div className="flex items-center gap-3">
            <div className="h-9 w-9 bg-blue-600 rounded flex items-center justify-center shadow-md select-none">
              <span className="text-sm font-black text-white">U</span>
            </div>
            <div>
              <span className="text-sm font-semibold tracking-wider text-white uppercase">
                UE5 MULTI-BRIDGE
              </span>
              <span className="text-[10px] block font-mono text-gray-500 uppercase">Interactive Stage Hub</span>
            </div>
          </div>

          {/* Quick View Swap Buttons (Hidden from restricted clients) */}
          {isDeveloperUser && (
            <div className="flex items-center gap-2">
              <button
                id="admin-view-toggle"
                onClick={() => setActiveView("admin")}
                className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-xs font-semibold cursor-pointer transition select-none ${
                  activeView === "admin"
                    ? "bg-white/5 border border-white/10 text-white shadow-lg"
                    : "text-gray-400 hover:text-white hover:bg-white/5"
                }`}
              >
                <Monitor className="h-3.5 w-3.5" />
                <span className="hidden sm:inline">Admin Console</span>
              </button>

              <div className="h-4 w-px bg-white/10"></div>

              <div className="relative inline-block">
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
                  className="px-2.5 py-1.5 text-xs bg-[#0A0A0A] hover:bg-white/5 border border-white/10 text-gray-300 rounded-lg focus:outline-none focus:ring-1 focus:ring-blue-500/50 cursor-pointer text-ellipsis max-w-[150px] sm:max-w-[200px]"
                >
                  <option value="">Select Portal...</option>
                  {clients.map((c) => (
                    <option key={c.id} value={c.id}>
                      Portal: {c.name}
                    </option>
                  ))}
                </select>
              </div>
            </div>
          )}

          {/* Dynamic User Profile Menu & Log Out */}
          <div className="flex items-center gap-3">
            <div className="text-right">
              <span className="text-xs text-gray-300 block font-semibold leading-none truncate max-w-[150px]" title={currentUser.email || ""}>
                {currentUser.email}
              </span>
              <span className="text-[9px] font-mono text-blue-400 uppercase tracking-widest mt-0.5 block">
                {userProfile?.role} GRP
              </span>
            </div>

            <button 
              onClick={handleLogout}
              className="p-1.5 rounded-lg border border-white/10 bg-white/5 hover:bg-white/10 text-gray-400 hover:text-rose-400 transition-colors cursor-pointer"
              title="Sign Out of Stage Hub"
            >
              <LogOut className="h-3.5 w-3.5" />
            </button>
          </div>

        </div>
      </header>

      {/* Main Container */}
      <main className="flex-1 max-w-7xl mx-auto w-full px-4 sm:px-6 lg:px-8 py-8" id="app-main-content">
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
          <div className="text-center py-24 glass rounded-2xl border border-white/10 shadow-xl max-w-lg mx-auto">
            <Compass className="h-12 w-12 text-gray-600 mx-auto mb-4 animate-spin-slow" />
            <h2 className="text-lg font-bold text-white">No Staging Portal Assigned</h2>
            <p className="text-gray-500 text-xs mt-1.5 px-4 font-mono">
              Awaiting Developer configurations. If your profile was recently registered, ask the Owner or Admin to delegate a portal slug.
            </p>
          </div>
        )}
      </main>

      {/* Sticky Universal Footer */}
      <footer className="bg-[#070707] border-t border-white/5 py-3 text-xs text-gray-500 font-mono mt-auto flex-none">
        <div className="max-w-7xl mx-auto px-4 flex flex-col md:flex-row items-center justify-between gap-4">
          <div className="flex items-center gap-2">
            <span className="h-1.5 w-1.5 rounded-full bg-blue-500 shadow-[0_0_6px_rgba(59,130,246,0.6)] animate-pulse"></span>
            <span>
              SightPortal UE5 Multi-Bridge v1.4.2 • Cloud DB Integration Enabled
            </span>
          </div>

          {/* Real-Time Connectivity Diagnostic Indicators */}
          <div className="flex flex-wrap items-center gap-x-5 gap-y-2 text-[10px] md:text-[11px]">
            {/* 1. Dashboard API Server Indicator */}
            <div className="flex items-center gap-2 px-2.5 py-1 bg-white/5 border border-white/10 rounded-lg">
              <span className="text-gray-400">Database Status:</span>
              <span className="flex items-center gap-1.5">
                <span className={`h-2 w-2 rounded-full ${
                  backendAlive 
                    ? "bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.8)]" 
                    : "bg-rose-500 shadow-[0_0_8px_rgba(244,63,94,0.8)] animate-pulse"
                }`}></span>
                <span className={backendAlive ? "text-emerald-400 font-medium animate-pulse" : "text-rose-400 font-medium"}>
                  {backendAlive ? "Cloud Sync" : "Local Fallback"}
                </span>
              </span>
            </div>

            {/* 2. Unreal Engine Direct Pipeline Link */}
            <div className="flex items-center gap-2 px-2.5 py-1 bg-white/5 border border-white/10 rounded-lg">
              <Server className="h-3 w-3 text-gray-400" />
              <span className="text-gray-400">Unreal Pipeline:</span>
              <span className="flex items-center gap-1.5">
                <span className={`h-2 w-2 rounded-full ${
                  ue5Alive 
                    ? "bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.8)]" 
                    : "bg-amber-500 shadow-[0_0_8px_rgba(245,158,11,0.8)] animate-pulse"
                }`}></span>
                <span className={ue5Alive ? "text-emerald-400 font-medium" : "text-amber-400 font-medium"}>
                  {ue5Alive ? "Linked (Direct IPC)" : "Awaiting Engine"}
                </span>
              </span>
              {ue5Alive ? (
                <Wifi className="h-3.5 w-3.5 text-emerald-400 animate-pulse ml-0.5" />
              ) : (
                <WifiOff className="h-3.5 w-3.5 text-amber-500 opacity-60 ml-0.5" />
              )}
            </div>
          </div>
        </div>
      </footer>
    </div>
  );
}
