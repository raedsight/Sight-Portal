import { BugNotificationPayload, BugNotificationResult } from "../types";

/**
 * Dispatches an automated bug notification email to the administrator's email address.
 * Automatically triggered when a client reports a bug or updates any existing bug.
 */
export async function notifyAdminBugEvent(payload: BugNotificationPayload): Promise<BugNotificationResult> {
  try {
    const res = await fetch("/api/notify-bug", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(payload),
    });

    if (!res.ok) {
      const errData = await res.json().catch(() => ({}));
      throw new Error(errData.error || `HTTP ${res.status}: Failed to send bug email notification`);
    }

    const data: BugNotificationResult = await res.json();
    return data;
  } catch (err: any) {
    console.warn("[Bug Notification] Could not dispatch email via API:", err.message || err);
    return {
      success: false,
      message: err.message || "Failed to dispatch email",
      recipient: payload.adminEmails?.[0] || "raed.sight@gmail.com",
      subject: `[Bug Notice] #${payload.bug.id.slice(-6)} - ${payload.bug.title}`,
      mode: "logged",
      timestamp: new Date().toISOString(),
    };
  }
}

/**
 * Triggers a test verification email to the admin email address
 */
export async function sendTestBugEmail(targetEmail?: string): Promise<{ success: boolean; message: string; previewUrl?: string }> {
  const res = await fetch("/api/notify-test-email", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ targetEmail }),
  });

  const data = await res.json();
  if (!res.ok) {
    throw new Error(data.error || "Failed to trigger test email");
  }
  return data;
}

/**
 * Fetches recent email dispatch audit history
 */
export async function getNotificationAuditHistory(): Promise<{ history: any[]; defaultAdminEmail: string }> {
  const res = await fetch("/api/notify-audit-history");
  if (!res.ok) return { history: [], defaultAdminEmail: "raed.sight@gmail.com" };
  return res.json();
}

/**
 * Fetches notification configuration metadata
 */
export async function getNotificationConfig(): Promise<{ adminEmail: string; hasSmtp: boolean; hasResend: boolean; mode: string }> {
  const res = await fetch("/api/notify-config");
  if (!res.ok) return { adminEmail: "raed.sight@gmail.com", hasSmtp: false, hasResend: false, mode: "test/ethereal" };
  return res.json();
}
