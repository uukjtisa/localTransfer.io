# localTransfer.io — Android Host

**Spec only. No code is to be written from this document until it is approved.**

A Kotlin app that makes a phone the *host* of localTransfer.io instead of the PC: it runs the
server, serves the same page to every other device on the Wi-Fi, and stays alive while doing it.

---

## 1. What this is, and what it is not

**It is** a launcher and a terminal wrapped around a background HTTP server.

**It is not** a mobile client, and not a re-implementation of the UI. The phone serves the *same
HTML page* the desktop build serves. Everything a user sees — the share page, the file explorer,
the player, the context menus — is the existing page, unchanged. Other devices browse to the
phone. The phone's own screen only shows:

| Screen | Purpose |
|---|---|
| **Launcher** | Start/stop the server, the access URLs with a QR code, live status, storage figures |
| **Terminal** | The same command set as the desktop REPL (`/help`, `/db`, `/ff`, `/sl`, `/open`, …) |
| **Files** | A real file manager — browse, pick forwarding folders, delete, move things into the saving folder |
| **Upload** | A deliberately minimal picker, for when the app cannot be left |

The fourth item exists for a specific reason: on Android you often *cannot* leave the app to go
find a file in another app, because leaving may get the process killed. So there is a small
built-in picker rather than a full-featured one. It is a safety valve, not a feature to grow.

---

## 2. The page is shared, not rewritten

The desktop page is generated, not hand-written — `build-page.js` transforms the approved mock into
`page.html`, which is then spliced into the C++ raw string literal.

**The Android build consumes the exact same `page.html`.** It is copied into
`app/src/main/assets/page.html` by a Gradle task that runs the same builder. It is never edited by
hand and never forked.

> If the page is ever forked between the two hosts, the project has two UIs to maintain and they
> will drift. This is the single most important constraint in this document.

The page already discovers everything it needs at runtime (`/api/info`, `/api/database`,
`/api/disk_space`, `/events`), so it does not care what is serving it — as long as the Android
server implements the same endpoints with the same shapes.

---

## 3. Endpoint parity

The Android server must implement all of these identically, or the page breaks in ways that are
hard to trace:

| Endpoint | Notes |
|---|---|
| `GET /`, `/share`, `/database` | All three serve `page.html`. `/` redirect is client-side, nothing to do. |
| `GET /api/info` | `ips`, `saving_dir`, **`is_host`** |
| `GET /api/stats` | `files`, `bytes`, `clients` |
| `GET /api/disk_space` | `disk_free`, `disk_total`, `storage_cap`, `max_upload`, `disk_root`, `db_used` |
| `GET /api/database` | `files[]` **newest-first**, `forwarding_folders[]` with the recursive `tree` |
| `POST /api/database/delete` | `{id, delete_file}` |
| `GET /events` | SSE: `db_update`, `pastebin_update`, `:ping` every 20 s |
| `GET/POST /api/pastebin` | |
| `POST /upload` | multipart, plus the `metadata` field carrying `lastModified` |
| `GET /download`, `/download_ff`, `/download_db_zip`, `/download_ff_zip` | |
| `GET /preview_inline`, `/preview_inline_ff` | **Range requests required** — the video player seeks. |
| `GET /thumb`, `/thumb_ff` | |
| `GET /api/archive_browse` | ZIP listing |

**`is_host` on Android is always `false`** for remote browsers, and `true` only for a request from
loopback (the phone's own browser). This is what hides "Open containing folder" — which on Android
means an intent, not Explorer.

### Two rules carried over from the desktop build

1. **Forwarding-folder paths come from the client and must be validated on every endpoint that
   accepts one.** Skipping the check turns the server into an arbitrary-file-read hole. On Android
   this is worse than on Windows, because the process may hold All-Files access.
2. **Filename encoding must round-trip.** The desktop substitutes fullwidth Unicode for
   Windows-illegal characters. Android's filesystem allows most of them, so the Android host should
   store names verbatim — but it must still *decode* incoming names so files transferred from a
   Windows host display correctly.

---

## 4. Staying alive — the actual hard part

Everything else here is ordinary app work. This is the part that decides whether the project is
usable, and it deserves the most care.

### Foreground service

The server runs in a `Service` promoted to foreground with a persistent notification showing the
access URL and a Stop action.

- Android 14+ requires a declared `foregroundServiceType`. Use **`specialUse`** with a documented
  justification (`PROPERTY_SPECIAL_USE_FGS_SUBTYPE`), since this is a user-initiated LAN server and
  fits none of the standard categories cleanly. `dataSync` is the fallback if `specialUse` is
  rejected, but it is capped and can be stopped by the system.
- Permissions: `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_SPECIAL_USE`, `POST_NOTIFICATIONS`.

### Locks

- **`WifiManager.WifiLock`** in `WIFI_MODE_FULL_HIGH_PERF` — without it the Wi-Fi radio powers down
  when the screen is off and transfers stall or the server becomes unreachable.
- **`PowerManager.PARTIAL_WAKE_LOCK`** held *only while a transfer is in flight*, not for the whole
  service lifetime. A permanently held wake lock will drain the battery and is the kind of thing
  that gets an app uninstalled.

### Battery optimisation exemption

Request `ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` and check
`PowerManager.isIgnoringBatteryOptimizations()` on every launch, prompting if it has been revoked.

> Be honest in the UI about why. This permission is restricted on Google Play; a sideloaded build
> is fine, but if this is ever published the listing will need a justification or the feature must
> degrade gracefully.

### The OEM problem

Stock Android respects the above. Xiaomi/MIUI, Huawei/EMUI, Samsung, Oppo and Vivo all ship
additional process killers that ignore it. The app must:

- Detect the manufacturer and, on first run, show a short screen with the exact path to that
  vendor's autostart/battery screen, with a button that fires the vendor intent where one is known.
- Detect that it was killed (service `onDestroy` without a user stop, or a missing heartbeat across
  restarts) and surface it plainly: *"Android stopped the server in the background. Here is how to
  prevent that on your phone."*

Never claim the server will survive; tell the user what the OS did.

---

## 5. Storage and the file manager

Scoped storage makes this the second-hardest part.

- **Saving folder**: default to app-specific external storage (`getExternalFilesDir`) which needs no
  permission at all. Uploads land here and work on a clean install with zero prompts.
- **Anywhere else** (a real file manager, forwarding folders pointing at `DCIM`, `Download`, an SD
  card): requires either
  - **`MANAGE_EXTERNAL_STORAGE`** ("All files access") — one prompt, full `java.io.File` access,
    makes the file manager and forwarding folders straightforward. Play-restricted; fine sideloaded.
    **Recommended**, because a file manager is an explicitly requested feature.
  - or **SAF** (`ACTION_OPEN_DOCUMENT_TREE` + `takePersistableUriPermission`), which avoids the
    restricted permission but forces every path in the codebase to become a `DocumentFile`/tree URI.
    That is a large, invasive change and it makes the forwarding-folder tree slow.

The app should function without either — the built-in saving folder always works — and ask for
All-Files access only when the user first opens Files or adds a forwarding folder.

**`/open` on Android** maps to `Intent.ACTION_VIEW` with a `FileProvider` content URI, and
`ACTION_OPEN_DOCUMENT_TREE` for folders. It stays visible only when `is_host` is true.

---

## 6. Discovery

Use **`NsdManager`** to register `_http._tcp` so the phone appears in Bonjour browsers and on Apple
devices.

Two honest limitations, both measured on the desktop build:

- `NsdManager` registers a *service*, not an arbitrary hostname. It cannot publish
  `localTransfer.io.local` the way the C++ responder does. Expect service discovery to work and a
  typed `.local` hostname not to.
- **Chrome on Android does not resolve `.local` at all.** Other Android phones on the network will
  need the numeric URL regardless.

So the launcher's primary affordance is **the QR code and the numeric URL**, not a hostname.

### Port

**Port 80 is unavailable** — ports below 1024 need root on Android. Default to **8080**, fall back
through the same list the desktop uses. The URL will always carry `:8080`. Do not design any UI
that assumes a bare hostname.

---

## 7. Stack

| Concern | Choice | Why |
|---|---|---|
| HTTP server | **Ktor CIO** embedded | Kotlin-native, coroutine-based; supports SSE, range requests and multipart without fighting it. NanoHTTPD is smaller but range + SSE are hand-rolled. |
| UI | **Compose** | Consistent with the other apps in this workspace. |
| Persistence | `DataStore` for settings, a JSON file for the database index | Mirrors `database.json`; no need for Room at this size. |
| Thumbnails | Android's own decoders | `ThumbnailUtils` / `MediaMetadataRetriever` give image thumbnails, video posters and durations with no ffmpeg dependency. This is *better* than the desktop path — implement `/thumb` natively and skip the ffmpeg question entirely. |
| Terminal | Compose text list + input | Reuse the desktop command grammar verbatim, including `/` and `\` prefixes and the aliases. |

---

## 8. Build phases

1. **Server core** — Ktor, `page.html` from assets, `/api/info`, `/api/stats`, `/api/database`,
   `/upload`, `/download`, SSE. Launcher shows the URL. Nothing else. Proven when a laptop can
   browse the phone and upload a file.
2. **Survival** — foreground service, notification, Wi-Fi lock, battery exemption, OEM guidance,
   kill detection. Proven by an hour with the screen off and a successful transfer at the end.
3. **Storage** — All-Files access, saving-folder picker, forwarding folders with the recursive tree,
   `/download_ff`, `/thumb`.
4. **Parity** — pastebin, ZIP endpoints, archive browse, range requests verified against the real
   video player, `is_host` gating.
5. **Terminal + Files** — the command set and the in-app file manager.
6. **Minimal upload** — the in-app picker.

Phases 1 and 2 are the whole risk. If phase 2 cannot be made reliable on the target phone, the
project should stop there rather than be built on top of a server the OS keeps killing.

---

## 9. Open questions

- **Which phone is the target?** OEM behaviour in §4 decides how much of this works, and the answer
  changes the phase-2 estimate more than anything else in this document.
- **Sideload only, or eventually Play?** Play forces the SAF path in §5 and the loss of the battery
  exemption in §4. Sideload-only makes both easy. Assume sideload unless told otherwise.
- **Should the phone host and the PC host ever run at once?** If yes, the `.local` names and service
  instance names will collide and need distinct instance labels.
