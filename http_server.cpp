// ================================================================
//  localTransfer.io  –  http_server.cpp
//  Embedded HTML page, HTTP helpers, file upload/download,
//  multipart parser, client handler, port detection, server thread.
// ================================================================
#include "http_server.h"
#include "utils.h"
#include "database.h"

static const std::string HTML_PAGE = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>localTransfer.io</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
:root{
  --bg:#0d1117;
  --surface:#161b22;
  --surface2:#1c2128;
  --border:#30363d;
  --border2:#3d444d;
  --green:#3fb950;
  --green2:#2ea043;
  --green-dim:#1a4a28;
  --blue:#58a6ff;
  --blue-dim:#1f3a5c;
  --ok:#3fb950;
  --warn:#d29922;
  --err:#f85149;
  --txt:#c9d1d9;
  --txt2:#8b949e;
  --dim:#484f58;
  --dim2:#21262d;
}
*{margin:0;padding:0;box-sizing:border-box;}
html{scroll-behavior:smooth;}
body{background:var(--bg);color:var(--txt);font-family:'Inter',sans-serif;min-height:100vh;overflow-x:hidden;}

/* Subtle grain overlay */
body::before{content:'';position:fixed;inset:0;background-image:url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='0.03'/%3E%3C/svg%3E");pointer-events:none;z-index:9999;opacity:.4;}

/* HEADER */
header{display:flex;align-items:center;justify-content:space-between;padding:14px 20px;border-bottom:1px solid var(--border);background:rgba(12,12,14,.97);backdrop-filter:blur(12px);position:sticky;top:0;z-index:100;}
.logo{font-family:'JetBrains Mono',monospace;font-size:1rem;letter-spacing:.04em;color:var(--txt);}
.logo span{color:var(--green);}
.logo em{color:var(--txt2);font-style:normal;}
.header-right{display:flex;align-items:center;gap:8px;}

/* STATUS PILL */
.pill{display:inline-flex;align-items:center;gap:5px;font-size:.65rem;font-family:'JetBrains Mono',monospace;padding:4px 10px;border-radius:20px;border:1px solid;letter-spacing:.05em;}
.pill.online{border-color:var(--ok);color:var(--ok);background:rgba(76,175,125,.07);}
@keyframes blink{0%,100%{opacity:1;}50%{opacity:.25;}}
.pill.online::before{content:'';width:5px;height:5px;border-radius:50%;background:var(--ok);animation:blink 2s infinite;}

/* HAMBURGER MENU */
.menu-wrap{position:relative;}
.menu-btn{font-family:'JetBrains Mono',monospace;font-size:.75rem;padding:6px 10px;border-radius:6px;border:1px solid var(--border2);color:var(--txt2);background:var(--surface2);cursor:pointer;transition:all .2s;display:flex;align-items:center;gap:6px;}
.menu-btn:hover,.menu-btn.open{border-color:var(--green-dim);color:var(--green);background:rgba(63,185,80,.07);}
.menu-dropdown{position:absolute;top:calc(100% + 6px);right:0;background:var(--surface);border:1px solid var(--border2);border-radius:8px;padding:6px;min-width:160px;z-index:200;display:none;box-shadow:0 8px 32px rgba(0,0,0,.5);}
.menu-dropdown.open{display:block;}
.menu-item{display:flex;align-items:center;gap:8px;padding:8px 10px;border-radius:5px;font-size:.78rem;font-family:'JetBrains Mono',monospace;color:var(--txt2);cursor:pointer;transition:all .15s;border:none;background:none;width:100%;text-align:left;}
.menu-item:hover,.menu-item.active{background:rgba(63,185,80,.1);color:var(--green);}
.menu-item-icon{font-size:.9rem;opacity:.7;}
.menu-sep{height:1px;background:var(--border);margin:4px 0;}

/* INFO BUTTON */
.info-btn{font-family:'JetBrains Mono',monospace;font-size:.7rem;padding:6px 10px;border-radius:6px;border:1px solid var(--border2);color:var(--txt2);background:var(--surface2);cursor:pointer;transition:all .2s;}
.info-btn:hover,.info-btn.active{border-color:var(--blue);color:var(--blue);background:rgba(88,166,255,.07);}

/* INFO PANEL (slides in from right) */
.info-panel{position:fixed;top:0;right:-340px;width:320px;height:100vh;background:var(--surface);border-left:1px solid var(--border2);z-index:300;transition:right .3s ease;display:flex;flex-direction:column;overflow:hidden;}
.info-panel.open{right:0;}
.info-panel-header{padding:16px 18px;border-bottom:1px solid var(--border);display:flex;align-items:center;justify-content:space-between;}
.info-panel-header h3{font-size:.7rem;letter-spacing:.15em;text-transform:uppercase;color:var(--dim);font-family:'JetBrains Mono',monospace;}
.info-close{background:none;border:none;color:var(--dim);font-size:1rem;cursor:pointer;padding:2px 6px;border-radius:4px;}
.info-close:hover{color:var(--txt);}
.info-body{flex:1;overflow-y:auto;padding:16px;}
.info-section{margin-bottom:20px;}
.info-section-title{font-size:.6rem;letter-spacing:.15em;text-transform:uppercase;color:var(--dim);font-family:'JetBrains Mono',monospace;margin-bottom:10px;}
.info-row{display:flex;justify-content:space-between;align-items:center;padding:7px 0;border-bottom:1px solid var(--dim2);}
.info-row:last-child{border-bottom:none;}
.info-label{font-size:.72rem;color:var(--txt2);}
.info-value{font-size:.72rem;font-family:'JetBrains Mono',monospace;color:var(--green);}
.info-value.ok{color:var(--ok);}
.info-value.warn{color:var(--warn);}

/* STORAGE BAR */
.storage-bar-wrap{margin-top:10px;}
.storage-bar-labels{display:flex;justify-content:space-between;font-size:.62rem;font-family:'JetBrains Mono',monospace;color:var(--dim);margin-bottom:6px;}
.storage-bar-track{height:5px;background:var(--dim2);border-radius:3px;overflow:hidden;}
.storage-bar-fill{height:100%;background:linear-gradient(90deg,var(--green2),var(--green));border-radius:3px;transition:width .4s;}
.storage-bar-fill.warn{background:linear-gradient(90deg,var(--warn),#e8a060);}
.storage-bar-fill.full{background:linear-gradient(90deg,var(--err),#d46070);}

/* SPEED METERS */
.speed-row{display:flex;gap:10px;margin-top:8px;}
.speed-card{flex:1;background:var(--dim2);border-radius:6px;padding:10px 12px;}
.speed-card-label{font-size:.58rem;letter-spacing:.1em;text-transform:uppercase;color:var(--dim);font-family:'JetBrains Mono',monospace;margin-bottom:4px;}
.speed-card-val{font-size:.85rem;font-family:'JetBrains Mono',monospace;color:var(--green);}

/* STATS BAR */
.statsbar{display:flex;border-bottom:1px solid var(--border);background:var(--surface);}
.stat{flex:1;padding:10px 16px;border-right:1px solid var(--border);}
.stat:last-child{border-right:none;}
.stat label{color:var(--dim);display:block;font-size:.55rem;letter-spacing:.12em;text-transform:uppercase;font-family:'JetBrains Mono',monospace;margin-bottom:3px;}
.stat value{color:var(--green);font-size:.82rem;font-family:'JetBrains Mono',monospace;}

/* LAYOUT */
.layout{display:flex;min-height:calc(100vh - 108px);}
main{flex:1;min-width:0;padding:28px 24px 80px;}

/* SIDEBAR */
.sidebar{width:0;overflow:hidden;border-left:0px solid var(--border);transition:width .3s ease,border-width .3s;background:var(--surface);display:flex;flex-direction:column;}
.sidebar.open{width:360px;border-left:1px solid var(--border);}
.sb-inner{width:360px;padding:16px 14px;flex:1;overflow-y:auto;display:flex;flex-direction:column;}
.sb-panel-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px;}
.sb-panel-header h3{margin-bottom:0;}
.sb-close-btn{font-family:'JetBrains Mono',monospace;font-size:.6rem;padding:4px 10px;border-radius:5px;border:1px solid var(--border2);color:var(--txt2);background:transparent;cursor:pointer;transition:all .15s;white-space:nowrap;}
.sb-close-btn:hover{border-color:var(--err);color:var(--err);}
.sb-inner h3{font-family:'JetBrains Mono',monospace;font-size:.6rem;letter-spacing:.18em;text-transform:uppercase;color:var(--dim);}

/* DB ITEMS */
.db-item{padding:11px 13px;border:1px solid var(--border);border-radius:6px;margin-bottom:7px;background:var(--bg);position:relative;overflow:hidden;animation:slidein .2s ease both;transition:border-color .2s;}
.db-item:hover{border-color:var(--green-dim);}
.db-item::before{content:'';position:absolute;left:0;top:0;bottom:0;width:2px;background:linear-gradient(to bottom,var(--green2),var(--green));}
.db-item-name{font-size:.82rem;font-weight:600;color:var(--txt);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-bottom:3px;}
.db-item-meta{font-family:'JetBrains Mono',monospace;font-size:.6rem;color:var(--dim);margin-bottom:8px;}
.db-item-actions{display:flex;gap:5px;}
.db-btn{font-family:'JetBrains Mono',monospace;font-size:.58rem;padding:3px 9px;border-radius:4px;cursor:pointer;border:1px solid;transition:all .15s;text-decoration:none;display:inline-block;background:transparent;}
.db-btn-dl{border-color:var(--green-dim);color:var(--green2);}
.db-btn-dl:hover{border-color:var(--green);color:var(--green);background:rgba(63,185,80,.08);}
.db-btn-del{border-color:#3a2030;color:var(--err);}
.db-btn-del:hover{background:rgba(201,76,92,.08);border-color:var(--err);}
.sb-empty{font-family:'JetBrains Mono',monospace;font-size:.7rem;color:var(--dim);text-align:center;padding:40px 0;}

/* FF SECTION */
.ff-section{margin-top:16px;}
.ff-section-header{font-family:'JetBrains Mono',monospace;font-size:.58rem;letter-spacing:.15em;text-transform:uppercase;color:var(--blue);margin-bottom:10px;padding-bottom:6px;border-bottom:1px solid var(--border);}
.ff-folder{margin-bottom:10px;}

/* FF TREE */
.ff-file{display:flex;align-items:center;justify-content:space-between;padding:5px 8px;border-bottom:1px solid var(--dim2);font-size:.72rem;}
.ff-file:last-child{border-bottom:none;}
.ff-file-name{color:var(--txt2);flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.ff-file-actions{display:flex;gap:4px;flex-shrink:0;}
.ff-dl-btn{font-family:'JetBrains Mono',monospace;font-size:.55rem;padding:2px 7px;border-radius:3px;border:1px solid var(--blue);color:var(--blue);text-decoration:none;transition:all .15s;cursor:pointer;background:none;}
.ff-dl-btn:hover{background:rgba(88,166,255,.12);}
.ff-view-btn{font-family:'JetBrains Mono',monospace;font-size:.55rem;padding:2px 7px;border-radius:3px;border:1px solid var(--border2);color:var(--txt2);cursor:pointer;background:none;transition:all .15s;}
.ff-view-btn:hover{border-color:var(--green-dim);color:var(--green);}
.ff-size{font-family:'JetBrains Mono',monospace;font-size:.55rem;color:var(--dim);margin-right:6px;white-space:nowrap;}

/* FF tree indentation */
.ff-tree-dir{margin-bottom:2px;}
.ff-tree-dir-hdr{display:flex;align-items:center;gap:6px;padding:4px 8px;cursor:pointer;border-radius:4px;font-size:.72rem;color:var(--blue);background:rgba(88,166,255,.05);border-left:2px solid var(--blue);}
.ff-tree-dir-hdr:hover{background:rgba(88,166,255,.12);}
.ff-tree-dir-hdr .tree-toggle{font-size:.65rem;transition:transform .15s;flex-shrink:0;}
.ff-tree-dir-hdr.collapsed .tree-toggle{transform:rotate(-90deg);}
.ff-tree-dir-hdr .dir-dl-btn{font-family:'JetBrains Mono',monospace;font-size:.52rem;padding:2px 6px;border-radius:3px;border:1px solid var(--blue);color:var(--blue);background:none;cursor:pointer;margin-left:auto;transition:all .15s;}
.ff-tree-dir-hdr .dir-dl-btn:hover{background:rgba(88,166,255,.15);}
.ff-tree-children{margin-left:14px;border-left:1px solid var(--border);padding-left:4px;}
.ff-tree-dir.collapsed > .ff-tree-children{display:none;}

/* Thumbnail in list */
.file-thumb{width:36px;height:36px;border-radius:4px;object-fit:cover;flex-shrink:0;border:1px solid var(--border);}
.thumb-placeholder{width:36px;height:36px;border-radius:4px;display:flex;align-items:center;justify-content:center;background:var(--surface2);font-size:1.2rem;flex-shrink:0;border:1px solid var(--border);}

/* DB TOOLBAR (search/filter/sort/view) */
.db-toolbar{display:flex;flex-direction:column;gap:6px;margin-bottom:10px;}
.db-search-row{display:flex;gap:6px;}
.db-search{flex:1;background:var(--bg);border:1px solid var(--border);border-radius:5px;color:var(--txt);font-family:'JetBrains Mono',monospace;font-size:.72rem;padding:5px 9px;outline:none;transition:border-color .2s;}
.db-search:focus{border-color:var(--green-dim);}
.db-controls-row{display:flex;gap:5px;flex-wrap:wrap;}
.db-ctl{font-family:'JetBrains Mono',monospace;font-size:.6rem;padding:3px 8px;border-radius:4px;border:1px solid var(--border2);color:var(--txt2);background:var(--bg);cursor:pointer;transition:all .15s;}
.db-ctl:hover,.db-ctl.active{border-color:var(--green-dim);color:var(--green);}
.db-ctl select{background:transparent;border:none;color:inherit;font:inherit;cursor:pointer;outline:none;}

/* COMPACT mode */
.db-item.compact{padding:5px 10px;margin-bottom:3px;}
.db-item.compact .db-item-meta{margin-bottom:4px;}
.db-item.compact .db-item-name{font-size:.75rem;}

/* DB item view button */
.db-btn-view{border-color:#1f3a5c;color:#4a8abf;}
.db-btn-view:hover{border-color:var(--blue);color:var(--blue);background:rgba(88,166,255,.08);}

/* ── PREVIEW MODAL ── */
.preview-overlay{position:fixed;inset:0;background:rgba(0,0,0,.85);z-index:1000;display:flex;align-items:center;justify-content:center;backdrop-filter:blur(4px);}
.preview-overlay.hidden{display:none;}
.preview-modal{background:var(--surface);border:1px solid var(--border2);border-radius:10px;max-width:92vw;max-height:92vh;display:flex;flex-direction:column;overflow:hidden;min-width:320px;}
.preview-hdr{display:flex;align-items:center;justify-content:space-between;padding:12px 16px;border-bottom:1px solid var(--border);flex-shrink:0;}
.preview-hdr-name{font-size:.8rem;font-weight:600;color:var(--txt);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:60vw;}
.preview-close{background:none;border:none;color:var(--dim);font-size:1.1rem;cursor:pointer;padding:2px 8px;border-radius:4px;transition:all .15s;}
.preview-close:hover{color:var(--err);background:rgba(248,81,73,.1);}
.preview-body{flex:1;overflow:auto;display:flex;align-items:center;justify-content:center;padding:12px;min-height:200px;}
.preview-img{max-width:100%;max-height:70vh;border-radius:6px;object-fit:contain;}
.preview-video{max-width:100%;max-height:70vh;border-radius:6px;}
.preview-audio-wrap{padding:20px;text-align:center;}
.preview-audio{width:100%;max-width:400px;}
.preview-text{white-space:pre-wrap;font-family:'JetBrains Mono',monospace;font-size:.75rem;color:var(--txt);background:var(--bg);padding:14px;border-radius:6px;overflow:auto;max-height:65vh;width:100%;line-height:1.6;}
.preview-archive{width:100%;max-height:65vh;overflow:auto;}
.preview-archive-item{display:flex;align-items:center;gap:8px;padding:5px 8px;border-bottom:1px solid var(--dim2);font-size:.72rem;font-family:'JetBrains Mono',monospace;}
.preview-archive-item:hover{background:var(--surface2);}
.preview-footer{padding:10px 16px;border-top:1px solid var(--border);display:flex;gap:8px;align-items:center;flex-wrap:wrap;flex-shrink:0;}
.preview-foot-btn{font-family:'JetBrains Mono',monospace;font-size:.62rem;padding:4px 10px;border-radius:4px;cursor:pointer;border:1px solid var(--border2);color:var(--txt2);background:transparent;transition:all .15s;}
.preview-foot-btn:hover{border-color:var(--green-dim);color:var(--green);}
.preview-foot-btn.active{border-color:var(--green);color:var(--green);}
.preview-unsupported{text-align:center;padding:30px;color:var(--txt2);}
.preview-unsupported p{font-size:.8rem;margin-bottom:12px;}

/* Archive password prompt */
.arch-pw-row{display:flex;gap:6px;margin-bottom:10px;}
.arch-pw-input{flex:1;background:var(--bg);border:1px solid var(--border);border-radius:5px;color:var(--txt);font-family:'JetBrains Mono',monospace;font-size:.72rem;padding:5px 9px;outline:none;}
.arch-pw-input:focus{border-color:var(--blue);}


/* PASTEBIN */
.paste-area{flex:1;display:flex;flex-direction:column;gap:8px;}
.paste-textarea{flex:1;min-height:340px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--txt);font-family:'JetBrains Mono',monospace;font-size:.78rem;padding:12px;resize:vertical;outline:none;line-height:1.6;transition:border-color .2s;}
.paste-textarea:focus{border-color:var(--green-dim);}
.paste-meta{font-family:'JetBrains Mono',monospace;font-size:.6rem;color:var(--dim);}
.paste-synced{color:var(--ok);}
.paste-syncing{color:var(--warn);}
.paste-actions{display:flex;gap:6px;flex-wrap:wrap;}
.paste-act-btn{font-family:'JetBrains Mono',monospace;font-size:.6rem;padding:4px 10px;border-radius:4px;cursor:pointer;border:1px solid var(--border2);color:var(--dim);background:transparent;transition:all .15s;}
.paste-act-btn:hover{border-color:var(--green-dim);color:var(--green);}

/* DROP ZONE */
h2{font-size:.6rem;letter-spacing:.18em;text-transform:uppercase;color:var(--dim);margin-bottom:14px;font-family:'JetBrains Mono',monospace;}
.dropzone{border:1px solid var(--border);border-radius:10px;padding:40px 28px;text-align:center;cursor:pointer;transition:all .25s;background:var(--surface);position:relative;overflow:hidden;}
.dropzone::after{content:'';position:absolute;inset:0;background:radial-gradient(ellipse at center,rgba(201,168,76,.04) 0%,transparent 70%);pointer-events:none;}
.dropzone:hover,.dropzone.over{border-color:var(--green-dim);box-shadow:0 0 0 1px rgba(63,185,80,.2),inset 0 0 40px rgba(63,185,80,.03);}
.drop-glyph{font-size:2rem;margin-bottom:12px;filter:drop-shadow(0 0 12px rgba(63,185,80,.35));}
.drop-main{font-size:1.1rem;font-weight:600;color:var(--txt);margin-bottom:6px;}
.drop-sub{font-size:.75rem;color:var(--txt2);font-family:'JetBrains Mono',monospace;}

/* ACTIONS */
.actions{display:flex;gap:9px;margin-top:14px;justify-content:center;}
.btn{padding:9px 24px;border-radius:7px;font-family:'Inter',sans-serif;font-weight:600;font-size:.82rem;letter-spacing:.04em;cursor:pointer;transition:all .2s;border:none;}
.btn-primary{background:var(--green);color:#0d1117;}
.btn-primary:hover{background:var(--green2);box-shadow:0 4px 20px rgba(63,185,80,.25);}
#fileInput{display:none;}

/* PROGRESS */
.prog-wrap{margin-top:16px;display:none;}
.prog-header{display:flex;justify-content:space-between;font-family:'JetBrains Mono',monospace;font-size:.65rem;color:var(--dim);margin-bottom:6px;}
.prog-track{height:3px;background:var(--border);border-radius:2px;overflow:hidden;}
.prog-bar{height:100%;width:0%;background:linear-gradient(90deg,var(--green2),var(--green));transition:width .2s;}
.prog-status{font-family:'JetBrains Mono',monospace;font-size:.65rem;color:var(--green);margin-top:5px;text-align:right;}

/* STORAGE STATUS WIDGET */
.storage-widget{margin-bottom:22px;padding:14px 16px;background:var(--surface);border:1px solid var(--border);border-radius:8px;}
.storage-widget-title{font-family:'JetBrains Mono',monospace;font-size:.58rem;letter-spacing:.15em;text-transform:uppercase;color:var(--dim);margin-bottom:10px;}
.storage-nums{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:6px;}
.storage-free{font-family:'JetBrains Mono',monospace;font-size:.8rem;color:var(--green);}
.storage-total{font-family:'JetBrains Mono',monospace;font-size:.65rem;color:var(--dim);}

/* SENT FILE LIST */
.file-list{margin-top:28px;}
@keyframes slidein{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:none;}}
.file-item{display:flex;align-items:center;gap:11px;padding:11px 14px;border:1px solid var(--border);border-radius:7px;margin-bottom:7px;background:var(--surface);animation:slidein .25s ease both;position:relative;overflow:hidden;transition:border-color .2s;}
.file-item:hover{border-color:var(--green-dim);}
.file-item::before{content:'';position:absolute;left:0;top:0;bottom:0;width:2px;background:linear-gradient(to bottom,var(--green2),var(--green));}
.ficon{font-size:1.25rem;}
.fmeta{flex:1;min-width:0;}
.fname{font-size:.85rem;font-weight:600;color:var(--txt);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.fdetail{font-family:'JetBrains Mono',monospace;font-size:.6rem;color:var(--dim);margin-top:2px;}
.fbadge{font-family:'JetBrains Mono',monospace;font-size:.58rem;padding:2px 8px;border:1px solid var(--ok);color:var(--ok);border-radius:10px;flex-shrink:0;background:rgba(76,175,125,.07);}

/* TOAST */
.toast{position:fixed;bottom:24px;right:24px;padding:10px 18px;border-radius:7px;font-family:'JetBrains Mono',monospace;font-size:.72rem;pointer-events:none;opacity:0;transition:opacity .3s;z-index:500;border:1px solid;max-width:320px;}
.toast.ok{background:rgba(12,30,20,.97);border-color:var(--ok);color:var(--ok);}
.toast.err{background:rgba(30,12,16,.97);border-color:var(--err);color:var(--err);}
.toast.show{opacity:1;}

/* URLS */
.url-box{margin-bottom:20px;padding:13px 16px;background:var(--surface);border:1px solid var(--border);border-radius:8px;}
.url-item{font-family:'JetBrains Mono',monospace;font-size:.76rem;color:var(--green);padding:2px 0;}
.url-item::before{content:'› ';color:var(--dim);}

/* MOBILE */
@media(max-width:700px){
  header{padding:10px 14px;}
  .logo{font-size:.85rem;}
  .statsbar{flex-wrap:wrap;}
  .stat{min-width:50%;border-right:none;border-bottom:1px solid var(--border);}
  main{padding:18px 14px 60px;}
  .sidebar.open{width:100%;position:fixed;inset:0;z-index:200;overflow-y:auto;}
  .sb-inner{width:100%;}
  .dropzone{padding:28px 16px;}
  .info-panel{width:100%;right:-100%;}
  .info-panel.open{right:0;}
  .storage-widget .storage-nums{flex-direction:column;gap:2px;}
}
@media(max-width:420px){
  .stat{min-width:100%;}
  .actions{flex-direction:column;align-items:stretch;}
  .btn{width:100%;}
}
</style>
</head>
<body>

<header>
  <div class="logo">local<span>Transfer</span><em>.io</em></div>
  <div class="header-right">
    <button class="info-btn" id="infoBtn" onclick="toggleInfo()">⊙ Info</button>
    <div class="menu-wrap">
      <button class="menu-btn" id="menuBtn" onclick="toggleMenu()">☰ <span id="menuLabel">Panels</span></button>
      <div class="menu-dropdown" id="menuDropdown">
        <button class="menu-item" id="menuDb" onclick="openPanel('db')"><span class="menu-item-icon">⬡</span> Database</button>
        <button class="menu-item" id="menuPaste" onclick="openPanel('paste')"><span class="menu-item-icon">⌨</span> Pastebin</button>
      </div>
    </div>
    <div class="pill online">● ONLINE</div>
  </div>
</header>

<div class="statsbar">
  <div class="stat"><label>Active Port</label><value id="sPort">—</value></div>
  <div class="stat"><label>Files Sent</label><value id="sFiles">0</value></div>
  <div class="stat"><label>Data Transferred</label><value id="sBytes">0 B</value></div>
  <div class="stat"><label>Connected</label><value id="sClients">0</value></div>
</div>

<!-- INFO PANEL -->
<div class="info-panel" id="infoPanel">
  <div class="info-panel-header">
    <h3>Connection & Storage</h3>
    <button class="info-close" onclick="toggleInfo()">✕</button>
  </div>
  <div class="info-body">
    <div class="info-section">
      <div class="info-section-title">Storage</div>
      <div class="storage-bar-wrap">
        <div class="storage-bar-labels">
          <span id="iUsed">—</span>
          <span id="iTotal">—</span>
        </div>
        <div class="storage-bar-track"><div class="storage-bar-fill" id="iStorBar" style="width:0%"></div></div>
      </div>
      <div class="info-row" style="margin-top:10px">
        <span class="info-label">Free on disk</span>
        <span class="info-value" id="iFree">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">App storage cap</span>
        <span class="info-value" id="iCap">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">Max upload</span>
        <span class="info-value ok" id="iMaxUp">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">Disk root</span>
        <span class="info-value" id="iDiskRoot">—</span>
      </div>
    </div>
    <div class="info-section">
      <div class="info-section-title">Transfer Speed (live)</div>
      <div class="speed-row">
        <div class="speed-card">
          <div class="speed-card-label">↑ Upload</div>
          <div class="speed-card-val" id="iUpSpeed">—</div>
        </div>
        <div class="speed-card">
          <div class="speed-card-label">↓ Receive</div>
          <div class="speed-card-val" id="iDlSpeed">—</div>
        </div>
      </div>
    </div>
    <div class="info-section">
      <div class="info-section-title">Server</div>
      <div class="info-row">
        <span class="info-label">Port</span>
        <span class="info-value" id="iPort">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">Active clients</span>
        <span class="info-value" id="iClients">—</span>
      </div>
    </div>
  </div>
</div>

<div class="layout">
<main>
  <div class="url-box">
    <h2 style="margin-bottom:8px">Access from any device on this network</h2>
    <div id="urlList">Loading…</div>
  </div>

  <!-- Storage widget -->
  <div class="storage-widget" id="storageWidget">
    <div class="storage-widget-title">Storage</div>
    <div class="storage-nums">
      <span class="storage-free" id="swFree">—</span>
      <span class="storage-total" id="swTotal">—</span>
    </div>
    <div class="storage-bar-track"><div class="storage-bar-fill" id="swBar" style="width:0%"></div></div>
  </div>

  <h2>Upload Files</h2>
  <div class="dropzone" id="dropZone">
    <div class="drop-glyph">⬆</div>
    <div class="drop-main">Drop files here</div>
    <div class="drop-sub" id="dropSub">Any type · Saved to Desktop</div>
  </div>
  <div class="actions">
    <label for="fileInput" class="btn btn-primary">Choose Files</label>
    <input type="file" id="fileInput" multiple>
  </div>
  <div class="prog-wrap" id="progWrap">
    <div class="prog-header"><span id="progLabel">Uploading…</span><span id="progPct">0%</span></div>
    <div class="prog-track"><div class="prog-bar" id="progBar"></div></div>
    <div class="prog-status" id="progStatus">—</div>
  </div>
  <div class="file-list">
    <h2>Sent Files</h2>
    <div id="fileItems"></div>
  </div>
</main>

<aside class="sidebar" id="sidebar">
  <!-- DATABASE PANEL -->
  <div class="sb-inner" id="panelDb" style="display:none">
    <div class="sb-panel-header">
      <h3>Database</h3>
      <button class="sb-close-btn" onclick="closePanel()">✕ Close</button>
    </div>
    <!-- Toolbar -->
    <div class="db-toolbar">
      <div class="db-search-row">
        <input class="db-search" id="dbSearch" type="text" placeholder="🔍 Search files…" oninput="applyDbFilters()">
      </div>
      <div class="db-controls-row">
        <button class="db-ctl" id="ctlCompact" onclick="toggleCompact()" title="Toggle compact view">⊟ Compact</button>
        <button class="db-ctl" onclick="applyDbFilters()">
          <select id="dbSortSel" onchange="applyDbFilters()" style="background:transparent;border:none;color:inherit;font:inherit;cursor:pointer;outline:none;">
            <option value="time_desc">↓ Newest</option>
            <option value="time_asc">↑ Oldest</option>
            <option value="size_desc">↓ Largest</option>
            <option value="size_asc">↑ Smallest</option>
            <option value="name_asc">A→Z</option>
            <option value="name_desc">Z→A</option>
          </select>
        </button>
        <button class="db-ctl" onclick="applyDbFilters()">
          <select id="dbTypeSel" onchange="applyDbFilters()" style="background:transparent;border:none;color:inherit;font:inherit;cursor:pointer;outline:none;">
            <option value="">All Types</option>
            <option value="image">🖼 Images</option>
            <option value="video">🎬 Video</option>
            <option value="audio">🎵 Audio</option>
            <option value="text">📝 Text</option>
            <option value="archive">🗜 Archives</option>
            <option value="other">📁 Other</option>
          </select>
        </button>
      </div>
    </div>
    <div id="dbItems"><div class="sb-empty">Loading…</div></div>
    <div class="ff-section" id="ffSection" style="display:none">
      <div class="ff-section-header">⟳ Forwarding Folders</div>
      <div id="ffItems"></div>
    </div>
  </div>
  <!-- PASTEBIN PANEL -->
  <div class="sb-inner" id="panelPaste" style="display:none">
    <div class="sb-panel-header">
      <h3>Pastebin <span id="pasteSyncLabel" class="paste-synced" style="margin-left:8px;font-size:.6rem">● synced</span></h3>
      <button class="sb-close-btn" onclick="closePanel()">✕ Close</button>
    </div>
    <div class="paste-area">
      <textarea class="paste-textarea" id="pasteTA" placeholder="Type here — live across all connected devices…" spellcheck="false"></textarea>
      <div class="paste-meta" id="pasteCharCount">0 chars</div>
      <div class="paste-actions">
        <button class="paste-act-btn" onclick="pasteClear()">⌧ clear</button>
        <button class="paste-act-btn" onclick="pasteCopyLocal()">⎘ copy</button>
      </div>
    </div>
  </div>
</aside>
</div>

<!-- PREVIEW MODAL -->
<div class="preview-overlay hidden" id="previewOverlay" onclick="closePreview(event)">
  <div class="preview-modal" onclick="event.stopPropagation()">
    <div class="preview-hdr">
      <span class="preview-hdr-name" id="previewTitle">File Preview</span>
      <div style="display:flex;gap:8px;align-items:center">
        <a id="previewDlBtn" class="preview-foot-btn" style="text-decoration:none" href="#" download>↓ Download</a>
        <button class="preview-close" onclick="closePreview()">✕</button>
      </div>
    </div>
    <div class="preview-body" id="previewBody">
      <div class="preview-unsupported"><p>Loading…</p></div>
    </div>
    <div class="preview-footer" id="previewFooter">
      <span id="previewInfo" style="font-family:'JetBrains Mono',monospace;font-size:.6rem;color:var(--dim);flex:1"></span>
      <button class="preview-foot-btn" id="btnViewAsText" onclick="previewAsText()" title="Force text view">📄 View as Text</button>
      <button class="preview-foot-btn" id="btnShowFull" onclick="previewShowFull()" style="display:none">Show Full Text</button>
    </div>
  </div>
</div>
<div class="toast" id="toast"></div>

<script>
const port = location.port || '80';
document.getElementById('sPort').textContent = port;
document.getElementById('iPort').textContent  = port;

// ── CLOSE MENU ON OUTSIDE CLICK ──
document.addEventListener('click', e => {
  const wrap = document.getElementById('menuBtn').closest('.menu-wrap');
  if (!wrap.contains(e.target)) document.getElementById('menuDropdown').classList.remove('open');
});
function toggleMenu(){
  document.getElementById('menuDropdown').classList.toggle('open');
  document.getElementById('menuBtn').classList.toggle('open');
}

// ── INFO PANEL ──
function toggleInfo(){
  const p = document.getElementById('infoPanel');
  const b = document.getElementById('infoBtn');
  const open = p.classList.toggle('open');
  b.classList.toggle('active', open);
  if (open) loadDiskSpace();
}
let diskPollInterval = null;
function loadDiskSpace(){
  fetch('/api/disk_space').then(r=>r.json()).then(d=>{
    const freeB  = Number(d.disk_free);
    const totB   = Number(d.disk_total);
    const capB   = Number(d.storage_cap);
    const maxUp  = Number(d.max_upload);
    const dbUsed = Number(d.db_used || 0);  // actual sum of database file sizes

    // Pool = cap if set, otherwise total disk
    const pool = capB > 0 ? capB : totB;
    // "used" = db_used (app files only), shown against the pool
    const pct  = pool > 0 ? Math.min(100, dbUsed / pool * 100) : 0;

    document.getElementById('iFree').textContent    = fmtBytes(freeB);
    document.getElementById('iCap').textContent     = capB > 0 ? fmtBytes(capB) : 'None';
    document.getElementById('iMaxUp').textContent   = fmtBytes(maxUp);
    document.getElementById('iDiskRoot').textContent= d.disk_root || '—';
    document.getElementById('iClients').textContent = document.getElementById('sClients').textContent;

    const bar = document.getElementById('iStorBar');
    bar.style.width = pct.toFixed(1) + '%';
    bar.className = 'storage-bar-fill' + (pct>90?' full':pct>70?' warn':'');

    document.getElementById('iUsed').textContent  = fmtBytes(dbUsed) + ' used';
    document.getElementById('iTotal').textContent = fmtBytes(pool);

    // Main storage widget
    document.getElementById('swFree').textContent  = fmtBytes(freeB) + ' free';
    document.getElementById('swTotal').textContent = capB > 0 ? ('Cap: '+fmtBytes(capB)) : fmtBytes(totB);
    const swBar = document.getElementById('swBar');
    swBar.style.width = pct.toFixed(1) + '%';
    swBar.className = 'storage-bar-fill' + (pct>90?' full':pct>70?' warn':'');
  }).catch(()=>{});
}
// Poll disk space every 5s for live update
setInterval(loadDiskSpace, 5000);
loadDiskSpace();

// ── SPEED TRACKING ──
let lastBytes = 0, lastTime = Date.now(), lastDlBytes = 0;
function updateSpeed(){
  const now = Date.now();
  const bytes = Number(document.getElementById('sBytes').dataset.raw || 0);
  const dt = (now - lastTime) / 1000;
  if (dt > 0 && lastBytes > 0) {
    const spd = (bytes - lastBytes) / dt;
    document.getElementById('iUpSpeed').textContent = fmtBytes(spd) + '/s';
  }
  lastBytes = bytes;
  lastTime  = now;
}
setInterval(updateSpeed, 2000);

// ── INFO ──
fetch('/api/info').then(r=>r.json()).then(d=>{
  document.getElementById('urlList').innerHTML = d.ips.map(ip=>`<div class="url-item">http://${ip}:${port}/</div>`).join('');
  if (d.saving_dir) document.getElementById('dropSub').textContent = `Any type · Saved to ${d.saving_dir}`;
}).catch(()=>{ document.getElementById('urlList').innerHTML = `<div class="url-item">http://${location.hostname}:${port}/</div>`; });

// ── STATS POLL ──
function updateStats(){
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    document.getElementById('sFiles').textContent   = d.files;
    const bytesEl = document.getElementById('sBytes');
    bytesEl.textContent = fmtBytes(d.bytes);
    bytesEl.dataset.raw = d.bytes;
    document.getElementById('sClients').textContent = d.clients;
    document.getElementById('iClients').textContent = d.clients;
  }).catch(()=>{});
}
setInterval(updateStats, 4000);
updateStats();

// ── SSE LIVE UPDATES ──
// Robust SSE with:
//   • Proper teardown before every reconnect (no zombie connections)
//   • Watchdog: reconnects if no ping/event received in 35s (server pings every 20s)
//   • visibilitychange: instant reconnect when phone wakes up / tab becomes active
//   • Exponential back-off capped at 15s
let sse = null;
let sseWatchdog = null;
let sseBackoff = 1000; // ms, doubles on each failed attempt up to 15s
const SSE_TIMEOUT = 35000; // ms — must be > server ping interval (20s)

function resetSseWatchdog() {
  clearTimeout(sseWatchdog);
  sseWatchdog = setTimeout(() => {
    // No ping or data arrived in time — assume connection is silently dead
    reconnectSSE();
  }, SSE_TIMEOUT);
}

function teardownSSE() {
  clearTimeout(sseWatchdog);
  if (sse) {
    sse.onerror = null; // prevent onerror from firing during close
    sse.close();
    sse = null;
  }
}

function connectSSE() {
  teardownSSE();
  sse = new EventSource('/events');

  sse.addEventListener('db_update', e => {
    sseBackoff = 1000; // successful data — reset back-off
    resetSseWatchdog();
    try {
      const d = JSON.parse(e.data);
      renderDatabase(d.files||[], d.forwarding_folders||[]);
    } catch(_){}
  });

  sse.addEventListener('pastebin_update', e => {
    sseBackoff = 1000;
    resetSseWatchdog();
    try {
      const d = JSON.parse(e.data);
      const ta = document.getElementById('pasteTA');
      if (!pasteLocalEdit) { ta.value = d.content; pasteUpdateMeta(); }
    } catch(_){}
  });

  // onopen fires when connection is (re-)established
  sse.onopen = () => {
    sseBackoff = 1000;
    resetSseWatchdog();
  };

  sse.onerror = () => {
    teardownSSE();
    setTimeout(connectSSE, Math.min(sseBackoff, 15000));
    sseBackoff = Math.min(sseBackoff * 2, 15000);
  };

  // Server sends ":ping\n\n" SSE comments every 20s.
  // EventSource fires an unnamed "message" event for ":ping" comment lines —
  // we listen to onmessage as a catch-all to reset the watchdog on any traffic.
  // (SSE comment lines don't trigger named event listeners.)
  sse.onmessage = () => {
    sseBackoff = 1000;
    resetSseWatchdog();
  };

  resetSseWatchdog();
}

function reconnectSSE() {
  teardownSSE();
  connectSSE();
}

connectSSE();

// ── RECONNECT ON WAKE-UP ──
// When the phone comes back from sleep or the tab becomes visible again,
// force an immediate SSE reconnect instead of waiting for the watchdog.
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible') {
    reconnectSSE();
    // Also refresh data immediately on wake-up
    loadDiskSpace();
    updateStats();
  }
});

// ── PANEL MANAGEMENT ──
let activePanel = null;
function openPanel(which){
  document.getElementById('menuDropdown').classList.remove('open');
  document.getElementById('menuBtn').classList.remove('open');
  const sidebar = document.getElementById('sidebar');
  const panelDb   = document.getElementById('panelDb');
  const panelPaste= document.getElementById('panelPaste');
  const mDb = document.getElementById('menuDb');
  const mPaste = document.getElementById('menuPaste');

  if (activePanel === which) { closePanel(); return; }
  activePanel = which;
  sidebar.classList.add('open');
  mDb.classList.toggle('active', which==='db');
  mPaste.classList.toggle('active', which==='paste');

  if (which === 'db') {
    panelDb.style.display='flex'; panelDb.style.flexDirection='column';
    panelPaste.style.display='none';
    loadDatabase();
  } else {
    panelPaste.style.display='flex'; panelPaste.style.flexDirection='column';
    panelDb.style.display='none';
    loadPastebin();
  }
}
function closePanel(){
  activePanel = null;
  document.getElementById('sidebar').classList.remove('open');
  document.getElementById('menuDb').classList.remove('active');
  document.getElementById('menuPaste').classList.remove('active');
  document.getElementById('panelDb').style.display='none';
  document.getElementById('panelPaste').style.display='none';
}

// ── FILENAME ENCODING ──
// The server stores Windows-safe encoded filenames on disk and in database.json.
// Illegal chars are substituted with fullwidth Unicode equivalents:
//   | → ｜  * → ＊  ? → ？  " → ＂  < → ＜  > → ＞  : → ：
// decodeDisplayName() reverses this for display and the download= attribute.
function decodeDisplayName(name) {
  return String(name)
    .replace(/｜/g, '|')
    .replace(/＊/g, '*')
    .replace(/？/g, '?')
    .replace(/＂/g, '"')
    .replace(/＜/g, '<')
    .replace(/＞/g, '>')
    .replace(/：/g, ':');
}

// ── FILE TYPE CATEGORIZATION ──
const EXT_IMAGE   = new Set(['jpg','jpeg','png','gif','webp','bmp','ico','tiff','tif','svg','avif','heic','heif','raw','cr2','nef','arw','dng']);
const EXT_VIDEO   = new Set(['mp4','mkv','mov','avi','wmv','flv','webm','m4v','mpeg','mpg','3gp','ogv','ts','vob','rm','rmvb','m2ts','mts']);
const EXT_AUDIO   = new Set(['mp3','flac','wav','aac','ogg','m4a','wma','opus','aiff','aif','ape','mka','mid','midi','ra','amr','ac3','dts']);
const EXT_TEXT    = new Set(['txt','md','markdown','rst','csv','tsv','log','ini','cfg','conf','yaml','yml','json','xml','html','htm','xhtml',
  'css','js','jsx','ts','tsx','py','rb','php','java','c','cpp','cc','cxx','h','hpp','cs','go','rs','swift','kt','kts','sh','bash','zsh',
  'fish','ps1','bat','cmd','lua','r','pl','pm','sql','graphql','gql','toml','env','gitignore','dockerfile','makefile','cmake','gradle',
  'vue','svelte','dart','scala','clj','cljs','ex','exs','erl','hrl','hs','elm','ml','mli','v','vhd','asm','s','nasm','tex','bib','srt',
  'vtt','sub','ass','nfo','reg','inf','htaccess','nginx','apacheconf']);
const EXT_ARCHIVE = new Set(['zip','rar','7z','tar','gz','bz2','xz','lzma','cab','iso','dmg','pkg','deb','rpm','msi','jar','war','ear',
  'apk','ipa','tar.gz','tgz','tar.bz2','tar.xz','tar.zst','zst','lz4','lz','zlib','z','gz','br']);

// Browser-native playable types
const EXT_IMG_NATIVE   = new Set(['jpg','jpeg','png','gif','webp','bmp','svg','avif']);
const EXT_VIDEO_NATIVE = new Set(['mp4','webm','ogv','mov']);
const EXT_AUDIO_NATIVE = new Set(['mp3','wav','ogg','flac','m4a','aac','opus','aiff','aif']);

function getExt(name){ return (name||'').split('.').pop().toLowerCase(); }
function fileCategory(name){
  const e=getExt(name);
  if(EXT_IMAGE.has(e))   return 'image';
  if(EXT_VIDEO.has(e))   return 'video';
  if(EXT_AUDIO.has(e))   return 'audio';
  if(EXT_TEXT.has(e))    return 'text';
  if(EXT_ARCHIVE.has(e)) return 'archive';
  return 'other';
}
function fileIcon(name){
  const cat=fileCategory(name);
  const e=getExt(name);
  if(cat==='image')   return '🖼';
  if(cat==='video')   return '🎬';
  if(cat==='audio')   return '🎵';
  if(cat==='archive') return '🗜';
  if(cat==='text'){
    if(['py','rb','php','js','jsx','ts','tsx','java','c','cpp','go','rs','swift','kt','sh','bash','ps1','bat'].includes(e)) return '💻';
    if(['json','xml','yaml','yml','toml'].includes(e)) return '⚙';
    if(['sql'].includes(e)) return '🗃';
    return '📝';
  }
  return '📁';
}

// ── DB STATE ──
let _dbFiles=[], _dbFFs=[], _dbCompact=false;

// ── DATABASE ──
function loadDatabase(){
  fetch('/api/database').then(r=>r.json()).then(d=>{
    renderDatabase(d.files||[], d.forwarding_folders||[]);
  }).catch(()=>{ document.getElementById('dbItems').innerHTML='<div class="sb-empty">Error loading</div>'; });
}

function renderDatabase(files, ffs){
  _dbFiles=files; _dbFFs=ffs;
  applyDbFilters();

  // Forwarding folders
  const ffSec = document.getElementById('ffSection');
  const ffEl  = document.getElementById('ffItems');
  if(ffs && ffs.length > 0){
    ffSec.style.display='block';
    ffEl.innerHTML='';
    ffs.forEach(ff=>{
      const wrap=document.createElement('div');
      wrap.className='ff-folder';
      wrap.style.marginBottom='12px';
      // Header row
      const hdr=document.createElement('div');
      hdr.style.cssText='display:flex;align-items:center;justify-content:space-between;padding:5px 8px;background:var(--surface2);border-radius:4px;border-left:2px solid var(--blue);margin-bottom:4px;';
      hdr.innerHTML=`<span style="font-size:.7rem;color:var(--txt2);font-family:'JetBrains Mono',monospace;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1" title="${escHtml(ff.path)}">${escHtml(ff.path)}</span>`;
      if(ff.subfolders_enabled){
        hdr.innerHTML+=`<a class="ff-dl-btn" style="margin-left:8px" href="/download_ff_zip?id=${encodeURIComponent(ff.id)}" download>⬇ ZIP</a>`;
      }
      wrap.appendChild(hdr);

      if(ff.subfolders_enabled && ff.tree){
        // Render recursive tree
        const treeEl=renderFfTree(ff.tree, ff.id, 0);
        wrap.appendChild(treeEl);
      } else {
        // Flat file list
        const listEl=document.createElement('div');
        const items=ff.contents||[];
        if(!items.length){
          listEl.innerHTML='<div style="font-size:.65rem;color:var(--dim);padding:4px 8px">Empty folder</div>';
        } else {
          items.forEach(c=>{
            const displayName=decodeDisplayName(c.name||c);
            const ext=getExt(displayName);
            const cat=fileCategory(displayName);
            const fi=document.createElement('div');
            fi.className='ff-file';
            fi.innerHTML=`
              <span class="ff-file-name" title="${escHtml(displayName)}">${fileIcon(displayName)} ${escHtml(displayName)}</span>
              <div class="ff-file-actions">
                <button class="ff-view-btn" onclick='openPreview(${JSON.stringify({name:displayName,path:c.path||c,src:"ff"})})'>👁 View</button>
                <a class="ff-dl-btn" href="/download_ff?path=${encodeURIComponent(c.path||c)}" download="${escHtml(displayName)}">↓</a>
              </div>`;
            listEl.appendChild(fi);
          });
        }
        wrap.appendChild(listEl);
      }
      ffEl.appendChild(wrap);
    });
  } else {
    ffSec.style.display='none';
  }
}

function renderFfTree(node, ffId, depth){
  const el=document.createElement('div');
  if(node.isDir){
    el.className='ff-tree-dir';
    const hdr=document.createElement('div');
    hdr.className='ff-tree-dir-hdr';
    const isRoot=(depth===0);
    if(!isRoot){
      hdr.innerHTML=`<span class="tree-toggle">▾</span><span>📁 ${escHtml(node.name)}</span>`;
      const dlBtn=document.createElement('button');
      dlBtn.className='dir-dl-btn';
      dlBtn.textContent='⬇ ZIP';
      dlBtn.onclick=()=>{ window.location='/download_ff_zip?path='+encodeURIComponent(node.path); };
      hdr.appendChild(dlBtn);
      hdr.onclick=(e)=>{
        if(e.target===dlBtn||dlBtn.contains(e.target)) return;
        const dir=el;
        dir.classList.toggle('collapsed');
        hdr.classList.toggle('collapsed');
      };
    }
    el.appendChild(hdr);
    if(!isRoot){
      const kids=document.createElement('div');
      kids.className='ff-tree-children';
      (node.children||[]).forEach(c=>kids.appendChild(renderFfTree(c,ffId,depth+1)));
      el.appendChild(kids);
    } else {
      // Root node: render children directly (header already shown by parent)
      const kids=document.createElement('div');
      kids.style.marginLeft='0';
      (node.children||[]).forEach(c=>kids.appendChild(renderFfTree(c,ffId,depth+1)));
      el.appendChild(kids);
    }
  } else {
    el.className='ff-file';
    const displayName=decodeDisplayName(node.name);
    el.innerHTML=`
      <span class="ff-file-name" title="${escHtml(displayName)}">${fileIcon(displayName)} ${escHtml(displayName)}</span>
      <div class="ff-file-actions">
        <span class="ff-size">${fmtBytes(node.size)}</span>
        <button class="ff-view-btn" onclick='openPreview(${JSON.stringify({name:displayName,path:node.path,src:"ff"})})'>👁</button>
        <a class="ff-dl-btn" href="/download_ff?path=${encodeURIComponent(node.path)}" download="${escHtml(displayName)}">↓</a>
      </div>`;
  }
  return el;
}

// ── APPLY FILTERS/SORT TO DB ITEMS ──
function applyDbFilters(){
  const q=(document.getElementById('dbSearch')||{value:''}).value.toLowerCase();
  const sort=(document.getElementById('dbSortSel')||{value:'time_desc'}).value;
  const type=(document.getElementById('dbTypeSel')||{value:''}).value;
  let files=[..._dbFiles];

  // Filter by search
  if(q) files=files.filter(f=>decodeDisplayName(f.name).toLowerCase().includes(q));
  // Filter by type
  if(type) files=files.filter(f=>fileCategory(decodeDisplayName(f.name))===type);
  // Sort
  files.sort((a,b)=>{
    const na=decodeDisplayName(a.name), nb=decodeDisplayName(b.name);
    if(sort==='time_desc') return 0; // already newest-first from server
    if(sort==='time_asc')  return 1;  // reverse (crude but workable)
    if(sort==='size_desc') return b.size-a.size;
    if(sort==='size_asc')  return a.size-b.size;
    if(sort==='name_asc')  return na.localeCompare(nb);
    if(sort==='name_desc') return nb.localeCompare(na);
    return 0;
  });
  if(sort==='time_asc') files.reverse();

  const el=document.getElementById('dbItems');
  if(!files.length){
    el.innerHTML='<div class="sb-empty">'+(q||type?'No matches':'No files in database')+'</div>';
    return;
  }
  el.innerHTML='';
  files.forEach(f=>{
    const displayName=decodeDisplayName(f.name);
    const cat=fileCategory(displayName);
    const isImg=EXT_IMG_NATIVE.has(getExt(displayName));
    const item=document.createElement('div');
    item.className='db-item'+(_dbCompact?' compact':'');
    item.id='dbItem_'+f.id;
    item.innerHTML=`
      ${isImg?`<img class="file-thumb" src="/preview_inline?id=${encodeURIComponent(f.id)}" onerror="this.style.display='none'" loading="lazy">`:
               `<div class="thumb-placeholder">${fileIcon(displayName)}</div>`}
      <div class="db-item-name">${escHtml(displayName)}</div>
      <div class="db-item-meta">${fmtBytes(f.size)} · ${escHtml(f.timestamp)} · from ${escHtml(f.from)}</div>
      <div class="db-item-actions">
        <button class="db-btn db-btn-view" onclick='openPreview({name:${JSON.stringify(displayName)},id:${JSON.stringify(f.id)},src:"db"})'>👁 View</button>
        <a class="db-btn db-btn-dl" href="/download?id=${encodeURIComponent(f.id)}" download="${escHtml(displayName)}">↓ DL</a>
        <button class="db-btn db-btn-del" onclick="dbDelete('${f.id}')">✕</button>
      </div>`;
    el.appendChild(item);
  });
}
function toggleCompact(){
  _dbCompact=!_dbCompact;
  const btn=document.getElementById('ctlCompact');
  if(btn) btn.classList.toggle('active',_dbCompact);
  applyDbFilters();
}

function dbDelete(id){
  if(!confirm('Delete this file from the host?')) return;
  fetch('/api/database/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id,delete_file:true})})
  .then(r=>r.json()).then(d=>{
    if(d.success){ const el=document.getElementById('dbItem_'+id); if(el) el.remove(); showToast('✓ File deleted'); }
    else showToast('Delete failed: '+(d.error||'unknown'),true);
  }).catch(()=>showToast('Network error',true));
}

// ── PREVIEW MODAL ──
let _previewCurrent=null, _previewFullText=false;

function openPreview(info){
  _previewCurrent=info;
  _previewFullText=false;
  const overlay=document.getElementById('previewOverlay');
  overlay.classList.remove('hidden');
  document.getElementById('previewTitle').textContent=info.name||'Preview';
  document.getElementById('previewBody').innerHTML='<div style="color:var(--dim);font-family:\'JetBrains Mono\',monospace;font-size:.8rem">Loading…</div>';
  document.getElementById('btnShowFull').style.display='none';
  document.getElementById('previewInfo').textContent='';

  const ext=getExt(info.name||'');
  const cat=fileCategory(info.name||'');
  let url='';
  if(info.src==='db')   url='/preview_inline?id='+encodeURIComponent(info.id);
  else                  url='/preview_inline_ff?path='+encodeURIComponent(info.path);
  const dlUrl=(info.src==='db')?'/download?id='+encodeURIComponent(info.id):'/download_ff?path='+encodeURIComponent(info.path);
  const dlBtn=document.getElementById('previewDlBtn');
  dlBtn.href=dlUrl; dlBtn.download=info.name||'file';

  if(cat==='image'){
    if(EXT_IMG_NATIVE.has(ext)){
      document.getElementById('previewBody').innerHTML=`<img class="preview-img" src="${url}" alt="${escHtml(info.name)}">`;
      document.getElementById('previewInfo').textContent='Image preview';
    } else {
      _showUnsupported(info.name, url, cat);
    }
  } else if(cat==='video'){
    if(EXT_VIDEO_NATIVE.has(ext)){
      document.getElementById('previewBody').innerHTML=`<video class="preview-video" controls autoplay><source src="${url}"><p style="color:var(--dim)">Cannot play this video.</p></video>`;
    } else {
      _showUnsupported(info.name, url, cat);
    }
  } else if(cat==='audio'){
    if(EXT_AUDIO_NATIVE.has(ext)){
      document.getElementById('previewBody').innerHTML=`<div class="preview-audio-wrap"><div style="font-size:3rem;margin-bottom:16px">🎵</div><p style="color:var(--txt2);margin-bottom:12px">${escHtml(info.name)}</p><audio class="preview-audio" controls autoplay><source src="${url}"></audio></div>`;
    } else {
      _showUnsupported(info.name, url, cat);
    }
  } else if(cat==='text'){
    _fetchTextPreview(url, info.name);
  } else if(cat==='archive'){
    _fetchArchivePreview(url, info, ext);
  } else {
    _showUnsupported(info.name, url, cat);
  }
}

function _showUnsupported(name, url, cat){
  document.getElementById('previewBody').innerHTML=`
    <div class="preview-unsupported">
      <div style="font-size:3rem;margin-bottom:12px">${fileIcon(name)}</div>
      <p>${escHtml(name)}</p>
      <p style="font-size:.72rem;color:var(--dim);margin-top:4px">This file type cannot be previewed directly in the browser.</p>
      <div style="display:flex;gap:8px;justify-content:center;margin-top:16px;flex-wrap:wrap">
        <button class="preview-foot-btn active" onclick="previewAsText()">📄 View as Text</button>
      </div>
    </div>`;
}

function _fetchTextPreview(url, name){
  fetch(url).then(r=>r.text()).then(txt=>{
    const TRUNC=8000;
    const truncated=txt.length>TRUNC;
    const display=truncated?txt.slice(0,TRUNC):txt;
    document.getElementById('previewBody').innerHTML=`<pre class="preview-text">${escHtml(display)}</pre>`;
    document.getElementById('previewInfo').textContent=txt.length+' chars';
    const sfBtn=document.getElementById('btnShowFull');
    if(truncated){ sfBtn.style.display='inline-block'; sfBtn.dataset.full=txt; }
    else sfBtn.style.display='none';
  }).catch(()=>{ document.getElementById('previewBody').innerHTML='<div class="preview-unsupported"><p style="color:var(--err)">Failed to load text</p></div>'; });
}

function _fetchArchivePreview(url, info, ext){
  if(ext==='zip'){
    fetch('/api/archive_browse?src='+info.src+'&'+(info.src==='db'?'id='+encodeURIComponent(info.id):'path='+encodeURIComponent(info.path)))
    .then(r=>r.json()).then(d=>{
      if(d.password_protected){
        document.getElementById('previewBody').innerHTML=`
          <div class="preview-unsupported">
            <p>🔒 This archive is password protected.</p>
            <div class="arch-pw-row" style="margin-top:12px">
              <input class="arch-pw-input" id="archPwInput" type="password" placeholder="Enter password…">
              <button class="preview-foot-btn active" onclick="_archiveWithPw('${escHtml(info.src)}','${escHtml(info.id||'')}','${escHtml(info.path||'')}')">Unlock</button>
              <button class="preview-foot-btn" onclick="closePreview()">Cancel</button>
            </div>
          </div>`;
      } else if(d.entries){
        _renderArchiveList(d.entries);
      } else {
        document.getElementById('previewBody').innerHTML='<div class="preview-unsupported"><p style="color:var(--err)">Failed to read archive.</p></div>';
      }
    }).catch(()=>{ _showUnsupported(info.name, url, 'archive'); });
  } else {
    _showUnsupported(info.name, url, 'archive');
  }
}

function _archiveWithPw(src, id, path){
  const pw=document.getElementById('archPwInput').value;
  const qs=src==='db'?'id='+encodeURIComponent(id):'path='+encodeURIComponent(path);
  fetch('/api/archive_browse?src='+src+'&'+qs+'&pw='+encodeURIComponent(pw))
  .then(r=>r.json()).then(d=>{
    if(d.entries) _renderArchiveList(d.entries);
    else document.getElementById('previewBody').innerHTML='<div class="preview-unsupported"><p style="color:var(--err)">Wrong password or error.</p></div>';
  });
}

function _renderArchiveList(entries){
  let html='<div class="preview-archive">';
  entries.forEach(e=>{
    html+=`<div class="preview-archive-item">${e.isDir?'📁':'📄'} <span style="flex:1">${escHtml(e.name)}</span><span style="color:var(--dim)">${e.isDir?'':fmtBytes(e.size)}</span></div>`;
  });
  html+='</div>';
  document.getElementById('previewBody').innerHTML=html;
  document.getElementById('previewInfo').textContent=entries.length+' entries';
}

function previewAsText(){
  if(!_previewCurrent) return;
  const info=_previewCurrent;
  let url='';
  if(info.src==='db')   url='/preview_inline?id='+encodeURIComponent(info.id);
  else                  url='/preview_inline_ff?path='+encodeURIComponent(info.path);
  _fetchTextPreview(url, info.name);
}

function previewShowFull(){
  const sfBtn=document.getElementById('btnShowFull');
  const full=sfBtn.dataset.full||'';
  document.getElementById('previewBody').innerHTML=`<pre class="preview-text">${escHtml(full)}</pre>`;
  sfBtn.style.display='none';
}

function closePreview(e){
  if(e && e.target!==document.getElementById('previewOverlay')) return;
  document.getElementById('previewOverlay').classList.add('hidden');
  // Stop any playing media
  document.querySelectorAll('#previewBody audio,#previewBody video').forEach(m=>{m.pause();m.src='';});
  _previewCurrent=null;
}
// Keyboard: Escape to close preview
document.addEventListener('keydown',e=>{ if(e.key==='Escape') document.getElementById('previewOverlay').classList.add('hidden'); });


// ── PASTEBIN ──
let pasteLocalEdit = false, pasteEditTimer = null, pasteSyncTimer = null;
function loadPastebin(){
  fetch('/api/pastebin').then(r=>r.json()).then(d=>{
    document.getElementById('pasteTA').value = d.content||'';
    pasteUpdateMeta();
  }).catch(()=>{});
}
function pasteUpdateMeta(){
  document.getElementById('pasteCharCount').textContent = document.getElementById('pasteTA').value.length + ' chars';
}
function pasteSyncNow(){
  const content = document.getElementById('pasteTA').value;
  setSyncLabel(false);
  fetch('/api/pastebin',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({content})})
  .then(()=>{ pasteLocalEdit=false; setSyncLabel(true); }).catch(()=>setSyncLabel(true));
}
function setSyncLabel(synced){
  const el = document.getElementById('pasteSyncLabel');
  el.textContent = synced ? '● synced' : '⟳ syncing…';
  el.className = synced ? 'paste-synced' : 'paste-syncing';
}
document.getElementById('pasteTA').addEventListener('input', ()=>{
  pasteLocalEdit = true; pasteUpdateMeta(); setSyncLabel(false);
  clearTimeout(pasteSyncTimer); pasteSyncTimer = setTimeout(pasteSyncNow, 300);
  clearTimeout(pasteEditTimer); pasteEditTimer = setTimeout(()=>{ pasteLocalEdit=false; }, 600);
});
function pasteClear(){ document.getElementById('pasteTA').value=''; pasteUpdateMeta(); pasteSyncNow(); }
function pasteCopyLocal(){ navigator.clipboard.writeText(document.getElementById('pasteTA').value).then(()=>showToast('✓ Copied')); }

// ── UPLOAD ──
function uploadFiles(files){
  if(!files||!files.length) return;
  const meta={};
  for(const f of files) meta[f.name]={lastModified:f.lastModified,size:f.size};
  const fd=new FormData();
  for(const f of files) fd.append('files',f);
  fd.append('metadata',JSON.stringify(meta));
  const wrap=document.getElementById('progWrap');
  const bar=document.getElementById('progBar');
  const pct=document.getElementById('progPct');
  const lbl=document.getElementById('progLabel');
  const sts=document.getElementById('progStatus');
  wrap.style.display='block'; bar.style.width='0%'; pct.textContent='0%';
  lbl.textContent=`Uploading ${files.length} file${files.length>1?'s':''}…`;
  const xhr=new XMLHttpRequest();
  let startTime = Date.now(), lastLoaded = 0;
  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){
      const p=Math.round(e.loaded/e.total*100);
      bar.style.width=p+'%'; pct.textContent=p+'%';
      const dt=(Date.now()-startTime)/1000;
      const spd = dt>0 ? (e.loaded-lastLoaded)/Math.max(0.5,dt) : 0;
      sts.textContent=fmtBytes(e.loaded)+' / '+fmtBytes(e.total)+(spd>0?' · '+fmtBytes(e.loaded/Math.max(0.1,dt))+'/s':'');
      document.getElementById('iUpSpeed').textContent = fmtBytes(e.loaded/Math.max(0.1,dt)) + '/s';
    }
  };
  xhr.onload=()=>{
    wrap.style.display='none';
    if(xhr.status===200){
      try{
        const res=JSON.parse(xhr.responseText);
        for(const f of res.files) addFileCard(f.name,f.size,f.ts,res.saving_dir);
        showToast('✓ '+res.files.length+' file(s) saved');
        updateStats(); loadDiskSpace();
      }catch(e){showToast('Parse error',true);}
    } else showToast('Upload failed: HTTP '+xhr.status,true);
  };
  xhr.onerror=()=>{wrap.style.display='none';showToast('Network error',true);};
  xhr.open('POST','/upload'); xhr.send(fd);
}
function addFileCard(name,size,ts,savingDir){
  const div=document.createElement('div'); div.className='file-item';
  div.innerHTML=`<div class="ficon">${fileIcon(name)}</div>
    <div class="fmeta"><div class="fname">${escHtml(name)}</div>
    <div class="fdetail">${fmtBytes(size)} · ${ts} · ${escHtml(savingDir||'Desktop')}</div></div>
    <div class="fbadge">✓ SAVED</div>`;
  document.getElementById('fileItems').prepend(div);
  const sf = document.getElementById('sFiles');
  sf.textContent = parseInt(sf.textContent||0)+1;
}

// ── DRAG & DROP ──
const dz = document.getElementById('dropZone');
dz.addEventListener('dragover', e=>{ e.preventDefault(); dz.classList.add('over'); });
dz.addEventListener('dragleave', ()=>dz.classList.remove('over'));
dz.addEventListener('drop', e=>{ e.preventDefault(); dz.classList.remove('over'); uploadFiles(e.dataTransfer.files); });
dz.addEventListener('click', ()=>document.getElementById('fileInput').click());
document.getElementById('fileInput').addEventListener('change', e=>uploadFiles(e.target.files));

// ── UTILS ──
function showToast(msg,isErr=false){
  const t=document.getElementById('toast');
  t.textContent=msg; t.className='toast '+(isErr?'err':'ok')+' show';
  setTimeout(()=>t.className='toast',4000);
}
function fmtBytes(b){
  b=Number(b);
  if(b<1024) return b+' B';
  if(b<1048576) return (b/1024).toFixed(1)+' KB';
  if(b<1073741824) return (b/1048576).toFixed(1)+' MB';
  return (b/1073741824).toFixed(2)+' GB';
}
function escHtml(s){ return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }

</script>
</body>
</html>
)html";

// ────────────────────────────────────────────────────────────────
//  HTTP-INTERNAL HELPERS  (findSeq, getHeaderVal, extractFilename, etc.)
// ────────────────────────────────────────────────────────────────

static size_t findSeq(const std::vector<uint8_t>& buf, size_t start, const std::string& pat) {
    if (pat.empty() || buf.size() < pat.size()) return std::string::npos;
    for (size_t i = start; i + pat.size() <= buf.size(); ++i) {
        if (memcmp(buf.data() + i, pat.data(), pat.size()) == 0) return i;
    }
    return std::string::npos;
}

static std::string getHeaderVal(const std::string& headers, const std::string& name) {
    std::string h = headers, n = name;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    size_t pos = h.find(n);
    if (pos == std::string::npos) return "";
    pos = h.find(':', pos);
    if (pos == std::string::npos) return "";
    size_t eol = h.find('\n', pos);
    std::string val = headers.substr(pos + 1, eol == std::string::npos ? std::string::npos : eol - pos - 1);
    return trim(val);
}

static std::string extractFilename(const std::string& cd) {
    size_t pos = cd.find("filename*=");
    if (pos != std::string::npos) {
        pos = cd.find("''", pos);
        if (pos != std::string::npos) {
            std::string enc = cd.substr(pos + 2);
            std::string dec;
            for (size_t i = 0; i < enc.size(); ++i) {
                if (enc[i] == '%' && i + 2 < enc.size()) {
                    char hex[3] = {enc[i+1], enc[i+2], 0};
                    dec += (char)strtol(hex, nullptr, 16); i += 2;
                } else dec += enc[i];
            }
            return trim(dec);
        }
    }
    pos = cd.find("filename=");
    if (pos == std::string::npos) return "";
    pos += 9;
    if (pos < cd.size() && cd[pos] == '"') {
        size_t end = cd.find('"', pos + 1);
        if (end != std::string::npos) return cd.substr(pos + 1, end - pos - 1);
    }
    size_t end = cd.find_first_of(";\r\n", pos);
    return trim(cd.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
}

static int64_t extractJsonInt64(const std::string& json, const std::string& filename, const std::string& key) {
    std::string fkey = "\"" + filename + "\"";
    size_t fi = json.find(fkey);
    if (fi == std::string::npos) return 0;
    std::string kkey = "\"" + key + "\"";
    size_t ki = json.find(kkey, fi);
    if (ki == std::string::npos) return 0;
    size_t ci = json.find(':', ki + kkey.size());
    if (ci == std::string::npos) return 0;
    ++ci;
    while (ci < json.size() && (json[ci]==' '||json[ci]=='\t')) ++ci;
    if (ci >= json.size()) return 0;
    try { return (int64_t)std::stoll(json.substr(ci)); } catch (...) { return 0; }
}

static void setFileTimes(const std::string& path, int64_t lastModifiedMs) {
    if (lastModifiedMs <= 0) return;
    LONGLONG ll = (LONGLONG)lastModifiedMs * 10000LL + 116444736000000000LL;
    FILETIME ft;
    ft.dwLowDateTime  = (DWORD)(ll & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(ll >> 32);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
    HANDLE h = CreateFileW(wpath.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) { SetFileTime(h, &ft, &ft, &ft); CloseHandle(h); }
}

// ────────────────────────────────────────────────────────────────
//  FILENAME ENCODER / DECODER
//  Maps Windows-illegal chars to visually-similar fullwidth Unicode
//  equivalents that are valid in NTFS.  The mapping is reversible so
//  the original name can be reconstructed client-side (and in the
//  Content-Disposition header) without any lossy substitution.
//
//  Mapping (UTF-8 byte sequences):
//    |  (0x7C)  →  ｜  U+FF5C  EF BD 9C
//    *  (0x2A)  →  ＊  U+FF0A  EF BC 8A
//    ?  (0x3F)  →  ？  U+FF1F  EF BC 9F
//    "  (0x22)  →  ＂  U+FF02  EF BC 82
//    <  (0x3C)  →  ＜  U+FF1C  EF BC 9C
//    >  (0x3E)  →  ＞  U+FF1E  EF BC 9E
//    :  (0x3A)  →  ：  U+FF1A  EF BC 9A
// ────────────────────────────────────────────────────────────────
static std::string encodeFilenameForDisk(const std::string& name) {
    std::string out;
    out.reserve(name.size() * 2);
    for (unsigned char c : name) {
        switch (c) {
            case '|': out += "\xEF\xBD\x9C"; break;  // ｜ U+FF5C
            case '*': out += "\xEF\xBC\x8A"; break;  // ＊ U+FF0A
            case '?': out += "\xEF\xBC\x9F"; break;  // ？ U+FF1F
            case '"': out += "\xEF\xBC\x82"; break;  // ＂ U+FF02
            case '<': out += "\xEF\xBC\x9C"; break;  // ＜ U+FF1C
            case '>': out += "\xEF\xBC\x9E"; break;  // ＞ U+FF1E
            case ':': out += "\xEF\xBC\x9A"; break;  // ： U+FF1A
            default:  out += (char)c;         break;
        }
    }
    return out;
}

static std::string decodeFilenameFromDisk(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (size_t i = 0; i < name.size(); ) {
        unsigned char c  = (unsigned char)name[i];
        if (c == 0xEF && i + 2 < name.size()) {
            unsigned char b1 = (unsigned char)name[i+1];
            unsigned char b2 = (unsigned char)name[i+2];
            if      (b1 == 0xBD && b2 == 0x9C) { out += '|'; i += 3; continue; } // ｜
            else if (b1 == 0xBC && b2 == 0x8A) { out += '*'; i += 3; continue; } // ＊
            else if (b1 == 0xBC && b2 == 0x9F) { out += '?'; i += 3; continue; } // ？
            else if (b1 == 0xBC && b2 == 0x82) { out += '"'; i += 3; continue; } // ＂
            else if (b1 == 0xBC && b2 == 0x9C) { out += '<'; i += 3; continue; } // ＜
            else if (b1 == 0xBC && b2 == 0x9E) { out += '>'; i += 3; continue; } // ＞
            else if (b1 == 0xBC && b2 == 0x9A) { out += ':'; i += 3; continue; } // ：
        }
        out += name[i++];
    }
    return out;
}

static std::string sanitizeFilename(const std::string& name) {
    // Encode illegal chars to fullwidth Unicode equivalents
    std::string encoded = encodeFilenameForDisk(name);
    // Strip path separators (/ and \) — they are not encodable as single chars
    std::string out;
    for (char c : encoded) {
        if (c == '/' || c == '\\') out += '_';
        else out += c;
    }
    if (out.empty() || out == "." || out == "..") out = "upload";
    return out;
}

static std::string uniquePath(const std::string& dir, const std::string& name) {
    std::string path = dir + "\\" + name;
    if (GetFileAttributesW(toWide(path).c_str()) == INVALID_FILE_ATTRIBUTES) return path;
    size_t dot = name.rfind('.');
    std::string base = (dot == std::string::npos) ? name : name.substr(0, dot);
    std::string ext  = (dot == std::string::npos) ? ""   : name.substr(dot);
    for (int i = 2; i < 9999; ++i) {
        path = dir + "\\" + base + "(" + std::to_string(i) + ")" + ext;
        if (GetFileAttributesW(toWide(path).c_str()) == INVALID_FILE_ATTRIBUTES) return path;
    }
    return dir + "\\" + name;
}

// URL decode a query string value
static std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') { out += ' '; }
        else if (s[i] == '%' && i+2 < s.size()) {
            char hex[3] = {s[i+1], s[i+2], 0};
            out += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else { out += s[i]; }
    }
    return out;
}

// Parse query string: extract value for key
static std::string queryParam(const std::string& path, const std::string& key) {
    size_t q = path.find('?');
    if (q == std::string::npos) return "";
    std::string qs = path.substr(q + 1);
    std::string k = key + "=";
    size_t pos = qs.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    size_t end = qs.find('&', pos);
    std::string val = qs.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    return urlDecode(val);
}

// ────────────────────────────────────────────────────────────────
//  HTTP HELPERS
// ────────────────────────────────────────────────────────────────

static bool recvAll(SOCKET s, std::string& headers, std::vector<uint8_t>& body) {
    std::string raw;
    raw.reserve(8192);
    char buf[65536];
    for (;;) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        raw.append(buf, n);
        auto pos = raw.find("\r\n\r\n");
        if (pos != std::string::npos) {
            headers = raw.substr(0, pos);
            std::string tail = raw.substr(pos + 4);
            body.assign(tail.begin(), tail.end());
            break;
        }
        if (raw.size() > 128 * 1024) return false; // header too large
    }
    std::string cl = getHeaderVal(headers, "Content-Length");
    if (cl.empty()) return true;
    int64_t need = 0;
    try { need = std::stoll(cl); } catch (...) { return false; }
    Log(L_VERB, "  Content-Length: " + cl + " bytes");

    // Pre-reserve to avoid O(n^2) insertions
    if (need > 0 && need <= (int64_t)4ULL * 1024 * 1024 * 1024) { // cap at 4GB
        try { body.reserve((size_t)need); } catch (...) { return false; }
    }

    // For large uploads, remove the per-recv timeout so slow connections don't get cut off
    if (need > (int64_t)32 * 1024 * 1024) { // > 32 MB
        DWORD noTimeout = 0; // infinite
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&noTimeout, sizeof(noTimeout));
    }

    while ((int64_t)body.size() < need) {
        int64_t rem = need - (int64_t)body.size();
        int chunk = (int)std::min(rem, (int64_t)sizeof(buf));
        int n = recv(s, buf, chunk, 0);
        if (n <= 0) break;
        body.insert(body.end(), buf, buf + n);
    }
    return (int64_t)body.size() >= need;
}

static void sendResp(SOCKET s, int code, const std::string& ctype,
                     const std::string& body, const std::string& extraHdrs = "") {
    const char* status = (code == 200) ? "OK" : (code == 404) ? "Not Found" : "Bad Request";
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << status << "\r\n"
        << "Content-Type: "   << ctype  << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << extraHdrs << "\r\n" << body;
    std::string resp = oss.str();
    send(s, resp.data(), (int)resp.size(), 0);
}

// Stream a file download.
// filePath = actual path on disk (UTF-8, may contain fullwidth-encoded chars).
// diskName = the filename as it exists on disk (encoded, e.g. "hello ｜ world.mp4").
//            This is what Content-Disposition sends — the browser's download= attribute
//            on the <a> tag (set client-side to the decoded original) takes precedence
//            for UI-triggered downloads on same-origin requests.
static void sendFileDownload(SOCKET s, const std::string& filePath, const std::string& diskName) {
    HANDLE hf = CreateFileW(toWide(filePath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        sendResp(s, 404, "text/plain", "File not found");
        return;
    }
    LARGE_INTEGER sz; GetFileSizeEx(hf, &sz);

    // Percent-encode the disk name (UTF-8 bytes) for the RFC 5987 filename* parameter.
    // This keeps the encoded fullwidth chars intact in the header as %XX sequences.
    std::string rfc5987;
    for (unsigned char c : diskName) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            rfc5987 += (char)c;
        } else {
            char hex[4];
            sprintf_s(hex, "%%%02X", (unsigned)c);
            rfc5987 += hex;
        }
    }
    // ASCII-safe fallback for the plain filename= parameter
    std::string asciiName;
    for (unsigned char c : diskName) {
        if (c >= 32 && c < 127 && c != '"' && c != '\\') asciiName += (char)c;
        else asciiName += '_';
    }
    if (asciiName.empty()) asciiName = "download";

    std::ostringstream hdr;
    hdr << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/octet-stream\r\n"
        << "Content-Disposition: attachment; filename=\"" << asciiName
        << "\"; filename*=UTF-8''" << rfc5987 << "\r\n"
        << "Content-Length: " << sz.QuadPart << "\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n\r\n";
    std::string hdrStr = hdr.str();
    send(s, hdrStr.data(), (int)hdrStr.size(), 0);

    char chunk[65536];
    DWORD readBytes = 0;
    while (ReadFile(hf, chunk, sizeof(chunk), &readBytes, nullptr) && readBytes > 0) {
        int sent = 0, total = (int)readBytes;
        while (sent < total) {
            int r = send(s, chunk + sent, total - sent, 0);
            if (r <= 0) goto done;
            sent += r;
        }
    }
done:
    CloseHandle(hf);
}

// ────────────────────────────────────────────────────────────────
//  MINIMAL ZIP WRITER (store-mode, no compression)
// ────────────────────────────────────────────────────────────────
struct ZipBuilder {
    std::vector<uint8_t> data;
    struct Entry { std::string name; uint32_t crc; uint32_t size; uint32_t offset; };
    std::vector<Entry> entries;

    static uint32_t crc32(const uint8_t* buf, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= buf[i];
            for (int j = 0; j < 8; j++) crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        }
        return ~crc;
    }
    void writeLE16(uint16_t v){ data.push_back(v&0xFF); data.push_back((v>>8)&0xFF); }
    void writeLE32(uint32_t v){ data.push_back(v&0xFF); data.push_back((v>>8)&0xFF); data.push_back((v>>16)&0xFF); data.push_back((v>>24)&0xFF); }
    void writeBytes(const void* p, size_t n){ auto* b=(const uint8_t*)p; data.insert(data.end(),b,b+n); }

    bool addFile(const std::string& entryName, const std::string& diskPath) {
        HANDLE hf = CreateFileW(toWide(diskPath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER sz; GetFileSizeEx(hf, &sz);
        if (sz.QuadPart > 512LL*1024*1024) { CloseHandle(hf); return false; } // skip >512MB files
        std::vector<uint8_t> buf((size_t)sz.QuadPart);
        DWORD r = 0; ReadFile(hf, buf.data(), (DWORD)buf.size(), &r, nullptr);
        CloseHandle(hf);
        if (r != (DWORD)sz.QuadPart) return false;

        uint32_t crc = crc32(buf.data(), buf.size());
        uint32_t offset = (uint32_t)data.size();

        // Local file header
        writeLE32(0x04034b50); writeLE16(20); writeLE16(0); writeLE16(0); // sig, ver, flags, method(store)
        writeLE16(0); writeLE16(0); // mod time/date
        writeLE32(crc); writeLE32((uint32_t)buf.size()); writeLE32((uint32_t)buf.size());
        writeLE16((uint16_t)entryName.size()); writeLE16(0); // fname len, extra len
        writeBytes(entryName.data(), entryName.size());
        writeBytes(buf.data(), buf.size());

        entries.push_back({entryName, crc, (uint32_t)buf.size(), offset});
        return true;
    }

    std::vector<uint8_t> finish() {
        uint32_t cdOffset = (uint32_t)data.size();
        for (auto& e : entries) {
            writeLE32(0x02014b50); writeLE16(20); writeLE16(20); writeLE16(0); writeLE16(0); writeLE16(0);
            writeLE16(0); writeLE16(0); // time/date
            writeLE32(e.crc); writeLE32(e.size); writeLE32(e.size);
            writeLE16((uint16_t)e.name.size()); writeLE16(0); writeLE16(0); writeLE16(0); writeLE16(0);
            writeLE32(0); writeLE32(e.offset);
            writeBytes(e.name.data(), e.name.size());
        }
        uint32_t cdSize = (uint32_t)data.size() - cdOffset;
        // EOCD
        writeLE32(0x06054b50); writeLE16(0); writeLE16(0);
        writeLE16((uint16_t)entries.size()); writeLE16((uint16_t)entries.size());
        writeLE32(cdSize); writeLE32(cdOffset); writeLE16(0);
        return data;
    }
};

// Collect all files in a directory tree into a ZipBuilder (relative paths)
static void zipAddDir(ZipBuilder& zb, const std::string& base, const std::string& rel, int depth) {
    if (depth > 20) return; // safety
    std::wstring wpattern = toWide(base + "\\" + rel + "\\*");
    WIN32_FIND_DATAW fdW;
    HANDLE h = FindFirstFileW(wpattern.c_str(), &fdW);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        // Only skip the special . and .. entries, not dot-files like .gitignore
        if (fdW.cFileName[0] == L'.' && (fdW.cFileName[1] == L'\0' || (fdW.cFileName[1] == L'.' && fdW.cFileName[2] == L'\0'))) continue;
        char fnUtf8[MAX_PATH*4] = {};
        WideCharToMultiByte(CP_UTF8, 0, fdW.cFileName, -1, fnUtf8, sizeof(fnUtf8), nullptr, nullptr);
        std::string childRel = rel.empty() ? fnUtf8 : (rel + "/" + fnUtf8);
        std::string childAbs = base + "\\" + (rel.empty() ? "" : rel+"\\") + fnUtf8;
        // normalize rel to use forward slashes
        for (char& c : childRel) if (c == '\\') c = '/';
        if (fdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            zipAddDir(zb, base, childRel, depth+1);
        } else {
            zb.addFile(childRel, childAbs);
        }
    } while (FindNextFileW(h, &fdW));
    FindClose(h);
}

// Send ZIP response
static void sendZip(SOCKET s, const std::string& folderPath, const std::string& zipName) {
    ZipBuilder zb;
    // Add top-level files
    std::wstring wpattern = toWide(folderPath + "\\*");
    WIN32_FIND_DATAW fdW;
    HANDLE h = FindFirstFileW(wpattern.c_str(), &fdW);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fdW.cFileName[0] == L'.' && (fdW.cFileName[1] == L'\0' || (fdW.cFileName[1] == L'.' && fdW.cFileName[2] == L'\0'))) continue;
            char fnUtf8[MAX_PATH*4]={};
            WideCharToMultiByte(CP_UTF8,0,fdW.cFileName,-1,fnUtf8,sizeof(fnUtf8),nullptr,nullptr);
            std::string childAbs = folderPath + "\\" + fnUtf8;
            if (fdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                zipAddDir(zb, folderPath, fnUtf8, 1);
            } else {
                zb.addFile(fnUtf8, childAbs);
            }
        } while (FindNextFileW(h, &fdW));
        FindClose(h);
    }
    auto zipData = zb.finish();

    std::string safeZipName;
    for (unsigned char c : zipName) {
        if (c >= 32 && c < 127 && c != '"' && c != '\\') safeZipName += (char)c;
        else safeZipName += '_';
    }
    if (safeZipName.empty()) safeZipName = "folder";
    std::ostringstream hdr;
    hdr << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/zip\r\n"
        << "Content-Disposition: attachment; filename=\"" << safeZipName << ".zip\"\r\n"
        << "Content-Length: " << zipData.size() << "\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n\r\n";
    std::string hdrStr = hdr.str();
    send(s, hdrStr.data(), (int)hdrStr.size(), 0);
    send(s, (const char*)zipData.data(), (int)zipData.size(), 0);
}

// ────────────────────────────────────────────────────────────────
//  INLINE PREVIEW SENDER (sends file with correct Content-Type, inline CD)
// ────────────────────────────────────────────────────────────────
static std::string mimeForExt(const std::string& ext) {
    if(ext=="jpg"||ext=="jpeg") return "image/jpeg";
    if(ext=="png")  return "image/png";
    if(ext=="gif")  return "image/gif";
    if(ext=="webp") return "image/webp";
    if(ext=="bmp")  return "image/bmp";
    if(ext=="svg")  return "image/svg+xml";
    if(ext=="avif") return "image/avif";
    if(ext=="mp4")  return "video/mp4";
    if(ext=="webm") return "video/webm";
    if(ext=="ogv")  return "video/ogg";
    if(ext=="mov")  return "video/quicktime";
    if(ext=="mp3")  return "audio/mpeg";
    if(ext=="wav")  return "audio/wav";
    if(ext=="ogg")  return "audio/ogg";
    if(ext=="flac") return "audio/flac";
    if(ext=="m4a")  return "audio/mp4";
    if(ext=="aac")  return "audio/aac";
    if(ext=="opus") return "audio/opus";
    if(ext=="aiff"||ext=="aif") return "audio/aiff";
    // text types
    if(ext=="html"||ext=="htm") return "text/html; charset=utf-8";
    if(ext=="css") return "text/css";
    if(ext=="js")  return "application/javascript";
    if(ext=="json") return "application/json";
    if(ext=="xml") return "application/xml";
    if(ext=="svg") return "image/svg+xml";
    // fallback text
    return "text/plain; charset=utf-8";
}

static void sendFileInline(SOCKET s, const std::string& filePath, const std::string& name) {
    HANDLE hf = CreateFileW(toWide(filePath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) { sendResp(s, 404, "text/plain", "Not found"); return; }
    LARGE_INTEGER sz; GetFileSizeEx(hf, &sz);

    // Limit inline preview to 20MB
    const int64_t LIMIT = 20LL*1024*1024;
    std::string mime;
    {
        size_t dot = name.rfind('.');
        std::string ext = (dot != std::string::npos) ? name.substr(dot+1) : "";
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        mime = mimeForExt(ext);
    }

    std::ostringstream hdr;
    hdr << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << mime << "\r\n"
        << "Content-Disposition: inline\r\n"
        << "Content-Length: " << std::min(sz.QuadPart, LIMIT) << "\r\n"
        << "Cache-Control: max-age=60\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n\r\n";
    std::string h = hdr.str();
    send(s, h.data(), (int)h.size(), 0);

    char chunk[65536];
    DWORD readBytes = 0;
    int64_t remaining = std::min(sz.QuadPart, LIMIT);
    while (remaining > 0) {
        DWORD toRead = (DWORD)std::min((int64_t)sizeof(chunk), remaining);
        if (!ReadFile(hf, chunk, toRead, &readBytes, nullptr) || readBytes == 0) break;
        int sent = 0;
        while (sent < (int)readBytes) {
            int r = send(s, chunk+sent, (int)readBytes-sent, 0);
            if (r <= 0) goto done_inline;
            sent += r;
        }
        remaining -= readBytes;
    }
done_inline:
    CloseHandle(hf);
}

// ────────────────────────────────────────────────────────────────
//  ARCHIVE BROWSER (ZIP only)
// ────────────────────────────────────────────────────────────────
static std::string browseZip(const std::string& filePath, const std::string& pw) {
    // Read entire zip into memory
    HANDLE hf = CreateFileW(toWide(filePath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return "{\"error\":\"Cannot open file\"}";
    LARGE_INTEGER sz; GetFileSizeEx(hf, &sz);
    if (sz.QuadPart > 256LL*1024*1024) { CloseHandle(hf); return "{\"error\":\"Archive too large to browse\"}"; }
    std::vector<uint8_t> buf((size_t)sz.QuadPart);
    DWORD r = 0; ReadFile(hf, buf.data(), (DWORD)buf.size(), &r, nullptr);
    CloseHandle(hf);

    // Parse local file headers (store-only, basic)
    std::string json = "{\"entries\":[";
    bool first = true;
    size_t i = 0;
    while (i + 30 <= buf.size()) {
        // Local file header signature
        if (buf[i]==0x50&&buf[i+1]==0x4B&&buf[i+2]==0x03&&buf[i+3]==0x04) {
            uint16_t flags  = buf[i+6]|(buf[i+7]<<8);
            uint32_t compSz = buf[i+18]|(buf[i+19]<<8)|((uint32_t)buf[i+20]<<16)|((uint32_t)buf[i+21]<<24);
            uint32_t uncompSz = buf[i+22]|(buf[i+23]<<8)|((uint32_t)buf[i+24]<<16)|((uint32_t)buf[i+25]<<24);
            uint16_t fnLen  = buf[i+26]|(buf[i+27]<<8);
            uint16_t extLen = buf[i+28]|(buf[i+29]<<8);
            if (i+30+fnLen > buf.size()) break;
            // Check for encryption flag
            if (flags & 0x01) return "{\"password_protected\":true}";
            std::string name(buf.begin()+i+30, buf.begin()+i+30+fnLen);
            bool isDir = (!name.empty() && name.back()=='/');
            if (!first) json += ",";
            json += "{\"name\":\"" + jsonEscape(name) + "\",\"size\":" + std::to_string(isDir?0:uncompSz) + ",\"isDir\":" + (isDir?"true":"false") + "}";
            first = false;
            i = i + 30 + fnLen + extLen + compSz;
        } else if (buf[i]==0x50&&buf[i+1]==0x4B&&buf[i+2]==0x01&&buf[i+3]==0x02) {
            break; // Central directory — stop
        } else {
            i++;
        }
    }
    json += "]}";
    return json;
}


struct UploadedFile { std::string name; uint64_t size = 0; std::string ts; std::string savedPath; };

static std::vector<UploadedFile> handleUpload(const std::string& reqHeaders,
                                               const std::vector<uint8_t>& body,
                                               const std::string& savingDir,
                                               const std::string& clientIP) {
    std::vector<UploadedFile> results;
    std::string ct = getHeaderVal(reqHeaders, "Content-Type");
    size_t bpos = ct.find("boundary=");
    if (bpos == std::string::npos) { Log(L_ERR, "No boundary in Content-Type"); return results; }
    std::string boundary = "--" + trim(ct.substr(bpos + 9));
    std::string endBound = boundary + "--";
    std::string metadataJson;

    size_t pos = 0;
    while (true) {
        size_t dpos = findSeq(body, pos, boundary);
        if (dpos == std::string::npos) break;
        dpos += boundary.size();
        if (dpos + 2 <= body.size() && body[dpos] == '\r' && body[dpos+1] == '\n') dpos += 2;
        else if (dpos < body.size() && body[dpos] == '-') break;

        std::string hdrsep = "\r\n\r\n";
        size_t hend = findSeq(body, dpos, hdrsep);
        if (hend == std::string::npos) break;
        std::string partHdrs(body.begin() + dpos, body.begin() + hend);
        size_t contentStart = hend + 4;

        size_t nextBound = findSeq(body, contentStart, "\r\n" + boundary);
        if (nextBound == std::string::npos) {
            nextBound = findSeq(body, contentStart, "\r\n" + endBound);
            if (nextBound == std::string::npos) break;
        }
        size_t contentEnd = nextBound;

        std::string cd = getHeaderVal(partHdrs, "Content-Disposition");
        std::string filename = extractFilename(cd);

        if (filename.empty()) {
            if (cd.find("name=\"metadata\"") != std::string::npos) {
                metadataJson.assign(body.begin() + contentStart, body.begin() + contentEnd);
            }
        } else {
            std::string safe = sanitizeFilename(filename);
            std::string outPath = uniquePath(savingDir, safe);
            uint64_t sz = contentEnd - contentStart;

            // ── Storage space check ──
            // If a cap is set: reject if db_used + this file would exceed it.
            // Otherwise:       reject if this file alone exceeds the disk headroom allowed.
            {
                uint64_t cap = g_storageLimitBytes.load();
                if (cap > 0) {
                    uint64_t dbUsed = computeDbUsedBytes();
                    if (dbUsed + sz > cap) {
                        Log(L_ERR, "  REJECTED " + safe + " (" + formatBytes(sz) +
                            "): would exceed storage cap of " + formatBytes(cap) +
                            " (currently used: " + formatBytes(dbUsed) + ")");
                        pos = nextBound + 2;
                        continue;
                    }
                } else {
                    uint64_t maxAllowed = getEffectiveUploadLimitBytes();
                    if (maxAllowed > 0 && sz > maxAllowed) {
                        Log(L_ERR, "  REJECTED " + safe + " (" + formatBytes(sz) +
                            "): exceeds available headroom of " + formatBytes(maxAllowed));
                        pos = nextBound + 2;
                        continue;
                    }
                }
            }

            Log(L_NET, "  Saving: " + safe + " (" + formatBytes(sz) + ")");

            // Use CreateFileW so UTF-8 encoded filename (fullwidth chars) is stored correctly
            HANDLE hf = CreateFileW(toWide(outPath).c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf == INVALID_HANDLE_VALUE) {
                Log(L_ERR, "  Cannot create file: " + outPath);
            } else {
                const size_t CHUNK = 4 * 1024 * 1024;
                size_t off = 0;
                while (off < sz) {
                    size_t toWrite = std::min(CHUNK, sz - off);
                    DWORD w = 0;
                    WriteFile(hf, body.data() + contentStart + off, (DWORD)toWrite, &w, nullptr);
                    off += w;
                }
                CloseHandle(hf);

                if (!metadataJson.empty()) {
                    int64_t lm = extractJsonInt64(metadataJson, filename, "lastModified");
                    if (lm > 0) setFileTimes(outPath, lm);
                }

                g_totalBytes += sz;
                g_fileCount++;

                UploadedFile uf;
                uf.name      = filename; // original name — returned to client for upload card display
                uf.size      = sz;
                uf.ts        = nowHuman();
                uf.savedPath = outPath;
                results.push_back(uf);

                // Add to in-memory file list (use original name)
                { std::lock_guard<std::mutex> lk(g_filesMtx);
                  FileRecord fr; fr.name=filename; fr.savedPath=outPath;
                  fr.size=sz; fr.timestamp=uf.ts; fr.from=clientIP;
                  g_files.push_back(fr); }

                // Add to database — name matches the actual filename on disk (encoded)
                DbEntry de;
                de.id        = generateId();
                de.name      = safe;     // encoded name, same as the file on disk
                de.savedPath = outPath;
                de.size      = sz;
                de.timestamp = uf.ts;
                de.from      = clientIP;
                dbAddEntry(de);

                Log(L_OK, "  Saved: " + outPath);
            }
        }
        pos = nextBound + 2;
    }
    return results;
}
// ────────────────────────────────────────────────────────────────
//  CLIENT HANDLER
// ────────────────────────────────────────────────────────────────

static void handleClient(SOCKET s, std::string clientIP) {
    ++g_activeClients;

    // Log connect only when this is the first connection from this IP
    {
        std::lock_guard<std::mutex> lk(g_ipMtx);
        if (++g_ipCount[clientIP] == 1)
            Log(L_VERB, "Device connected: " + clientIP);
    }

    DWORD tv = 30000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

    std::string hdrs;
    std::vector<uint8_t> body;

    if (!recvAll(s, hdrs, body)) {
        // Silent on poll failures
        closesocket(s);
        --g_activeClients;
        std::lock_guard<std::mutex> lk(g_ipMtx);
        if (--g_ipCount[clientIP] == 0) { g_ipCount.erase(clientIP); Log(L_VERB, "Device disconnected: " + clientIP); }
        return;
    }

    std::string firstLine = hdrs.substr(0, hdrs.find('\n'));
    std::string method, path;
    { std::istringstream ss(firstLine); ss >> method >> path; }

    // Strip query string for route matching
    std::string route = path;
    size_t qpos = route.find('?');
    if (qpos != std::string::npos) route = route.substr(0, qpos);

    Log(L_VERB, clientIP + "  " + method + " " + path);

    std::string savingDir = getSavingDir();

    if (route == "/" || route == "/index.html") {
        sendResp(s, 200, "text/html; charset=utf-8", HTML_PAGE);

    } else if (route == "/api/info") {
        auto ips = getLocalIPs();
        std::string json = "{\"ips\":[";
        for (size_t i = 0; i < ips.size(); ++i) {
            json += "\"" + ips[i] + "\"";
            if (i + 1 < ips.size()) json += ",";
        }
        json += "],\"saving_dir\":\"" + jsonEscape(savingDir) + "\"}";
        sendResp(s, 200, "application/json", json);

    } else if (route == "/api/stats") {
        char buf[256];
        sprintf_s(buf, "{\"files\":%llu,\"bytes\":%llu,\"clients\":%d}",
            g_fileCount.load(), g_totalBytes.load(), g_activeClients.load() - 1);
        sendResp(s, 200, "application/json", std::string(buf));

    } else if (route == "/api/disk_space") {
        std::string dbPath = getDbPath();
        uint64_t freeBytes  = getDiskFreeBytes(dbPath);
        uint64_t totalBytes = getDiskTotalBytes(dbPath);
        uint64_t capBytes   = g_storageLimitBytes.load();
        uint64_t pct        = g_uploadLimitPct.load();
        uint64_t maxUpload  = getEffectiveUploadLimitBytes();
        uint64_t dbUsed     = computeDbUsedBytes();   // sum of database file sizes
        std::string root    = getDiskRoot(dbPath);
        char buf2[512];
        sprintf_s(buf2,
            "{\"disk_free\":%llu,\"disk_total\":%llu,\"storage_cap\":%llu,"
            "\"upload_limit_pct\":%llu,\"max_upload\":%llu,\"disk_root\":\"%s\","
            "\"db_used\":%llu}",
            freeBytes, totalBytes, capBytes, pct, maxUpload, jsonEscape(root).c_str(), dbUsed);
        sendResp(s, 200, "application/json", std::string(buf2));

    } else if (route == "/events") {
        // Server-Sent Events — keep connection open
        std::string hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n";
        send(s, hdr.data(), (int)hdr.size(), 0);

        // Register this client
        { std::lock_guard<std::mutex> lk(g_sseMtx); g_sseClients.push_back(s); }
        Log(L_VERB, "SSE client registered: " + clientIP);

        // Send initial state
        {
            std::string dbJson = buildDbJson();
            std::string dbMsg = "event: db_update\ndata: " + dbJson + "\n\n";
            send(s, dbMsg.data(), (int)dbMsg.size(), 0);
        }
        {
            std::string pc;
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); pc = g_pastebin; }
            std::string pMsg = "event: pastebin_update\ndata: {\"content\":\"" + jsonEscape(pc) + "\"}\n\n";
            send(s, pMsg.data(), (int)pMsg.size(), 0);
        }

        // Keep thread alive; the socket is now in g_sseClients.
        // Block here by trying to recv (will return when client disconnects or times out)
        // Set a longer recv timeout for SSE clients
        DWORD sseTimeout = 120000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&sseTimeout, sizeof(sseTimeout));
        char dummy[8];
        while (g_running) {
            int r = recv(s, dummy, sizeof(dummy), 0);
            if (r <= 0) break;
        }
        // Remove from SSE clients
        { std::lock_guard<std::mutex> lk(g_sseMtx);
          g_sseClients.erase(std::remove(g_sseClients.begin(), g_sseClients.end(), s), g_sseClients.end()); }
        Log(L_VERB, "SSE client disconnected: " + clientIP);
        closesocket(s);
        --g_activeClients;
        {
            std::lock_guard<std::mutex> lk(g_ipMtx);
            if (--g_ipCount[clientIP] == 0) { g_ipCount.erase(clientIP); Log(L_VERB, "Device disconnected: " + clientIP); }
        }
        return;

    } else if (route == "/api/pastebin") {
        if (method == "GET") {
            std::string content;
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); content = g_pastebin; }
            sendResp(s, 200, "application/json",
                     "{\"content\":\"" + jsonEscape(content) + "\"}");
        } else if (method == "POST") {
            std::string bodyStr(body.begin(), body.end());
            std::string newContent = jsGetStr(bodyStr, "content");
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); g_pastebin = newContent; }
            saveConfigPastebin(newContent);
            sseBroadcastPastebin();
            sendResp(s, 200, "application/json", "{\"success\":true}");
        } else {
            sendResp(s, 400, "text/plain", "Method not allowed");
        }

    } else if (route == "/api/database") {
        sendResp(s, 200, "application/json", buildDbJson());

    } else if (route == "/api/database/delete" && method == "POST") {
        std::string bodyStr(body.begin(), body.end());
        std::string id = jsGetStr(bodyStr, "id");
        bool deleteFile = true;
        {
            size_t dfp = bodyStr.find("\"delete_file\":");
            if (dfp != std::string::npos) {
                std::string rest = bodyStr.substr(dfp + 14);
                rest = trim(rest);
                if (rest.rfind("false",0)==0 || rest.rfind("0",0)==0) deleteFile = false;
            }
        }
        if (id.empty()) {
            sendResp(s, 400, "application/json", "{\"success\":false,\"error\":\"No id provided\"}");
        } else {
            std::string pathToDelete;
            {
                std::lock_guard<std::mutex> lk(g_dbMtx);
                auto it = std::find_if(g_database.begin(), g_database.end(),
                    [&](const DbEntry& e){ return e.id == id; });
                if (it != g_database.end()) {
                    pathToDelete = it->savedPath;
                    g_database.erase(it);
                }
            }
            if (!pathToDelete.empty()) {
                dbSave();
                if (deleteFile) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, pathToDelete.c_str(), -1, nullptr, 0);
                    std::wstring wp(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, pathToDelete.c_str(), -1, &wp[0], wlen);
                    DeleteFileW(wp.c_str());
                    Log(L_OK, "Deleted: " + pathToDelete);
                }
                sseBroadcastDb();
                sendResp(s, 200, "application/json", "{\"success\":true}");
            } else {
                sendResp(s, 404, "application/json", "{\"success\":false,\"error\":\"ID not found\"}");
            }
        }

    } else if (route == "/download") {
        std::string id = queryParam(path, "id");
        if (id.empty()) { sendResp(s, 400, "text/plain", "Missing id"); }
        else {
            std::string filePath, encodedName;
            {
                std::lock_guard<std::mutex> lk(g_dbMtx);
                for (auto& e : g_database) {
                    if (e.id == id) { filePath = e.savedPath; encodedName = e.name; break; }
                }
            }
            if (filePath.empty()) { sendResp(s, 404, "text/plain", "File not found in database"); }
            else {
                // Pass the encoded disk name — Content-Disposition carries the disk name.
                // The client's download= attribute (set to the decoded original) takes
                // precedence for UI-triggered same-origin downloads.
                Log(L_NET, "Download: " + decodeFilenameFromDisk(encodedName) + " → " + clientIP);
                sendFileDownload(s, filePath, encodedName);
            }
        }

    } else if (route == "/download_ff") {
        // Download a file from a forwarding folder by path
        std::string encodedPath = queryParam(path, "path");
        if (encodedPath.empty()) { sendResp(s, 400, "text/plain", "Missing path"); }
        else {
            // Verify it's actually in a forwarding folder
            bool allowed = false;
            {
                std::lock_guard<std::mutex> lk(g_ffMtx);
                for (auto& ff : g_ffFolders) {
                    for (auto& fp : ff.contents) {
                        if (fp == encodedPath) { allowed = true; break; }
                    }
                    if (allowed) break;
                }
            }
            if (!allowed) { sendResp(s, 403, "text/plain", "File not in any forwarding folder"); }
            else {
                // Extract basename (encoded on disk); pass to sendFileDownload as-is.
                // The client's download= attribute carries the decoded original name.
                size_t sl = encodedPath.rfind('\\');
                if (sl == std::string::npos) sl = encodedPath.rfind('/');
                std::string encodedBasename = (sl != std::string::npos) ? encodedPath.substr(sl+1) : encodedPath;
                Log(L_NET, "FF Download: " + decodeFilenameFromDisk(encodedBasename) + " → " + clientIP);
                sendFileDownload(s, encodedPath, encodedBasename);
            }
        }

    } else if (route == "/preview_inline") {
        // Serve a database file inline for preview
        std::string id = queryParam(path, "id");
        if (id.empty()) { sendResp(s, 400, "text/plain", "Missing id"); }
        else {
            std::string filePath, name;
            { std::lock_guard<std::mutex> lk(g_dbMtx);
              for (auto& e : g_database) if (e.id == id) { filePath=e.savedPath; name=e.name; break; } }
            if (filePath.empty()) sendResp(s, 404, "text/plain", "Not found");
            else sendFileInline(s, filePath, decodeFilenameFromDisk(name));
        }

    } else if (route == "/preview_inline_ff") {
        // Serve a forwarding folder file inline
        std::string fp = queryParam(path, "path");
        if (fp.empty()) { sendResp(s, 400, "text/plain", "Missing path"); }
        else {
            bool allowed = false;
            { std::lock_guard<std::mutex> lk(g_ffMtx);
              for (auto& ff : g_ffFolders) {
                  for (auto& c : ff.contents) if (c == fp) { allowed=true; break; }
                  if (!allowed && ff.subfoldersEnabled) {
                      // Also check if it's a subpath of this ff
                      if (fp.rfind(ff.path, 0) == 0) { allowed=true; }
                  }
                  if (allowed) break;
              } }
            if (!allowed) sendResp(s, 403, "text/plain", "Not in any forwarding folder");
            else {
                size_t sl = fp.rfind('\\');
                if (sl == std::string::npos) sl = fp.rfind('/');
                std::string bn = (sl != std::string::npos) ? fp.substr(sl+1) : fp;
                sendFileInline(s, fp, decodeFilenameFromDisk(bn));
            }
        }

    } else if (route == "/download_ff_zip") {
        // Download entire forwarding folder (or subdir) as zip
        std::string id = queryParam(path, "id");
        std::string subpath = queryParam(path, "path");
        if (!id.empty()) {
            std::string ffPath, ffId;
            { std::lock_guard<std::mutex> lk(g_ffMtx);
              for (auto& ff : g_ffFolders) if (ff.id == id) { ffPath=ff.path; ffId=ff.id; break; } }
            if (ffPath.empty()) sendResp(s, 404, "text/plain", "Forwarding folder not found");
            else {
                size_t sl = ffPath.rfind('\\');
                std::string folderName = (sl != std::string::npos) ? ffPath.substr(sl+1) : ffPath;
                Log(L_NET, "ZIP download FF [" + ffId + "]: " + ffPath + " → " + clientIP);
                sendZip(s, ffPath, folderName);
            }
        } else if (!subpath.empty()) {
            // Check it's in an FF
            bool allowed = false;
            { std::lock_guard<std::mutex> lk(g_ffMtx);
              for (auto& ff : g_ffFolders) {
                  if (ff.subfoldersEnabled && subpath.rfind(ff.path, 0) == 0) { allowed=true; break; }
              } }
            if (!allowed) sendResp(s, 403, "text/plain", "Not in any forwarding folder");
            else {
                size_t sl = subpath.rfind('\\');
                std::string folderName = (sl != std::string::npos) ? subpath.substr(sl+1) : subpath;
                DWORD attrs = GetFileAttributesW(toWide(subpath).c_str());
                if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
                    sendResp(s, 404, "text/plain", "Directory not found");
                else { Log(L_NET, "ZIP subdir: " + subpath + " → " + clientIP); sendZip(s, subpath, folderName); }
            }
        } else {
            sendResp(s, 400, "text/plain", "Missing id or path");
        }

    } else if (route == "/api/archive_browse") {
        std::string src = queryParam(path, "src");
        std::string pw  = queryParam(path, "pw");
        std::string filePath, name;
        if (src == "db") {
            std::string id = queryParam(path, "id");
            { std::lock_guard<std::mutex> lk(g_dbMtx);
              for (auto& e : g_database) if (e.id == id) { filePath=e.savedPath; name=e.name; break; } }
        } else {
            filePath = queryParam(path, "path");
            size_t sl = filePath.rfind('\\');
            if (sl == std::string::npos) sl = filePath.rfind('/');
            name = (sl != std::string::npos) ? filePath.substr(sl+1) : filePath;
        }
        if (filePath.empty()) sendResp(s, 404, "application/json", "{\"error\":\"Not found\"}");
        else {
            std::string ext = name;
            size_t dot = ext.rfind('.');
            if (dot != std::string::npos) ext = ext.substr(dot+1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == "zip") sendResp(s, 200, "application/json", browseZip(filePath, pw));
            else sendResp(s, 200, "application/json", "{\"error\":\"Unsupported archive type\"}");
        }

    } else if (route == "/upload" && method == "POST") {
        Log(L_INFO, "Upload from " + clientIP + " (" + formatBytes(body.size()) + ")");
        auto uploaded = handleUpload(hdrs, body, savingDir, clientIP);
        if (uploaded.empty()) {
            sendResp(s, 400, "application/json", "{\"success\":false,\"error\":\"No files parsed\"}");
        } else {
            std::string json = "{\"success\":true,\"saving_dir\":\"" + jsonEscape(savingDir) + "\",\"files\":[";
            for (size_t i = 0; i < uploaded.size(); ++i) {
                auto& f = uploaded[i];
                json += "{\"name\":\"" + jsonEscape(f.name) + "\",\"size\":" +
                        std::to_string(f.size) + ",\"ts\":\"" + f.ts + "\"}";
                if (i + 1 < uploaded.size()) json += ",";
            }
            json += "]}";
            sendResp(s, 200, "application/json", json);
            Log(L_OK, std::to_string(uploaded.size()) + " file(s) saved from " + clientIP);
            sseBroadcastDb();
        }
    } else {
        sendResp(s, 404, "text/plain", "404 Not Found");
    }

    closesocket(s);
    --g_activeClients;
    {
        std::lock_guard<std::mutex> lk(g_ipMtx);
        if (--g_ipCount[clientIP] == 0) { g_ipCount.erase(clientIP); Log(L_VERB, "Device disconnected: " + clientIP); }
    }
}
// ────────────────────────────────────────────────────────────────
//  PORT DETECTION + SERVER
// ────────────────────────────────────────────────────────────────

bool portAvailable(int port) {
    SOCKET ts = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ts == INVALID_SOCKET) return false;
    BOOL reuse = TRUE;
    setsockopt(ts, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);
    int r = bind(ts, (sockaddr*)&addr, sizeof(addr));
    closesocket(ts);
    return r == 0;
}

int findOpenPort() {
    std::vector<int> preferred = {8080, 8081, 8082, 8083, 8084, 8085, 9090, 9091, 7070, 7777, 3000, 4000, 5000};
    for (int p : preferred) {
        if (portAvailable(p)) { Log(L_OK, "Port " + std::to_string(p) + " is available"); return p; }
        Log(L_VERB, "Port " + std::to_string(p) + " in use, trying next...");
    }
    for (int p = 49152; p < 65535; ++p) {
        if (portAvailable(p)) { Log(L_WARN, "Using fallback port " + std::to_string(p)); return p; }
    }
    return -1;
}

void serverThread(int port) {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) { Log(L_ERR, "socket() failed"); g_running = false; return; }

    BOOL reuse = TRUE;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    int sndbuf = 256*1024, rcvbuf = 256*1024;
    setsockopt(srv, SOL_SOCKET, SO_SNDBUF, (char*)&sndbuf, sizeof(sndbuf));
    setsockopt(srv, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, sizeof(rcvbuf));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0) {
        Log(L_ERR, "bind() failed: " + std::to_string(WSAGetLastError()));
        closesocket(srv); g_running = false; return;
    }
    if (listen(srv, SOMAXCONN) != 0) {
        Log(L_ERR, "listen() failed");
        closesocket(srv); g_running = false; return;
    }
    Log(L_OK, "Server listening on 0.0.0.0:" + std::to_string(port));

    // NOTE: Do NOT set SO_RCVTIMEO here — that affects recv(), not accept().
    // We use select() with a 1-second timeout before each accept() so that
    // the loop checks g_running every second and exits cleanly on /quit.

    while (g_running) {
        // Wait up to 1 second for an incoming connection
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(srv, &readfds);
        struct timeval tv = {1, 0};
        int sel = select(0, &readfds, nullptr, nullptr, &tv);
        if (sel == 0) continue;                   // timeout — re-check g_running
        if (sel == SOCKET_ERROR) {
            if (g_running) Log(L_WARN, "select() error " + std::to_string(WSAGetLastError()));
            continue;
        }

        sockaddr_in ca{};
        int al = sizeof(ca);
        SOCKET cs = accept(srv, (sockaddr*)&ca, &al);
        if (cs == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (g_running) Log(L_WARN, "accept() error " + std::to_string(err));
            continue;
        }
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &ca.sin_addr, ip, sizeof(ip));
        std::thread(handleClient, cs, std::string(ip)).detach();
    }
    closesocket(srv);
    Log(L_INFO, "Server thread stopped.");
}