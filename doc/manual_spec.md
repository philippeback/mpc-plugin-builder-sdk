# CANONICAL MANUAL SPECIFICATION (`doc/manual_spec.md`)

**Scope:** Normalized structural, typographic, taxonomic, and parameter-documentation specification extracted across three aligned documentation lineages:
1. **Digidesign / Avid (Wizoo-AIR Era):** *Hybrid 2*, *AIR Virtual Instruments Plug-Ins Guide*
2. **AIR Music Technology (Desktop Era):** *Hybrid 3 User Guide (Manual Version 1.0)*
3. **inMusic Brands / Akai Professional (MPC Plugin Era):** *AIR Jura*, *AIR Mini D*, *AIR Flavor Pro*, *MPC Software & Plugin User Guides*

**Normative Keyword Convention:** `MUST`, `SHOULD`, `MAY`, `INVARIANT` follow strict specification semantics. Elements marked `[SOURCE-OBSERVED]` reflect directly verified manual patterns; elements marked `[SDK-PROFILE]` define deterministic bindings for generating manuals from plugins built in this SDK.

---

## 0. Lineage Invariant Matrix

| Axis | L1: Digidesign / Avid AIR (`Hybrid 2`) | L2: AIR Desktop (`Hybrid 3 v1.0`) | L3: Akai / AIR MPC (`Jura`, `Flavor Pro`) | Unified Invariant Rule |
|---|---|---|---|---|
| **Title Header** | Product Name + Guide Type + Platform | `Product Name - User Guide`, `Manual Version X.Y` | `Product Name - User Guide`, `Manual Version X.Y`, Language | Product + `"User Guide"` + Version stamp |
| **TOC Placement** | Front-matter hierarchical chapters | Flat/nested Contents list pre-Introduction | Hierarchical Contents list pre-Introduction | Mandatory TOC mirroring exact heading tree |
| **Auth / Activation** | iLok authorization, Pro Tools RTAS reqs | Serial / trial / authorization flow | `inMusic Software Center` / **inMusic Profile**, Standalone media paths | Dedicated setup + activation + storage chapter |
| **Formatting Legend** | **Conventions Used in This Guide** | Same inline & callout grammar | **Conventions** (**Bold**, *Italic*, `>`, callouts) | Self-documented inline/callout legend |
| **UI Breakdown** | Graphic callouts -> Global -> Part A/B -> Common | Graphic -> Global -> Morph -> Part Pages -> Sequencer -> Common -> FX | Interface overview -> Top Global bar -> Module sections (Vintage/Modern) | Visual overview -> Global header -> Signal-ordered modules |
| **Parameter Spec** | `Parameter \| Description \| Range` | Parameter list grouped per UI panel/tab | Control name + Range/Choices + sonic effect | Exhaustive per-control spec ordered top-to-bottom / signal-flow |
| **Hardware Mapping** | MIDI CC / Pro Tools controller focus | MIDI CC mapping & Morph Group | **Touchscreen** tabs + **Q-Link** knob pages (`Standalone` vs `DAW`) | Explicit physical controller & tab-to-knob binding |
| **Close Matter** | Appendices + MIDI CC + Legal/Avid notice | Appendix + Support / License notices | Appendix + Troubleshooting + **Trademarks and Licenses** | Mandatory Appendix + Troubleshooting + Trademarks/Licenses |

---

## 1. Macro Document Grammar (`EBNF`)

```ebnf
Manual              ::= TitleBlock
                        TableOfContents
                        ChapterIntroduction
                        ChapterSetup
                        [ ChapterConventions ]
                        ChapterInterfaceGlobal
                        { ModuleSection }+
                        [ SignalFlowSection ]
                        [ HowToWorkflowSection ]
                        ChapterAppendix
                        LegalClose ;

TitleBlock          ::= ProductTitle DocumentType [ EditionLine ] ManualVersion BrandStamp ;
ProductTitle        ::= STRING ;                  (* e.g. "Hybrid 3", "Flavor Pro", "Mushin" *)
DocumentType        ::= "User Guide" ;
ManualVersion       ::= "Manual Version " DIGIT "." DIGIT ;

TableOfContents     ::= Heading("Table of Contents") TocEntry+ ;

ChapterIntroduction ::= Heading("Introduction")
                        ( ThanksLead | OverviewBlock )
                        [ FeatureList ]
                        SystemRequirements
                        ProductSupport ;

ChapterSetup        ::= Heading("Installation") | Heading("Setup")
                        DesktopInstallSteps
                        ActivationSteps
                        [ StandaloneTransferSteps ] ;

ChapterConventions  ::= Heading("Conventions Used in This Guide")
                        BoldRule ItalicRule AngleRule CalloutLegend ;

ChapterInterfaceGlobal ::= Heading("Operation") | Heading("User Interface")
                           GlobalHeaderBar
                           ViewNavigationModel
                           [ LayoutToggle ] ;     (* e.g. Part A/B/Common, Vintage/Modern, Tab pages *)

ModuleSection       ::= ModuleHeading [ ModuleOverview ] [ TabScreenshotRef ] ControlTableOrList ;
ControlTableOrList  ::= ParamRow+ ;

HowToWorkflowSection::= Heading("\"How-To\" Instructions") | Heading("Workflows") WorkflowProcedure+ ;

ChapterAppendix     ::= Heading("Appendix")
                        [ QuickParamOrMidiTable ]
                        TroubleshootingSection ;

LegalClose          ::= Heading("Trademarks and Licenses") CopyrightNotice TrademarkClauses ;
```

---

## 2. Typographical & Inline Token Specification `[SOURCE-OBSERVED]`

All inline formatting tokens MUST obey strict semantic separation:

| Token | Rendered Style | Semantic Domain | Canonical Examples |
|---|---|---|---|
| **Bold Control** | `**Text**` | Physical hardware buttons/knobs, on-screen buttons, switches, menus, dialog window titles, tab labels, preset browser controls | **Q-Link Knob 4**, **Dry/Wet Mix**, **Save**, **Touchscreen**, **Part A** |
| *Italic Reference* | `*Text*` | Cross-references to other chapters/manuals, file paths, folder names, URLs, newly introduced domain terms, light emphasis | `*MIDI Implementation*`, *`VST2 Plugins`*, *`Synths/`*, *Exhaustion* |
| Angle Chain | `**Menu > Sub > Item**` | Multi-step menu or preference navigation paths using `>` separators | **File > Export > MIDI**, **Preferences > Enhancements (Beta)** |
| Keycap / Action | `[Verb]` + **Target** | Standard imperative UI verb vocabulary | *Tap* (touchscreen), *Turn* (hardware encoder), *Click* (desktop mouse), *Double-tap* (reset/overlay) |
| Symbol `△` | Margin / inline notice marker | Legacy Digidesign/AIR disclosure or important margin callout icon | Precedes critical data-loss / routing notices `[SOURCE-OBSERVED: L1]` |
| Value Unit | Exact signed/unit literal | Numeric ranges, polarity, choices, dB/Hz/ms/ratio units | `–60 dB to +18 dB`, `Off, On`, `20 Hz – 20 kHz`, `–100% to +100%` |

---

## 3. Admonition & Callout Grammar `[SOURCE-OBSERVED]`

Callouts MUST appear as visually offset blocks prefixed with an exact capitalized header token:

```yaml
Callouts:
  Important:
    token: "Important:"
    severity: critical
    usage:
      - Actions risking project/session data loss (e.g., host service restart, state overwrite)
      - System requirements, audio-routing prerequisites, or destructiveDSP limits
      - Standalone enablement flags (OS >= 3.10.1, External Plugin Support)
  Note:
    token: "Note:"
    severity: informational
    usage:
      - Contextual exceptions (e.g., Parameter 0 host behavior, FX receiving no MIDI)
      - Behavioral differences between Standalone MPC and Desktop VST/AU/AAX hosts
      - Supplementary technical clarification without interrupting procedure flow
  Tip:
    token: "Tip:"
    severity: advisory
    usage:
      - Sound-design recipes and creative macro pairings
      - Q-Link / hardware performance shortcuts
```

**Ordering Rule:** Within procedural numbered lists (`1.`, `2.`, `3.`), callouts MUST align beneath the step they modify rather than breaking list numbering.

---

## 4. Section Blueprint (`S0–S8`)

### `S0`: Front Cover / Header Block
```yaml
required_fields:
  product_name: string
  document_type: "User Guide"
  manual_version: "Manual Version X.Y"
  language: "English"          # optional header badge in multi-lingual releases
  brand_attribution: string    # e.g., "AIR Music Technology" / "Akai Professional" / vendor
```

### `S1`: Table of Contents
- Top-level chapters + module H2 children MUST match exact document anchors.
- Follows logical learning sequence: **Introduction -> Setup -> Operation/Global -> Modules -> Reference/Appendix -> Legal**.

### `S2`: Introduction `[SOURCE-OBSERVED]`
1. **Opening ("Thank you for purchasing..."):** 1 short welcome paragraph naming the product, category (*virtual instrument* vs *multi-stage effect processor*), emulation/synthesis lineage, and host scope (`MPC Standalone`, `MPC Software`, `VST/AU/AAX`).
2. **Overview & Key Features:** Bullet inventory summarizing synthesis/DSP engine count, polyphony/architecture highlights, preset count, modulation/sequencer engines, and integrated FX.
3. **System Requirements & Product Support:**
   - Host/OS matrix (`Windows`, `macOS`, `MPC Standalone OS >= X.Y.Z`, `Plugin Formats`).
   - Support portal URLs + registration prompt.

### `S3`: Installation & Authorization `[SOURCE-OBSERVED]`
1. **Desktop Installation:** Installer artifact execution (`.exe` / `.pkg`), plugin format scan paths (`VST2`/`VST3`/`AU`/`AAX`).
2. **Activation / Licensing:**
   - `[L2/L3]` Signing into **inMusic Profile** via **inMusic Software Center** or plugin UI (`Serial key` or `Trial/Unlicensed` mode).
   - `[SDK-PROFILE]` Open/standalone custom plugin equivalent: folder copy verification & license attribution note (if third-party DSP reused).
3. **MPC Standalone Transfer & Enablement:**
   - Moving package (`<Manufacturer> - VST - <Name>/`) to `VST2 Plugins` (or `Synths/`) on internal/removable drive.
   - Standalone enablement steps (**Preferences > Enhancements (Beta) > Enable Enhancements + External Plugin Support** for SDK External VST2 builds).

### `S4`: Conventions Used in This Guide `[SOURCE-OBSERVED]`
- Concise 4-item legend (Bold UI/HW, Italic refs/paths, `>` menu steps, Callout severity levels).

### `S5`: User Interface & Global Controls `[SOURCE-OBSERVED]`
Must document persistent workspace chrome before diving into modules:
1. **Top Header / Global Bar:**
   - Plugin Menu / Manual link / Settings cog
   - Preset Browser (`<` `>` step arrows, preset dropdown / bank filter, save/overwrite)
   - Tempo / Host Sync / Tap Tempo
   - Global Output Gain / Mix / Bypass / I-O Clipping & Gain-Reduction Meters
2. **Navigation Topology:**
   - Multi-tab layout model (`MAIN` macros + function tabs `F1..F6` & SubTabs on MPC **Touchscreen**).
   - **Q-Link Control Mapping Contract:** Explicit 16-knob grid mapping (`Row 1..4` x `Col 1..4`) per tab/subtab so users know hardware bank correspondence without hunting.
   - Multi-engine or dual-view parity if applicable (`Part A / Part B / Common` in *Hybrid*, `Vintage / Modern` in *Jura*, `Bank 1 / Bank 2` in macro performance pages).

### `S6`: Module & Architecture Chapters `[SOURCE-OBSERVED]`
Section order MUST follow **signal flow** (or `Global -> Source -> Filter -> Mod -> Time/Rhythm -> FX`), strictly typed by plugin kind:

#### Profile A: Instrument Architecture (`Hybrid 2/3`, `Jura`, `Mini D`)
1. `Signal Flow & Voice Architecture` (Voice mode, Polyphony, Unison/Detune, Glide, Pitch Bend)
2. `Part / Sound Generation (Oscillators, Sub, Noise, Wavetable, Cross-Mod / Sync)`
3. `Filter Section` (Dual filters, Series/Parallel/Stereo routing, Drive/Saturation, Keytrack)
4. `Envelopes` (Amp, Filter, Mod ADSR/ADR + Curves/Trigger modes)
5. `LFOs` (Rate/Sync, Waveforms including S&H/Pump, Phase/Retrig)
6. `Arpeggiator / Step Sequencer` (Steps, Playback Modes, Latch, Swing, Gate)
7. `Modulation Matrix & Morph Groups` (Multi-target macros + CC assignments)
8. `Insert & Master Effects` (Chorus, Delay, Reverb, Distortion/Decimator, EQ)
9. `Presets & Bank Browser`

#### Profile B: Effect Architecture (`Flavor Pro`, `Mushin`)
1. `Signal Flow & Gain Staging` (Auto-Level / Input Trim, Routing Topologies `Serial / Parallel / Stereo / M/S`, Pre/Post stage swaps)
2. `Core Processing Modules` (Each DSP block documented in exact audio-path order, e.g., Waveshaper -> Dual Filter -> Noise -> Info Decay)
3. `Time, Rhythm & Dynamics Modules` (Trance Gate / Step Sequencer, Sidechain / Pump Compressor + Key Filter, Stereo Delay, Master Limiter)
4. `Modulation Sources & Performance Macros` (LFOs, Envelope Follower, Mod Matrix, `MAIN` Macro Orbs, Telemetry/Visualizer tabs)
5. `Factory Presets / Program Index` (Categorized by bank prefix with intent summary)

### `S7`: Operational Procedures ("How-To" Workflows)
- Short, numbered task recipes (`1.` `2.` `3.`) covering non-obvious workflows:
  - Mapping external **MIDI CC** or **MPC Q-Link** macros
  - Configuring **Sidechain Source** (`INT` audio vs `4x4` internal pulse) or **Morph Groups**
  - Managing **Presets / Programs** across DAW vs MPC Standalone

### `S8`: Appendix, Troubleshooting & Legal Close `[SOURCE-OBSERVED]`
1. **Appendix A: Complete Parameter Index / Q-Link Map** (Tab/SubTab -> Q-Link 1..16 -> Parameter ID/Name -> Automatable flag).
2. **Appendix B: Built-in Preset Directory** (Indexed catalog grouped by prefix).
3. **Appendix C: Troubleshooting** (`Symptom -> Root Cause -> Resolution` table: plugin invisible, silent track, UI tabs missing, parameter-0 host quirk, CPU spikes).
4. **Trademarks and Licenses:** Brand attributions (*Akai Professional*, *MPC*, *AIR Music Technology*, *inMusic Brands, Inc.*), third-party DSP open-source license notes (if any), legal disclaimers.

---

## 5. Canonical Parameter Documentation Schema `[SOURCE-OBSERVED]`

Every exposed control MUST map into the 3-column Digidesign/AIR/Akai reference structure:

```markdown
| Parameter | Description | Range / Values |
|---|---|---|
| **Control Name** | Concise 1-2 sentence explanation naming what sonic property changes, interaction with switches/routing, and modulation sensitivity. | `Min – Max Unit` (Default: `X`) |
```

### Normalization Rules for Parameter Entries
1. **Order Rule:** Order controls inside each module strictly top-to-bottom, left-to-right matching screen rows (or Q-Link 1..16 order where screen rows mirror Q-Links).
2. **Exact Name Parity:** Column 1 MUST match the exact on-screen label (**Display Name**) — NEVER an internal C++ enum (`kFACutoff`). Internal IDs belong only in Appendix developer tables if included.
3. **Conditional Visibility:** If a control is gated by a switch (`Delay Sync = On` shows **Delay Tempo**, `Off` shows **Delay Time**), Column 2 MUST explicitly state `(Active when **Delay Sync** is On/Off)`.
4. **Read-Only Meters / Visualizers (`automatable = false`):** Documented separately from knob tables or marked `[Read-Only Meter]` / `[Visualizer]` so users do not hunt for automation lanes on display-only telemetry.
5. **Unit Normalization:**
   - Frequency: `Hz` (`20 Hz – 20 kHz`)
   - Time / Release / Attack: `ms` or `s` (`0.1 ms – 2000 ms`)
   - Level / Threshold / Ceiling: `dB` (`–60.0 dB to 0.0 dB`, `Off` if muted at min)
   - Ratios: `X:1` (`1.0:1 – 20.0:1`)
   - Normalized / Bipolar Modulators: `0.00 – 1.00` or `–1.00 to +1.00` (`0% – 100%` if formatted in percent)
   - Discrete Switches / Choices: Comma-separated bold/code literals matching UI labels identically (e.g., `SERIAL, PARAL, STEREO, M/S` or `INT, 4x4`).

---

## 6. Lineage Comparison & SDK Mapping Contract `[SDK-PROFILE]`

When generating a User Guide for an SDK plugin (e.g., `plugins/Mushin/`), extract metadata deterministically from the project tree rather than prose guessing:

| Manual Slot | Source File / Field in SDK Project |
|---|---|
| `Product Name` / `Vendor` / `Version` | `plugin.json` (`name`, `manufacturer`, `version`) |
| `Kind (Instrument / Effect)` | `plugin.json` (`kind`) -> selects Profile A vs Profile B |
| `System Requirements` | `AGENTS.md` baseline (`MPC OS >= 3.10.1`, `Enhancements + External Plugin Support`) |
| `Install Path` | `build/<Manufacturer> - VST - <Name>/` -> `<Drive>/VST2 Plugins/` or `tools/mpc_remote.py` |
| `Tabs & SubTabs (S5/S6)` | `skins/TUI.json` (`tabsData`: `tabName`, `fnKeyIndex`, `fnKeySubIndex`) |
| `Q-Link Matrix (S5/S8)` | `skins/Q-Links.json` (`tabIndex`, `subTabIndex`, slot `1..16` -> parameter handle) |
| `Parameter Ranges & Defaults` | `build/params.json` (`id`, `name`, `min`, `max`, `default`, `unit`, `choices`, `automatable`) |
| `Factory Preset Index (S8)` | `build/params.json` (`programs[]`) grouped by `PREFIX:` |
| `DSP Signal Flow` | Top-of-file header comment + `process()` block sequence in `src/<Plugin>.cpp` |
| `Notices & Legal` | `plugins/<Name>/README.md` (limitations, CPU notes, borrowed DSP licenses) |

---

## 7. Compliance Verification Checklist

Before finalizing any User Guide generated under this spec, verify:
- [ ] Front matter contains Product Name, `"User Guide"`, and `Manual Version`.
- [ ] TOC matches every H1/H2 heading 1:1.
- [ ] MPC Standalone (`3.10.1+`, `Enhancements > External Plugin Support`) AND Desktop (if applicable) setup paths are documented.
- [ ] All controls use exact `**Bold**` UI names, `*Italic*` references/paths, `>` menu separators, and `Important:` / `Note:` / `Tip:` callouts.
- [ ] Every automatable parameter from `build/params.json` appears once in its module `Parameter | Description | Range / Values` table; read-only meters/visualizers are explicitly labeled read-only.
- [ ] Every Q-Link tab/subtab slot and conditional visibility rule is documented.
- [ ] Appendix includes Troubleshooting and `Trademarks and Licenses`.
