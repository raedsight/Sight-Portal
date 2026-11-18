import { useEffect, useState } from "react";
import { Trash2, X } from "lucide-react";
import { Client } from "../types";
import { portalJsonHeaders } from "../portalApi";

interface PortalUser {
  uid: string;
  email: string;
}

interface PortalUsersPanelProps {
  client: Client;
  onClose: () => void;
  onRemoved?: (email: string) => void;
}

// The client accounts of one portal, for the moderator who looks after it. Only client accounts are listed
// and can be removed here; the server refuses anything else.
export default function PortalUsersPanel({ client, onClose, onRemoved }: PortalUsersPanelProps) {
  const [users, setUsers] = useState<PortalUser[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [removing, setRemoving] = useState<string | null>(null);

  const load = async () => {
    setLoading(true);
    setError(null);
    try {
      const res = await fetch(`/api/portal-users?slug=${encodeURIComponent(client.id)}`, { headers: await portalJsonHeaders() });
      const body = await res.json().catch(() => null);
      if (!res.ok) throw new Error(body?.error || `The server answered ${res.status}.`);
      setUsers(Array.isArray(body?.users) ? body.users : []);
    } catch (e: any) {
      setError(e?.message || "The users could not be loaded.");
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    load();
  }, [client.id]);

  const remove = async (user: PortalUser) => {
    if (!confirm(`Remove '${user.email || user.uid}' from ${client.name}? They will lose access to this portal.`)) return;
    setRemoving(user.uid);
    setError(null);
    try {
      const res = await fetch(`/api/portal-users/${encodeURIComponent(user.uid)}?slug=${encodeURIComponent(client.id)}`, {
        method: "DELETE",
        headers: await portalJsonHeaders(),
      });
      const body = await res.json().catch(() => null);
      if (!res.ok) throw new Error(body?.error || `The server answered ${res.status}.`);
      setUsers((list) => list.filter((u) => u.uid !== user.uid));
      onRemoved?.(user.email || user.uid);
    } catch (e: any) {
      setError(e?.message || "The user could not be removed.");
    } finally {
      setRemoving(null);
    }
  };

  return (
    <div className="fixed inset-0 z-[70] bg-black/70 flex items-center justify-center p-4" onClick={onClose}>
      <div
        className="w-full max-w-lg glass rounded-2xl border border-[var(--ink-faint)] shadow-2xl p-6"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-start justify-between gap-4 mb-4">
          <div>
            <h2 className="h-display text-base text-white">Portal Users</h2>
            <p className="text-[var(--ink-muted)] text-xs mt-1">Client accounts with access to {client.name}</p>
          </div>
          <button onClick={onClose} title="Close" className="p-1.5 rounded border border-white/10 text-gray-400 hover:text-white cursor-pointer">
            <X className="h-4 w-4" />
          </button>
        </div>

        {error && <div className="mb-3 text-xs text-rose-300 border border-rose-500/30 bg-rose-950/30 rounded-lg px-3 py-2">{error}</div>}

        {loading ? (
          <p className="text-xs text-gray-400 py-6 text-center">Loading users...</p>
        ) : users.length === 0 ? (
          <p className="text-xs text-gray-400 py-6 text-center">No client accounts are assigned to this portal.</p>
        ) : (
          <ul className="divide-y divide-white/5 max-h-[50vh] overflow-y-auto">
            {users.map((user) => (
              <li key={user.uid} className="flex items-center justify-between gap-3 py-3">
                <span className="text-xs text-white truncate" title={user.uid}>{user.email || user.uid}</span>
                <button
                  onClick={() => remove(user)}
                  disabled={removing === user.uid}
                  title="Remove this client account from the portal"
                  className="p-1.5 rounded border border-white/10 bg-white/5 hover:bg-red-950/20 text-red-400 hover:border-red-900/40 cursor-pointer disabled:opacity-40"
                >
                  <Trash2 className="h-3.5 w-3.5" />
                </button>
              </li>
            ))}
          </ul>
        )}
      </div>
    </div>
  );
}
