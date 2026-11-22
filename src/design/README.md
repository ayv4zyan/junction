# JunctionDesign

The look of Junction, in one module: **tokens** (the values), a **Qt Quick Controls style** (what
plain `Button {}`, `Menu {}`, `TextField {}` look like) and a few **components**. It knows nothing
about Canvases or the Library; the app depends on it, never the other way round
([ADR 0003](../../docs/adr/0003-own-controls-style-in-design-module.md)).

```qml
import QtQuick.Controls
import JunctionDesign

Button { text: qsTr("Save"); highlighted: true }          // styled automatically
Rectangle { color: Colors.panel; height: Sizes.control }   // tokens
ask.confirm({ title: qsTr("Delete “%1”?").arg(name), action: qsTr("Delete"), destructive: true,
              onAccepted: () => library.deleteCanvas(project, name) })
```

Check a change in **`junction-gallery`** (built next to the app): every token and component, in
every state, in both themes.

## The rules

1. **Tokens, never typed-in values.** Screens take colors from `Colors`, spacing from `Space`,
   heights and icon sizes from `Sizes`, text sizes from `Fonts`, timing from `Motion`; the values
   themselves live only here. Hairlines (1px, 1.5px), offsets of a few pixels around an outline,
   and sizes in canvas units divided by the zoom are fine as numbers.
2. **Square.** Every corner is `Sizes.radius` (0): panels, buttons, rows, swatches, Cards,
   exports. Something is round only when it marks a *point*, not a surface: Handles and Waypoints.
3. **Compact, on a 4px grid.** Controls and rows are 28px (`Sizes.control`), toolbars 40px.
   Lists and controls use the tight steps; containers (dialogs, toasts) the roomy ones (`Space.xl`).
4. **One typeface.** Geist, bundled: 11 for counts and hints, 13 for all UI text, 15 for titles and
   Card text. Icons are 16px; small indicators (chevrons, checks) 12px.
5. **One level of depth.** Things that float above the window (menus, tooltips, dialogs, toasts,
   the selection toolbar) use `Floating`: border plus one crisp shadow. Nothing else has a shadow.
6. **Readable color.** Text reaches WCAG AA (4.5:1), colored lines 3:1. The accent fill marks the
   main action; red (`danger`) is only for what destroys something, and for errors.
7. **Motion with a purpose.** Animate only to show where something came from or went
   (continuity), that an action worked (feedback), what changed, what is on top (layering), or
   what can be done here (hints). Things coming in ease out (`Motion.enter`); things leaving ease
   in and take 2/3 of the time (`Motion.out()`). Nothing the pointer moves directly (dragging,
   panning, zooming) is ever animated.

## How it is enforced

- **The build checks the app.** `design-check` (in `check/`) runs over `src/app/qml` before the app
  builds, and fails the build on code that goes around the design system: colors (hex, `Qt.rgba`,
  named), font sizes, `pointSize`, font families, durations and delays, `Easing` curves, rounded
  corners, and importing a specific Controls style or `QtQuick.Templates`. Its rules are tested in
  `tst_design_check`. A line that really must differ says why:
  `delay: 0   // design-exception: shows at once, while typing`.
- **The C++ side's copies** of design values (`src/core/DesignValues.h`, for painting and export)
  are compared with the tokens by `tst_design_values`.
- **CI** (`.github/workflows/ci.yml`) builds and tests every pull request, so all of this applies
  even when nothing was run locally.
- **The boundary:** `junction-gallery` links this module alone, so it stops building if the module
  ever depends on the app.

## What's here

| Kind | Files |
|---|---|
| Tokens | `Colors` `Space` `Sizes` `Fonts` `Motion` |
| Controls style | `ApplicationWindow` `Label` `Button` `ToolButton` `TextField` `CheckBox` `Menu` `MenuItem` `MenuSeparator` `ToolTip` `Dialog` `ScrollBar` `ToolBar` (anything else falls back to Fusion) |
| Components | `Floating` (the floating surface), `Toast` (message + optional action), `Swatch` (a color to pick), `Separator`, `AskDialog` (`confirm` / `text`), `MenuEntry` (menu item with a shortcut hint, optionally red), `TextMenu` (cut/copy/paste for any text field), `FocusRing` |
| Typeface | `fonts/` (Geist, SIL Open Font License), installed by `JunctionDesign::installFonts()` |
| Check | `check/`: `design-check`, run by the app's build (see above) |

Screen pieces (sidebar rows, the selection toolbar, Cards) are built *from* these, but live in the
app.
