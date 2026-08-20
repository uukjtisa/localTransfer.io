# localTransfer.io — Android Host

**Status: specification only. No code has been written.**

A phone-side host for localTransfer.io. The phone becomes the server; laptops,
tablets and other phones on the same Wi-Fi open it in a browser exactly as they
open the Windows build today.

---

## 1. What this is, and what it is not

The Android app is a **launcher and a terminal**. It is not a second client and
it must never grow into one.

- The **UI that matters is the served web page** — the same `/share` and
  `/database` documents the desktop build serves. It is not reimplemented in
  Compose. One design, one place to change it.
- The app's own screens exist only to start and stop the server, watch what it
  is doing, and reach the files it is serving.
- A user who wants to browse or upload opens the phone's own browser at
  `http://localhost:<port>/`. That path is the same code every other device runs.

The one deliberate exception is a **minimal in-app upload**, covered in §6.

---

## 2. Screens

Three, and no more.

### 2.1 Launcher (home)

The whole point of the app.

```
┌──────────────────────────────┐
│  [lT.io]              ● LIVE │
├──────────────────────────────┤
│                              │
│      ┌──────────────┐        │
│      │    START     │        │   big, single, unmissable
│      └──────────────┘        │
│                              │
│  http://192.168.1.42:8080/   │   tap to copy · long-press for QR
│  http://localtransfer-io.local/│
│                              │
│  Serving   D:/…/localTransfer│
│  3 devices · 148 MB today    │
├──────────────────────────────┤
│  Launcher │ Terminal │ Files │
└──────────────────────────────┘
```

- One start/stop control. Running state must be obvious from the lock screen too
  (see §5).
- Every reachable URL listed, tappable to copy, long-press for a QR code — a QR
  is the fastest way to get a laptop onto a phone-hosted server.
- Live counters: connected clients, bytes moved, current serving folder.

### 2.2 Terminal

The desktop build's REPL, on the phone. Same command surface: `/help`,
`/status`, `/files`, `/db`, `/ff`, `/sl`, `/pastebin`, `/open`, `/port`.

- Monospace, scrollback, selectable and copyable text.
- The autocomplete from `main.cpp`'s `COMPLETIONS[]` reappears as a horizontal
  chip strip above the soft keyboard — typing `/f` offers `/files`, `/ff --new`,
  and so on. Tapping a chip completes it.
- Log levels keep their colours.
- `/open` behaves differently here: there is no Explorer. `--db` and `--ff`
  open the **Files** screen at that folder; `--web` fires an `ACTION_VIEW`
  intent at `http://localhost:<port>/`.

### 2.3 Files

The app's own file manager, because Android has no dependable system one and
the server needs to be pointed at real directories.

- Browse, sort by name/size/date, search, multi-select.
- Set the **saving directory** (where uploads land).
- Add and remove **forwarding folders**, mirroring `/ff --new` and `/ff --remove`.
- Preview images, video and text inline; share out via `ACTION_SEND`.
- Delete, rename, create folder, copy path.

---

## 3. Storage and permissions

This is where an Android port actually gets hard, and it decides the design.

### 3.1 Which storage model

**Use the Storage Access Framework (SAF) for user-chosen folders, and app-scoped
storage for the default.**

- Default saving directory: `getExternalFilesDir(null)/localTransfer`. No
  permission needed, survives updates, removed on uninstall.
- Any other folder — including every forwarding folder — is chosen through
  `ACTION_OPEN_DOCUMENT_TREE` and persisted with
  `takePersistableUriPermission()`. The app stores the tree URI, not a path.
- Do **not** request `MANAGE_EXTERNAL_STORAGE`. Google Play rejects it for
  anything but file managers and backup tools, and this app would not survive
  review. If a build is ever sideloaded-only, it can be reconsidered — as an
  option, never a requirement.

Consequence to design around: **a SAF tree URI is not a filesystem path.** The
existing C++ walks directories with `FindFirstFileW`. On Android the equivalent
must go through `DocumentFile`/`ContentResolver`. Either the file layer is
abstracted behind an interface with a JNI-backed Android implementation, or the
server is rewritten in Kotlin. See §7.

### 3.2 Permission list

| Permission | Why | When asked |
|---|---|---|
| `INTERNET` | serve HTTP | install time |
| `ACCESS_WIFI_STATE`, `ACCESS_NETWORK_STATE` | show the Wi-Fi address, detect network changes | install time |
| `FOREGROUND_SERVICE` | keep serving with the screen off | install time |
| `FOREGROUND_SERVICE_DATA_SYNC` | required subtype, Android 14+ | install time |
| `POST_NOTIFICATIONS` | the foreground notification is the stop control | first start, Android 13+ |
| `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` | survive Doze during a long transfer | §5, on first start, with an explanation |
| `WAKE_LOCK` | hold the CPU during an active transfer only | install time |

No storage permission is in that list, by design — SAF grants are per-folder and
are not permissions.

---

## 4. The server

### 4.1 Same wire behaviour

Every endpoint the desktop build serves, with the same responses:

`/`, `/share`, `/database`, `/upload`, `/download`, `/download_ff`,
`/download_ff_zip`, `/download_db_zip`, `/preview_inline`, `/preview_inline_ff`,
`/thumb`, `/thumb_ff`, `/preview_clip`, `/events`, and every `/api/*` route.

The HTML is byte-identical to the desktop build's. It ships as an asset,
regenerated by the same generator, so the two never drift.

### 4.2 Android-specific answers

- **`is_host`** is true only for loopback. On a phone the "host" is whoever is
  using the phone, and they reach it through `localhost`. Everything else on the
  Wi-Fi is remote and must not see folder-opening actions.
- **`/open`** maps to in-app navigation, not `ShellExecute`.
- **Thumbnails** do not shell out to ffmpeg. Android has the tools in-platform:
  - images → `ThumbnailUtils.createImageThumbnail`
  - video posters → `MediaMetadataRetriever.getFrameAtTime`
  - durations → `MediaMetadataRetriever.METADATA_KEY_DURATION`
  - Hover-preview clips are **dropped on Android.** Transcoding a clip per video
    on a phone costs battery and heat out of all proportion to the benefit. The
    page already degrades to a poster frame with a duration badge when `clip` is
    false — the desktop and phone builds differ only in that one field.
- The thumbnail cache lives in `context.cacheDir`, so Android can evict it under
  storage pressure without breaking anything. It regenerates on demand.

### 4.3 Discovery

`NsdManager` replaces the hand-rolled mDNS responder — the platform already has
one and there is no reason to ship a second. Register `_http._tcp` with service
name `localTransfer`.

The same caveat as the desktop build applies and should be stated in the UI:
**Chrome on Android does not resolve `.local` names.** The numeric address is
the one that always works, which is exactly why the QR code exists.

---

## 5. Staying alive

The hard problem. A phone will kill a background process mid-transfer, and the
user cannot leave the app open the whole time.

**Foreground service**, started with `startForeground()` within five seconds of
`startForegroundService()`, type `dataSync`:

- Ongoing notification showing the URL, connected clients, and live throughput.
- A **Stop** action on the notification — the primary way to stop the server.
- The notification is not dismissible while serving. That is the platform
  contract for a foreground service and it is also the honest signal that
  something is still running.

**Battery optimisation exemption:**

- Ask on first start, not at install, and only after explaining plainly:
  *"Android may pause the server when the screen is off and interrupt a transfer.
  Allow it to keep running?"*
- Send `ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`.
- **The app must work when the user says no.** Degrade: warn on the launcher
  screen that transfers may pause with the screen off, and suggest keeping the
  app open. Never nag, never block starting the server, never re-prompt more
  than once per install.

**Wake locks:** a `PARTIAL_WAKE_LOCK` held **only while a transfer is in
flight**, released the moment the last one ends. Holding one for the whole
session is what makes an app a battery villain. A `WIFI_MODE_FULL_HIGH_PERF`
Wi-Fi lock follows the same rule.

**Manufacturer reality:** Xiaomi, Samsung, Oppo and Huawei kill background work
regardless of the above. Detect those OEMs on first run and offer a one-tap link
to the relevant autostart settings page, with a short note on what to enable.
This cannot be solved in code; it can only be explained.

---

## 6. In-app upload (minimal, and deliberately so)

> The requirement, in the user's words: *"an upload minimal cause sometimes you
> can't close the app or else it'll die."*

The problem: to upload from the phone hosting the server, you would open the
phone's browser — but leaving the app can get it killed on a hostile OEM. So the
app carries the smallest possible upload of its own.

**In scope:**
- A share target: `ACTION_SEND` and `ACTION_SEND_MULTIPLE` for every MIME type,
  so "Share → localTransfer" from Gallery or Files ingests straight into the
  database. This is the primary path and needs no app switching at all.
- A single **＋** button on the launcher opening the system picker, feeding the
  same ingest path.
- A compact progress row per file, and nothing else.

**Explicitly out of scope:** browsing, previewing, searching, sorting, the
pastebin, and anything else the web UI already does. If the user wants those,
they open the browser. This upload exists so a transfer can start without
leaving the app — not to become a second client.

---

## 7. Porting strategy — the decision to make first

Two routes. This must be settled before any code, because it decides everything.

### Option A — Rewrite the server in Kotlin

- **For:** one language; SAF, `MediaMetadataRetriever` and `NsdManager` are
  native; no NDK; smallest APK; easiest to debug and ship.
- **Against:** the HTTP parsing, multipart upload handling, ZIP writer and SSE
  layer are reimplemented and then maintained twice. Bugs fixed in one do not
  reach the other.
- Ktor or NanoHTTPD as the base, or a hand-rolled `ServerSocket` loop mirroring
  the current structure.

### Option B — Reuse the C++ core through the NDK

- **For:** one implementation of the protocol; behaviour is identical by
  construction.
- **Against:** every Win32 call has to go. `WIN32_LEAN_AND_MEAN`, `winsock2.h`,
  `FindFirstFileW`, `CreateProcessW`, `ShellExecuteW`, `SHGetFolderPathW`,
  `GetDiskFreeSpaceExW` and the console layer are all Windows-only. Directory
  walking cannot be POSIX either, because SAF has no paths — it needs JNI calls
  back into `DocumentFile`. That is a large, awkward abstraction layer, and the
  console REPL would need rewriting regardless.

**Recommendation: Option A.** The genuinely reusable asset is the **HTML page**,
and that is shared either way as a build asset. The C++ that would be preserved
under Option B is the part that is most entangled with Win32 and the part most
in conflict with Android's storage model. Rewriting ~2,000 lines of
straightforward server logic in Kotlin is cheaper than maintaining a JNI-to-SAF
bridge for years.

**Keep in sync across both builds:**
1. The generated HTML page (single source, shared asset).
2. The filename encoding — fullwidth substitutes for `| * ? " < > :`. Three
   implementations already exist (disk, C++ decode, JS decode); Android adds a
   fourth. See `CLAUDE.md`.
3. The `/api/*` response shapes, including `is_host`, `thumb`, `clip` and `dur`.

---

## 8. Module layout

```
android/
  app/
    src/main/
      java/io/localtransfer/
        ui/            launcher, terminal, files  (Compose)
        server/        HTTP, routes, SSE, upload, zip
        storage/       SAF wrapper, saving dir, forwarding folders
        media/         thumbnails, posters, durations
        service/       foreground service, wake locks, notification
        terminal/      command parser, completions, log buffer
        net/           NsdManager registration, address discovery
      assets/
        page.html      generated — never hand-edited
      AndroidManifest.xml
    src/test/          command parser, filename encoding, route table
```

---

## 9. Done when

- Serving from the phone, a laptop loads `/share` and `/database` and both
  behave exactly as against the Windows build.
- Upload from the laptop, download from the laptop, both directions verified.
- A forwarding folder chosen through SAF appears in the explorer with its real
  nested structure.
- The server survives ten minutes with the screen off, with the exemption
  granted; and when it is refused, the app says so plainly instead of failing
  silently.
- Sharing a photo into the app from Gallery ingests it without opening a browser.
- The terminal runs `/status`, `/files`, `/ff --list` and `/open --db`.
- Killing the app from Recents stops the service cleanly and drops the
  notification — no orphaned socket, no zombie port.
