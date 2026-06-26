/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import { useState, useEffect } from "react";
import { 
  getStoredClients, 
  saveStoredClients, 
  getStoredLogs, 
  saveStoredLogs, 
  DEFAULT_LOGS, 
  DEFAULT_CLIENTS 
} from "./data";
import { Client, Log } from "./types";
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
  Radio,
  Activity,
  AlertOctagon
} from "lucide-react";

export default function App() {
  const [activeView, setActiveView] = useState<"admin" | "client">("admin");
  const [clients, setClients] = useState<Client[]>([]);
  const [logs, setLogs] = useState<Log[]>([]);
  const [selectedClient, setSelectedClient] = useState<Client | null>(null);

  // Probe states for real-time connection status
  const [backendAlive, setBackendAlive] = useState<boolean>(true);
  const [ue5Alive, setUe5Alive] = useState<boolean>(false);

  // Initialize and load clients and logs
  useEffect(() => {
    const loadedClients = getStoredClients();
    const loadedLogs = getStoredLogs();
    
    setClients(loadedClients);
    setLogs(loadedLogs);
  }, []);

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
          // no-cors bypasses preflight blocks allowing open loop check
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

    // Initial check on load or change
    probeEndpoints();

    // Re-check periodically every 6 seconds
    const interval = setInterval(probeEndpoints, 6000);
    return () => clearInterval(interval);
  }, [selectedClient, clients]);

  // Handle client creations
  const handleAddClient = (newClient: Client) => {
    const updatedClients = [newClient, ...clients];
    setClients(updatedClients);
    saveStoredClients(updatedClients);

    // Record system config log
    handleRecordLog({
      clientId: "system",
      clientName: "System Central",
      type: "config_change",
      status: "success",
      details: `Created new client portal profile: '${newClient.name}' [Slug: ${newClient.id}]`,
      payload: JSON.stringify(newClient),
    });
  };

  // Handle client configurations updates
  const handleUpdateClient = (updatedClient: Client) => {
    const updatedClients = clients.map((c) => (c.id === updatedClient.id ? updatedClient : c));
    setClients(updatedClients);
    saveStoredClients(updatedClients);

    // Update selected client if currently active to keep branding synced
    if (selectedClient && selectedClient.id === updatedClient.id) {
      setSelectedClient(updatedClient);
    }

    // Record system log
    handleRecordLog({
      clientId: updatedClient.id,
      clientName: updatedClient.name,
      type: "config_change",
      status: "success",
      details: `Updated workspace settings and theme styling for client: ${updatedClient.name}`,
      payload: JSON.stringify(updatedClient),
    });
  };

  // Handle client deletions
  const handleDeleteClient = (id: string) => {
    const target = clients.find(c => c.id === id);
    const updatedClients = clients.filter((c) => c.id !== id);
    setClients(updatedClients);
    saveStoredClients(updatedClients);

    if (selectedClient && selectedClient.id === id) {
      setSelectedClient(null);
      setActiveView("admin");
    }

    // Record system log
    handleRecordLog({
      clientId: "system",
      clientName: "System Central",
      type: "config_change",
      status: "success",
      details: `Removed client portal portfolio: '${target?.name || id}'`,
    });
  };

  const handleRecordLog = (newLogFields: Omit<Log, "id" | "timestamp">) => {
    const fullLog: Log = {
      ...newLogFields,
      id: `log-${Date.now()}-${Math.random().toString(36).substr(2, 4)}`,
      timestamp: new Date().toISOString(),
    };
    
    // Set a max capacity check for logs to prevent local storage quota locks
    setLogs((prevLogs) => {
      const trimmed = [fullLog, ...prevLogs].slice(0, 100);
      saveStoredLogs(trimmed);
      return trimmed;
    });
  };

  const handleClearLogs = () => {
    if (confirm("Are you sure you want to flush and erase all system activity logs?")) {
      setLogs([]);
      saveStoredLogs([]);
    }
  };

  const handleLaunchClientWorkspace = (client: Client) => {
    setSelectedClient(client);
    setActiveView("client");
  };

  return (
    <div className="min-h-screen bg-[#050505] text-[#e5e5e5] flex flex-col font-sans selection:bg-blue-600 selection:text-white" id="app-wrapper">
      
      {/* Universal Sticky Glass Top Bar for Developer / Director Navigation */}
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

          {/* Quick View Swap Buttons */}
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

            {/* Quick Access Client Selector */}
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

          {/* User Sign-In Mock / Developer HUD Metadata */}
          <div className="hidden lg:flex items-center gap-2 text-right">
            <div className="text-xs">
              <span className="text-gray-300 block font-semibold leading-none">raed.sight@gmail.com</span>
              <span className="text-[10px] font-mono text-blue-400">Owner Account Verified</span>
            </div>
            <div className="h-8 w-8 rounded-full bg-blue-600/20 border border-blue-500/40 flex items-center justify-center font-bold text-blue-400 text-xs shadow-inner">
              RS
            </div>
          </div>

        </div>
      </header>

      {/* Main Container */}
      <main className="flex-1 max-w-7xl mx-auto w-full px-4 sm:px-6 lg:px-8 py-8" id="app-main-content">
        {activeView === "admin" ? (
          <AdminConsole
            clients={clients}
            logs={logs}
            onAddClient={handleAddClient}
            onUpdateClient={handleUpdateClient}
            onDeleteClient={handleDeleteClient}
            onClearLogs={handleClearLogs}
            onSelectClientView={handleLaunchClientWorkspace}
          />
        ) : selectedClient ? (
          <ClientDashboard
            client={selectedClient}
            onBackToAdmin={() => setActiveView("admin")}
            onRecordLog={handleRecordLog}
            onUpdateClient={handleUpdateClient}
          />
        ) : (
          <div className="text-center py-24 glass rounded-2xl border border-white/10 shadow-xl max-w-lg mx-auto">
            <Compass className="h-12 w-12 text-gray-600 mx-auto mb-4 animate-spin-slow" />
            <h2 className="text-lg font-bold text-white">No Portal Selected</h2>
            <p className="text-gray-500 text-xs mt-1.5">
              Jump back to the central console to designate a specific client workspace
            </p>
            <button
              onClick={() => setActiveView("admin")}
              className="mt-6 px-4 py-2 text-xs bg-blue-600 hover:bg-blue-500 rounded-lg text-white"
            >
              Return Home
            </button>
          </div>
        )}
      </main>

      {/* Sticky Universal Footer with Real-Time Connectivity Diagnostic Indicators */}
      <footer className="bg-[#070707] border-t border-white/5 py-3 text-xs text-gray-500 font-mono mt-auto flex-none">
        <div className="max-w-7xl mx-auto px-4 flex flex-col md:flex-row items-center justify-between gap-4">
          <div className="flex items-center gap-2">
            <span className="h-1.5 w-1.5 rounded-full bg-blue-500 shadow-[0_0_6px_rgba(59,130,246,0.6)] animate-pulse"></span>
            <span>
              SightPortal UE5 Multi-Bridge v1.4.2 • Studio Engine Integration
            </span>
          </div>

          {/* Real-Time Connectivity Dashboard Monitors */}
          <div className="flex flex-wrap items-center gap-x-5 gap-y-2 text-[10px] md:text-[11px]">
            {/* 1. Dashboard API Server Indicator */}
            <div className="flex items-center gap-2 px-2.5 py-1 bg-white/5 border border-white/10 rounded-lg">
              <span className="text-gray-400">Server Status:</span>
              <span className="flex items-center gap-1.5">
                <span className={`h-2 w-2 rounded-full ${
                  backendAlive 
                    ? "bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.8)]" 
                    : "bg-rose-500 shadow-[0_0_8px_rgba(244,63,94,0.8)] animate-pulse"
                }`}></span>
                <span className={backendAlive ? "text-emerald-400 font-medium animate-pulse" : "text-rose-400 font-medium"}>
                  {backendAlive ? "Online" : "Disconnect"}
                </span>
              </span>
            </div>            {/* 2. Unreal Engine Direct Pipeline Link */}
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
