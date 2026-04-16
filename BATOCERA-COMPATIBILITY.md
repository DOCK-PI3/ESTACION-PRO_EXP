# Batocera Theme Compatibility Guide for ESTACION-PRO

**Version**: 1.3  
**Date**: April 12, 2026  
**Status**: High compatibility for Batocera v4.0 - v7 themes with dynamic Theme Configuration profile support

---

## 🎯 Overview

ESTACION-PRO now has **high compatibility** with Batocera-EmulationStation themes. Most Batocera themes load directly, and the remaining gaps are now concentrated around rarer theme-specific edge cases rather than core `gridtile`, menu-widget, or common subset/runtime behavior.

## ✅ What's Fully Compatible

### Core Theme Structure
- ✅ XML-based theme format with `<theme>` root
- ✅ `<variables>` and custom variable binding
- ✅ `<include>` with `subset` system for theme variations
- ✅ `<customView>` with inheritance
- ✅ Storyboard animations

### Element-Level Attributes (NEW!)
- ✅ **`lang` attribute** - Show element only if theme language matches
  - Example: `<text lang="en_US">English text</text>`
  - Example: `<image lang="es_ES">Imagen en español</image>`
- ✅ **`ifSubset` attribute** - Show element if subset option selected
  - Format: `ifSubset="subsetName:optionName"`
  - Example: `<image ifSubset="colorset:dark">Dark theme image</image>`

### Conditional System
- ✅ **`if` attribute** with comparisons and boolean logic
- ✅ **`tinyScreen` attribute**
- ✅ **`ifArch` attribute**
- ✅ **`ifHelpPrompts` attribute**
- ✅ Logical operators: `&&`, `||`, `!`
- ✅ Membership operators: `in`, `not in`
- ✅ Case-insensitive operators: `~=`, `!~=`, `==i`, `!=i`

### Image/Media Elements
- ✅ `<image>` element with all positioning/sizing properties
- ✅ `<video>` element with snapshot fallbacks
- ✅ `<carousel>` with full customization
- ✅ `<grid>` / `<imagegrid>` layouts
- ✅ **NEW**: `flipHorizontal` and `flipVertical` image effects
- ✅ **NEW**: Batocera flip aliases `flipX` / `flipY`
- ✅ **NEW**: Batocera v29+ format `<x>`, `<y>`, `<w>`, `<h>` (auto-normalized to `pos`, `size`)

### Colors, Fonts, Sizing
- ✅ All color formats: `RRGGBB` and `RRGGBBAA` hex
- ✅ Gradient support: `colorEnd` and `gradientType`
- ✅ Font sizing with `fontSize` property
- ✅ Text alignment, letter case, line spacing
- ✅ `glowColor`, `glowSize`, `glowOffset` on text and datetime components

### Batocera-Specific Features
- ✅ Inline subset filtering: `<include subset="...">`
- ✅ Inline subset capability discovery from included XML and inline `<include subset="..." name="...">` declarations
- ✅ Batocera themes with/without `<variant>` entries
- ✅ Legacy scalar values in pair properties
- ✅ Flexible "enabled" attribute in animations
- ✅ Extra attributes (safely ignored)
- ✅ Grid `imageSource` alias support
- ✅ Symmetric grid container `padding`
- ✅ Legacy Batocera locale/path aliases: `$language`, `$lang`, `$region`
- ✅ Dynamic Theme Configuration persistence per theme
- ✅ Theme profile export/import from UI (single theme and batch)

---

## ✅ Current Coverage

### Supported in Runtime
| Feature | Status | Notes |
|---------|--------|-------|
| `gridtile` element family | Supported | Default/selected fallback parsing, backgrounds, tile text/corner-radius styling, size derivation, selection semantics, visibility gating, overlays |
| `gridtile.selectionMode` on default/selected tiles | Supported | Selected priority; `image` mode no longer falls back to full-tile selector when image bounds are unavailable |
| `gridtile.imageSizeMode` / per-tile overlay stack | Supported | `size`/`minSize`/`maxSize`, per-state override, and `selected > default` with `zIndex` ordering |
| Menu widgets: `menuText`, `menuTextSmall`, `menuTextEdit`, `menuSwitch`, `menuSlider`, `menuButton`, `menuGroup` | Supported | All seven Batocera widget types map to runtime `mMenuColor*` channels |

### Detailed Compatibility Notes
| Feature | Current behavior |
|---------|------------------|
| Grid `padding` | Supported as symmetric container padding on ESTACION-PRO grids |
| Grid `selectionMode` | Approximation available at grid container level, plus fallback from `gridtile` default/selected |
| `gridtile.overlay` / `gridtile.overlay:selected` | Supported as per-tile image overlays |
| `gridtile.favorite` / `gridtile.favorite:selected` | Supported and rendered conditionally when game is favorite |
| `gridtile.marquee` / `gridtile.marquee:selected` | Supported with dynamic marquee media from current game |
| `gridtile.cheevos` / `gridtile.cheevos:selected` | Supported as approximation (mapped to `completed` metadata state) |
| Mixed `gridtile` default/selected overlays | Supported with per-family selected-state priority and sorted by image `zIndex` |
| `gridtile.default.backgroundColor` | Supported: maps to grid tile background color rectangle |
| `gridtile.selected.backgroundColor` | Supported: highlighted tile renders with a distinct background color |
| `gridtile.default.textColor` / `gridtile.default.textBackgroundColor` | Supported: default tile name text and text-background colors fallback from default gridtile (parser/runtime mapping aligned in Session 36) |
| `gridtile.selected.textColor` / `gridtile.selected.textBackgroundColor` | Supported: selected tile text colors fallback from selected gridtile (applied after native selected defaults; parser/runtime mapping aligned in Session 36) |
| `gridtile.default.imageCornerRadius` | Supported: rounds tile images when root `grid.imageCornerRadius` is not defined (parser/runtime mapping aligned in Session 36) |
| `gridtile.default.size` + `gridtile.selected.size` | Supported: selection scale factor is derived automatically from available axis ratios (x/y), including one-axis definitions |
| `gridtile.default.size` (without `grid.itemSize`) | Supported: used as base tile size fallback (normalized or absolute values) |
| `gridtile.selected.size` (without `grid.itemSize` and without `gridtile.default.size`) | Supported: used as base tile size fallback when selected is the only size source |
| `gridtile.default.visible` | Supported as default-style gate: when false, default fallback overrides and default overlay styles are ignored |
| `gridtile.selected.visible` | Supported as selected-style gate: when false, selected fallback overrides and selected overlay styles are ignored |
| `gridtile.selectionMode` / `gridtile.imageSizeMode` value parsing | Supported with case-insensitive and whitespace-tolerant parsing (`full`/`image`, `size`/`minSize`/`maxSize`) |
| `grid.imageGradientType` value parsing | Supported with case-insensitive and whitespace-tolerant parsing (`horizontal`/`vertical`) |
| `imagegrid` advanced grid-style properties | Supported: parser coverage now mirrors runtime grid behavior for gradients/selector/background/text and transition-related keys (Session 37) |
| `imagegrid.gameImage` / `imagegrid.folderImage` aliases | Supported: mapped to ESTACION-PRO `defaultImage` / `defaultFolderImage` fallback channels (Session 119) |
| `carousel` / `gamecarousel` logo/marquee fallback | **Fully Batocera-compatible**: If no logo/marquee is declared, ESTACION-PRO now automatically falls back to the game's logo (Session 119, matches Batocera behavior) |
| `carousel.imageSource` alias | Supported: `imageSource` now maps to the same media-priority parsing path as `imageType` (Session 38) |
| `textlist.selectorOffsetY` alias | Supported: legacy Batocera selector Y-offset key now maps to runtime textlist selector offset handling (Session 39) |
| `text.glowOffset`, `datetime/clock glow*`, `clock.stationary` | Supported: parser declarations now match existing runtime handling in `TextComponent`/`DateTimeComponent`, so these keys no longer get dropped (Session 40) |
| `systemstatus.colorEnd` | Supported: end-color for icon/text color-shift gradient now declared in parser; was consumed at runtime but silently dropped (Session 41) |
| `gamelistinfo` text-compatible keys (`text/systemdata/metadata/defaultValue`, suffix/case, glow, spacing, background margins/radius) | Supported: parser declarations now mirror `TextComponent` runtime consumption used by gamelist info labels, so these keys are no longer silently dropped (Session 43) |
| `gamecarousel` alias + wheel type naming (`vertical_wheel`, `horizontal_wheel`) | Supported: `gamecarousel` maps to `carousel`, and snake_case wheel type names are normalized to runtime variants (Session 45) |
| Additional Batocera theme subsets beyond dedicated selectors | Supported: UI theme settings now expose extra subsets dynamically using `ThemeSubset_<name>` persistence, so non-standard selectors such as `shader`, `selectorstyle`, `logocolor`, `randomart`, etc. are no longer hidden behind hardcoded subset assumptions (Session 57) |
| Subset `appliesTo` metadata in UI settings | Supported: subset capabilities now preserve `appliesTo`, and dynamic extra subset selectors are filtered live by the currently selected gamelist view style so view-specific options are not shown out of context (Session 58) |
| Subsets declared in included Batocera XML files | Supported: theme capability discovery now scans nested `<include>` graphs when collecting subsets, so selectors declared in files like ArcadePlanet `_art/*.xml` and `_art/systemview/*.xml` become visible to the settings UI instead of being limited to the root `theme.xml` (Session 59) |
| Placeholder-based subset labels in settings UI | Supported with readable fallback: unresolved `displayName="${...}"` subset and option labels now fall back to sanitized names derived from the subset/option key, preventing raw placeholder text from leaking into the settings menu (Session 60) |
| Subset-level `ifSubset` dependencies in dynamic settings UI | Supported: capability discovery now retains subset `ifSubset` conditions and dynamic extra subset selectors are filtered against current subset selections, so conditional controls (for example `logocolor` behind `logostyle:monochrome`) no longer appear when prerequisites are not active (Session 61) |
| Batocera inline subset options declared only on `<include subset="..." name="...">` | Supported: these options are now collected into theme capabilities even without an explicit `<subset>` block, so they show up in dynamic Theme Configuration UI instead of being silently unavailable (Session 77) |
| Legacy Batocera placeholder aliases `$language`, `$lang`, `$region` | Supported: resolved before `${...}` placeholder evaluation for better compatibility with older Batocera/Retrobat theme packs (Session 77) |
| Theme-specific Theme Configuration persistence | Supported: each theme stores/restores its own subset/profile state under appdata `themesettings/<theme>.cfg`, including automatic save/load when switching themes (Session 78) |
| Manual Theme Configuration profile lifecycle in UI | Supported: `SAVE`, `LOAD`, and `RESET THEME CONFIGURATION PROFILE` actions are available directly inside `UI SETTINGS` (Session 79) |
| External Theme Configuration profile transfer | Supported: single-theme import/export to `themesettings/exchange/<theme>.cfg` from `UI SETTINGS` (Session 80) |
| Batch Theme Configuration profile transfer | Supported: one-click export/import for all detected themes from `UI SETTINGS` (Session 81) |
| Dynamic binding tokens in bindable properties (`{global:*}`, `{system:*}`, `{subset:*}`, `{collection:*}`, `{game:*}`, `{systemrandom:*}`) | Supported: parser resolves these token families in property values and storyboard fields after static `${...}` expansion. `collection:*` is mapped to collection context (`system.*` when active; empty otherwise). `game:*` includes runtime cursor-context injection from `GamelistView` (`game.name/path/media/metadata/...`) plus `game.system.*` bridge keys, and `game:system:*` prioritizes `game.system.<prop>` lookup. `systemrandom:*` and `system:random:*` are variable-bridge compatible. Dot-notation tokens without source colon (for example `{system.theme}`) resolve through direct variable lookup; no-colon aliases `{lang}`, `{themePath}`, `{currentPath}` are supported; global aliases include `{global:language}`, `{global:battery}`, `{global:batteryLevel}`, `{global:cheevos}`, `{global:cheevosUser}`, `{global:netplay}`, `{global:netplay.username}`, `{global:ip}` plus dot-style screen aliases (`screen.width`, `screen.height`, `screen.ratio`, `screen.vertical`) and static placeholder aliases `${global.cheevos}`, `${global.cheevos.username}`, `${global.netplay}`, `${global.netplay.username}`, `${screen.*}` (Sessions 66-69, 83-85). |
| Storyboard `animation enabled` expression semantics | Supported: `enabled` evaluates boolean/comparison expressions, ternary conditions, object/global method calls, and Batocera bindings after resolver expansion, including file/path methods, string methods, math helpers, and date/time helpers (`elapsed`, `expandseconds`, `formatseconds`, `date`, `time`, `year`, `month`, `day`), with safe fallback for unresolved dynamic forms (Sessions 63-65, 83-84) |
| `menu` / `screen` view blocks in Batocera themes | Supported: parser now accepts these view names, avoiding silent skips for menu-related theme blocks (Session 45) |
| `imagegrid` primary declarations consumed where `grid` is expected | Supported: `ThemeData::getElement(... expected "grid")` accepts `imagegrid` as compatible type (Session 45) |
| Included XML asset paths declared root-relative (common Batocera layout) | Supported via fallback: missing include-relative assets are re-resolved against theme root path (Session 46) |
| `menuText` | Supported: maps `color` → primary text, `selectorColor` → selector bar, `separatorColor` → separators |
| `menuTextSmall` | Supported: maps `color` → secondary/small text color (`mMenuColorSecondary`) |
| `menuSwitch` | Supported as Batocera alias: maps to menu primary/secondary/selector colors |
| `menuSlider` | Supported as Batocera alias: maps to menu primary/disabled-knob/selector colors |
| `menuTextEdit` | Supported: maps `color`/`inactive`/`active` to text input + keyboard text colors |
| `menuButton` | Supported: maps `color`/`colorFocused`/`textColor`/`textColorFocused` to button color globals |
| `menuGroup` | Supported: maps `color` → section header text (`mMenuColorTertiary`), `separatorColor` → separator lines |

---

## 🆕 NEW: Format Normalization (Batocera v29+ Support)

ESTACION-PRO now automatically converts Batocera v29+ positioning format to its native format.

### Before (Batocera v29+)
```xml
<image name="logo">
  <x>0.1</x>
  <y>0.2</y>
  <w>0.3</w>
  <h>0.4</h>
</image>
```

### After (Auto-Converted)
```xml
<!-- Internally normalized to: -->
<pos>0.1 0.2</pos>
<size>0.3 0.4</size>
```

**No action required** - ESTACION-PRO handles this automatically.

---

## 🎪 NEW: Element-Level Language & Subset Filtering

ESTACION-PRO now supports Batocera's element-level conditional attributes directly. This makes multilingual themes much easier to create.

### Using `lang` Attribute

Show different elements based on the selected theme language:

```xml
<text name="help_en" lang="en_US">
  <text>Press A to select</text>
</text>

<text name="help_es" lang="es_ES">
  <text>Presione A para seleccionar</text>
</text>

<text name="help_fr" lang="fr_FR">
  <text>Appuyez sur A pour sélectionner</text>
</text>
```

Only the element matching the current language will be displayed. **No need for variable-based workarounds anymore!**

### Using `ifSubset` Attribute

Show elements conditionally based on selected theme options:

```xml
<!-- Only show if "colorset" subset is set to "dark" -->
<image name="background" ifSubset="colorset:dark">
  <path>./assets/bg_dark.png</path>
</image>

<!-- Only show for "colorset:light" option -->
<image name="background" ifSubset="colorset:light">
  <path>./assets/bg_light.png</path>
</image>

<!-- Only show if "layout" is "classic" -->
<view name="system" ifSubset="layout:classic">
  <!-- Classic layout elements -->
</view>
```

### Comparison: Before vs After

**Batocera way (Now Supported)**:
```xml
<text lang="en_US"><text>Hello</text></text>
<text lang="es_ES"><text>Hola</text></text>
```

---

## 🎪 Image Flipping Support

ESTACION-PRO fully supports image flipping effects. Use these properties on `<image>` elements:

```xml
<image name="logo">
  <flipHorizontal>true</flipHorizontal>  <!-- Mirror left-right -->
  <flipVertical>true</flipVertical>      <!-- Mirror top-bottom -->
</image>
```

---

## 📋 Adapting Batocera Themes to ESTACION-PRO

### Step 1: Identify Incompatibilities
Check your theme.xml for:
```bash
# Search for unsupported attributes
grep -r "if=\"" .                    # Element conditionals
grep -r "lang=\"" . | grep -v include  # Language attributes
grep -r "tinyScreen" .               # Small screen detection
grep -r "ifArch" .                   # Architecture filtering
grep -r "imageSource" .              # Grid media type switching
```

### Step 2: For Language/Conditional Layouts
Batocera uses element-level conditions. ESTACION-PRO now supports them directly:

**Batocera way (Now Fully Supported!):**
```xml
<text name="help" lang="en_US">
  <text>Press A to select</text>
</text>
<text name="help" lang="es_ES">
  <text>Presione A para seleccionar</text>
</text>
```

**This now works out-of-the-box in ESTACION-PRO** ✨

For subset-based conditionals:
```xml
<image ifSubset="colorset:dark">
  <path>./dark-version.png</path>
</image>
<image ifSubset="colorset:light">
  <path>./light-version.png</path>
</image>
```

### Step 3: For Screen-Sized Layouts
ESTACION-PRO now supports Batocera-style expressions directly, so these conditions can be kept as-is:

```xml
<element if="${screen.height} >= 1080">
  <!-- Large screen layout -->
</element>

<element tinyScreen="true">
  <!-- Small-screen layout -->
</element>
```

### Step 4: Remove Unsupported Features
Safely remove or comment out:
- Highly customized `<gridtile ...>` families when the theme depends on Batocera per-tile composition (favorites/cheevos/overlay stacks)
- Advanced `selectionMode` behavior beyond `full` and `image`
- Advanced `imageSizeMode` behavior beyond the ESTACION-PRO approximation

---

## 📚 Compatible Batocera Themes

Most Batocera v4-v7 themes now work **without modification or with minimal changes**:

| Theme | Status | Notes |
|-------|--------|-------|
| **Modern ES-DE** | ✅ Full | Fully compatible now (carousel/gamecarousel logo fallback Batocera parity: Session 119) |
| **Slate ES-DE** | ✅ Full | Tested and working |
| **Linear ES-DE** | ✅ Full | All features supported |
| **Colorful v3** | ✅ Full | Now works with lang support ✨ |
| **Alekfull v9+** | ✅ Full | `lang` conditions now work ✨ |
| **Retrorigs** | ⚠️ Partial | Uses advanced gridtile behavior not fully mirrored |
| **Any Batocera v4.x** | ✅ Mostly | Old formats and conditionals supported |
| **Batocera v5.24+** | ✅ Mostly | Modern conditionals and text glow supported |

**Key Improvement**: Batocera conditionals, expression parsing, language filtering, architecture filtering and text glow/shadow now work directly.

---

## 🔧 Testing Your Theme

After making changes:

1. **Validate XML**:
   ```bash
   xmllint theme.xml  # Check syntax
   ```

2. **Check Log Messages**:
   In ESTACION-PRO debug mode, look for:
   ```
   ThemeData::normalizeElementFormat(): Converting legacy format...
   ImageComponent: flipHorizontal/flipVertical applied...
   ```

3. **Visual Test**:
   - Load theme in ESTACION-PRO
   - Check that images display correctly
   - Verify positioning matches your intent
   - Test flipX/flipY if used

---

## 🚀 Future Improvements Planned

The following features are still relevant for future ESTACION-PRO releases:

1. **Visual QA and regression sweep** - verify a broad sample of Batocera themes across mixed content/media availability
2. **Dedicated Batocera-style Theme Configuration submenu** - optional UX split from the existing dynamic integration inside `UI SETTINGS`

### ✅ Already Implemented in This Update
- ✅ Automatic fallback for logo/marquee in `carousel`/`gamecarousel` when not declared (Batocera parity, Session 119)

### ✅ Already Implemented in This Update
- ✅ Element `lang` attribute - Language filtering
- ✅ Element `ifSubset` attribute - Subset-based filtering
- ✅ Format x,y,w,h normalization - Batocera v29+ support
- ✅ `if`, `tinyScreen`, `ifArch`, `ifHelpPrompts`
- ✅ Membership and case-insensitive operators
- ✅ `imageSource` for grid
- ✅ All seven Batocera menu widget types: `menuText`, `menuTextSmall`, `menuTextEdit`, `menuSwitch`, `menuSlider`, `menuButton`, `menuGroup`
- ✅ Text `glowOffset` shadow rendering
- ✅ Symmetric grid padding support
- ✅ `imagegrid` recognized as primary grid component
- ✅ `gridtile` default/selected fallback parsing (`padding`, `selectionMode`, `imageColor`, `imageSizeMode`)
- ✅ `gridtile.selected.selectionMode` fallback with selected-state priority when native `selectionMode` is missing
- ✅ `gridtile.default.backgroundColor` and `gridtile.selected.backgroundColor` per-state tile background
- ✅ `gridtile.default.size` + `gridtile.selected.size` → automatic `itemScale` derivation (works with one or both axes)
- ✅ `gridtile.default.visible` now gates default fallback overrides and default overlay styles
- ✅ `gridtile.selected.visible` gating for selected fallback overrides
- ✅ `gridtile.selectionMode` and `gridtile.imageSizeMode` parsing is now case-insensitive and whitespace-tolerant
- ✅ `selectionMode=image` no longer falls back to full-tile selector when an image is unavailable

### 🚧 Estado actual de los aliases dinámicos
- ✅ `collection:*` support is implemented via `{collection:...}` and safely degrades when not in collection context.
- ✅ `system:*` support is implemented, including `{system:random:...}` and `{systemrandom:...}` variants.
- ✅ `game:*` support is implemented, including `{game:system:...}` nested property access.
- ✅ `global:*` support is now improved: `{global:clock}`, `{global:help}`, `{global:arch}`, `{global:architecture}`, `{global:cheevos.username}`, `{global:netplay.username}`, `{global:battery}`, `{global:battery.level}`, and related aliases are resolved.
- ⚠️ Remaining work: validate edge-case themes that use less common Batocera global aliases such as `{global:ip}` and other rare `global:*` names, and exercise method-heavy `if` expressions / storyboard enabled expressions against real Batocera theme packs.

- ✅ `gridtile.default.size` now provides base `itemSize` fallback when `grid.itemSize` is not set
- ✅ `gridtile.selected.size` can now provide base `itemSize` fallback when it is the only available size definition
- ✅ `gridtile.selected.visible` now also gates selected overlay styles (`overlay:selected`, `favorite:selected`, `marquee:selected`, `cheevos:selected`)
- ✅ layout metrics are recalculated when gridtile fallbacks derive a new `itemScale` (prevents stale spacing/margins)
- ✅ grid theme opacity resets to default on applyTheme start (prevents opacity carryover when new theme omits `opacity`/`visible`)
- ✅ selected color flags reset on applyTheme start (prevents stale selected tint/background behavior across theme reloads)
- ✅ background/selector image resources and unfocused-saturation flag reset on applyTheme start (prevents stale visuals and stale dimming behavior across theme reloads)
- ✅ baseline grid defaults now reset each applyTheme pass (item size/scale, image fit/colors, selector/text defaults, unfocused defaults), preventing carryover when properties are omitted
- ✅ transition/letter-case/suffix/fade defaults reset each applyTheme pass (prevents stale behavior/text-format state when those properties are omitted)
- ✅ additional string enums now parse case-insensitively and whitespace-tolerantly (`imageFit`, `imageInterpolation`, `selectorLayer`, gradient types, transitions, `letterCase*`)
- ✅ inline subset capability discovery from root and included XML files, including inline `<include subset="..." name="...">` declarations
- ✅ legacy Batocera `$language`, `$lang`, `$region` alias compatibility in placeholder resolution
- ✅ theme-scoped Theme Configuration profile persistence with auto save/load on theme switch
- ✅ UI profile actions: save, load, reset
- ✅ external Theme Configuration profile import/export for current theme
- ✅ batch Theme Configuration profile import/export for all themes

---

## 📞 Support

For issues with theme compatibility:

1. Check this guide for known limitations
2. Verify XML syntax with `xmllint`
3. Enable debug logging in ESTACION-PRO settings
4. Check `es_log.txt` for detailed error messages

---

**ESTACION-PRO**: Making Batocera themes work seamlessly. 🎮
