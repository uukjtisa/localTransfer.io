// ================================================================
//  localTransfer.io  –  media.h
//  Optional media assist: downscaled thumbnails, video poster frames,
//  durations, and short hover-preview clips.
//
//  ffmpeg is a SOFT dependency and must stay that way — the whole point
//  of this program is one portable exe. When ffmpeg is absent every
//  entry point below degrades: thumbnails fall back to the original
//  image served inline, durations report 0, and clips are simply never
//  offered, so the page shows a type icon instead.
//
//  Cache layout, next to the exe (state lives on disk, not in memory,
//  so it survives a restart and repairs itself if you delete it):
//      thumbs/<key>.jpg   downscaled thumbnail / video poster frame
//      thumbs/<key>.mp4   muted preview clip
//      thumbs/<key>.dur   duration in whole seconds, as text
// ================================================================
#pragma once
#include "globals.h"

// Probe for ffmpeg/ffprobe and create the cache directory. Call once at start.
void mediaInit();

// True when ffmpeg was found on PATH.
bool mediaHasFfmpeg();

// Cache key for a forwarding-folder file (absolute paths are not filenames).
std::string mediaKeyForPath(const std::string& path);

// Absolute paths into the cache. These do not create anything.
std::string mediaThumbFile(const std::string& key);
std::string mediaClipFile(const std::string& key);

// Present-on-disk checks — cheap, used when building the database JSON.
bool     mediaHasThumb(const std::string& key);
bool     mediaHasClip(const std::string& key);
uint64_t mediaDuration(const std::string& key);   // seconds, 0 when unknown

// Generate on demand. Both return false when ffmpeg is missing, the source
// is unreadable, or generation failed. Safe to call concurrently: work on a
// given key is serialised and a second caller waits for the first.
bool mediaEnsureThumb(const std::string& key, const std::string& src, bool isVideo);
bool mediaEnsureClip (const std::string& key, const std::string& src);

// Background warmer: walks the database and fills in thumbnails and durations
// one file at a time so the UI populates without blocking any request.
void mediaWarmerThread();
