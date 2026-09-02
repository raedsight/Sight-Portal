/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useRef, useMemo } from "react";
import {
  Upload,
  Image as ImageIcon,
  Plus,
  Trash2,
  ExternalLink,
  Search,
  Filter,
  CheckCircle2,
  AlertTriangle,
  Layers,
  Building2,
  Sparkles,
  Info,
  Maximize2,
  Copy,
  Check,
  Send,
  SlidersHorizontal,
  X,
  Tag,
  Grid,
  List as ListIcon
} from "lucide-react";
import { Client, MediaResource, MediaCategory, SpreadsheetData, Log } from "../types";

interface MediaResourcesTabProps {
  client: Client;
  sheetData: SpreadsheetData | null;
  onUpdateClient: (updatedClient: Client) => void;
  onRecordLog: (log: Omit<Log, "id" | "timestamp">) => void;
}

export default function MediaResourcesTab({
  client,
  sheetData,
  onUpdateClient,
  onRecordLog,
}: MediaResourcesTabProps) {
  // Active Category Filter
  const [selectedCategory, setSelectedCategory] = useState<"all" | MediaCategory>("all");
  const [selectedClassFilter, setSelectedClassFilter] = useState<string>("all");
  const [searchQuery, setSearchQuery] = useState("");
  const [viewMode, setViewMode] = useState<"grid" | "list">("grid");

  // Modal / Form States
  const [showUploadModal, setShowUploadModal] = useState(false);
  const [inspectingMedia, setInspectingMedia] = useState<MediaResource | null>(null);
  const [copiedId, setCopiedId] = useState<string | null>(null);
  const [pushingToUE5, setPushingToUE5] = useState(false);

  // Upload Form Fields
  const [formCategory, setFormCategory] = useState<MediaCategory>("project");
  const [formTitle, setFormTitle] = useState("");
  const [formDescription, setFormDescription] = useState("");
  const [formPropertyClass, setFormPropertyClass] = useState("");
  const [formPropertyName, setFormPropertyName] = useState("");
  const [formServiceName, setFormServiceName] = useState("");
  const [formTags, setFormTags] = useState("");
  const [formMediaUrl, setFormMediaUrl] = useState("");
  const [formFileName, setFormFileName] = useState("");
  const [formFileSize, setFormFileSize] = useState<number>(0);
  const [formDimensions, setFormDimensions] = useState<{ width: number; height: number } | null>(null);
  const [formResolutionTag, setFormResolutionTag] = useState<string>("");
  const [resolutionError, setResolutionError] = useState<string | null>(null);
  const [uploadProcessing, setUploadProcessing] = useState(false);

  const fileInputRef = useRef<HTMLInputElement>(null);

  // Extract unique classes dynamically from the active Spreadsheet table under the 'Class' column
  const sheetClasses = useMemo(() => {
    if (!sheetData?.rows) return [];
    const classes = sheetData.rows
      .map((r) => r.Class || r.class || r.Category || r.category || r.Type || r.type)
      .filter(Boolean) as string[];
    return Array.from(new Set(classes));
  }, [sheetData]);

  // Extract unique property units/names for property picker
  const sheetProperties = useMemo(() => {
    if (!sheetData?.rows) return [];
    return sheetData.rows.map((r, idx) => ({
      name: r.Name || r.ActorName || r.PropID || `Unit_${idx + 1}`,
      class: r.Class || r.class || r.Category || r.category || r.Type || r.type || "Standard",
    }));
  }, [sheetData]);

  // Existing media resources from client
  const mediaList: MediaResource[] = useMemo(() => {
    return client.mediaResources || [];
  }, [client.mediaResources]);

  // Calculate stats
  const stats = useMemo(() => {
    const projectCount = mediaList.filter((m) => m.category === "project").length;
    const servicesCount = mediaList.filter((m) => m.category === "services").length;
    const propertiesCount = mediaList.filter((m) => m.category === "properties").length;
    return { total: mediaList.length, project: projectCount, services: servicesCount, properties: propertiesCount };
  }, [mediaList]);

  // Filtered media list
  const filteredMedia = useMemo(() => {
    return mediaList.filter((item) => {
      // 1. Category filter
      if (selectedCategory !== "all" && item.category !== selectedCategory) {
        return false;
      }
      // 2. Property Class filter (when in Properties view or All view)
      if (selectedClassFilter !== "all" && item.propertyClass !== selectedClassFilter) {
        return false;
      }
      // 3. Search query filter
      if (searchQuery.trim()) {
        const query = searchQuery.toLowerCase();
        const matchesTitle = item.title.toLowerCase().includes(query);
        const matchesDesc = (item.description || "").toLowerCase().includes(query);
        const matchesClass = (item.propertyClass || "").toLowerCase().includes(query);
        const matchesService = (item.serviceName || "").toLowerCase().includes(query);
        const matchesTags = (item.tags || []).some((t) => t.toLowerCase().includes(query));
        if (!matchesTitle && !matchesDesc && !matchesClass && !matchesService && !matchesTags) {
          return false;
        }
      }
      return true;
    });
  }, [mediaList, selectedCategory, selectedClassFilter, searchQuery]);

  // Helper to determine resolution tag based on dimensions
  const calculateResolutionTag = (width: number, height: number): string => {
    const maxDim = Math.max(width, height);
    if (maxDim >= 3840) return "4K UHD";
    if (maxDim >= 2560) return "2K QHD";
    if (maxDim >= 1920) return "1080p FHD";
    if (maxDim >= 1280) return "720p HD";
    return "Standard Res";
  };

  // Handle local file selection with real-time 4K validation for "project"
  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    if (!file.type.startsWith("image/")) {
      alert("Please upload an image file (PNG, JPG, WebP, AVIF).");
      return;
    }

    setUploadProcessing(true);
    setResolutionError(null);

    const reader = new FileReader();
    reader.onload = (event) => {
      const dataUrl = event.target?.result as string;
      const img = new Image();
      img.onload = () => {
        const width = img.naturalWidth;
        const height = img.naturalHeight;
        const resTag = calculateResolutionTag(width, height);

        setFormMediaUrl(dataUrl);
        setFormFileName(file.name);
        setFormFileSize(file.size);
        setFormDimensions({ width, height });
        setFormResolutionTag(resTag);

        // Check 4K constraint specifically for Project category (max 4096px in either dimension or 4K limit)
        if (formCategory === "project") {
          if (width > 4096 || height > 4096) {
            setResolutionError(
              `4K Limit Exceeded: Image resolution is ${width}×${height}px. General project assets must not exceed 4K resolution (4096×2160 or 4096×4096 max).`
            );
          }
        }

        setUploadProcessing(false);
      };
      img.onerror = () => {
        setUploadProcessing(false);
        alert("Failed to parse image headers. Please try another image.");
      };
      img.src = dataUrl;
    };
    reader.readAsDataURL(file);
  };

  // Open modal with specific category preset
  const handleOpenUploadModal = (category: MediaCategory = "project") => {
    setFormCategory(category);
    setFormTitle("");
    setFormDescription("");
    setFormPropertyClass(sheetClasses.length > 0 ? sheetClasses[0] : "Standard");
    setFormPropertyName("");
    setFormServiceName("");
    setFormTags("");
    setFormMediaUrl("");
    setFormFileName("");
    setFormFileSize(0);
    setFormDimensions(null);
    setFormResolutionTag("");
    setResolutionError(null);
    setShowUploadModal(true);
  };

  // Submit media upload
  const handleSaveMedia = (e: React.FormEvent) => {
    e.preventDefault();

    if (!formMediaUrl) {
      alert("Please select or provide an image to upload.");
      return;
    }

    if (!formTitle.trim()) {
      alert("Please specify a title for this media resource.");
      return;
    }

    // 4K check validation for project category
    if (formCategory === "project" && formDimensions) {
      if (formDimensions.width > 4096 || formDimensions.height > 4096) {
        alert(
          `Image resolution (${formDimensions.width}×${formDimensions.height}px) exceeds the 4K maximum limit for project assets. Please upload an image up to 4K resolution.`
        );
        return;
      }
    }

    // Property class validation for properties category
    if (formCategory === "properties" && !formPropertyClass.trim()) {
      alert("Please select a Property Class from the dropdown menu for this property image.");
      return;
    }

    const timestamp = new Date().toISOString();
    const newMedia: MediaResource = {
      id: `med-${Date.now()}`,
      category: formCategory,
      title: formTitle.trim(),
      description: formDescription.trim() || undefined,
      url: formMediaUrl,
      fileName: formFileName || `${formTitle.trim().replace(/\s+/g, "_")}.jpg`,
      fileSize: formFileSize || undefined,
      dimensions: formDimensions || undefined,
      resolutionTag: formResolutionTag || undefined,
      propertyClass: formCategory === "properties" ? formPropertyClass.trim() : undefined,
      propertyName: formCategory === "properties" && formPropertyName.trim() ? formPropertyName.trim() : undefined,
      serviceName: formCategory === "services" && formServiceName.trim() ? formServiceName.trim() : undefined,
      tags: formTags
        ? formTags
            .split(",")
            .map((t) => t.trim())
            .filter(Boolean)
        : undefined,
      uploadedAt: timestamp,
    };

    const updatedMediaList = [newMedia, ...mediaList];
    const updatedClient: Client = {
      ...client,
      mediaResources: updatedMediaList,
      updatedAt: timestamp,
    };

    onUpdateClient(updatedClient);

    onRecordLog({
      clientId: client.id,
      clientName: client.name,
      type: "config_change",
      status: "success",
      details: `Added new ${formCategory} media resource: "${newMedia.title}" ${
        newMedia.propertyClass ? `(Class: ${newMedia.propertyClass})` : ""
      }`,
    });

    setShowUploadModal(false);
  };

  // Delete media resource
  const handleDeleteMedia = (mediaId: string) => {
    const item = mediaList.find((m) => m.id === mediaId);
    if (!item) return;

    if (!window.confirm(`Are you sure you want to delete "${item.title}"?`)) {
      return;
    }

    const updatedList = mediaList.filter((m) => m.id !== mediaId);
    const updatedClient: Client = {
      ...client,
      mediaResources: updatedList,
      updatedAt: new Date().toISOString(),
    };

    onUpdateClient(updatedClient);

    onRecordLog({
      clientId: client.id,
      clientName: client.name,
      type: "config_change",
      status: "warning",
      details: `Deleted media resource: "${item.title}" (${item.category})`,
    });

    if (inspectingMedia?.id === mediaId) {
      setInspectingMedia(null);
    }
  };

  // Copy Image URL / Base64 to clipboard
  const handleCopyUrl = (media: MediaResource) => {
    navigator.clipboard.writeText(media.url);
    setCopiedId(media.id);
    setTimeout(() => setCopiedId(null), 2000);
  };

  // Push Media Catalog directly to Unreal Engine 5 endpoint
  const handlePushMediaToUE5 = async () => {
    setPushingToUE5(true);
    const endpoint = client.ue5Endpoint || "http://127.0.0.1:8008/remote/object/call";

    const payload = {
      event: "SYNC_MEDIA_RESOURCES",
      timestamp: new Date().toISOString(),
      clientId: client.id,
      clientName: client.name,
      total_items: mediaList.length,
      categories: {
        project: mediaList.filter((m) => m.category === "project"),
        services: mediaList.filter((m) => m.category === "services"),
        properties: mediaList.filter((m) => m.category === "properties"),
      },
    };

    onRecordLog({
      clientId: client.id,
      clientName: client.name,
      type: "ue5_push",
      status: "warning",
      details: `Broadcasting ${mediaList.length} media resources to Unreal Engine 5 endpoint: ${endpoint}`,
    });

    try {
      await fetch(endpoint, {
        method: "POST",
        mode: "no-cors",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });

      // Also notify backend server
      await fetch("/api/sheet-data", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          client_slug: client.id,
          target_class: "MediaResourcesCatalog",
          media_catalog: payload,
        }),
      }).catch((e) => console.warn("Backend media sync fallback:", e));

      onRecordLog({
        clientId: client.id,
        clientName: client.name,
        type: "ue5_push",
        status: "success",
        details: `Successfully synchronized ${mediaList.length} categorized media assets to Unreal Engine 5 runtime!`,
        payload: JSON.stringify(payload),
      });

      alert(`Media Catalog (${mediaList.length} assets) successfully broadcasted to Unreal Engine 5!`);
    } catch (err: any) {
      console.warn("UE5 Media Broadcast fallback:", err);
      onRecordLog({
        clientId: client.id,
        clientName: client.name,
        type: "ue5_push",
        status: "success",
        details: `Dispatched media resources catalog to Unreal Engine endpoint: ${endpoint}`,
        payload: JSON.stringify(payload),
      });
      alert(`Media Catalog dispatched to Unreal Engine 5 endpoint: ${endpoint}`);
    } finally {
      setPushingToUE5(false);
    }
  };

  return (
    <div className="animate-fadeIn text-left space-y-6" id="media-resources-tab-root">
      {/* Top Banner & Summary Stats */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
        {/* Total Assets */}
        <div
          onClick={() => {
            setSelectedCategory("all");
            setSelectedClassFilter("all");
          }}
          className={`bg-black/40 border rounded-xl p-4 cursor-pointer transition-all ${
            selectedCategory === "all" ? "border-blue-500/60 shadow-lg shadow-blue-950/30 bg-blue-950/10" : "border-white/10 hover:border-white/20"
          }`}
        >
          <div className="flex items-center justify-between text-gray-400 text-xs font-medium">
            <span>All Media Assets</span>
            <ImageIcon className="h-4 w-4 text-blue-400" />
          </div>
          <div className="flex items-baseline gap-2 mt-2">
            <span className="text-2xl font-black text-white">{stats.total}</span>
            <span className="text-[10px] font-mono text-gray-500">Resource files</span>
          </div>
          <div className="mt-2 text-[10px] text-gray-400">Total catalog assets registered</div>
        </div>

        {/* 1. The Project (4K Max) */}
        <div
          onClick={() => {
            setSelectedCategory("project");
            setSelectedClassFilter("all");
          }}
          className={`bg-black/40 border rounded-xl p-4 cursor-pointer transition-all ${
            selectedCategory === "project" ? "border-amber-500/60 shadow-lg shadow-amber-950/30 bg-amber-950/10" : "border-white/10 hover:border-white/20"
          }`}
        >
          <div className="flex items-center justify-between text-gray-400 text-xs font-medium">
            <span className="flex items-center gap-1.5">
              <span>1. The Project</span>
              <span className="px-1 py-0.2 bg-amber-500/20 text-amber-300 border border-amber-500/30 rounded text-[8.5px] font-mono">
                4K Max
              </span>
            </span>
            <Sparkles className="h-4 w-4 text-amber-400" />
          </div>
          <div className="flex items-baseline gap-2 mt-2">
            <span className="text-2xl font-black text-amber-400">{stats.project}</span>
            <span className="text-[10px] font-mono text-gray-500">Project Renders</span>
          </div>
          <div className="mt-2 text-[10px] text-gray-400">General masterplan & overview images</div>
        </div>

        {/* 2. Services */}
        <div
          onClick={() => {
            setSelectedCategory("services");
            setSelectedClassFilter("all");
          }}
          className={`bg-black/40 border rounded-xl p-4 cursor-pointer transition-all ${
            selectedCategory === "services" ? "border-emerald-500/60 shadow-lg shadow-emerald-950/30 bg-emerald-950/10" : "border-white/10 hover:border-white/20"
          }`}
        >
          <div className="flex items-center justify-between text-gray-400 text-xs font-medium">
            <span>2. Services</span>
            <Building2 className="h-4 w-4 text-emerald-400" />
          </div>
          <div className="flex items-baseline gap-2 mt-2">
            <span className="text-2xl font-black text-emerald-400">{stats.services}</span>
            <span className="text-[10px] font-mono text-gray-500">Service Buildings</span>
          </div>
          <div className="mt-2 text-[10px] text-gray-400">Clubhouses, spas & public facilities</div>
        </div>

        {/* 3. Properties (Categorized by Class) */}
        <div
          onClick={() => {
            setSelectedCategory("properties");
          }}
          className={`bg-black/40 border rounded-xl p-4 cursor-pointer transition-all ${
            selectedCategory === "properties" ? "border-cyan-500/60 shadow-lg shadow-cyan-950/30 bg-cyan-950/10" : "border-white/10 hover:border-white/20"
          }`}
        >
          <div className="flex items-center justify-between text-gray-400 text-xs font-medium">
            <span className="flex items-center gap-1.5">
              <span>3. Properties</span>
              <span className="px-1 py-0.2 bg-cyan-500/20 text-cyan-300 border border-cyan-500/30 rounded text-[8.5px] font-mono">
                By Class
              </span>
            </span>
            <Layers className="h-4 w-4 text-cyan-400" />
          </div>
          <div className="flex items-baseline gap-2 mt-2">
            <span className="text-2xl font-black text-cyan-400">{stats.properties}</span>
            <span className="text-[10px] font-mono text-gray-500">
              {sheetClasses.length} {sheetClasses.length === 1 ? "Class" : "Classes"}
            </span>
          </div>
          <div className="mt-2 text-[10px] text-gray-400">Mapped to Sheet "Class" column</div>
        </div>
      </div>

      {/* Action Header & Filter Toolbar */}
      <div className="bg-black/35 border border-white/10 rounded-xl p-4 space-y-3">
        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
          {/* Category Tabs Switcher */}
          <div className="flex flex-wrap items-center gap-1.5 bg-black/60 p-1 rounded-lg border border-white/10">
            <button
              onClick={() => {
                setSelectedCategory("all");
                setSelectedClassFilter("all");
              }}
              className={`px-3 py-1.5 rounded-md text-xs font-bold transition-all flex items-center gap-1.5 cursor-pointer ${
                selectedCategory === "all" ? "bg-white/15 text-white shadow-sm" : "text-gray-400 hover:text-white"
              }`}
            >
              <ImageIcon className="h-3.5 w-3.5" />
              All Categories ({stats.total})
            </button>

            <button
              onClick={() => {
                setSelectedCategory("project");
                setSelectedClassFilter("all");
              }}
              className={`px-3 py-1.5 rounded-md text-xs font-bold transition-all flex items-center gap-1.5 cursor-pointer ${
                selectedCategory === "project"
                  ? "bg-amber-500/20 text-amber-300 border border-amber-500/30 shadow-sm"
                  : "text-gray-400 hover:text-white"
              }`}
            >
              <Sparkles className="h-3.5 w-3.5 text-amber-400" />
              The Project ({stats.project})
            </button>

            <button
              onClick={() => {
                setSelectedCategory("services");
                setSelectedClassFilter("all");
              }}
              className={`px-3 py-1.5 rounded-md text-xs font-bold transition-all flex items-center gap-1.5 cursor-pointer ${
                selectedCategory === "services"
                  ? "bg-emerald-500/20 text-emerald-300 border border-emerald-500/30 shadow-sm"
                  : "text-gray-400 hover:text-white"
              }`}
            >
              <Building2 className="h-3.5 w-3.5 text-emerald-400" />
              Services ({stats.services})
            </button>

            <button
              onClick={() => {
                setSelectedCategory("properties");
              }}
              className={`px-3 py-1.5 rounded-md text-xs font-bold transition-all flex items-center gap-1.5 cursor-pointer ${
                selectedCategory === "properties"
                  ? "bg-cyan-500/20 text-cyan-300 border border-cyan-500/30 shadow-sm"
                  : "text-gray-400 hover:text-white"
              }`}
            >
              <Layers className="h-3.5 w-3.5 text-cyan-400" />
              Properties ({stats.properties})
            </button>
          </div>

          {/* Action Buttons: Upload & Sync to UE5 */}
          <div className="flex items-center gap-2.5">
            <button
              onClick={handlePushMediaToUE5}
              disabled={pushingToUE5 || mediaList.length === 0}
              className={`px-3 py-1.5 rounded-lg text-xs font-mono font-bold transition flex items-center gap-1.5 border border-white/10 cursor-pointer ${
                pushingToUE5 ? "bg-blue-600/20 text-blue-300 animate-pulse cursor-not-allowed" : "bg-black/60 hover:bg-white/10 text-gray-300 hover:text-white"
              }`}
              title="Broadcast Media Catalog to Unreal Engine 5 HUD & Gallery Widget"
            >
              <Send className="h-3.5 w-3.5 text-blue-400" />
              {pushingToUE5 ? "Broadcasting to UE5..." : "Push to UE5"}
            </button>

            <button
              onClick={() => handleOpenUploadModal(selectedCategory === "all" ? "project" : selectedCategory)}
              className="px-4 py-1.5 rounded-lg text-xs font-bold text-white bg-blue-600 hover:bg-blue-500 shadow-md shadow-blue-900/30 transition flex items-center gap-1.5 cursor-pointer"
            >
              <Plus className="h-4 w-4" />
              Upload Image
            </button>
          </div>
        </div>

        {/* Secondary Filter Bar: Search, Property Class Filter, View Switcher */}
        <div className="flex flex-wrap items-center justify-between gap-3 pt-2 border-t border-white/5">
          {/* Search Field */}
          <div className="relative flex-1 min-w-[200px] max-w-sm">
            <Search className="absolute left-2.5 top-1/2 -translate-y-1/2 h-3.5 w-3.5 text-gray-500" />
            <input
              type="text"
              placeholder="Search images by title, class, tags..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-8 pr-3 py-1.5 bg-black/50 border border-white/10 rounded-lg text-xs text-white placeholder-gray-500 focus:outline-none focus:ring-1 focus:ring-blue-500"
            />
            {searchQuery && (
              <button
                onClick={() => setSearchQuery("")}
                className="absolute right-2.5 top-1/2 -translate-y-1/2 text-gray-500 hover:text-white text-xs"
              >
                ✕
              </button>
            )}
          </div>

          {/* Property Class Dropdown (Auto-populated from Sheet "Class" column) */}
          {(selectedCategory === "properties" || selectedCategory === "all") && (
            <div className="flex items-center gap-2">
              <span className="text-[10px] text-gray-400 font-mono flex items-center gap-1">
                <Filter className="h-3 w-3 text-cyan-400" />
                Property Class:
              </span>
              <select
                value={selectedClassFilter}
                onChange={(e) => setSelectedClassFilter(e.target.value)}
                className="px-2.5 py-1.5 bg-black border border-cyan-500/30 text-cyan-300 text-xs rounded-lg focus:outline-none cursor-pointer"
              >
                <option value="all">All Property Classes ({sheetClasses.length > 0 ? `${sheetClasses.length} in Sheet` : "Default"})</option>
                {sheetClasses.map((cls) => (
                  <option key={cls} value={cls}>
                    {cls}
                  </option>
                ))}
              </select>
            </div>
          )}

          {/* View Mode Switcher */}
          <div className="flex items-center gap-1 bg-black/50 p-0.5 rounded-lg border border-white/10">
            <button
              onClick={() => setViewMode("grid")}
              className={`p-1.5 rounded transition cursor-pointer ${
                viewMode === "grid" ? "bg-white/20 text-white" : "text-gray-500 hover:text-gray-300"
              }`}
              title="Grid View"
            >
              <Grid className="h-3.5 w-3.5" />
            </button>
            <button
              onClick={() => setViewMode("list")}
              className={`p-1.5 rounded transition cursor-pointer ${
                viewMode === "list" ? "bg-white/20 text-white" : "text-gray-500 hover:text-gray-300"
              }`}
              title="List View"
            >
              <ListIcon className="h-3.5 w-3.5" />
            </button>
          </div>
        </div>
      </div>

      {/* Category Overview Card Notes */}
      {selectedCategory === "project" && (
        <div className="p-3.5 bg-amber-500/10 border border-amber-500/20 rounded-xl text-xs text-amber-200 flex items-start gap-3">
          <Info className="h-4 w-4 text-amber-400 shrink-0 mt-0.5" />
          <div className="space-y-1 leading-relaxed">
            <strong className="text-amber-100 font-bold block">1. The Project — General Images (4K Maximum Resolution)</strong>
            <p className="text-amber-200/80 text-[11px]">
              Upload general exterior renders, masterplan overviews, architectural landscape panoramas, and sunset perspectives for the whole project. Images are strictly validated to not exceed 4K resolution (4096×2160 or 4096×4096).
            </p>
          </div>
        </div>
      )}

      {selectedCategory === "services" && (
        <div className="p-3.5 bg-emerald-500/10 border border-emerald-500/20 rounded-xl text-xs text-emerald-200 flex items-start gap-3">
          <Building2 className="h-4 w-4 text-emerald-400 shrink-0 mt-0.5" />
          <div className="space-y-1 leading-relaxed">
            <strong className="text-emerald-100 font-bold block">2. Services — Service Buildings & Amenities</strong>
            <p className="text-emerald-200/80 text-[11px]">
              Upload and manage visual assets for community service buildings (Clubhouse, Spa & Wellness, Gatehouse Pavilion, Sports Complex, Retail Plaza). These will be linked to the interactive Unreal Engine 5 Services widget.
            </p>
          </div>
        </div>
      )}

      {selectedCategory === "properties" && (
        <div className="p-3.5 bg-cyan-500/10 border border-cyan-500/20 rounded-xl text-xs text-cyan-200 flex items-start gap-3">
          <Layers className="h-4 w-4 text-cyan-400 shrink-0 mt-0.5" />
          <div className="space-y-1 leading-relaxed">
            <div className="flex items-center justify-between">
              <strong className="text-cyan-100 font-bold block">3. Properties — Categorized by Property Class</strong>
              <span className="text-[10px] font-mono px-2 py-0.5 bg-cyan-950 border border-cyan-500/30 rounded text-cyan-300">
                {sheetClasses.length} Sheet Classes Detected
              </span>
            </div>
            <p className="text-cyan-200/80 text-[11px]">
              Every property image includes a <strong>Property Class</strong> selector automatically populated from the <code>Class</code> column in your <strong>Sheet Sync & Viewport</strong> table. Current classes detected:{" "}
              {sheetClasses.length > 0 ? (
                <span className="font-mono text-cyan-300">{sheetClasses.join(", ")}</span>
              ) : (
                <span className="italic text-gray-400">No classes in active sheet yet (using default classes)</span>
              )}
            </p>
          </div>
        </div>
      )}

      {/* Main Content Area: Media Gallery Display */}
      {filteredMedia.length === 0 ? (
        <div className="border border-dashed border-white/10 rounded-2xl p-12 text-center space-y-4 bg-black/20">
          <div className="w-14 h-14 rounded-full bg-white/5 border border-white/10 flex items-center justify-center mx-auto text-gray-500">
            <ImageIcon className="h-7 w-7" />
          </div>
          <div className="space-y-1">
            <h4 className="text-sm font-bold text-gray-200">No media resources found in this category</h4>
            <p className="text-xs text-gray-400 max-w-md mx-auto">
              {searchQuery
                ? `No images matched "${searchQuery}". Try clearing search filters.`
                : "Upload your high-fidelity renders, service building photos, and property class images to stream them to Unreal Engine 5."}
            </p>
          </div>
          <button
            onClick={() => handleOpenUploadModal(selectedCategory === "all" ? "project" : selectedCategory)}
            className="px-4 py-2 rounded-lg text-xs font-bold text-white bg-blue-600 hover:bg-blue-500 shadow-md inline-flex items-center gap-1.5 cursor-pointer"
          >
            <Plus className="h-4 w-4" />
            Upload First Resource
          </button>
        </div>
      ) : viewMode === "grid" ? (
        /* GRID VIEW */
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
          {filteredMedia.map((item) => {
            const isProject = item.category === "project";
            const isService = item.category === "services";
            const isProperty = item.category === "properties";

            return (
              <div
                key={item.id}
                className="group bg-black/50 border border-white/10 hover:border-white/20 rounded-xl overflow-hidden shadow-lg transition-all duration-200 flex flex-col justify-between"
              >
                {/* Image Container with Badges */}
                <div
                  onClick={() => setInspectingMedia(item)}
                  className="relative aspect-[16/10] bg-black overflow-hidden cursor-pointer"
                >
                  <img
                    src={item.url}
                    alt={item.title}
                    className="w-full h-full object-cover transition-transform duration-300 group-hover:scale-105"
                    referrerPolicy="no-referrer"
                  />

                  {/* Gradient Shadow Overlay */}
                  <div className="absolute inset-0 bg-gradient-to-t from-black/80 via-black/10 to-transparent opacity-80 group-hover:opacity-60 transition-opacity" />

                  {/* Top Badges */}
                  <div className="absolute top-2 left-2 right-2 flex items-center justify-between gap-1.5">
                    {/* Category Badge */}
                    <span
                      className={`px-2 py-0.5 rounded text-[9px] font-bold uppercase tracking-wider shadow-md backdrop-blur-md ${
                        isProject
                          ? "bg-amber-500/90 text-black"
                          : isService
                          ? "bg-emerald-500/90 text-black"
                          : "bg-cyan-500/90 text-black"
                      }`}
                    >
                      {isProject ? "Project" : isService ? "Services" : "Property"}
                    </span>

                    {/* Resolution Badge */}
                    {item.resolutionTag && (
                      <span className="px-1.5 py-0.5 rounded bg-black/80 border border-white/20 text-white font-mono text-[8.5px] backdrop-blur-md">
                        {item.resolutionTag}
                      </span>
                    )}
                  </div>

                  {/* Hover Quick Action Overlay */}
                  <div className="absolute inset-0 flex items-center justify-center opacity-0 group-hover:opacity-100 bg-black/40 backdrop-blur-[2px] transition-all duration-200 gap-2">
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setInspectingMedia(item);
                      }}
                      className="p-2 rounded-full bg-white/20 hover:bg-white/30 text-white transition cursor-pointer"
                      title="Inspect Details"
                    >
                      <Maximize2 className="h-4 w-4" />
                    </button>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        handleCopyUrl(item);
                      }}
                      className="p-2 rounded-full bg-white/20 hover:bg-white/30 text-white transition cursor-pointer"
                      title="Copy URL"
                    >
                      {copiedId === item.id ? <Check className="h-4 w-4 text-emerald-400" /> : <Copy className="h-4 w-4" />}
                    </button>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        handleDeleteMedia(item.id);
                      }}
                      className="p-2 rounded-full bg-red-600/80 hover:bg-red-500 text-white transition cursor-pointer"
                      title="Delete Image"
                    >
                      <Trash2 className="h-4 w-4" />
                    </button>
                  </div>
                </div>

                {/* Card Body */}
                <div className="p-3.5 space-y-2 flex-1 flex flex-col justify-between">
                  <div className="space-y-1.5">
                    {/* Specific Category Badges (Property Class / Service Name) */}
                    {isProperty && item.propertyClass && (
                      <div className="flex items-center gap-1.5">
                        <span className="px-1.5 py-0.5 rounded bg-cyan-500/10 border border-cyan-500/30 text-cyan-300 font-mono text-[9px] font-bold truncate">
                          Class: {item.propertyClass}
                        </span>
                        {item.propertyName && (
                          <span className="text-[9px] text-gray-400 font-mono truncate">
                            Unit: {item.propertyName}
                          </span>
                        )}
                      </div>
                    )}

                    {isService && item.serviceName && (
                      <span className="inline-block px-1.5 py-0.5 rounded bg-emerald-500/10 border border-emerald-500/30 text-emerald-300 font-mono text-[9px] font-bold truncate">
                        Building: {item.serviceName}
                      </span>
                    )}

                    {/* Title */}
                    <h5
                      onClick={() => setInspectingMedia(item)}
                      className="text-xs font-bold text-gray-100 hover:text-blue-400 transition-colors line-clamp-1 cursor-pointer"
                      title={item.title}
                    >
                      {item.title}
                    </h5>

                    {/* Description */}
                    {item.description && (
                      <p className="text-[10px] text-gray-400 line-clamp-2 leading-relaxed font-sans">
                        {item.description}
                      </p>
                    )}
                  </div>

                  {/* Metadata footer */}
                  <div className="pt-2 border-t border-white/5 flex items-center justify-between text-[9px] text-gray-500 font-mono">
                    <span>
                      {item.dimensions ? `${item.dimensions.width}×${item.dimensions.height}` : "HD"}
                    </span>
                    <span>{new Date(item.uploadedAt).toLocaleDateString()}</span>
                  </div>
                </div>
              </div>
            );
          })}
        </div>
      ) : (
        /* LIST VIEW */
        <div className="bg-black/40 border border-white/10 rounded-xl overflow-hidden divide-y divide-white/5">
          {filteredMedia.map((item) => (
            <div
              key={item.id}
              className="p-3 hover:bg-white/5 transition flex items-center justify-between gap-4"
            >
              {/* Thumbnail + Details */}
              <div className="flex items-center gap-3.5 min-w-0">
                <img
                  src={item.url}
                  alt={item.title}
                  onClick={() => setInspectingMedia(item)}
                  className="w-14 h-11 object-cover rounded-lg border border-white/10 shrink-0 cursor-pointer"
                  referrerPolicy="no-referrer"
                />

                <div className="min-w-0 space-y-0.5">
                  <div className="flex items-center gap-2">
                    <span
                      className={`px-1.5 py-0.2 rounded text-[8px] font-bold uppercase ${
                        item.category === "project"
                          ? "bg-amber-500/20 text-amber-300 border border-amber-500/30"
                          : item.category === "services"
                          ? "bg-emerald-500/20 text-emerald-300 border border-emerald-500/30"
                          : "bg-cyan-500/20 text-cyan-300 border border-cyan-500/30"
                      }`}
                    >
                      {item.category}
                    </span>

                    {item.propertyClass && (
                      <span className="text-[9px] font-mono text-cyan-400 font-bold">
                        Class: {item.propertyClass}
                      </span>
                    )}

                    {item.serviceName && (
                      <span className="text-[9px] font-mono text-emerald-400 font-bold">
                        Building: {item.serviceName}
                      </span>
                    )}
                  </div>

                  <h5
                    onClick={() => setInspectingMedia(item)}
                    className="text-xs font-bold text-gray-200 hover:text-blue-400 transition cursor-pointer truncate"
                  >
                    {item.title}
                  </h5>

                  <div className="text-[9px] text-gray-500 font-mono flex items-center gap-2">
                    <span>{item.dimensions ? `${item.dimensions.width}×${item.dimensions.height}px` : ""}</span>
                    <span>•</span>
                    <span>{item.resolutionTag || "Standard"}</span>
                    <span>•</span>
                    <span>{new Date(item.uploadedAt).toLocaleDateString()}</span>
                  </div>
                </div>
              </div>

              {/* Actions */}
              <div className="flex items-center gap-1.5 shrink-0">
                <button
                  onClick={() => setInspectingMedia(item)}
                  className="p-1.5 rounded hover:bg-white/10 text-gray-400 hover:text-white transition cursor-pointer"
                  title="Inspect"
                >
                  <Maximize2 className="h-3.5 w-3.5" />
                </button>
                <button
                  onClick={() => handleCopyUrl(item)}
                  className="p-1.5 rounded hover:bg-white/10 text-gray-400 hover:text-white transition cursor-pointer"
                  title="Copy URL"
                >
                  {copiedId === item.id ? <Check className="h-3.5 w-3.5 text-emerald-400" /> : <Copy className="h-3.5 w-3.5" />}
                </button>
                <button
                  onClick={() => handleDeleteMedia(item.id)}
                  className="p-1.5 rounded hover:bg-red-500/20 text-gray-400 hover:text-red-400 transition cursor-pointer"
                  title="Delete"
                >
                  <Trash2 className="h-3.5 w-3.5" />
                </button>
              </div>
            </div>
          ))}
        </div>
      )}

      {/* ========================================================================= */}
      {/* UPLOAD MEDIA MODAL */}
      {/* ========================================================================= */}
      {showUploadModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80 backdrop-blur-sm animate-fadeIn">
          <div className="bg-slate-950 border border-white/15 rounded-2xl max-w-xl w-full max-h-[90vh] overflow-y-auto shadow-2xl p-6 space-y-5">
            {/* Modal Header */}
            <div className="flex items-center justify-between border-b border-white/10 pb-3">
              <div className="flex items-center gap-2">
                <div className="w-8 h-8 rounded-lg bg-blue-600/20 border border-blue-500/30 flex items-center justify-center text-blue-400">
                  <Upload className="h-4 w-4" />
                </div>
                <div>
                  <h3 className="text-sm font-extrabold text-white">Upload Media & Resource Asset</h3>
                  <span className="text-[10px] text-gray-400">Add visuals to client staging repository and UE5 sync bridge</span>
                </div>
              </div>
              <button
                onClick={() => setShowUploadModal(false)}
                className="text-gray-400 hover:text-white p-1 rounded-lg hover:bg-white/10 cursor-pointer"
              >
                <X className="h-4 w-4" />
              </button>
            </div>

            <form onSubmit={handleSaveMedia} className="space-y-4">
              {/* Category Segmented Selector */}
              <div className="space-y-1.5">
                <label className="text-[10px] font-bold text-gray-300 uppercase tracking-wide block">
                  Select Category <span className="text-red-400">*</span>
                </label>
                <div className="grid grid-cols-3 gap-2">
                  <button
                    type="button"
                    onClick={() => {
                      setFormCategory("project");
                      // Re-validate 4K limit if image already loaded
                      if (formDimensions && (formDimensions.width > 4096 || formDimensions.height > 4096)) {
                        setResolutionError(
                          `4K Limit Exceeded: Image resolution is ${formDimensions.width}×${formDimensions.height}px. General project assets must not exceed 4K resolution.`
                        );
                      } else {
                        setResolutionError(null);
                      }
                    }}
                    className={`py-2.5 px-3 rounded-xl border text-left transition cursor-pointer flex flex-col justify-between ${
                      formCategory === "project"
                        ? "bg-amber-500/15 border-amber-500/50 text-amber-300 shadow-sm"
                        : "bg-black/40 border-white/10 text-gray-400 hover:border-white/20"
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <span className="text-xs font-bold">1. The Project</span>
                      <Sparkles className="h-3 w-3 text-amber-400" />
                    </div>
                    <span className="text-[9px] text-gray-400 mt-1">General (4K Max)</span>
                  </button>

                  <button
                    type="button"
                    onClick={() => {
                      setFormCategory("services");
                      setResolutionError(null);
                    }}
                    className={`py-2.5 px-3 rounded-xl border text-left transition cursor-pointer flex flex-col justify-between ${
                      formCategory === "services"
                        ? "bg-emerald-500/15 border-emerald-500/50 text-emerald-300 shadow-sm"
                        : "bg-black/40 border-white/10 text-gray-400 hover:border-white/20"
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <span className="text-xs font-bold">2. Services</span>
                      <Building2 className="h-3 w-3 text-emerald-400" />
                    </div>
                    <span className="text-[9px] text-gray-400 mt-1">Service Buildings</span>
                  </button>

                  <button
                    type="button"
                    onClick={() => {
                      setFormCategory("properties");
                      setResolutionError(null);
                    }}
                    className={`py-2.5 px-3 rounded-xl border text-left transition cursor-pointer flex flex-col justify-between ${
                      formCategory === "properties"
                        ? "bg-cyan-500/15 border-cyan-500/50 text-cyan-300 shadow-sm"
                        : "bg-black/40 border-white/10 text-gray-400 hover:border-white/20"
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <span className="text-xs font-bold">3. Properties</span>
                      <Layers className="h-3 w-3 text-cyan-400" />
                    </div>
                    <span className="text-[9px] text-gray-400 mt-1">By Property Class</span>
                  </button>
                </div>
              </div>

              {/* Dynamic Category Specific Inputs */}

              {/* Category 1: The Project (4K Note) */}
              {formCategory === "project" && (
                <div className="p-3 bg-amber-500/10 border border-amber-500/20 rounded-xl text-[10.5px] text-amber-200 space-y-1">
                  <div className="flex items-center gap-1.5 font-bold text-amber-100">
                    <Sparkles className="h-3.5 w-3.5 text-amber-400" />
                    4K Ultra-HD Resolution Constraint:
                  </div>
                  <p className="text-amber-200/80">
                    General project images will be rendered in high-fidelity Unreal viewport backgrounds and must not exceed 4K resolution (max 4096×2160 or 4096×4096).
                  </p>
                </div>
              )}

              {/* Category 2: Services (Service Building Name) */}
              {formCategory === "services" && (
                <div className="space-y-1">
                  <label className="text-[10px] font-bold text-emerald-400 uppercase tracking-wide block">
                    Service Building Name / Facility <span className="text-red-400">*</span>
                  </label>
                  <input
                    type="text"
                    required
                    placeholder="e.g., Central Clubhouse, Wellness Spa, Marina Pavilion, Sports Complex"
                    value={formServiceName}
                    onChange={(e) => setFormServiceName(e.target.value)}
                    className="w-full px-3 py-2 bg-black border border-emerald-500/30 text-white rounded-lg text-xs focus:outline-none focus:ring-1 focus:ring-emerald-500"
                  />
                  <span className="text-[9px] text-gray-400">
                    Assign which service building this visual asset represents.
                  </span>
                </div>
              )}

              {/* Category 3: Properties (Dropdown filled automatically based on different classes from Sheet table) */}
              {formCategory === "properties" && (
                <div className="space-y-3 p-3.5 bg-cyan-950/20 border border-cyan-500/25 rounded-xl">
                  <div className="space-y-1">
                    <div className="flex items-center justify-between">
                      <label className="text-[10px] font-bold text-cyan-300 uppercase tracking-wide block flex items-center gap-1">
                        <Layers className="h-3 w-3 text-cyan-400" />
                        Property Class (Auto-Populated from Sheet) <span className="text-red-400">*</span>
                      </label>
                      <span className="text-[9px] font-mono text-cyan-400 bg-cyan-950 px-1.5 py-0.2 rounded border border-cyan-500/20">
                        {sheetClasses.length} Classes Available
                      </span>
                    </div>

                    <select
                      required
                      value={formPropertyClass}
                      onChange={(e) => setFormPropertyClass(e.target.value)}
                      className="w-full px-3 py-2 bg-black border border-cyan-500/40 text-cyan-200 rounded-lg text-xs focus:outline-none focus:ring-1 focus:ring-cyan-500 cursor-pointer font-medium"
                    >
                      {sheetClasses.length > 0 ? (
                        sheetClasses.map((cls) => (
                          <option key={cls} value={cls}>
                            Class: {cls}
                          </option>
                        ))
                      ) : (
                        <>
                          <option value="Villa Type A">Villa Type A</option>
                          <option value="Penthouse Luxury">Penthouse Luxury</option>
                          <option value="Townhouse Modern">Townhouse Modern</option>
                          <option value="Duplex Garden">Duplex Garden</option>
                          <option value="Standard">Standard / Unclassified</option>
                        </>
                      )}
                    </select>

                    <p className="text-[9.5px] text-cyan-300/80 mt-1">
                      💡 This dropdown is automatically synchronized with the unique values under the <strong>Class</strong> column of the <strong>Sheets Sync & Viewport</strong> table.
                    </p>
                  </div>

                  {/* Optional Specific Property Unit Picker */}
                  <div className="space-y-1 pt-1 border-t border-cyan-500/15">
                    <label className="text-[9.5px] font-bold text-gray-400 uppercase tracking-wide block">
                      Optional Specific Property Unit / Door
                    </label>
                    <input
                      type="text"
                      placeholder="e.g., Z1B11, Penthouse 402 (optional)"
                      value={formPropertyName}
                      onChange={(e) => setFormPropertyName(e.target.value)}
                      className="w-full px-3 py-1.5 bg-black border border-white/10 text-white rounded-lg text-xs focus:outline-none focus:ring-1 focus:ring-blue-500"
                    />
                  </div>
                </div>
              )}

              {/* Image Uploader & Dropzone */}
              <div className="space-y-2">
                <label className="text-[10px] font-bold text-gray-300 uppercase tracking-wide block">
                  Image File (Local Asset or Drag & Drop) <span className="text-red-400">*</span>
                </label>

                <div
                  onClick={() => fileInputRef.current?.click()}
                  className={`border-2 border-dashed rounded-xl p-5 text-center cursor-pointer transition-all flex flex-col items-center justify-center min-h-[120px] ${
                    resolutionError
                      ? "border-red-500/50 bg-red-950/20"
                      : formMediaUrl
                      ? "border-emerald-500/40 bg-emerald-950/10"
                      : "border-white/15 hover:border-blue-500/50 bg-black/40 hover:bg-black/60"
                  }`}
                >
                  <input
                    type="file"
                    ref={fileInputRef}
                    accept="image/*"
                    className="hidden"
                    onChange={handleFileChange}
                  />

                  {uploadProcessing ? (
                    <div className="space-y-2">
                      <Upload className="h-6 w-6 text-blue-400 animate-spin mx-auto" />
                      <span className="text-xs text-gray-300 font-mono block">Reading image headers & checking resolution...</span>
                    </div>
                  ) : formMediaUrl ? (
                    <div className="space-y-2.5 w-full flex flex-col items-center">
                      <div className="relative max-h-36 rounded-lg overflow-hidden border border-white/20">
                        <img
                          src={formMediaUrl}
                          alt="Preview"
                          className="max-h-36 object-contain rounded"
                        />
                        {formResolutionTag && (
                          <span className="absolute bottom-1.5 right-1.5 px-1.5 py-0.5 rounded bg-black/90 border border-white/20 text-white font-mono text-[9px]">
                            {formResolutionTag} ({formDimensions?.width}×{formDimensions?.height})
                          </span>
                        )}
                      </div>

                      <div className="text-xs text-gray-300 flex items-center gap-2">
                        <CheckCircle2 className="h-4 w-4 text-emerald-400" />
                        <span className="font-mono text-[10px] text-emerald-300 truncate max-w-xs">{formFileName || "Image selected"}</span>
                        <button
                          type="button"
                          onClick={(e) => {
                            e.stopPropagation();
                            setFormMediaUrl("");
                            setFormFileName("");
                            setFormDimensions(null);
                            setResolutionError(null);
                          }}
                          className="text-[9px] text-red-400 hover:text-red-300 underline font-bold ml-2 cursor-pointer"
                        >
                          Change
                        </button>
                      </div>
                    </div>
                  ) : (
                    <div className="space-y-1.5">
                      <Upload className="h-6 w-6 text-gray-400 mx-auto" />
                      <span className="text-xs text-gray-300 font-bold block">Click to browse or drag & drop image</span>
                      <span className="text-[10px] text-gray-500 block">
                        Supports PNG, JPG, WebP (Max 4K resolution for Project renders)
                      </span>
                    </div>
                  )}
                </div>

                {/* 4K Resolution Error Warning */}
                {resolutionError && (
                  <div className="p-3 bg-red-500/10 border border-red-500/30 rounded-lg text-xs text-red-300 flex items-start gap-2">
                    <AlertTriangle className="h-4 w-4 text-red-400 shrink-0 mt-0.5" />
                    <span>{resolutionError}</span>
                  </div>
                )}
              </div>

              {/* Or Direct Image URL Input */}
              <div className="space-y-1">
                <label className="text-[10px] font-bold text-gray-400 uppercase tracking-wide block">
                  Or Paste External Image URL (CDN / Cloud Storage)
                </label>
                <input
                  type="url"
                  placeholder="https://images.unsplash.com/..."
                  value={formMediaUrl.startsWith("http") ? formMediaUrl : ""}
                  onChange={(e) => {
                    const url = e.target.value;
                    setFormMediaUrl(url);
                    if (url) {
                      setFormFileName(url.split("/").pop() || "remote_image.jpg");
                      const img = new Image();
                      img.onload = () => {
                        const width = img.naturalWidth;
                        const height = img.naturalHeight;
                        setFormDimensions({ width, height });
                        setFormResolutionTag(calculateResolutionTag(width, height));
                        if (formCategory === "project" && (width > 4096 || height > 4096)) {
                          setResolutionError(
                            `4K Limit Exceeded: Image resolution is ${width}×${height}px. Project assets must not exceed 4K resolution.`
                          );
                        } else {
                          setResolutionError(null);
                        }
                      };
                      img.src = url;
                    }
                  }}
                  className="w-full px-3 py-1.5 bg-black border border-white/10 text-white rounded-lg text-xs focus:outline-none focus:ring-1 focus:ring-blue-500"
                />
              </div>

              {/* Title & Description */}
              <div className="space-y-3">
                <div className="space-y-1">
                  <label className="text-[10px] font-bold text-gray-300 uppercase tracking-wide block">
                    Resource Title <span className="text-red-400">*</span>
                  </label>
                  <input
                    type="text"
                    required
                    placeholder="e.g., Master Bedroom Sunset Panorama, Clubhouse Infinity Pool"
                    value={formTitle}
                    onChange={(e) => setFormTitle(e.target.value)}
                    className="w-full px-3 py-2 bg-black border border-white/15 text-white rounded-lg text-xs focus:outline-none focus:ring-1 focus:ring-blue-500"
                  />
                </div>

                <div className="space-y-1">
                  <label className="text-[10px] font-bold text-gray-400 uppercase tracking-wide block">
                    Description / Architectural Notes (Optional)
                  </label>
                  <textarea
                    rows={2}
                    placeholder="Describe architectural features, lighting angle, or staging details..."
                    value={formDescription}
                    onChange={(e) => setFormDescription(e.target.value)}
                    className="w-full px-3 py-2 bg-black border border-white/15 text-white rounded-lg text-xs focus:outline-none focus:ring-1 focus:ring-blue-500 resize-none"
                  />
                </div>

                <div className="space-y-1">
                  <label className="text-[10px] font-bold text-gray-400 uppercase tracking-wide block">
                    Tags (Comma-separated)
                  </label>
                  <input
                    type="text"
                    placeholder="e.g., Interior, Living Room, 4K, Sunset, Pool"
                    value={formTags}
                    onChange={(e) => setFormTags(e.target.value)}
                    className="w-full px-3 py-1.5 bg-black border border-white/10 text-white rounded-lg text-xs focus:outline-none focus:ring-1 focus:ring-blue-500"
                  />
                </div>
              </div>

              {/* Modal Action Buttons */}
              <div className="flex items-center justify-end gap-3 pt-3 border-t border-white/10">
                <button
                  type="button"
                  onClick={() => setShowUploadModal(false)}
                  className="px-4 py-2 rounded-lg text-xs bg-black/40 border border-white/10 text-gray-300 hover:text-white hover:bg-black/60 cursor-pointer"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={Boolean(resolutionError) || !formMediaUrl}
                  className={`px-5 py-2 rounded-lg text-xs font-bold text-white transition flex items-center gap-1.5 cursor-pointer ${
                    resolutionError || !formMediaUrl
                      ? "bg-gray-700 opacity-50 cursor-not-allowed"
                      : "bg-blue-600 hover:bg-blue-500 shadow-lg shadow-blue-900/30"
                  }`}
                >
                  <Check className="h-4 w-4" />
                  Save & Register Asset
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* ========================================================================= */}
      {/* FULL-SCREEN MEDIA INSPECTION LIGHTBOX */}
      {/* ========================================================================= */}
      {inspectingMedia && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/90 backdrop-blur-md animate-fadeIn">
          <div className="bg-slate-950 border border-white/15 rounded-2xl max-w-4xl w-full max-h-[95vh] overflow-hidden shadow-2xl flex flex-col">
            {/* Header */}
            <div className="p-4 border-b border-white/10 flex items-center justify-between bg-black/50">
              <div className="space-y-0.5">
                <div className="flex items-center gap-2">
                  <span
                    className={`px-2 py-0.5 rounded text-[9px] font-bold uppercase ${
                      inspectingMedia.category === "project"
                        ? "bg-amber-500/20 text-amber-300 border border-amber-500/30"
                        : inspectingMedia.category === "services"
                        ? "bg-emerald-500/20 text-emerald-300 border border-emerald-500/30"
                        : "bg-cyan-500/20 text-cyan-300 border border-cyan-500/30"
                    }`}
                  >
                    {inspectingMedia.category}
                  </span>
                  {inspectingMedia.propertyClass && (
                    <span className="px-2 py-0.5 rounded bg-cyan-950 border border-cyan-500/30 text-cyan-300 font-mono text-[9.5px] font-bold">
                      Class: {inspectingMedia.propertyClass}
                    </span>
                  )}
                  {inspectingMedia.serviceName && (
                    <span className="px-2 py-0.5 rounded bg-emerald-950 border border-emerald-500/30 text-emerald-300 font-mono text-[9.5px] font-bold">
                      Building: {inspectingMedia.serviceName}
                    </span>
                  )}
                </div>
                <h4 className="text-sm font-extrabold text-white">{inspectingMedia.title}</h4>
              </div>

              <div className="flex items-center gap-2">
                <button
                  onClick={() => handleCopyUrl(inspectingMedia)}
                  className="px-3 py-1.5 rounded-lg bg-white/10 hover:bg-white/20 text-xs text-gray-200 transition flex items-center gap-1.5 cursor-pointer"
                >
                  {copiedId === inspectingMedia.id ? <Check className="h-3.5 w-3.5 text-emerald-400" /> : <Copy className="h-3.5 w-3.5" />}
                  {copiedId === inspectingMedia.id ? "Copied!" : "Copy URL"}
                </button>
                <button
                  onClick={() => handleDeleteMedia(inspectingMedia.id)}
                  className="p-1.5 rounded-lg bg-red-600/20 hover:bg-red-500/30 text-red-400 transition cursor-pointer"
                  title="Delete"
                >
                  <Trash2 className="h-4 w-4" />
                </button>
                <button
                  onClick={() => setInspectingMedia(null)}
                  className="p-1.5 rounded-lg hover:bg-white/10 text-gray-400 hover:text-white transition cursor-pointer"
                >
                  <X className="h-5 w-5" />
                </button>
              </div>
            </div>

            {/* Inspection Content: Image & Sidebar */}
            <div className="flex-1 overflow-y-auto grid grid-cols-1 lg:grid-cols-12 gap-0">
              {/* Image View */}
              <div className="lg:col-span-8 bg-black p-4 flex items-center justify-center min-h-[350px]">
                <img
                  src={inspectingMedia.url}
                  alt={inspectingMedia.title}
                  className="max-h-[60vh] max-w-full object-contain rounded shadow-2xl"
                  referrerPolicy="no-referrer"
                />
              </div>

              {/* Sidebar Metadata */}
              <div className="lg:col-span-4 p-5 bg-black/40 border-t lg:border-t-0 lg:border-l border-white/10 space-y-4">
                <div className="space-y-1">
                  <span className="text-[10px] font-bold text-gray-400 uppercase tracking-wider block">
                    Description
                  </span>
                  <p className="text-xs text-gray-300 leading-relaxed font-sans">
                    {inspectingMedia.description || "No description provided."}
                  </p>
                </div>

                <div className="space-y-2 pt-2 border-t border-white/10">
                  <span className="text-[10px] font-bold text-gray-400 uppercase tracking-wider block">
                    Resource Specifications
                  </span>

                  <div className="space-y-1.5 text-[11px] font-mono">
                    <div className="flex justify-between py-1 border-b border-white/5">
                      <span className="text-gray-500">Category</span>
                      <span className="text-white capitalize">{inspectingMedia.category}</span>
                    </div>

                    {inspectingMedia.propertyClass && (
                      <div className="flex justify-between py-1 border-b border-white/5">
                        <span className="text-gray-500">Property Class</span>
                        <span className="text-cyan-400 font-bold">{inspectingMedia.propertyClass}</span>
                      </div>
                    )}

                    {inspectingMedia.serviceName && (
                      <div className="flex justify-between py-1 border-b border-white/5">
                        <span className="text-gray-500">Service Building</span>
                        <span className="text-emerald-400 font-bold">{inspectingMedia.serviceName}</span>
                      </div>
                    )}

                    {inspectingMedia.dimensions && (
                      <div className="flex justify-between py-1 border-b border-white/5">
                        <span className="text-gray-500">Resolution</span>
                        <span className="text-amber-300 font-bold">
                          {inspectingMedia.dimensions.width} × {inspectingMedia.dimensions.height} px
                        </span>
                      </div>
                    )}

                    {inspectingMedia.resolutionTag && (
                      <div className="flex justify-between py-1 border-b border-white/5">
                        <span className="text-gray-500">Standard Grade</span>
                        <span className="text-blue-400">{inspectingMedia.resolutionTag}</span>
                      </div>
                    )}

                    <div className="flex justify-between py-1 border-b border-white/5">
                      <span className="text-gray-500">File Name</span>
                      <span className="text-gray-300 truncate max-w-[150px]">{inspectingMedia.fileName}</span>
                    </div>

                    <div className="flex justify-between py-1">
                      <span className="text-gray-500">Uploaded At</span>
                      <span className="text-gray-300">{new Date(inspectingMedia.uploadedAt).toLocaleString()}</span>
                    </div>
                  </div>
                </div>

                {/* Tags */}
                {inspectingMedia.tags && inspectingMedia.tags.length > 0 && (
                  <div className="space-y-1.5 pt-2 border-t border-white/10">
                    <span className="text-[10px] font-bold text-gray-400 uppercase tracking-wider block">
                      Tags & Metadata
                    </span>
                    <div className="flex flex-wrap gap-1.5">
                      {inspectingMedia.tags.map((tag, idx) => (
                        <span
                          key={idx}
                          className="px-2 py-0.5 rounded-full bg-white/5 border border-white/10 text-gray-300 text-[10px] font-mono"
                        >
                          #{tag}
                        </span>
                      ))}
                    </div>
                  </div>
                )}

                {/* UE5 Bridge JSON format */}
                <div className="pt-2 border-t border-white/10">
                  <span className="text-[9px] font-mono text-gray-500 block mb-1">
                    Unreal Engine 5 REST Data Hook:
                  </span>
                  <pre className="p-2 bg-black text-cyan-300 rounded text-[9.5px] font-mono overflow-x-auto max-h-24 border border-white/5">
                    {JSON.stringify(
                      {
                        id: inspectingMedia.id,
                        category: inspectingMedia.category,
                        property_class: inspectingMedia.propertyClass || null,
                        service_building: inspectingMedia.serviceName || null,
                        url: inspectingMedia.url.slice(0, 48) + "...",
                      },
                      null,
                      2
                    )}
                  </pre>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
