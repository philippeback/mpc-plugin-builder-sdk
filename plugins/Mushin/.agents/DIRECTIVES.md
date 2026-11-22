# MUSHIN — MPC Standalone UI & Control Directives (`agy` / Agent Enforcement)

## 1. 4-Column Q-Link Bank Grid Alignment (Mandatory)
- **Strict 4-Column Cadence:** Every parameter page (`S-CURVE`, `FILTER`, `LFO MOD`, `MOD 2`, `SIDECHN`) must render on a strict **4-column grid** (`4 columns × N rows`) so each row maps 1-to-1 to the 4 physical Q-Link encoders on MPC Live II (and 4×4 banks on MPC X).
- **No 3-Column Orphan Rows:** Rows containing fewer than 4 active parameters (such as `LFO2->A Cut / Res / Grit` or `Noise Level / Noise Route / Filter FM`) must occupy explicit slots `1, 2, 3` of a 4-column track with **Column 4 reserved as an explicit empty/unassigned slot** (`[ --- ]`). Never center or stretch 3 controls across 4 Q-Link slots, which breaks visual correspondence with the yellow Q-Link focus frame.
- **Deterministic Bank Paging:** Each horizontal row of 4 grid cells equals **1 Q-Link Bank**. Pressing Q-Link Bank Up/Down or touching a row moves the yellow Q-Link focus rectangle cleanly across full 4-column rows.

## 2. Explicit Iconography & Text Fallback Directives (No Faceless Buttons)
- **Zero Faceless Buttons:** Blank rounded pills or empty toggle rectangles (such as the unlabelled pink/slate mode buttons in `Noise Route`) are strictly forbidden. Every discrete mode switch, filter topology selector, LFO waveform button, and routing matrix cell must display either:
  1. **Embedded vector path (`juce::Path` / compiled `BinaryData` SVG)** at `20×20 px` to `24×24 px` with `2.0 px` stroke width, OR
  2. **Explicit uppercase text label fallback** (`LP`, `BP`, `HP`, `A`, `B`, `A+B`, `SINE`, `SAW`, `S&H`) at `13px+` bold sans-serif whenever no icon is supplied or drawable bounds are empty.
- **Automatic Text Fallback Rule:** If a button or segmented option has no icon asset or its vector drawable fails to load/parse, it **must immediately fall back to rendering its concise uppercase label**. Never render an empty colored pill.
- **Standalone Linux Vector Rendering (`armv7-linux-gnueabihf` / `aarch64`):**
  - Do **not** rely on external `.svg` file paths at runtime or WebView/HTML asset URLs that fail inside the standalone MPC VST2 sandbox.
  - Compile all icons directly into the C++ binary as **pre-parsed `juce::Drawable` / `juce::Path` geometry** (or embedded `BinaryData` SVG strings parsed at construction), backed by the mandatory text label fallback.
- **Active vs. Inactive State Contrast:**
  - Active segmented button: High-contrast accent fill (`#FF4D8D` / `#00E5FF`) with dark icon/text (`#0D0F12`).
  - Inactive segmented button: Dark slate fill (`#1E242B`) with muted light-gray icon/text (`#9AA5B1`) and `1px` border (`#323F4B`).

## 3. Touch-to-Select & Jog Wheel (Data Dial) Focus Contract
- **Touch Selection Without Drag-Fiddling:** Tapping any parameter cell (rotary gauge, slider, toggle, or segmented selector) immediately selects that control as the **Active Focus Parameter** without requiring precision finger dragging.
- **Jog Wheel / Data Dial & `-` / `+` Binding:**
  - Once a control is touch-selected, turning the hardware **Data Dial (Jog Wheel)** or pressing the physical **`-` / `+` buttons** must immediately increment/decrement that parameter's host `AudioProcessorParameter` value (`beginChangeGesture()` → `setValueNotifyingHost()` → `endChangeGesture()`).
  - **Step Resolution:**
    - Continuous parameters: Default step `1/100` (or `1/200` fine step when `SHIFT` is held).
    - Discrete/segmented parameters: `1` discrete index per detent/click of the Data Dial.
    - Toggle parameters: Any Data Dial step or `-`/`+` press toggles state cleanly.
- **Active Focus Visual Indicator:** The currently selected parameter cell must display a distinct high-contrast selection border (`2px solid #00E5FF` or `#FFD60A` focus ring) distinct from the 4-knob Q-Link bank highlight so the operator always knows at a glance which exact control the Jog Wheel owns.
- **Host / SDK Focus Synchronization:** Ensure touch selection updates the MPC SDK / VST2 active parameter focus hook (and synchronizes the active Q-Link bank row to the touched control) so the hardware Data Dial routes natively without fighting host focus.

## 4. Touch & Telemetry Polish on 7" Capacitive Glass
- **Minimum Control Hitboxes:** Parameter cells and segmented selector buttons must be at least `48×36 px` with `6 px` horizontal spacing to prevent adjacent thumb misfires.
- **Parameter Value Readouts:** Every Q-Link encoder cell must display:
  - Top/Side: Parameter name (`13px` medium, `#CBD2D9`).
  - Primary readout: Live formatted value (`16px` bold monospace, `#FFFFFF`) with unit (`Hz`, `dB`, `%`).
  - Visual arc or horizontal bar gauge synchronized to normalized `[0.0, 1.0]` parameter state.

## 5. Agent Verification Gate (`agy` Pre-Commit Check)
Before marking any UI task complete on `mushin`:
1. Verify all `BinaryData` / `juce::Path` icons are compiled into the target `armv7` / `aarch64` `.so` binary with zero runtime external file dependencies.
2. Confirm every button or segmented control renders either a non-empty vector icon **or** a non-empty uppercase text label—never blank.
3. Confirm every parameter grid row uses a 4-column layout template matching the physical Q-Link encoder bank order (`Q1` left → `Q4` right).
4. Confirm tapping any parameter cell visibly highlights it as selected and binds the MPC Data Dial (Jog Wheel) and `-`/`+` buttons to adjust its value smoothly.

## 6. Color Scheme & Theme Palette (Compile-Time Tokens Only)
- **Palette-Only Architecture:** Port Mushin's theming strictly as a flat compile-time C++ `ThemePalette` struct (no background images, textures, or external CSS/JSON theme files).
- **Required Semantic Tokens:**
  - `bgCanvas`: `#0D0F12` (main viewport fill, seamless with MPC OS chrome)
  - `bgCell`: `#161B22` (parameter cell / inactive button fill)
  - `borderSubtle`: `#262E3A` (1px Q-Link cell dividers)
  - `textPrimary`: `#F0F4F8` (live numeric values and active labels)
  - `textSecondary`: `#8B949E` (parameter names and inactive labels)
  - `accentPrimary`: `#00E5FF` (main parameter arc/bar fill and active selection)
  - `accentMod`: `#FF4D8D` (modulation routing, LFO/sidechain indicators, active mode pills)
  - `focusRing`: `#FFD60A` (touch-selected / Jog Wheel target border)
- **Theme Switching:** If multiple themes from Mushin are preserved, switch exclusively by swapping active `ThemePalette` struct instances and calling `repaint()`.

- **Compile-Time Theme Tokens:** Port Mushin's theme engine as a lightweight C++ `ThemePalette` struct (surface, card, border, primary accent, modulation accent, focus ring). Do not parse external CSS/JSON theme files from disk at runtime.
- **Zero Per-Frame Background Repaints:** Any background artwork, technical grid, or subtle vignette must either be:
  1. Embedded as a static `BinaryData` image blitted via `ImageCache`, OR
  2. Rendered once on `resized()` / theme switch into a cached offscreen buffer (`setBufferedToImage(true)` on a non-opaque/opaque static backdrop layer).
- **Functional Restraint (Anti-Clutter):** Backgrounds must remain low-frequency dark surfaces (`luminance < 12%`) with subtle 4-column Q-Link lane dividers. Never place high-contrast illustrations, busy patterns, or fake hardware screws behind parameter text and gauges.