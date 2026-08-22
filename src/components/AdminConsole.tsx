/**
 * @license
 * SPDX-License-Identifier: Apache-2.5
 */

import React, { useState, useEffect } from "react";
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
  Sparkles,
  ShieldAlert,
  UserCheck,
  Building2,
  Lock,
  Copy,
  Check,
  Save
} from "lucide-react";
import { Client, Log, BgStyleType, ThemePreset } from "../types";
import { SPREADSHEET_TEMPLATES } from "../data";
import { UserProfile, syncThemePresets, saveThemePreset, deleteThemePreset } from "../firebase";
import ThemePresets from "./ThemePresets";

interface AdminConsoleProps {
  clients: Client[];
  logs: Log[];
  onAddClient: (client: Client) => void;
  onUpdateClient: (client: Client) => void;
  onDeleteClient: (id: string) => void;
  onClearLogs: () => void;
  onSelectClientView: (client: Client) => void;
  currentUserProfile: UserProfile | null;
  userProfiles: UserProfile[];
  onUpdateUserProfile: (profile: UserProfile) => void;
  onDeleteUserProfile: (uid: string) => void;
}

export default function AdminConsole({
  clients,
  logs,
  onAddClient,
  onUpdateClient,
  onDeleteClient,
  onClearLogs,
  onSelectClientView,
  currentUserProfile,
  userProfiles,
  onUpdateUserProfile,
  onDeleteUserProfile,
}: AdminConsoleProps) {
  // Navigation tabs
  const [activeTab, setActiveTab] = useState<"portals" | "users" | "themes">("portals");

  // Form & Search States
  const [showAddForm, setShowAddForm] = useState(false);
  const [editingClient, setEditingClient] = useState<Client | null>(null);
  const [searchQuery, setSearchQuery] = useState("");
  const [userSearchQuery, setUserSearchQuery] = useState("");
  const [logSearchQuery, setLogSearchQuery] = useState("");
  const [logTypeFilter, setLogTypeFilter] = useState<string>("all");
  const [expandedLogId, setExpandedLogId] = useState<string | null>(null);
  const [copiedClientId, setCopiedClientId] = useState<string | null>(null);

  // New Client Form Inputs
  const [name, setName] = useState("");
  const [company, setCompany] = useState("");
  const [sheetId, setSheetId] = useState("");
  const [sheetTab, setSheetTab] = useState("Sheet1");
  const [ue5Endpoint, setUe5Endpoint] = useState("http://127.0.0.1:8008/remote/object/call");
  const [webSocketEndpoint, setWebSocketEndpoint] = useState("ws://127.0.0.1:8009");
  
  // Theme options
  const [logoText, setLogoText] = useState("");
  const [primaryColor, setPrimaryColor] = useState("#0070FF");
  const [accentColor, setAccentColor] = useState("#f59e0b");
  const [bgStyle, setBgStyle] = useState<BgStyleType>("cyber");
  const [fontFamily, setFontFamily] = useState<"sans" | "mono" | "grotesk">("grotesk");
  const [selectedTemplate, setSelectedTemplate] = useState<string>("Virtual Camera & Rig Parameters");

  const [userEdits, setUserEdits] = useState<Record<string, { role: "owner" | "admin" | "client"; clientId: string | null }>>({});
  const [themePresets, setThemePresets] = useState<ThemePreset[]>([]);

  // Listen for theme presets changes
  useEffect(() => {
    const unsub = syncThemePresets(setThemePresets);
    return () => unsub.then(unsubFn => unsubFn());
  }, []);

  const isOwner = currentUserProfile?.role === "owner";
  const canManageClients = currentUserProfile?.role === "owner" || currentUserProfile?.role === "admin";

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
    
    const initialSheet = SPREADSHEET_TEMPLATES[selectedTemplate] || SPREADSHEET_TEMPLATES["ArchViz Real-Estate Portfolio"];
    
    const newClient: Client = {
      id: newId || `client-${Date.now()}`,
      name,
      company,
      sheetId: sheetId || "1BxiMVs0XRA5nFMdKv1aM9ldm5i-YSgcbL1g6xGoS18A", // fallback template
      sheetTab: sheetTab || "Sheet1",
      ue5Endpoint: ue5Endpoint || "http://127.0.0.1:8008/remote/object/call",
      webSocketEndpoint: webSocketEndpoint || `ws://127.0.0.1:8009/ws/${newId}`,
      branding: {
        logoText: logoText || name.toUpperCase() + " STAGE",
        primaryColor,
        accentColor,
        bgStyle,
        fontFamily,
      },
      updatedAt: new Date().toISOString(),
      sheetData: initialSheet,
      bugs: []
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
    setWebSocketEndpoint(client.webSocketEndpoint || "");
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
      webSocketEndpoint,
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
    setWebSocketEndpoint("ws://127.0.0.1:8009");
    setLogoText("");
    setPrimaryColor("#0070FF");
    setAccentColor("#f59e0b");
    setBgStyle("cyber");
    setFontFamily("sans");
  };

  const selectColorPreset = (p: { primary: string; accent: string }) => {
    setPrimaryColor(p.primary);
    setAccentColor(p.accent);
  };

  const handleCopyLink = (clientId: string) => {
    const portalUrl = `${window.location.origin}${window.location.pathname}?portal=${clientId}`;
    navigator.clipboard.writeText(portalUrl);
    setCopiedClientId(clientId);
    setTimeout(() => setCopiedClientId(null), 2500);
  };

  // Filter lists based on search queries
  const filteredClients = clients.filter(c => 
    c.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    c.company.toLowerCase().includes(searchQuery.toLowerCase()) || 
    c.id.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const filteredLogs = logs.filter(log => {
    const matchesSearch = log.clientId.toLowerCase().includes(logSearchQuery.toLowerCase()) ||
                          log.clientName.toLowerCase().includes(logSearchQuery.toLowerCase()) ||
                          log.details.toLowerCase().includes(logSearchQuery.toLowerCase());
    const matchesType = logTypeFilter === "all" || log.type === logTypeFilter;
    return matchesSearch && matchesType;
  });

  const filteredUsers = userProfiles.filter(u => 
    u.email.toLowerCase().includes(userSearchQuery.toLowerCase()) ||
    u.role.toLowerCase().includes(userSearchQuery.toLowerCase()) ||
    u.uid.toLowerCase().includes(userSearchQuery.toLowerCase())
  );

  return (
    <div className="space-y-8" id="admin-main-view">
      
      {/* Admin Header & Stats */}
      <div className="glass border border-white/10 rounded-xl p-6 sm:p-8 relative overflow-hidden shadow-2xl" id="admin-hero">
        <div className="absolute top-0 right-0 w-96 h-96 bg-amber-500/5 rounded-full blur-3xl -mr-20 -mt-20 pointer-events-none"></div>
        <div className="absolute bottom-0 left-0 w-64 h-64 bg-amber-500/5 rounded-full blur-2xl -ml-20 -mb-20 pointer-events-none"></div>
        
        <div className="relative flex flex-col md:flex-row md:items-center justify-between gap-6">
          <div>
            <div className="flex items-center gap-2 mb-2">
              <span className="px-2.5 py-1 text-xs font-mono font-medium rounded-full bg-amber-500/10 text-amber-400 border border-amber-500/20">
                ADMINISTRATION PANEL
              </span>
              <span className="flex h-2 w-2 rounded-full bg-amber-500 status-pulse"></span>
              <span className="text-xs text-gray-500 font-mono">Role: {currentUserProfile?.role.toUpperCase()} GRP</span>
            </div>
            <h1 className="text-3xl font-sans font-bold text-white tracking-tight">
              SightPortal Stage Controller
            </h1>
            <p className="mt-2 text-gray-400 text-sm max-w-2xl">
              Configure secure staging portals, assign granular workspace permissions, connect Google Sheets parameters, and stream values to Unreal Engine 5 actors.
            </p>
          </div>

          <div className="flex gap-2">
            <button
              id="register-client-btn"
              onClick={() => {
                resetForm();
                setEditingClient(null);
                setShowAddForm(!showAddForm);
              }}
              className="flex items-center gap-2 px-5 py-3 rounded-lg bg-amber-500 hover:bg-amber-600 text-black font-bold shadow-md transition-all duration-200 text-sm cursor-pointer"
            >
              {showAddForm ? <Minimize2 className="h-4 w-4" /> : <Plus className="h-4 w-4" />}
              {showAddForm ? "Close Form" : "Register Client"}
            </button>
          </div>
        </div>

        {/* Tab switcher buttons */}
        <div className="flex gap-1.5 mt-8 border-b border-white/10 pb-0.5">
          <button
            onClick={() => setActiveTab("portals")}
            className={`px-4 py-2 text-xs font-mono uppercase tracking-wider font-bold transition border-b-2 -mb-[2px] cursor-pointer ${
              activeTab === "portals"
                ? "border-amber-500 text-amber-400"
                : "border-transparent text-gray-500 hover:text-gray-300"
            }`}
          >
            Client Portals & Logs
          </button>
          <button
            onClick={() => setActiveTab("users")}
            className={`px-4 py-2 text-xs font-mono uppercase tracking-wider font-bold transition border-b-2 -mb-[2px] cursor-pointer flex items-center gap-1.5 ${
              activeTab === "users"
                ? "border-amber-500 text-amber-400"
                : "border-transparent text-gray-500 hover:text-gray-300"
            }`}
          >
            User Group Directory
            <span className="px-1.5 py-0.5 text-[9px] bg-amber-500/20 text-amber-300 rounded-full font-sans font-bold">
              {userProfiles.length}
            </span>
          </button>
          <button
            onClick={() => setActiveTab("themes")}
            className={`px-4 py-2 text-xs font-mono uppercase tracking-wider font-bold transition border-b-2 -mb-[2px] cursor-pointer flex items-center gap-1.5 ${
              activeTab === "themes"
                ? "border-amber-500 text-amber-400"
                : "border-transparent text-gray-500 hover:text-gray-300"
            }`}
          >
            Branding Presets
            <span className="px-1.5 py-0.5 text-[9px] bg-amber-500/20 text-amber-300 rounded-full font-sans font-bold">
              {themePresets.length}
            </span>
          </button>
        </div>
      </div>

      {/* Client Setup Form */}
      {(showAddForm || editingClient) && (
        <form 
          id="client-setup-form"
          onSubmit={editingClient ? handleUpdateClientSubmit : handleCreateClient}
          className="glass rounded-xl p-6 shadow-xl relative animate-fadeIn"
        >
          <div className="flex items-center justify-between pb-4 mb-6 border-b border-white/10">
            <div className="flex items-center gap-2">
              <Palette className="h-5 w-5 text-amber-500" />
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
              <div className="border-l-2 border-amber-500 pl-3 py-1 mb-2">
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
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 transition-colors font-sans"
                />
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Staging Company / Client Legal Entity *</label>
                <input
                  type="text"
                  required
                  value={company}
                  onChange={(e) => setCompany(e.target.value)}
                  placeholder="e.g. Horizon Media Group Ltd"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 transition-colors font-sans"
                />
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Default Google Sheets ID or spreadsheet URL *</label>
                <input
                  type="text"
                  value={sheetId}
                  onChange={(e) => setSheetId(e.target.value)}
                  placeholder="e.g. 1BxiMVs0XRA5nFMd... or full shareable link"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 transition-colors font-sans"
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Sheet Tab Name</label>
                  <input
                    type="text"
                    value={sheetTab}
                    onChange={(e) => setSheetTab(e.target.value)}
                    placeholder="Sheet1"
                    className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 transition-colors font-sans"
                  />
                </div>
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Unreal Endpoint Server *</label>
                  <input
                    type="text"
                    required
                    value={ue5Endpoint}
                    onChange={(e) => setUe5Endpoint(e.target.value)}
                    placeholder="http://127.0.0.1:8008/remote/object/call"
                    className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 transition-colors font-mono"
                  />
                </div>
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Live WebSocket Connection Endpoint URL</label>
                <input
                  type="text"
                  value={webSocketEndpoint}
                  onChange={(e) => setWebSocketEndpoint(e.target.value)}
                  placeholder="e.g. ws://127.0.0.1:8009 or leave empty for dynamic default"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 transition-colors font-mono"
                />
              </div>
            </div>

            {/* Column 2: Branding */}
            <div className="lg:col-span-6 space-y-4">
              <div className="border-l-2 border-amber-500 pl-3 py-1 mb-2">
                <span className="text-xs font-bold text-gray-300 tracking-wider uppercase">2. Brand Themes & Staged Layouts</span>
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Apply Theme Preset</label>
                <select
                  onChange={(e) => {
                    const preset = themePresets.find(p => p.id === e.target.value);
                    if (preset) {
                      setLogoText(preset.branding.logoText);
                      setPrimaryColor(preset.branding.primaryColor);
                      setAccentColor(preset.branding.accentColor);
                      setBgStyle(preset.branding.bgStyle);
                      setFontFamily(preset.branding.fontFamily);
                    }
                  }}
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 cursor-pointer"
                >
                  <option value="">Select a preset...</option>
                  {themePresets.map(p => <option key={p.id} value={p.id}>{p.name}</option>)}
                </select>
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-400 mb-1">Header Logo Display Text</label>
                <input
                  type="text"
                  value={logoText}
                  onChange={(e) => setLogoText(e.target.value)}
                  placeholder="e.g. HORIZON PORTAL"
                  className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 transition-colors font-sans"
                />
              </div>

              {/* Color selectors */}
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Primary Theme Color</label>
                  <div className="flex gap-2">
                    <input
                      type="color"
                      value={primaryColor}
                      onChange={(e) => setPrimaryColor(e.target.value)}
                      className="h-9 w-9 bg-transparent border border-white/10 rounded cursor-pointer shrink-0"
                    />
                    <input
                      type="text"
                      value={primaryColor}
                      onChange={(e) => setPrimaryColor(e.target.value)}
                      className="w-full px-3 py-1.5 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 font-mono"
                    />
                  </div>
                </div>
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Accent Theme Color</label>
                  <div className="flex gap-2">
                    <input
                      type="color"
                      value={accentColor}
                      onChange={(e) => setAccentColor(e.target.value)}
                      className="h-9 w-9 bg-transparent border border-white/10 rounded cursor-pointer shrink-0"
                    />
                    <input
                      type="text"
                      value={accentColor}
                      onChange={(e) => setAccentColor(e.target.value)}
                      className="w-full px-3 py-1.5 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 font-mono"
                    />
                  </div>
                </div>
              </div>

              {/* Color Presets */}
              <div>
                <label className="block text-[11px] font-semibold text-gray-500 mb-1.5">Quick Color Presets</label>
                <div className="flex flex-wrap gap-1.5">
                  {colorPresets.map((p) => (
                    <button
                      key={p.name}
                      type="button"
                      onClick={() => selectColorPreset(p)}
                      className="px-2.5 py-1 text-[10px] bg-white/5 border border-white/5 hover:border-white/10 rounded text-gray-400 hover:text-white transition flex items-center gap-1.5 cursor-pointer"
                    >
                      <span className="w-2.5 h-2.5 rounded-full" style={{ backgroundColor: p.primary }}></span>
                      {p.name}
                    </button>
                  ))}
                </div>
              </div>

              {/* Layout styling preferences */}
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Visual BG Atmosphere</label>
                  <select
                    value={bgStyle}
                    onChange={(e) => setBgStyle(e.target.value as BgStyleType)}
                    className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 cursor-pointer"
                  >
                    <option value="cyber">Cyberpunk Space (Dark Theme)</option>
                    <option value="dark">Immersive Carbon (Sleek Theme)</option>
                    <option value="clean">Studio Slate (Light Theme)</option>
                  </select>
                </div>
                <div>
                  <label className="block text-xs font-semibold text-gray-400 mb-1">Typography Pairing</label>
                  <select
                    value={fontFamily}
                    onChange={(e) => setFontFamily(e.target.value as "sans" | "mono" | "grotesk")}
                    className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white focus:outline-none focus:border-amber-500 cursor-pointer"
                  >
                    <option value="grotesk">Space Grotesk (Tech-forward)</option>
                    <option value="sans">Inter (Modern Clean UI)</option>
                    <option value="mono">Fira Mono (Terminal Aesthetic)</option>
                  </select>
                </div>
              </div>
            </div>
          </div>

          <div className="flex items-center justify-between gap-3 mt-8 pt-4 border-t border-white/10">
            <div>
              {editingClient && canManageClients && (
                <button
                  type="button"
                  id={`delete-client-form-btn-${editingClient.id}`}
                  onClick={() => {
                    if (confirm(`Remove custom bridge and portal for client '${editingClient.name}'?`)) {
                      onDeleteClient(editingClient.id);
                      setShowAddForm(false);
                      setEditingClient(null);
                      resetForm();
                    }
                  }}
                  className="px-3 py-2 text-xs font-semibold text-rose-400 hover:text-rose-300 border border-rose-500/20 hover:border-rose-500/40 rounded-lg transition-colors cursor-pointer flex items-center gap-1.5"
                >
                  <Trash2 className="h-3.5 w-3.5" />
                  Delete Client Portal
                </button>
              )}
            </div>
            <div className="flex items-center gap-3">
              <button
                type="button"
                onClick={() => {
                  setShowAddForm(false);
                  setEditingClient(null);
                  resetForm();
                }}
                className="px-4 py-2 text-xs font-semibold text-gray-400 hover:text-white transition-colors cursor-pointer font-sans"
              >
                Cancel Setup
              </button>
              <button
                type="submit"
                className="px-6 py-2.5 text-xs font-bold bg-amber-500 hover:bg-amber-600 text-black rounded-lg transition-all shadow-md cursor-pointer font-mono uppercase tracking-wider"
              >
                {editingClient ? "Overwrite Client Profile" : "Activate Client Portal"}
              </button>
            </div>
          </div>
        </form>
      )}

      {/* RENDER VIEW ACCORDING TO SELECTED TAB */}
      {activeTab === "portals" && (
        <div className="grid grid-cols-1 lg:grid-cols-12 min-h-[calc(100vh-110px)] border-t border-[var(--ink-faint)]" id="admin-columns-container">
          
          {/* Pane 1: Transmission Hub Sidebar */}
          <aside className="lg:col-span-3 pane pane-alt flex flex-col justify-between">
            <div>
              <div className="label mb-6">Transmission Hub</div>
              <div className="h-display text-4xl mb-3 text-[var(--ink)]">Admin</div>
              <p className="text-xs text-[var(--ink-muted)] mb-8 leading-relaxed font-sans">
                Secure staging configuration and UE5 transmission telemetry for active viewport links.
              </p>

              <button
                id="register-client-btn"
                onClick={() => {
                  resetForm();
                  setEditingClient(null);
                  setShowAddForm(!showAddForm);
                }}
                className="btn-primary w-full py-3 mb-6"
              >
                {showAddForm ? "Close Form" : "Register Client"}
              </button>

              {/* Navigation Tabs */}
              <div className="space-y-1 mb-8">
                <button
                  onClick={() => setActiveTab("portals")}
                  className={`w-full text-left px-3 py-2 text-xs font-mono uppercase tracking-wider font-bold rounded transition ${
                    activeTab === "portals"
                      ? "bg-[var(--accent-soft)] text-[var(--accent)] border border-[var(--accent)]"
                      : "text-[var(--ink-muted)] hover:text-white"
                  }`}
                >
                  Client Portals ({clients.length})
                </button>
                <button
                  onClick={() => setActiveTab("users")}
                  className={`w-full text-left px-3 py-2 text-xs font-mono uppercase tracking-wider font-bold rounded transition flex items-center justify-between ${
                    activeTab === "users"
                      ? "bg-[var(--accent-soft)] text-[var(--accent)] border border-[var(--accent)]"
                      : "text-[var(--ink-muted)] hover:text-white"
                  }`}
                >
                  <span>User Directory</span>
                  <span className="text-[10px] px-1.5 py-0.5 bg-[var(--ink-faint)] rounded">
                    {userProfiles.length}
                  </span>
                </button>
                <button
                  onClick={() => setActiveTab("themes")}
                  className={`w-full text-left px-3 py-2 text-xs font-mono uppercase tracking-wider font-bold rounded transition flex items-center justify-between ${
                    activeTab === "themes"
                      ? "bg-[var(--accent-soft)] text-[var(--accent)] border border-[var(--accent)]"
                      : "text-[var(--ink-muted)] hover:text-white"
                  }`}
                >
                  <span>Branding Presets</span>
                  <span className="text-[10px] px-1.5 py-0.5 bg-[var(--ink-faint)] rounded">
                    {themePresets.length}
                  </span>
                </button>
              </div>

              {/* Client Search Filter */}
              <div className="mb-8">
                <div className="label mb-2">Search Filter</div>
                <div className="relative">
                  <Search className="absolute left-2.5 top-2.5 h-3.5 w-3.5 text-[var(--ink-muted)]" />
                  <input
                    type="text"
                    placeholder="Filter clients..."
                    value={searchQuery}
                    onChange={(e) => setSearchQuery(e.target.value)}
                    className="w-full pl-8 pr-3 py-1.5 bg-[var(--bg)] border border-[var(--ink-faint)] rounded text-xs text-[var(--ink)] focus:outline-none focus:border-[var(--accent)] font-mono"
                  />
                </div>
              </div>
            </div>

            {/* Hub Metrics */}
            <div className="pt-6 border-t border-[var(--ink-faint)] grid gap-4">
              <div>
                <div className="label">Directories</div>
                <div className="flex justify-between mt-2 font-mono text-xs">
                  <span className="opacity-70">User Groups</span>
                  <span className="text-[var(--accent)] font-bold">{userProfiles.length.toString().padStart(2, "0")}</span>
                </div>
              </div>
              <div className="pt-3 border-t border-[var(--ink-faint)]">
                <div className="label">Branding</div>
                <div className="flex justify-between mt-2 font-mono text-xs">
                  <span className="opacity-70">Presets</span>
                  <span className="text-[var(--accent)] font-bold">{themePresets.length.toString().padStart(2, "0")}</span>
                </div>
              </div>
            </div>
          </aside>

          {/* Pane 2: Client Portals Main Center */}
          <main className="lg:col-span-6 pane">
            <div className="flex justify-between items-baseline mb-8">
              <div className="h-display text-2xl text-[var(--ink)]">Client Portals</div>
              <div className="label">Count: [{filteredClients.length.toString().padStart(2, "0")}]</div>
            </div>

            {filteredClients.length === 0 ? (
              <div className="text-center py-16 border border-dashed border-[var(--ink-faint)] rounded-lg bg-black/20">
                <Building2 className="h-10 w-10 text-[var(--ink-muted)] mx-auto mb-3" />
                <span className="text-[var(--ink-muted)] text-sm block font-sans">No Client Portals Match Filter</span>
                <span className="text-[var(--ink-muted)] text-xs font-mono mt-1 block">Register a client to activate a live UE5 connection bridge</span>
              </div>
            ) : (
              <div className="grid gap-4" id="clients-list">
                {filteredClients.map((client, idx) => {
                  const isCopied = copiedClientId === client.id;
                  const isActiveCard = idx === 0; // Highlight top portal
                  return (
                    <div
                      key={client.id}
                      id={`client-card-${client.id}`}
                      className={`client-card ${isActiveCard ? "active" : ""}`}
                    >
                      <div className="flex justify-between items-start">
                        <div>
                          <h3 className="h-display text-xl text-[var(--accent)]">{client.name}</h3>
                          <p className="text-xs text-[var(--ink-muted)] mt-1 font-mono">
                            Sheet: {client.sheetId.length > 28 ? client.sheetId.substring(0, 28) + "..." : client.sheetId}
                          </p>
                          <p className="text-[11px] text-[var(--ink-muted)] mt-0.5">
                            {client.company} • UE5: {client.ue5Endpoint}
                          </p>
                        </div>
                        <span className="label text-[var(--accent)]">
                          {isActiveCard ? "Active" : "Staged"}
                        </span>
                      </div>

                      <div className="mt-6 flex flex-wrap gap-2 items-center">
                        <button
                          title="Edit Configuration"
                          id={`edit-client-btn-${client.id}`}
                          onClick={() => handleStartEdit(client)}
                          className="btn-ghost flex items-center gap-1.5"
                        >
                          <Edit3 className="h-3 w-3" />
                          Edit
                        </button>

                        <button
                          title="Copy Dedicated Client Link"
                          onClick={() => handleCopyLink(client.id)}
                          className="btn-ghost flex items-center gap-1.5"
                        >
                          {isCopied ? <Check className="h-3 w-3 text-emerald-400" /> : <Copy className="h-3 w-3" />}
                          {isCopied ? "Copied" : "Link"}
                        </button>

                        {/* Deletion accessible to Owner and Admin GRP */}
                        {canManageClients && (
                          <button
                            title="Delete Client Portal"
                            id={`delete-client-btn-${client.id}`}
                            onClick={() => {
                              if (confirm(`Remove custom bridge and portal for client '${client.name}'?`)) {
                                onDeleteClient(client.id);
                              }
                            }}
                            className="btn-ghost text-rose-400 border-rose-500/20 hover:border-rose-500"
                          >
                            <Trash2 className="h-3 w-3" />
                          </button>
                        )}

                        <button
                          id={`launch-portal-btn-${client.id}`}
                          onClick={() => onSelectClientView(client)}
                          className="btn-primary py-1.5 px-4 text-[0.65rem] ml-auto flex items-center gap-1.5"
                        >
                          <ExternalLink className="h-3 w-3" />
                          Go to Portal
                        </button>
                      </div>
                    </div>
                  );
                })}
              </div>
            )}

            {/* Instruction Box */}
            <div className="instruction-box">
              <div className="label mb-3 text-[var(--accent)]">System Integration</div>
              <p className="opacity-90 mb-3 text-xs leading-relaxed">
                To connect UE5: Import <code>/ue5-plugin</code> classes into your project Source. Update <code>Build.cs</code> with <code>"WebSockets"</code>. Use <code>ASightPortalSiteManager</code> actor for real-time telemetry streaming.
              </p>
              <div className="p-3 bg-black/30 rounded border border-[var(--accent-soft)] font-mono text-[0.65rem] text-[var(--accent)] break-all select-all">
                WSS://ais-pre-4wjcvfkjzt7ohntjrl7gk5-405891248157.europe-west3.run.app/ws/{filteredClients[0]?.id || "hyperion-vis"}
              </div>
            </div>
          </main>

          {/* Pane 3: System Audit Logs Terminal */}
          <aside className="lg:col-span-3 pane flex flex-col h-full">
            <div className="flex justify-between items-center mb-6">
              <div className="label">System Audit Logs</div>
              <button
                id="clear-logs-btn"
                onClick={onClearLogs}
                disabled={!isOwner}
                title={isOwner ? "Flush audit log streams" : "Owner GRP privilege only"}
                className="btn-ghost px-2 py-0.5 text-[0.55rem] rounded"
              >
                FLUSH
              </button>
            </div>

            {/* Log Filters */}
            <div className="mb-4 space-y-2">
              <select
                value={logTypeFilter}
                onChange={(e) => setLogTypeFilter(e.target.value)}
                className="portal-select w-full py-1 text-[0.65rem]"
              >
                <option value="all">All Log Types</option>
                <option value="fetch_sheet">Google Sheet Fetch</option>
                <option value="ue5_push">Unreal Transmission</option>
                <option value="config_change">Design Configurations</option>
                <option value="error">System Errors</option>
              </select>

              <div className="relative">
                <Search className="absolute left-2.5 top-2.5 h-3 w-3 text-[var(--ink-muted)]" />
                <input
                  type="text"
                  placeholder="Filter logs..."
                  value={logSearchQuery}
                  onChange={(e) => setLogSearchQuery(e.target.value)}
                  className="w-full pl-7 pr-2 py-1 bg-[var(--bg)] border border-[var(--ink-faint)] rounded text-[0.65rem] text-[var(--ink)] font-mono focus:outline-none"
                />
              </div>
            </div>

            {/* Logs stream list */}
            <div className="flex-1 overflow-y-auto font-mono text-[0.65rem] bg-[#0d0e10] p-3 rounded border border-[var(--secondary)]" id="logs-panel">
              {filteredLogs.length === 0 ? (
                <div className="text-center py-16 text-[var(--ink-muted)] italic text-xs">
                  No logs recorded yet.
                </div>
              ) : (
                filteredLogs.map((log) => {
                  const formattedTime = new Date(log.timestamp).toLocaleTimeString();
                  return (
                    <div key={log.id} className="log-line">
                      <span className="text-[var(--accent)] mr-2">[{formattedTime}]</span>
                      <span className="uppercase text-[0.6rem] px-1 py-0.5 rounded bg-[var(--border)] mr-2 text-[var(--accent)] font-bold">
                        {log.type}
                      </span>
                      <span className="text-[var(--ink)] opacity-90">{log.details}</span>
                    </div>
                  );
                })
              )}
            </div>
          </aside>
        </div>
      )}
      {activeTab === "themes" && <ThemePresets presets={themePresets} />}
      {activeTab === "users" && (
        <div className="glass rounded-xl p-6 shadow-2xl space-y-6 animate-fadeIn" id="user-directory-panel">
          <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4 pb-4 border-b border-white/10">
            <div>
              <h2 className="text-lg font-bold text-white flex items-center gap-2">
                <Lock className="h-5 w-5 text-amber-500" />
                User Authentication & Role Directory
              </h2>
              <p className="text-xs text-gray-400 mt-1 font-sans">
                Manage accounts, assign roles (Owner, Admin, Client groups), and allocate portal isolation paths.
              </p>
            </div>

            <div className="relative w-full sm:w-72">
              <Search className="absolute left-2.5 top-2.5 h-4 w-4 text-gray-500" />
              <input
                type="text"
                placeholder="Search user profile records..."
                value={userSearchQuery}
                onChange={(e) => setUserSearchQuery(e.target.value)}
                className="w-full pl-9 pr-3 py-1.5 bg-black/60 border border-white/10 rounded-lg text-xs text-slate-300 focus:outline-none focus:border-amber-500 placeholder-gray-600 font-sans"
              />
            </div>
          </div>

          <div className="overflow-x-auto">
            <table className="w-full text-left font-mono text-xs text-gray-300 border-collapse">
              <thead>
                <tr className="border-b border-white/10 text-gray-500 uppercase text-[10px] tracking-wider">
                  <th className="py-3 px-4 font-bold">User Account Info</th>
                  <th className="py-3 px-4 font-bold">Role Group</th>
                  <th className="py-3 px-4 font-bold">Designated Portal Link</th>
                  <th className="py-3 px-4 font-bold text-right">Commit Permissions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-white/5">
                {filteredUsers.length === 0 ? (
                  <tr>
                    <td colSpan={4} className="py-12 text-center text-gray-600">
                      <UserCheck className="h-8 w-8 text-gray-800 mx-auto mb-2" />
                      No registered user profiles found matching query.
                    </td>
                  </tr>
                ) : (
                  filteredUsers.map((profile) => {
                    // Pull current state or edit overrides
                    const currentEdits = userEdits[profile.uid] || { role: profile.role, clientId: profile.clientId || "" };
                    const isEdited = currentEdits.role !== profile.role || currentEdits.clientId !== (profile.clientId || "");
                    const isEditingSelf = currentUserProfile?.uid === profile.uid;

                    return (
                      <tr key={profile.uid} className="hover:bg-white/5 transition-colors">
                        {/* User identity info */}
                        <td className="py-4 px-4 font-sans">
                          <div className="flex items-center gap-3">
                            <div className="h-8 w-8 rounded-full bg-amber-500/10 border border-amber-500/20 flex items-center justify-center font-bold text-amber-400 text-xs shrink-0 uppercase">
                              {profile.email.substring(0, 2)}
                            </div>
                            <div>
                              <span className="text-white font-semibold block text-xs">{profile.email}</span>
                              <span className="text-[10px] text-gray-500 font-mono block mt-0.5 select-all">UID: {profile.uid}</span>
                            </div>
                          </div>
                        </td>

                        {/* Assign group */}
                        <td className="py-4 px-4">
                          <select
                            value={currentEdits.role}
                            disabled={isEditingSelf && profile.role === "owner"} // prevent lock-out of the owner profile
                            onChange={(e) => {
                              const nextRole = e.target.value as "owner" | "admin" | "client";
                              setUserEdits({
                                ...userEdits,
                                [profile.uid]: {
                                  ...currentEdits,
                                  role: nextRole,
                                  // Clear client ID assignment if promoted to developer roles
                                  clientId: nextRole !== "client" ? "" : currentEdits.clientId
                                }
                              });
                            }}
                            className="px-2 py-1 bg-[#050505] border border-white/10 rounded text-xs text-white focus:outline-none focus:border-amber-500 cursor-pointer"
                          >
                            <option value="owner">Owner GRP (All / Delete)</option>
                            <option value="admin">Admin GRP (Edit / Create)</option>
                            <option value="client">Client GRP (Isolated view)</option>
                          </select>
                        </td>

                        {/* Assign portal constraints */}
                        <td className="py-4 px-4">
                          {currentEdits.role === "client" ? (
                            <select
                              value={currentEdits.clientId || ""}
                              onChange={(e) => {
                                setUserEdits({
                                  ...userEdits,
                                  [profile.uid]: {
                                    ...currentEdits,
                                    clientId: e.target.value || null
                                  }
                                });
                              }}
                              className="px-2 py-1 bg-[#050505] border border-white/10 rounded text-xs text-white focus:outline-none focus:border-amber-500 max-w-[200px] cursor-pointer"
                            >
                              <option value="">Awaiting Allocation...</option>
                              {clients.map(c => (
                                <option key={c.id} value={c.id}>
                                  Portal: {c.name}
                                </option>
                              ))}
                            </select>
                          ) : (
                            <span className="text-amber-400 text-[10px] uppercase font-bold tracking-wider flex items-center gap-1">
                              <Sparkles className="h-3 w-3 animate-pulse" />
                              Unlimited Access
                            </span>
                          )}
                        </td>

                        {/* Save & Delete Directory user profiles */}
                        <td className="py-4 px-4 text-right">
                          <div className="flex items-center justify-end gap-2">
                            {isEdited && (
                              <button
                                onClick={() => {
                                  onUpdateUserProfile({
                                    uid: profile.uid,
                                    email: profile.email,
                                    role: currentEdits.role,
                                    clientId: currentEdits.clientId || null
                                  });
                                  // Clear edit buffers
                                  const editsCopy = { ...userEdits };
                                  delete editsCopy[profile.uid];
                                  setUserEdits(editsCopy);
                                }}
                                className="px-3 py-1 bg-emerald-600 hover:bg-emerald-500 text-white text-[10px] font-bold rounded cursor-pointer transition uppercase"
                              >
                                Save Changes
                              </button>
                            )}

                            {/* Deleting a user profile record is strictly for Owner GRP */}
                            <button
                              onClick={() => onDeleteUserProfile(profile.uid)}
                              disabled={!isOwner || isEditingSelf}
                              title={isOwner ? "Remove user account" : "Owner GRP privilege only"}
                              className={`p-1.5 rounded border transition-colors cursor-pointer ${
                                isOwner && !isEditingSelf
                                  ? "border-white/10 bg-white/5 hover:bg-red-950/20 text-red-400 hover:border-red-900/40"
                                  : "opacity-30 border-transparent text-gray-600 cursor-not-allowed"
                              }`}
                            >
                              <Trash2 className="h-3.5 w-3.5" />
                            </button>
                          </div>
                        </td>
                      </tr>
                    );
                  })
                )}
              </tbody>
            </table>
          </div>
          
          <div className="bg-white/5 border border-white/5 p-4 rounded-xl text-xs flex gap-3 text-gray-400 items-start leading-relaxed">
            <ShieldAlert className="h-5 w-5 text-amber-500 shrink-0 mt-0.5" />
            <div>
              <strong className="text-white block font-sans mb-1">Staging Rules Reference Manual:</strong>
              <ul className="list-disc pl-5 space-y-1 mt-1 font-sans">
                <li><strong>Owner GRP</strong>: Full unlimited write actions. Only role that can delete clients, wipe system activity logs, or revoke user accounts.</li>
                <li><strong>Admin GRP</strong>: Manage configs, register portal slugs, configure Google Sheets IDs, and assign user permissions, but cannot perform deletions.</li>
                <li><strong>Client GRP</strong>: Completely isolated. Clients can only log into their assigned client slug and are strictly banned from seeing or editing other client spaces.</li>
              </ul>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
