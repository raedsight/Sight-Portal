/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState } from "react";
import { 
  Users, 
  Plus, 
  Settings, 
  FileSpreadsheet, 
  Terminal, 
  RefreshCw, 
  Database, 
  Trash2, 
  Edit3, 
  Palette, 
  ExternalLink,
  Cpu,
  Search,
  CheckCircle2,
  AlertTriangle,
  XCircle,
  Eye,
  Minimize2,
  Sliders,
  Sparkles
} from "lucide-react";
import { Client, Log, BgStyleType } from "../types";
import { SPREADSHEET_TEMPLATES } from "../data";

interface AdminConsoleProps {
  clients: Client[];
  logs: Log[];
  onAddClient: (client: Client) => void;
  onUpdateClient: (client: Client) => void;
  onDeleteClient: (id: string) => void;
  onClearLogs: () => void;
  onSelectClientView: (client: Client) => void;
}

export default function AdminConsole({
  clients,
  logs,
  onAddClient,
  onUpdateClient,
  onDeleteClient,
  onClearLogs,
  onSelectClientView,
}: AdminConsoleProps) {
  // Navigation / Modal States
  const [showAddForm, setShowAddForm] = useState(false);
  const [editingClient, setEditingClient] = useState<Client | null>(null);
  const [searchQuery, setSearchQuery] = useState("");
  const [logSearchQuery, setLogSearchQuery] = useState("");
  const [logTypeFilter, setLogTypeFilter] = useState<string>("all");
  const [expandedLogId, setExpandedLogId] = useState<string | null>(null);

  // New Client Form Inputs
  const [name, setName] = useState("");
  const [company, setCompany] = useState("");
  const [sheetId, setSheetId] = useState("");
  const [sheetTab, setSheetTab] = useState("Sheet1");
  const [ue5Endpoint, setUe5Endpoint] = useState("http://127.0.0.1:8008/remote/object/call");
  
  // Theme options
  const [logoText, setLogoText] = useState("");
  const [primaryColor, setPrimaryColor] = useState("#0070FF");
  const [accentColor, setAccentColor] = useState("#f59e0b");
  const [bgStyle, setBgStyle] = useState<BgStyleType>("cyber");
  const [fontFamily, setFontFamily] = useState<"sans" | "mono" | "grotesk">("grotesk");
  const [selectedTemplate, setSelectedTemplate] = useState<string>("Virtual Camera & Rig Parameters");

  const colorPresets = [
    { name: "Unreal Blue", primary: "#0070FF", accent: "#38bdf8" },
    { name: "Luxury Gold", primary: "#f59e0b", accent: "#1e1b4b" },
    { name: "Cyber Radiant", primary: "#06b6d4", accent: "#10b981" },
    { name: "Sleek Carbon", primary: "#e2e8f0", accent: "#0f172a" },
    { name: "Aperture Green", primary: "#22c55e", accent: "#6366f1" },
    { name: "Sleek Crimson", primary: "#ef4444", accent: "#f59e0b" },
  ];

  const handleCreateClient = (e: React.FormEvent) => {
    e.preventDefault();
    if (!name.trim() || !company.trim()) return;

    const newId = name.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/(^-|-$)/g, "");
    
    const newClient: Client = {
      id: newId || `client-${Date.now()}`,
      name,
      company,
      sheetId: sheetId || "1BxiMVs0XRA5nFMdKv1aM9ldm5i-YSgcbL1g6xGoS18A", // fallback template
      sheetTab: sheetTab || "Sheet1",
      ue5Endpoint: ue5Endpoint || "http://127.0.0.1:8008/remote/object/call",
      branding: {
        logoText: logoText || name.toUpperCase() + " STAGE",
        primaryColor,
        accentColor,
        bgStyle,
        fontFamily,
      },
      updatedAt: new Date().toISOString(),
    };

    onAddClient(newClient);
    resetForm();
    setShowAddForm(false);
  };

  const handleStartEdit = (client: Client) => {
    setEditingClient(client);
    setName(client.name);
    setCompany(client.company);
    setSheetId(client.sheetId);
    setSheetTab(client.sheetTab);
    setUe5Endpoint(client.ue5Endpoint);
    setLogoText(client.branding.logoText);
    setPrimaryColor(client.branding.primaryColor);
    setAccentColor(client.branding.accentColor);
    setBgStyle(client.branding.bgStyle);
    setFontFamily(client.branding.fontFamily);
  };

  const handleUpdateClientSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!editingClient) return;

    const updated: Client = {
      ...editingClient,
      name,
      company,
      sheetId,
      sheetTab,
      ue5Endpoint,
      branding: {
        logoText: logoText || name.toUpperCase() + " STAGE",
        primaryColor,
        accentColor,
        bgStyle,
        fontFamily,
      },
      updatedAt: new Date().toISOString(),
    };

    onUpdateClient(updated);
    setEditingClient(null);
    resetForm();
  };

  const resetForm = () => {
    setName("");
    setCompany("");
    setSheetId("");
    setSheetTab("Sheet1");
    setUe5Endpoint("http://127.0.0.1:8008/remote/object/call");
    setLogoText("");
    setPrimaryColor("#d946ef");
    setAccentColor("#3b82f6");
    setBgStyle("cyber");
    setFontFamily("sans");
  };

  const selectColorPreset = (p: { primary: string; accent: string }) => {
    setPrimaryColor(p.primary);
    setAccentColor(p.accent);
  };

  // Filter clients based on query
  const filteredClients = clients.filter(c => 
    c.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    c.company.toLowerCase().includes(searchQuery.toLowerCase()) || 
    c.id.toLowerCase().includes(searchQuery.toLowerCase())
  );

  // Filter logs based on filters
  const filteredLogs = logs.filter(log => {
    const matchesSearch = log.clientId.toLowerCase().includes(logSearchQuery.toLowerCase()) ||
                          log.clientName.toLowerCase().includes(logSearchQuery.toLowerCase()) ||
                          log.details.toLowerCase().includes(logSearchQuery.toLowerCase());
    const matchesType = logTypeFilter === "all" || log.type === logTypeFilter;
    return matchesSearch && matchesType;
  });

  return (
    <div className="space-y-8" id="admin-main-view">
      {/* Admin Title Banner */}
      <div className="glass border border-white/10 rounded-xl p-6 sm:p-8 relative overflow-hidden shadow-2xl" id="admin-hero">
        <div className="absolute top-0 right-0 w-96 h-96 bg-blue-500/5 rounded-full blur-3xl -mr-20 -mt-20"></div>
        <div className="absolute bottom-0 left-0 w-64 h-64 bg-blue-500/5 rounded-full blur-2xl -ml-20 -mb-20"></div>
        
        <div className="relative flex flex-col md:flex-row md:items-center justify-between gap-6">
          <div>
            <div className="flex items-center gap-2 mb-2">
              <span className="px-2.5 py-1 text-xs font-mono font-medium rounded-full bg-blue-600/10 text-blue-400 border border-blue-500/20">
                ADMINISTRATION PANEL
              </span>
              <span className="flex h-2 w-2 rounded-full bg-blue-500 status-pulse"></span>
              <span className="text-xs text-gray-500 font-mono">System Secure</span>
            </div>
            <h1 className="text-3xl font-sans font-bold text-white tracking-tight">
              Client & Unreal Engine 5 Sheet Controller
            </h1>
            <p className="mt-2 text-gray-400 text-sm max-w-2xl">
              Configure remote clients, build custom-branded visual layouts, link real Google Sheets, 
              and review transmission logs routing values to UE5 scene components.
            </p>
          </div>
          <button
            id="register-client-btn"
            onClick={() => {
              resetForm();
              setEditingClient(null);
              setShowAddForm(!showAddForm);
            }}
            className="flex items-center gap-2 px-5 py-3 rounded-lg bg-blue-600 hover:bg-blue-500 text-white font-medium shadow-md transition-all duration-200 text-sm self-start md:self-auto cursor-pointer"
          >
            {showAddForm ? <Minimize2 className="h-4 w-4" /> : <Plus className="h-4 w-4" />}
            {showAddForm ? "Close Form" : "Register Client"}
          </button>
        </div>

        {/* Status Metrics Ribbon */}
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-4 mt-8 pt-6 border-t border-white/10 font-mono text-xs" id="admin-stats">
          <div className="bg-black/30 p-3 rounded-lg border border-white/5">
            <div className="text-gray-500 mb-1">REGISTERED CLIENTS</div>
            <div className="text-xl font-bold text-white flex items-center gap-1.5">
              <Users className="h-4 w-4 text-blue-400" />
              {clients.length}
            </div>
          </div>
          <div className="bg-black/30 p-3 rounded-lg border border-white/5">
            <div className="text-gray-500 mb-1">UE5 SERVER CHANNELS</div>
            <div className="text-xl font-bold text-white flex items-center gap-1.5">
              <Cpu className="h-4 w-4 text-blue-400 status-pulse" />
              {clients.filter(c => c.ue5Endpoint).length} Active
            </div>
          </div>
          <div className="bg-black/30 p-3 rounded-lg border border-white/5">
            <div className="text-gray-500 mb-1">TOTAL STREAM LOGS</div>
            <div className="text-xl font-bold text-white flex items-center gap-1.5">
              <Terminal className="h-4 w-4 text-blue-400" />
              {logs.length}
            </div>
          </div>
          <div className="bg-black/30 p-3 rounded-lg border border-white/5">
            <div className="text-gray-500 mb-1">HEALTH MONITOR</div>
            <div className="text-xl font-bold text-blue-500 flex items-center gap-1.5">
              <CheckCircle2 className="h-4 w-4" />
              System Secure
            </div>
          </div>
        </div>
      </div>

      {/* Register / Update Client Form */}
      {(showAddForm || editingClient) && (
        <form 
          id="client-setup-form"
          onSubmit={editingClient ? handleUpdateClientSubmit : handleCreateClient}
          className="glass rounded-xl p-6 shadow-xl relative animate-fadeIn"
        >
          <div className="flex items-center justify-between pb-4 mb-6 border-b border-white/10">
            <div className="flex items-center gap-2">
              <Palette className="h-5 w-5 text-blue-400" />
              <h2 className="text-lg font-bold text-white font-sans">
                {editingClient ? `Editing Client Profile: ${editingClient.name}` : "Register New Client Portal"}
              </h2>
            </div>
            <button 
              type="button" 
              onClick={() => {
                setShowAddForm(false);
                setEditingClient(null);
              }}
              className="text-gray-400 hover:text-white text-sm font-mono cursor-pointer"
            >
              Cancel
            </button>
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
            {/* Column 1: Core credentials */}
            <div className="lg:col-span-6 space-y-4">
              <div className="border-l-2 border-blue-500 pl-3 py-1 mb-2">
                <span className="text-xs font-bold text-gray-300 tracking-wider uppercase">1. Core Information & Sheet Bindings</span>
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Client Name / Label *</label>
                <input
                  type="text"
                  required
                  value={name}
                  onChange={(e) => {
                    setName(e.target.value);
                    if (!logoText) setLogoText(e.target.value.toUpperCase() + " SYSTEM");
                  }}
                  placeholder="e.g. Neon Horizon Interactive"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-blue-500 transition-colors"
                />
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Company / Organization *</label>
                <input
                  type="text"
                  required
                  value={company}
                  onChange={(e) => setCompany(e.target.value)}
                  placeholder="e.g. Horizon XR Solutions Inc."
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-blue-500 transition-colors"
                />
              </div>

              <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Google Sheet Template</label>
                  <select
                    value={selectedTemplate}
                    onChange={(e) => {
                      setSelectedTemplate(e.target.value);
                      // Auto populate some sheets
                    }}
                    className="w-full px-3 py-2 text-xs bg-black/60 border border-white/10 rounded-lg text-gray-300 focus:outline-none focus:border-blue-500"
                  >
                    {Object.keys(SPREADSHEET_TEMPLATES).map(key => (
                      <option key={key} value={key}>{key}</option>
                    ))}
                  </select>
                </div>
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Sheet Tab/Grid ID Name</label>
                  <input
                    type="text"
                    value={sheetTab}
                    onChange={(e) => setSheetTab(e.target.value)}
                    placeholder="e.g. Sheet1"
                    className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-blue-500 transition-colors font-mono"
                  />
                </div>
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-2 flex items-center justify-between">
                  <span>Unique Google Sheet URL or ID</span>
                  <span className="text-[10px] text-gray-500 font-mono">Any valid public/shared sheet link</span>
                </label>
                <input
                  type="text"
                  value={sheetId}
                  onChange={(e) => setSheetId(e.target.value)}
                  placeholder="e.g. 1BxiMVs0XRA5nFMdKv1aM9ldm5i-YSgcbL1g6xGoS18A or Full URL"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-gray-300 focus:outline-none focus:border-blue-500 transition-colors font-mono"
                />
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1 flex items-center gap-1.5">
                  <Sliders className="h-3 w-3 text-blue-400" />
                  Target Unreal Engine 5 Remote Server Endpoint
                </label>
                <input
                  type="text"
                  value={ue5Endpoint}
                  onChange={(e) => setUe5Endpoint(e.target.value)}
                  placeholder="e.g. http://localhost:8008/remote/object/call"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-blue-400 focus:outline-none focus:border-blue-500 transition-colors font-mono"
                />
                <p className="mt-1 text-[11px] text-gray-550">
                  Must link to UE5 Web Remote Control listener on client workspace (e.g. port 8008/8012).
                </p>
              </div>
            </div>

            {/* Column 2: Branding / Theme editor */}
            <div className="lg:col-span-6 space-y-4">
              <div className="border-l-2 border-emerald-500 pl-3 py-1 mb-2">
                <span className="text-xs font-bold text-gray-300 tracking-wider uppercase">2. Portal Branding & Theme Editor</span>
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Custom Brand Logo Text</label>
                <input
                  type="text"
                  value={logoText}
                  onChange={(e) => setLogoText(e.target.value)}
                  placeholder="e.g. HYPNOS ENGINE SYSTEM"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-blue-500"
                />
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-2">Choose Palette Presets</label>
                <div className="grid grid-cols-2 sm:grid-cols-3 gap-2">
                  {colorPresets.map((p) => (
                    <button
                      key={p.name}
                      type="button"
                      onClick={() => selectColorPreset(p)}
                      className="p-2 border border-white/5 rounded-lg bg-black/40 hover:bg-white/5 transition text-[11px] text-gray-300 flex items-center gap-2 cursor-pointer"
                    >
                      <span className="flex h-3 w-3 rounded-full shrink-0" style={{ backgroundColor: p.primary }}></span>
                      <span className="truncate">{p.name}</span>
                    </button>
                  ))}
                </div>
              </div>

              {/* Custom Color Picking */}
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1 flex items-center justify-between">
                    <span>Primary Color Hex</span>
                    <span 
                      className="inline-block w-4 h-4 rounded-full border border-white/10" 
                      style={{ backgroundColor: primaryColor }}
                    ></span>
                  </label>
                  <input
                    type="text"
                    value={primaryColor}
                    onChange={(e) => setPrimaryColor(e.target.value)}
                    className="w-full px-3 py-1.5 text-sm bg-black/60 border border-white/10 rounded-lg text-white font-mono"
                  />
                </div>
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1 flex items-center justify-between">
                    <span>Accent Color Hex</span>
                    <span 
                      className="inline-block w-4 h-4 rounded-full border border-white/10" 
                      style={{ backgroundColor: accentColor }}
                    ></span>
                  </label>
                  <input
                    type="text"
                    value={accentColor}
                    onChange={(e) => setAccentColor(e.target.value)}
                    className="w-full px-3 py-1.5 text-sm bg-black/60 border border-white/10 rounded-lg text-white font-mono"
                  />
                </div>
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Background Studio Vibe</label>
                  <select
                    value={bgStyle}
                    onChange={(e) => setBgStyle(e.target.value as BgStyleType)}
                    className="w-full px-3 py-2 text-xs bg-black/60 border border-white/10 rounded-lg text-gray-250 focus:outline-none focus:border-blue-500"
                  >
                    <option value="cyber">Cyber Obsidian Grid (Dark)</option>
                    <option value="dark">Charcoal Steel (Dark)</option>
                    <option value="light">Studio Soft Ash (Light)</option>
                    <option value="clean">Minimalist Slate (Ultra Clean)</option>
                  </select>
                </div>

                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Typography Sizing Theme</label>
                  <select
                    value={fontFamily}
                    onChange={(e) => setFontFamily(e.target.value as "sans" | "mono" | "grotesk")}
                    className="w-full px-3 py-2 text-xs bg-black/60 border border-white/10 rounded-lg text-gray-250 focus:outline-none focus:border-blue-500"
                  >
                    <option value="sans">Inter Sans (Neutral Pro)</option>
                    <option value="grotesk">Space Grotesk (Tech Head)</option>
                    <option value="mono">JetBrains Mono (Data Pure)</option>
                  </select>
                </div>
              </div>

              {/* Realtime Theme Preview Widget */}
              <div className="mt-4 p-4 rounded-lg bg-black/30 border border-white/5 status-pulse">
                <span className="text-[10px] text-gray-550 font-mono block mb-2">INTEGRATED LIVE PORTAL DESIGN PREVIEW:</span>
                <div 
                  className={`p-3 rounded-lg border flex flex-col gap-1 transition-all ${
                    bgStyle === "cyber" ? "bg-black border-blue-500/10 shadow-[0_0_15px_-3px_rgba(0,112,255,0.1)]" :
                    bgStyle === "clean" ? "bg-slate-50 border-slate-300 text-slate-900 shadow-sm" :
                    bgStyle === "light" ? "bg-zinc-100 border-zinc-200 text-zinc-900" : "bg-[#0A0A0A] border-white/10 text-white"
                  }`}
                >
                  <div className="flex items-center justify-between">
                    <span className="text-xs font-bold" style={{ color: primaryColor, fontFamily: fontFamily === "mono" ? "monospace" : "sans-serif" }}>
                      {logoText || "STAGE BRAND"}
                    </span>
                    <span className="text-[9px] px-1.5 py-0.5 rounded-full bg-blue-500/10 text-blue-400 font-mono border border-blue-550/20">
                      CONNECTED
                    </span>
                  </div>
                  <div className="text-[11px] opacity-80 mt-1 line-clamp-1">
                    Google sheet table linked. Dispatches securely to {ue5Endpoint || "No IP specified"}.
                  </div>
                  <button 
                    type="button" 
                    className="mt-2 text-[10px] px-2.5 py-1 rounded text-white self-start transition-opacity"
                    style={{ backgroundColor: primaryColor }}
                  >
                    Transmit Coordinates
                  </button>
                </div>
              </div>
            </div>
          </div>

          <div className="flex items-center justify-end gap-3 mt-8 pt-4 border-t border-white/10">
            <button
              type="button"
              onClick={() => {
                setShowAddForm(false);
                setEditingClient(null);
                resetForm();
              }}
              className="px-4 py-2 text-xs font-semibold text-gray-400 hover:text-white transition-colors cursor-pointer"
            >
              Cancel Setup
            </button>
            <button
              type="submit"
              className="px-6 py-2.5 text-xs font-bold bg-blue-600 hover:bg-blue-500 text-white rounded-lg transition-all shadow-md cursor-pointer"
            >
              {editingClient ? "Overwrite Client Profile" : "Activate Client Portal"}
            </button>
          </div>
        </form>
      )}

      {/* Main Administrative Columns */}
      <div className="grid grid-cols-1 xl:grid-cols-12 gap-8" id="admin-columns-container">
        {/* Left Column: Client Management */}
        <div className="xl:col-span-7 space-y-6">
          <div className="glass rounded-xl p-5 shadow-lg">
            <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 mb-6">
              <div className="flex items-center gap-2">
                <Users className="h-5 w-5 text-blue-400" />
                <h2 className="text-lg font-bold text-white font-sans">Registered Client Portals</h2>
              </div>
              <div className="relative w-full sm:w-64">
                <Search className="absolute left-2.5 top-2.5 h-4 w-4 text-gray-500" />
                <input
                  type="text"
                  placeholder="Filter clients..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="w-full pl-9 pr-3 py-1.5 bg-black/60 border border-white/10 rounded-lg text-xs text-slate-300 focus:outline-none focus:border-blue-500 placeholder-gray-600 font-sans"
                />
              </div>
            </div>

            {filteredClients.length === 0 ? (
              <div className="text-center py-12 border border-dashed border-white/10 rounded-lg bg-black/20">
                <Users className="h-10 w-10 text-gray-700 mx-auto mb-3" />
                <span className="text-gray-400 text-sm block">No Client Portals Match Filter</span>
                <span className="text-gray-600 text-xs font-mono mt-1 block">Register a client to activate a live UE5 connection bridge</span>
              </div>
            ) : (
              <div className="grid grid-cols-1 gap-4" id="clients-list">
                {filteredClients.map((client) => (
                  <div
                    key={client.id}
                    id={`client-card-${client.id}`}
                    className="p-4 rounded-xl bg-black/40 border border-white/5 hover:border-white/10 hover:bg-black/60 transition-all flex flex-col md:flex-row md:items-center justify-between gap-4 shadow-sm"
                  >
                    <div className="space-y-2">
                      <div className="flex items-center gap-2.5">
                        <span 
                          className="w-2.5 h-2.5 rounded-full ring-4 ring-black shadow-md"
                          style={{ backgroundColor: client.branding.primaryColor }}
                        ></span>
                        <h3 className="text-sm font-bold text-white font-sans">{client.name}</h3>
                        <span className="px-1.5 py-0.5 text-[10px] bg-white/5 border border-white/10 text-gray-450 font-mono rounded">
                          {client.id}
                        </span>
                      </div>
                      <span className="text-xs text-gray-400 block font-sans">
                        {client.company}
                      </span>

                      <div className="flex flex-wrap gap-y-1 gap-x-4 pt-1 font-mono text-[10px] text-gray-500">
                        <span className="flex items-center gap-1.5 min-w-[200px]">
                          <FileSpreadsheet className="h-3 w-3 text-cyan-500 shrink-0" />
                          <span className="truncate max-w-[170px]">Sheet: {client.sheetId}</span>
                        </span>
                        <span className="flex items-center gap-1.5">
                          <Cpu className="h-3 w-3 text-emerald-400 shrink-0" />
                          <span className="truncate max-w-[180px]">UE5: {client.ue5Endpoint}</span>
                        </span>
                      </div>
                    </div>

                    {/* Actions and Client portal preview launching */}
                    <div className="flex items-center gap-2 border-t md:border-t-0 pt-3 md:pt-0 border-white/5 shrink-0">
                      <button
                        title="Edit Configuration"
                        id={`edit-client-btn-${client.id}`}
                        onClick={() => handleStartEdit(client)}
                        className="p-2 border border-white/10 bg-white/5 hover:bg-white/10 text-blue-400 rounded-lg transition-colors cursor-pointer animate-none"
                      >
                        <Edit3 className="h-3.5 w-3.5" />
                      </button>
                      <button
                        title="Delete Client Portal"
                        id={`delete-client-btn-${client.id}`}
                        onClick={() => {
                          if (confirm(`Remove custom bridge and portal for client '${client.name}'?`)) {
                             onDeleteClient(client.id);
                          }
                        }}
                        className="p-2 border border-white/10 bg-white/5 hover:bg-red-950/30 text-red-400 hover:border-red-900/50 rounded-lg transition-colors cursor-pointer animate-none"
                      >
                        <Trash2 className="h-3.5 w-3.5" />
                      </button>

                      <button
                        id={`launch-portal-btn-${client.id}`}
                        onClick={() => onSelectClientView(client)}
                        className="flex items-center gap-1.5 px-3.5 py-1.5 text-xs font-bold text-white bg-blue-600 hover:bg-blue-500 rounded-lg transition shadow-md select-none cursor-pointer"
                        style={{ backgroundColor: client.branding.primaryColor }}
                      >
                        <ExternalLink className="h-3 w-3" />
                        Go to Portal
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
          
          {/* Quick Unreal Control Setup instructions Card for Operator */}
          <div className="glass rounded-xl p-5 shadow-lg text-gray-300">
            <h3 className="text-sm font-bold text-white font-sans mb-3 flex items-center gap-2">
              <Sparkles className="h-4 w-4 text-blue-400" />
              How to Connect Unreal Engine 5 Real-Time Remote Control
            </h3>
            <div className="space-y-3 text-xs opacity-90 font-sans">
              <p>
                To broadcast dynamic data updates from client spreadsheets straight to active Scene actors, enable the Web Remote Control in UE5:
              </p>
              <ol className="list-decimal pl-5 space-y-1.5 font-mono text-[11px] text-gray-500">
                <li>Go to <strong className="text-white">Plugins</strong> &gt; search and enable <strong className="text-blue-400">Web Remote Control</strong>.</li>
                <li>Go to <strong className="text-white">Project Settings</strong> &gt; search and enable <strong className="text-blue-400">Remote Control Web Server</strong>.</li>
                <li>Launch your editor. Port <strong className="text-blue-400 font-bold">8008</strong> will auto-bind to receive secure JSON payloads.</li>
                <li>Make sure client endpoints are directed appropriately (e.g. <code className="bg-black/50 px-1 py-0.5 border border-white/5 text-blue-400">http://127.0.0.1:8008/remote/object/call</code>).</li>
              </ol>
            </div>
          </div>
        </div>

        {/* Right Column: Dynamic System Log Terminal */}
        <div className="xl:col-span-5 space-y-6">
          <div className="glass rounded-xl p-5 shadow-lg flex flex-col h-[640px]">
            {/* Terminal Header */}
            <div className="flex items-center justify-between pb-4 border-b border-white/10 flex-none">
              <div className="flex items-center gap-2">
                <Terminal className="h-4 w-4 text-blue-400" />
                <h2 className="text-sm font-bold text-white font-mono tracking-wider">SYSTEM CENTRAL AUDIT LOGS</h2>
              </div>
              <button
                id="clear-logs-btn"
                onClick={onClearLogs}
                className="flex items-center gap-1.5 px-2.5 py-1 text-[10px] font-mono bg-white/5 hover:bg-white/10 border border-white/10 rounded text-gray-400 hover:text-red-400 cursor-pointer transition-colors"
              >
                Clear
              </button>
            </div>

            {/* Filter toolbar */}
            <div className="py-3 flex flex-col sm:flex-row gap-2 border-b border-white/10 flex-none">
              <select
                value={logTypeFilter}
                onChange={(e) => setLogTypeFilter(e.target.value)}
                className="px-2 py-1.5 text-[10px] bg-black/60 border border-white/10 text-gray-300 font-mono rounded focus:outline-none"
              >
                <option value="all">Filters: All Log Types</option>
                <option value="fetch_sheet">Type: Google Sheet Fetch</option>
                <option value="ue5_push">Type: Unreal Transmission</option>
                <option value="config_change">Type: Design Configurations</option>
                <option value="error">Type: System Errors</option>
              </select>

              <div className="relative flex-1">
                <Search className="absolute left-2 top-2 h-3.5 w-3.5 text-gray-500" />
                <input
                  type="text"
                  placeholder="Filter logs by client/text..."
                  value={logSearchQuery}
                  onChange={(e) => setLogSearchQuery(e.target.value)}
                  className="w-full pl-7 pr-2 py-1 bg-black/60 border border-white/10 rounded text-[10px] text-gray-350 font-mono focus:outline-none placeholder-gray-700"
                />
              </div>
            </div>

            {/* Logs stream body */}
            <div className="flex-1 overflow-y-auto pt-4 space-y-3 font-mono text-[11px] scrollbar-thin" id="logs-panel">
              {filteredLogs.length === 0 ? (
                <div className="text-center py-20 text-gray-600">
                  <Database className="h-8 w-8 text-gray-800 mx-auto mb-2" />
                  <span className="block italic">No logs recorded yet.</span>
                  <span className="block text-[10px] mt-1 text-gray-700">Client activities will list here in real-time</span>
                </div>
              ) : (
                filteredLogs.map((log) => {
                  const isExpanded = expandedLogId === log.id;
                  const formattedTime = new Date(log.timestamp).toLocaleTimeString();
                  
                  return (
                    <div 
                      key={log.id} 
                      className={`p-2.5 rounded border border-white/5 transition-colors ${
                        log.status === "error" ? "bg-red-950/15 border-red-900/40" :
                        log.status === "warning" ? "bg-amber-950/15 border-amber-900/40" :
                        "bg-black/40 hover:bg-black/60"
                      }`}
                    >
                      <div className="flex items-start justify-between gap-2">
                        <div className="space-y-1">
                          <div className="flex items-center flex-wrap gap-1.5">
                            <span className="text-[10px] text-gray-500">{formattedTime}</span>
                            <span 
                              className={`px-1 rounded text-[9px] uppercase font-bold shrink-0 ${
                                log.type === "fetch_sheet" ? "bg-cyan-500/10 text-cyan-400 border border-cyan-500/10" :
                                log.type === "ue5_push" ? "bg-emerald-500/10 text-emerald-400 border border-emerald-500/10" :
                                log.type === "config_change" ? "bg-purple-500/10 text-purple-400 border border-purple-500/10" :
                                "bg-red-500/10 text-red-400 border border-red-500/10"
                              }`}
                            >
                              {log.type}
                            </span>
                            <span className="text-gray-300 font-bold max-w-[120px] truncate">{log.clientName}</span>
                          </div>
                          <p className="text-gray-400 text-[10.5px] leading-relaxed">{log.details}</p>
                        </div>

                        {/* Expand Button */}
                        {log.payload && (
                          <button
                            onClick={() => setExpandedLogId(isExpanded ? null : log.id)}
                            className="p-1 text-gray-500 hover:text-gray-300 transition-colors shrink-0 cursor-pointer"
                          >
                            {isExpanded ? <Minimize2 className="h-3 w-3" /> : <Eye className="h-3 w-3" />}
                          </button>
                        )}
                      </div>

                      {/* Expanded Payload Viewer */}
                      {isExpanded && log.payload && (
                        <div className="mt-2.5 pt-2.5 border-t border-white/10 overflow-x-auto">
                          <span className="text-[9px] text-gray-550 block mb-1 uppercase font-bold tracking-wider">Payload Content:</span>
                          <pre className="text-[10px] bg-black p-2 rounded text-blue-300 leading-normal max-h-48 overflow-y-auto whitespace-pre-wrap select-text">
                            {JSON.stringify(JSON.parse(log.payload), null, 2)}
                          </pre>
                        </div>
                      )}
                    </div>
                  );
                })
              )}
            </div>
            
            <div className="pt-2 text-center text-[10px] text-gray-600 font-mono border-t border-white/10 flex-none">
              Auto-scrolling stream active • UTF-8 format
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

