/**
 * Sight Portal insights: buyer shortlists (QR handoff), sales leads and usage analytics sent by the Unreal app.
 *
 * Storage is Firestore through the Admin SDK, using the service account in the FIREBASE_SERVICE_ACCOUNT secret and
 * the database named in firebase-applet-config.json. When the secret or the firebase-admin package is missing the
 * module falls back to a JSON file on disk, so local development still works; that file does not survive a Cloud
 * Run restart, and the protected endpoints (leads, analytics summary) stay closed because callers cannot be verified.
 *
 * Analytics event names and parameters follow Google Analytics 4 conventions (view_item, add_to_wishlist,
 * generate_lead, search...), so the same events can be forwarded to GA4 later without renaming.
 */
import type { Express, Request, Response } from "express";
import crypto from "crypto";
import fs from "fs";
import path from "path";
import nodemailer from "nodemailer";
import firebaseConfig from "../firebase-applet-config.json";
import { DEFAULT_ADMIN_EMAIL } from "./serverEmailNotifier";

// ---------------------------------------------------------------------------------------------------------------------
// Types

export interface ShortlistUnit {
  name: string;
  zone?: string;
  block?: string;
  doorNo?: number;
  price?: number;
  surface?: number;
  buildingSurface?: number;
  bedrooms?: number;
  bathrooms?: number;
  propertyClass?: string;
  availability?: string;
}

export interface Shortlist {
  id: string;
  clientSlug: string;
  createdAt: string;
  units: ShortlistUnit[];
  currency?: string;
  // The client's display name when the shortlist was made, shown as the page's brand line
  projectName?: string;
  sessionId?: string;
  leadId?: string;
  openedCount: number;
  lastOpenedAt?: string;
}

export interface Lead {
  id: string;
  clientSlug: string;
  createdAt: string;
  name: string;
  phone: string;
  note?: string;
  shortlistId?: string;
  unitNames: string[];
  sessionId?: string;
  status: "new" | "contacted" | "closed";
}

export interface AnalyticsEvent {
  clientSlug: string;
  sessionId: string;
  name: string;
  params: Record<string, string | number | boolean>;
  ts: string;
  receivedAt: string;
}

interface DailyAggregate {
  clientSlug: string;
  day: string; // YYYY-MM-DD (UTC)
  events: Record<string, number>;
  units: Record<string, Record<string, number>>; // unit name -> event name -> count
  sessions: number;
}

// ---------------------------------------------------------------------------------------------------------------------
// Validation helpers

const SLUG_PATTERN = /^[a-z0-9][a-z0-9_-]{0,63}$/i;
// GA4: letters, digits and underscores, starting with a letter, at most 40 characters
const EVENT_NAME_PATTERN = /^[a-zA-Z][a-zA-Z0-9_]{0,39}$/;
const MAX_UNITS = 30;
const MAX_EVENTS_PER_REQUEST = 100;

// How much traffic from the Unreal app the server accepts for one client. Chosen per client in the portal
// (Leads & Analytics); "default" suits a normal showroom day.
export type TrafficPreset = "default" | "busy" | "open";
export interface TrafficLimits {
  label: string;
  // Analytics batches one Unreal session may send per minute
  sessionBatchesPerMinute: number;
  shortlistsPerMinute: number;
  shortlistsPerDay: number;
  analyticsBatchesPerMinute: number;
  // How often the Unreal app is asked to send its analytics
  flushSeconds: number;
}
export const TRAFFIC_PRESETS: Record<TrafficPreset, TrafficLimits> = {
  default: { label: "Default", sessionBatchesPerMinute: 20, shortlistsPerMinute: 10, shortlistsPerDay: 200, analyticsBatchesPerMinute: 300, flushSeconds: 10 },
  busy: { label: "Busy Day", sessionBatchesPerMinute: 30, shortlistsPerMinute: 20, shortlistsPerDay: 400, analyticsBatchesPerMinute: 600, flushSeconds: 5 },
  open: { label: "Open Day", sessionBatchesPerMinute: 50, shortlistsPerMinute: 50, shortlistsPerDay: 500, analyticsBatchesPerMinute: 600, flushSeconds: 5 },
};
const asTrafficPreset = (value: unknown): TrafficPreset => (value === "busy" || value === "open" ? value : "default");
const MAX_PARAMS = 25;

const cleanText = (value: unknown, max: number): string =>
  typeof value === "string" ? value.replace(/[\u0000-\u001f\u007f]/g, " ").trim().slice(0, max) : "";

const cleanNumber = (value: unknown): number | undefined => {
  const n = typeof value === "number" ? value : typeof value === "string" && value.trim() !== "" ? Number(value) : NaN;
  return Number.isFinite(n) ? n : undefined;
};

const cleanSlug = (value: unknown): string | null => {
  const slug = cleanText(value, 64);
  return SLUG_PATTERN.test(slug) ? slug : null;
};

const dropUndefined = <T extends object>(value: T): T =>
  Object.fromEntries(Object.entries(value).filter(([, v]) => v !== undefined)) as T;

export const sanitizeUnit = (raw: any): ShortlistUnit | null => {
  if (!raw || typeof raw !== "object") return null;
  const name = cleanText(raw.name ?? raw.Name, 80);
  if (!name) return null;
  return dropUndefined({
    name,
    zone: cleanText(raw.zone ?? raw.Zone, 40) || undefined,
    block: cleanText(raw.block ?? raw.Block, 40) || undefined,
    doorNo: cleanNumber(raw.doorNo ?? raw.door_no ?? raw.DoorNo),
    price: cleanNumber(raw.price ?? raw.Price),
    surface: cleanNumber(raw.surface ?? raw.Surface),
    buildingSurface: cleanNumber(raw.buildingSurface ?? raw.building_surface ?? raw.BuildingSurface),
    bedrooms: cleanNumber(raw.bedrooms ?? raw.BedroomsCount),
    bathrooms: cleanNumber(raw.bathrooms ?? raw.BathroomsCount),
    propertyClass: cleanText(raw.propertyClass ?? raw.property_class ?? raw.Class, 60) || undefined,
    availability: cleanText(raw.availability ?? raw.Availability, 40) || undefined,
  });
};

export const sanitizeEventParams = (raw: any): Record<string, string | number | boolean> => {
  const out: Record<string, string | number | boolean> = {};
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return out;
  for (const [key, value] of Object.entries(raw)) {
    if (Object.keys(out).length >= MAX_PARAMS) break;
    if (!EVENT_NAME_PATTERN.test(key)) continue;
    if (typeof value === "string") out[key] = cleanText(value, 100);
    else if (typeof value === "number" && Number.isFinite(value)) out[key] = value;
    else if (typeof value === "boolean") out[key] = value;
  }
  return out;
};

// Unit names become Firestore map keys in the daily aggregate: '.', '/' and friends are not allowed there
const aggregateKey = (unitName: string): string => unitName.replace(/[.\/\[\]*`~$#]/g, "_").slice(0, 80);

const newId = (bytes = 9): string => crypto.randomBytes(bytes).toString("base64url");

const escapeHtml = (value: unknown): string =>
  String(value ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c] as string));

// ---------------------------------------------------------------------------------------------------------------------
// Storage

interface InsightsStore {
  readonly mode: "firestore" | "disk";
  saveShortlist(shortlist: Shortlist): Promise<void>;
  getShortlist(id: string): Promise<Shortlist | null>;
  noteShortlistOpened(id: string): Promise<void>;
  saveLead(lead: Lead): Promise<void>;
  listLeads(clientSlug: string, limit: number): Promise<Lead[]>;
  updateLeadStatus(id: string, clientSlug: string, status: Lead["status"]): Promise<boolean>;
  saveEvents(events: AnalyticsEvent[], newSessions: number): Promise<void>;
  listDaily(clientSlug: string, sinceDay: string): Promise<DailyAggregate[]>;
  // The individual events of one kind, newest first (for the dashboard's drill-down)
  listEvents(clientSlug: string, name: string, sinceIso: string, limit: number): Promise<AnalyticsEvent[]>;
}

const DISK_PATH = path.join(process.cwd(), "insights_cache.json");

interface DiskState {
  shortlists: Record<string, Shortlist>;
  leads: Record<string, Lead>;
  daily: Record<string, DailyAggregate>;
  events: AnalyticsEvent[];
}

const MAX_DISK_EVENTS = 5000;

function createDiskStore(): InsightsStore {
  let state: DiskState = { shortlists: {}, leads: {}, daily: {}, events: [] };
  try {
    if (fs.existsSync(DISK_PATH)) {
      const parsed = JSON.parse(fs.readFileSync(DISK_PATH, "utf-8"));
      state = { shortlists: parsed.shortlists || {}, leads: parsed.leads || {}, daily: parsed.daily || {}, events: Array.isArray(parsed.events) ? parsed.events : [] };
    }
  } catch (e) {
    console.warn("[Insights] Could not read the disk cache:", e);
  }
  const persist = () => {
    try {
      fs.writeFileSync(DISK_PATH, JSON.stringify(state), "utf-8");
    } catch (e) {
      console.warn("[Insights] Could not write the disk cache:", e);
    }
  };
  return {
    mode: "disk",
    async saveShortlist(shortlist) { state.shortlists[shortlist.id] = shortlist; persist(); },
    async getShortlist(id) { return state.shortlists[id] || null; },
    async noteShortlistOpened(id) {
      const s = state.shortlists[id];
      if (s) { s.openedCount = (s.openedCount || 0) + 1; s.lastOpenedAt = new Date().toISOString(); persist(); }
    },
    async saveLead(lead) { state.leads[lead.id] = lead; persist(); },
    async listLeads(clientSlug, limit) {
      return Object.values(state.leads).filter((l) => l.clientSlug === clientSlug)
        .sort((a, b) => b.createdAt.localeCompare(a.createdAt)).slice(0, limit);
    },
    async updateLeadStatus(id, clientSlug, status) {
      const lead = state.leads[id];
      if (!lead || lead.clientSlug !== clientSlug) return false;
      lead.status = status; persist(); return true;
    },
    async saveEvents(events, newSessions) {
      state.events.push(...events);
      if (state.events.length > MAX_DISK_EVENTS) state.events = state.events.slice(-MAX_DISK_EVENTS);
      for (const [key, delta] of Object.entries(aggregateEvents(events, newSessions))) {
        const current = state.daily[key] || { clientSlug: delta.clientSlug, day: delta.day, events: {}, units: {}, sessions: 0 };
        for (const [name, count] of Object.entries(delta.events)) current.events[name] = (current.events[name] || 0) + count;
        for (const [unit, counts] of Object.entries(delta.units)) {
          current.units[unit] = current.units[unit] || {};
          for (const [name, count] of Object.entries(counts)) current.units[unit][name] = (current.units[unit][name] || 0) + count;
        }
        current.sessions += delta.sessions;
        state.daily[key] = current;
      }
      persist();
    },
    async listDaily(clientSlug, sinceDay) {
      return Object.values(state.daily).filter((d) => d.clientSlug === clientSlug && d.day >= sinceDay);
    },
    async listEvents(clientSlug, name, sinceIso, limit) {
      return state.events.filter((e) => e.clientSlug === clientSlug && e.name === name && e.ts >= sinceIso)
        .sort((a, b) => b.ts.localeCompare(a.ts)).slice(0, limit);
    },
  };
}

// Groups a batch of events into per-client, per-day counters
export function aggregateEvents(events: AnalyticsEvent[], newSessions: number): Record<string, DailyAggregate> {
  const out: Record<string, DailyAggregate> = {};
  events.forEach((event, index) => {
    const day = event.ts.slice(0, 10);
    const key = `${event.clientSlug}_${day}`;
    const agg = out[key] || (out[key] = { clientSlug: event.clientSlug, day, events: {}, units: {}, sessions: 0 });
    if (index === 0) agg.sessions += newSessions;
    agg.events[event.name] = (agg.events[event.name] || 0) + 1;
    const unit = typeof event.params.item_id === "string" ? aggregateKey(event.params.item_id) : "";
    if (unit) {
      agg.units[unit] = agg.units[unit] || {};
      agg.units[unit][event.name] = (agg.units[unit][event.name] || 0) + 1;
    }
  });
  return out;
}

interface AdminContext {
  store: InsightsStore;
  verifyIdToken: (token: string) => Promise<{ uid: string; email?: string; emailVerified?: boolean }>;
  getUserProfile: (uid: string) => Promise<{ role?: string; clientId?: string; clientIds?: string[] } | null>;
  // Email addresses of the moderators assigned to this client
  listModeratorEmails: (slug: string) => Promise<string[]>;
  // The client accounts assigned to this client's portal
  listClientUsers: (slug: string) => Promise<{ uid: string; email: string }[]>;
  // Removes a client account's profile, only if it is a client account of this portal. False when it is not.
  deleteClientUser: (slug: string, uid: string) => Promise<boolean>;
  // True when the portal has a client with this id (the slug used in /ws/<slug>)
  clientExists: (slug: string) => Promise<boolean>;
  getTrafficPreset: (slug: string) => Promise<TrafficPreset>;
  setTrafficPreset: (slug: string, preset: TrafficPreset) => Promise<void>;
  getClientName: (slug: string) => Promise<string>;
}

function parseServiceAccount(raw: string): Record<string, any> {
  const text = raw.trim();
  // Accept the JSON itself or a base64 copy of it (for secret fields that reject multi-line values)
  const json = text.startsWith("{") ? text : Buffer.from(text, "base64").toString("utf-8");
  const parsed = JSON.parse(json);
  if (typeof parsed.private_key === "string") parsed.private_key = parsed.private_key.replace(/\\n/g, "\n");
  return parsed;
}

async function createAdminContext(): Promise<AdminContext | null> {
  const raw = process.env.FIREBASE_SERVICE_ACCOUNT;
  if (!raw) {
    console.warn("[Insights] FIREBASE_SERVICE_ACCOUNT is not set: using the disk cache (not durable).");
    return null;
  }
  try {
    const serviceAccount = parseServiceAccount(raw);
    const config = firebaseConfig as Record<string, any>;
    if (serviceAccount.project_id && config.projectId && serviceAccount.project_id !== config.projectId) {
      console.warn(`[Insights] The service account is for project "${serviceAccount.project_id}" but the portal uses "${config.projectId}".`);
    }
    const adminApp = await import("firebase-admin/app");
    const adminFirestore = await import("firebase-admin/firestore");
    const adminAuth = await import("firebase-admin/auth");
    const app = adminApp.getApps().find((a) => a.name === "sight-insights") ||
      adminApp.initializeApp({ credential: adminApp.cert(serviceAccount as any), projectId: serviceAccount.project_id }, "sight-insights");
    const databaseId: string = config.firestoreDatabaseId || "(default)";
    const db = databaseId !== "(default)" ? adminFirestore.getFirestore(app, databaseId) : adminFirestore.getFirestore(app);
    db.settings({ ignoreUndefinedProperties: true });
    const auth = adminAuth.getAuth(app);
    const { FieldValue } = adminFirestore;
    console.log(`[Insights] Firestore storage ready: project "${serviceAccount.project_id}", database "${databaseId}".`);

    const store: InsightsStore = {
      mode: "firestore",
      async saveShortlist(shortlist) { await db.collection("shortlists").doc(shortlist.id).set(shortlist); },
      async getShortlist(id) {
        const snap = await db.collection("shortlists").doc(id).get();
        return snap.exists ? (snap.data() as Shortlist) : null;
      },
      async noteShortlistOpened(id) {
        await db.collection("shortlists").doc(id).update({ openedCount: FieldValue.increment(1), lastOpenedAt: new Date().toISOString() });
      },
      async saveLead(lead) { await db.collection("leads").doc(lead.id).set(lead); },
      async listLeads(clientSlug, limit) {
        // Sorted here rather than in the query, so no composite index has to be created
        const snap = await db.collection("leads").where("clientSlug", "==", clientSlug).limit(1000).get();
        return snap.docs.map((d) => d.data() as Lead).sort((a, b) => b.createdAt.localeCompare(a.createdAt)).slice(0, limit);
      },
      async updateLeadStatus(id, clientSlug, status) {
        const ref = db.collection("leads").doc(id);
        const snap = await ref.get();
        if (!snap.exists || (snap.data() as Lead).clientSlug !== clientSlug) return false;
        await ref.update({ status });
        return true;
      },
      async saveEvents(events, newSessions) {
        const batch = db.batch();
        for (const event of events) batch.set(db.collection("analyticsEvents").doc(), event);
        for (const [key, delta] of Object.entries(aggregateEvents(events, newSessions))) {
          const update: Record<string, any> = { clientSlug: delta.clientSlug, day: delta.day };
          if (delta.sessions) update.sessions = FieldValue.increment(delta.sessions);
          const eventCounts: Record<string, any> = {};
          for (const [name, count] of Object.entries(delta.events)) eventCounts[name] = FieldValue.increment(count);
          update.events = eventCounts;
          const unitCounts: Record<string, any> = {};
          for (const [unit, counts] of Object.entries(delta.units)) {
            unitCounts[unit] = {};
            for (const [name, count] of Object.entries(counts)) unitCounts[unit][name] = FieldValue.increment(count);
          }
          update.units = unitCounts;
          batch.set(db.collection("analyticsDaily").doc(key), update, { merge: true });
        }
        await batch.commit();
      },
      async listDaily(clientSlug, sinceDay) {
        const snap = await db.collection("analyticsDaily").where("clientSlug", "==", clientSlug).limit(800).get();
        return snap.docs.map((d) => {
          const data = d.data() as Partial<DailyAggregate>;
          return { clientSlug, day: data.day || "", events: data.events || {}, units: data.units || {}, sessions: data.sessions || 0 };
        }).filter((d) => d.day >= sinceDay);
      },
      async listEvents(clientSlug, name, sinceIso, limit) {
        // Two equality filters need no composite index; the date filter and the sort are done here
        const snap = await db.collection("analyticsEvents").where("clientSlug", "==", clientSlug).where("name", "==", name).limit(3000).get();
        return snap.docs.map((d) => d.data() as AnalyticsEvent).filter((e) => e.ts >= sinceIso)
          .sort((a, b) => b.ts.localeCompare(a.ts)).slice(0, limit);
      },
    };
    return {
      store,
      verifyIdToken: async (token) => {
        const decoded = await auth.verifyIdToken(token);
        return { uid: decoded.uid, email: decoded.email, emailVerified: decoded.email_verified === true };
      },
      getUserProfile: async (uid) => {
        const snap = await db.collection("users").doc(uid).get();
        return snap.exists ? (snap.data() as { role?: string; clientId?: string; clientIds?: string[] }) : null;
      },
      listModeratorEmails: async (slug) => {
        // One array filter needs no extra index; the role is checked here
        const snap = await db.collection("users").where("clientIds", "array-contains", slug).limit(50).get();
        return snap.docs.map((d) => d.data() as { role?: string; email?: string })
          .filter((u) => u.role === "moderator" && typeof u.email === "string" && /^[^\s@,;<>]+@[^\s@,;<>]+$/.test(u.email))
          .map((u) => u.email as string);
      },
      listClientUsers: async (slug) => {
        const snap = await db.collection("users").where("clientId", "==", slug).limit(200).get();
        return snap.docs.filter((d) => d.data().role === "client")
          .map((d) => ({ uid: d.id, email: typeof d.data().email === "string" ? d.data().email : "" }));
      },
      deleteClientUser: async (slug, uid) => {
        const ref = db.collection("users").doc(uid);
        const snap = await ref.get();
        const data = snap.data();
        if (!snap.exists || data?.role !== "client" || data?.clientId !== slug) return false;
        await ref.delete();
        return true;
      },
      clientExists: async (slug) => (await db.collection("clients").doc(slug).get()).exists,
      getTrafficPreset: async (slug) => asTrafficPreset((await db.collection("clients").doc(slug).get()).data()?.trafficPreset),
      setTrafficPreset: async (slug, preset) => { await db.collection("clients").doc(slug).update({ trafficPreset: preset }); },
      getClientName: async (slug) => {
        const data = (await db.collection("clients").doc(slug).get()).data();
        return typeof data?.name === "string" ? data.name : "";
      },
    };
  } catch (e: any) {
    console.error("[Insights] Could not start Firestore storage, using the disk cache instead:", e?.message || e);
    return null;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Email

async function sendLeadEmail(lead: Lead, shortlistUrl: string | null): Promise<void> {
  const recipient = process.env.LEADS_EMAIL || DEFAULT_ADMIN_EMAIL;
  const subject = `New lead: ${lead.name} (${lead.clientSlug})`;
  const lines = [
    `Name: ${lead.name}`,
    `Phone: ${lead.phone}`,
    lead.note ? `Note: ${lead.note}` : "",
    `Project: ${lead.clientSlug}`,
    lead.unitNames.length ? `Shortlisted units: ${lead.unitNames.join(", ")}` : "No units shortlisted",
    shortlistUrl ? `Shortlist: ${shortlistUrl}` : "",
    `Received: ${lead.createdAt}`,
  ].filter(Boolean);
  const text = lines.join("\n");
  const html = `<div style="font-family:Arial,sans-serif;font-size:14px;line-height:1.6">${lines.map((l) => `<div>${escapeHtml(l)}</div>`).join("")}</div>`;
  try {
    if (process.env.SMTP_HOST && process.env.SMTP_USER) {
      const transporter = nodemailer.createTransport({
        host: process.env.SMTP_HOST,
        port: parseInt(process.env.SMTP_PORT || "587", 10),
        secure: process.env.SMTP_SECURE === "true" || process.env.SMTP_PORT === "465",
        auth: { user: process.env.SMTP_USER, pass: process.env.SMTP_PASS },
      });
      await transporter.sendMail({ from: process.env.FROM_EMAIL || `"Sight Portal" <no-reply@sightportal.app>`, to: recipient, subject, text, html });
    } else if (process.env.RESEND_API_KEY) {
      const res = await fetch("https://api.resend.com/emails", {
        method: "POST",
        headers: { Authorization: `Bearer ${process.env.RESEND_API_KEY}`, "Content-Type": "application/json" },
        body: JSON.stringify({ from: process.env.RESEND_FROM || "Sight Portal <onboarding@resend.dev>", to: [recipient], subject, html, text }),
      });
      if (!res.ok) throw new Error(`Resend answered ${res.status}`);
    } else {
      console.log(`[Insights] No email service is configured; lead ${lead.id} is stored but no email was sent.`);
    }
  } catch (e: any) {
    console.warn(`[Insights] Lead ${lead.id} is stored, but the email failed:`, e?.message || e);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Public shortlist page

const formatNumber = (value: number | undefined, digits = 0): string =>
  typeof value === "number" ? value.toLocaleString("en-US", { maximumFractionDigits: digits }) : "";

export function renderShortlistPage(shortlist: Shortlist | null): string {
  // What the buyer sees: the brand and the project, nothing that points at the portal
  const brand = shortlist?.projectName ? `Sight Solution - ${shortlist.projectName}` : "Sight Viz";
  const heading = shortlist ? "Your shortlist" : "Shortlist not found";
  const cards = (shortlist?.units || []).map((unit) => {
    const facts = [
      unit.propertyClass ? escapeHtml(unit.propertyClass) : "",
      typeof unit.bedrooms === "number" ? `${unit.bedrooms} bed` : "",
      typeof unit.bathrooms === "number" ? `${unit.bathrooms} bath` : "",
      typeof unit.surface === "number" ? `${formatNumber(unit.surface, 1)} m&sup2;` : "",
    ].filter(Boolean).join(" &middot; ");
    const place = [unit.zone ? `Zone ${escapeHtml(unit.zone)}` : "", unit.block ? `Block ${escapeHtml(unit.block)}` : "",
      typeof unit.doorNo === "number" && unit.doorNo > 0 ? `Door ${unit.doorNo}` : ""].filter(Boolean).join(" &middot; ");
    const price = typeof unit.price === "number" && unit.price > 0
      ? `${formatNumber(unit.price)} ${escapeHtml(shortlist?.currency || "IQD")}` : "";
    return `<article class="card">
      <div class="row"><h2>${escapeHtml(unit.name)}</h2>${unit.availability ? `<span class="badge">${escapeHtml(unit.availability)}</span>` : ""}</div>
      ${price ? `<div class="price">${price}</div>` : ""}
      ${facts ? `<div class="facts">${facts}</div>` : ""}
      ${place ? `<div class="place">${place}</div>` : ""}
    </article>`;
  }).join("\n");
  const body = shortlist
    ? `<p class="lead">${shortlist.units.length} ${shortlist.units.length === 1 ? "home" : "homes"} saved on ${escapeHtml(shortlist.createdAt.slice(0, 10))}. Prices and availability are as shown at that time.</p>${cards || "<p>No homes were saved.</p>"}`
    : `<p class="lead">This link is not valid or the shortlist has been removed. Ask your sales advisor for a new one.</p>`;
  return `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="robots" content="noindex,nofollow"><meta name="referrer" content="no-referrer"><title>${escapeHtml(brand)} - QR</title>
<style>
  body{margin:0;font-family:system-ui,-apple-system,"Segoe UI",Roboto,Arial,sans-serif;background:#0d1117;color:#e6edf3}
  main{max-width:640px;margin:0 auto;padding:24px 16px 48px}
  .brand{font-size:13px;letter-spacing:.08em;text-transform:uppercase;color:#e7c873;margin:0}
  h1{font-size:24px;margin:8px 0 4px}
  .lead{color:#9aa7b4;font-size:14px;margin:0 0 20px}
  .card{background:#161b22;border:1px solid #2a323c;border-radius:14px;padding:16px;margin-bottom:12px}
  .row{display:flex;align-items:center;justify-content:space-between;gap:12px}
  h2{font-size:18px;margin:0}
  .badge{font-size:12px;border:1px solid #c9a24b;color:#e7c873;border-radius:999px;padding:2px 10px;white-space:nowrap}
  .price{font-size:20px;font-weight:700;color:#e7c873;margin-top:8px}
  .facts{margin-top:8px;font-size:14px}
  .place{margin-top:4px;font-size:13px;color:#9aa7b4}
</style></head>
<body><main><p class="brand">${escapeHtml(brand)}</p><h1>${heading}</h1>${body}</main></body></html>`;
}

// ---------------------------------------------------------------------------------------------------------------------
// Routes

/**
 * When PUBLIC_SHORTLIST_URL names a domain of its own, requests that arrive on that domain may only open shortlist
 * pages: everything else (the portal sign-in, the API) answers "Not found", so the link gives no way into the portal.
 * Must be added before every other route.
 */
export function shortlistHostGuard(req: Request, res: Response, next: () => void): void {
  let publicHost = "";
  try {
    publicHost = process.env.PUBLIC_SHORTLIST_URL ? new URL(process.env.PUBLIC_SHORTLIST_URL).host.toLowerCase() : "";
  } catch {
    publicHost = "";
  }
  const requestHost = String(req.headers["x-forwarded-host"] || req.headers.host || "").split(",")[0].trim().toLowerCase();
  if (publicHost && requestHost === publicHost && !/^\/s\/[A-Za-z0-9_-]+\/?$/.test(req.path)) {
    res.status(404).type("text/plain").send("Not found");
    return;
  }
  next();
}

export interface InsightsOptions {
  // Used only when Firestore is unavailable (local development); with Firestore the clients collection decides
  isKnownClient: (slug: string) => boolean;
}

// Checks that the request comes from a signed-in portal user who may act on this client.
// Answers the request itself (401/403/503) and returns false when it may not go ahead.
export type ClientAccessCheck = (req: Request, res: Response, clientSlug: string) => Promise<boolean>;

// The signed-in portal user behind a request
export interface PortalUser {
  email: string;
  role: "owner" | "admin" | "moderator" | "client" | "";
  clientId: string;
  clientIds: string[];
}

export interface InsightsRoutes {
  // For reading a client's leads and analytics
  authorize: ClientAccessCheck;
  // For changing what Unreal shows for a client (property data, media, re-sync) and for filing its bug tickets
  authorizeWrite: ClientAccessCheck;
  // For the admin console only: the owner and admins. Answers the request itself and returns null when refused.
  authorizeAdmin: (req: Request, res: Response) => Promise<PortalUser | null>;
  // For the Google Drive endpoints: any portal account with a portal assigned (sign-in sent in X-Portal-Auth)
  authorizeMember: (req: Request, res: Response) => Promise<boolean>;
  // Email addresses of the moderators assigned to a client (empty without the database connection)
  listModeratorEmails: (clientSlug: string) => Promise<string[]>;
}

const isLoopback = (req: Request): boolean => {
  const address = req.socket.remoteAddress || "";
  return !req.headers["x-forwarded-for"] && (address === "127.0.0.1" || address === "::1" || address === "::ffff:127.0.0.1");
};

export async function registerInsightsRoutes(app: Express, options: InsightsOptions): Promise<InsightsRoutes> {
  const admin = await createAdminContext();
  const store = admin?.store || createDiskStore();

  // Limits for the endpoints the Unreal app calls without signing in. One counter per key and time window.
  const hits = new Map<string, { count: number; resetAt: number }>();
  const overLimit = (key: string, max: number, windowMs: number): boolean => {
    const now = Date.now();
    const entry = hits.get(key);
    if (!entry || entry.resetAt <= now) {
      if (hits.size > 5000) for (const [k, v] of hits) if (v.resetAt <= now) hits.delete(k);
      if (hits.size > 50_000) hits.clear();
      hits.set(key, { count: 1, resetAt: now + windowMs });
      return false;
    }
    entry.count += 1;
    return entry.count > max;
  };
  const tooMany = (res: Response): true => {
    res.status(429).json({ status: "error", error: "Too many requests. Try again shortly." });
    return true;
  };
  // Per caller. X-Forwarded-For is a list: the first entry is whatever the caller claims and can be faked, the
  // last one is added by Google's own front end and cannot. Counting only the first let a script dodge the limit
  // by changing it on every request, so both are counted; the last may be shared by several callers behind one
  // of Google's proxies, so it gets a wider allowance.
  const limited = (req: Request, res: Response, bucket: string, max: number, windowMs: number): boolean => {
    const forwarded = String(req.headers["x-forwarded-for"] || "").split(",").map((part) => part.trim()).filter(Boolean);
    const direct = req.socket.remoteAddress || "unknown";
    const claimed = forwarded[0] || direct;
    const seenByGoogle = forwarded[forwarded.length - 1] || direct;
    const overClaimed = overLimit(`${bucket}:c:${claimed}`, max, windowMs);
    const overSeen = overLimit(`${bucket}:g:${seenByGoogle}`, max * 5, windowMs);
    return overClaimed || overSeen ? tooMany(res) : false;
  };
  // Per client, whoever the caller is: a ceiling well above what a busy showroom produces, so a script that
  // gets past the per-caller limits still cannot flood one client's leads or analytics.
  const clientLimited = (res: Response, bucket: string, clientSlug: string, perMinute: number, perDay: number | null): boolean =>
    overLimit(`${bucket}:m:${clientSlug}`, perMinute, 60_000) || (perDay !== null && overLimit(`${bucket}:d:${clientSlug}`, perDay, 86_400_000)) ? tooMany(res) : false;

  // The preset chosen for each client, remembered for a minute to avoid a database read on every request
  const trafficPresets = new Map<string, { preset: TrafficPreset; until: number }>();
  const trafficPresetOf = async (clientSlug: string): Promise<TrafficPreset> => {
    const cached = trafficPresets.get(clientSlug);
    if (cached && (!admin || cached.until > Date.now())) return cached.preset;
    if (!admin) return "default";
    let preset: TrafficPreset = "default";
    try {
      preset = await admin.getTrafficPreset(clientSlug);
    } catch (e: any) {
      console.error("[Insights] Could not read a client's traffic preset:", e?.message || e);
    }
    if (trafficPresets.size > 500) trafficPresets.clear();
    trafficPresets.set(clientSlug, { preset, until: Date.now() + 60_000 });
    return preset;
  };
  const trafficLimitsOf = async (clientSlug: string): Promise<TrafficLimits> => TRAFFIC_PRESETS[await trafficPresetOf(clientSlug)];

  // Data is only accepted for clients that exist in the portal, so a mistyped slug cannot create orphan records.
  // Answers are remembered for five minutes to avoid a database read on every analytics batch.
  const knownClients = new Map<string, { known: boolean; until: number }>();
  const isKnownClient = async (slug: string): Promise<boolean> => {
    if (!admin) return options.isKnownClient(slug);
    const cached = knownClients.get(slug);
    if (cached && cached.until > Date.now()) return cached.known;
    const known = await admin.clientExists(slug);
    if (knownClients.size > 500) knownClients.clear();
    knownClients.set(slug, { known, until: Date.now() + (known ? 300_000 : 30_000) });
    return known;
  };

  const baseUrl = (req: Request): string => {
    // The address buyers see in the QR link. Set PUBLIC_SHORTLIST_URL to a domain of your own to keep the portal's out of it.
    if (process.env.PUBLIC_SHORTLIST_URL && /^https?:\/\//.test(process.env.PUBLIC_SHORTLIST_URL)) return process.env.PUBLIC_SHORTLIST_URL.replace(/\/+$/, "");
    if (process.env.APP_URL && /^https?:\/\//.test(process.env.APP_URL)) return process.env.APP_URL.replace(/\/+$/, "");
    const protocol = (req.headers["x-forwarded-proto"] as string)?.split(",")[0] || req.protocol || "https";
    return `${protocol}://${req.headers["x-forwarded-host"] || req.get("host")}`;
  };

  // Resolves the signed-in portal user and whether they may see a client's leads and analytics
  const authorize: ClientAccessCheck = async (req, res, clientSlug) => {
    if (!admin) {
      res.status(503).json({ status: "error", error: "Leads and analytics need the FIREBASE_SERVICE_ACCOUNT secret on the server." });
      return false;
    }
    return checkUser(req, res, clientSlug);
  };

  // Same check for the endpoints that change what Unreal shows. Without the service account the server cannot
  // verify anyone, so it only accepts changes from the same machine while developing, never on the live site.
  const authorizeWrite: ClientAccessCheck = async (req, res, clientSlug) => {
    if (!admin) {
      if (process.env.NODE_ENV !== "production" && isLoopback(req)) return true;
      res.status(503).json({ status: "error", error: "Changes need the FIREBASE_SERVICE_ACCOUNT secret on the server." });
      return false;
    }
    if (!clientSlug) {
      res.status(400).json({ status: "error", error: "client_slug is required." });
      return false;
    }
    return checkUser(req, res, clientSlug);
  };

  // Who is signed in. Answers 401 itself and returns null when nobody is, or the sign-in has expired.
  const identify = async (req: Request, res: Response, headerName: "authorization" | "x-portal-auth" = "authorization"): Promise<PortalUser | null> => {
    if (!admin) return null;
    const header = String(req.headers[headerName] || "");
    const token = header.startsWith("Bearer ") ? header.slice(7) : "";
    if (!token) {
      res.status(401).json({ status: "error", error: "Sign in to the portal first." });
      return null;
    }
    try {
      const user = await admin.verifyIdToken(token);
      const email = user.email || "";
      if (!!email && user.emailVerified === true && email.toLowerCase() === DEFAULT_ADMIN_EMAIL.toLowerCase()) {
        return { email, role: "owner", clientId: "", clientIds: [] };
      }
      const profile = await admin.getUserProfile(user.uid);
      const role = profile?.role === "owner" || profile?.role === "admin" || profile?.role === "moderator" || profile?.role === "client" ? profile.role : "";
      return {
        email,
        role,
        clientId: typeof profile?.clientId === "string" ? profile.clientId : "",
        clientIds: Array.isArray(profile?.clientIds) ? profile.clientIds.filter((id) => typeof id === "string") : [],
      };
    } catch {
      res.status(401).json({ status: "error", error: "The sign-in has expired. Sign in again." });
      return null;
    }
  };

  // The owner and admins reach every client, a moderator the clients assigned to them, a client account its own
  const checkUser: ClientAccessCheck = async (req, res, clientSlug) => {
    const user = await identify(req, res);
    if (!user) return false;
    const allowed = user.role === "owner" || user.role === "admin" ||
      (user.role === "moderator" && user.clientIds.includes(clientSlug)) ||
      (user.role === "client" && user.clientId === clientSlug);
    if (!allowed) res.status(403).json({ status: "error", error: "This account cannot view that client." });
    return allowed;
  };

  // The people who look after a client's portal: the owner, admins and the moderators assigned to it
  const checkStaff: ClientAccessCheck = async (req, res, clientSlug) => {
    if (!admin) {
      res.status(503).json({ status: "error", error: "This needs the FIREBASE_SERVICE_ACCOUNT secret on the server." });
      return false;
    }
    const user = await identify(req, res);
    if (!user) return false;
    const allowed = user.role === "owner" || user.role === "admin" || (user.role === "moderator" && user.clientIds.includes(clientSlug));
    if (!allowed) res.status(403).json({ status: "error", error: "This account cannot manage that client's users." });
    return allowed;
  };

  const authorizeAdmin = async (req: Request, res: Response): Promise<PortalUser | null> => {
    if (!admin) {
      if (process.env.NODE_ENV !== "production" && isLoopback(req)) return { email: DEFAULT_ADMIN_EMAIL, role: "owner", clientId: "", clientIds: [] };
      res.status(503).json({ status: "error", error: "This needs the FIREBASE_SERVICE_ACCOUNT secret on the server." });
      return null;
    }
    const user = await identify(req, res);
    if (!user) return null;
    if (user.role !== "owner" && user.role !== "admin") {
      res.status(403).json({ status: "error", error: "Only the owner and admins can do this." });
      return null;
    }
    return user;
  };

  // Anyone with a place in the portal: the owner, admins, moderators with a portal, client accounts with a portal.
  // The sign-in is read from X-Portal-Auth, because these requests use Authorization for the Google Drive token.
  const authorizeMember = async (req: Request, res: Response): Promise<boolean> => {
    if (!admin) {
      if (process.env.NODE_ENV !== "production" && isLoopback(req)) return true;
      res.status(503).json({ status: "error", error: "This needs the FIREBASE_SERVICE_ACCOUNT secret on the server." });
      return false;
    }
    const user = await identify(req, res, "x-portal-auth");
    if (!user) return false;
    const allowed = user.role === "owner" || user.role === "admin" ||
      (user.role === "moderator" && user.clientIds.length > 0) || (user.role === "client" && !!user.clientId);
    if (!allowed) res.status(403).json({ status: "error", error: "This account has no portal assigned." });
    return allowed;
  };

  const listModeratorEmails = async (clientSlug: string): Promise<string[]> => {
    if (!admin || !clientSlug) return [];
    try {
      return await admin.listModeratorEmails(clientSlug);
    } catch (e: any) {
      console.error("[Insights] Could not list the moderators of a client:", e?.message || e);
      return [];
    }
  };

  app.get("/api/insights/status", (_req, res) => {
    res.json({ status: "ok", storage: store.mode, durable: store.mode === "firestore" });
  });

  // Unreal: saves the buyer's shortlist (and the lead typed in the app) and returns the link shown as a QR code
  app.post("/api/shortlists", async (req, res) => {
    if (limited(req, res, "shortlist", 20, 60_000)) return;
    try {
      const clientSlug = cleanSlug(req.body?.client_slug);
      if (!clientSlug || !(await isKnownClient(clientSlug))) return res.status(400).json({ status: "error", error: "Unknown client_slug." });
      const rawUnits: any[] = Array.isArray(req.body?.units) ? req.body.units : [];
      const units = rawUnits.slice(0, MAX_UNITS).map(sanitizeUnit).filter((u): u is ShortlistUnit => !!u);
      const leadName = cleanText(req.body?.lead?.name, 80);
      const leadPhone = cleanText(req.body?.lead?.phone, 40);
      const hasLead = !!leadName && !!leadPhone;
      if (!units.length && !hasLead) return res.status(400).json({ status: "error", error: "Send at least one unit or a lead." });
      const limits = await trafficLimitsOf(clientSlug);
      if (clientLimited(res, "shortlist", clientSlug, limits.shortlistsPerMinute, limits.shortlistsPerDay)) return;
      if (leadPhone && !/^[+0-9()\-.\s]{5,40}$/.test(leadPhone)) return res.status(400).json({ status: "error", error: "The phone number is not valid." });

      const now = new Date().toISOString();
      const sessionId = cleanText(req.body?.session_id, 64) || undefined;
      const shortlist: Shortlist = dropUndefined({
        id: newId(), clientSlug, createdAt: now, units, sessionId,
        currency: cleanText(req.body?.currency, 8) || undefined, openedCount: 0,
        projectName: admin ? cleanText(await admin.getClientName(clientSlug), 80) || undefined : undefined,
      });
      let lead: Lead | null = null;
      if (hasLead) {
        lead = dropUndefined({
          id: newId(), clientSlug, createdAt: now, name: leadName, phone: leadPhone,
          note: cleanText(req.body?.lead?.note, 500) || undefined,
          shortlistId: shortlist.id, unitNames: units.map((u) => u.name), sessionId, status: "new" as const,
        });
        shortlist.leadId = lead.id;
      }
      await store.saveShortlist(shortlist);
      if (lead) await store.saveLead(lead);
      const url = `${baseUrl(req)}/s/${shortlist.id}`;
      // The lead is always saved; the email about it is held back if they arrive faster than people can type them
      if (lead && !overLimit(`lead-email:${clientSlug}`, 20, 3_600_000)) void sendLeadEmail(lead, url);
      res.json({ status: "ok", id: shortlist.id, url, lead_id: lead?.id || null, durable: store.mode === "firestore" });
    } catch (e: any) {
      console.error("[Insights] Could not save a shortlist:", e?.message || e);
      res.status(500).json({ status: "error", error: "The shortlist could not be saved." });
    }
  });

  // The buyer's phone: units only, never the lead's contact details
  app.get("/api/shortlists/:id", async (req, res) => {
    if (limited(req, res, "shortlist-read", 120, 60_000)) return;
    try {
      const shortlist = /^[A-Za-z0-9_-]{6,32}$/.test(req.params.id) ? await store.getShortlist(req.params.id) : null;
      if (!shortlist) return res.status(404).json({ status: "error", error: "Shortlist not found." });
      res.json({ status: "ok", id: shortlist.id, client_slug: shortlist.clientSlug, created_at: shortlist.createdAt, currency: shortlist.currency || null, units: shortlist.units });
    } catch (e: any) {
      console.error("[Insights] Could not read a shortlist:", e?.message || e);
      res.status(500).json({ status: "error", error: "The shortlist could not be read." });
    }
  });

  app.get("/s/:id", async (req, res) => {
    if (limited(req, res, "shortlist-read", 120, 60_000)) return;
    res.setHeader("Cache-Control", "no-store");
    res.setHeader("X-Robots-Tag", "noindex, nofollow");
    res.setHeader("Referrer-Policy", "no-referrer");
    try {
      const shortlist = /^[A-Za-z0-9_-]{6,32}$/.test(req.params.id) ? await store.getShortlist(req.params.id) : null;
      if (shortlist) void store.noteShortlistOpened(shortlist.id).catch(() => undefined);
      res.status(shortlist ? 200 : 404).type("html").send(renderShortlistPage(shortlist));
    } catch (e: any) {
      console.error("[Insights] Could not show a shortlist:", e?.message || e);
      res.status(500).type("html").send(renderShortlistPage(null));
    }
  });

  // Unreal: usage events in batches. { client_slug, session_id, session_start?, events: [{ name, params, ts }] }
  app.post("/api/analytics/events", async (req, res) => {
    if (limited(req, res, "analytics", 120, 60_000)) return;
    try {
      const clientSlug = cleanSlug(req.body?.client_slug);
      if (!clientSlug || !(await isKnownClient(clientSlug))) return res.status(400).json({ status: "error", error: "Unknown client_slug." });
      const sessionId = cleanText(req.body?.session_id, 64);
      if (!sessionId) return res.status(400).json({ status: "error", error: "session_id is required." });
      // One Unreal session sends a batch every few seconds; a whole showroom stays below the client ceiling
      const limits = await trafficLimitsOf(clientSlug);
      if (overLimit(`analytics:s:${clientSlug}:${sessionId}`, limits.sessionBatchesPerMinute, 60_000)) return void tooMany(res);
      if (clientLimited(res, "analytics", clientSlug, limits.analyticsBatchesPerMinute, null)) return;
      const rawEvents: any[] = Array.isArray(req.body?.events) ? req.body.events : [];
      const receivedAt = new Date().toISOString();
      const events: AnalyticsEvent[] = [];
      for (const raw of rawEvents.slice(0, MAX_EVENTS_PER_REQUEST)) {
        const name = cleanText(raw?.name, 40);
        if (!EVENT_NAME_PATTERN.test(name)) continue;
        const parsed = typeof raw?.ts === "string" ? Date.parse(raw.ts) : NaN;
        // A kiosk clock can be wrong: keep the event time only when it is within a week of the server's
        const ts = Number.isFinite(parsed) && Math.abs(parsed - Date.now()) < 7 * 86_400_000 ? new Date(parsed).toISOString() : receivedAt;
        events.push({ clientSlug, sessionId, name, params: sanitizeEventParams(raw?.params), ts, receivedAt });
      }
      if (!events.length) return res.status(400).json({ status: "error", error: "No valid events." });
      await store.saveEvents(events, req.body?.session_start === true ? 1 : 0);
      // flush_seconds tells the Unreal app how often to send, so it follows the preset chosen in the portal
      res.json({ status: "ok", accepted: events.length, rejected: rawEvents.length - events.length, flush_seconds: limits.flushSeconds });
    } catch (e: any) {
      console.error("[Insights] Could not save analytics events:", e?.message || e);
      res.status(500).json({ status: "error", error: "The events could not be saved." });
    }
  });

  // Portal dashboard (signed in): the individual events of one kind, with the visitor's name when that visit left a lead
  app.get("/api/analytics/events", async (req, res) => {
    const clientSlug = cleanSlug(req.query.slug);
    if (!clientSlug) return res.status(400).json({ status: "error", error: "slug is required." });
    if (!(await authorize(req, res, clientSlug))) return;
    try {
      const name = cleanText(req.query.name, 40);
      if (!EVENT_NAME_PATTERN.test(name)) return res.status(400).json({ status: "error", error: "name is required." });
      const days = Math.min(Math.max(parseInt(String(req.query.days || "30"), 10) || 30, 1), 365);
      const sinceIso = new Date(Date.now() - days * 86_400_000).toISOString();
      const [events, leads] = await Promise.all([store.listEvents(clientSlug, name, sinceIso, 500), store.listLeads(clientSlug, 500)]);
      const leadBySession = new Map<string, string>();
      for (const lead of leads) if (lead.sessionId && !leadBySession.has(lead.sessionId)) leadBySession.set(lead.sessionId, lead.name);
      res.json({
        status: "ok", client_slug: clientSlug, name, days,
        events: events.map((e) => ({ ts: e.ts, session: e.sessionId.slice(0, 6), visitor: leadBySession.get(e.sessionId) || null, params: e.params })),
      });
    } catch (e: any) {
      console.error("[Insights] Could not list analytics events:", e?.message || e);
      res.status(500).json({ status: "error", error: "The events could not be loaded." });
    }
  });

  // Portal dashboard (signed in): totals per event, per day and per unit
  app.get("/api/analytics/summary", async (req, res) => {
    const clientSlug = cleanSlug(req.query.slug);
    if (!clientSlug) return res.status(400).json({ status: "error", error: "slug is required." });
    if (!(await authorize(req, res, clientSlug))) return;
    try {
      const days = Math.min(Math.max(parseInt(String(req.query.days || "30"), 10) || 30, 1), 365);
      const sinceDay = new Date(Date.now() - (days - 1) * 86_400_000).toISOString().slice(0, 10);
      const daily = (await store.listDaily(clientSlug, sinceDay)).sort((a, b) => a.day.localeCompare(b.day));
      const totals: Record<string, number> = {};
      const units: Record<string, Record<string, number>> = {};
      let sessions = 0;
      for (const day of daily) {
        sessions += day.sessions;
        for (const [name, count] of Object.entries(day.events)) totals[name] = (totals[name] || 0) + count;
        for (const [unit, counts] of Object.entries(day.units)) {
          units[unit] = units[unit] || {};
          for (const [name, count] of Object.entries(counts)) units[unit][name] = (units[unit][name] || 0) + count;
        }
      }
      const topUnits = Object.entries(units)
        .map(([unit, counts]) => ({ unit, counts, total: Object.values(counts).reduce((a, b) => a + b, 0) }))
        .sort((a, b) => b.total - a.total).slice(0, 50);
      res.json({
        status: "ok", client_slug: clientSlug, days, since: sinceDay, sessions, totals, top_units: topUnits,
        by_day: daily.map((d) => ({ day: d.day, sessions: d.sessions, events: d.events })),
      });
    } catch (e: any) {
      console.error("[Insights] Could not build the analytics summary:", e?.message || e);
      res.status(500).json({ status: "error", error: "The summary could not be built." });
    }
  });

  app.get("/api/leads", async (req, res) => {
    const clientSlug = cleanSlug(req.query.slug);
    if (!clientSlug) return res.status(400).json({ status: "error", error: "slug is required." });
    if (!(await authorize(req, res, clientSlug))) return;
    try {
      const leads = await store.listLeads(clientSlug, 500);
      res.json({ status: "ok", client_slug: clientSlug, leads: leads.map((l) => ({ ...l, shortlist_url: l.shortlistId ? `${baseUrl(req)}/s/${l.shortlistId}` : null })) });
    } catch (e: any) {
      console.error("[Insights] Could not list leads:", e?.message || e);
      res.status(500).json({ status: "error", error: "The leads could not be listed." });
    }
  });

  app.post("/api/leads/:id/status", async (req, res) => {
    const clientSlug = cleanSlug(req.body?.slug);
    const status = cleanText(req.body?.status, 20);
    if (!clientSlug || !["new", "contacted", "closed"].includes(status)) return res.status(400).json({ status: "error", error: "slug and a valid status are required." });
    if (!(await authorize(req, res, clientSlug))) return;
    try {
      const updated = await store.updateLeadStatus(req.params.id, clientSlug, status as Lead["status"]);
      if (!updated) return res.status(404).json({ status: "error", error: "Lead not found." });
      res.json({ status: "ok" });
    } catch (e: any) {
      console.error("[Insights] Could not update a lead:", e?.message || e);
      res.status(500).json({ status: "error", error: "The lead could not be updated." });
    }
  });

  // Portal: the traffic preset of one client. Anyone who can open the client's leads sees it; the owner, admins
  // and the client's moderators change it.
  const canManage = (user: PortalUser, clientSlug: string): boolean =>
    user.role === "owner" || user.role === "admin" || (user.role === "moderator" && user.clientIds.includes(clientSlug));
  const presetList = (Object.keys(TRAFFIC_PRESETS) as TrafficPreset[]).map((id) => ({ id, ...TRAFFIC_PRESETS[id] }));

  app.get("/api/traffic-preset", async (req, res) => {
    const clientSlug = cleanSlug(req.query.slug);
    if (!clientSlug) return res.status(400).json({ status: "error", error: "slug is required." });
    if (!admin) return res.json({ status: "ok", preset: await trafficPresetOf(clientSlug), can_edit: process.env.NODE_ENV !== "production" && isLoopback(req), presets: presetList });
    const user = await identify(req, res);
    if (!user) return;
    if (!canManage(user, clientSlug) && !(user.role === "client" && user.clientId === clientSlug)) {
      return res.status(403).json({ status: "error", error: "This account cannot view that client." });
    }
    res.json({ status: "ok", preset: await trafficPresetOf(clientSlug), can_edit: canManage(user, clientSlug), presets: presetList });
  });

  app.put("/api/traffic-preset", async (req, res) => {
    const clientSlug = cleanSlug(req.body?.slug);
    const requested = req.body?.preset;
    if (!clientSlug || typeof requested !== "string" || !Object.prototype.hasOwnProperty.call(TRAFFIC_PRESETS, requested)) return res.status(400).json({ status: "error", error: "slug and a known preset are required." });
    const preset = asTrafficPreset(requested);
    try {
      if (admin) {
        if (!(await checkStaff(req, res, clientSlug))) return;
        if (!(await admin.clientExists(clientSlug))) return res.status(404).json({ status: "error", error: "Unknown client." });
        await admin.setTrafficPreset(clientSlug, preset);
      } else if (!(process.env.NODE_ENV !== "production" && isLoopback(req))) {
        return res.status(503).json({ status: "error", error: "This needs the FIREBASE_SERVICE_ACCOUNT secret on the server." });
      }
      trafficPresets.set(clientSlug, { preset, until: Date.now() + 60_000 });
      res.json({ status: "ok", preset });
    } catch (e: any) {
      console.error("[Insights] Could not save a client's traffic preset:", e?.message || e);
      res.status(500).json({ status: "error", error: "The setting could not be saved." });
    }
  });

  // Portal: the client accounts of one portal, for the people who look after it
  app.get("/api/portal-users", async (req, res) => {
    const clientSlug = String(req.query.slug || "").trim();
    if (!clientSlug) return res.status(400).json({ status: "error", error: "slug is required." });
    if (!(await checkStaff(req, res, clientSlug)) || !admin) return;
    try {
      res.json({ status: "ok", users: await admin.listClientUsers(clientSlug) });
    } catch (e: any) {
      console.error("[Insights] Could not list a client's users:", e?.message || e);
      res.status(500).json({ status: "error", error: "The users could not be loaded." });
    }
  });

  // Portal: removes one client account from a portal. Only client accounts of that portal can be removed here;
  // moderators, admins and the owner are managed in the admin console.
  app.delete("/api/portal-users/:uid", async (req, res) => {
    const clientSlug = String(req.query.slug || "").trim();
    const uid = String(req.params.uid || "").trim();
    if (!clientSlug || !uid || uid.includes("/")) return res.status(400).json({ status: "error", error: "slug and user are required." });
    if (!(await checkStaff(req, res, clientSlug)) || !admin) return;
    try {
      const removed = await admin.deleteClientUser(clientSlug, uid);
      if (!removed) return res.status(404).json({ status: "error", error: "That user is not a client account of this portal." });
      res.json({ status: "ok" });
    } catch (e: any) {
      console.error("[Insights] Could not remove a client's user:", e?.message || e);
      res.status(500).json({ status: "error", error: "The user could not be removed." });
    }
  });

  return { authorize, authorizeWrite, authorizeAdmin, authorizeMember, listModeratorEmails };
}
