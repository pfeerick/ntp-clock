#!/usr/bin/env bun
/**
 * Zero-dependency dev server for the NTP clock web UI.
 *
 * Lets you iterate on web/pages + web/partials + web/assets in a real
 * browser without flashing hardware. It re-reads and re-composes the
 * page sources on every request (combined with `bun --watch`, editing
 * any file under web/ picks up immediately).
 *
 * This is dev-only tooling. The firmware itself is built from
 * src/generated/webpages.h, produced by scripts/generate_webpages.py
 * (stdlib Python, run automatically by PlatformIO) -- bun is never
 * required to build or flash the firmware.
 *
 * Usage: bun run dev   (== bun --watch web/dev-server.ts), then browse
 * http://localhost:8266
 */

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

const WEB_DIR = import.meta.dir;
const PAGES_DIR = join(WEB_DIR, "pages");
const MOCK_VALUES_PATH = join(WEB_DIR, "mock-values.json");
const PORT = 8266;
const DEFAULT_NTP_SYNC_INTERVAL_SECONDS = 60 * 60 * 8; // matches ntpUpdateInterval in src/globals.h

// --- virtual RTC ---------------------------------------------------------
// Mirrors the firmware's Time library semantics: the clock free-runs off
// the host clock, but /configSave can offset it (like setTime()), and NTP
// sync (on /sync, on "boot", and automatically every ntpSyncIntervalSeconds)
// snaps it back to true time by zeroing the offset. `bootTime` is a
// separate, offset-independent marker used only for the /info uptime
// counter -- restarting the (emulated) device resets it, but setting the
// clock does not.
let offsetMs = 0;
let bootTime = Date.now();

function virtualNow(): Date {
  return new Date(Date.now() + offsetMs);
}

// --- include composition -----------------------------------------------
// Mirrors the logic in scripts/generate_webpages.py: a line that is
// *only* an `<!-- @include path/relative/to/web-root -->` directive is
// replaced with the fully-resolved contents of that file, recursively.
// Include paths are always resolved relative to web/ (WEB_DIR).

const INCLUDE_RE = /^\s*<!--\s*@include\s+(\S+)\s*-->\s*$/;

function resolveIncludes(path: string, stack: string[] = []): string {
  if (stack.includes(path)) {
    const chain = [...stack, path].map((p) => relativeToWeb(p)).join(" -> ");
    throw new Error(`dev-server: circular @include detected: ${chain}`);
  }
  if (!existsSync(path)) {
    const including = stack.length ? relativeToWeb(stack[stack.length - 1]) : "<root>";
    throw new Error(
      `dev-server: @include target not found: ${relativeToWeb(path)} (included from ${including})`
    );
  }

  const text = readFileSync(path, "utf8");
  const lines = text.split("\n").map((line) => {
    const match = line.match(INCLUDE_RE);
    if (!match) return line;
    const includePath = join(WEB_DIR, match[1]);
    return resolveIncludes(includePath, [...stack, path]);
  });
  return lines.join("\n");
}

function relativeToWeb(path: string): string {
  return path.startsWith(WEB_DIR) ? path.slice(WEB_DIR.length + 1) : path;
}

function composePage(name: string): string {
  return resolveIncludes(join(PAGES_DIR, `${name}.html`));
}

// --- placeholder substitution --------------------------------------------
// Plain string replacement of exact %KEY% tokens only -- never regex on
// "%", so `%ESP.getHeapFragmentation%%` (a literal trailing percent
// sign) is left untouched by any other key's replacement.

function replaceToken(html: string, key: string, value: string): string {
  return html.split(`%${key}%`).join(value);
}

function loadMockValues(): Record<string, string | number> {
  return JSON.parse(readFileSync(MOCK_VALUES_PATH, "utf8"));
}

function applyMockValues(html: string): string {
  let out = html;
  for (const [key, value] of Object.entries(loadMockValues())) {
    out = replaceToken(out, key, String(value));
  }
  return out;
}

function deviceName(): string {
  const values = loadMockValues();
  return String(values["DEVICE_NAME"] ?? "ntp-clock-dev");
}

function ntpSyncIntervalSeconds(): number {
  const raw = Number(loadMockValues()["ntpSyncIntervalSeconds"]);
  return Number.isFinite(raw) && raw > 0 ? raw : DEFAULT_NTP_SYNC_INTERVAL_SECONDS;
}

// --- page renderers (mirroring src/webserverHelper.h handlers) ----------

function renderIndex(): string {
  return replaceToken(composePage("index"), "DEVICE_NAME", deviceName());
}

function renderConfig(): string {
  return replaceToken(composePage("config"), "DEVICE_NAME", deviceName());
}

// Same integer math as http_infoPage() in src/webserverHelper.h.
function uptimeFields(uptimeSeconds: number) {
  const systemUpTimeSc = uptimeSeconds % 60;
  const systemUpTimeMn = Math.floor(uptimeSeconds / 60) % 60;
  const systemUpTimeHr = Math.floor(uptimeSeconds / (60 * 60)) % 24;
  const systemUpTimeDy = Math.floor(uptimeSeconds / (60 * 60 * 24));
  return { systemUpTimeDy, systemUpTimeHr, systemUpTimeMn, systemUpTimeSc };
}

function renderInfo(): string {
  // Everything except the uptime fields (including DEVICE_NAME) comes from
  // web/mock-values.json, the single source of truth for dev-server mocks.
  // Uptime is live -- seconds since this dev-server process started --
  // rather than mocked, so it behaves like the real device's counter.
  let html = applyMockValues(composePage("info"));

  const uptimeSeconds = Math.floor((Date.now() - bootTime) / 1000);
  const { systemUpTimeDy, systemUpTimeHr, systemUpTimeMn, systemUpTimeSc } = uptimeFields(uptimeSeconds);
  html = replaceToken(html, "systemUpTimeDy", String(systemUpTimeDy));
  html = replaceToken(html, "systemUpTimeHr", String(systemUpTimeHr));
  html = replaceToken(html, "systemUpTimeMn", String(systemUpTimeMn));
  html = replaceToken(html, "systemUpTimeSc", String(systemUpTimeSc));
  html = replaceToken(html, "uptime", String(uptimeSeconds));
  return html;
}

// Matches the firmware's set-time parsing: sscanf(dateTimeStr,
// "%d-%d-%dT%d:%d:%d", ...) >= 5, i.e. year/month/day/hour/minute
// required, seconds optional.
const SET_TIME_RE = /^(\d+)-(\d+)-(\d+)T(\d+):(\d+)(?::(\d+))?/;

function renderConfigSave(setTimeArg: string | null): string {
  let statusMsg = "";
  if (setTimeArg !== null) {
    const match = setTimeArg.match(SET_TIME_RE);
    if (match) {
      const [, y, mo, d, h, mi, s] = match;
      // datetime-local values are wall-clock/local time, same as the
      // fields the firmware hands to setTime() -- so build a local Date.
      const parsed = new Date(
        Number(y),
        Number(mo) - 1,
        Number(d),
        Number(h),
        Number(mi),
        s ? Number(s) : 0
      );
      const hostNow = Date.now();
      offsetMs = parsed.getTime() - hostNow;
      statusMsg = "Time set!";
      console.log(`[rtc] set-time -> ${parsed.toString()} (offset ${offsetMs}ms)`);
    } else {
      // Invalid input: like the firmware, leave the clock untouched.
      statusMsg = "Error setting time!";
    }
  }
  let html = composePage("config-save");
  html = replaceToken(html, "DEVICE_NAME", deviceName());
  html = replaceToken(html, "STATUS_MSG", statusMsg);
  return html;
}

// --- non-page routes (mirroring firmware plain-text/JSON responses) -----

function renderTimedate(): string {
  const now = virtualNow();
  const hour = now.getHours();
  return JSON.stringify({
    hour,
    minute: now.getMinutes(),
    second: now.getSeconds(),
    isAM: hour < 12 ? 1 : 0,
    day: now.getDate(),
    month: now.getMonth() + 1,
    year: now.getFullYear(),
  });
}

function renderNotFound(req: Request, url: URL): string {
  const params = [...url.searchParams.entries()];
  let message = "File Not Found\n\n";
  message += "URI: " + url.pathname;
  message += "\nMethod: " + req.method;
  message += "\nArguments: " + params.length;
  message += "\n";
  for (const [name, value] of params) {
    message += ` ${name}: ${value}\n`;
  }
  return message;
}

// --- automatic NTP sync ---------------------------------------------------
// Mirrors setSyncInterval(ntpUpdateInterval) in the firmware: every
// ntpSyncIntervalSeconds (web/mock-values.json, default matches the
// firmware's 8-hour ntpUpdateInterval), snap the virtual clock back to
// true time, discarding any manual /configSave offset. The interval is
// re-read from mock-values.json before each reschedule, so dialing it
// down (e.g. to 60) takes effect after the current wait without needing
// a server restart.

function performAutoSync() {
  if (offsetMs !== 0) {
    console.log(`[ntp] auto-sync: correcting clock (was offset by ${offsetMs}ms)`);
  } else {
    console.log("[ntp] auto-sync: clock already correct");
  }
  offsetMs = 0;
}

function scheduleAutoSync() {
  setTimeout(() => {
    performAutoSync();
    scheduleAutoSync();
  }, ntpSyncIntervalSeconds() * 1000);
}

scheduleAutoSync();

// --- server ---------------------------------------------------------------

const html = (body: string, init: ResponseInit = {}) =>
  new Response(body, { headers: { "Content-Type": "text/html" }, ...init });
const text = (body: string, init: ResponseInit = {}) =>
  new Response(body, { headers: { "Content-Type": "text/plain" }, ...init });
const json = (body: string, init: ResponseInit = {}) =>
  new Response(body, { headers: { "Content-Type": "application/json" }, ...init });

Bun.serve({
  port: PORT,
  fetch(req) {
    const url = new URL(req.url);

    try {
      switch (url.pathname) {
        case "/":
          return html(renderIndex());
        case "/sync":
          // NTP sync snaps the clock back to true time, same as the
          // firmware's wifi::setupNTP() call in http_sync().
          offsetMs = 0;
          console.log("[ntp] synced");
          return html(renderIndex());
        case "/info":
          return html(renderInfo());
        case "/config":
          return html(renderConfig());
        case "/configSave":
          return html(renderConfigSave(url.searchParams.get("set-time")));
        case "/getTimedate":
          return json(renderTimedate());
        case "/restart":
          // Emulate a reboot: NTP re-syncs on boot (offset cleared) and
          // uptime restarts from zero.
          offsetMs = 0;
          bootTime = Date.now();
          console.log("[device] restart: clock re-synced, uptime reset");
          return text("Restart!");
        case "/resetWifi":
          return text("Clearing WiFi credentials. You will need to reconfigure AP!");
        default:
          return text(renderNotFound(req, url), { status: 404 });
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      console.error(message);
      return text(message, { status: 500 });
    }
  },
});

console.log(`ntp-clock web dev server running at http://localhost:${PORT}`);
