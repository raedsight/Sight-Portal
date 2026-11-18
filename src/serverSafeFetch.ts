import dns from "dns/promises";
import net from "net";

// Fetching an image from an address that came from outside (a media item's link, an upload by link).
// The server sits inside Google's network, so an address must never be allowed to point back into it:
// only public https addresses are fetched, every redirect is checked again, and only images of a
// sensible size are accepted.

const MAX_IMAGE_BYTES = 25 * 1024 * 1024;
const FETCH_TIMEOUT_MS = 15_000;
const MAX_REDIRECTS = 3;

export class UnsafeUrlError extends Error {}

function isPrivateIPv4(address: string): boolean {
  const p = address.split(".").map((n) => Number(n));
  if (p.length !== 4 || p.some((n) => !Number.isInteger(n) || n < 0 || n > 255)) return true;
  return (
    p[0] === 0 || p[0] === 10 || p[0] === 127 ||
    (p[0] === 100 && p[1] >= 64 && p[1] <= 127) ||   // carrier-grade NAT
    (p[0] === 169 && p[1] === 254) ||                // link-local, cloud metadata
    (p[0] === 172 && p[1] >= 16 && p[1] <= 31) ||
    (p[0] === 192 && p[1] === 168) ||
    (p[0] === 192 && p[1] === 0 && p[2] === 0) ||
    (p[0] === 198 && (p[1] === 18 || p[1] === 19)) ||
    p[0] >= 224                                      // multicast and reserved
  );
}

function isPrivateIPv6(address: string): boolean {
  const a = address.toLowerCase();
  if (a === "::" || a === "::1") return true;
  const mapped = a.match(/^::ffff:(\d+\.\d+\.\d+\.\d+)$/);
  if (mapped) return isPrivateIPv4(mapped[1]);
  if (a.startsWith("::ffff:")) return true;           // mapped address in hex form
  if (a.startsWith("64:ff9b:")) return true;          // NAT64
  const first = parseInt(a.split(":")[0] || "0", 16);
  return (first & 0xfe00) === 0xfc00 ||               // unique local fc00::/7
    (first & 0xffc0) === 0xfe80 ||                    // link-local fe80::/10
    (first & 0xff00) === 0xff00;                      // multicast
}

export function isPrivateAddress(address: string): boolean {
  const kind = net.isIP(address);
  if (kind === 4) return isPrivateIPv4(address);
  if (kind === 6) return isPrivateIPv6(address);
  return true;
}

async function assertPublicHttps(rawUrl: string): Promise<URL> {
  let url: URL;
  try {
    url = new URL(rawUrl);
  } catch {
    throw new UnsafeUrlError("That is not a valid web address.");
  }
  if (url.protocol !== "https:") throw new UnsafeUrlError("Only https addresses can be fetched.");
  if (url.username || url.password) throw new UnsafeUrlError("Addresses with a user name or password are not fetched.");
  if (url.port && url.port !== "443") throw new UnsafeUrlError("Only the standard https port is fetched.");
  const host = url.hostname.replace(/^\[|\]$/g, "");
  if (!host || host === "localhost" || host.endsWith(".localhost") || host.endsWith(".internal") || host.endsWith(".local")) {
    throw new UnsafeUrlError("That address is not on the public internet.");
  }
  const addresses = net.isIP(host) ? [host] : (await dns.lookup(host, { all: true })).map((entry) => entry.address);
  if (addresses.length === 0 || addresses.some((address) => isPrivateAddress(address))) {
    throw new UnsafeUrlError("That address is not on the public internet.");
  }
  return url;
}

export interface FetchedImage {
  buffer: Buffer;
  contentType: string;
}

// Throws UnsafeUrlError when the address may not be fetched, or a plain Error when the source does not answer with an image
export async function fetchPublicImage(rawUrl: string): Promise<FetchedImage> {
  let current = rawUrl;
  for (let hop = 0; hop <= MAX_REDIRECTS; hop++) {
    const url = await assertPublicHttps(current);
    const response = await fetch(url, {
      redirect: "manual",
      signal: AbortSignal.timeout(FETCH_TIMEOUT_MS),
      headers: { Accept: "image/png,image/jpeg,image/*;q=0.8" },
    });
    if (response.status >= 300 && response.status < 400) {
      const location = response.headers.get("location");
      if (!location) throw new Error(`The source redirected without an address (HTTP ${response.status}).`);
      current = new URL(location, url).toString();
      continue;
    }
    const contentType = (response.headers.get("content-type") || "").split(";")[0].trim().toLowerCase();
    if (!response.ok || !contentType.startsWith("image/")) {
      throw new Error(`Could not fetch image from source (HTTP ${response.status}, ${contentType || "no content type"})`);
    }
    if (Number(response.headers.get("content-length") || 0) > MAX_IMAGE_BYTES) throw new Error("The image is too large.");
    const chunks: Buffer[] = [];
    let total = 0;
    if (response.body) {
      for await (const chunk of response.body as any) {
        total += chunk.length;
        if (total > MAX_IMAGE_BYTES) throw new Error("The image is too large.");
        chunks.push(Buffer.from(chunk));
      }
    }
    return { buffer: Buffer.concat(chunks), contentType };
  }
  throw new Error("The source redirected too many times.");
}
