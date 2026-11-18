import React, { useCallback, useEffect, useState } from "react";
import { BarChart3, MessageCircle, Phone, RefreshCw, Trash2, Users } from "lucide-react";
import { auth } from "../firebase";

interface LeadRow {
  id: string;
  createdAt: string;
  name: string;
  phone: string;
  note?: string;
  unitNames: string[];
  status: "new" | "contacted" | "closed";
  shortlist_url?: string | null;
}

interface Summary {
  days: number;
  sessions: number;
  totals: Record<string, number>;
  top_units: { unit: string; counts: Record<string, number>; total: number }[];
  by_day: { day: string; sessions: number; events: Record<string, number> }[];
}

// Friendly names for the events the Unreal app sends (Google Analytics 4 naming)
const EVENT_LABELS: Record<string, string> = {
  session_start: "Sessions started",
  view_item: "Units viewed",
  select_item: "Units opened from search",
  search: "Searches",
  add_to_wishlist: "Added to favourites",
  remove_from_wishlist: "Removed from favourites",
  view_item_list: "Compare opened",
  view_floor_plan: "Floor plans viewed",
  enter_interior: "Interiors entered",
  get_directions: "Directions requested",
  share: "Shortlists shared by QR",
  generate_lead: "Leads captured",
};

// Shown as tiles in this order, also when the count is 0, so unused features stand out
const EVENT_ORDER = [
  "session_start", "view_item", "search", "select_item", "add_to_wishlist", "remove_from_wishlist",
  "view_item_list", "view_floor_plan", "generate_lead", "share", "get_directions", "enter_interior",
];
// Features the Unreal app does not report yet: a 0 here says nothing about how useful they are
const NOT_REPORTED_YET = new Set(["get_directions"]);

interface EventRow {
  ts: string;
  session: string;
  visitor: string | null;
  params: Record<string, string | number | boolean>;
}

const SEARCH_PARAM_LABELS: Record<string, string> = {
  query: "text", zone: "zone", block: "block", type: "type", availability: "availability", bedrooms: "bedrooms from",
  bathrooms: "bathrooms from", price_min: "price from", price_max: "price to", surface_min: "area from", surface_max: "area to", sort: "sorted by",
};

// What one event was about, in words: the unit, the units compared, the filters used...
function describeEvent(name: string, params: EventRow["params"]): string {
  const text = (key: string) => (params[key] === undefined || params[key] === "" ? "" : String(params[key]));
  if (name === "search") {
    const parts = Object.entries(SEARCH_PARAM_LABELS).filter(([key]) => text(key)).map(([key, label]) => `${label}: ${text(key)}`);
    const found = text("results") ? ` (${text("results")} units found)` : "";
    return (parts.length ? parts.join(", ") : "No filters, browsing all units") + found;
  }
  if (name === "view_item_list" || name === "generate_lead") return text("items").split(",").join(", ") || "(no units recorded)";
  if (name === "view_floor_plan") return [text("item_id"), text("type") && `type ${text("type")}`].filter(Boolean).join(", ") || "(no unit recorded)";
  if (text("item_id")) return text("item_id");
  if (text("detail")) return text("detail");
  const rest = Object.entries(params).map(([k, v]) => `${k}: ${v}`).join(", ");
  return rest || "(no details)";
}

const STATUSES: LeadRow["status"][] = ["new", "contacted", "closed"];

// Country code added to phone numbers typed in local form (a leading 0), so WhatsApp can open the chat. 964 = Iraq.
const DEFAULT_COUNTRY_CODE = "964";

// WhatsApp needs the number in international form, digits only. Returns "" when the number cannot be used.
export function whatsAppNumber(phone: string): string {
  const trimmed = (phone || "").trim();
  let digits = trimmed.replace(/\D/g, "");
  if (trimmed.startsWith("+")) {
    // already international
  } else if (digits.startsWith("00")) {
    digits = digits.slice(2);
  } else if (digits.startsWith("0")) {
    digits = DEFAULT_COUNTRY_CODE + digits.slice(1);
  }
  return digits.length >= 8 && digits.length <= 15 ? digits : "";
}

// Opens WhatsApp (app or web) on the advisor's device with the buyer's number and a ready message; the advisor presses send.
export function whatsAppLink(lead: { name: string; phone: string; shortlist_url?: string | null }, projectName?: string): string {
  const number = whatsAppNumber(lead.phone);
  if (!number || !lead.shortlist_url) return "";
  const firstName = (lead.name || "").trim().split(/\s+/)[0];
  const greeting = firstName ? `Hello ${firstName},` : "Hello,";
  const where = projectName ? ` at ${projectName}` : "";
  const text = `${greeting} here are the homes you saved${where}: ${lead.shortlist_url}`;
  return `https://wa.me/${number}?text=${encodeURIComponent(text)}`;
}

async function authorizedFetch(url: string, init?: RequestInit): Promise<any> {
  const token = await auth.currentUser?.getIdToken();
  if (!token) throw new Error("Sign in to the portal to see leads and analytics.");
  const res = await fetch(url, {
    ...init,
    headers: { ...(init?.headers || {}), Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
  });
  const body = await res.json().catch(() => null);
  if (!res.ok) throw new Error(body?.error || `The server answered ${res.status}.`);
  return body;
}

interface TrafficPresetInfo {
  id: string;
  label: string;
  sessionBatchesPerMinute: number;
  shortlistsPerMinute: number;
  shortlistsPerDay: number;
  analyticsBatchesPerMinute: number;
  flushSeconds: number;
}

interface TrafficState {
  preset: string;
  canEdit: boolean;
  presets: TrafficPresetInfo[];
}

export default function InsightsTab({ clientId, clientName }: { clientId: string; clientName?: string }) {
  const [days, setDays] = useState(30);
  const [leads, setLeads] = useState<LeadRow[]>([]);
  const [summary, setSummary] = useState<Summary | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [openEvent, setOpenEvent] = useState<string | null>(null);
  const [eventRows, setEventRows] = useState<EventRow[]>([]);
  const [eventLoading, setEventLoading] = useState(false);
  const [traffic, setTraffic] = useState<TrafficState | null>(null);
  const [trafficSaving, setTrafficSaving] = useState(false);

  useEffect(() => {
    let cancelled = false;
    setTraffic(null);
    authorizedFetch(`/api/traffic-preset?slug=${encodeURIComponent(clientId)}`)
      .then((data) => {
        if (!cancelled) setTraffic({ preset: data.preset, canEdit: data.can_edit === true, presets: Array.isArray(data.presets) ? data.presets : [] });
      })
      .catch(() => {});
    return () => {
      cancelled = true;
    };
  }, [clientId]);

  const changeTrafficPreset = async (preset: string) => {
    if (!traffic) return;
    const previous = traffic.preset;
    setTrafficSaving(true);
    setTraffic({ ...traffic, preset });
    try {
      await authorizedFetch("/api/traffic-preset", { method: "PUT", body: JSON.stringify({ slug: clientId, preset }) });
    } catch (e: any) {
      setTraffic({ ...traffic, preset: previous });
      setError(e?.message || "The traffic setting could not be saved.");
    } finally {
      setTrafficSaving(false);
    }
  };
  const activeTraffic = traffic?.presets.find((p) => p.id === traffic.preset);

  // Click a tile: load what is behind its number
  const showEvent = async (name: string) => {
    if (openEvent === name) {
      setOpenEvent(null);
      return;
    }
    setOpenEvent(name);
    setEventRows([]);
    setEventLoading(true);
    try {
      const data = await authorizedFetch(`/api/analytics/events?slug=${encodeURIComponent(clientId)}&name=${encodeURIComponent(name)}&days=${days}`);
      setEventRows(data.events || []);
    } catch (e: any) {
      setError(e?.message || "Could not load the details.");
    } finally {
      setEventLoading(false);
    }
  };

  const load = useCallback(async () => {
    setLoading(true);
    setError("");
    try {
      const slug = encodeURIComponent(clientId);
      const [leadData, summaryData] = await Promise.all([
        authorizedFetch(`/api/leads?slug=${slug}`),
        authorizedFetch(`/api/analytics/summary?slug=${slug}&days=${days}`),
      ]);
      setLeads(leadData.leads || []);
      setSummary(summaryData);
    } catch (e: any) {
      setError(e?.message || "Could not load leads and analytics.");
    } finally {
      setLoading(false);
    }
  }, [clientId, days]);

  useEffect(() => {
    void load();
  }, [load]);

  const changeStatus = async (lead: LeadRow, status: LeadRow["status"]) => {
    const previous = lead.status;
    setLeads((rows) => rows.map((r) => (r.id === lead.id ? { ...r, status } : r)));
    try {
      await authorizedFetch(`/api/leads/${encodeURIComponent(lead.id)}/status`, {
        method: "POST",
        body: JSON.stringify({ slug: clientId, status }),
      });
    } catch (e: any) {
      setLeads((rows) => rows.map((r) => (r.id === lead.id ? { ...r, status: previous } : r)));
      setError(e?.message || "Could not update the lead.");
    }
  };

  // Owner, admins and the project's moderators only (the same people who can change the traffic limits)
  const canDeleteLeads = traffic?.canEdit === true;
  const deleteLead = async (lead: LeadRow) => {
    if (!confirm(`Delete the lead '${lead.name}' (${lead.phone}) for good? This cannot be undone.`)) return;
    try {
      await authorizedFetch(`/api/leads/${encodeURIComponent(lead.id)}?slug=${encodeURIComponent(clientId)}`, { method: "DELETE" });
      setLeads((rows) => rows.filter((r) => r.id !== lead.id));
    } catch (e: any) {
      setError(e?.message || "Could not delete the lead.");
    }
  };

  // Every known event, in a fixed order, 0 included; then anything else the app sent
  const known = EVENT_ORDER.map((name) => [name, summary?.totals?.[name] ?? 0] as [string, number]);
  const extra = (Object.entries(summary?.totals ?? {}) as [string, number][]).filter(([name]) => !EVENT_ORDER.includes(name));
  const totals = [...known, ...extra];
  // The same thing done several times, counted once per kind
  const breakdown = (
    Object.entries(
      eventRows.reduce<Record<string, number>>((acc, row) => {
        const key = describeEvent(openEvent || "", row.params);
        acc[key] = (acc[key] || 0) + 1;
        return acc;
      }, {})
    ) as [string, number][]
  ).sort((a, b) => b[1] - a[1]);

  return (
    <div className="animate-fadeIn text-left space-y-6">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h3 className="text-sm font-bold text-white uppercase tracking-wider">Leads & Analytics</h3>
          <p className="text-xs text-gray-400 mt-1">Buyer contacts and usage sent by the Unreal app for this project.</p>
        </div>
        <div className="flex items-center gap-2">
          <select
            value={days}
            onChange={(e) => setDays(Number(e.target.value))}
            className="px-3 py-1.5 bg-black border border-white/10 text-white rounded-lg text-xs"
          >
            <option value={7}>Last 7 days</option>
            <option value={30}>Last 30 days</option>
            <option value={90}>Last 90 days</option>
          </select>
          <button
            onClick={() => void load()}
            disabled={loading}
            className="px-3 py-1.5 border border-white/10 text-white rounded-lg text-xs flex items-center gap-1.5 hover:bg-white/5 disabled:opacity-50 cursor-pointer"
          >
            <RefreshCw className={`h-3.5 w-3.5 ${loading ? "animate-spin" : ""}`} />
            Refresh
          </button>
        </div>
      </div>

      {traffic && activeTraffic && (
        <div className="flex flex-wrap items-center gap-3 p-3 rounded-lg border border-white/10 bg-white/5">
          <div className="text-xs font-bold text-white uppercase tracking-wider">Traffic limits</div>
          <select
            value={traffic.preset}
            disabled={!traffic.canEdit || trafficSaving}
            onChange={(e) => void changeTrafficPreset(e.target.value)}
            title={traffic.canEdit ? "How much traffic from the Unreal app the server accepts for this project" : "Set by the Sight team"}
            className="px-3 py-1.5 bg-black border border-white/10 text-white rounded-lg text-xs disabled:opacity-60"
          >
            {traffic.presets.map((p) => (
              <option key={p.id} value={p.id}>{p.label}</option>
            ))}
          </select>
          <p className="text-xs text-gray-400">
            Up to {activeTraffic.shortlistsPerMinute} shortlists a minute and {activeTraffic.shortlistsPerDay} a day,{" "}
            {activeTraffic.analyticsBatchesPerMinute} analytics batches a minute ({activeTraffic.sessionBatchesPerMinute} per visit).
            Unreal sends analytics every {activeTraffic.flushSeconds} seconds.
          </p>
        </div>
      )}

      {error && <div className="p-3 rounded-lg border border-red-500/30 bg-red-950/20 text-xs text-red-300">{error}</div>}

      <section className="space-y-3">
        <h4 className="text-xs font-bold text-emerald-300 uppercase tracking-wide flex items-center gap-2">
          <Users className="h-4 w-4" /> Leads ({leads.length})
        </h4>
        {leads.length === 0 ? (
          <p className="text-xs text-gray-500">{loading ? "Loading..." : "No leads yet. They appear here when a buyer leaves their details in the app."}</p>
        ) : (
          <div className="overflow-x-auto border border-white/10 rounded-xl">
            <table className="w-full text-xs">
              <thead className="bg-white/5 text-gray-400 uppercase text-[10px]">
                <tr>
                  <th className="text-left px-3 py-2">Received</th>
                  <th className="text-left px-3 py-2">Name</th>
                  <th className="text-left px-3 py-2">Phone</th>
                  <th className="text-left px-3 py-2">Units</th>
                  <th className="text-left px-3 py-2">Note</th>
                  <th className="text-left px-3 py-2">Status</th>
                </tr>
              </thead>
              <tbody>
                {leads.map((lead) => (
                  <tr key={lead.id} className="border-t border-white/5 text-gray-200 align-top">
                    <td className="px-3 py-2 whitespace-nowrap">{new Date(lead.createdAt).toLocaleString()}</td>
                    <td className="px-3 py-2">{lead.name}</td>
                    <td className="px-3 py-2 whitespace-nowrap">
                      <a href={`tel:${lead.phone}`} className="text-emerald-300 inline-flex items-center gap-1">
                        <Phone className="h-3 w-3" />
                        {lead.phone}
                      </a>
                      {whatsAppLink(lead, clientName) && (
                        <a
                          href={whatsAppLink(lead, clientName)}
                          target="_blank"
                          rel="noreferrer"
                          title="Opens WhatsApp with the shortlist link ready to send"
                          className="mt-1 flex w-fit items-center gap-1 rounded border border-emerald-500/40 bg-emerald-950/30 px-2 py-0.5 text-[11px] text-emerald-200 hover:bg-emerald-900/40"
                        >
                          <MessageCircle className="h-3 w-3" />
                          Send on WhatsApp
                        </a>
                      )}
                    </td>
                    <td className="px-3 py-2">
                      {lead.shortlist_url ? (
                        <a href={lead.shortlist_url} target="_blank" rel="noreferrer" className="text-amber-300 underline">
                          {lead.unitNames.join(", ") || "Open shortlist"}
                        </a>
                      ) : (
                        lead.unitNames.join(", ")
                      )}
                    </td>
                    <td className="px-3 py-2 text-gray-400">{lead.note}</td>
                    <td className="px-3 py-2">
                      <select
                        value={lead.status}
                        onChange={(e) => void changeStatus(lead, e.target.value as LeadRow["status"])}
                        className="px-2 py-1 bg-black border border-white/10 text-white rounded text-xs"
                      >
                        {STATUSES.map((s) => (
                          <option key={s} value={s}>
                            {s}
                          </option>
                        ))}
                      </select>
                      {canDeleteLeads && (
                        <button
                          onClick={() => void deleteLead(lead)}
                          title="Delete this lead for good"
                          className="ml-2 p-1.5 rounded border border-white/10 bg-white/5 hover:bg-red-950/20 text-red-400 hover:border-red-900/40 cursor-pointer align-middle"
                        >
                          <Trash2 className="h-3.5 w-3.5" />
                        </button>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </section>

      <section className="space-y-3">
        <h4 className="text-xs font-bold text-cyan-300 uppercase tracking-wide flex items-center gap-2">
          <BarChart3 className="h-4 w-4" /> Usage, last {summary?.days ?? days} days
        </h4>
        {!summary ? (
          <p className="text-xs text-gray-500">{loading ? "Loading..." : "No usage has been recorded in this period."}</p>
        ) : (
          <>
            <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
              {totals.map(([name, count]) => (
                <button
                  type="button"
                  key={name}
                  onClick={() => void showEvent(name)}
                  title="Show the details behind this number"
                  className={`p-3 rounded-xl border text-left cursor-pointer transition hover:bg-white/10 ${
                    openEvent === name ? "border-cyan-400/70 bg-cyan-950/30" : "border-white/10 bg-white/5"
                  }`}
                >
                  <div className={`text-lg font-bold ${count === 0 ? "text-gray-500" : "text-white"}`}>
                    {NOT_REPORTED_YET.has(name) && count === 0 ? "n/a" : count.toLocaleString()}
                  </div>
                  <div className="text-[10px] text-gray-400 uppercase tracking-wide">{EVENT_LABELS[name] || name}</div>
                  {NOT_REPORTED_YET.has(name) && count === 0 && <div className="text-[9px] text-gray-500">Not in the app yet</div>}
                </button>
              ))}
            </div>
            {openEvent && (
              <div className="border border-cyan-500/30 rounded-xl p-3 space-y-3 bg-cyan-950/10">
                <div className="flex items-center justify-between">
                  <h5 className="text-xs font-bold text-cyan-200">{EVENT_LABELS[openEvent] || openEvent}: details</h5>
                  <button type="button" onClick={() => setOpenEvent(null)} className="text-[11px] text-gray-400 hover:text-white cursor-pointer">
                    Close
                  </button>
                </div>
                {eventLoading ? (
                  <p className="text-xs text-gray-500">Loading...</p>
                ) : eventRows.length === 0 ? (
                  <p className="text-xs text-gray-500">
                    {NOT_REPORTED_YET.has(openEvent)
                      ? "The Unreal app does not report this yet."
                      : "Nothing recorded in this period. Details are kept from 17 Nov 2026 onwards."}
                  </p>
                ) : (
                  <>
                    {openEvent !== "session_start" && (
                      <table className="w-full text-xs">
                        <thead className="text-gray-400 uppercase text-[10px]">
                          <tr>
                            <th className="text-left px-2 py-1">What</th>
                            <th className="text-right px-2 py-1">Times</th>
                          </tr>
                        </thead>
                        <tbody>
                          {breakdown.slice(0, 30).map(([what, times]) => (
                            <tr key={what} className="border-t border-white/5 text-gray-200">
                              <td className="px-2 py-1">{what}</td>
                              <td className="px-2 py-1 text-right">{times}</td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    )}
                    <table className="w-full text-xs">
                      <thead className="text-gray-400 uppercase text-[10px]">
                        <tr>
                          <th className="text-left px-2 py-1">When</th>
                          <th className="text-left px-2 py-1">Who</th>
                          <th className="text-left px-2 py-1">What</th>
                        </tr>
                      </thead>
                      <tbody>
                        {eventRows.slice(0, 100).map((row, i) => (
                          <tr key={i} className="border-t border-white/5 text-gray-200 align-top">
                            <td className="px-2 py-1 whitespace-nowrap">{new Date(row.ts).toLocaleString()}</td>
                            <td className="px-2 py-1 whitespace-nowrap">{row.visitor || `Visitor ${row.session}`}</td>
                            <td className="px-2 py-1">{openEvent === "session_start" ? "Visit started" : describeEvent(openEvent, row.params)}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                    {eventRows.length > 100 && <p className="text-[10px] text-gray-500">Showing the latest 100 of {eventRows.length}.</p>}
                  </>
                )}
              </div>
            )}
            {summary.top_units.length > 0 && (
              <div className="overflow-x-auto border border-white/10 rounded-xl">
                <table className="w-full text-xs">
                  <thead className="bg-white/5 text-gray-400 uppercase text-[10px]">
                    <tr>
                      <th className="text-left px-3 py-2">Unit</th>
                      <th className="text-right px-3 py-2">Viewed</th>
                      <th className="text-right px-3 py-2">Favourited</th>
                      <th className="text-right px-3 py-2">Directions</th>
                      <th className="text-right px-3 py-2">All activity</th>
                    </tr>
                  </thead>
                  <tbody>
                    {summary.top_units.slice(0, 20).map((row) => (
                      <tr key={row.unit} className="border-t border-white/5 text-gray-200">
                        <td className="px-3 py-2">{row.unit}</td>
                        <td className="px-3 py-2 text-right">{row.counts.view_item || 0}</td>
                        <td className="px-3 py-2 text-right">{row.counts.add_to_wishlist || 0}</td>
                        <td className="px-3 py-2 text-right">{row.counts.get_directions || 0}</td>
                        <td className="px-3 py-2 text-right">{row.total}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </>
        )}
      </section>
    </div>
  );
}
