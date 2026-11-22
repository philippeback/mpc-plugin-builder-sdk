#!/usr/bin/env python3
"""Render an interactive local HTML5 preview of the Mushin MPC Plugin Skins UI.

Usage:
    python plugins/Mushin/bin/preview_ui.py [--no-open] [--out PATH]

Reads:
    - plugins/Mushin/build/PhilippeBack - VST - Mushin/Plugin Skins/TUI.json
    - plugins/Mushin/build/PhilippeBack - VST - Mushin/Plugin Skins/Q-Links.json
    - plugins/Mushin/build/params.json
    - PNG filmstrips in Plugin Skins/

Generates a self-contained HTML5 Canvas previewer (1280x628 MPC touch screen)
with interactive knobs, faders, switches, Q-Link bank focus rectangles, and
a "Save PNG" export button. Standard library only.
"""

import argparse
import base64
import json
import os
import sys
import webbrowser

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGIN_DIR = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(PLUGIN_DIR, "build")
SKIN_DIR = os.path.join(BUILD_DIR, "PhilippeBack - VST - Mushin", "Plugin Skins")
TUI_PATH = os.path.join(SKIN_DIR, "TUI.json")
QLINKS_PATH = os.path.join(SKIN_DIR, "Q-Links.json")
PARAMS_PATH = os.path.join(BUILD_DIR, "params.json")
DEFAULT_OUT = os.path.join(BUILD_DIR, "ui_preview.html")


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def encode_pngs(skin_dir):
    images = {}
    for name in sorted(os.listdir(skin_dir)):
        if name.lower().endswith(".png"):
            p = os.path.join(skin_dir, name)
            with open(p, "rb") as f:
                b64 = base64.b64encode(f.read()).decode("ascii")
            images[name] = "data:image/png;base64," + b64
    return images


HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Mushin — MPC Standalone UI Preview</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #07090b;
    color: #f0f4f8;
    font-family: "Titillium Web", "Segoe UI", system-ui, sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 18px;
    gap: 12px;
    user-select: none;
  }
  .toolbar {
    width: 1280px;
    max-width: 100%;
    display: flex;
    justify-content: space-between;
    align-items: center;
    background: #11151b;
    border: 1px solid #262e3a;
    border-radius: 6px;
    padding: 8px 14px;
    font-size: 13px;
  }
  .toolbar .title { font-weight: 700; letter-spacing: 0.5px; color: #00e5ff; }
  .toolbar .hint { color: #8b949e; }
  .toolbar button {
    background: #161b22;
    color: #f0f4f8;
    border: 1px solid #00e5ff;
    border-radius: 4px;
    padding: 5px 12px;
    cursor: pointer;
    font-weight: 600;
  }
  .toolbar button:hover { background: #00e5ff; color: #0d0f12; }
  .mpc-frame {
    background: #14171c;
    border: 2px solid #262e3a;
    border-radius: 8px;
    padding: 10px;
    box-shadow: 0 16px 48px rgba(0, 0, 0, 0.7);
  }
  canvas {
    display: block;
    width: 1280px;
    height: 628px;
    max-width: calc(100vw - 48px);
    height: auto;
    cursor: pointer;
    border-radius: 4px;
  }
  .tab-bar {
    display: grid;
    grid-template-columns: repeat(7, 1fr);
    gap: 6px;
    margin-top: 8px;
    width: 1280px;
    max-width: calc(100vw - 48px);
  }
  .tab-btn {
    background: #161b22;
    color: #8b949e;
    border: 1px solid #262e3a;
    border-radius: 4px;
    padding: 9px 6px;
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 0.6px;
    cursor: pointer;
    text-align: center;
  }
  .tab-btn.active {
    background: #00e5ff;
    color: #0d0f12;
    border-color: #00e5ff;
  }
  .status-bar {
    width: 1280px;
    max-width: 100%;
    display: flex;
    justify-content: space-between;
    color: #8b949e;
    font-size: 12px;
    font-family: ui-monospace, Consolas, monospace;
  }
</style>
</head>
<body>
  <div class="toolbar">
    <div>
      <span class="title">MUSHIN — MPC UI PREVIEW (1280×628)</span>
      <span class="hint"> · Drag knobs/faders · Click switches · Scroll wheel to nudge · Click row to focus Q-Link bank</span>
    </div>
    <div>
      <button id="btnReset">Reset Defaults</button>
      <button id="btnSavePng">Save PNG</button>
    </div>
  </div>

  <div class="mpc-frame">
    <canvas id="screen" width="1280" height="628"></canvas>
    <div class="tab-bar" id="tabBar"></div>
  </div>

  <div class="status-bar">
    <span id="statusLeft">Selected: none</span>
    <span id="statusRight"></span>
  </div>

<script>
const TUI = __TUI_JSON__;
const QLINKS = __QLINKS_JSON__;
const PARAMS_INFO = __PARAMS_JSON__;
const IMAGES_B64 = __IMAGES_JSON__;

const canvas = document.getElementById("screen");
const ctx = canvas.getContext("2d");
const tabBar = document.getElementById("tabBar");
const statusLeft = document.getElementById("statusLeft");
const statusRight = document.getElementById("statusRight");

// Load images
const imgs = {};
let loadedCount = 0;
const imgKeys = Object.keys(IMAGES_B64);

// Parameter state (normalized 0..1)
const paramValues = PARAMS_INFO.params.map(p => p.default);
let activeTabIdx = 0;
let selectedParamIdx = -1;
let activeQlinkGroup = 0;

// Parse "x y w h"
function parseBounds(b) {
  const parts = b.split(" ").map(Number);
  return { x: parts[0], y: parts[1], w: parts[2], h: parts[3] };
}

function argbToCss(argb) {
  if (!argb || argb.length !== 8) return "#ffffff";
  const a = parseInt(argb.slice(0, 2), 16) / 255;
  const r = parseInt(argb.slice(2, 4), 16);
  const g = parseInt(argb.slice(4, 6), 16);
  const b = parseInt(argb.slice(6, 8), 16);
  return `rgba(${r},${g},${b},${a.toFixed(3)})`;
}

// Build local component definition lookup
const localDefs = {};
for (const def of TUI.pageData.componentDefinitions.localComponentDefinitions) {
  localDefs[def.key] = def.value;
}

const tabs = TUI.pageData.tabs;
tabBar.style.gridTemplateColumns = `repeat(${tabs.length}, 1fr)`;

// Build tab buttons
tabs.forEach((tab, idx) => {
  const btn = document.createElement("button");
  btn.className = "tab-btn" + (idx === 0 ? " active" : "");
  btn.textContent = tab.tabName;
  btn.onclick = () => {
    activeTabIdx = idx;
    selectedParamIdx = -1;
    activeQlinkGroup = 0;
    document.querySelectorAll(".tab-btn").forEach((b, i) => b.classList.toggle("active", i === idx));
    render();
  };
  tabBar.appendChild(btn);
});

function formatParamValue(pIdx) {
  const p = PARAMS_INFO.params[pIdx];
  const v = paramValues[pIdx];
  if (p.choices && p.choices.length > 0) {
    const ci = Math.min(p.choices.length - 1, Math.max(0, Math.round(v * (p.choices.length - 1))));
    return p.choices[ci];
  }
  return Math.round(v * 100) + "%";
}

function getParamChoiceIndex(pIdx) {
  const p = PARAMS_INFO.params[pIdx];
  if (!p.choices || p.choices.length === 0) return 0;
  return Math.min(p.choices.length - 1, Math.max(0, Math.round(paramValues[pIdx] * (p.choices.length - 1))));
}

function checkVisible(comp) {
  // Handle TUI.json visibility rules if present
  const vis = comp.bounds && comp.bounds.whenVisible;
  if (!vis || vis === "Always") return true;
  if (typeof vis === "object" && vis.handleName) {
    const m = String(vis.handleName).match(/Parameter (\d+)/);
    if (m) {
      const pIdx = parseInt(m[1], 10);
      const val = getParamChoiceIndex(pIdx);
      return val === vis.value;
    }
  }
  return true;
}

function drawFilmStrip(imgName, numFrames, normVal, dx, dy, dw, dh) {
  const img = imgs[imgName];
  if (!img) return;
  const frameH = img.height / numFrames;
  const frameIdx = Math.min(numFrames - 1, Math.max(0, Math.round(normVal * (numFrames - 1))));
  ctx.drawImage(img, 0, frameIdx * frameH, img.width, frameH, dx, dy, dw, dh);
}

function renderComponent(comp, offsetX = 0, offsetY = 0, paramOverride = null) {
  if (!checkVisible(comp)) return;
  const b = parseBounds(comp.bounds.bounds);
  const x = b.x + offsetX;
  const y = b.y + offsetY;
  const w = b.w;
  const h = b.h;
  const cd = comp.componentData;
  const type = cd.type;

  // Remap handle
  let pIdx = paramOverride;
  if (comp["handle remapping"] && comp["handle remapping"].map) {
    for (const m of comp["handle remapping"].map) {
      if (m.key === "Data" && m.value.startsWith("Parameter ")) {
        pIdx = parseInt(m.value.replace("Parameter ", ""), 10);
      }
    }
  }

  if (type === "Label") {
    const d = cd.data;
    const ts = d.textStyle;
    ctx.save();
    ctx.fillStyle = argbToCss(ts.colour);
    const fh = ts.font.height || 18;
    const weight = (ts.font.style || "").toLowerCase().includes("bold") ? "700" : "600";
    ctx.font = `${weight} ${Math.round(fh * 0.78)}px "Titillium Web", "Segoe UI", sans-serif`;
    ctx.textBaseline = "middle";
    let text = d.text || "";
    if ((d.type === "Name" || d.type === "ParameterName") && pIdx !== null) text = PARAMS_INFO.params[pIdx].name;
    else if ((d.type === "Value" || d.type === "ParameterValue") && pIdx !== null) text = formatParamValue(pIdx);

    if (ts.justification === "centre" || (ts.justification && ts.justification.includes("horizontallyCentred"))) {
      ctx.textAlign = "center";
      ctx.fillText(text, x + w / 2, y + h / 2, w - 4);
    } else if (ts.justification === "right") {
      ctx.textAlign = "right";
      ctx.fillText(text, x + w - 4, y + h / 2, w - 4);
    } else {
      ctx.textAlign = "left";
      ctx.fillText(text, x + 4, y + h / 2, w - 4);
    }
    ctx.restore();
    return;
  }

  if (type === "Rectangle") {
    const d = cd.data;
    ctx.save();
    if (d.fillColour && !d.fillColour.startsWith("00")) {
      ctx.fillStyle = argbToCss(d.fillColour);
      ctx.fillRect(x, y, w, h);
    }
    if (d.strokeColour && d.strokeThickness > 0) {
      ctx.strokeStyle = argbToCss(d.strokeColour);
      ctx.lineWidth = d.strokeThickness;
      ctx.strokeRect(x, y, w, h);
    }
    ctx.restore();
    return;
  }

  if (type === "Knob" || type === "FilmStrip") {
    const d = cd.data;
    const normVal = pIdx !== null ? paramValues[pIdx] : 0;
    const imgName = d.filmStrip || d.image;
    drawFilmStrip(imgName, d.numFrames || 127, normVal, x, y, w, h);
    return;
  }

  if (type === "Button") {
    const d = cd.data;
    const nGrp = d.numButtonsInGroup || 1;
    let isOn = false;
    if (pIdx !== null) {
      if (nGrp === 1) {
        isOn = paramValues[pIdx] >= 0.5;
      } else {
        const bid = (d.buttonId !== undefined) ? d.buttonId : 0;
        isOn = getParamChoiceIndex(pIdx) === bid;
      }
    }
    const imgName = isOn ? d.onImage : d.offImage;
    if (imgs[imgName]) ctx.drawImage(imgs[imgName], x, y, w, h);
    return;
  }

  if (type === "Toggle") {
    const d = cd.data;
    const isOn = pIdx !== null ? paramValues[pIdx] >= 0.5 : false;
    const imgName = isOn ? d.onImage : d.offImage;
    if (imgs[imgName]) ctx.drawImage(imgs[imgName], x, y, w, h);
    return;
  }

  if (type === "Envelope") {
    const d = cd.data;
    const findIdx = name => PARAMS_INFO.params.findIndex(p => p.name === name);
    const iStart = findIdx("Gate Start");
    const iHold  = findIdx("Gate Hold");
    const iEnd   = findIdx("Gate End");
    const iDepth = findIdx("Gate Depth");

    const startNorm = iStart >= 0 ? paramValues[iStart] : 0.05;
    const holdNorm  = iHold  >= 0 ? paramValues[iHold]  : 0.44;
    const endNorm   = iEnd   >= 0 ? paramValues[iEnd]   : 0.05;
    const depthNorm = iDepth >= 0 ? paramValues[iDepth] : 1.0;

    ctx.save();
    ctx.fillStyle = argbToCss(d.backgroundColour || "ff10141a");
    ctx.strokeStyle = argbToCss(d.scaleColour || "ff262e3a");
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, 6);
    ctx.fill();
    ctx.stroke();

    const padX = 18, padY = 14;
    const gx = x + padX, gy = y + padY, gw = w - padX * 2, gh = h - padY * 2;

    // Subtle grid lines
    ctx.strokeStyle = "rgba(38, 46, 58, 0.7)";
    for (let i = 1; i <= 3; i++) {
      const ly = gy + (gh * i) / 4;
      ctx.beginPath(); ctx.moveTo(gx, ly); ctx.lineTo(gx + gw, ly); ctx.stroke();
    }
    for (let i = 1; i <= 3; i++) {
      const lx = gx + (gw * i) / 4;
      ctx.beginPath(); ctx.moveTo(lx, gy); ctx.lineTo(lx, gy + gh); ctx.stroke();
    }

    // Gate trapezoid geometry matching TranceGateProcessor
    const holdW = Math.max(0.1, 0.1 + 0.9 * holdNorm) * gw;
    const maxTrans = gw * 0.35;
    let atkW = Math.min(startNorm * maxTrans, holdW * 0.5);
    let decW = Math.min(endNorm * maxTrans, holdW * 0.5);
    if (atkW + decW > holdW) {
      atkW = holdW * 0.5;
      decW = holdW * 0.5;
    }
    const yTop = gy + 4;
    const yFloor = gy + gh * (0.1 + 0.85 * depthNorm);

    ctx.beginPath();
    ctx.moveTo(gx, yFloor);
    ctx.lineTo(gx + atkW, yTop);
    ctx.lineTo(gx + holdW - decW, yTop);
    ctx.lineTo(gx + holdW, yFloor);
    ctx.lineTo(gx + gw, yFloor);

    // Fill under curve
    ctx.save();
    ctx.lineTo(gx + gw, gy + gh);
    ctx.lineTo(gx, gy + gh);
    ctx.closePath();
    ctx.fillStyle = argbToCss(d.fillColour || "3300e5ff");
    ctx.fill();
    ctx.restore();

    // Stroke curve
    ctx.strokeStyle = argbToCss(d.lineColour || "ff00e5ff");
    ctx.lineWidth = 2.5;
    ctx.stroke();

    // Stage handles
    const pts = [
      [gx + atkW, yTop],
      [gx + holdW - decW, yTop],
      [gx + holdW, yFloor]
    ];
    for (const [px, py] of pts) {
      ctx.beginPath();
      ctx.arc(px, py, 5, 0, Math.PI * 2);
      ctx.fillStyle = "#10141a";
      ctx.fill();
      ctx.lineWidth = 2;
      ctx.strokeStyle = "#ffffff";
      ctx.stroke();
    }
    ctx.restore();
    return;
  }

  if (type === "Image") {
    const d = cd.data;
    if (d.image && imgs[d.image]) ctx.drawImage(imgs[d.image], x, y, w, h);
    return;
  }

  // Local component instance (knobUni, knobBi, faderUni, toggle, seg2, seg3, seg4)
  if (localDefs[type]) {
    const def = localDefs[type];

    // Cell card background
    ctx.save();
    ctx.fillStyle = "rgba(22, 27, 34, 0.72)";
    ctx.strokeStyle = (pIdx !== null && pIdx === selectedParamIdx) ? "#ffd60a" : "#262e3a";
    ctx.lineWidth = (pIdx !== null && pIdx === selectedParamIdx) ? 2 : 1;
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, 6);
    ctx.fill();
    ctx.stroke();
    ctx.restore();

    if (type === "seg2" || type === "seg3" || type === "seg4") {
      const n = parseInt(type.slice(3), 10);
      const p = pIdx !== null ? PARAMS_INFO.params[pIdx] : null;
      const choiceIdx = pIdx !== null ? getParamChoiceIndex(pIdx) : 0;
      const segW = n === 2 ? 122 : (n === 3 ? 80 : 58);
      const xs = n === 2 ? [15, 141] : (n === 3 ? [15, 99, 183] : [15, 77, 139, 201]);

      // Parameter name at top
      ctx.save();
      ctx.fillStyle = "#8b949e";
      ctx.font = '600 13px "Titillium Web", "Segoe UI", sans-serif';
      ctx.textAlign = "left";
      ctx.textBaseline = "middle";
      ctx.fillText(p ? p.name : cd.name, x + 15, y + 18, w - 30);
      ctx.restore();

      for (let i = 0; i < n; i++) {
        const active = (i === choiceIdx);
        const imgName = `seg${n}_${active ? "on" : "off"}.png`;
        if (imgs[imgName]) ctx.drawImage(imgs[imgName], x + xs[i], y + 40, segW, 48);
        ctx.save();
        ctx.fillStyle = active ? "#0d0f12" : "#f0f4f8";
        ctx.font = '700 13px "Titillium Web", "Segoe UI", sans-serif';
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        const label = (p && p.choices && p.choices[i]) ? p.choices[i] : String(i);
        ctx.fillText(label, x + xs[i] + segW / 2, y + 40 + 24, segW - 6);
        ctx.restore();
      }
      return;
    }

    for (const sub of def.componentsData) {
      if (sub.componentData.type === "Focus") continue;
      renderComponent(sub, x, y, pIdx);
    }
  }
}

function getInteractiveRects() {
  const tab = tabs[activeTabIdx];
  const rects = [];
  for (const comp of tab.componentDefinition.componentsData) {
    if (!checkVisible(comp)) continue;
    let pIdx = null;
    if (comp["handle remapping"] && comp["handle remapping"].map) {
      for (const m of comp["handle remapping"].map) {
        if (m.key === "Data" && m.value.startsWith("Parameter ")) {
          pIdx = parseInt(m.value.replace("Parameter ", ""), 10);
        }
      }
    }
    if (pIdx !== null) {
      rects.push({ comp, pIdx, bounds: parseBounds(comp.bounds.bounds) });
    }
  }
  return rects;
}

function render() {
  const tab = tabs[activeTabIdx];
  const bg = argbToCss(tab.componentDefinition.backgroundData.unfocussed.colour);
  ctx.fillStyle = bg;
  ctx.fillRect(0, 0, 1280, 628);

  const bgImg = tab.componentDefinition.backgroundData.unfocussed.image;
  if (bgImg && imgs[bgImg]) {
    ctx.drawImage(imgs[bgImg], 0, 0, 1280, 628);
  }

  // Render components
  for (const comp of tab.componentDefinition.componentsData) {
    renderComponent(comp, 0, 0, null);
  }

  // Draw Q-Link row focus rectangle for activeQlinkGroup (0..3)
  const rowYs = [45, 196, 347, 498];
  const gy = rowYs[activeQlinkGroup] || 45;
  ctx.save();
  ctx.strokeStyle = "rgba(0, 229, 255, 0.38)";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.roundRect(13, gy - 8, 1254, 123, 8);
  ctx.stroke();
  ctx.restore();

  if (selectedParamIdx >= 0) {
    const p = PARAMS_INFO.params[selectedParamIdx];
    statusLeft.textContent = `Selected: ${p.name} (Param ${p.index}) = ${formatParamValue(p.index)}`;
  } else {
    statusLeft.textContent = `Tab: ${tab.tabName} · Q-Link Row ${activeQlinkGroup + 1}`;
  }
  statusRight.textContent = `${tabs.length} tabs · ${PARAMS_INFO.params.length} parameters`;
}

// Mouse interaction
let dragState = null;

function canvasCoords(evt) {
  const r = canvas.getBoundingClientRect();
  return {
    x: (evt.clientX - r.left) * (1280 / r.width),
    y: (evt.clientY - r.top) * (628 / r.height)
  };
}

canvas.addEventListener("mousedown", evt => {
  const pt = canvasCoords(evt);
  const rects = getInteractiveRects();
  for (const item of rects) {
    const b = item.bounds;
    if (pt.x >= b.x && pt.x <= b.x + b.w && pt.y >= b.y && pt.y <= b.y + b.h) {
      selectedParamIdx = item.pIdx;
      const rowIdx = Math.min(3, Math.max(0, Math.floor((b.y - 30) / 151)));
      activeQlinkGroup = rowIdx;

      const type = item.comp.componentData.type;
      const p = PARAMS_INFO.params[item.pIdx];
      if (type === "toggle") {
        paramValues[item.pIdx] = paramValues[item.pIdx] >= 0.5 ? 0.0 : 1.0;
        render();
        return;
      }
      if (type === "radio2" || type === "radio3" || type === "radio4" || type === "seg2" || type === "seg3" || type === "seg4") {
        const n = parseInt(type.slice(3), 10);
        const segW = n === 2 ? 122 : (n === 3 ? 80 : 58);
        const xs = n === 2 ? [15, 141] : (n === 3 ? [15, 99, 183] : [15, 77, 139, 201]);
        let clicked = -1;
        for (let i = 0; i < n; i++) {
          if (pt.x >= b.x + xs[i] && pt.x <= b.x + xs[i] + segW) clicked = i;
        }
        if (clicked >= 0 && n > 1) {
          paramValues[item.pIdx] = clicked / (n - 1);
        }
        render();
        return;
      }
      dragState = { pIdx: item.pIdx, startY: pt.y, startX: pt.x, startVal: paramValues[item.pIdx], isFader: type.startsWith("fader") };
      render();
      return;
    }
  }
});

window.addEventListener("mousemove", evt => {
  if (!dragState) return;
  const pt = canvasCoords(evt);
  const delta = dragState.isFader ? (pt.x - dragState.startX) / 180 : (dragState.startY - pt.y) / 150;
  paramValues[dragState.pIdx] = Math.min(1, Math.max(0, dragState.startVal + delta));
  render();
});

window.addEventListener("mouseup", () => { dragState = null; });

canvas.addEventListener("wheel", evt => {
  if (selectedParamIdx < 0) return;
  evt.preventDefault();
  const step = evt.shiftKey ? 0.005 : 0.02;
  const dir = evt.deltaY < 0 ? 1 : -1;
  paramValues[selectedParamIdx] = Math.min(1, Math.max(0, paramValues[selectedParamIdx] + dir * step));
  render();
}, { passive: false });

document.getElementById("btnReset").onclick = () => {
  PARAMS_INFO.params.forEach((p, i) => { paramValues[i] = p.default; });
  render();
};

document.getElementById("btnSavePng").onclick = () => {
  const a = document.createElement("a");
  a.download = `mushin-${tabs[activeTabIdx].tabName.toLowerCase()}.png`;
  a.href = canvas.toDataURL("image/png");
  a.click();
};

if (imgKeys.length === 0) {
  render();
} else {
  for (const k of imgKeys) {
    const im = new Image();
    im.onload = () => {
      loadedCount++;
      if (loadedCount === imgKeys.length) render();
    };
    im.src = IMAGES_B64[k];
    imgs[k] = im;
  }
}
</script>
</body>
</html>
"""


def main():
    ap = argparse.ArgumentParser(description="Generate and open an interactive HTML5 preview of Mushin's MPC UI.")
    ap.add_argument("--out", default=DEFAULT_OUT, help="Output HTML file path")
    ap.add_argument("--no-open", action="store_true", help="Do not open the browser automatically")
    args = ap.parse_args()

    for required in (TUI_PATH, QLINKS_PATH, PARAMS_PATH):
        if not os.path.isfile(required):
            print(f"ERROR: Missing required build artifact: {required}", file=sys.stderr)
            print("Run: python tools/build.py plugins/Mushin", file=sys.stderr)
            return 1

    tui = load_json(TUI_PATH)
    qlinks = load_json(QLINKS_PATH)
    params = load_json(PARAMS_PATH)
    images = encode_pngs(SKIN_DIR)

    html = (
        HTML_TEMPLATE
        .replace("__TUI_JSON__", json.dumps(tui))
        .replace("__QLINKS_JSON__", json.dumps(qlinks))
        .replace("__PARAMS_JSON__", json.dumps(params))
        .replace("__IMAGES_JSON__", json.dumps(images))
    )

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(html)

    print(f"Wrote interactive UI preview: {args.out}")
    if not args.no_open:
        webbrowser.open("file://" + os.path.abspath(args.out).replace("\\", "/"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
