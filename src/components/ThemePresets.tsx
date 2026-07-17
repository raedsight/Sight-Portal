import React, { useState } from "react";
import { Palette, Trash2, Plus, Save } from "lucide-react";
import { ThemePreset, BgStyleType } from "../types";
import { saveThemePreset, deleteThemePreset } from "../firebase";

export default function ThemePresets({ presets }: { presets: ThemePreset[] }) {
  const [name, setName] = useState("");
  const [logoText, setLogoText] = useState("");
  const [primaryColor, setPrimaryColor] = useState("#0070FF");
  const [accentColor, setAccentColor] = useState("#f59e0b");
  const [bgStyle, setBgStyle] = useState<BgStyleType>("cyber");
  const [fontFamily, setFontFamily] = useState<"sans" | "mono" | "grotesk">("grotesk");

  const handleSave = async () => {
    if (!name.trim()) return;
    const newPreset: ThemePreset = {
      id: `preset-${Date.now()}`,
      name,
      branding: {
        logoText,
        primaryColor,
        accentColor,
        bgStyle,
        fontFamily,
      },
      updatedAt: new Date().toISOString(),
    };
    await saveThemePreset(newPreset);
    setName("");
    setLogoText("");
  };

  return (
    <div className="glass rounded-xl p-6 shadow-2xl space-y-6 animate-fadeIn">
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        <div className="space-y-4">
          <h2 className="text-lg font-bold text-white flex items-center gap-2">
            <Palette className="h-5 w-5 text-blue-400" />
            Create New Preset
          </h2>
          <input
            type="text"
            value={name}
            onChange={(e) => setName(e.target.value)}
            placeholder="Preset Name"
            className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white"
          />
          <input
            type="text"
            value={logoText}
            onChange={(e) => setLogoText(e.target.value)}
            placeholder="Logo Text"
            className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white"
          />
          <div className="grid grid-cols-2 gap-4">
            <input type="color" value={primaryColor} onChange={(e) => setPrimaryColor(e.target.value)} className="w-full h-10 rounded cursor-pointer" />
            <input type="color" value={accentColor} onChange={(e) => setAccentColor(e.target.value)} className="w-full h-10 rounded cursor-pointer" />
          </div>
          <div className="grid grid-cols-2 gap-4">
            <select
              value={bgStyle}
              onChange={(e) => setBgStyle(e.target.value as BgStyleType)}
              className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white cursor-pointer"
            >
              <option value="cyber">Cyberpunk</option>
              <option value="dark">Dark</option>
              <option value="light">Light</option>
              <option value="clean">Clean</option>
            </select>
            <select
              value={fontFamily}
              onChange={(e) => setFontFamily(e.target.value as "sans" | "mono" | "grotesk")}
              className="w-full px-3 py-2 text-sm bg-black/60 border border-white/10 rounded-lg text-white cursor-pointer"
            >
              <option value="grotesk">Grotesk</option>
              <option value="sans">Sans</option>
              <option value="mono">Mono</option>
            </select>
          </div>
          <button onClick={handleSave} className="flex items-center gap-2 px-4 py-2 bg-blue-600 rounded-lg text-white text-sm font-bold">
            <Save className="h-4 w-4" /> Save Preset
          </button>
        </div>
        <div className="space-y-4">
          <h2 className="text-lg font-bold text-white">Stored Presets ({presets.length})</h2>
          {presets.length === 0 ? (
            <p className="text-gray-500 text-sm italic">No presets saved yet.</p>
          ) : (
            presets.map((p) => (
              <div key={p.id} className="flex items-center justify-between p-3 bg-black/40 rounded-lg border border-white/10">
                <span className="text-white text-sm font-sans">{p.name}</span>
                <button onClick={() => deleteThemePreset(p.id)} className="text-red-400 hover:text-red-300">
                  <Trash2 className="h-4 w-4" />
                </button>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
}
