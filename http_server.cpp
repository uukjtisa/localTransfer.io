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
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>localTransfer.io</title>
<style>

/* ══════════════════════════════════════════════════════════════
   THEME TOKENS — three themes, one variable contract
   ══════════════════════════════════════════════════════════════ */
:root,
[data-theme="midnight"]{
  --bg:#0b0e14;        --bg2:#0f131b;
  --surface:#151a23;   --surface2:#1b212c;  --surface3:#222937;
  --border:#242c3a;    --border2:#323c4d;   --border3:#3f4b60;
  --txt:#e3e8ef;       --txt2:#9aa5b6;      --txt3:#6b7688;  --dim:#4a5464;
  --accent:#3fb950;    --accent2:#2ea043;   --accent-soft:rgba(63,185,80,.10); --accent-ring:rgba(63,185,80,.28);
  --blue:#58a6ff;      --blue-soft:rgba(88,166,255,.10);
  --warn:#d29922;      --err:#f85149;       --err-soft:rgba(248,81,73,.10);
  --on-accent:#06120a;
  --shadow:0 12px 40px -8px rgba(0,0,0,.7);
  --shadow-sm:0 2px 10px -2px rgba(0,0,0,.5);
  --grain:.035;
}
[data-theme="slate"]{
  --bg:#12151a;        --bg2:#171b22;
  --surface:#1d222b;   --surface2:#242a35;  --surface3:#2c3340;
  --border:#2e3542;    --border2:#3c4555;   --border3:#4b5668;
  --txt:#e8ecf2;       --txt2:#a7b0be;      --txt3:#78828f;  --dim:#59626f;
  --accent:#7c9cf5;    --accent2:#6485e8;   --accent-soft:rgba(124,156,245,.12); --accent-ring:rgba(124,156,245,.30);
  --blue:#7c9cf5;      --blue-soft:rgba(124,156,245,.12);
  --warn:#e0a94a;      --err:#f2726a;       --err-soft:rgba(242,114,106,.12);
  --on-accent:#0a1024;
  --shadow:0 12px 40px -8px rgba(0,0,0,.6);
  --shadow-sm:0 2px 10px -2px rgba(0,0,0,.4);
  --grain:.025;
}
[data-theme="light"]{
  --bg:#f6f8fa;        --bg2:#ffffff;
  --surface:#ffffff;   --surface2:#f2f5f8;  --surface3:#e8edf2;
  --border:#dde3ea;    --border2:#c9d2dc;   --border3:#aeb9c6;
  --txt:#1c2128;       --txt2:#5b6673;      --txt3:#7d8894;  --dim:#9aa4b0;
  --accent:#1a7f37;    --accent2:#136a2c;   --accent-soft:rgba(26,127,55,.09);  --accent-ring:rgba(26,127,55,.22);
  --blue:#0969da;      --blue-soft:rgba(9,105,218,.09);
  --warn:#9a6700;      --err:#cf222e;       --err-soft:rgba(207,34,46,.09);
  --on-accent:#ffffff;
  --shadow:0 12px 32px -10px rgba(31,41,55,.22);
  --shadow-sm:0 1px 4px rgba(31,41,55,.10);
  --grain:0;
}
:root{
  --t-fast:.13s;  --t:.22s;  --t-slow:.36s;
  --ease:cubic-bezier(.32,.72,0,1);
  --ease-out:cubic-bezier(.16,1,.3,1);
  --r-sm:6px; --r:9px; --r-lg:14px; --r-xl:20px;
  --sb-w:380px;
}
@media (prefers-reduced-motion: reduce){
  :root{ --t-fast:0s; --t:0s; --t-slow:0s; }
  *,*::before,*::after{ animation-duration:.001s !important; animation-iteration-count:1 !important; transition-duration:.001s !important; }
}
body.no-motion{ --t-fast:0s; --t:0s; --t-slow:0s; }
body.no-motion *{ animation-duration:.001s !important; transition-duration:.001s !important; }

/* ══════════════════════════════════════════════════════════════ BASE */
*{margin:0;padding:0;box-sizing:border-box;}
html{-webkit-text-size-adjust:100%;}
body{background:var(--bg);color:var(--txt);font-family:'Inter',system-ui,sans-serif;font-size:15px;line-height:1.5;
  min-height:100vh;overflow-x:hidden;transition:background var(--t) var(--ease),color var(--t) var(--ease);}
.mono{font-family:'JetBrains Mono',ui-monospace,monospace;}
button,input,select,textarea{font-family:inherit;color:inherit;}
button{background:none;border:none;cursor:pointer;}
::selection{background:var(--accent-ring);color:var(--txt);}
*::-webkit-scrollbar{width:9px;height:9px;}
*::-webkit-scrollbar-track{background:transparent;}
*::-webkit-scrollbar-thumb{background:var(--border2);border-radius:5px;border:2px solid transparent;background-clip:padding-box;}
*::-webkit-scrollbar-thumb:hover{background:var(--border3);background-clip:padding-box;}
.grain{position:fixed;inset:0;pointer-events:none;z-index:9999;opacity:var(--grain);
  background-image:url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='.9' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)'/%3E%3C/svg%3E");}
.ic{width:15px;height:15px;flex-shrink:0;stroke:currentColor;fill:none;stroke-width:1.9;stroke-linecap:round;stroke-linejoin:round;}
.ic.sm{width:13px;height:13px;}
.ic.lg{width:19px;height:19px;}
.ic.f{fill:currentColor;stroke:none;}
:focus-visible{outline:2px solid var(--accent);outline-offset:2px;border-radius:4px;}

/* ══════════════════════════════════════════════════════════════ SHELL */
.shell{container-type:inline-size;container-name:app;width:100%;height:100vh;position:relative;background:var(--bg);
  display:flex;flex-direction:column;overflow:hidden;}

/* ══════════════════════════════════════════════════════════════ HEADER */
.hdr{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:0 20px;height:56px;
  border-bottom:1px solid var(--border);background:color-mix(in srgb,var(--bg) 82%,transparent);
  backdrop-filter:blur(16px) saturate(1.4);position:sticky;top:0;z-index:60;}
.brand{display:flex;align-items:center;gap:9px;font-weight:600;font-size:.95rem;letter-spacing:-.01em;white-space:nowrap;}
.brand-mark{width:29px;height:29px;border-radius:9px;display:grid;place-items:center;flex-shrink:0;
  background:var(--accent);color:var(--on-accent);box-shadow:0 2px 12px -3px var(--accent-ring);}
.brand-mark .ic{width:17px;height:17px;stroke-width:2.6;}
.brand b{font-weight:700;} .brand span{color:var(--accent);} .brand em{color:var(--txt3);font-style:normal;font-weight:400;}
.hdr-r{display:flex;align-items:center;gap:7px;}

/* ── LOGO CANDIDATES ── */
.brand-mark.mono{font-family:'JetBrains Mono',monospace;font-weight:700;font-size:1rem;letter-spacing:-.08em;
  text-indent:-.05em;line-height:1;color:var(--on-accent);}
/* inverse tile: dark chip, accent letters */
.brand-mark.ink{background:var(--surface3);border:1px solid var(--border2);color:var(--accent);box-shadow:none;}
.brand-mark.outline{background:none;border:1.8px solid var(--accent);color:var(--accent);box-shadow:none;}
.brand-mark.chev{font-family:'JetBrains Mono',monospace;font-weight:700;font-size:1.05rem;line-height:1;
  color:var(--on-accent);text-indent:-.06em;}
/* compact mono wordmark: lT.io */
.wm{font-family:'JetBrains Mono',monospace;font-weight:700;font-size:1.04rem;letter-spacing:-.045em;line-height:1;}
.wm span{color:var(--accent);}
/* bracketed CLI wordmark: [lT.io] */
.brk{font-family:'JetBrains Mono',monospace;font-weight:700;font-size:1.06rem;letter-spacing:-.03em;color:var(--txt);}
.brk i{color:var(--dim);font-style:normal;font-weight:400;}
/* terminal caret */
.caret{width:7px;height:15px;background:var(--accent);border-radius:1px;display:inline-block;margin-left:1px;
  animation:cblink 1.15s steps(1) infinite;}
@keyframes cblink{0%,50%{opacity:1;}50.01%,100%{opacity:0;}}
.seg{display:flex;gap:2px;padding:3px;background:var(--surface2);border:1px solid var(--border);border-radius:var(--r);}
.seg button{display:flex;align-items:center;gap:6px;padding:5px 11px;border-radius:6px;font-size:.78rem;font-weight:500;
  color:var(--txt2);transition:color var(--t-fast),background var(--t-fast);position:relative;white-space:nowrap;}
.seg button:hover{color:var(--txt);}
.seg button.on{background:var(--surface3);color:var(--accent);box-shadow:var(--shadow-sm);}
.ibtn{width:32px;height:32px;border-radius:8px;display:grid;place-items:center;color:var(--txt2);
  border:1px solid transparent;position:relative;overflow:hidden;
  transition:color var(--t-fast),background var(--t-fast),border-color var(--t-fast),transform var(--t-fast);}
.ibtn:hover{color:var(--txt);background:var(--surface2);border-color:var(--border);}
.ibtn:active{transform:scale(.92);}
.ibtn.on{color:var(--accent);background:var(--accent-soft);border-color:var(--accent-ring);}
.pill{display:inline-flex;align-items:center;gap:6px;height:28px;padding:0 11px;border-radius:99px;
  font-family:'JetBrains Mono',monospace;font-size:.63rem;font-weight:500;letter-spacing:.09em;
  border:1px solid var(--accent-ring);color:var(--accent);background:var(--accent-soft);}
.pill i{width:6px;height:6px;border-radius:50%;background:var(--accent);position:relative;}
.pill i::after{content:'';position:absolute;inset:-3px;border-radius:50%;border:1px solid var(--accent);
  animation:ripple 2.4s cubic-bezier(0,.6,.6,1) infinite;}
@keyframes ripple{0%{transform:scale(.55);opacity:1;}100%{transform:scale(1.9);opacity:0;}}

/* ══════════════════════════════════════════════════════════════ STATS */
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:1px;background:var(--border);border-bottom:1px solid var(--border);}
.stat{background:var(--bg2);padding:11px 20px;transition:background var(--t-fast);}
.stat:hover{background:var(--surface);}
.stat label{display:block;font-size:.58rem;letter-spacing:.13em;text-transform:uppercase;color:var(--txt3);
  font-family:'JetBrains Mono',monospace;margin-bottom:3px;font-weight:500;}
.stat b{font-family:'JetBrains Mono',monospace;font-size:.98rem;font-weight:500;color:var(--txt);letter-spacing:-.01em;}
.stat.acc b{color:var(--accent);}

/* ══════════════════════════════════════════════════════════════ LAYOUT */
.layout{display:flex;align-items:stretch;flex:1;min-height:0;}
main{flex:1;min-width:0;padding:26px 24px 90px;width:100%;overflow-y:auto;}
main>*{max-width:1100px;margin-left:auto;margin-right:auto;}
.sec-t{display:flex;align-items:center;gap:8px;font-size:.62rem;letter-spacing:.16em;text-transform:uppercase;
  color:var(--txt3);font-family:'JetBrains Mono',monospace;font-weight:600;margin-bottom:12px;}
.sec-t::after{content:'';flex:1;height:1px;background:var(--border);}
.urls{display:grid;grid-template-columns:repeat(auto-fill,minmax(212px,1fr));gap:9px;margin-bottom:22px;}
.url{display:flex;align-items:center;gap:10px;padding:11px 13px;border-radius:var(--r);background:var(--surface);
  border:1px solid var(--border);cursor:pointer;position:relative;overflow:hidden;
  transition:border-color var(--t-fast),transform var(--t-fast),box-shadow var(--t-fast);}
.url:hover{border-color:var(--accent-ring);transform:translateY(-1px);box-shadow:var(--shadow-sm);}
.url code{text-align:left;}
.url .ic{color:var(--accent);}
.url code{font-family:'JetBrains Mono',monospace;font-size:.7rem;letter-spacing:-.02em;color:var(--txt);flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.url .cp{font-size:.6rem;color:var(--txt3);font-family:'JetBrains Mono',monospace;opacity:0;transition:opacity var(--t-fast);}
.url:hover .cp{opacity:1;}
.storage{background:var(--surface);border:1px solid var(--border);border-radius:var(--r-lg);padding:16px 18px;margin-bottom:22px;}
.storage-top{display:flex;align-items:baseline;justify-content:space-between;gap:10px;margin-bottom:10px;flex-wrap:wrap;}
.storage-top b{font-family:'JetBrains Mono',monospace;font-size:1.15rem;font-weight:500;letter-spacing:-.02em;}
.storage-top span{font-family:'JetBrains Mono',monospace;font-size:.7rem;color:var(--txt3);}
.track{height:6px;border-radius:99px;background:var(--surface3);overflow:hidden;position:relative;}
.track>i{display:block;height:100%;border-radius:99px;width:0;background:linear-gradient(90deg,var(--accent2),var(--accent));
  transition:width 1.1s var(--ease-out);position:relative;overflow:hidden;}
.track>i::after{content:'';position:absolute;inset:0;
  background:linear-gradient(90deg,transparent,rgba(255,255,255,.28),transparent);animation:sheen 2.6s ease-in-out infinite;}
@keyframes sheen{0%{transform:translateX(-100%);}55%,100%{transform:translateX(320%);}}
.storage-legend{display:flex;gap:16px;margin-top:10px;font-size:.66rem;color:var(--txt3);font-family:'JetBrains Mono',monospace;flex-wrap:wrap;}
.storage-legend span{display:flex;align-items:center;gap:5px;}
.dot{width:7px;height:7px;border-radius:2px;background:var(--accent);}
.dot.b{background:var(--surface3);}

/* ══════════════════════════════════════════════════════════════ DROPZONE */
.dz{position:relative;border-radius:var(--r-lg);padding:38px 26px;text-align:center;cursor:pointer;
  background:var(--surface);overflow:hidden;border:1px dashed var(--border2);
  transition:border-color var(--t),background var(--t),transform var(--t-fast);}
.dz::before{content:'';position:absolute;inset:0;opacity:0;transition:opacity var(--t);
  background:radial-gradient(ellipse 70% 100% at 50% 0%,var(--accent-soft),transparent 70%);}
.dz:hover{border-color:var(--accent-ring);} .dz:hover::before{opacity:1;}
.dz.over{border-color:var(--accent);border-style:solid;background:var(--surface2);transform:scale(1.008);}
.dz.over::before{opacity:1;}
.dz-ico{width:52px;height:52px;margin:0 auto 14px;border-radius:16px;display:grid;place-items:center;
  background:var(--accent-soft);border:1px solid var(--accent-ring);color:var(--accent);transition:transform var(--t) var(--ease-out);}
.dz:hover .dz-ico{transform:translateY(-3px) scale(1.04);}
.dz.over .dz-ico{animation:bob .9s ease-in-out infinite;}
@keyframes bob{0%,100%{transform:translateY(-2px);}50%{transform:translateY(-8px);}}
.dz-ico .ic{width:24px;height:24px;stroke-width:2;}
.dz h3{font-size:1.05rem;font-weight:600;margin-bottom:5px;letter-spacing:-.015em;}
.dz p{font-size:.76rem;color:var(--txt3);font-family:'JetBrains Mono',monospace;}
.dz-cta{margin-top:16px;display:flex;gap:9px;justify-content:center;flex-wrap:wrap;}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:7px;height:38px;padding:0 18px;border-radius:var(--r);
  font-size:.83rem;font-weight:600;border:1px solid var(--border2);color:var(--txt);background:var(--surface2);
  position:relative;overflow:hidden;
  transition:background var(--t-fast),border-color var(--t-fast),transform var(--t-fast),box-shadow var(--t-fast);}
.btn:hover{background:var(--surface3);border-color:var(--border3);}
.btn:active{transform:scale(.97);}
.btn.pri{background:var(--accent);border-color:var(--accent);color:#fff;}
.btn.pri:hover{background:var(--accent2);border-color:var(--accent2);box-shadow:0 6px 20px -6px var(--accent-ring);}
.btn.sm{height:31px;padding:0 12px;font-size:.75rem;}
.btn.danger{color:var(--err);border-color:var(--err-soft);}
.btn.danger:hover{background:var(--err-soft);border-color:var(--err);}
.rip{position:absolute;border-radius:50%;transform:scale(0);pointer-events:none;background:currentColor;opacity:.22;
  animation:rip .55s var(--ease-out) forwards;}
@keyframes rip{to{transform:scale(2.6);opacity:0;}}

/* ══════════════════════════════════════════════════════════════ PROGRESS */
.prog{display:none;margin-top:16px;padding:15px 17px;border-radius:var(--r-lg);background:var(--surface);
  border:1px solid var(--border);align-items:center;gap:15px;opacity:1;animation:pop var(--t-slow) var(--ease-out);}
.prog.on{display:flex;}
@keyframes pop{from{opacity:0;transform:translateY(8px) scale(.98);}to{opacity:1;transform:none;}}
.ring{position:relative;width:50px;height:50px;flex-shrink:0;}
.ring svg{transform:rotate(-90deg);width:50px;height:50px;}
.ring circle{fill:none;stroke-width:4;stroke-linecap:round;}
.ring .bgc{stroke:var(--surface3);}
.ring .fgc{stroke:var(--accent);stroke-dasharray:132;stroke-dashoffset:132;transition:stroke-dashoffset .25s linear;}
.ring b{position:absolute;inset:0;display:grid;place-items:center;font-family:'JetBrains Mono',monospace;font-size:.68rem;font-weight:600;}
.prog-body{flex:1;min-width:0;}
.prog-body h4{font-size:.82rem;font-weight:600;margin-bottom:3px;}
.prog-body p{font-family:'JetBrains Mono',monospace;font-size:.66rem;color:var(--txt3);}

/* ══════════════════════════════════════════════════════════════ SENT */
.sent{margin-top:28px;}
.sent-item{display:flex;align-items:center;gap:12px;padding:11px 14px;border-radius:var(--r);background:var(--surface);
  border:1px solid var(--border);margin-bottom:7px;opacity:1;animation:slidein var(--t-slow) var(--ease-out);}
@keyframes slidein{from{opacity:0;transform:translateY(-10px);}to{opacity:1;transform:none;}}
.sent-item .m{flex:1;min-width:0;}
.sent-item .m b{display:block;font-size:.84rem;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.sent-item .m span{font-family:'JetBrains Mono',monospace;font-size:.63rem;color:var(--txt3);}
.tag{display:inline-flex;align-items:center;gap:4px;padding:3px 9px;border-radius:99px;font-family:'JetBrains Mono',monospace;
  font-size:.58rem;font-weight:500;letter-spacing:.05em;border:1px solid var(--accent-ring);color:var(--accent);
  background:var(--accent-soft);flex-shrink:0;}

/* ══════════════════════════════════════════════════════════════ SIDEBAR */
.sb{width:0;flex-shrink:0;position:relative;background:var(--surface);border-left:0 solid var(--border);overflow:hidden;
  transition:width var(--t-slow) var(--ease),border-width var(--t-slow) var(--ease);display:flex;flex-direction:column;}
.sb.open{width:var(--sb-w);border-left-width:1px;}
.sb.dragging{transition:none;}
.sb-grip{position:absolute;left:-3px;top:0;bottom:0;width:7px;cursor:col-resize;z-index:20;}
.sb-grip::after{content:'';position:absolute;left:3px;top:0;bottom:0;width:1px;background:transparent;transition:background var(--t-fast);}
.sb-grip:hover::after,.sb.dragging .sb-grip::after{background:var(--accent);}
.sb-in{width:100%;display:none;flex-direction:column;height:100%;min-height:0;}
.sb-in.on{display:flex;}
.sb-hd{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:13px 15px;
  border-bottom:1px solid var(--border);flex-shrink:0;}
.sb-hd h3{font-size:.68rem;letter-spacing:.15em;text-transform:uppercase;color:var(--txt3);
  font-family:'JetBrains Mono',monospace;font-weight:600;display:flex;align-items:center;gap:7px;}
.sb-hd .n{padding:1px 7px;border-radius:99px;background:var(--surface3);color:var(--txt2);font-size:.62rem;letter-spacing:0;}
.sb-body{flex:1;overflow-y:auto;padding:13px 13px 26px;min-height:0;}

/* Toolbar */
.tb{display:flex;flex-direction:column;gap:8px;margin-bottom:12px;position:sticky;top:0;z-index:5;background:var(--surface);padding-bottom:9px;}
.srch{position:relative;}
.srch .ic{position:absolute;left:10px;top:50%;transform:translateY(-50%);color:var(--txt3);pointer-events:none;}
.srch input{width:100%;height:34px;padding:0 30px 0 32px;border-radius:8px;background:var(--bg);border:1px solid var(--border);
  color:var(--txt);font-family:'JetBrains Mono',monospace;font-size:.75rem;outline:none;
  transition:border-color var(--t-fast),box-shadow var(--t-fast);}
.srch input:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-ring);}
.srch input::placeholder{color:var(--dim);}
.srch .clr{position:absolute;right:6px;top:50%;transform:translateY(-50%);width:22px;height:22px;border-radius:5px;
  display:none;place-items:center;color:var(--txt3);}
.srch .clr:hover{background:var(--surface3);color:var(--txt);}
.srch.has .clr{display:grid;}
.tb-row{display:flex;gap:6px;align-items:center;flex-wrap:wrap;}
.ctl{display:inline-flex;align-items:center;gap:5px;height:29px;padding:0 9px;border-radius:7px;border:1px solid var(--border);
  background:var(--bg);color:var(--txt2);font-family:'JetBrains Mono',monospace;font-size:.68rem;
  transition:border-color var(--t-fast),color var(--t-fast),background var(--t-fast);}
.ctl:hover{border-color:var(--border2);color:var(--txt);}
.ctl.on{border-color:var(--accent-ring);color:var(--accent);background:var(--accent-soft);}
.ctl select{background:transparent;border:none;outline:none;font:inherit;color:inherit;cursor:pointer;padding-right:2px;}
.ctl select option{background:var(--surface);color:var(--txt);}
.grow{flex:1;}
.dens{display:flex;gap:1px;padding:2px;background:var(--bg);border:1px solid var(--border);border-radius:7px;}
.dens button{width:27px;height:23px;border-radius:5px;display:grid;place-items:center;color:var(--txt3);
  transition:background var(--t-fast),color var(--t-fast);}
.dens button:hover{color:var(--txt);}
.dens button.on{background:var(--surface3);color:var(--accent);}
.grp{display:flex;align-items:center;gap:9px;margin:14px 0 8px;position:sticky;top:0;z-index:3;}
.grp:first-child{margin-top:2px;}
.grp b{font-family:'JetBrains Mono',monospace;font-size:.6rem;letter-spacing:.14em;text-transform:uppercase;color:var(--txt3);
  font-weight:600;background:var(--surface);padding-right:4px;}
.grp .n{font-family:'JetBrains Mono',monospace;font-size:.58rem;color:var(--dim);background:var(--surface);padding:0 4px;}
.grp::after{content:'';flex:1;height:1px;background:var(--border);}

/* ══════════════════════════════════════════════════════════════
   THUMBNAILS — the NxDashVids treatment, applied to every row
   ══════════════════════════════════════════════════════════════ */
.thumb{position:relative;width:44px;height:44px;border-radius:8px;flex-shrink:0;overflow:hidden;
  background:var(--surface3);border:1px solid var(--border);display:grid;place-items:center;color:var(--txt2);}
.thumb img,.thumb video{position:absolute;inset:0;width:100%;height:100%;object-fit:cover;transition:opacity var(--t);}
.thumb video{opacity:0;}
.thumb.playing video{opacity:1;}
.thumb.playing img{opacity:0;}
.thumb .ph{position:relative;z-index:1;}
.thumb .ph .ext{display:none;}
.rows.grid .thumb .ph{display:flex;flex-direction:column;align-items:center;gap:7px;}
.rows.grid .thumb .ph .ext{display:block;font-family:'JetBrains Mono',monospace;font-size:.55rem;font-weight:700;
  letter-spacing:.14em;text-transform:uppercase;color:var(--txt3);opacity:.8;}
.rows.grid .thumb.has-media .ph{display:none;}
.thumb.has-media .ph{display:none;}
.thumb .dur{position:absolute;bottom:2px;right:2px;z-index:3;background:rgba(0,0,0,.82);color:#fff;
  font-family:'JetBrains Mono',monospace;font-size:.5rem;font-weight:600;padding:0 3px;border-radius:3px;line-height:1.5;}
.thumb .pv{position:absolute;top:2px;left:2px;z-index:3;background:var(--accent);color:#fff;border-radius:3px;
  width:12px;height:12px;display:grid;place-items:center;opacity:0;transition:opacity var(--t-fast);}
.thumb.playing .pv{opacity:1;}
.thumb .play{position:absolute;inset:0;z-index:2;display:grid;place-items:center;color:#fff;
  background:linear-gradient(transparent 40%,rgba(0,0,0,.45));opacity:0;transition:opacity var(--t-fast);}
.row:hover .thumb .play{opacity:1;}
.thumb.image{color:#e0a4d8;} .thumb.video{color:#8fb8ff;} .thumb.audio{color:#ffc46b;}
.thumb.text{color:#7fd6a8;}  .thumb.archive{color:#c9a4ff;} .thumb.other{color:var(--txt3);}

/* ══════════════════════════════════════════════════════════════ ROWS */
.rows{display:flex;flex-direction:column;gap:6px;}
.rows.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px;}
.row{position:relative;display:flex;align-items:center;gap:11px;padding:9px 11px;border-radius:var(--r);
  background:var(--bg);border:1px solid var(--border);cursor:default;overflow:hidden;
  transition:border-color var(--t-fast),background var(--t-fast),transform var(--t-fast),box-shadow var(--t-fast);}
.row::before{content:'';position:absolute;left:0;top:6px;bottom:6px;width:2px;border-radius:0 2px 2px 0;background:var(--accent);
  opacity:0;transform:scaleY(.3);transition:opacity var(--t-fast),transform var(--t) var(--ease-out);}
.row:hover{border-color:var(--border2);background:var(--surface2);}
.row:hover::before{opacity:1;transform:scaleY(1);}
.row.sel{border-color:var(--accent);background:var(--accent-soft);}
.row.sel::before{opacity:1;transform:scaleY(1);}
.rmeta{flex:1;min-width:0;}
.rmeta b{display:block;font-size:.79rem;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;line-height:1.35;}
.rmeta span{display:block;font-family:'JetBrains Mono',monospace;font-size:.61rem;color:var(--txt3);
  white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:1px;}
.racts{display:flex;gap:3px;flex-shrink:0;opacity:0;transform:translateX(4px);
  transition:opacity var(--t-fast),transform var(--t-fast);}
.row:hover .racts,.row:focus-within .racts{opacity:1;transform:none;}
.act{width:27px;height:27px;border-radius:6px;display:grid;place-items:center;color:var(--txt3);
  border:1px solid transparent;transition:all var(--t-fast);}
.act:hover{color:var(--accent);background:var(--accent-soft);border-color:var(--accent-ring);}
.act.del:hover{color:var(--err);background:var(--err-soft);border-color:var(--err);}
.cbx{width:18px;height:18px;border-radius:5px;border:1.6px solid var(--border2);flex-shrink:0;display:none;place-items:center;
  color:transparent;transition:all var(--t-fast);}
body.selmode .cbx{display:grid;}
.cbx:hover{border-color:var(--accent);}
.row.sel .cbx{background:var(--accent);border-color:var(--accent);color:#fff;}
.row.sel .cbx .ic{animation:tick var(--t) var(--ease-out) both;}
@keyframes tick{from{transform:scale(.3);opacity:0;}to{transform:none;opacity:1;}}
body.selmode .row{cursor:pointer;} body.selmode .racts{display:none;}

/* Compact */
.rows.compact .row{padding:5px 9px;gap:9px;}
.rows.compact .thumb{width:28px;height:28px;border-radius:6px;}
.rows.compact .thumb .ic{width:13px;height:13px;}
.rows.compact .thumb .dur,.rows.compact .thumb .pv{display:none;}
.rows.compact .rmeta b{font-size:.74rem;}
.rows.compact .rmeta span{font-size:.57rem;}
.rows.compact .act{width:24px;height:24px;}

/* Grid — the video-wall layout */
.rows.grid .row{flex-direction:column;align-items:stretch;gap:0;padding:0;}
.rows.grid .row::before{display:none;}
.rows.grid .thumb{width:100%;height:auto;aspect-ratio:16/10;border-radius:0;border:none;border-bottom:1px solid var(--border);}
.rows.grid .thumb .ic{width:26px;height:26px;}
.rows.grid .thumb .dur{bottom:5px;right:5px;font-size:.58rem;padding:1px 5px;}
.rows.grid .thumb .pv{top:5px;left:5px;width:auto;height:16px;padding:0 5px;gap:3px;font-family:'JetBrains Mono',monospace;
  font-size:.5rem;font-weight:700;letter-spacing:.06em;}
.rows.grid .rmeta{padding:8px 9px 9px;}
.rows.grid .rmeta b{font-size:.72rem;white-space:normal;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;}
.rows.grid .racts{position:absolute;top:6px;right:6px;background:color-mix(in srgb,var(--bg) 80%,transparent);
  backdrop-filter:blur(8px);border-radius:7px;padding:2px;}
.rows.grid .cbx{position:absolute;top:7px;left:7px;background:color-mix(in srgb,var(--bg) 82%,transparent);
  backdrop-filter:blur(8px);z-index:4;}

/* Skeleton */
.sk{display:flex;align-items:center;gap:11px;padding:9px 11px;border-radius:var(--r);background:var(--bg);
  border:1px solid var(--border);margin-bottom:6px;}
.sk .b{background:linear-gradient(90deg,var(--surface2) 25%,var(--surface3) 50%,var(--surface2) 75%);
  background-size:220% 100%;animation:shimmer 1.35s linear infinite;border-radius:5px;}
@keyframes shimmer{from{background-position:220% 0;}to{background-position:-120% 0;}}
.sk .b.sq{width:44px;height:44px;border-radius:8px;flex-shrink:0;}
.sk .l{flex:1;display:flex;flex-direction:column;gap:6px;}
.sk .b.l1{height:9px;width:72%;} .sk .b.l2{height:7px;width:45%;}
.empty{text-align:center;padding:46px 18px;}
.empty .ec{width:52px;height:52px;margin:0 auto 13px;border-radius:15px;display:grid;place-items:center;
  background:var(--surface2);border:1px solid var(--border);color:var(--txt3);}
.empty b{display:block;font-size:.86rem;font-weight:600;margin-bottom:4px;}
.empty p{font-size:.73rem;color:var(--txt3);font-family:'JetBrains Mono',monospace;}

/* Forwarding folders */
.ffs{margin-top:20px;}
.ff{margin-bottom:10px;border:1px solid var(--border);border-radius:var(--r);overflow:hidden;background:var(--bg);}
.ff-hd{display:flex;align-items:center;gap:8px;padding:8px 10px;background:var(--surface2);border-left:2px solid var(--blue);
  cursor:pointer;transition:background var(--t-fast);}
.ff-hd:hover{background:var(--surface3);}
.ff-hd .ic{color:var(--blue);}
.ff-hd code{flex:1;font-family:'JetBrains Mono',monospace;font-size:.68rem;color:var(--txt2);overflow:hidden;
  text-overflow:ellipsis;white-space:nowrap;}
.ff-hd .chev{transition:transform var(--t) var(--ease);}
.ff.closed .chev{transform:rotate(-90deg);}
.ff-body{display:grid;grid-template-rows:1fr;transition:grid-template-rows var(--t) var(--ease);}
.ff.closed .ff-body{grid-template-rows:0fr;}
.ff-body>div{overflow:hidden;min-height:0;}
.ff-f{display:flex;align-items:center;gap:9px;padding:6px 10px;font-size:.72rem;border-top:1px solid var(--border);
  transition:background var(--t-fast);}
.ff-f:hover{background:var(--surface2);}
.ff-f .thumb{width:30px;height:30px;border-radius:6px;}
.ff-f .thumb .ic{width:14px;height:14px;}
.ff-f .thumb .dur,.ff-f .thumb .pv{display:none;}
.ff-f .nm{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:var(--txt2);}
.ff-f .sz{font-family:'JetBrains Mono',monospace;font-size:.58rem;color:var(--dim);}
.ff-f .racts{opacity:1;transform:none;}

/* ══════════════════════════════════════════════════════════════ SELECTION BAR */
.selbar{position:fixed;left:50%;bottom:22px;z-index:400;transform:translate(-50%,150%);opacity:0;pointer-events:none;
  display:flex;align-items:center;gap:9px;padding:9px 11px 9px 15px;border-radius:99px;background:var(--surface);
  border:1px solid var(--border2);box-shadow:var(--shadow);backdrop-filter:blur(14px);
  transition:transform var(--t-slow) var(--ease-out),opacity var(--t) var(--ease-out);}
.selbar.on{transform:translate(-50%,0);opacity:1;pointer-events:auto;}
.selbar b{font-family:'JetBrains Mono',monospace;font-size:.73rem;color:var(--accent);white-space:nowrap;}
.selbar .sep{width:1px;height:20px;background:var(--border);}

/* ══════════════════════════════════════════════════════════════ PASTEBIN */
.paste{display:flex;flex-direction:column;gap:9px;height:100%;}
.paste textarea{flex:1;min-height:300px;resize:none;padding:13px;border-radius:var(--r);background:var(--bg);
  border:1px solid var(--border);color:var(--txt);font-family:'JetBrains Mono',monospace;font-size:.77rem;
  line-height:1.65;outline:none;transition:border-color var(--t-fast),box-shadow var(--t-fast);}
.paste textarea:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-ring);}
.paste-ft{display:flex;align-items:center;gap:7px;flex-wrap:wrap;}
.sync{display:inline-flex;align-items:center;gap:5px;font-family:'JetBrains Mono',monospace;font-size:.62rem;color:var(--accent);}
.sync i{width:5px;height:5px;border-radius:50%;background:currentColor;}
.sync.busy{color:var(--warn);}
.sync.busy i{animation:blink .8s ease-in-out infinite;}
@keyframes blink{50%{opacity:.2;}}

/* ══════════════════════════════════════════════════════════════ DRAWER */
.scrim{position:fixed;inset:0;background:rgba(0,0,0,.55);backdrop-filter:blur(3px);z-index:290;opacity:0;pointer-events:none;
  transition:opacity var(--t) var(--ease);}
.scrim.on{opacity:1;pointer-events:auto;}
.drawer{position:fixed;top:0;right:0;bottom:0;width:330px;max-width:88vw;z-index:300;background:var(--surface);
  border-left:1px solid var(--border2);display:flex;flex-direction:column;transform:translateX(102%);
  transition:transform var(--t-slow) var(--ease);box-shadow:var(--shadow);}
.drawer.on{transform:none;}
.drawer-hd{display:flex;align-items:center;justify-content:space-between;padding:14px 16px;border-bottom:1px solid var(--border);}
.drawer-hd h3{font-size:.68rem;letter-spacing:.15em;text-transform:uppercase;color:var(--txt3);
  font-family:'JetBrains Mono',monospace;font-weight:600;}
.drawer-b{flex:1;overflow-y:auto;padding:16px;}
.iblk{margin-bottom:22px;}
.iblk-t{font-size:.58rem;letter-spacing:.15em;text-transform:uppercase;color:var(--txt3);
  font-family:'JetBrains Mono',monospace;font-weight:600;margin-bottom:9px;}
.irow{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:7px 0;border-bottom:1px solid var(--border);}
.irow:last-child{border-bottom:none;}
.irow small{font-size:.73rem;color:var(--txt2);}
.irow b{font-family:'JetBrains Mono',monospace;font-size:.72rem;font-weight:500;color:var(--accent);}
.spd{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
.spd div{background:var(--bg);border:1px solid var(--border);border-radius:var(--r-sm);padding:9px 11px;}
.spd label{display:block;font-size:.55rem;letter-spacing:.11em;text-transform:uppercase;color:var(--txt3);
  font-family:'JetBrains Mono',monospace;margin-bottom:3px;}
.spd b{font-family:'JetBrains Mono',monospace;font-size:.83rem;color:var(--accent);font-weight:500;}
.themes{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;}
.th{padding:9px 7px;border-radius:var(--r-sm);border:1px solid var(--border);background:var(--bg);display:flex;
  flex-direction:column;align-items:center;gap:6px;transition:all var(--t-fast);}
.th:hover{border-color:var(--border2);transform:translateY(-1px);}
.th.on{border-color:var(--accent);background:var(--accent-soft);}
.th .sw{display:flex;gap:2px;}
.th .sw i{width:11px;height:16px;border-radius:3px;}
.th small{font-size:.6rem;font-family:'JetBrains Mono',monospace;color:var(--txt2);}
.th.on small{color:var(--accent);}
.tog{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:8px 0;}
.tog small{font-size:.73rem;color:var(--txt2);}
.switch{width:38px;height:21px;border-radius:99px;background:var(--surface3);border:1px solid var(--border2);
  position:relative;transition:background var(--t-fast),border-color var(--t-fast);flex-shrink:0;}
.switch::after{content:'';position:absolute;top:2px;left:2px;width:15px;height:15px;border-radius:50%;background:var(--txt3);
  transition:transform var(--t) var(--ease-out),background var(--t-fast);}
.switch.on{background:var(--accent-soft);border-color:var(--accent);}
.switch.on::after{transform:translateX(17px);background:var(--accent);}

/* ══════════════════════════════════════════════════════════════
   PREVIEW MODAL + VIDEO PLAYER (ported from NxDashVids)
   ══════════════════════════════════════════════════════════════ */
.modal-wrap{position:fixed;inset:0;z-index:600;display:grid;place-items:center;padding:22px;
  background:rgba(0,0,0,.75);backdrop-filter:blur(6px);opacity:0;pointer-events:none;transition:opacity var(--t) var(--ease);}
.modal-wrap.on{opacity:1;pointer-events:auto;}
.modal{width:min(880px,100%);max-height:90vh;display:flex;flex-direction:column;overflow:hidden;background:var(--surface);
  border:1px solid var(--border2);border-radius:var(--r-xl);box-shadow:var(--shadow);
  transform:scale(.94) translateY(14px);transition:transform var(--t-slow) var(--ease-out);}
.modal-wrap.on .modal{transform:none;}
.modal-hd{display:flex;align-items:center;gap:11px;padding:12px 15px;border-bottom:1px solid var(--border);flex-shrink:0;}
.modal-hd .thumb{width:30px;height:30px;}
.modal-hd b{flex:1;font-size:.85rem;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.modal-b{flex:1;overflow:auto;min-height:0;background:var(--bg);display:grid;place-items:center;}
.modal-b.pad{padding:20px;}
.modal-ft{display:flex;align-items:center;gap:8px;padding:11px 15px;border-top:1px solid var(--border);flex-wrap:wrap;flex-shrink:0;}
.modal-ft small{flex:1;font-family:'JetBrains Mono',monospace;font-size:.62rem;color:var(--txt3);}

/* Player */
.pw{background:#000;overflow:hidden;position:relative;width:100%;user-select:none;}
.pw video#mainPlayer{width:100%;display:block;aspect-ratio:16/9;max-height:64vh;background:#000;object-fit:contain;}
.pw video#seekVideo{position:absolute;width:1px;height:1px;opacity:0;pointer-events:none;}
.pctl{position:absolute;bottom:0;left:0;right:0;background:linear-gradient(transparent,rgba(0,0,0,.88));
  padding:46px 12px 11px;opacity:0;transition:opacity .25s;z-index:5;}
.pw:hover .pctl,.pctl.active{opacity:1;}
.pbar-wrap{width:100%;height:4px;background:rgba(255,255,255,.25);border-radius:2px;cursor:pointer;position:relative;
  margin-bottom:9px;transition:height .15s;}
.pbar-wrap:hover{height:6px;}
.pbuf{position:absolute;left:0;top:0;bottom:0;background:rgba(255,255,255,.35);border-radius:2px;width:0;}
.pbar{height:100%;background:var(--accent);border-radius:2px;width:0%;position:relative;}
.pbar::after{content:'';position:absolute;right:-6px;top:50%;transform:translateY(-50%);width:12px;height:12px;
  background:var(--accent);border-radius:50%;opacity:0;transition:opacity .15s;}
.pbar-wrap:hover .pbar::after{opacity:1;}
.hover-line{position:absolute;top:-4px;bottom:-4px;width:2px;background:#fff;pointer-events:none;opacity:0;z-index:2;}
.pbar-wrap:hover .hover-line{opacity:1;}
.prow{display:flex;align-items:center;gap:6px;}
.pbtn{color:#fff;width:32px;height:32px;display:flex;align-items:center;justify-content:center;gap:1px;border-radius:6px;
  font-size:.68rem;font-weight:600;font-family:'JetBrains Mono',monospace;transition:background .12s,transform .12s;}
.pbtn:hover{background:rgba(255,255,255,.16);}
.pbtn:active{transform:scale(.9);}
.pbtn.w{width:auto;padding:0 8px;}
.ptime{font-size:.68rem;color:#d6dbe3;font-family:'JetBrains Mono',monospace;margin-left:4px;white-space:nowrap;}
.vol-wrap{display:flex;align-items:center;gap:3px;}
.vol{width:0;opacity:0;height:3px;appearance:none;background:rgba(255,255,255,.35);border-radius:2px;outline:none;
  cursor:pointer;transition:width .2s var(--ease),opacity .2s;}
.vol-wrap:hover .vol,.vol:focus{width:64px;opacity:1;}
.vol::-webkit-slider-thumb{appearance:none;width:11px;height:11px;border-radius:50%;background:#fff;cursor:pointer;}
.vol::-moz-range-thumb{width:11px;height:11px;border-radius:50%;background:#fff;cursor:pointer;border:none;}
.pspacer{flex:1;}
.pw:fullscreen{width:100vw;height:100vh;border-radius:0;background:#000;}
.pw:fullscreen video#mainPlayer{aspect-ratio:auto;max-height:none;width:100%;height:100%;object-fit:contain;}
.pw:fullscreen .pctl{padding-bottom:24px;}
.seekpv{position:absolute;bottom:34px;pointer-events:none;z-index:20;display:flex;flex-direction:column;align-items:center;
  transform:translateX(-50%);opacity:0;}
.seekpv.show{opacity:1;}
.seekpv canvas{border-radius:5px;border:2px solid var(--accent);background:#000;display:block;}
.seekpv .st{background:var(--accent);color:#fff;font-size:.6rem;font-weight:700;font-family:'JetBrains Mono',monospace;
  padding:1px 8px;border-radius:0 0 4px 4px;margin-top:-2px;}
.skipind{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(0,0,0,.72);color:#fff;
  padding:9px 17px;border-radius:9px;font-size:1.15rem;font-weight:700;font-family:'JetBrains Mono',monospace;
  pointer-events:none;z-index:10;opacity:0;transition:opacity .15s;}
.bigplay{position:absolute;inset:0;display:grid;place-items:center;z-index:4;pointer-events:none;opacity:0;transition:opacity var(--t);}
.bigplay.on{opacity:1;}
.bigplay i{width:64px;height:64px;border-radius:50%;background:rgba(0,0,0,.6);display:grid;place-items:center;color:#fff;
  backdrop-filter:blur(6px);}
/* ── GENERAL CONTEXT MENU (one helper, every surface) ── */
.ctxm{position:fixed;background:var(--surface);border:1px solid var(--border2);border-radius:11px;padding:5px;
  min-width:216px;max-width:280px;z-index:10000;box-shadow:var(--shadow);
  opacity:1;animation:ctxin .13s var(--ease-out);}
@keyframes ctxin{from{opacity:0;transform:scale(.965) translateY(-5px);}to{opacity:1;transform:none;}}
.ctxi{display:flex;align-items:center;gap:10px;padding:7px 9px;font-size:.765rem;cursor:pointer;color:var(--txt);
  border-radius:7px;transition:background .1s,color .1s;user-select:none;}
.ctxi .ic{color:var(--txt3);transition:color .1s;}
.ctxi:hover,.ctxi.hi{background:var(--surface2);}
.ctxi:hover .ic,.ctxi.hi .ic{color:var(--accent);}
.ctxi .lbl{flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.ctxi .hint{color:var(--dim);font-family:'JetBrains Mono',monospace;font-size:.63rem;flex-shrink:0;}
.ctxi.on{color:var(--accent);} .ctxi.on .ic{color:var(--accent);}
.ctxi.danger{color:var(--err);} .ctxi.danger .ic{color:var(--err);}
.ctxi.danger:hover,.ctxi.danger.hi{background:var(--err-soft);}
.ctxi.danger:hover .ic,.ctxi.danger.hi .ic{color:var(--err);}
.ctxi.off{opacity:.38;pointer-events:none;}
.ctxsep{height:1px;background:var(--border);margin:4px 5px;}
.ctxhd{display:flex;align-items:center;gap:8px;padding:6px 9px 7px;margin-bottom:2px;border-bottom:1px solid var(--border);}
.ctxhd .thumb{width:26px;height:26px;border-radius:6px;}
.ctxhd .thumb .ic{width:12px;height:12px;}
.ctxhd .thumb .dur,.ctxhd .thumb .pv,.ctxhd .thumb .play{display:none;}
.ctxhd .t{min-width:0;flex:1;}
.ctxhd b{display:block;font-size:.72rem;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.ctxhd small{display:block;font-family:'JetBrains Mono',monospace;font-size:.58rem;color:var(--txt3);}
.pv-img{max-width:100%;max-height:64vh;object-fit:contain;display:block;}
.pv-audio{width:100%;padding:26px;text-align:center;}
.pv-text{white-space:pre-wrap;font-family:'JetBrains Mono',monospace;font-size:.74rem;line-height:1.65;padding:16px;
  width:100%;max-height:60vh;overflow:auto;color:var(--txt2);}

/* ══════════════════════════════════════════════════════════════ TOASTS */
.toasts{position:fixed;right:20px;bottom:20px;z-index:800;display:flex;flex-direction:column;gap:8px;align-items:flex-end;}
.toast{display:flex;align-items:center;gap:9px;padding:10px 14px;border-radius:var(--r);background:var(--surface);
  border:1px solid var(--border2);box-shadow:var(--shadow);font-size:.77rem;max-width:320px;opacity:1;
  animation:toastin var(--t-slow) var(--ease-out);}
.toast.out{animation:toastout var(--t) var(--ease) both;}
@keyframes toastin{from{opacity:0;transform:translateX(26px) scale(.95);}to{opacity:1;transform:none;}}
@keyframes toastout{to{opacity:0;transform:translateX(26px) scale(.95);}}
.toast .ic{color:var(--accent);}
.toast.err{border-color:var(--err);} .toast.err .ic{color:var(--err);}

/* ══════════════════════════════════════════════════════════════ MOBILE */
.mnav{display:none;} .sheet-hd{display:none;}
@container app (max-width: 760px){
  .shell{height:auto;min-height:100vh;overflow:visible;display:block;}
  main{overflow:visible;}
  .hdr{padding:0 14px;height:52px;}
  .brand em{display:none;}
  .hdr .seg{display:none;}
  .hdr .pill span{display:none;}
  .hdr .pill{padding:0 9px;}
  .stats{grid-template-columns:repeat(2,1fr);}
  .stat{padding:9px 14px;}
  .layout{display:block;min-height:0;}
  main{padding:16px 14px 96px;}
  .urls{grid-template-columns:1fr;}
  .sb{position:fixed;left:0;right:0;bottom:0;top:auto;width:100% !important;height:86vh;max-height:86vh;z-index:310;
      border-left:none;border-top:1px solid var(--border2);border-radius:22px 22px 0 0;box-shadow:var(--shadow);
      transform:translateY(101%);transition:transform var(--t-slow) var(--ease);overflow:hidden;}
  .sb.open{transform:none;width:100% !important;}
  .sb-grip{display:none;}
  .sheet-hd{display:block;padding:9px 0 3px;flex-shrink:0;}
  .sheet-hd i{display:block;width:38px;height:4px;border-radius:99px;background:var(--border2);margin:0 auto;}
  .sb-body{padding:12px 12px 90px;}
  .act{width:34px;height:34px;}
  .racts{opacity:1;transform:none;}
  .row{padding:10px 11px;}
  .thumb{width:48px;height:48px;}
  .ctl{height:34px;padding:0 11px;font-size:.72rem;}
  .dens button{width:34px;height:28px;}
  .srch input{height:40px;font-size:.8rem;}
  .rows.grid{grid-template-columns:repeat(2,1fr);}
  .cbx{width:22px;height:22px;}
  .mnav{display:grid;grid-template-columns:repeat(4,1fr);gap:2px;position:fixed;left:0;right:0;bottom:0;z-index:320;
    height:60px;padding:5px 6px calc(5px + env(safe-area-inset-bottom));
    background:color-mix(in srgb,var(--bg) 90%,transparent);backdrop-filter:blur(18px) saturate(1.5);
    border-top:1px solid var(--border);}
  .mnav button{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:3px;border-radius:11px;
    color:var(--txt3);font-size:.58rem;font-weight:500;font-family:'JetBrains Mono',monospace;
    transition:color var(--t-fast),background var(--t-fast);position:relative;overflow:hidden;}
  .mnav button .ic{width:19px;height:19px;}
  .mnav button.on{color:var(--accent);background:var(--accent-soft);}
  .drawer{width:100%;max-width:100%;border-radius:22px 22px 0 0;top:auto;height:82vh;transform:translateY(101%);
    border-left:none;border-top:1px solid var(--border2);}
  .drawer.on{transform:none;}
  .selbar{bottom:74px;left:12px;right:12px;transform:translateY(150%);border-radius:var(--r-lg);
    justify-content:space-between;padding:9px 10px 9px 13px;}
  .selbar.on{transform:none;}
  .toasts{left:12px;right:12px;bottom:74px;align-items:stretch;}
  .toast{max-width:none;}
  .modal-wrap{padding:0;align-items:end;}
  .modal{width:100%;max-height:94vh;border-radius:20px 20px 0 0;}
  .pw video#mainPlayer{max-height:44vh;}
  .pctl{opacity:1;padding:38px 9px 10px;}          /* no hover on touch */
  .pbar-wrap{height:6px;}
  .pbtn{width:38px;height:38px;}
  .vol{width:56px;opacity:1;}
  .dz{padding:28px 16px;}
  .dz-cta{flex-direction:column;}
  .dz-cta .btn{width:100%;}
}
@container app (max-width: 420px){
  .stats{grid-template-columns:1fr 1fr;}
  .brand b{font-size:.88rem;}
  .rows.grid{grid-template-columns:repeat(2,1fr);}
}

/* ══════════════════════════════════════════════════════════════
   FILE EXPLORER  —  /database
   ══════════════════════════════════════════════════════════════ */
body[data-view="share"]    #expShell{display:none;}
body[data-view="explorer"] #shell{display:none;}

.exp{display:flex;flex-direction:column;height:100%;min-height:0;}

/* simulated address bar (mock-only affordance) */
.addr{display:flex;align-items:center;gap:8px;padding:6px 14px;border-bottom:1px solid var(--border);
  background:var(--bg2);font-family:'JetBrains Mono',monospace;font-size:.68rem;color:var(--txt3);flex-shrink:0;}
.addr .u{flex:1;padding:4px 10px;border-radius:6px;background:var(--bg);border:1px solid var(--border);
  color:var(--txt2);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.addr .u b{color:var(--accent);font-weight:500;}

/* toolbar */
.exp-tb{display:flex;align-items:center;gap:7px;padding:9px 14px;border-bottom:1px solid var(--border);
  background:var(--surface);flex-shrink:0;flex-wrap:wrap;}
.nav-btn{width:30px;height:30px;border-radius:7px;display:grid;place-items:center;color:var(--txt2);
  border:1px solid var(--border);background:var(--bg);transition:all var(--t-fast);}
.nav-btn:hover:not(:disabled){color:var(--accent);border-color:var(--accent-ring);background:var(--accent-soft);}
.nav-btn:disabled{opacity:.3;cursor:default;}

/* breadcrumb */
.crumbs{flex:1;min-width:160px;display:flex;align-items:center;gap:2px;padding:0 8px;height:30px;
  border-radius:7px;background:var(--bg);border:1px solid var(--border);overflow:hidden;}
.crumb{display:inline-flex;align-items:center;gap:5px;padding:3px 7px;border-radius:5px;
  font-size:.73rem;color:var(--txt2);white-space:nowrap;transition:background var(--t-fast),color var(--t-fast);}
.crumb:hover{background:var(--surface2);color:var(--txt);}
.crumb.last{color:var(--txt);font-weight:600;}
.crumb .ic{color:var(--txt3);}
.crumb-sep{color:var(--dim);font-size:.7rem;flex-shrink:0;}

/* explorer body */
.exp-body{flex:1;display:flex;min-height:0;}

/* ── left tree ── */
.tree{width:212px;flex-shrink:0;border-right:1px solid var(--border);background:var(--surface);
  overflow-y:auto;padding:10px 8px 20px;position:relative;}
.tree-grip{position:absolute;right:-3px;top:0;bottom:0;width:7px;cursor:col-resize;z-index:6;}
.tree-grip:hover{background:var(--accent-ring);}
.tsec{font-family:'JetBrains Mono',monospace;font-size:.55rem;letter-spacing:.14em;text-transform:uppercase;
  color:var(--dim);padding:9px 8px 5px;font-weight:600;}
.tnode{display:flex;align-items:center;gap:6px;padding:5px 8px;border-radius:6px;font-size:.75rem;
  color:var(--txt2);cursor:pointer;white-space:nowrap;transition:background var(--t-fast),color var(--t-fast);}
.tnode:hover{background:var(--surface2);color:var(--txt);}
.tnode.on{background:var(--accent-soft);color:var(--accent);}
.tnode.on .ic{color:var(--accent);}
.tnode .ic{color:var(--txt3);flex-shrink:0;}
.tnode .nm{flex:1;overflow:hidden;text-overflow:ellipsis;}
.tnode .ct{font-family:'JetBrains Mono',monospace;font-size:.58rem;color:var(--dim);}
.tnode .tw{width:12px;display:grid;place-items:center;flex-shrink:0;transition:transform var(--t) var(--ease);}
.tnode.closed .tw{transform:rotate(-90deg);}
.tkids{margin-left:11px;border-left:1px solid var(--border);padding-left:3px;}
.tkids.hide{display:none;}

/* ── file list ── */
.files{flex:1;min-width:0;display:flex;flex-direction:column;background:var(--bg);position:relative;}
.cols{display:flex;align-items:stretch;border-bottom:1px solid var(--border);background:var(--surface);
  flex-shrink:0;position:sticky;top:0;z-index:4;user-select:none;}
.col{display:flex;align-items:center;gap:5px;padding:7px 10px;font-family:'JetBrains Mono',monospace;
  font-size:.6rem;letter-spacing:.1em;text-transform:uppercase;color:var(--txt3);font-weight:600;
  position:relative;cursor:pointer;overflow:hidden;white-space:nowrap;transition:color var(--t-fast),background var(--t-fast);}
.col:hover{color:var(--txt);background:var(--surface2);}
.col.sorted{color:var(--accent);}
.col .ar{font-size:.55rem;opacity:0;transition:opacity var(--t-fast);}
.col.sorted .ar{opacity:1;}
.col-grip{position:absolute;right:0;top:0;bottom:0;width:5px;cursor:col-resize;z-index:5;}
.col-grip:hover{background:var(--accent);}

.rowsx{flex:1;overflow-y:auto;position:relative;padding-bottom:20px;}
.fr{display:flex;align-items:center;border-bottom:1px solid transparent;cursor:default;
  font-size:.76rem;color:var(--txt2);position:relative;transition:background var(--t-fast);}
.fr:nth-child(even){background:color-mix(in srgb,var(--surface) 40%,transparent);}
.fr:hover{background:var(--surface2);}
.fr.sel{background:var(--accent-soft);color:var(--txt);}
.fr.sel::before{content:'';position:absolute;left:0;top:0;bottom:0;width:2px;background:var(--accent);}
.fr.cursor{outline:1px solid var(--accent-ring);outline-offset:-1px;}
.fc{padding:6px 10px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;flex-shrink:0;}
.fc.name{display:flex;align-items:center;gap:9px;}
.fc.name .thumb{width:26px;height:26px;border-radius:5px;}
.fc.name .thumb .ic{width:13px;height:13px;}
.fc.name .thumb .dur{font-size:.44rem;padding:0 2px;}
.fc.name .thumb .pv,.fc.name .thumb .play{display:none;}
.fc.name b{font-weight:500;color:var(--txt);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.fr.dir .fc.name b{color:var(--blue);}
.fr{user-select:none;}
.fr.dir:hover .fc.name b{text-decoration:underline;}
.fc.mono{font-family:'JetBrains Mono',monospace;font-size:.66rem;color:var(--txt3);}
.fc.num{text-align:right;}

/* rubber band */
.band{position:absolute;border:1px solid var(--accent);background:var(--accent-soft);pointer-events:none;z-index:8;border-radius:2px;}

/* ── detail pane ── */
.detail{width:288px;flex-shrink:0;border-left:1px solid var(--border);background:var(--surface);
  display:flex;flex-direction:column;overflow:hidden;position:relative;}
.detail.hide{display:none;}
.detail-grip{position:absolute;left:-3px;top:0;bottom:0;width:7px;cursor:col-resize;z-index:6;}
.detail-grip:hover{background:var(--accent-ring);}
.detail-hd{padding:11px 13px;border-bottom:1px solid var(--border);display:flex;align-items:center;justify-content:space-between;}
.detail-hd h4{font-family:'JetBrains Mono',monospace;font-size:.6rem;letter-spacing:.14em;text-transform:uppercase;
  color:var(--txt3);font-weight:600;}
.detail-b{flex:1;overflow-y:auto;padding:13px;}
.dprev{width:100%;aspect-ratio:16/10;border-radius:9px;overflow:hidden;background:var(--bg);
  border:1px solid var(--border);display:grid;place-items:center;margin-bottom:13px;position:relative;}
.dprev img,.dprev video{width:100%;height:100%;object-fit:cover;}
.dprev .ic{width:34px;height:34px;color:var(--txt3);}
.dname{font-size:.82rem;font-weight:600;color:var(--txt);word-break:break-word;margin-bottom:3px;line-height:1.35;}
.dsub{font-family:'JetBrains Mono',monospace;font-size:.62rem;color:var(--txt3);margin-bottom:13px;}
.dgrid{display:flex;flex-direction:column;gap:1px;background:var(--border);border:1px solid var(--border);
  border-radius:8px;overflow:hidden;margin-bottom:13px;}
.dgrid div{display:flex;justify-content:space-between;gap:10px;padding:6px 10px;background:var(--surface);
  font-size:.68rem;}
.dgrid span{color:var(--txt3);flex-shrink:0;}
.dgrid b{font-family:'JetBrains Mono',monospace;font-weight:500;color:var(--txt2);text-align:right;
  overflow:hidden;text-overflow:ellipsis;}
.dacts{display:flex;flex-direction:column;gap:6px;}
.dacts .btn{width:100%;}

/* ── status bar ── */
.status{display:flex;align-items:center;gap:14px;padding:6px 14px;border-top:1px solid var(--border);
  background:var(--surface);font-family:'JetBrains Mono',monospace;font-size:.63rem;color:var(--txt3);flex-shrink:0;}
.status .sp{flex:1;}
.status b{color:var(--accent);font-weight:500;}

/* ── explorer mobile ── */
@container app (max-width: 860px){
  .tree{position:fixed;left:0;top:0;bottom:0;width:250px;z-index:330;transform:translateX(-101%);
    transition:transform var(--t-slow) var(--ease);box-shadow:var(--shadow);}
  .tree.open{transform:none;}
  .tree-grip{display:none;}
  .detail{position:fixed;right:0;top:0;bottom:0;width:290px;max-width:88vw;z-index:330;
    transform:translateX(101%);transition:transform var(--t-slow) var(--ease);box-shadow:var(--shadow);}
  .detail.show{transform:none;display:flex;}
  .detail-grip{display:none;}
  .fc.type,.fc.from{display:none;}
  .col.type,.col.from{display:none;}
  .exp-tb{padding:8px 10px;}
  .addr{display:none;}
  .fc{padding:9px 10px;}
}


</style>
</head>
<body data-theme="midnight" data-view="share">

<svg style="display:none" aria-hidden="true">
  <symbol id="i-bolt" viewBox="0 0 24 24"><path d="M13 2 4.5 13.5H11l-1 8.5 8.5-11.5H12z"/></symbol>
  <symbol id="i-xfer" viewBox="0 0 24 24"><path d="M3.5 8.5h14M14 5l3.5 3.5L14 12M20.5 15.5h-14M10 12l-3.5 3.5L10 19"/></symbol>
  <symbol id="i-up" viewBox="0 0 24 24"><path d="M12 19V5M5 12l7-7 7 7"/></symbol>
  <symbol id="i-dl" viewBox="0 0 24 24"><path d="M12 4v12M6 12l6 6 6-6M4 20h16"/></symbol>
  <symbol id="i-eye" viewBox="0 0 24 24"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7-10-7-10-7Z"/><circle cx="12" cy="12" r="3"/></symbol>
  <symbol id="i-trash" viewBox="0 0 24 24"><path d="M4 7h16M9 7V5h6v2M6 7l1 13h10l1-13M10 11v6M14 11v6"/></symbol>
  <symbol id="i-check" viewBox="0 0 24 24"><path d="m4 12 5.5 5.5L20 6.5"/></symbol>
  <symbol id="i-search" viewBox="0 0 24 24"><circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/></symbol>
  <symbol id="i-x" viewBox="0 0 24 24"><path d="M6 6l12 12M18 6 6 18"/></symbol>
  <symbol id="i-list" viewBox="0 0 24 24"><path d="M4 6h16M4 12h16M4 18h16"/></symbol>
  <symbol id="i-rows" viewBox="0 0 24 24"><path d="M4 5h16M4 9.5h16M4 14h16M4 18.5h16"/></symbol>
  <symbol id="i-grid" viewBox="0 0 24 24"><rect x="3.5" y="3.5" width="7" height="7" rx="1.5"/><rect x="13.5" y="3.5" width="7" height="7" rx="1.5"/><rect x="3.5" y="13.5" width="7" height="7" rx="1.5"/><rect x="13.5" y="13.5" width="7" height="7" rx="1.5"/></symbol>
  <symbol id="i-db" viewBox="0 0 24 24"><ellipse cx="12" cy="6" rx="8" ry="3"/><path d="M4 6v12c0 1.7 3.6 3 8 3s8-1.3 8-3V6M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3"/></symbol>
  <symbol id="i-paste" viewBox="0 0 24 24"><rect x="5" y="4" width="14" height="17" rx="2"/><path d="M9 4V3h6v1M9 10h6M9 14h4"/></symbol>
  <symbol id="i-info" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 7.6v.6"/></symbol>
  <symbol id="i-folder" viewBox="0 0 24 24"><path d="M3 7a2 2 0 0 1 2-2h4l2 2.5h8a2 2 0 0 1 2 2V18a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2Z"/></symbol>
  <symbol id="i-zip" viewBox="0 0 24 24"><rect x="4" y="3" width="16" height="18" rx="2"/><path d="M10 3v3M14 6v3M10 9v3M14 12v3M10 15h4v4h-4z"/></symbol>
  <symbol id="i-chev" viewBox="0 0 24 24"><path d="m7 10 5 5 5-5"/></symbol>
  <symbol id="i-copy" viewBox="0 0 24 24"><rect x="8" y="8" width="12" height="12" rx="2"/><path d="M4 16V6a2 2 0 0 1 2-2h10"/></symbol>
  <symbol id="i-link" viewBox="0 0 24 24"><path d="M10 13.5a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7L11.6 6"/><path d="M14 10.5a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1.4-1.2"/></symbol>
  <symbol id="i-cbx" viewBox="0 0 24 24"><rect x="3.5" y="3.5" width="17" height="17" rx="3"/><path d="m8 12 3 3 5-5"/></symbol>
  <symbol id="i-img" viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="8.5" cy="9.5" r="1.8"/><path d="m4 18 5-5 3.5 3.5L16 13l4 4"/></symbol>
  <symbol id="i-vid" viewBox="0 0 24 24"><rect x="3" y="5" width="13" height="14" rx="2"/><path d="m16 10 5-3v10l-5-3z"/></symbol>
  <symbol id="i-aud" viewBox="0 0 24 24"><path d="M9 18V6l10-2v12"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="16.5" cy="16" r="2.5"/></symbol>
  <symbol id="i-txt" viewBox="0 0 24 24"><path d="M6 3h8l5 5v13a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1Z"/><path d="M14 3v5h5M8.5 13h7M8.5 17h5"/></symbol>
  <symbol id="i-file" viewBox="0 0 24 24"><path d="M6 3h8l5 5v13a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1Z"/><path d="M14 3v5h5"/></symbol>
  <symbol id="i-clear" viewBox="0 0 24 24"><path d="M19 5 5 19M5 5l14 14"/></symbol>
  <symbol id="i-sort" viewBox="0 0 24 24"><path d="M7 4v16M7 20l-3-3M7 20l3-3M17 20V4M17 4l-3 3M17 4l3 3"/></symbol>
  <symbol id="i-loop" viewBox="0 0 24 24"><path d="M17 2.5 20.5 6 17 9.5M20.5 6H7a4 4 0 0 0-4 4v1M7 21.5 3.5 18 7 14.5M3.5 18H17a4 4 0 0 0 4-4v-1"/></symbol>
  <!-- filled player glyphs -->
  <symbol id="p-play" viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></symbol>
  <symbol id="p-pause" viewBox="0 0 24 24"><path d="M6 5h4v14H6zM14 5h4v14h-4z"/></symbol>
  <symbol id="p-back" viewBox="0 0 24 24"><path d="M11 18V6l-8.5 6 8.5 6zm.5-6l8.5 6V6l-8.5 6z"/></symbol>
  <symbol id="p-fwd" viewBox="0 0 24 24"><path d="M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z"/></symbol>
  <symbol id="p-volh" viewBox="0 0 24 24"><path d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z"/></symbol>
  <symbol id="p-volm" viewBox="0 0 24 24"><path d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02z"/></symbol>
  <symbol id="p-volx" viewBox="0 0 24 24"><path d="M16.5 12c0-1.77-1.02-3.29-2.5-4.03v2.21l2.45 2.45c.03-.2.05-.41.05-.63zm2.5 0c0 .94-.2 1.82-.54 2.64l1.51 1.51A8.8 8.8 0 0 0 21 12c0-4.28-2.99-7.86-7-8.77v2.06c2.89.86 5 3.54 5 6.71zM4.27 3L3 4.27 7.73 9H3v6h4l5 5v-6.73l4.25 4.25c-.67.52-1.42.93-2.25 1.18v2.06a8.99 8.99 0 0 0 3.69-1.81L19.73 21 21 19.73l-9-9L4.27 3zM12 4L9.91 6.09 12 8.18V4z"/></symbol>
  <symbol id="p-pip" viewBox="0 0 24 24"><path d="M19 11h-8v6h8v-6zm4 8V5c0-1.1-.9-2-2-2H3c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2zm-2 0H3V5h18v14z"/></symbol>
  <symbol id="p-fs" viewBox="0 0 24 24"><path d="M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"/></symbol>
  <symbol id="p-rot" viewBox="0 0 24 24"><path d="M15.55 5.55L11 1v3.07C7.06 4.56 4 7.92 4 12s3.06 7.44 7 7.93v-2.02c-2.84-.48-5-2.94-5-5.91s2.16-5.43 5-5.91V1l4.55 4.55zM19.93 11a7.906 7.906 0 0 0-1.62-3.89l-1.42 1.42c.54.75.88 1.6 1.02 2.47h2.02zM13 17.93v2.02a7.935 7.935 0 0 0 3.89-1.62l-1.42-1.42c-.75.54-1.59.89-2.47 1.02zm3.89-2.42l1.42 1.41A7.945 7.945 0 0 0 19.93 13h-2.02a5.979 5.979 0 0 1-1.02 2.51z"/></symbol>
</svg>




<div class="shell" id="shell">
<div class="grain"></div>

<header class="hdr">
  <div class="brand" id="brand"></div>
  <div class="hdr-r">
    <div class="seg">
      <button data-panel="db" id="navDb"><svg class="ic sm"><use href="#i-db"/></svg> Database</button>
      <button data-panel="paste" id="navPaste"><svg class="ic sm"><use href="#i-paste"/></svg> Pastebin</button>
    </div>
    <button class="ibtn" id="btnInfo" title="Connection & storage"><svg class="ic"><use href="#i-info"/></svg></button>
    <div class="pill"><i></i><span>ONLINE</span></div>
  </div>
</header>

<div class="stats">
  <div class="stat"><label>Active Port</label><b id="stPortVal">—</b></div>
  <div class="stat acc"><label>Files Sent</label><b id="stFiles">0</b></div>
  <div class="stat"><label>Transferred</label><b id="stBytes">0 B</b></div>
  <div class="stat"><label>Connected</label><b id="stClients">0</b></div>
</div>

<div class="layout">
<main>
  <div class="sec-t">Access from any device on this network</div>
  <div class="urls" id="urlList">
    
  </div>

  <div class="storage">
    <div class="storage-top">
      <b><span id="swUsed">—</span> <span style="font-size:.7rem;color:var(--txt3)">used</span></b>
      <span id="swMeta">—</span>
    </div>
    <div class="track"><i id="storBar"></i></div>
    <div class="storage-legend">
      <span><i class="dot"></i> localTransfer files</span>
      <span><i class="dot b"></i> Available under cap</span>
    </div>
  </div>

  <div class="sec-t">Upload</div>
  <div class="dz" id="dz">
    <div class="dz-ico"><svg class="ic"><use href="#i-up"/></svg></div>
    <h3>Drop files here</h3>
    <p id="dropSub">Any type</p>
    <div class="dz-cta">
      <button class="btn pri" id="btnChoose"><svg class="ic"><use href="#i-up"/></svg> Choose Files</button>
      <input type="file" id="fileInput" multiple style="display:none">
    </div>
  </div>

  <div class="prog" id="prog">
    <div class="ring">
      <svg viewBox="0 0 50 50"><circle class="bgc" cx="25" cy="25" r="21"/><circle class="fgc" id="ringFg" cx="25" cy="25" r="21"/></svg>
      <b id="ringPct">0%</b>
    </div>
    <div class="prog-body">
      <h4 id="progT">Uploading 3 files…</h4>
      <p id="progS">0 B / 48.2 MB</p>
      <div class="track" style="margin-top:8px"><i id="progBar" style="transition:width .2s linear"></i></div>
    </div>
    <button class="ibtn" id="progCancel"><svg class="ic"><use href="#i-x"/></svg></button>
  </div>

  <div class="sent">
    <div class="sec-t">Sent this session</div>
    <div id="sentList"></div>
  </div>
</main>

<aside class="sb" id="sb">
  <div class="sb-grip" id="grip"></div>
  <div class="sheet-hd"><i></i></div>

  <div class="sb-in" id="panelDb">
    <div class="sb-hd">
      <h3><svg class="ic sm"><use href="#i-db"/></svg> Database <span class="n" id="dbCount">0</span></h3>
      <div style="display:flex;align-items:center;gap:6px">
        <button class="ctl" id="openExplorer" title="Open the full file explorer"><svg class="ic sm"><use href="#i-grid"/></svg> File explorer</button>
        <button class="ibtn" id="closeDb"><svg class="ic"><use href="#i-x"/></svg></button>
      </div>
    </div>
    <div class="sb-body">
      <div class="tb">
        <div class="srch" id="srchWrap">
          <svg class="ic sm"><use href="#i-search"/></svg>
          <input id="q" type="text" placeholder="Search files…" spellcheck="false">
          <button class="clr" id="qClear"><svg class="ic sm"><use href="#i-x"/></svg></button>
        </div>
        <div class="tb-row">
          <label class="ctl"><svg class="ic sm"><use href="#i-sort"/></svg>
            <select id="sortSel">
              <option value="time_desc">Newest</option>
              <option value="time_asc">Oldest</option>
              <option value="size_desc">Largest</option>
              <option value="size_asc">Smallest</option>
              <option value="name_asc">A → Z</option>
              <option value="name_desc">Z → A</option>
            </select>
          </label>
          <label class="ctl">
            <select id="typeSel">
              <option value="">All types</option>
              <option value="image">Images</option>
              <option value="video">Video</option>
              <option value="audio">Audio</option>
              <option value="text">Text</option>
              <option value="archive">Archives</option>
              <option value="other">Other</option>
            </select>
          </label>
          <span class="grow"></span>
          <div class="dens" id="dens">
            <button data-d="list" title="List"><svg class="ic sm"><use href="#i-list"/></svg></button>
            <button data-d="compact" title="Compact"><svg class="ic sm"><use href="#i-rows"/></svg></button>
            <button data-d="grid" title="Gallery"><svg class="ic sm"><use href="#i-grid"/></svg></button>
          </div>
        </div>
        <div class="tb-row">
          <button class="ctl" id="btnSelMode"><svg class="ic sm"><use href="#i-cbx"/></svg> Select</button>
          <button class="ctl" id="btnDlAll"><svg class="ic sm"><use href="#i-zip"/></svg> Download all</button>
          <button class="ctl" id="btnGroup"><svg class="ic sm"><use href="#i-list"/></svg> Group by date</button>
        </div>
      </div>
      <div id="dbList"></div>
      <div class="ffs" id="ffs">
        <div class="sec-t" style="margin-bottom:9px">Forwarding folders</div>
        <div id="ffList"></div>
      </div>
    </div>
  </div>

  <div class="sb-in" id="panelPaste">
    <div class="sb-hd">
      <h3><svg class="ic sm"><use href="#i-paste"/></svg> Pastebin</h3>
      <div style="display:flex;align-items:center;gap:9px">
        <span class="sync" id="syncLbl"><i></i> synced</span>
        <button class="ibtn" id="closePaste"><svg class="ic"><use href="#i-x"/></svg></button>
      </div>
    </div>
    <div class="sb-body">
      <div class="paste">
        <textarea id="pasteTA" spellcheck="false" placeholder="Type here — live across every connected device…">ssh -p 443 -R0:127.0.0.1:8080 qr@free.pinggy.io</textarea>
        <div class="paste-ft">
          <small class="mono" style="color:var(--txt3);font-size:.62rem" id="pasteCount">48 chars</small>
          <span class="grow"></span>
          <button class="btn sm" id="pasteCopy"><svg class="ic sm"><use href="#i-copy"/></svg> Copy</button>
          <button class="btn sm danger" id="pasteClear"><svg class="ic sm"><use href="#i-clear"/></svg> Clear</button>
        </div>
      </div>
    </div>
  </div>
</aside>
</div>

<nav class="mnav">
  <button data-m="up" class="on"><svg class="ic"><use href="#i-up"/></svg> Upload</button>
  <button data-m="db"><svg class="ic"><use href="#i-db"/></svg> Files</button>
  <button data-m="paste"><svg class="ic"><use href="#i-paste"/></svg> Paste</button>
  <button data-m="info"><svg class="ic"><use href="#i-info"/></svg> Info</button>
</nav>

<div class="selbar" id="selbar">
  <b id="selN">0 selected</b>
  <span class="sep"></span>
  <button class="btn sm" id="selAll">Select all</button>
  <button class="btn sm pri" id="selZip"><svg class="ic sm"><use href="#i-zip"/></svg> Download ZIP</button>
  <button class="btn sm danger" id="selDel"><svg class="ic sm"><use href="#i-trash"/></svg></button>
  <button class="ibtn" id="selX"><svg class="ic"><use href="#i-x"/></svg></button>
</div>


</div><!-- /shell -->

<div class="shell" id="expShell">
<div class="grain"></div>
<div class="exp">

  <header class="hdr">
    <div class="brand" id="expBrand"></div>
    <div class="hdr-r">
      <div class="seg">
        <button id="expToShare"><svg class="ic sm"><use href="#i-up"/></svg> Share</button>
        <button class="on"><svg class="ic sm"><use href="#i-grid"/></svg> Explorer</button>
      </div>
      <button class="ibtn" id="expTreeBtn" title="Toggle tree"><svg class="ic"><use href="#i-list"/></svg></button>
      <button class="ibtn" id="expDetailBtn" title="Toggle details"><svg class="ic"><use href="#i-info"/></svg></button>
      <div class="pill"><i></i><span>ONLINE</span></div>
    </div>
  </header>

  

  <div class="exp-tb">
    <button class="nav-btn" id="navBack"  title="Back"><svg class="ic sm" style="transform:rotate(90deg)"><use href="#i-chev"/></svg></button>
    <button class="nav-btn" id="navFwd"   title="Forward"><svg class="ic sm" style="transform:rotate(-90deg)"><use href="#i-chev"/></svg></button>
    <button class="nav-btn" id="navUp"    title="Up one level"><svg class="ic sm" style="transform:rotate(180deg)"><use href="#i-chev"/></svg></button>
    <button class="nav-btn" id="navRefresh" title="Refresh"><svg class="ic sm"><use href="#i-loop"/></svg></button>
    <div class="crumbs" id="crumbs"></div>
    <div class="srch" style="width:190px">
      <svg class="ic sm"><use href="#i-search"/></svg>
      <input id="expQ" type="text" placeholder="Search here…" spellcheck="false">
    </div>
    <button class="ctl" id="expZip"><svg class="ic sm"><use href="#i-zip"/></svg> Download</button>
  </div>

  <div class="exp-body">
    <aside class="tree" id="tree">
      <div class="tree-grip" id="treeGrip"></div>
      <div id="treeInner"></div>
    </aside>

    <section class="files" id="files">
      <div class="cols" id="cols"></div>
      <div class="rowsx" id="rowsx"></div>
    </section>

    <aside class="detail" id="detail">
      <div class="detail-grip" id="detailGrip"></div>
      <div class="detail-hd">
        <h4>Details</h4>
        <button class="ibtn" id="detailClose"><svg class="ic"><use href="#i-x"/></svg></button>
      </div>
      <div class="detail-b" id="detailBody"></div>
    </aside>
  </div>

  <div class="status">
    <span id="stItems">0 items</span>
    <span id="stSel"></span>
    <span class="sp"></span>
    <span id="stPath"></span>
    <span class="free">—</span>
  </div>
</div>
</div>


<!-- ═══ SHARED OVERLAYS — direct children of <body> so they render in BOTH views ═══ -->
<div class="scrim" id="scrim"></div>
<aside class="drawer" id="drawer">
  <div class="sheet-hd"><i></i></div>
  <div class="drawer-hd">
    <h3>Connection & Storage</h3>
    <button class="ibtn" id="closeDrawer"><svg class="ic"><use href="#i-x"/></svg></button>
  </div>
  <div class="drawer-b">
    <div class="iblk">
      <div class="iblk-t">Appearance</div>
      <div class="themes" id="themes">
        <button class="th" data-t="midnight"><span class="sw"><i style="background:#0b0e14"></i><i style="background:#151a23"></i><i style="background:#3fb950"></i></span><small>Midnight</small></button>
        <button class="th" data-t="slate"><span class="sw"><i style="background:#12151a"></i><i style="background:#1d222b"></i><i style="background:#7c9cf5"></i></span><small>Slate</small></button>
        <button class="th" data-t="light"><span class="sw"><i style="background:#f6f8fa"></i><i style="background:#fff;border:1px solid #dde3ea"></i><i style="background:#1a7f37"></i></span><small>Light</small></button>
      </div>
      <div class="tog" style="margin-top:10px">
        <small>Hover-preview videos</small>
        <button class="switch on" id="togHover"></button>
      </div>
      <div class="tog">
        <small>Autoplay on open</small>
        <button class="switch on" id="togAutoplay"></button>
      </div>
    </div>
    <div class="iblk">
      <div class="iblk-t">Transfer speed (live)</div>
      <div class="spd">
        <div><label>↑ Upload</label><b id="spUp">0 B/s</b></div>
        <div><label>↓ Receive</label><b>0 B/s</b></div>
      </div>
    </div>
    <div class="iblk">
      <div class="iblk-t">Storage</div>
      <div class="irow"><small>Used by app</small><b id="iUsed">—</b></div>
      <div class="irow"><small>Storage cap</small><b id="iCap">—</b></div>
      <div class="irow"><small>Max upload</small><b id="iMaxUp">—</b></div>
      <div class="irow"><small>Free on disk</small><b id="iFree">—</b></div>
      
    </div>
    <div class="iblk">
      <div class="iblk-t">Server</div>
      <div class="irow"><small>Port</small><b id="iPort">—</b></div>
      <div class="irow"><small>Active clients</small><b id="iClients">—</b></div>
      <div class="irow"><small>Saving folder</small><b id="iSavingDir">—</b></div>
    </div>
  </div>
</aside>

<!-- ═══ PREVIEW MODAL ═══ -->
<div class="modal-wrap" id="modalWrap">
  <div class="modal">
    <div class="modal-hd">
      <div class="thumb" id="modalThumb"></div>
      <b id="modalName">file</b>
      <button class="ibtn" id="modalX"><svg class="ic"><use href="#i-x"/></svg></button>
    </div>
    <div class="modal-b" id="modalBody"></div>
    <div class="modal-ft">
      <small id="modalInfo"></small>
      <button class="btn sm" id="btnAsText">View as text</button>
      <button class="btn sm pri"><svg class="ic sm"><use href="#i-dl"/></svg> Download</button>
    </div>
  </div>
</div>


<div class="toasts" id="toasts"></div>




<script>

/* ══════════════════════════════════════════════════════════════
   EMBEDDED REAL ASSETS (from D:\CPP_programs\NxDashVids\web)
   ══════════════════════════════════════════════════════════════ */

/* ══════════════════════════════════════════════════════════════ DATA */
const HR = 3600e3, DAY = 24*HR;
let NOW = Date.now();
setInterval(()=>{ NOW = Date.now(); }, 30000);

let FILES = [], FFS = [], FTREE = {};



const EXT = {
  image:'jpg jpeg png gif webp bmp svg avif heic tif tiff ico',
  video:'mp4 mkv mov avi webm m4v wmv flv mpeg 3gp',
  audio:'mp3 flac wav aac ogg m4a wma opus aiff mid',
  text:'txt md log csv json xml yaml yml ini cfg conf html css js ts py cpp c h hpp cs go rs kt sh bat ps1 sql diff ino kicad_pcb docx pptx pdf',
  archive:'zip rar 7z tar gz bz2 xz iso jar apk exe bin msi',
};
const MAP = {}; for (const k in EXT) EXT[k].split(' ').forEach(e=>MAP[e]=k);
const ICON = {image:'i-img',video:'i-vid',audio:'i-aud',text:'i-txt',archive:'i-zip',other:'i-file'};
const ext = n => (n.split('.').pop()||'').toLowerCase();
const cat = n => MAP[ext(n)] || 'other';

function fmt(b){
  if(b<1024) return b+' B';
  if(b<1048576) return (b/1024).toFixed(1)+' KB';
  if(b<1073741824) return (b/1048576).toFixed(1)+' MB';
  return (b/1073741824).toFixed(2)+' GB';
}
function fmtDur(s){
  if(!isFinite(s)||s<0) s=0;
  s=Math.floor(s);
  const h=Math.floor(s/3600), m=Math.floor(s%3600/60), x=s%60;
  return h? h+':'+String(m).padStart(2,'0')+':'+String(x).padStart(2,'0')
          : m+':'+String(x).padStart(2,'0');
}
function ago(t){
  const d=Date.now()-t;
  if(d<HR) return Math.max(1,Math.round(d/60000))+'m ago';
  if(d<DAY) return Math.round(d/HR)+'h ago';
  return Math.round(d/DAY)+'d ago';
}
function bucket(t){
  const d=Date.now()-t;
  if(d<DAY) return 'Today';
  if(d<2*DAY) return 'Yesterday';
  if(d<7*DAY) return 'This week';
  if(d<31*DAY) return 'This month';
  return 'Older';
}
const esc = s => String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');

/* ══════════════════════════════════════════════════════════════ STATE */
const LS='lt.';
const store={
  get(k,d){ try{const v=localStorage.getItem(LS+k);return v===null?d:JSON.parse(v);}catch(e){return d;} },
  set(k,v){ try{localStorage.setItem(LS+k,JSON.stringify(v));}catch(e){} },
  wipe(){ Object.keys(localStorage).filter(k=>k.startsWith(LS)).forEach(k=>localStorage.removeItem(k)); },
};
const S={
  theme:store.get('theme','midnight'), sort:store.get('sort','time_desc'), type:store.get('type',''),
  q:store.get('q',''), density:store.get('density','list'), panel:store.get('panel',null),
  sbW:store.get('sbW',380), group:store.get('group',true),
  hoverPv:store.get('hoverPv',true), autoplay:store.get('autoplay',true),
  vol:store.get('vol',1), logo:store.get('logo','brk'),
};
function save(){ for(const k in S) store.set(k,S[k]); }

const $ = s=>document.querySelector(s);
const $$ = s=>[...document.querySelectorAll(s)];
const reduced = ()=>document.body.classList.contains('no-motion') ||
                    matchMedia('(prefers-reduced-motion: reduce)').matches;

/* ══════════════════════════════════════════════════════════════
   LOGO CANDIDATES — click through them in the mock bar
   ══════════════════════════════════════════════════════════════ */

const LOGOS = { brk:{ html:'<b class="brk"><i>[</i>l<span>T</span>.io<i>]</i></b>' } };
function setLogo(id){
  id='brk'; S.logo=id;
  $('#brand').innerHTML=LOGOS[id].html;
}

document.body.dataset.theme=S.theme;
setLogo(S.logo);
$('#q').value=S.q; $('#sortSel').value=S.sort; $('#typeSel').value=S.type;
$('#srchWrap').classList.toggle('has',!!S.q);
document.documentElement.style.setProperty('--sb-w',S.sbW+'px');
$$('#dens button').forEach(b=>b.classList.toggle('on',b.dataset.d===S.density));
$('#btnGroup').classList.toggle('on',S.group);
$$('.th').forEach(b=>b.classList.toggle('on',b.dataset.t===S.theme));
$('#togHover').classList.toggle('on',S.hoverPv);
$('#togAutoplay').classList.toggle('on',S.autoplay);

let selMode=false, selected=new Set(), loading=true;

/* Host-only actions (open a folder in Explorer, etc.) are meaningless from a
   phone on the LAN. In the real app this comes from /api/info -> is_host;
   here it is a toggle so both states can be reviewed. */
let IS_HOST = false;
function isHost(){ return IS_HOST; }
function setHost(v){ IS_HOST = !!v; }


/* ══════════════════════════════════════════════════════════════
   THUMBNAIL MARKUP — one function, used by every surface
   ══════════════════════════════════════════════════════════════ */
function thumbHTML(f, opts){
  opts = opts || {};
  const c = cat(f.name||f.n||'');
  const th = f.th || null;
  const pv = (f.pv && S.hoverPv) ? f.pv : null;
  const isVid = c==='video';
  let inner = `<span class="ph"><svg class="ic"><use href="#${ICON[c]}"/></svg><b class="ext">${ext(f.name||f.n||"").slice(0,5)||"file"}</b></span>`;
  if(th) inner += `<img src="${th}" alt="" loading="lazy">`;
  if(pv && S.hoverPv) inner += `<video src="${pv}" muted loop playsinline preload="none"></video>`;
  if(isVid){
    inner += `<span class="play"><svg class="ic lg f"><use href="#p-play"/></svg></span>`;
    if(f.dur) inner += `<span class="dur">${fmtDur(f.dur)}</span>`;
    if(pv && S.hoverPv) inner += `<span class="pv"><svg class="ic f" style="width:8px;height:8px"><use href="#p-play"/></svg>${opts.big?'PREVIEW':''}</span>`;
  }
  return `<div class="thumb ${c}${th?' has-media':''}" data-pv="${pv?1:0}">${inner}</div>`;
}

/* Hover-to-play wiring, delegated so it survives re-renders */
function wireHoverPreview(scope){
  scope.querySelectorAll('.thumb[data-pv="1"]').forEach(t=>{
    if(t._wired) return; t._wired=true;
    const v=t.querySelector('video'); if(!v) return;
    let timer=null;
    const host = t.closest('.row,.ff-f') || t;
    host.addEventListener('mouseenter',()=>{
      if(!S.hoverPv) return;
      timer=setTimeout(()=>{ t.classList.add('playing'); v.currentTime=0; v.play().catch(()=>{}); },200);
    });
    host.addEventListener('mouseleave',()=>{
      clearTimeout(timer); t.classList.remove('playing'); v.pause();
    });
  });
}

/* ══════════════════════════════════════════════════════════════ LIST */
function visible(){
  let out=[...FILES];
  const q=S.q.trim().toLowerCase();
  if(q) out=out.filter(f=>f.name.toLowerCase().includes(q));
  if(S.type) out=out.filter(f=>cat(f.name)===S.type);
  const s=S.sort;
  out.sort((a,b)=>{
    if(s==='time_desc') return b.t-a.t;            // ← real newest-first, the default
    if(s==='time_asc')  return a.t-b.t;
    if(s==='size_desc') return b.size-a.size;
    if(s==='size_asc')  return a.size-b.size;
    if(s==='name_asc')  return a.name.localeCompare(b.name,undefined,{numeric:true});
    if(s==='name_desc') return b.name.localeCompare(a.name,undefined,{numeric:true});
    return 0;
  });
  return out;
}

function rowHTML(f){
  return `<div class="row${selected.has(f.id)?' sel':''}" data-id="${f.id}">
    <div class="cbx"><svg class="ic sm"><use href="#i-check"/></svg></div>
    ${thumbHTML(f,{big:S.density==='grid'})}
    <div class="rmeta">
      <b title="${esc(f.name)}">${esc(f.name)}</b>
      <span>${fmt(f.size)} · ${ago(f.t)} · ${f.from}</span>
    </div>
    <div class="racts">
      <button class="act" data-a="view" title="Preview"><svg class="ic sm"><use href="#i-eye"/></svg></button>
      <button class="act" data-a="dl" title="Download"><svg class="ic sm"><use href="#i-dl"/></svg></button>
      <button class="act del" data-a="del" title="Delete"><svg class="ic sm"><use href="#i-trash"/></svg></button>
    </div>
  </div>`;
}

function renderDb(animate=true){
  const el=$('#dbList'), list=visible();
  $('#dbCount').textContent=FILES.length;

  if(loading){
    el.innerHTML=Array.from({length:6},()=>
      `<div class="sk"><div class="b sq"></div><div class="l"><div class="b l1"></div><div class="b l2"></div></div></div>`).join('');
    return;
  }
  if(!list.length){
    el.innerHTML=`<div class="empty"><div class="ec"><svg class="ic lg"><use href="#i-search"/></svg></div>
      <b>${S.q||S.type?'No matches':'Nothing here yet'}</b>
      <p>${S.q||S.type?'Try a different filter':'Uploaded files land here'}</p></div>`;
    return;
  }

  const first=new Map();
  if(animate && !reduced()) $$('#dbList .row').forEach(r=>first.set(r.dataset.id,r.getBoundingClientRect()));

  const byDate = S.group && (S.sort==='time_desc'||S.sort==='time_asc');
  let html='';
  if(byDate){
    let cur=null, open=false;
    list.forEach(f=>{
      const b=bucket(f.t);
      if(b!==cur){
        if(open) html+='</div>';
        const n=list.filter(x=>bucket(x.t)===b).length;
        html+=`<div class="grp"><b>${b}</b><span class="n">${n}</span></div><div class="rows ${S.density}">`;
        cur=b; open=true;
      }
      html+=rowHTML(f);
    });
    if(open) html+='</div>';
  } else {
    html=`<div class="rows ${S.density}">`+list.map(rowHTML).join('')+'</div>';
  }
  el.innerHTML=html;
  wireHoverPreview(el);

  if(reduced()) return;
  $$('#dbList .row').forEach((r,i)=>{
    const f0=first.get(r.dataset.id);
    if(f0){
      const f1=r.getBoundingClientRect(), dx=f0.left-f1.left, dy=f0.top-f1.top;
      if(dx||dy) r.animate([{transform:`translate(${dx}px,${dy}px)`},{transform:'none'}],
                           {duration:380,easing:'cubic-bezier(.16,1,.3,1)'});
    } else {
      r.animate([{opacity:0,transform:'translateY(-9px)'},{opacity:1,transform:'none'}],
                {duration:320,delay:Math.min(i*22,420),easing:'cubic-bezier(.16,1,.3,1)',fill:'both'});
    }
  });
}

/* ══════════════════════════════════════════════════════════════ FF TREE */
function ffNode(k,depth){
  if(k.d){
    return `<div class="ff-f" style="padding-left:${10+depth*13}px">
      <svg class="ic sm" style="color:var(--blue)"><use href="#i-folder"/></svg>
      <span class="nm" style="color:var(--blue)">${esc(k.d)}</span>
      <button class="act" title="Download folder as ZIP"><svg class="ic sm"><use href="#i-zip"/></svg></button>
    </div>` + k.kids.map(c=>ffNode(c,depth+1)).join('');
  }
  return `<div class="ff-f" style="padding-left:${10+depth*13}px" data-ffname="${esc(k.n)}">
    ${thumbHTML({name:k.n,th:k.th,pv:k.pv,dur:k.dur})}
    <span class="nm">${esc(k.n)}</span><span class="sz">${fmt(k.s)}</span>
    <div class="racts">
      <button class="act" data-a="view"><svg class="ic sm"><use href="#i-eye"/></svg></button>
      <button class="act"><svg class="ic sm"><use href="#i-dl"/></svg></button>
    </div>
  </div>`;
}
function renderFf(){
  $('#ffList').innerHTML=FFS.map(ff=>`
    <div class="ff${ff.open?'':' closed'}" data-ff="${ff.id}">
      <div class="ff-hd">
        <svg class="ic sm chev"><use href="#i-chev"/></svg>
        <svg class="ic sm"><use href="#i-folder"/></svg>
        <code title="${esc(ff.path)}">${esc(ff.path)}</code>
        <button class="act" title="Download folder as ZIP"><svg class="ic sm"><use href="#i-zip"/></svg></button>
      </div>
      <div class="ff-body"><div>${ff.kids.map(k=>ffNode(k,0)).join('')}</div></div>
    </div>`).join('');
  wireHoverPreview($('#ffList'));
}

/* ══════════════════════════════════════════════════════════════ PANELS */
function openPanel(which){
  if(S.panel===which){ closePanel(); return; }
  S.panel=which; save();
  $('#sb').classList.add('open');
  $('#panelDb').classList.toggle('on',which==='db');
  $('#panelPaste').classList.toggle('on',which==='paste');
  $('#navDb').classList.toggle('on',which==='db');
  $('#navPaste').classList.toggle('on',which==='paste');
  syncMNav();
  if(which==='db'){ loading=true; renderDb(false); setTimeout(()=>{loading=false;renderDb();},650); }
}
function closePanel(){
  S.panel=null; save();
  $('#sb').classList.remove('open');
  $('#navDb').classList.remove('on'); $('#navPaste').classList.remove('on');
  setTimeout(()=>{ if(!S.panel){ $('#panelDb').classList.remove('on'); $('#panelPaste').classList.remove('on'); } },260);
  syncMNav();
}
$('#navDb').onclick=()=>openPanel('db');
$('#navPaste').onclick=()=>openPanel('paste');
$('#closeDb').onclick=closePanel;
$('#closePaste').onclick=closePanel;

function syncMNav(){
  const active = drawerOpen ? 'info' : (S.panel || 'up');
  $$('.mnav button').forEach(b=>b.classList.toggle('on',b.dataset.m===active));
}
$$('.mnav button').forEach(b=>b.onclick=()=>{
  const m=b.dataset.m;
  if(m==='up'){ closePanel(); closeDrawer(); window.scrollTo({top:0,behavior:reduced()?'auto':'smooth'}); }
  else if(m==='info'){ closePanel(); toggleDrawer(); }
  else { closeDrawer(); openPanel(m); }
  syncMNav();
});

let drawerOpen=false;
const toggleDrawer=()=>drawerOpen?closeDrawer():openDrawer();
function openDrawer(){ drawerOpen=true; $('#drawer').classList.add('on'); $('#scrim').classList.add('on'); $('#btnInfo').classList.add('on'); syncMNav(); }
function closeDrawer(){ drawerOpen=false; $('#drawer').classList.remove('on'); $('#scrim').classList.remove('on'); $('#btnInfo').classList.remove('on'); syncMNav(); }
$('#btnInfo').onclick=toggleDrawer;
$('#closeDrawer').onclick=closeDrawer;
$('#scrim').onclick=closeDrawer;

/* ══════════════════════════════════════════════════════════════ TOOLBAR */
let qT=null;
$('#q').addEventListener('input',e=>{
  S.q=e.target.value; $('#srchWrap').classList.toggle('has',!!S.q);
  clearTimeout(qT); qT=setTimeout(()=>{save();renderDb();},110);
});
$('#qClear').onclick=()=>{ $('#q').value=''; S.q=''; $('#srchWrap').classList.remove('has'); save(); renderDb(); };
$('#sortSel').onchange=e=>{ S.sort=e.target.value; save(); renderDb(); };
$('#typeSel').onchange=e=>{ S.type=e.target.value; save(); renderDb(); };
$$('#dens button').forEach(b=>b.onclick=()=>{
  S.density=b.dataset.d; save();
  $$('#dens button').forEach(x=>x.classList.toggle('on',x===b));
  renderDb();
});
$('#btnGroup').onclick=()=>{ S.group=!S.group; save(); $('#btnGroup').classList.toggle('on',S.group); renderDb(); };
$('#togHover').onclick=()=>{ S.hoverPv=!S.hoverPv; save(); $('#togHover').classList.toggle('on',S.hoverPv); renderDb(false); renderFf(); toast('Hover previews '+(S.hoverPv?'on':'off')); };
$('#togAutoplay').onclick=()=>{ S.autoplay=!S.autoplay; save(); $('#togAutoplay').classList.toggle('on',S.autoplay); };

/* ══════════════════════════════════════════════════════════════ SELECT */
function setSelMode(on){
  selMode=on; document.body.classList.toggle('selmode',on);
  $('#btnSelMode').classList.toggle('on',on);
  if(!on) selected.clear();
  updateSelBar(); renderDb(false);
}
function updateSelBar(){
  $('#selbar').classList.toggle('on',selMode&&selected.size>0);
  $('#selN').textContent=selected.size+' selected';
}
$('#btnSelMode').onclick=()=>setSelMode(!selMode);
$('#selX').onclick=()=>setSelMode(false);
$('#selAll').onclick=()=>{ visible().forEach(f=>selected.add(f.id)); renderDb(false); updateSelBar(); };
$('#selZip').onclick=()=>{ zipIds([...selected], selected.size+' files'); setSelMode(false); };
$('#selDel').onclick=()=>{ const ids=[...selected], n=ids.length;
  if(!confirm('Delete '+n+' file(s) from the host?')) return;
  apiDelete(ids,()=>toast(n+' file(s) deleted')); setSelMode(false); };
$('#btnDlAll').onclick=()=>{ const v=visible(); zipIds(v.map(f=>f.id), v.length+' shown files'); };

$('#dbList').addEventListener('click',e=>{
  const row=e.target.closest('.row'); if(!row) return;
  const id=row.dataset.id, f=FILES.find(x=>x.id===id);
  if(selMode){
    selected.has(id)?selected.delete(id):selected.add(id);
    row.classList.toggle('sel'); updateSelBar(); return;
  }
  const a=e.target.closest('.act');
  if(!a){ if(e.target.closest('.thumb')) openPreview(f); return; }
  if(a.dataset.a==='view') openPreview(f);
  if(a.dataset.a==='dl') location.href=f.dlUrl;
  if(a.dataset.a==='del'){ if(confirm('Delete '+f.name+' from the host?')) apiDelete([id],()=>toast('Deleted '+f.name)); }
});
$('#ffList').addEventListener('click',e=>{
  const hd=e.target.closest('.ff-hd');
  if(hd){ if(e.target.closest('.act')){ const id=hd.parentElement.dataset.ff; location.href='/download_ff_zip?id='+encodeURIComponent(id); return; } hd.parentElement.classList.toggle('closed'); return; }
  const ff=e.target.closest('.ff-f');
  if(ff && (e.target.closest('[data-a="view"]')||e.target.closest('.thumb'))){
    const name=ff.dataset.ffname;
    let found=null;
    (function walk(ks){ ks.forEach(k=>k.d?walk(k.kids):(k.n===name&&(found=k))); })(FFS.flatMap(f=>f.kids));
    if(found) openPreview({name:found.n,size:found.s,th:found.th,pv:found.pv,dur:found.dur,
      t:found.t||Date.now(),from:'forwarded',url:found.url,dlUrl:found.dlUrl});
  }
});

/* ══════════════════════════════════════════════════════════════
   PREVIEW + PLAYER  (NxDashVids port)
   ══════════════════════════════════════════════════════════════ */
let P = { v:null, seekV:null, playing:false, ctlTimer:null, speedIdx:0, cur:null };
const SPEEDS=[1,1.25,1.5,1.75,2,0.5,0.75];

function openPreview(f){
  if(!f) return;
  /* stop and detach any player still running from the last preview */
  if(P.v){ try{ P.v.pause(); P.v.removeAttribute('src'); P.v.load(); }catch(e){} }
  P.v=null; P.seekV=null; P.playing=false; clearTimeout(P.ctlTimer);
  P.cur=f;
  const c=cat(f.name);
  $('#modalName').textContent=f.name;
  $('#modalInfo').textContent=fmt(f.size)+' · '+c+(f.dur?' · '+fmtDur(f.dur):'');
  $('#modalThumb').outerHTML=thumbHTML(f).replace('class="thumb','id="modalThumb" class="thumb');
  const body=$('#modalBody');
  body.classList.toggle('pad', c!=='video');
  $('#btnAsText').style.display = (c==='video'||c==='image') ? 'none' : '';

  const e2=ext(f.name);
  if(c==='video' && NATIVE_VID.has(e2)){
    body.innerHTML = playerHTML(f.url);
    setupPlayer();
  } else if(c==='image' && NATIVE_IMG.has(e2)){
    body.innerHTML = `<img class="pv-img" src="${f.url}" alt="${esc(f.name)}">`;
  } else if(c==='audio'){
    body.innerHTML = `<div class="pv-audio"><div class="thumb audio" style="width:70px;height:70px;margin:0 auto 14px">
        <span class="ph"><svg class="ic" style="width:30px;height:30px"><use href="#i-aud"/></svg></span></div>
      <p style="color:var(--txt2);font-size:.82rem;margin-bottom:14px">${esc(f.name)}</p>
      <audio controls autoplay src="${f.url}" style="width:100%;max-width:420px"></audio></div>`;
  } else if(c==='text'){
    body.innerHTML = '<pre class="pv-text">Loading…</pre>';
    fetch(f.url).then(r=>r.text()).then(t=>{
      const TR=200000, cutT=t.length>TR;
      body.innerHTML = `<pre class="pv-text">${esc(cutT?t.slice(0,TR):t)}${cutT?'\n\n… truncated':''}</pre>`;
      $('#modalInfo').textContent = fmt(f.size)+' · '+t.length+' chars';
    }).catch(()=>{ body.innerHTML='<pre class="pv-text">Could not read this file.</pre>'; });
  } else {
    body.innerHTML = `<div class="empty"><div class="ec"><svg class="ic lg"><use href="#${ICON[c]}"/></svg></div>
      <b>${esc(f.name)}</b><p>No inline preview for this type</p></div>`;
  }
  $('#modalWrap').classList.add('on');
}

function playerHTML(src){
  return `<div class="pw" id="pw">
    <video id="mainPlayer" src="${src}" playsinline controlslist="nodownload noremoteplayback"></video>
    <video id="seekVideo" src="${src}" muted preload="auto"></video>
    <div class="bigplay" id="bigplay"><i><svg class="ic f" style="width:28px;height:28px"><use href="#p-play"/></svg></i></div>
    <div class="seekpv" id="seekpv"><canvas id="seekCanvas" width="160" height="90"></canvas><div class="st" id="seekTime">0:00</div></div>
    <div class="pctl" id="pctl">
      <div class="pbar-wrap" id="pbarWrap">
        <div class="pbuf" id="pbuf"></div>
        <div class="pbar" id="pbar"></div>
        <div class="hover-line" id="hoverLine"></div>
      </div>
      <div class="prow">
        <button class="pbtn" id="btnPlay"><svg class="ic f"><use href="#p-play"/></svg></button>
        <button class="pbtn w" id="btnBack"><svg class="ic sm f"><use href="#p-back"/></svg>5</button>
        <button class="pbtn w" id="btnFwd">5<svg class="ic sm f"><use href="#p-fwd"/></svg></button>
        <div class="vol-wrap">
          <button class="pbtn" id="btnMute"><svg class="ic f"><use href="#p-volh"/></svg></button>
          <input type="range" class="vol" id="vol" min="0" max="100" value="100">
        </div>
        <span class="ptime" id="ptime">0:00 / 0:00</span>
        <span class="pspacer"></span>
        <button class="pbtn w" id="btnSpeed">1x</button>
        <button class="pbtn" id="btnPip"><svg class="ic f"><use href="#p-pip"/></svg></button>
        <button class="pbtn" id="btnFs"><svg class="ic f"><use href="#p-fs"/></svg></button>
        <button class="pbtn" id="btnRot"><svg class="ic f"><use href="#p-rot"/></svg></button>
      </div>
    </div>
  </div>`;
}

function setupPlayer(){
  const v=$('#mainPlayer'), sv=$('#seekVideo'), pw=$('#pw'), pctl=$('#pctl');
  if(!v||!pw) return;
  P.v=v; P.seekV=sv; P.speedIdx=0;
  v.volume=S.vol;
  /* true only while this <video> is still the one on screen */
  const live = ()=> P.v===v && document.contains(v);

  const setIcon=(id,sym)=>{ const e=$(id); if(e) e.innerHTML=`<svg class="ic f"><use href="#${sym}"/></svg>`; };

  v.addEventListener('play', ()=>{ if(!live())return; P.playing=true; setIcon('#btnPlay','p-pause'); const b=$('#bigplay'); if(b)b.classList.remove('on'); showCtl(); });
  v.addEventListener('pause',()=>{ if(!live())return; P.playing=false; setIcon('#btnPlay','p-play'); const b=$('#bigplay'); if(b)b.classList.add('on'); showCtl(); });
  v.addEventListener('timeupdate', ()=>{ if(live()) updateTime(); });
  v.addEventListener('loadedmetadata', ()=>{ if(live()) updateTime(); });
  v.addEventListener('progress', ()=>{
    if(!live()) return;
    const bf=$('#pbuf'); if(!bf) return;
    if(v.buffered.length && v.duration) bf.style.width=(v.buffered.end(v.buffered.length-1)/v.duration*100)+'%';
  });
  v.addEventListener('volumechange',()=>{
    if(!live()) return;
    const vs=$('#vol'); if(vs) vs.value=v.volume*100; S.vol=v.volume; save();
    setIcon('#btnMute', v.muted||v.volume===0 ? 'p-volx' : (v.volume>0.5?'p-volh':'p-volm'));
  });

  $('#btnPlay').onclick=togglePlay;
  $('#btnBack').onclick=()=>skip(-5);
  $('#btnFwd').onclick=()=>skip(5);
  $('#btnMute').onclick=()=>{ v.muted=!v.muted; };
  $('#vol').oninput=e=>{ v.volume=e.target.value/100; v.muted=false; };
  $('#btnSpeed').onclick=cycleSpeed;
  $('#btnPip').onclick=togglePip;
  $('#btnFs').onclick=toggleFs;
  $('#btnRot').onclick=()=>{
    if(!screen.orientation||!screen.orientation.lock) return toast('Rotation lock unavailable',true);
    const p=screen.orientation.type.startsWith('portrait');
    screen.orientation.lock(p?'landscape':'portrait').catch(()=>{});
  };

  pw.addEventListener('mousemove',showCtl);
  pw.addEventListener('mouseleave',()=>{ if(P.playing) pctl.classList.remove('active'); });
  pw.addEventListener('click',e=>{ if(e.target===v) togglePlay(); });

  /* double-click left/right = ±5s with flash */
  pw.addEventListener('dblclick',e=>{
    const r=pw.getBoundingClientRect();
    const sec=(e.clientX-r.left)<r.width/2 ? -5 : 5;
    skip(sec);
    let ind=pw.querySelector('.skipind');
    if(!ind){ ind=document.createElement('div'); ind.className='skipind'; pw.appendChild(ind); }
    ind.textContent=(sec<0?'':'+')+sec+'s';
    ind.style.opacity='1';
    setTimeout(()=>ind.style.opacity='0',400);
  });

  /* ── seek preview + drag scrubbing ── */
  const wrap=$('#pbarWrap'), pvEl=$('#seekpv'), cv=$('#seekCanvas'), st=$('#seekTime'), hl=$('#hoverLine');
  let showing=false, dragging=false, lastT=0, throttle=null;

  const at=e=>{
    const r=wrap.getBoundingClientRect();
    const x=Math.max(0,Math.min((e.clientX||0)-r.left,r.width));
    return {x, t:(x/r.width)*(v.duration||0)};
  };
  function draw(e){
    if(!v.duration) return;
    const {x,t}=at(e);
    pvEl.style.left=x+'px'; pvEl.classList.add('show'); st.textContent=fmtDur(t);
    hl.style.left=x+'px'; hl.style.opacity='1';
    sv.currentTime=t;
  }
  sv.addEventListener('seeked',()=>{
    if(showing && sv.videoWidth>0) requestAnimationFrame(()=>cv.getContext('2d').drawImage(sv,0,0,160,90));
  });
  function hide(){ showing=false; pvEl.classList.remove('show'); hl.style.opacity='0'; }
  function onMove(e){ if(throttle) return; throttle=setTimeout(()=>throttle=null,30); showing=true; draw(e); }

  wrap.addEventListener('mousemove',onMove);
  wrap.addEventListener('mouseleave',()=>{ if(!dragging) hide(); });
  wrap.addEventListener('mousedown',e=>{
    e.preventDefault(); dragging=true; showing=true; lastT=at(e).t; draw(e);
    const mv=ev=>{ if(throttle) return; throttle=setTimeout(()=>throttle=null,30); lastT=at(ev).t; draw(ev); };
    const up=()=>{ dragging=false; document.removeEventListener('mousemove',mv); v.currentTime=lastT; setTimeout(hide,300); };
    document.addEventListener('mousemove',mv);
    document.addEventListener('mouseup',up,{once:true});
  });
  wrap.addEventListener('touchstart',e=>{ dragging=true;showing=true; const ev={clientX:e.touches[0].clientX}; lastT=at(ev).t; draw(ev); },{passive:true});
  wrap.addEventListener('touchmove',e=>{ e.preventDefault(); const ev={clientX:e.touches[0].clientX}; lastT=at(ev).t; showing=true; draw(ev); },{passive:false});
  wrap.addEventListener('touchend',()=>{ dragging=false; v.currentTime=lastT; setTimeout(hide,300); });

  /* context menu — same helper every other surface uses */
  pw.addEventListener('contextmenu',e=>{
    e.preventDefault(); e.stopPropagation();
    CTX.open(e,[
      {head:P.cur},
      {icon:'p-play', label:v.paused?'Play':'Pause', hint:'Space', on:togglePlay},
      {icon:'i-loop', label:'Loop', checked:v.loop, on:()=>{ v.loop=!v.loop; toast('Loop '+(v.loop?'on':'off')); }},
      {icon:'i-sort', label:'Speed · '+v.playbackRate+'x', on:cycleSpeed, keepOpen:true},
      {sep:true},
      {icon:'p-pip', label:'Picture-in-Picture', on:togglePip},
      {icon:'p-fs',  label:'Fullscreen', hint:'F', on:toggleFs},
      {sep:true},
      {icon:'i-dl',   label:'Download', on:()=>{ if(P.cur&&P.cur.dlUrl) location.href=P.cur.dlUrl; }},
      {icon:'i-link', label:'Copy direct link', on:copyLink},
    ]);
  });

  document.addEventListener('fullscreenchange',()=>{
    if(document.fullscreenElement && screen.orientation?.lock) screen.orientation.lock('landscape').catch(()=>{});
    else if(!document.fullscreenElement && screen.orientation?.unlock) screen.orientation.unlock();
  });

  showCtl();
  const bp=$('#bigplay');
  if(S.autoplay) v.play().catch(()=>{ const b=$('#bigplay'); if(b) b.classList.add('on'); });
  else if(bp) bp.classList.add('on');
}

function showCtl(){
  const c=$('#pctl'); if(!c) return;
  c.classList.add('active');
  clearTimeout(P.ctlTimer);
  if(P.playing) P.ctlTimer=setTimeout(()=>c.classList.remove('active'),3000);
}
function togglePlay(){ if(!P.v) return; P.v.paused?P.v.play():P.v.pause(); }
function skip(s){ if(!P.v) return; P.v.currentTime=Math.max(0,Math.min(P.v.duration||0,P.v.currentTime+s)); showCtl(); }
function updateTime(){
  const v=P.v; if(!v||!v.duration) return;
  const t=$('#ptime'), b=$('#pbar');
  if(!t||!b) return;
  t.textContent=fmtDur(v.currentTime)+' / '+fmtDur(v.duration);
  b.style.width=(v.currentTime/v.duration*100)+'%';
}
function cycleSpeed(){
  if(!P.v) return;
  P.speedIdx=(P.speedIdx+1)%SPEEDS.length;
  P.v.playbackRate=SPEEDS[P.speedIdx];
  $('#btnSpeed').textContent=SPEEDS[P.speedIdx]+'x';
  const cs=$('#ctxSpeed'); if(cs) cs.textContent=SPEEDS[P.speedIdx]+'x';
}
function toggleFs(){
  const pw=$('#pw'); if(!pw) return;
  if(document.fullscreenElement) document.exitFullscreen();
  else pw.requestFullscreen?.().catch(()=>{});
}
function togglePip(){
  if(!P.v) return;
  if(document.pictureInPictureElement) document.exitPictureInPicture();
  else if(document.pictureInPictureEnabled) P.v.requestPictureInPicture().catch(()=>toast('PiP unavailable',true));
}







function closeModal(){
  $('#modalWrap').classList.remove('on');
  $$('#modalBody video,#modalBody audio').forEach(m=>{m.pause();m.removeAttribute('src');});
  setTimeout(()=>{ $('#modalBody').innerHTML=''; },260);
  P.v=null; P.playing=false;
}
$('#modalX').onclick=closeModal;
$('#modalWrap').onclick=e=>{ if(e.target===$('#modalWrap')) closeModal(); };
$('#btnAsText').onclick=()=>{
  if(!P.cur) return;
  const b=$('#modalBody'); b.classList.add('pad');
  b.innerHTML='<pre class="pv-text">Loading…</pre>';
  fetch(P.cur.url).then(r=>r.text()).then(t=>{
    const TR=200000, cutT=t.length>TR;
    b.innerHTML=`<pre class="pv-text">${esc(cutT?t.slice(0,TR):t)}${cutT?'\n\n… truncated':''}</pre>`;
  }).catch(()=>{ b.innerHTML='<pre class="pv-text">Could not read this file.</pre>'; });
};

function sampleTextUnused(n){
  return `# ${n}\n\n[BUILD] Mode: RELEASE\n[BUILD] Compiling globals.cpp utils.cpp database.cpp http_server.cpp main.cpp\n`+
    `[OK] Build successful: localTransfer.io.exe\n\n`+
    `-- inline text preview renders here, first 8 KB, with a "Show full" button --\n`;
}

/* keyboard: player map + global escape */
document.addEventListener('keydown',e=>{
  if(e.key==='Escape'){ if(CTX.close()) return; closeModal(); closeDrawer(); if(selMode) setSelMode(false); return; }
  const tag=e.target.tagName;
  if(tag==='INPUT'||tag==='TEXTAREA') return;
  if(!P.v || !$('#modalWrap').classList.contains('on')) return;
  switch(e.key){
    case ' ': e.preventDefault(); togglePlay(); break;
    case 'ArrowLeft':  e.preventDefault(); skip(-5); break;
    case 'ArrowRight': e.preventDefault(); skip(5); break;
    case 'ArrowUp':    e.preventDefault(); P.v.volume=Math.min(1,P.v.volume+.1); break;
    case 'ArrowDown':  e.preventDefault(); P.v.volume=Math.max(0,P.v.volume-.1); break;
    case 'f': case 'F': toggleFs(); break;
    case 'm': case 'M': P.v.muted=!P.v.muted; break;
  }
});

/* ══════════════════════════════════════════════════════════════ SIDEBAR RESIZE */
(function(){
  const sb=$('#sb'), grip=$('#grip');
  let dragging=false;
  grip.addEventListener('pointerdown',e=>{
    dragging=true; sb.classList.add('dragging'); grip.setPointerCapture(e.pointerId);
    document.body.style.userSelect='none'; document.body.style.cursor='col-resize';
  });
  grip.addEventListener('pointermove',e=>{
    if(!dragging) return;
    const w=Math.min(720,Math.max(300,innerWidth-e.clientX));
    document.documentElement.style.setProperty('--sb-w',w+'px');
  });
  grip.addEventListener('pointerup',e=>{
    if(!dragging) return;
    dragging=false; sb.classList.remove('dragging'); grip.releasePointerCapture(e.pointerId);
    document.body.style.userSelect=''; document.body.style.cursor='';
    S.sbW=parseInt(getComputedStyle(document.documentElement).getPropertyValue('--sb-w'))||380;
    save(); toast('Sidebar width saved: '+S.sbW+'px');
  });
})();

/* ══════════════════════════════════════════════════════════════ THEMES */
$$('.th').forEach(b=>b.onclick=()=>{
  S.theme=b.dataset.t; save();
  document.body.dataset.theme=S.theme;
  $$('.th').forEach(x=>x.classList.toggle('on',x===b));
  toast('Theme: '+S.theme);
});

/* ══════════════════════════════════════════════════════════════ TOASTS + RIPPLE */
/* ══════════════════════════════════════════════════════════════
   GENERAL CONTEXT MENU
   CTX.open(event, items) — items are:
     {icon, label, hint, on, checked, danger, disabled, keepOpen}
     {sep:true}                     separator
     {head:fileObject}              file header (thumb + name + meta)
     {head:'TEXT'}                  plain header
   Edge-aware, Escape/scroll/outside-click closes, ↑↓/Enter navigate.
   ══════════════════════════════════════════════════════════════ */
const CTX = (function(){
  let el=null, openedAt=0, scroller=null, scrollAt=0;
  function detach(){ if(scroller){ scroller.removeEventListener('scroll',onScroll); scroller=null; } }
  /* a re-render can fire scroll without moving — only a real scroll dismisses */
  function onScroll(){ if(!scroller) return; if(scroller.scrollTop===scrollAt) return; if(armed()) close(); }
  function armed(){ return performance.now() - openedAt > 180; }
  function scrollParent(node){
    let x=node;
    while(x && x!==document.body && x!==document.documentElement){
      const s=getComputedStyle(x);
      if(/(auto|scroll)/.test(s.overflowY+s.overflowX)) return x;
      x=x.parentElement;
    }
    return null;
  }
  function close(){ detach(); if(!el) return false; el.remove(); el=null; return true; }
  function open(e, items){
    close();
    el=document.createElement('div');
    el.className='ctxm';
    /* strip host-only entries for remote devices, then collapse any separator
       runs those removals produced (and leading/trailing separators) */
    items = items.filter(it => !(it && it.hostOnly && !isHost()));
    items = items.filter((it,i,a)=>{
      if(!it || !it.sep) return true;
      let prev=null; for(let k=i-1;k>=0;k--){ if(a[k]){ prev=a[k]; break; } }
      if(!prev || prev.sep) return false;
      let next=null; for(let k=i+1;k<a.length;k++){ if(a[k]){ next=a[k]; break; } }
      return !!next && !next.sep;
    });
    items.forEach(it=>{
      if(it.sep){ el.insertAdjacentHTML('beforeend','<div class="ctxsep"></div>'); return; }
      if(it.head!==undefined){
        if(!it.head) return;
        if(typeof it.head==='string'){
          el.insertAdjacentHTML('beforeend',`<div class="ctxhd"><div class="t"><b>${esc(it.head)}</b></div></div>`);
        } else {
          const f=it.head;
          el.insertAdjacentHTML('beforeend',
            `<div class="ctxhd">${thumbHTML(f)}<div class="t"><b>${esc(f.name||f.n||'')}</b>`
            +`<small>${fmt(f.size||f.s||0)}${f.dur?' · '+fmtDur(f.dur):''}</small></div></div>`);
        }
        return;
      }
      const d=document.createElement('div');
      d.className='ctxi'+(it.danger?' danger':'')+(it.disabled?' off':'')+(it.checked?' on':'');
      d.innerHTML=`<svg class="ic sm${/^p-/.test(it.checked?'i-check':(it.icon||''))?' f':''}">`
        +`<use href="#${it.checked?'i-check':(it.icon||'i-file')}"/></svg>`
        +`<span class="lbl">${esc(it.label)}</span>`
        +(it.hint?`<span class="hint">${esc(it.hint)}</span>`:'');
      d.addEventListener('click',ev=>{
        ev.stopPropagation();
        if(it.keepOpen){ it.on&&it.on(); return; }
        close(); it.on&&it.on();
      });
      el.appendChild(d);
    });
    document.body.appendChild(el);

    /* edge-aware placement */
    const r=el.getBoundingClientRect(), M=8;
    let x=e.clientX, y=e.clientY;
    if(x+r.width  > innerWidth  - M) x=Math.max(M, x-r.width);
    if(y+r.height > innerHeight - M) y=Math.max(M, innerHeight-M-r.height);
    el.style.left=x+'px'; el.style.top=y+'px';

    /* Close on scroll of the scroller the menu was opened inside — NOT on
       every descendant scroll, or a background re-render nukes the menu. */
    openedAt = performance.now();
    scroller = e.target && e.target.nodeType ? scrollParent(e.target) : null;
    if(scroller){ scrollAt=scroller.scrollTop; scroller.addEventListener('scroll', onScroll); }
    return el;
  }
  /* dismissal */
  document.addEventListener('pointerdown',ev=>{ if(el && !el.contains(ev.target)) close(); },true);
  document.addEventListener('wheel',()=>{ if(armed()) close(); },{passive:true,capture:true});
  window.addEventListener('blur',()=>{ if(armed()) close(); });
  window.addEventListener('resize',()=>{ if(armed()) close(); });
  document.addEventListener('keydown',ev=>{
    if(!el) return;
    const its=[...el.querySelectorAll('.ctxi:not(.off)')];
    const i=its.findIndex(n=>n.classList.contains('hi'));
    if(ev.key==='ArrowDown'||ev.key==='ArrowUp'){
      ev.preventDefault();
      its.forEach(n=>n.classList.remove('hi'));
      const nx=ev.key==='ArrowDown' ? (i+1)%its.length : (i<=0?its.length-1:i-1);
      its[nx]?.classList.add('hi');
    } else if(ev.key==='Enter'&&i>=0){ ev.preventDefault(); its[i].click(); }
  });
  return {open,close};
})();

/* ── helpers the menus call ── */
function copyLink(f){
  const n = f?.name || P.cur?.name || 'file';
  navigator.clipboard?.writeText(location.origin+'/download?id='+encodeURIComponent(f?.id||''));
  toast('Link copied · '+n);
}
function fileMenu(e,f){
  const n=selected.size;
  const multi = selMode && n>1 && selected.has(f.id);
  if(multi) return CTX.open(e,[
    {head:n+' files selected'},
    {icon:'i-zip',   label:'Download '+n+' as ZIP', on:()=>{ toast(`Zipping ${n} file(s)…`); setSelMode(false); }},
    {icon:'i-cbx',   label:'Clear selection', on:()=>setSelMode(false)},
    {sep:true},
    {icon:'i-trash', label:'Delete '+n+' files', danger:true,
      on:()=>{ FILES=FILES.filter(x=>!selected.has(x.id)); setSelMode(false); renderDb(); toast(n+' files deleted'); }},
  ]);
  CTX.open(e,[
    {head:f},
    {icon:'i-eye', label:'Preview',  hint:'Enter', on:()=>openPreview(f)},
    {icon:'i-dl',  label:'Download', on:()=>{ location.href=f.dlUrl; }},
    {icon:'i-link',label:'Copy direct link', on:()=>{ navigator.clipboard?.writeText(location.origin+f.dlUrl); toast('Link copied'); }},
    {sep:true},
    {icon:'i-cbx', label:selMode?(selected.has(f.id)?'Deselect':'Add to selection'):'Select',
      on:()=>{ if(!selMode) setSelMode(true);
               selected.has(f.id)?selected.delete(f.id):selected.add(f.id);
               renderDb(false); updateSelBar(); }},
    {sep:true},
    {icon:'i-folder', label:'Open containing folder', hostOnly:true, on:()=>toast('/open --db  →  Explorer')},
    {icon:'i-copy',   label:'Copy file name', on:()=>{ navigator.clipboard?.writeText(f.name); toast('Name copied'); }},
    {sep:true},
    {icon:'i-trash', label:'Delete', hint:'Del', danger:true,
      on:()=>{ FILES=FILES.filter(x=>x.id!==f.id); renderDb(); toast('Deleted '+f.name); }},
  ]);
}
function listMenu(e){
  const shown=visible().length;
  const dens=[['list','List'],['compact','Compact'],['grid','Gallery']];
  const sorts=[['time_desc','Newest'],['time_asc','Oldest'],['size_desc','Largest'],['name_asc','A → Z']];
  CTX.open(e,[
    {head:shown+' of '+FILES.length+' shown'},
    {icon:'i-zip', label:'Download all shown as ZIP', disabled:!shown,
      on:()=>toast(`Zipping ${shown} shown file(s)…`)},
    {icon:'i-cbx', label:selMode?'Exit select mode':'Select mode', on:()=>setSelMode(!selMode)},
    {sep:true},
    ...sorts.map(([k,l])=>({icon:'i-sort', label:l, checked:S.sort===k,
      on:()=>{ S.sort=k; $('#sortSel').value=k; save(); renderDb(); }})),
    {sep:true},
    ...dens.map(([k,l])=>({icon:'i-grid', label:l+' view', checked:S.density===k,
      on:()=>{ S.density=k; save(); $$('#dens button').forEach(b=>b.classList.toggle('on',b.dataset.d===k)); renderDb(); }})),
    {sep:true},
    {icon:'i-list', label:'Group by date', checked:S.group,
      on:()=>{ S.group=!S.group; save(); $('#btnGroup').classList.toggle('on',S.group); renderDb(); }},
    {icon:'i-eye', label:'Hover-preview videos', checked:S.hoverPv,
      on:()=>{ S.hoverPv=!S.hoverPv; save(); $('#togHover').classList.toggle('on',S.hoverPv); renderDb(false); renderFf(); }},
  ]);
}

/* ── wiring: right-click anywhere useful ── */
$('#dbList').addEventListener('contextmenu',e=>{
  e.preventDefault();
  const row=e.target.closest('.row');
  if(row){ const f=FILES.find(x=>x.id===row.dataset.id); if(f) return fileMenu(e,f); }
  listMenu(e);
});
$('#ffList').addEventListener('contextmenu',e=>{
  e.preventDefault();
  const hd=e.target.closest('.ff-hd');
  if(hd){
    const ff=FFS.find(x=>x.id===hd.parentElement.dataset.ff);
    const closed=hd.parentElement.classList.contains('closed');
    return CTX.open(e,[
      {head:ff.path},
      {icon:'i-zip',    label:'Download folder as ZIP', on:()=>{ location.href='/download_ff_zip?id='+encodeURIComponent(ff.id); }},
      {icon:'i-folder', label:'Open in Explorer', hostOnly:true, on:()=>toast('/open --ff '+ff.id+'  →  Explorer')},
      {sep:true},
      {icon:'i-chev',   label:closed?'Expand':'Collapse', on:()=>hd.parentElement.classList.toggle('closed')},
      {icon:'i-copy',   label:'Copy path', on:()=>{ navigator.clipboard?.writeText(ff.path); toast('Path copied'); }},
    ]);
  }
  const ffe=e.target.closest('.ff-f');
  if(ffe){
    const name=ffe.dataset.ffname; let found=null;
    (function walk(ks){ ks.forEach(k=>k.d?walk(k.kids):(k.n===name&&(found=k))); })(FFS.flatMap(f=>f.kids));
    const f=found?{name:found.n,size:found.s,th:found.th,pv:found.pv,dur:found.dur,t:NOW,from:'forward'}:null;
    return CTX.open(e,[
      {head:f||name},
      {icon:'i-eye',    label:'Preview', disabled:!f, on:()=>openPreview(f)},
      {icon:'i-dl',     label:'Download', on:()=>{ if(f&&f.dlUrl) location.href=f.dlUrl; }},
      {sep:true},
      {icon:'i-folder', label:'Open containing folder', hostOnly:true, on:()=>toast('/open --ff  →  Explorer')},
      {icon:'i-copy',   label:'Copy path', on:()=>{ navigator.clipboard?.writeText(name); toast('Path copied'); }},
    ]);
  }
  listMenu(e);
});
$('#sentList').addEventListener('contextmenu',e=>{
  const it=e.target.closest('.sent-item'); if(!it) return;
  e.preventDefault();
  const nm=it.querySelector('b')?.textContent||'';
  const f=FILES.find(x=>x.name===nm);
  if(f) fileMenu(e,f);
});

/* long-press opens the same menus on touch */
(function(){
  let t=null, sx=0, sy=0;
  const fire=(el,x,y)=>{ el.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:x,clientY:y})); };
  document.addEventListener('touchstart',e=>{
    const el=e.target.closest('.row,.ff-f,.ff-hd,.sent-item,#dbList');
    if(!el) return;
    const tch=e.touches[0]; sx=tch.clientX; sy=tch.clientY;
    t=setTimeout(()=>{ if(navigator.vibrate) navigator.vibrate(12); fire(e.target,sx,sy); },500);
  },{passive:true});
  document.addEventListener('touchmove',e=>{
    const tch=e.touches[0];
    if(t && (Math.abs(tch.clientX-sx)>10||Math.abs(tch.clientY-sy)>10)){ clearTimeout(t); t=null; }
  },{passive:true});
  ['touchend','touchcancel'].forEach(ev=>document.addEventListener(ev,()=>{ clearTimeout(t); t=null; },{passive:true}));
})();


/* ── main screen (upload page) right-click ── */
function themeItems(){
  return [['midnight','Midnight'],['slate','Slate'],['light','Light']].map(function(t){
    return {icon:'i-sun', label:'Theme · '+t[1], checked:S.theme===t[0], on:function(){
      S.theme=t[0]; save(); document.body.dataset.theme=t[0];
      $$('.th').forEach(function(x){ x.classList.toggle('on', x.dataset.t===t[0]); });
      toast('Theme: '+t[1]);
    }};
  });
}
function mainMenu(e){
  const url=e.target.closest('.url');
  if(url){
    const link=url.querySelector('code').textContent;
    return CTX.open(e,[
      {head:link},
      {icon:'i-copy', label:'Copy address', on:function(){ navigator.clipboard?.writeText(link); toast('Copied '+link); }},
      {icon:'i-link', label:'Open in new tab', on:function(){ toast('Opening '+link); }},
      {icon:'i-grid', label:'Show QR code', on:function(){ toast('QR for '+link); }},
    ]);
  }
  if(e.target.closest('#dz')||e.target.closest('.prog')){
    return CTX.open(e,[
      {head:'Upload'},
      {icon:'i-up',     label:'Choose files…', on:fakeUpload},
      {icon:'i-folder', label:'Open saving folder', hostOnly:true, on:function(){ toast('/open --db  \u2192  Explorer'); }},
      {icon:'i-copy',   label:'Copy saving path', on:function(){ navigator.clipboard?.writeText('D:\\CPP_programs\\localTransfer.io\\database'); toast('Path copied'); }},
    ]);
  }
  const sec=e.target.closest('.storage');
  if(sec){
    return CTX.open(e,[
      {head:'Storage'},
      {icon:'i-info',   label:'Connection & storage', on:openDrawer},
      {icon:'i-folder', label:'Open saving folder', hostOnly:true, on:function(){ toast('/open --db  \u2192  Explorer'); }},
    ]);
  }
  CTX.open(e,[
    {head:'localTransfer.io'},
    {icon:'i-up',     label:'Upload files…', on:fakeUpload},
    {sep:true},
    {icon:'i-db',     label:'Database panel', checked:S.panel==='db', on:function(){ openPanel('db'); }},
    {icon:'i-grid',   label:'Open file explorer', hint:'/database', on:function(){ gotoView('explorer'); }},
    {icon:'i-paste',  label:'Pastebin', checked:S.panel==='paste', on:function(){ openPanel('paste'); }},
    {icon:'i-info',   label:'Connection & storage', on:openDrawer},
    {sep:true},
    ...themeItems(),
  ]);
}
$('main').addEventListener('contextmenu',function(e){ e.preventDefault(); mainMenu(e); });
$('.hdr').addEventListener('contextmenu',function(e){ e.preventDefault(); mainMenu(e); });
$('.stats').addEventListener('contextmenu',function(e){ e.preventDefault(); mainMenu(e); });

function toast(msg,err){
  const t=document.createElement('div');
  t.className='toast'+(err?' err':'');
  t.innerHTML=`<svg class="ic sm"><use href="#${err?'i-x':'i-check'}"/></svg><span>${esc(msg)}</span>`;
  $('#toasts').appendChild(t);
  setTimeout(()=>{ t.classList.add('out'); setTimeout(()=>t.remove(),260); },2600);
}
document.addEventListener('pointerdown',e=>{
  const el=e.target.closest('.btn,.ctl,.act,.url,.mnav button,.ibtn');
  if(!el||reduced()||el.closest('.pctl')) return;
  const r=el.getBoundingClientRect(), d=Math.max(r.width,r.height);
  const s=document.createElement('span'); s.className='rip';
  s.style.cssText=`width:${d}px;height:${d}px;left:${e.clientX-r.left-d/2}px;top:${e.clientY-r.top-d/2}px;`;
  if(getComputedStyle(el).position==='static') el.style.position='relative';
  el.appendChild(s); setTimeout(()=>s.remove(),560);
});





/* ══════════════════════════════════════════════════════════════ PASTEBIN */
const ta=$('#pasteTA');
ta.addEventListener('input',()=>{
  $('#pasteCount').textContent=ta.value.length+' chars';
  const l=$('#syncLbl'); l.className='sync busy'; l.innerHTML='<i></i> syncing…';
  clearTimeout(ta._t); ta._t=setTimeout(()=>{ l.className='sync'; l.innerHTML='<i></i> synced'; },500);
});
$('#pasteCopy').onclick=()=>{ navigator.clipboard?.writeText(ta.value); toast('Copied to clipboard'); };
$('#pasteClear').onclick=()=>{ ta.value=''; ta.dispatchEvent(new Event('input')); };

/* ══════════════════════════════════════════════════════════════ MOCK BAR */
$('#openExplorer').onclick=function(){ gotoView('explorer'); };


/* ══════════════════════════════════════════════════════════════ GO */
renderFf();
if(S.panel){
  $('#sb').classList.add('open');
  $('#panelDb').classList.toggle('on',S.panel==='db');
  $('#panelPaste').classList.toggle('on',S.panel==='paste');
  $('#navDb').classList.toggle('on',S.panel==='db');
  $('#navPaste').classList.toggle('on',S.panel==='paste');
  if(S.panel==='db'){ renderDb(false); setTimeout(()=>{loading=false;renderDb();},700); }
} else loading=false;
syncMNav();

/* ══════════════════════════════════════════════════════════════
   FILE EXPLORER  —  /database
   Path model:  db:/            all files
                db:/type/image  by type
                db:/date/Today  by date
                ff:<id>/sub/dir real host folders, mirrored
   ══════════════════════════════════════════════════════════════ */

/* Forwarding folders as a REAL recursive tree, mirroring the host. */


const EX = {
  path: 'db:/',
  hist: ['db:/'], hi: 0,
  sort: 'time', dir: -1,
  q: '',
  sel: new Set(), cursor: -1,
  cols: store.get('expCols', {name:300, size:92, type:78, added:104, from:112}),
  treeW: store.get('expTreeW', 212),
  detailW: store.get('expDetailW', 288),
  detailOpen: store.get('expDetail', true),
  open: {ff01:true, ff02:false, dbtype:true, dbdate:true},
};

/* ── resolve a path to a list of entries ── */
function ffAt(p){                       // "ff:ff01/firmware/archive" -> node list
  const rest = p.slice(3);
  const bits = rest.split('/').filter(Boolean);
  const id = bits.shift();
  const root = FTREE[id];
  if(!root) return null;
  let kids = root.kids;
  for(const b of bits){
    const nx = kids.find(k=>k.d===b);
    if(!nx) return null;
    kids = nx.kids;
  }
  return kids;
}
function entriesAt(p){
  if(p.startsWith('ff:')){
    const kids = ffAt(p) || [];
    return kids.map(k=>k.d
      ? {dir:true, name:k.d, size:k.kids.length, t:k.t||Date.now(), from:'folder', kids:k.kids, _p:k._p}
      : {name:k.n, size:k.s, t:k.t||Date.now(), from:'forwarded', th:k.th, pv:k.pv, dur:k.dur,
         _p:k._p, url:k.url, dlUrl:k.dlUrl});
  }
  let list = FILES.slice();
  if(p.startsWith('db:/type/')) { const t=p.slice(9); list = list.filter(f=>cat(f.name)===t); }
  else if(p.startsWith('db:/date/')){ const d=p.slice(9); list = list.filter(f=>bucket(f.t)===d); }
  return list;
}
function pathLabel(p){
  if(p==='db:/') return ['Database'];
  if(p.startsWith('db:/type/')) return ['Database', ({image:'Images',video:'Video',audio:'Audio',text:'Text',archive:'Archives',other:'Other'})[p.slice(9)]||p.slice(9)];
  if(p.startsWith('db:/date/')) return ['Database', p.slice(9)];
  if(p.startsWith('ff:')){
    const bits=p.slice(3).split('/').filter(Boolean);
    const id=bits.shift();
    return ['Forwarding folders', (FTREE[id]?FTREE[id].path.split('\\').pop():id)].concat(bits);
  }
  return [p];
}
function realPath(p){
  if(p.startsWith('ff:')){
    const bits=p.slice(3).split('/').filter(Boolean);
    const id=bits.shift();
    return FTREE[id] ? FTREE[id].path + (bits.length?'\\'+bits.join('\\'):'') : p;
  }
  return 'D:\\CPP_programs\\localTransfer.io\\database';
}

/* ── navigation ── */
function go(p, push){
  EX.path=p; EX.sel.clear(); EX.cursor=-1;
  if(push!==false){ EX.hist=EX.hist.slice(0,EX.hi+1); EX.hist.push(p); EX.hi=EX.hist.length-1; }
  store.set('lastPath', p);
  renderExplorer();
  if(typeof syncExUrl==='function') syncExUrl();
}
function goBack(){ if(EX.hi>0){ EX.hi--; EX.path=EX.hist[EX.hi]; EX.sel.clear(); renderExplorer(); } }
function goFwd(){ if(EX.hi<EX.hist.length-1){ EX.hi++; EX.path=EX.hist[EX.hi]; EX.sel.clear(); renderExplorer(); } }
function goUp(){
  const p=EX.path;
  if(p.startsWith('ff:')){
    const bits=p.slice(3).split('/').filter(Boolean);
    if(bits.length>1){ bits.pop(); return go('ff:'+bits.join('/')+'/'); }
    return go('db:/');
  }
  if(p!=='db:/') return go('db:/');
}

/* ── sorted + filtered view ── */
function exList(){
  let l = entriesAt(EX.path).slice();
  if(EX.q) l = l.filter(f=>f.name.toLowerCase().includes(EX.q.toLowerCase()));
  const k=EX.sort, d=EX.dir;
  l.sort((a,b)=>{
    if(a.dir!==b.dir) return a.dir?-1:1;             // folders first, always
    let r=0;
    if(k==='name') r=a.name.localeCompare(b.name,undefined,{numeric:true});
    else if(k==='size') r=a.size-b.size;
    else if(k==='type') r=cat(a.name).localeCompare(cat(b.name));
    else if(k==='from') r=String(a.from).localeCompare(String(b.from));
    else r=a.t-b.t;
    return r*d;
  });
  return l;
}

/* ── render ── */
function renderCrumbs(){
  const parts=pathLabel(EX.path);
  const el=$('#crumbs'); el.innerHTML='';
  parts.forEach((p,i)=>{
    if(i) el.insertAdjacentHTML('beforeend','<span class="crumb-sep">›</span>');
    const b=document.createElement('button');
    b.className='crumb'+(i===parts.length-1?' last':'');
    b.innerHTML=(i===0?'<svg class="ic sm"><use href="#i-db"/></svg>':'')+esc(p);
    b.onclick=()=>{
      if(i===0) return go(EX.path.startsWith('ff:')?EX.path.split('/').slice(0,1).join('/')+'/':'db:/');
      if(EX.path.startsWith('ff:')){
        const bits=EX.path.slice(3).split('/').filter(Boolean);
        go('ff:'+bits.slice(0,i).join('/')+'/');
      }
    };
    el.appendChild(b);
  });
  $('#stPath').textContent=realPath(EX.path);
}

const COLDEF=[['name','Name'],['size','Size'],['type','Type'],['added','Added'],['from','From']];
function renderCols(){
  const el=$('#cols'); el.innerHTML='';
  COLDEF.forEach(([k,label])=>{
    const sk = k==='added' ? 'time' : k;
    const d=document.createElement('div');
    d.className='col '+k+(EX.sort===sk?' sorted':'');
    d.style.width=EX.cols[k]+'px';
    if(k==='name') d.style.flex='1 1 '+EX.cols[k]+'px';
    d.innerHTML=`<span>${label}</span><span class="ar">${EX.dir<0?'▼':'▲'}</span><span class="col-grip"></span>`;
    d.onclick=e=>{
      if(e.target.classList.contains('col-grip')) return;
      if(EX.sort===sk) EX.dir*=-1; else { EX.sort=sk; EX.dir = (sk==='name'||sk==='type'||sk==='from')?1:-1; }
      renderExplorer();
    };
    /* column resize */
    d.querySelector('.col-grip').addEventListener('pointerdown',ev=>{
      ev.stopPropagation(); ev.preventDefault();
      const x0=ev.clientX, w0=EX.cols[k];
      const mv=e2=>{ EX.cols[k]=Math.max(56,w0+(e2.clientX-x0)); renderCols(); renderRows(); };
      const up=()=>{ document.removeEventListener('pointermove',mv); store.set('expCols',EX.cols); };
      document.addEventListener('pointermove',mv);
      document.addEventListener('pointerup',up,{once:true});
    });
    el.appendChild(d);
  });
}

function frHTML(f,i){
  const c=cat(f.name);
  const w=EX.cols;
  return `<div class="fr${f.dir?' dir':''}${EX.sel.has(i)?' sel':''}${EX.cursor===i?' cursor':''}" data-i="${i}">
    <div class="fc name" style="flex:1 1 ${w.name}px;width:${w.name}px">
      ${f.dir?`<div class="thumb other"><span class="ph"><svg class="ic"><use href="#i-folder"/></svg></span></div>`:thumbHTML(f)}
      <b title="${esc(f.name)}">${esc(f.name)}</b>
    </div>
    <div class="fc mono num size" style="width:${w.size}px">${f.dir?(f.size+' items'):fmt(f.size)}</div>
    <div class="fc mono type" style="width:${w.type}px">${f.dir?'Folder':(ext(f.name)||'file').toUpperCase()}</div>
    <div class="fc mono added" style="width:${w.added}px">${ago(f.t)}</div>
    <div class="fc mono from" style="width:${w.from}px">${esc(f.from)}</div>
  </div>`;
}
function renderRows(){
  const l=exList();
  $('#rowsx').innerHTML = l.length
    ? l.map(frHTML).join('')
    : `<div class="empty"><div class="ec"><svg class="ic lg"><use href="#i-folder"/></svg></div>
         <b>${EX.q?'No matches':'This folder is empty'}</b>
         <p>${EX.q?'Try a different search':'Nothing here yet'}</p></div>`;
  $('#stItems').textContent = l.length+' item'+(l.length===1?'':'s');
  $('#stSel').textContent  = EX.sel.size ? ('· '+EX.sel.size+' selected') : '';
  wireHoverPreview($('#rowsx'));
  renderDetail();
}
function renderTree(){
  const counts={};
  ['image','video','audio','text','archive','other'].forEach(t=>counts[t]=FILES.filter(f=>cat(f.name)===t).length);
  const buckets=['Today','Yesterday','This week','This month','Older']
    .map(b=>[b,FILES.filter(f=>bucket(f.t)===b).length]).filter(x=>x[1]>0);

  function node(label,icon,path,count,depth,extra){
    const on = EX.path===path;
    return `<div class="tnode${on?' on':''}" data-p="${esc(path)}" style="padding-left:${8+depth*11}px">
      ${extra||'<span class="tw"></span>'}
      <svg class="ic sm"><use href="#${icon}"/></svg>
      <span class="nm">${esc(label)}</span>
      ${count!==undefined?`<span class="ct">${count}</span>`:''}
    </div>`;
  }
  function ffKids(id,kids,prefix,depth){
    return kids.filter(k=>k.d).map(k=>{
      const p=prefix+k.d+'/';
      const has=k.kids.some(x=>x.d);
      const isOpen=EX.open[p]!==false;
      return node(k.d,'i-folder',p,undefined,depth,
             has?`<span class="tw" data-toggle="${esc(p)}"><svg class="ic sm"><use href="#i-chev"/></svg></span>`:'<span class="tw"></span>')
        + (has?`<div class="tkids${isOpen?'':' hide'}">${ffKids(id,k.kids,p,depth+1)}</div>`:'');
    }).join('');
  }

  let h='<div class="tsec">Database</div>';
  h+=node('All files','i-db','db:/',FILES.length,0);
  h+='<div class="tkids">';
  [['image','Images','i-img'],['video','Video','i-vid'],['audio','Audio','i-aud'],
   ['text','Text','i-txt'],['archive','Archives','i-zip'],['other','Other','i-file']]
    .forEach(([t,l,ic])=>{ if(counts[t]) h+=node(l,ic,'db:/type/'+t,counts[t],1); });
  h+='</div>';
  h+='<div class="tsec">By date</div><div class="tkids">';
  buckets.forEach(([b,c])=>{ h+=node(b,'i-sort','db:/date/'+b,c,1); });
  h+='</div>';
  h+='<div class="tsec">Forwarding folders</div>';
  Object.keys(FTREE).forEach(id=>{
    const root=FTREE[id];
    const p='ff:'+id+'/';
    const has=root.kids.some(k=>k.d);
    const isOpen=EX.open[id]!==false;
    h+=node(root.path.split('\\').pop(),'i-folder',p,undefined,0,
        has?`<span class="tw${isOpen?'':' closed'}" data-toggle="${id}"><svg class="ic sm"><use href="#i-chev"/></svg></span>`:'<span class="tw"></span>');
    if(has) h+=`<div class="tkids${isOpen?'':' hide'}">${ffKids(id,root.kids,p,1)}</div>`;
  });
  $('#treeInner').innerHTML=h;
}
function renderDetail(){
  const el=$('#detailBody');
  const l=exList();
  const n=EX.sel.size;
  if(n===0){
    el.innerHTML=`<div class="empty" style="padding:30px 10px">
      <div class="ec"><svg class="ic lg"><use href="#i-folder"/></svg></div>
      <b>${esc(pathLabel(EX.path).slice(-1)[0])}</b>
      <p>${l.length} item${l.length===1?'':'s'}</p></div>
      <div class="dgrid"><div><span>Location</span><b title="${esc(realPath(EX.path))}">${esc(realPath(EX.path))}</b></div></div>`;
    return;
  }
  if(n>1){
    const tot=[...EX.sel].reduce((a,i)=>a+(l[i]&&!l[i].dir?l[i].size:0),0);
    el.innerHTML=`<div class="dname">${n} items selected</div>
      <div class="dsub">${fmt(tot)} total</div>
      <div class="dacts">
        <button class="btn sm pri" onclick="toast('Zipping '+${n}+' items…')"><svg class="ic sm"><use href="#i-zip"/></svg> Download as ZIP</button>
        <button class="btn sm danger" onclick="exDelete()"><svg class="ic sm"><use href="#i-trash"/></svg> Delete ${n} items</button>
      </div>`;
    return;
  }
  const f=l[[...EX.sel][0]];
  if(!f) { el.innerHTML=''; return; }
  const c=cat(f.name);
  const prev = f.dir ? `<svg class="ic"><use href="#i-folder"/></svg>`
    : (f.th ? `<img src="${A[f.th]}" alt="">` : `<svg class="ic"><use href="#${ICON[c]}"/></svg>`);
  el.innerHTML=`
    <div class="dprev">${prev}</div>
    <div class="dname">${esc(f.name)}</div>
    <div class="dsub">${f.dir?'Folder':fmt(f.size)}${f.dur?' · '+fmtDur(f.dur):''}</div>
    <div class="dgrid">
      <div><span>Type</span><b>${f.dir?'File folder':(ext(f.name)||'file').toUpperCase()}</b></div>
      <div><span>Added</span><b>${ago(f.t)}</b></div>
      <div><span>From</span><b>${esc(f.from)}</b></div>
      <div><span>Path</span><b title="${esc(realPath(EX.path))}">${esc(realPath(EX.path).split('\\').slice(-2).join('\\'))}</b></div>
    </div>
    <div class="dacts">
      ${f.dir?`<button class="btn sm pri" onclick="go('${esc(EX.path)}'+${JSON.stringify(f.name)}+'/')">Open folder</button>`
             :`<button class="btn sm pri" onclick="exOpen()"><svg class="ic sm"><use href="#i-eye"/></svg> Preview</button>`}
      <button class="btn sm" onclick="toast('Downloading ${esc(f.name)}')"><svg class="ic sm"><use href="#i-dl"/></svg> Download</button>
      <button class="btn sm danger" onclick="exDelete()"><svg class="ic sm"><use href="#i-trash"/></svg> Delete</button>
    </div>`;
}
function renderExplorer(){
  renderCrumbs(); renderCols(); renderTree(); renderRows();
  $('#navBack').disabled = EX.hi<=0;
  $('#navFwd').disabled  = EX.hi>=EX.hist.length-1;
  $('#navUp').disabled   = EX.path==='db:/';
  $('#detail').classList.toggle('hide', !EX.detailOpen);
  document.documentElement.style.setProperty('--tree-w', EX.treeW+'px');
  $('#tree').style.width=EX.treeW+'px';
  $('#detail').style.width=EX.detailW+'px';
}

/* ── selection + activation ── */
function exOpen(){ exActivate(EX.cursor>=0?EX.cursor:[...EX.sel][0]); }
function exDelete(){
  const l=exList();
  const picked=[...EX.sel].map(i=>l[i]).filter(Boolean);
  const ids=picked.map(f=>f.id).filter(Boolean);
  if(!ids.length){ toast('Only database files can be deleted here',true); return; }
  if(!confirm('Delete '+ids.length+' file(s) from the host?')) return;
  EX.sel.clear();
  apiDelete(ids,()=>toast(ids.length+' item(s) deleted'));
}
/* Repaint selection in place. Rebuilding innerHTML here would destroy the
   node between the two clicks of a double-click, so dblclick would never fire. */
function paintSelection(){
  $('#rowsx').querySelectorAll('.fr').forEach(r=>{
    const i=+r.dataset.i;
    r.classList.toggle('sel', EX.sel.has(i));
    r.classList.toggle('cursor', EX.cursor===i);
  });
  $('#stSel').textContent = EX.sel.size ? ('· '+EX.sel.size+' selected') : '';
  renderDetail();
}
function exSelect(i,ev){
  if(ev && ev.shiftKey && EX.cursor>=0){
    const a=Math.min(EX.cursor,i), b=Math.max(EX.cursor,i);
    EX.sel.clear(); for(let k=a;k<=b;k++) EX.sel.add(k);
  } else if(ev && (ev.ctrlKey||ev.metaKey)){
    EX.sel.has(i)?EX.sel.delete(i):EX.sel.add(i);
    EX.cursor=i;
  } else {
    EX.sel.clear(); EX.sel.add(i); EX.cursor=i;
  }
  paintSelection();
}
/* Open whatever is at index i, the way a file manager does. */
function exActivate(i){
  const l=exList(), f=l[i];
  if(!f) return;
  if(f.dir) go(EX.path+f.name+'/');
  else openPreview(f);
}

/* ── wiring ── */
/* Clicking an item that is already part of a multi-selection must not collapse
   that selection on mousedown — otherwise you can never drag a group. Windows
   defers the collapse to mouseup, so do the same. */
let pendingCollapse=-1;
$('#rowsx').addEventListener('mousedown',e=>{
  const r=e.target.closest('.fr');
  if(!r) return;
  const i=+r.dataset.i;
  if(e.button!==0){ if(!EX.sel.has(i)) exSelect(i,null); return; }
  if(e.ctrlKey||e.metaKey||e.shiftKey){ exSelect(i,e); return; }
  if(EX.sel.has(i)&&EX.sel.size>1){ pendingCollapse=i; return; }
  exSelect(i,e);
});
$('#rowsx').addEventListener('mouseup',e=>{
  if(pendingCollapse<0) return;
  const r=e.target.closest('.fr');
  if(r && +r.dataset.i===pendingCollapse) exSelect(pendingCollapse,null);
  pendingCollapse=-1;
});
$('#rowsx').addEventListener('dblclick',e=>{
  const r=e.target.closest('.fr');
  if(!r){ goUp(); return; }          /* double-click empty space = up one level */
  e.preventDefault();
  exActivate(+r.dataset.i);
});
$('#treeInner').addEventListener('click',e=>{
  const tw=e.target.closest('[data-toggle]');
  if(tw){ const k=tw.dataset.toggle; EX.open[k]=EX.open[k]===false; renderTree(); return; }
  const n=e.target.closest('.tnode');
  if(n) go(n.dataset.p);
});
$('#navBack').onclick=goBack; $('#navFwd').onclick=goFwd;
$('#navUp').onclick=goUp;     $('#navRefresh').onclick=()=>{ renderExplorer(); toast('Refreshed'); };
$('#expQ').addEventListener('input',e=>{ EX.q=e.target.value; renderRows(); });
$('#expZip').onclick=()=>toast(EX.sel.size?`Zipping ${EX.sel.size} selected…`:`Zipping ${exList().length} items…`);
$('#detailClose').onclick=()=>{ EX.detailOpen=false; store.set('expDetail',false); renderExplorer(); };
$('#expDetailBtn').onclick=()=>{ EX.detailOpen=!EX.detailOpen; store.set('expDetail',EX.detailOpen); renderExplorer(); };
$('#expTreeBtn').onclick=()=>$('#tree').classList.toggle('open');
$('#expToShare').onclick=()=>gotoView('share');

/* pane resizing */
function paneDrag(grip, get, set, invert){
  grip.addEventListener('pointerdown',e=>{
    e.preventDefault(); grip.setPointerCapture(e.pointerId);
    const x0=e.clientX, w0=get();
    const mv=e2=>set(Math.min(520,Math.max(150, w0 + (invert? (x0-e2.clientX) : (e2.clientX-x0)))));
    const up=()=>{ document.removeEventListener('pointermove',mv); };
    document.addEventListener('pointermove',mv);
    document.addEventListener('pointerup',up,{once:true});
  });
}
paneDrag($('#treeGrip'),   ()=>EX.treeW,   w=>{ EX.treeW=w;   $('#tree').style.width=w+'px';   store.set('expTreeW',w); }, false);
paneDrag($('#detailGrip'), ()=>EX.detailW, w=>{ EX.detailW=w; $('#detail').style.width=w+'px'; store.set('expDetailW',w); }, true);

/* rubber-band selection */
(function(){
  const area=$('#rowsx');
  let band=null, sx=0, sy=0;
  area.addEventListener('pointerdown',e=>{
    if(e.target.closest('.fr')) return;
    if(e.button!==0) return;
    const ar=area.getBoundingClientRect();
    sx=e.clientX; sy=e.clientY;
    band=document.createElement('div'); band.className='band';
    area.appendChild(band);
    if(!e.ctrlKey&&!e.metaKey){ EX.sel.clear(); paintSelection(); }
    const mv=e2=>{
      const x=Math.min(sx,e2.clientX), y=Math.min(sy,e2.clientY);
      const w=Math.abs(e2.clientX-sx), h=Math.abs(e2.clientY-sy);
      band.style.cssText=`left:${x-ar.left}px;top:${y-ar.top+area.scrollTop}px;width:${w}px;height:${h}px`;
      const box={left:x,top:y,right:x+w,bottom:y+h};
      EX.sel.clear();
      area.querySelectorAll('.fr').forEach(r=>{
        const rr=r.getBoundingClientRect();
        if(rr.left<box.right&&rr.right>box.left&&rr.top<box.bottom&&rr.bottom>box.top){
          EX.sel.add(+r.dataset.i); r.classList.add('sel');
        } else r.classList.remove('sel');
      });
      $('#stSel').textContent = EX.sel.size ? ('· '+EX.sel.size+' selected') : '';
    };
    const up=()=>{ document.removeEventListener('pointermove',mv); if(band){band.remove();band=null;} renderDetail(); };
    document.addEventListener('pointermove',mv);
    document.addEventListener('pointerup',up,{once:true});
  });
})();

/* keyboard */
document.addEventListener('keydown',e=>{
  if(document.body.dataset.view!=='explorer') return;
  if(/^(INPUT|TEXTAREA)$/.test(e.target.tagName)) return;
  if($('#modalWrap').classList.contains('on')) return;
  const l=exList();
  if(e.altKey && e.key==='ArrowLeft'){ e.preventDefault(); return goBack(); }
  if(e.altKey && e.key==='ArrowRight'){ e.preventDefault(); return goFwd(); }
  if(e.key==='ArrowDown'||e.key==='ArrowUp'){
    e.preventDefault();
    let i=EX.cursor + (e.key==='ArrowDown'?1:-1);
    i=Math.max(0,Math.min(l.length-1,i));
    if(e.shiftKey){ EX.sel.add(i); } else { EX.sel.clear(); EX.sel.add(i); }
    EX.cursor=i; paintSelection();
    const r=$('#rowsx').querySelector('.fr[data-i="'+i+'"]'); if(r) r.scrollIntoView({block:'nearest'});
  } else if(e.key==='Home'||e.key==='End'){
    e.preventDefault();
    const i=e.key==='Home'?0:l.length-1;
    EX.sel.clear(); EX.sel.add(i); EX.cursor=i; paintSelection();
    const r=$('#rowsx').querySelector('.fr[data-i="'+i+'"]'); if(r) r.scrollIntoView({block:'nearest'});
  } else if(e.key==='Enter'){ e.preventDefault(); exActivate(EX.cursor>=0?EX.cursor:[...EX.sel][0]); }
  else if(e.key==='Delete'){ if(EX.sel.size){ e.preventDefault(); exDelete(); } }
  else if(e.key==='Backspace'){ e.preventDefault(); goUp(); }
  else if(e.key==='F2'){ e.preventDefault(); toast('Rename — server-side, not in the mock'); }
  else if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='a'){
    e.preventDefault(); EX.sel.clear(); l.forEach((_,i)=>EX.sel.add(i)); paintSelection();
  }
  else if(e.key.length===1 && !e.ctrlKey && !e.metaKey && !e.altKey){
    /* type-ahead: jump to the next item starting with what you typed */
    const now=performance.now();
    EX._ta = (now - (EX._taAt||0) < 900) ? (EX._ta||'')+e.key.toLowerCase() : e.key.toLowerCase();
    EX._taAt = now;
    const from=(EX.cursor+ (EX._ta.length>1?0:1)) || 0;
    for(let k=0;k<l.length;k++){
      const i=(from+k)%l.length;
      if(l[i].name.toLowerCase().startsWith(EX._ta)){
        EX.sel.clear(); EX.sel.add(i); EX.cursor=i; paintSelection();
        const r=$('#rowsx').querySelector('.fr[data-i="'+i+'"]'); if(r) r.scrollIntoView({block:'nearest'});
        break;
      }
    }
  }
});

/* ── explorer right-click ── */
$('#rowsx').addEventListener('contextmenu',e=>{
  e.preventDefault();
  const r=e.target.closest('.fr');
  const l=exList();
  if(r){
    const i=+r.dataset.i;
    if(!EX.sel.has(i)) exSelect(i,null);
    const f=l[i], n=EX.sel.size;
    if(n>1) return CTX.open(e,[
      {head:n+' items selected'},
      {icon:'i-zip',   label:'Download '+n+' as ZIP', on:()=>toast(`Zipping ${n} items…`)},
      {icon:'i-copy',  label:'Copy names', on:()=>{ navigator.clipboard?.writeText([...EX.sel].map(k=>l[k].name).join('\n')); toast('Copied'); }},
      {sep:true},
      {icon:'i-trash', label:'Delete '+n+' items', danger:true, on:exDelete},
    ]);
    return CTX.open(e,[
      {head:f.dir?f.name:f},
      f.dir ? {icon:'i-folder', label:'Open', hint:'Enter', on:()=>go(EX.path+f.name+'/')}
            : {icon:'i-eye',    label:'Preview', hint:'Enter', on:()=>openPreview(f)},
      {icon:'i-dl',   label:f.dir?'Download as ZIP':'Download', on:()=>{ if(f.dir) location.href='/download_ff_zip?path='+encodeURIComponent(realPath(EX.path)+'\\'+f.name); else if(f.dlUrl) location.href=f.dlUrl; }},
      {icon:'i-link', label:'Copy direct link', on:()=>{ navigator.clipboard?.writeText(location.origin+(f.dlUrl||'')); toast('Link copied'); }},
      {sep:true},
      {icon:'i-folder', label:'Open containing folder', hostOnly:true, on:()=>toast('/open  →  '+realPath(EX.path))},
      {icon:'i-copy',   label:'Copy full path', on:()=>{ navigator.clipboard?.writeText(realPath(EX.path)+'\\'+f.name); toast('Path copied'); }},
      {icon:'i-sort',   label:'Rename', hint:'F2', on:()=>toast('Rename — server-side, not in the mock')},
      {sep:true},
      {icon:'i-info',  label:'Details', checked:EX.detailOpen, on:()=>{ EX.detailOpen=!EX.detailOpen; store.set('expDetail',EX.detailOpen); renderExplorer(); }},
      {icon:'i-trash', label:'Delete', hint:'Del', danger:true, on:exDelete},
    ]);
  }
  /* blank area */
  CTX.open(e,[
    {head:pathLabel(EX.path).slice(-1)[0]+' · '+l.length+' items'},
    {icon:'i-zip',    label:'Download folder as ZIP', on:()=>toast(`Zipping ${l.length} items…`)},
    {icon:'i-cbx',    label:'Select all', hint:'Ctrl+A', on:()=>{ EX.sel.clear(); l.forEach((_,i)=>EX.sel.add(i)); paintSelection(); }},
    {icon:'i-loop',   label:'Refresh', on:()=>{ renderExplorer(); toast('Refreshed'); }},
    {sep:true},
    ...[['name','Name'],['size','Size'],['type','Type'],['time','Date added']].map(([k,lb])=>({
      icon:'i-sort', label:'Sort by '+lb, checked:EX.sort===k,
      on:()=>{ if(EX.sort===k) EX.dir*=-1; else EX.sort=k; renderExplorer(); }})),
    {sep:true},
    {icon:'i-folder', label:'Open in Explorer', hostOnly:true, on:()=>toast('/open  →  '+realPath(EX.path))},
    {icon:'i-copy',   label:'Copy folder path', on:()=>{ navigator.clipboard?.writeText(realPath(EX.path)); toast('Path copied'); }},
    {icon:'i-info',   label:'Details pane', checked:EX.detailOpen, on:()=>{ EX.detailOpen=!EX.detailOpen; store.set('expDetail',EX.detailOpen); renderExplorer(); }},
  ]);
});
$('#treeInner').addEventListener('contextmenu',e=>{
  e.preventDefault();
  const n=e.target.closest('.tnode'); if(!n) return;
  const p=n.dataset.p;
  CTX.open(e,[
    {head:n.querySelector('.nm').textContent},
    {icon:'i-folder', label:'Open', on:()=>go(p)},
    {icon:'i-zip',    label:'Download as ZIP', on:()=>toast('Zipping folder…')},
    {sep:true},
    {icon:'i-folder', label:'Open in Explorer', hostOnly:true, on:()=>toast('/open  →  '+realPath(p))},
    {icon:'i-copy',   label:'Copy path', on:()=>{ navigator.clipboard?.writeText(realPath(p)); toast('Path copied'); }},
  ]);
});

/* ── view switching + last-endpoint memory ── */
function setView(v){
  document.body.dataset.view=v;
  store.set('lastView', v);
  if(v==='explorer'){
    $('#expBrand').innerHTML=LOGOS[S.logo].html;
    renderExplorer();
  }
}




/* ══════════════════════════════════════════════════════════════
   SERVER DATA LAYER
   ══════════════════════════════════════════════════════════════ */

/* The server stores Windows-safe encoded filenames on disk and in
   database.json. Illegal chars become fullwidth Unicode equivalents.
   This mirrors decodeFilenameFromDisk() in http_server.cpp — keep in sync. */
function decodeDisplayName(name){
  return String(name)
    .replace(/｜/g,'|').replace(/＊/g,'*').replace(/？/g,'?')
    .replace(/＂/g,'"').replace(/＜/g,'<').replace(/＞/g,'>').replace(/：/g,':');
}
function parseTs(s){
  const m=/^(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2}):(\d{2})/.exec(s||'');
  return m ? new Date(+m[1],+m[2]-1,+m[3],+m[4],+m[5],+m[6]).getTime() : Date.now();
}

/* Formats the browser can decode itself. Anything else falls back to an icon
   and a download button rather than a dead <video>. */
const NATIVE_IMG = new Set(['jpg','jpeg','png','gif','webp','bmp','svg','avif','ico']);
const NATIVE_VID = new Set(['mp4','webm','ogv','mov','m4v']);
const NATIVE_AUD = new Set(['mp3','wav','ogg','flac','m4a','aac','opus','aiff','aif']);

const SRV = { is_host:false, ips:[], saving_dir:'', port:(location.port||'80') };

function dbUrls(id){
  return { url:'/preview_inline?id='+encodeURIComponent(id),
           dlUrl:'/download?id='+encodeURIComponent(id) };
}
function ffUrls(p){
  return { url:'/preview_inline_ff?path='+encodeURIComponent(p),
           dlUrl:'/download_ff?path='+encodeURIComponent(p) };
}

/* /api/database -> FILES (share list) + FFS (share tree) + FTREE (explorer) */
function ingestDb(d){
  FILES = (d.files||[]).map(f=>{
    const name = decodeDisplayName(f.name);
    const u = dbUrls(f.id);
    const isImg = NATIVE_IMG.has(ext(name));
    return { id:f.id, name, size:+f.size||0, t:parseTs(f.timestamp),
             from:f.from||'', url:u.url, dlUrl:u.dlUrl,
             th: isImg ? ('/thumb?id='+encodeURIComponent(f.id)) : null };
  });

  const ffs = d.forwarding_folders||[];
  FFS = []; FTREE = {};
  ffs.forEach(ff=>{
    const kids = ff.tree ? convTree(ff.tree)
                         : (ff.contents||[]).map(c=>ffLeaf(c.path||c, c.name||c, +c.size||0));
    FFS.push({ id:ff.id, path:ff.path, open:true, kids });
    FTREE[ff.id] = { path:ff.path, kids };
  });
}
function ffLeaf(path, rawName, size, mtime){
  const name = decodeDisplayName(rawName);
  const u = ffUrls(path);
  const isImg = NATIVE_IMG.has(ext(name));
  return { n:name, s:size, t:mtime||Date.now(), _p:path,
           url:u.url, dlUrl:u.dlUrl,
           th: isImg ? ('/thumb_ff?path='+encodeURIComponent(path)) : null };
}
function convTree(node){
  return (node.children||[]).map(c=> c.isDir
    ? { d:decodeDisplayName(c.name), t:Date.now(), _p:c.path, kids:convTree(c) }
    : ffLeaf(c.path, c.name, +c.size||0));
}

function loadDatabase(){
  fetch('/api/database').then(r=>r.json()).then(d=>{
    ingestDb(d);
    loading=false;
    if(document.body.dataset.view==='explorer') renderExplorer();
    else { renderDb(false); renderFf(); }
  }).catch(()=>{ loading=false; renderDb(false); });
}

/* ── /api/info ── */
fetch('/api/info').then(r=>r.json()).then(d=>{
  SRV.ips = d.ips||[];
  SRV.saving_dir = d.saving_dir||'';
  SRV.is_host = !!d.is_host;
  IS_HOST = SRV.is_host;
  const list=$('#urlList');
  if(list) list.innerHTML = SRV.ips.map(ip=>
    `<button class="url"><svg class="ic"><use href="#i-link"/></svg>`+
    `<code>http://${esc(ip)}:${SRV.port}/</code><span class="cp">copy</span></button>`).join('');
  const ds=$('#dropSub');
  if(ds && SRV.saving_dir) ds.textContent='Any type · saved to '+SRV.saving_dir;
  const sd=$('#iSavingDir'); if(sd) sd.textContent=SRV.saving_dir;
}).catch(()=>{});

/* Copy an access URL by clicking its card */
document.addEventListener('click',e=>{
  const u=e.target.closest('.url'); if(!u) return;
  const code=u.querySelector('code'); if(!code) return;
  navigator.clipboard?.writeText(code.textContent);
  toast('Copied '+code.textContent);
});

/* ── stats + disk ── */
function updateStats(){
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    const set=(id,v)=>{ const e=$(id); if(e) e.textContent=v; };
    set('#stPortVal', SRV.port);
    set('#stFiles', d.files);
    set('#stBytes', fmt(d.bytes));
    set('#stClients', d.clients);
    const ic=$('#iClients'); if(ic) ic.textContent=d.clients;
  }).catch(()=>{});
}
function loadDiskSpace(){
  fetch('/api/disk_space').then(r=>r.json()).then(d=>{
    const freeB=+d.disk_free, totB=+d.disk_total, capB=+d.storage_cap, dbUsed=+(d.db_used||0);
    const pool = capB>0 ? capB : totB;
    const pct  = pool>0 ? Math.min(100, dbUsed/pool*100) : 0;
    const set=(id,v)=>{ const e=$(id); if(e) e.textContent=v; };
    set('#swUsed', fmt(dbUsed));
    set('#swMeta', 'of '+(capB>0?fmt(capB)+' cap':fmt(totB))+' · '+fmt(freeB)+' free on '+(d.disk_root||''));
    set('#iUsed', fmt(dbUsed)); set('#iCap', capB>0?fmt(capB):'None');
    set('#iMaxUp', fmt(+d.max_upload)); set('#iFree', fmt(freeB));
    const bar=$('#storBar');
    if(bar){ bar.style.width=pct.toFixed(1)+'%';
             bar.className = pct>90?'full':pct>70?'warn':''; }
    $$('.status .free').forEach(e=>e.textContent=fmt(freeB)+' free');
  }).catch(()=>{});
}
setInterval(updateStats, 4000);
setInterval(loadDiskSpace, 5000);

/* ── SSE ── */
let sse=null, sseWatchdog=null, sseBackoff=1000;
const SSE_TIMEOUT=35000;
function resetSseWatchdog(){ clearTimeout(sseWatchdog); sseWatchdog=setTimeout(reconnectSSE, SSE_TIMEOUT); }
function teardownSSE(){ clearTimeout(sseWatchdog); if(sse){ sse.onerror=null; sse.close(); sse=null; } }
function reconnectSSE(){ teardownSSE(); connectSSE(); }
function connectSSE(){
  teardownSSE();
  sse=new EventSource('/events');
  sse.addEventListener('db_update',e=>{
    sseBackoff=1000; resetSseWatchdog();
    try{
      ingestDb(JSON.parse(e.data));
      loading=false;
      if(document.body.dataset.view==='explorer') renderExplorer();
      else { renderDb(); renderFf(); }
    }catch(_){}
  });
  sse.addEventListener('pastebin_update',e=>{
    sseBackoff=1000; resetSseWatchdog();
    try{ const d=JSON.parse(e.data); const ta=$('#pasteTA');
         if(ta && !pasteLocalEdit){ ta.value=d.content; pasteUpdateMeta(); } }catch(_){}
  });
  sse.onopen=()=>{ sseBackoff=1000; resetSseWatchdog(); };
  sse.onmessage=()=>{ sseBackoff=1000; resetSseWatchdog(); };
  sse.onerror=()=>{ teardownSSE(); setTimeout(connectSSE, Math.min(sseBackoff,15000));
                    sseBackoff=Math.min(sseBackoff*2,15000); };
  resetSseWatchdog();
}
document.addEventListener('visibilitychange',()=>{
  if(document.visibilityState==='visible'){ reconnectSSE(); loadDiskSpace(); updateStats(); loadDatabase(); }
});


/* one card per file accepted this session */
function addSent(f){
  if(!f) return;
  const d=document.createElement("div");
  d.className="sent-item";
  d.innerHTML = thumbHTML(f)
    + `<div class="m"><b>${esc(f.name)}</b><span>${fmt(f.size)} · just now · ${esc(SRV.saving_dir||"")}</span></div>`
    + `<span class="tag"><svg class="ic sm"><use href="#i-check"/></svg> SAVED</span>`;
  const list=$("#sentList");
  if(list){ list.prepend(d); wireHoverPreview(d); }
}

/* ══════════════════════════════════════════════════════════════ UPLOAD */
function uploadFiles(files){
  if(!files||!files.length) return;
  const meta={};
  for(const f of files) meta[f.name]={lastModified:f.lastModified,size:f.size};
  const fd=new FormData();
  for(const f of files) fd.append('files',f);
  fd.append('metadata',JSON.stringify(meta));

  const wrap=$('#prog'); wrap.classList.add('on');
  $('#progT').textContent=`Uploading ${files.length} file${files.length>1?'s':''}…`;
  const ring=$('#ringFg'), pctEl=$('#ringPct'), bar=$('#progBar'), sts=$('#progS');
  const xhr=new XMLHttpRequest();
  const t0=Date.now();
  xhr.upload.onprogress=e=>{
    if(!e.lengthComputable) return;
    const p=e.loaded/e.total*100, dt=Math.max(0.1,(Date.now()-t0)/1000);
    pctEl.textContent=Math.round(p)+'%';
    ring.style.strokeDashoffset=132-(132*p/100);
    bar.style.width=p+'%';
    sts.textContent=fmt(e.loaded)+' / '+fmt(e.total)+' · '+fmt(e.loaded/dt)+'/s';
  };
  xhr.onload=()=>{
    wrap.classList.remove('on');
    bar.style.width='0%'; ring.style.strokeDashoffset=132; pctEl.textContent='0%';
    if(xhr.status===200){
      try{
        const res=JSON.parse(xhr.responseText);
        (res.files||[]).forEach(f=>addSent({name:decodeDisplayName(f.name),size:+f.size,t:Date.now()}));
        toast(res.files.length+' file(s) saved');
        updateStats(); loadDiskSpace(); loadDatabase();
      }catch(e){ toast('Could not parse the server reply',true); }
    } else toast('Upload failed: HTTP '+xhr.status,true);
  };
  xhr.onerror=()=>{ wrap.classList.remove('on'); toast('Network error during upload',true); };
  xhr.open('POST','/upload'); xhr.send(fd);
}
$('#fileInput').addEventListener('change',e=>uploadFiles(e.target.files));
$('#btnChoose').onclick=e=>{ e.stopPropagation(); $('#fileInput').click(); };
$('#progCancel').onclick=()=>location.reload();
{
  const dz=$('#dz');
  ['dragenter','dragover'].forEach(ev=>dz.addEventListener(ev,e=>{e.preventDefault();dz.classList.add('over');}));
  ['dragleave','drop'].forEach(ev=>dz.addEventListener(ev,e=>{e.preventDefault();dz.classList.remove('over');}));
  dz.addEventListener('drop',e=>uploadFiles(e.dataTransfer.files));
  dz.addEventListener('click',()=>$('#fileInput').click());
}

/* ══════════════════════════════════════════════════════════════ PASTEBIN */
let pasteLocalEdit=false, pasteEditTimer=null, pasteSyncTimer=null;
function loadPastebin(){
  fetch('/api/pastebin').then(r=>r.json()).then(d=>{ $('#pasteTA').value=d.content||''; pasteUpdateMeta(); }).catch(()=>{});
}
function pasteUpdateMeta(){ $('#pasteCount').textContent=$('#pasteTA').value.length+' chars'; }
function setSyncLabel(synced){
  const el=$('#syncLbl');
  el.className='sync'+(synced?'':' busy');
  el.innerHTML='<i></i> '+(synced?'synced':'syncing…');
}
function pasteSyncNow(){
  setSyncLabel(false);
  fetch('/api/pastebin',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({content:$('#pasteTA').value})})
    .then(()=>{ pasteLocalEdit=false; setSyncLabel(true); }).catch(()=>setSyncLabel(true));
}
$('#pasteTA').addEventListener('input',()=>{
  pasteLocalEdit=true; pasteUpdateMeta(); setSyncLabel(false);
  clearTimeout(pasteSyncTimer); pasteSyncTimer=setTimeout(pasteSyncNow,300);
  clearTimeout(pasteEditTimer); pasteEditTimer=setTimeout(()=>{pasteLocalEdit=false;},600);
});
$('#pasteCopy').onclick=()=>{ navigator.clipboard?.writeText($('#pasteTA').value); toast('Copied to clipboard'); };
$('#pasteClear').onclick=()=>{ $('#pasteTA').value=''; pasteUpdateMeta(); pasteSyncNow(); };

/* ══════════════════════════════════════════════════════════════ DELETE / ZIP */
function apiDelete(ids, done){
  let left=ids.length;
  if(!left) return;
  ids.forEach(id=>{
    fetch('/api/database/delete',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({id,delete_file:true})})
      .then(r=>r.json()).catch(()=>({}))
      .then(()=>{ if(--left===0){ loadDatabase(); done&&done(); } });
  });
}
function zipIds(ids,label){
  if(!ids.length) return;
  const qs='ids='+encodeURIComponent(ids.join(','));
  toast('Preparing '+(label||ids.length+' files')+'…');
  location.href='/download_db_zip?'+qs;
}

/* ══════════════════════════════════════════════════════════════
   VIEW ROUTING  —  /share and /database, with / remembering
   ══════════════════════════════════════════════════════════════ */
function viewFromPath(){
  const p=location.pathname.replace(/\/+$/,'') || '/';
  if(p==='/database') return 'explorer';
  if(p==='/share')    return 'share';
  return null;                       // '/' — decide from the saved preference
}
function gotoView(v, push){
  const path = v==='explorer' ? '/database' : '/share';
  if(push!==false && location.pathname!==path){
    try{ history.pushState({v}, '', path + (v==='explorer'?exQuery():'')); }catch(e){}
  }
  setView(v);
}
function exQuery(){
  return '?path='+encodeURIComponent(EX.path)+'&sort='+EX.sort+(EX.dir<0?'_desc':'_asc');
}
function syncExUrl(){
  if(document.body.dataset.view!=='explorer') return;
  try{ history.replaceState({v:'explorer'}, '', '/database'+exQuery()); }catch(e){}
}
window.addEventListener('popstate',()=>{
  const v=viewFromPath() || store.get('lastView','share');
  applyExFromQuery();
  setView(v);
});
function applyExFromQuery(){
  const q=new URLSearchParams(location.search);
  const p=q.get('path'); const s=q.get('sort');
  if(p) EX.path=p;
  if(s){ EX.dir = s.endsWith('_asc')?1:-1; EX.sort = s.replace(/_(asc|desc)$/,''); }
}

/* ══════════════════════════════════════════════════════════════ BOOT */
(function boot(){
  const fromPath = viewFromPath();
  if(fromPath===null){
    /* '/' — send this browser back where it was, but only if it has been here */
    const last=store.get('lastView',null);
    if(last==='explorer'){ location.replace('/database'); return; }
    if(last==='share')   { location.replace('/share');    return; }
  }
  applyExFromQuery();
  connectSSE();
  updateStats();
  loadDiskSpace();
  loadDatabase();
  loadPastebin();
  setView(fromPath || 'share');
})();

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
// True when the request came from this machine — loopback, or one of our own
// LAN addresses (you can browse the host box through its own IP).
static bool clientIsHost(const std::string& ip) {
    if (ip == "127.0.0.1" || ip == "::1" || ip == "localhost") return true;
    for (const auto& mine : getLocalIPs()) if (mine == ip) return true;
    return false;
}

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

static void sendFileInline(SOCKET s, const std::string& filePath, const std::string& name,
                           const std::string& reqHeaders = "") {
    HANDLE hf = CreateFileW(toWide(filePath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) { sendResp(s, 404, "text/plain", "Not found"); return; }
    LARGE_INTEGER sz; GetFileSizeEx(hf, &sz);
    int64_t fileSize = sz.QuadPart;

    // Limit inline preview to 20MB for non-media types
    const int64_t LIMIT = 20LL*1024*1024;
    std::string mime;
    bool isMedia = false;
    {
        size_t dot = name.rfind('.');
        std::string ext = (dot != std::string::npos) ? name.substr(dot+1) : "";
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        mime = mimeForExt(ext);
        isMedia = (mime.rfind("video/",0)==0 || mime.rfind("audio/",0)==0);
    }

    // Parse Range header — required for video/audio seeking in browsers
    std::string rangeHdr = getHeaderVal(reqHeaders, "Range");
    int64_t rangeStart = 0, rangeEnd = -1;
    bool hasRange = false;

    if (!rangeHdr.empty() && rangeHdr.rfind("bytes=", 0) == 0) {
        std::string spec = rangeHdr.substr(6);
        size_t dash = spec.find('-');
        if (dash != std::string::npos) {
            std::string startStr = spec.substr(0, dash);
            std::string endStr   = spec.substr(dash + 1);
            try {
                if (!startStr.empty()) {
                    rangeStart = std::stoll(startStr);
                    rangeEnd   = endStr.empty() ? fileSize - 1 : std::stoll(endStr);
                } else if (!endStr.empty()) {
                    // suffix range: last N bytes
                    int64_t suffix = std::stoll(endStr);
                    rangeStart = std::max(0LL, fileSize - suffix);
                    rangeEnd   = fileSize - 1;
                }
                rangeStart = std::max(0LL, std::min(rangeStart, fileSize - 1));
                rangeEnd   = std::max(rangeStart, std::min(rangeEnd, fileSize - 1));
                hasRange = true;
            } catch (...) { hasRange = false; }
        }
    }

    int64_t sendStart  = hasRange ? rangeStart : 0;
    int64_t sendEnd    = hasRange ? rangeEnd   : (isMedia ? fileSize - 1 : std::min(fileSize, LIMIT) - 1);
    int64_t sendLength = sendEnd - sendStart + 1;

    // Seek to the requested start position
    if (sendStart > 0) {
        LARGE_INTEGER liPos; liPos.QuadPart = sendStart;
        SetFilePointerEx(hf, liPos, nullptr, FILE_BEGIN);
    }

    std::ostringstream hdr;
    if (hasRange) {
        hdr << "HTTP/1.1 206 Partial Content\r\n"
            << "Content-Type: " << mime << "\r\n"
            << "Content-Disposition: inline\r\n"
            << "Content-Range: bytes " << sendStart << "-" << sendEnd << "/" << fileSize << "\r\n"
            << "Content-Length: " << sendLength << "\r\n"
            << "Accept-Ranges: bytes\r\n"
            << "Cache-Control: no-cache\r\n"
            << "Connection: close\r\n"
            << "Access-Control-Allow-Origin: *\r\n\r\n";
    } else {
        hdr << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: " << mime << "\r\n"
            << "Content-Disposition: inline\r\n"
            << "Content-Length: " << sendLength << "\r\n"
            << "Accept-Ranges: bytes\r\n"
            << "Cache-Control: max-age=60\r\n"
            << "Connection: close\r\n"
            << "Access-Control-Allow-Origin: *\r\n\r\n";
    }
    std::string h = hdr.str();
    send(s, h.data(), (int)h.size(), 0);

    char chunk[65536];
    DWORD readBytes = 0;
    int64_t remaining = sendLength;
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

    if (route == "/" || route == "/index.html" || route == "/share" || route == "/database") {
        // One page, two views. The client picks its view from the path and
        // redirects "/" to whichever endpoint that browser was last in.
        sendResp(s, 200, "text/html; charset=utf-8", HTML_PAGE);

    } else if (route == "/api/info") {
        auto ips = getLocalIPs();
        std::string json = "{\"ips\":[";
        for (size_t i = 0; i < ips.size(); ++i) {
            json += "\"" + ips[i] + "\"";
            if (i + 1 < ips.size()) json += ",";
        }
        json += "],\"saving_dir\":\"" + jsonEscape(savingDir) + "\",";
        json += "\"is_host\":" + std::string(clientIsHost(clientIP) ? "true" : "false") + "}";
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

    } else if (route == "/thumb") {
        // Thumbnail for a database image. Same file as the preview for now.
        std::string id = queryParam(path, "id");
        std::string filePath, name;
        { std::lock_guard<std::mutex> lk(g_dbMtx);
          for (auto& e : g_database) if (e.id == id) { filePath=e.savedPath; name=e.name; break; } }
        if (filePath.empty()) sendResp(s, 404, "text/plain", "Not found");
        else sendFileInline(s, filePath, decodeFilenameFromDisk(name), hdrs);

    } else if (route == "/thumb_ff") {
        // Thumbnail for a forwarding-folder image. MUST repeat the membership
        // check — 'path' comes from the client.
        std::string fp = queryParam(path, "path");
        bool allowed = false;
        { std::lock_guard<std::mutex> lk(g_ffMtx);
          for (auto& ff : g_ffFolders) {
              for (auto& c : ff.contents) if (c == fp) { allowed = true; break; }
              if (!allowed && ff.subfoldersEnabled && fp.rfind(ff.path, 0) == 0) allowed = true;
              if (allowed) break;
          } }
        if (!allowed) sendResp(s, 403, "text/plain", "Not in any forwarding folder");
        else {
            size_t sl = fp.rfind('\\');
            if (sl == std::string::npos) sl = fp.rfind('/');
            std::string bn = (sl != std::string::npos) ? fp.substr(sl+1) : fp;
            sendFileInline(s, fp, decodeFilenameFromDisk(bn), hdrs);
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
            else sendFileInline(s, filePath, decodeFilenameFromDisk(name), hdrs);
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
                sendFileInline(s, fp, decodeFilenameFromDisk(bn), hdrs);
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

    } else if (route == "/download_db_zip") {
        // Bulk download: zip the database entries named in ?ids=a,b,c
        std::string ids = urlDecode(queryParam(path, "ids"));
        if (ids.empty()) { sendResp(s, 400, "text/plain", "Missing ids"); }
        else {
            std::vector<std::string> want;
            { size_t p0 = 0;
              while (p0 <= ids.size()) {
                  size_t c = ids.find(',', p0);
                  if (c == std::string::npos) c = ids.size();
                  std::string one = trim(ids.substr(p0, c - p0));
                  if (!one.empty()) want.push_back(one);
                  p0 = c + 1;
              } }
            ZipBuilder zb;
            int added = 0;
            {
                std::lock_guard<std::mutex> lk(g_dbMtx);
                for (auto& id : want) {
                    for (auto& e : g_database) {
                        if (e.id != id) continue;
                        // decoded name inside the archive, so it opens with the
                        // original filename rather than the fullwidth substitutes
                        std::string entry = decodeFilenameFromDisk(e.name);
                        for (char& ch : entry) if (ch == '\\') ch = '/';
                        if (zb.addFile(entry, e.savedPath)) ++added;
                        break;
                    }
                }
            }
            if (!added) { sendResp(s, 404, "text/plain", "No matching files"); }
            else {
                auto zipData = zb.finish();
                Log(L_NET, "ZIP download: " + std::to_string(added) + " file(s) -> " + clientIP);
                std::ostringstream hdr;
                hdr << "HTTP/1.1 200 OK\r\n"
                    << "Content-Type: application/zip\r\n"
                    << "Content-Disposition: attachment; filename=\"localTransfer.zip\"\r\n"
                    << "Content-Length: " << zipData.size() << "\r\n"
                    << "Connection: close\r\n"
                    << "Access-Control-Allow-Origin: *\r\n\r\n";
                std::string hs = hdr.str();
                send(s, hs.data(), (int)hs.size(), 0);
                send(s, (const char*)zipData.data(), (int)zipData.size(), 0);
            }
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
    g_srvListenSocket = srv;

    while (g_running && !g_srvStop) {
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
    g_srvListenSocket = INVALID_SOCKET;
    Log(L_INFO, "Server thread stopped.");
}