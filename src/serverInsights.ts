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
}

const DISK_PATH = path.join(process.cwd(), "insights_cache.json");

interface DiskState {
  shortlists: Record<string, Shortlist>;
  leads: Record<string, Lead>;
  daily: Record<string, DailyAggregate>;
}

function createDiskStore(): InsightsStore {
  let state: DiskState = { shortlists: {}, leads: {}, daily: {} };
  try {
    if (fs.existsSync(DISK_PATH)) {
      const parsed = JSON.parse(fs.readFileSync(DISK_PATH, "utf-8"));
      state = { shortlists: parsed.shortlists || {}, leads: parsed.leads || {}, daily: parsed.daily || {} };
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
  verifyIdToken: (token: string) => Promise<{ uid: string; email?: string }>;
  getUserProfile: (uid: string) => Promise<{ role?: string; clientId?: string } | null>;
  // True when the portal has a client with this id (the slug used in /ws/<slug>)
  clientExists: (slug: string) => Promise<boolean>;
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
    };
    return {
      store,
      verifyIdToken: async (token) => {
        const decoded = await auth.verifyIdToken(token);
        return { uid: decoded.uid, email: decoded.email };
      },
      getUserProfile: async (uid) => {
        const snap = await db.collection("users").doc(uid).get();
        return snap.exists ? (snap.data() as { role?: string; clientId?: string }) : null;
      },
      clientExists: async (slug) => (await db.collection("clients").doc(slug).get()).exists,
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

export async function registerInsightsRoutes(app: Express, options: InsightsOptions): Promise<void> {
  const admin = await createAdminContext();
  const store = admin?.store || createDiskStore();

  // Simple per-address limiter for the endpoints the Unreal app calls without signing in
  const hits = new Map<string, { count: number; resetAt: number }>();
  const limited = (req: Request, res: Response, bucket: string, max: number, windowMs: number): boolean => {
    const key = `${bucket}:${req.ip || req.socket.remoteAddress || "unknown"}`;
    const now = Date.now();
    const entry = hits.get(key);
    if (!entry || entry.resetAt <= now) {
      hits.set(key, { count: 1, resetAt: now + windowMs });
      if (hits.size > 5000) for (const [k, v] of hits) if (v.resetAt <= now) hits.delete(k);
      return false;
    }
    entry.count += 1;
    if (entry.count > max) {
      res.status(429).json({ status: "error", error: "Too many requests. Try again shortly." });
      return true;
    }
    return false;
  };

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
  const authorize = async (req: Request, res: Response, clientSlug: string): Promise<boolean> => {
    if (!admin) {
      res.status(503).json({ status: "error", error: "Leads and analytics need the FIREBASE_SERVICE_ACCOUNT secret on the server." });
      return false;
    }
    const header = req.headers.authorization || "";
    const token = header.startsWith("Bearer ") ? header.slice(7) : "";
    if (!token) {
      res.status(401).json({ status: "error", error: "Sign in to the portal first." });
      return false;
    }
    try {
      const user = await admin.verifyIdToken(token);
      const isOwner = !!user.email && user.email.toLowerCase() === DEFAULT_ADMIN_EMAIL.toLowerCase();
      const profile = isOwner ? null : await admin.getUserProfile(user.uid);
      const allowed = isOwner || profile?.role === "owner" || profile?.role === "admin" ||
        (profile?.role === "client" && profile?.clientId === clientSlug);
      if (!allowed) res.status(403).json({ status: "error", error: "This account cannot view that client." });
      return allowed;
    } catch {
      res.status(401).json({ status: "error", error: "The sign-in has expired. Sign in again." });
      return false;
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
      if (lead) void sendLeadEmail(lead, url);
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
      res.json({ status: "ok", accepted: events.length, rejected: rawEvents.length - events.length });
    } catch (e: any) {
      console.error("[Insights] Could not save analytics events:", e?.message || e);
      res.status(500).json({ status: "error", error: "The events could not be saved." });
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
}
