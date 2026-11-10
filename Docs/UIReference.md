# Hades: UI Reference

Every UI asset in the project, as it is built in the editor: its parent class, its widget hierarchy, every property changed from its default, its graph, its Class Defaults, and what its C++ class does with it. Anything not listed is left at its default.

This is the state of the UI after patch 0010. How the UI works is in [UI](UI.md); how to build each asset step by step, and how to update from an earlier patch, is in [Inventory UI Setup](InventoryUISetup.md).

![The inventory screen with the name of each widget](Images/ui-inventory-anatomy.png)

## Contents

1. [Reading this reference](#1-reading-this-reference)
2. [Input and project settings](#2-input-and-project-settings)
3. [Root layout](#3-root-layout)
4. [Style kit](#4-style-kit)
5. [Shared widgets](#5-shared-widgets)
6. [Inventory pieces](#6-inventory-pieces)
7. [Screens](#7-screens)
8. [HUD](#8-hud)
9. [Where the widgets are used](#9-where-the-widgets-are-used)
10. [Appendix. Colors and sizes](#appendix-colors-and-sizes)

---

## 1. Reading this reference

Each widget is described in the same order:

- **A table** with its parent class, its folder, and what uses it.
- **Hierarchy**: every widget in it, in order. A name in the tree, such as `Background`, is the widget's exact name; a type alone, such as `Overlay`, keeps its default name. The widget's type is in brackets.
- **Properties**: every widget in the hierarchy, in the same order, with each property that differs from its default. Paths follow the Details panel, such as **Brush › Draw As**. A widget listed with *no changes* keeps all its defaults.
- **Graph**: every node, when it has one.
- **Class Defaults**: the widget's own settings, in **Graph › Class Defaults**.
- **C++**: what the parent class needs and does. Named parts marked *required* must exist with that exact name, or the Blueprint doesn't compile.

Notation:

| Written | Meaning |
| --- | --- |
| **Is Variable** | The checkbox beside the name, ticked. |
| Slot: Fill/Fill | The widget's slot in its parent: Horizontal Alignment Fill, Vertical Alignment Fill. |
| Slot: H Center, V Top | Horizontal Alignment Center, Vertical Alignment Top. |
| Slot: Size Auto / Fill | In a Horizontal or Vertical Box. |
| Padding 6 | All four sides 6. Padding Left 12, Top 4 means those two; the others are 0. |
| `C8A25AFF` | A color as hex sRGB: red, green, blue, alpha. |
| Width 64 | **Width Override** ticked and set to 64; Height likewise. |

Brushes drawn with one of the three UI textures are drawn as a Box: **Draw As** Box, **Image** the texture, **Image Size** 64 × 64, **Margin** on all four sides, **Tint** `FFFFFFFF`. Rounded Box brushes always have **Rounding Type** Fixed Radius unless written otherwise. A Border's **Brush Color**, outside its brush, stays `FFFFFFFF` unless listed.

---

## 2. Input and project settings

### 2.1 UI actions

**Project Settings › Game › Common UI Input Settings › Input Actions**:

| Action Tag | Default Display Name | Key Mappings |
| --- | --- | --- |
| `UI.Action.Escape` | Pause | `Escape`, `Gamepad Special Right` |
| `UI.Action.Inventory` | Inventory | `Tab`, `Gamepad Special Left` |

Nothing else is in the list.

### 2.2 `DT_UIInputActions`

| | |
| --- | --- |
| Type | Data Table, Row Structure `CommonInputActionDataBase` |
| Folder | `Content/Input` |
| Used by | `B_CommonInputData`, the screens' and menus' Input actions, the Quick Slot Component's Slot Keys, `W_InteractableMarker`, `W_InventoryScreen`'s tab keys |

| Row | Display Name | Keyboard Input Type Info › Key | Default Gamepad Input Type Info › Key | Nav Bar Priority |
| --- | --- | --- | --- | --- |
| `Confirm` | Confirm | `Enter` | `Gamepad Face Button Bottom` | 5 |
| `Back` | Close | `Escape` | `Gamepad Face Button Right` | 6 |
| `Organize` | Auto Sort (Hold) | `O` | `Gamepad Left Thumbstick Button` | 1 |
| `Move` | Move | `Space Bar` | `Gamepad Face Button Left` | 2 |
| `Rotate` | Rotate | `R` | `Gamepad Face Button Top` | 3 |
| `Split` | Split | `Left Shift` | `Gamepad Left Trigger` | 4 |
| `PreviousTab` | Previous Tab | `Q` | `Gamepad Left Shoulder` | 0 |
| `NextTab` | Next Tab | `E` | `Gamepad Right Shoulder` | 0 |
| `Shortcut1` | Shortcut 1 | `One` | `Gamepad D-pad Up` | 0 |
| `Shortcut2` | Shortcut 2 | `Two` | `Gamepad D-pad Right` | 0 |
| `Shortcut3` | Shortcut 3 | `Three` | `Gamepad D-pad Down` | 0 |
| `Shortcut4` | Shortcut 4 | `Four` | `Gamepad D-pad Left` | 0 |
| `Interact` | Interact | `F` | `Gamepad Face Button Left` | 0 |

`Organize` also has **Action Requires Hold** ticked and **Hold Time** `0.6`, under both **Keyboard Input Type Info** and **Default Gamepad Input Type Info**.

### 2.3 `B_CommonInputData`

| | |
| --- | --- |
| Parent class | `CommonUIInputData` |
| Folder | `Content/Input` |
| Used by | **Project Settings › Game › Common Input Settings › Input Data** |

**Class Defaults**

- **Default Click Action**: Data Table `DT_UIInputActions`, Row Name `Confirm`
- **Default Back Action**: Data Table `DT_UIInputActions`, Row Name `Back`

### 2.4 `CID_KeyboardMouse` and `CID_Gamepad`

| | |
| --- | --- |
| Parent class | `CommonInputBaseControllerData` |
| Folder | `Content/Input` |
| Used by | **Project Settings › Game › Common Input Settings › Platform Input › Windows › Controller Data**: element 0 `CID_KeyboardMouse`, element 1 `CID_Gamepad` |

**Class Defaults of `CID_KeyboardMouse`**

- Default › **Input Type**: Mouse and Keyboard
- Display › **Input Brush Data Map**: one element per key below. Each sets **Key**, **Key Brush › Image** (your icon) and **Key Brush › Image Size**: Y `40`, X so the icon keeps its shape (`40` for a square icon, `80` for one twice as wide).

| Key | Shown for |
| --- | --- |
| `Enter` | Confirm |
| `Escape` | Close |
| `Space Bar` | Move |
| `O` | Auto Sort |
| `R` | Rotate |
| `Q` | Previous tab |
| `E` | Next tab |
| `One` to `Four` | Shortcuts 1 to 4 |
| `F` | Interact, in the prompt |

**Class Defaults of `CID_Gamepad`**

- Default › **Input Type**: Gamepad
- Gamepad › **Gamepad Name**: Generic; **Gamepad Display Name**: `Gamepad`
- Display › **Input Brush Data Map**, set the same way:

| Key | Shown for |
| --- | --- |
| `Gamepad Face Button Bottom` | Confirm |
| `Gamepad Face Button Right` | Close |
| `Gamepad Face Button Left` | Move, and Interact in the prompt |
| `Gamepad Left Thumbstick Button` | Auto Sort |
| `Gamepad Face Button Top` | Rotate |
| `Gamepad Left Shoulder` | Previous tab |
| `Gamepad Right Shoulder` | Next tab |
| `Gamepad D-pad Up`, `Right`, `Down`, `Left` | Shortcuts 1 to 4 |

**Platform Input › Windows** also sets: **Default Input Type** Mouse and Keyboard; **Supports Mouse and Keyboard** ticked; **Supports Gamepad** ticked; **Default Gamepad Name** Generic.

### 2.5 Key and tab icon textures

Every key icon and tab icon texture, 128 × 128:

- Compression › **Compression Settings**: UserInterface2D (RGBA)
- Level Of Detail › **Mip Gen Settings**: SimpleAverage
- Level Of Detail › **Texture Group**: UI

---

## 3. Root layout

### 3.1 `W_PrimaryGameLayout`

The root layout Common Game gives each local player: four layer stacks, with the messages above them.

| | |
| --- | --- |
| Parent class | `PrimaryGameLayout` |
| Folder | `Content/UI/Foundation` |
| Used by | `B_HadesUIPolicy` (**Layout Class**) |

**Hierarchy**

```
[W_PrimaryGameLayout]
└─ Overlay
   ├─ GameLayer_Stack (Common Activatable Widget Stack)
   ├─ GameMenuLayer_Stack (Common Activatable Widget Stack)
   ├─ MenuLayer_Stack (Common Activatable Widget Stack)
   ├─ ModalLayer_Stack (Common Activatable Widget Stack)
   └─ W_Message (W_Message)
```

**Properties**

- **Overlay**: no changes.
- **`GameLayer_Stack`**, **`GameMenuLayer_Stack`**, **`MenuLayer_Stack`**, **`ModalLayer_Stack`**: **Is Variable**; Slot: Fill/Fill.
- **W_Message**: Slot: H Center, V Bottom; Padding Bottom 112.

**Graph**

1. **Event On Initialized** → **Register Layer** (Layer Tag `UI.Layer.Game`, Layer Widget `GameLayer_Stack`) → **Register Layer** (`UI.Layer.GameMenu`, `GameMenuLayer_Stack`) → **Register Layer** (`UI.Layer.Menu`, `MenuLayer_Stack`) → **Register Layer** (`UI.Layer.Modal`, `ModalLayer_Stack`).

**C++.** Common Game's `UPrimaryGameLayout` pushes screens onto the registered stacks by tag. Later children of the Overlay draw on top, so `W_Message` is above every layer.

### 3.2 `B_HadesUIPolicy`

| | |
| --- | --- |
| Parent class | `GameUIPolicy` |
| Folder | `Content/UI/Foundation` |
| Used by | `Config/DefaultGame.ini`: `[/Script/Hades.HadesUIManagerSubsystem]` `DefaultUIPolicyClass=/Game/UI/Foundation/B_HadesUIPolicy.B_HadesUIPolicy_C` |

**Class Defaults**

- **Layout Class**: `W_PrimaryGameLayout`

---

## 4. Style kit

All in `Content/UI/Style`.

### 4.1 Textures

| Texture | Looks like | Margin when drawn as a Box | Used by |
| --- | --- | --- | --- |
| `T_UI_Panel` | A dark panel with a quiet outline and brighter corner marks | `0.375` | `W_Panel` |
| `T_UI_Cell` | A dark cell that looks sunk in, shaded from the top and left | `0.25` | `BS_Slot`, `BS_SlotHolding`, `W_EquippedItem` |
| `T_UI_ItemTile` | A box lit softly from above, with a faint bevel | `0.25` | `W_InventoryItem` |

Imported from `SourceArt/UI`, 64 × 64. Each:

- Compression › **Compression Settings**: UserInterface2D (RGBA)
- Level Of Detail › **Texture Group**: UI
- **Mip Gen Settings**: FromTextureGroup (the default)

### 4.2 Text styles

Parent class `CommonTextStyle`. Every one uses **Font Family** Roboto (engine content) and **Margin** 0. When **Uses Drop Shadow** is ticked: **Shadow Offset** X `1`, Y `1`, **Shadow Color** `000000E6`.

| Asset | Typeface | Size | Letter Spacing | Color | Uses Drop Shadow | Line Height Percentage | Used for |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `TS_Title` | Regular | 34 | 400 | `E8E2D4FF` | off | 1 | Screen titles: ITEM BOX, PAUSED |
| `TS_Label` | Bold | 12 | 250 | `8A857BFF` | off | 1 | Panel titles, small labels, tab labels, the placeholder pages, the shortcut menu's title |
| `TS_ItemName` | Bold | 24 | 40 | `E8E2D4FF` | off | 1 | The item name in the details panel |
| `TS_Body` | Regular | 16 | 0 | `BDB6A8FF` | off | 1.15 | Descriptions, the action bar and its hint, names in the item box and the in-hand panel |
| `TS_Count` | Bold | 15 | 0 | `E8E2D4FF` | on | 1 | Stack counts and rounds |
| `TS_Button` | Bold | 15 | 120 | `E8E2D4FF` | off | 1 | Menu buttons, shortcut entries and the interaction prompt |
| `TS_Message` | Regular | 19 | 20 | `E8E2D4FF` | on | 1 | Messages such as "It's locked." |

### 4.3 `BS_Slot`

Cells and slots.

| | |
| --- | --- |
| Parent class | `CommonButtonStyle` |
| Used by | `W_InventoryCell`, `W_InventorySlot`, `W_ItemBoxEntry` (**Style**) |

**Class Defaults**

- **Normal Base**: Draw As Box; Image `T_UI_Cell`; Image Size 64 × 64; Margin `0.25`; Tint `FFFFFFFF`
- **Normal Hovered**: Draw As Rounded Box; Tint `1C1A16FF`; Outline Settings › Corner Radii `0, 0, 0, 0`, Color `C8A25AFF`, Width `2`, Rounding Type Fixed Radius
- **Normal Pressed**: Draw As Rounded Box; Tint `2A251DFF`; Outline Settings › Corner Radii `0, 0, 0, 0`, Color `E2C27EFF`, Width `2`, Rounding Type Fixed Radius
- **Disabled**: Draw As Rounded Box; Tint `0A0A0A80`; Outline Settings › Corner Radii `0, 0, 0, 0`, Color `1A1916FF`, Width `1`, Rounding Type Fixed Radius
- **Button Padding**: `0`
- **Min Width** `0`, **Min Height** `0`
- Selected brushes and text styles: no changes.

Common UI shows the Hovered look under the mouse and under gamepad focus, so the gold outline is also the gamepad cursor.

### 4.4 `BS_SlotHolding`

Cells while an item is moved: no hover outline, as the moved item shows where it goes.

| | |
| --- | --- |
| Parent class | `CommonButtonStyle` (a duplicate of `BS_Slot`) |
| Used by | `W_InventoryCell` (**Holding Style**) |

**Class Defaults**: as `BS_Slot`, except:

- **Normal Hovered**: Draw As Box; Image `T_UI_Cell`; Image Size 64 × 64; Margin `0.25`; Tint `FFFFFFFF`
- **Normal Pressed**: the same

### 4.5 `BS_MenuButton`

The item menu, shortcut menu and pause menu buttons.

| | |
| --- | --- |
| Parent class | `CommonButtonStyle` |
| Used by | `W_MenuButton`, `W_ShortcutEntry` (**Style**) |

**Class Defaults**

- **Normal Base**: Draw As Rounded Box; Tint `00000000`; Outline Settings › Corner Radii `2, 2, 2, 2`, Color `00000000`, Width `0`, Rounding Type Fixed Radius
- **Normal Hovered**: Draw As Rounded Box; Tint `C8A25A2E`; Outline Settings › Corner Radii `2, 2, 2, 2`, Color `C8A25AFF`, Width `1`, Rounding Type Fixed Radius
- **Normal Pressed**: Draw As Rounded Box; Tint `C8A25A59`; Outline Settings › Corner Radii `2, 2, 2, 2`, Color `E2C27EFF`, Width `1`, Rounding Type Fixed Radius
- **Disabled**: Draw As Rounded Box; Tint `00000000`; Outline Settings › Width `0`, Rounding Type Fixed Radius
- **Button Padding**: Left `14`, Top `9`, Right `14`, Bottom `9`
- **Min Width** `208`, **Min Height** `0`

### 4.6 `BS_ActionButton`

The action bar's entries and the tabs: flat, with a faint highlight under the mouse.

| | |
| --- | --- |
| Parent class | `CommonButtonStyle` |
| Used by | `W_BoundActionButton`, `W_TabButton` (**Style**) |

**Class Defaults**

- **Normal Base**: Draw As Rounded Box; Tint `00000000`; Outline Settings › Corner Radii `2, 2, 2, 2`, Color `00000000`, Width `0`, Rounding Type Fixed Radius
- **Normal Hovered**: Draw As Rounded Box; Tint `C8A25A1F`; Outline Settings › Corner Radii `2, 2, 2, 2`, Color `00000000`, Width `0`, Rounding Type Fixed Radius
- **Normal Pressed**: Draw As Rounded Box; Tint `C8A25A40`; Outline Settings › Corner Radii `2, 2, 2, 2`, Color `00000000`, Width `0`, Rounding Type Fixed Radius
- **Disabled**: Draw As Rounded Box; Tint `00000000`; Outline Settings › Width `0`, Rounding Type Fixed Radius
- **Button Padding**: Left `6`, Top `3`, Right `6`, Bottom `3`
- **Min Width** `0`, **Min Height** `0`
- **Normal Text Style**, **Normal Hovered Text Style**, **Selected Text Style**, **Selected Hovered Text Style**, **Disabled Text Style**: `TS_Body`. The action bar's buttons apply it to their action name.

### 4.7 `BS_QuickSlot`

The HUD's shortcuts: see-through boxes without an outline.

| | |
| --- | --- |
| Parent class | `CommonButtonStyle` |
| Used by | `W_QuickSlot` (**Style**) |

**Class Defaults**

- **Normal Base**: Draw As Rounded Box; Tint `0A0A0AB3`; Outline Settings › Corner Radii `0, 0, 0, 0`, Width `0`, Rounding Type Fixed Radius
- **Button Padding**: `0`
- **Min Width** `0`, **Min Height** `0`

The HUD's shortcuts never take the mouse or focus, so the other brushes are never seen.

---

## 5. Shared widgets

All in `Content/UI/Foundation`.

### 5.1 `W_Panel`

The framed panel every section sits in: a dark panel with corner marks, a small title, a thin line under it, and a slot for content.

| | |
| --- | --- |
| Parent class | User Widget |
| Used by | `W_InventoryScreen`, `W_ItemBoxScreen` |

**Hierarchy**

```
[W_Panel]
└─ Frame (Border)
   └─ Vertical Box
      ├─ Header (Vertical Box)
      │  ├─ TitleText (Common Text)
      │  └─ HeaderRule (Image)
      └─ Content (Named Slot)
```

**Properties**

- **`Frame`** (Border):
  - Brush › Draw As: Box
  - Brush › Image: `T_UI_Panel`
  - Brush › Image Size: 64 × 64
  - Brush › Margin: `0.375`
  - Brush › Tint: `FFFFFFFF`
  - Padding: `20`
- **Vertical Box**: no changes.
- **`Header`** (Vertical Box):
  - **Is Variable**
  - Slot: Size Auto; Horizontal Alignment Fill
- **`TitleText`** (Common Text):
  - **Is Variable**
  - Style: `TS_Label`
  - Text: `TITLE`
- **`HeaderRule`** (Image):
  - Slot: Horizontal Alignment Fill; Padding Top 8, Bottom 14
  - Brush › Image Size: 32 × 1
  - Color and Opacity: `2F2C28FF`
- **`Content`** (Named Slot):
  - Slot: Size Fill; Horizontal Alignment Fill; Vertical Alignment Fill

**Graph**

- Variable **`Title`**: Text, **Instance Editable**, Default Value empty.
- **Event Pre Construct** → `TitleText` › **Set Text** (In Text: `Title`) → `Header` › **Set Visibility** (In Visibility: **Select** with Index `Title` › **Is Empty**: False → Not Hit-Testable (Self & All Children), True → Collapsed).

A panel without a title shows no title row.

**Using it.** On each placed `W_Panel`, **Title** is under **Default** in its Details, and its contents go on its **Content** entry in the Hierarchy.

### 5.2 `W_BoundActionButton`

One entry of the action bar: a key's icon and the action's name.

| | |
| --- | --- |
| Parent class | `CommonBoundActionButton` |
| Used by | The **Action Bar** of `W_InventoryScreen` and `W_ItemBoxScreen` (**Action Button Class**) |

**Hierarchy**

```
[W_BoundActionButton]
└─ Horizontal Box
   ├─ InputActionWidget (Common Action Widget)
   └─ Text_ActionName (Common Text)
```

**Properties**

- **Horizontal Box**: no changes.
- **`InputActionWidget`** (Common Action Widget):
  - **Is Variable**
  - Slot: Size Auto; Vertical Alignment Center
- **`Text_ActionName`** (Common Text):
  - **Is Variable**
  - Slot: Size Auto; Vertical Alignment Center; Padding Left 8
  - Style: `TS_Body`
  - Text: `Action`

**Class Defaults**

- **Style**: `BS_ActionButton`
- Interaction › **Is Focusable**: unticked

No graph.

**C++.** Common UI's `UCommonBoundActionButton` finds `InputActionWidget` and `Text_ActionName` by name. The action bar makes one per action, sets the key's icon and the action's name, and clicking it does the action.

### 5.3 `W_TabButton`

One tab of the tab bar.

| | |
| --- | --- |
| Parent class | `HadesTabButton` |
| Used by | `W_InventoryScreen` (`TabBar`) |

**Hierarchy**

```
[W_TabButton]
└─ Size Box
   └─ Overlay
      ├─ ActiveFrame (Border)
      │  └─ Size Box
      └─ Vertical Box
         ├─ Icon (Image)
         └─ Label (Common Text)
```

**Properties**

- **Size Box**: Width 120; Height 92
- **Overlay**: no changes.
- **`ActiveFrame`** (Border):
  - **Is Variable**
  - Slot: H Center, V Top; Padding Top 2
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `00000000`
  - Brush › Outline Settings › Corner Radii `2, 2, 2, 2`, Color `BDB6A8FF`, Width `1`, Rounding Type Fixed Radius
  - Padding: `0`
- **Size Box** (in `ActiveFrame`, empty): Width 76; Height 68
- **Vertical Box**: Slot: H Center, V Top
- **`Icon`** (Image):
  - **Is Variable**
  - Slot: Horizontal Alignment Center; Padding Top 12
  - Brush › Image Size: 48 × 48
  - Color and Opacity: `E8E2D4FF`
- **`Label`** (Common Text):
  - **Is Variable**
  - Slot: Horizontal Alignment Center; Padding Top 14
  - Style: `TS_Label`
  - Text: `TAB`

**Class Defaults**

- **Style**: `BS_ActionButton`

No graph.

**On each placed tab**: Tab › **Icon Texture**, **Label Text**, and **Active Icon Scale** (1.2).

**C++** (`UHadesTabButton`). Finds `Icon`, `Label` and `ActiveFrame` (all optional). Sets the icon from Icon Texture without changing its size, and the label from Label Text. The open tab's icon is scaled by Active Icon Scale, its label and frame show; on other tabs the label and frame are hidden, not collapsed. Without an icon, the label always shows. Tabs can't take focus.

### 5.4 `W_MenuButton`

A text button.

| | |
| --- | --- |
| Parent class | `CommonButtonBase` |
| Used by | `W_ItemMenu`, `W_EscapeMenu` |

**Hierarchy**

```
[W_MenuButton]
└─ Label (Common Text)
```

**Properties**

- **`Label`** (Common Text):
  - **Is Variable**
  - Style: `TS_Button`
  - Text: `Button`

**Graph**

- Variable **`LabelText`**: Text, **Instance Editable**, Default Value `Button`.
- **Event Pre Construct** → `Label` › **Set Text** (In Text: `LabelText`).

**Class Defaults**

- **Style**: `BS_MenuButton`

**On each placed button**: **Label Text**, under **Default**.

### 5.5 `W_Message`

Messages such as "It's locked."

| | |
| --- | --- |
| Parent class | `HadesMessageWidget` |
| Used by | `W_PrimaryGameLayout` |

**Hierarchy**

```
[W_Message]
└─ Band (Border)
   └─ MessageText (Common Text)
```

**Properties**

- **`Band`** (Border):
  - **Is Variable**
  - Behavior › Visibility: Not Hit-Testable (Self & All Children)
  - Behavior › Render Opacity: `0`
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `000000A6`
  - Brush › Outline Settings › Corner Radii `3, 3, 3, 3`, Color `00000000`, Width `0`, Rounding Type Fixed Radius
  - Padding: Left 28, Top 10, Right 28, Bottom 10
  - Horizontal Alignment Center, Vertical Alignment Center
- **`MessageText`** (Common Text):
  - **Is Variable**
  - Style: `TS_Message`
  - Text: `Message`
  - Justification: Center

**Animation `ShowAnimation`**: one track, `Band` › **Render Opacity**, with keys 0.00 → `0`, 0.15 → `1`, 2.60 → `1`, 3.00 → `0`. The timeline ends at 3.00.

No graph.

**C++** (`UHadesMessageWidget`). Finds `MessageText` (*required*) and the animation `ShowAnimation` (optional). Empties the text on construct, then, for each message the player controller sends, sets the text and plays the animation from the start.

---

## 6. Inventory pieces

All in `Content/UI/Inventory`.

### 6.1 `W_InventoryItem`

An item in the grid. The grid places it over the item's cells, sizes the icon to them, and turns it with the item. It also stands in for the item being moved, on the cells it would land on.

| | |
| --- | --- |
| Parent class | `HadesInventoryItemWidget` |
| Used by | `W_InventoryGrid` (**Item Class**) |

**Hierarchy**

```
[W_InventoryItem]
└─ Overlay
   ├─ Background (Border)
   ├─ SelectedFill (Border)
   ├─ Icon (Image)
   ├─ CountText (Common Text)
   ├─ Scale Box
   │  └─ ShortcutKey (Common Action Widget)
   ├─ EquippedMarker (Image)
   ├─ Highlight (Border)
   └─ AmmoText (Common Text)
```

**Properties**

- **Overlay**:
  - Clipping › Clipping: Clip to Bounds. It cuts the gold square of `EquippedMarker` to a corner.
- **`Background`** (Border):
  - **Is Variable**
  - Slot: Fill/Fill
  - Brush › Draw As: Box
  - Brush › Image: `T_UI_ItemTile`
  - Brush › Image Size: 64 × 64
  - Brush › Margin: `0.25`
  - Brush › Tint: `FFFFFFFF`
  - Padding: `0`
- **`SelectedFill`** (Border):
  - **Is Variable**
  - Slot: Fill/Fill
  - Behavior › Visibility: Hidden
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `E8E2D429`
  - Brush › Outline Settings › Corner Radii `0, 0, 0, 0`, Width `0`, Rounding Type Fixed Radius
  - Padding: `0`
- **`Icon`** (Image):
  - **Is Variable**
  - Slot: H Center, V Center. Not Fill: the grid sizes the icon and turns it.
- **`CountText`** (Common Text):
  - **Is Variable**
  - Slot: H Right, V Bottom; Padding Right 6, Bottom 3
  - Style: `TS_Count`
  - Text: `99`
- **Scale Box**:
  - Slot: H Left, V Top; Padding Left 5, Top 5
  - Stretching › Stretch: User Specified
  - Stretching › User Specified Scale: `0.6`
  - Behavior › Visibility: Not Hit-Testable (Self & All Children)
- **`ShortcutKey`** (Common Action Widget):
  - **Is Variable**
  - Input Actions: empty
- **`EquippedMarker`** (Image):
  - **Is Variable**
  - Slot: H Right, V Top
  - Behavior › Visibility: Collapsed
  - Brush › Image Size: 22 × 22
  - Color and Opacity: `C8A25AFF`
  - Render Transform › Translation: X 11, Y -11
  - Render Transform › Angle: `45`
- **`Highlight`** (Border):
  - **Is Variable**
  - Slot: Fill/Fill
  - Behavior › Visibility: Hidden
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `00000000`
  - Brush › Outline Settings › Corner Radii `0, 0, 0, 0`, Color `C8A25AFF`, Width `2`, Rounding Type Fixed Radius
  - Padding: `0`
- **`AmmoText`** (Common Text):
  - **Is Variable**
  - Slot: H Right, V Bottom; Padding Right 6, Bottom 3
  - Style: `TS_Count`
  - Text: `10`

**Graph**

1. **Event On Held Changed** → **Set Render Opacity** (Target: self; In Opacity: **Select** on **Is Held**: False `1.0`, True `0`). The spot an item leaves looks empty while it's moved.
2. **Event On Hovered Changed** → `Highlight` › **Set Visibility** (**Select** on **Is Hovered**: False → Hidden, True → Not Hit-Testable (Self & All Children)).
3. **Event On Selected Changed** → `SelectedFill` › **Set Visibility** (**Select** on **Is Selected**: False → Hidden, True → Not Hit-Testable (Self & All Children)).

**Class Defaults**

- Drag Visual Class, Tooltip Class: None (the defaults).
- Inventory › Item › **Ammo Colors**: Full `8CC152`, Partly `E8E2D4`, Empty `D9534F` (the defaults).

**C++** (`UHadesInventoryItemWidget`, on the plugin's `UInventoryItemWidget`).

- The plugin finds `Icon` and `CountText` (optional). It sets the icon, hides the count for items that don't stack, and sends the Held, Hovered and Selected events. Items aren't hovered while something is held.
- Hades finds `ShortcutKey`, `EquippedMarker`, `Background` and `AmmoText` (optional). It sets the shortcut's key icon, or clears it; shows `EquippedMarker` while the item is in hand; hides `Background` on the stand-in for a moved item, so the item under it shows through; and shows a gun's rounds in `AmmoText`, colored by Ammo Colors, collapsing it for anything else.

### 6.2 `W_SlotItem`

An item inside a slot: the key items, the item box rows, the HUD's shortcuts, the in-hand box and the shortcut menu. Its icon shrinks to fit while the count keeps its size.

| | |
| --- | --- |
| Parent class | `InventoryItemWidget` |
| Used by | `W_InventorySlot`, `W_ItemBoxEntry`, `W_QuickSlot`, `W_EquippedItem`, `W_ShortcutEntry` (as `ItemWidget`) |

**Hierarchy**

```
[W_SlotItem]
└─ Overlay
   ├─ Scale Box
   │  └─ Icon (Image)
   └─ CountText (Common Text)
```

**Properties**

- **Overlay**: no changes.
- **Scale Box**:
  - Slot: Fill/Fill
  - Stretching › Stretch: Scale To Fit
  - Stretching › Stretch Direction: Down Only
- **`Icon`** (Image):
  - **Is Variable**
  - Slot: H Center, V Center
- **`CountText`** (Common Text):
  - **Is Variable**
  - Slot: H Right, V Bottom; Padding Right 2
  - Style: `TS_Count`
  - Text: `99`

**Graph**

1. **Event On Held Changed** → **Set Render Opacity** (Target: self; **Select** on **Is Held**: False `1.0`, True `0.35`).

**Class Defaults**: no changes.

**C++.** The plugin's item widget, as in 6.1. Outside a grid the icon takes its texture's size, 128 pixels per cell, and the Scale Box shrinks it to fit, never larger.

### 6.3 `W_InventoryCell`

One cell of the grid.

| | |
| --- | --- |
| Parent class | `InventoryCellWidget` |
| Used by | `W_InventoryGrid` (**Cell Class**) |

**Hierarchy**

```
[W_InventoryCell]
└─ Overlay
```

**Properties**

- **Overlay**: no changes, and no children. The grid sizes each cell.

**Class Defaults**

- **Style**: `BS_Slot`
- Cell › **Holding Style**: `BS_SlotHolding`

No graph.

**C++** (plugin). A Common button: Common UI only builds the button around designer content, so the empty Overlay is what makes the cell draw and take clicks. While an item is held, the cell uses Holding Style.

### 6.4 `W_InventoryGrid`

The pockets.

| | |
| --- | --- |
| Parent class | `InventoryGridWidget` |
| Used by | `W_InventoryScreen`, `W_ItemBoxScreen` (`MainGrid`) |

**Hierarchy**

```
[W_InventoryGrid]
└─ GridCanvas (Canvas Panel)
   └─ PlacementPreview (Border)
```

**Properties**

- **`GridCanvas`** (Canvas Panel):
  - **Is Variable**
- **`PlacementPreview`** (Border):
  - **Is Variable**
  - Slot: position and size left as they are; the grid moves it.
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `FFFFFFFF`
  - Brush › Outline Settings › Corner Radii `0, 0, 0, 0`, Width `0`, Rounding Type Fixed Radius
  - Brush Color: `FFFFFF59`
  - Padding: `0`

**Graph**

1. **Event On Placement Preview Changed** → `PlacementPreview` › **Set Brush Color** (In Brush Color: **Select** on **Action**):

   | Action | Color |
   | --- | --- |
   | None | `FFFFFF00` |
   | Place | `C8A25A6B` |
   | Merge | `C8A25A6B` |
   | Swap | `C8A25A6B` |
   | Invalid | `B02A2273` |

**Class Defaults**

- **Container Tag**: `Container.Pockets`
- **Cell Size**: `96`
- **Cell Class**: `W_InventoryCell`
- **Item Class**: `W_InventoryItem`

**C++** (plugin). Finds `GridCanvas` (*required*) and `PlacementPreview` (optional). Shows the pockets of the owning player, a cell per grid cell and an item widget per item. While an item is moved, it draws the item on the cells it would land on, over the preview: gold where it can go, red where it can't. Items glide to new cells at **Preview Glide Speed** (left at its default).

### 6.5 `W_InventorySlot`

One slot of a container. The key item list is made of them.

| | |
| --- | --- |
| Parent class | `InventorySlotWidget` |
| Used by | `W_InventoryList` (**Entry Class**), for the key items |

**Hierarchy**

```
[W_InventorySlot]
└─ Overlay
   ├─ SlotBox (Size Box)
   │  └─ ItemWidget (W_SlotItem)
   └─ PlacementPreview (Border)
```

**Properties**

- **Overlay**: no changes.
- **`SlotBox`** (Size Box):
  - **Is Variable**
  - Slot: H Center, V Center
  - Child Layout: Width 64; Height 64
- **`ItemWidget`** (W_SlotItem):
  - **Is Variable**
  - Slot: Fill/Fill; Padding 6
- **`PlacementPreview`** (Border):
  - **Is Variable**
  - Slot: Fill/Fill
  - Behavior › Visibility: Hidden
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `FFFFFFFF`
  - Brush › Outline Settings › Corner Radii `0, 0, 0, 0`, Width `0`, Rounding Type Fixed Radius
  - Brush Color: `FFFFFF59`
  - Padding: `0`

**Graph**

- Variable **`SlotSize`**: Vector 2D, **Instance Editable**, Default Value X `64`, Y `64`.
1. **Event Pre Construct** → **Set Size Box Size** (Target `SlotBox`, New Size `SlotSize`).
2. **Event On Placement Preview Changed** → `PlacementPreview` › **Set Brush Color** (**Select** on **Action**, the five colors of 6.4) → `PlacementPreview` › **Set Visibility** (**Select** on **Action**: None → Hidden; Place, Merge, Swap, Invalid → Not Hit-Testable (Self & All Children)).

**Class Defaults**

- **Style**: `BS_Slot`
- Container Tag, Slot Tag: empty.

**C++** (plugin). Finds `ItemWidget` and `NameText` (optional). Shows the item in one slot of a container, hiding the item widget, not collapsing it, while the slot is empty.

### 6.6 `W_ItemBoxEntry`

A row in the item box: the item and its name.

| | |
| --- | --- |
| Parent class | `InventorySlotWidget` |
| Used by | `W_ItemBoxScreen`'s `ItemBoxList` (**Entry Class**) |

**Hierarchy**

```
[W_ItemBoxEntry]
└─ Size Box
   └─ Horizontal Box
      ├─ Size Box
      │  └─ ItemWidget (W_SlotItem)
      └─ NameText (Common Text)
```

**Properties**

- **Size Box**: Child Layout: Width 312; Height 56
- **Horizontal Box**: no changes.
- **Size Box** (inner):
  - Slot: Vertical Alignment Center; Padding Left 4
  - Width 96; Height 48
- **`ItemWidget`** (W_SlotItem):
  - **Is Variable**
  - Slot: Fill/Fill; Padding 4
- **`NameText`** (Common Text):
  - **Is Variable**
  - Slot: Size Fill; Vertical Alignment Center; Padding Left 12, Right 8
  - Style: `TS_Body`
  - Text: `Item name`

**Class Defaults**

- **Style**: `BS_Slot`

No graph.

**C++** (plugin). As 6.5. `NameText` shows the item's name, or nothing for an empty row.

### 6.7 `W_InventoryList`

The key items and the item box.

| | |
| --- | --- |
| Parent class | `InventoryListWidget` |
| Used by | `W_InventoryScreen` (`KeyItemList`), `W_ItemBoxScreen` (`KeyItemList`, `ItemBoxList`) |

**Hierarchy**

```
[W_InventoryList]
└─ Scroll Box
   └─ EntryPanel (Wrap Box)
```

**Properties**

- **Scroll Box**: no changes.
- **`EntryPanel`** (Wrap Box):
  - **Is Variable**
  - Content › Inner Slot Padding: X 8, Y 8
  - Explicit Wrap Size: unticked (the default), so it wraps at the width it's given

**Class Defaults**

- **Entry Class**: `W_InventorySlot`
- **Container Tag**: empty; set where the list is placed
- **Show Empty Entry**: unticked

No graph.

**C++** (plugin). Finds `EntryPanel` (*required*) and fills it with an entry per item. An item dropped anywhere else on the list goes to the end. Read-only lists, such as the key items, can't take items.

### 6.8 `W_QuickSlot`

One of the four shortcuts in the HUD: a wide box, so a long gun gets the room it needs.

| | |
| --- | --- |
| Parent class | `HadesQuickSlotWidget` |
| Used by | `W_HUD` (`HudQuickSlot1` to `4`) |

**Hierarchy**

```
[W_QuickSlot]
└─ Overlay
   ├─ InHandFrame (Image)
   ├─ Size Box
   │  └─ ItemWidget (W_SlotItem)
   ├─ Scale Box
   │  └─ KeyIcon (Common Action Widget)
   └─ Horizontal Box
      ├─ AmmoText (Common Text)
      └─ SpareAmmoText (Common Text)
```

**Properties**

- **Overlay**: no changes.
- **`InHandFrame`** (Image):
  - **Is Variable**
  - Slot: Fill/Fill; Padding 0
  - Behavior › Visibility: Hidden
  - Brush › Draw As: Border
  - Brush › Tint: `C8A25AFF`
  - Brush › Image Size: 16 × 16
  - Brush › Margin: `0.125`, two pixels of the 16
  - Color and Opacity: `FFFFFFFF`

  A Border brush fades with its parent; a Rounded Box outline wouldn't.
- **Size Box**:
  - Slot: H Center, V Center
  - Width 144; Height 72
- **`ItemWidget`** (W_SlotItem):
  - **Is Variable**
  - Slot: Fill/Fill; Padding 8
- **Scale Box**:
  - Slot: H Left, V Top; Padding Left 4, Top 4
  - Stretching › Stretch: User Specified
  - Stretching › User Specified Scale: `0.6`
- **`KeyIcon`** (Common Action Widget):
  - **Is Variable**
  - Input Actions: empty
- **Horizontal Box**:
  - Slot: H Right, V Bottom; Padding Right 6, Bottom 3
  - Behavior › Visibility: Not Hit-Testable (Self & All Children)
- **`AmmoText`** (Common Text):
  - **Is Variable**
  - Style: `TS_Count`
  - Text: `10`
- **`SpareAmmoText`** (Common Text):
  - **Is Variable**
  - Style: `TS_Count`
  - Text: `/12`

**Graph**

1. **Event On In Hand Changed** → `InHandFrame` › **Set Visibility** (**Select** on **Is In Hand**: False → Hidden, True → Not Hit-Testable (Self & All Children)).

**Class Defaults**

- **Style**: `BS_QuickSlot`
- Quick Slot › **Ammo Colors**: the defaults (6.1)

**On each placed shortcut**: Quick Slot › **Slot Index**, and Interaction › **Is Focusable** unticked ([8.2](#82-w_hud)).

**C++** (`UHadesQuickSlotWidget`). Finds `ItemWidget`, `KeyIcon`, `NumberText`, `AmmoText` and `SpareAmmoText` (all optional). Shows the slot's item (hidden while empty), its key's icon from the hero's **Slot Keys**, and, for a gun, its rounds in Ammo Colors and the spare rounds as "/12". Sends **On In Hand Changed** when the slot's item goes in or out of hand. Refreshes when the shortcuts change and on every inventory change.

### 6.9 `W_ItemDetails`

The name and description of the item moved, hovered or focused.

| | |
| --- | --- |
| Parent class | `InventoryItemDetailsWidget` |
| Used by | `W_InventoryScreen`, `W_ItemBoxScreen` (`ItemDetails`) |

**Hierarchy**

```
[W_ItemDetails]
└─ Vertical Box
   ├─ NameText (Common Text)
   ├─ Underline (Image)
   └─ DescriptionText (Common Text)
```

**Properties**

- **Vertical Box**: no changes.
- **`NameText`** (Common Text):
  - **Is Variable**
  - Slot: Horizontal Alignment Fill
  - Style: `TS_ItemName`
  - Text: `Item Name`
- **`Underline`** (Image):
  - Slot: Horizontal Alignment Left; Padding Top 10
  - Brush › Image Size: 48 × 2
  - Color and Opacity: `C8A25AFF`
- **`DescriptionText`** (Common Text):
  - **Is Variable**
  - Slot: Horizontal Alignment Fill; Padding Top 14
  - Style: `TS_Body`
  - Text: `Description`
  - Auto Wrap Text: ticked

**Class Defaults**

- **Follow Hovered Item**: ticked
- **Keep Last Item**: unticked
- **Hide Without Item**: ticked

No graph.

**C++** (plugin). Finds `Icon`, `NameText`, `DescriptionText` and `CountText` (optional; this one has no icon, as the grid shows it). Shows the item being moved, or else the one hovered or focused, and hides its contents without one, keeping its size.

### 6.10 `W_EquippedItem`

What's in hand, in the IN HAND panel.

| | |
| --- | --- |
| Parent class | `HadesEquippedItemWidget` |
| Used by | `W_InventoryScreen`, `W_ItemBoxScreen` (`EquippedItem`) |

**Hierarchy**

```
[W_EquippedItem]
└─ Horizontal Box
   ├─ Border
   │  └─ Size Box
   │     └─ Overlay
   │        ├─ ItemWidget (W_SlotItem)
   │        └─ EmptyMarker (Image)
   └─ NameText (Common Text)
```

**Properties**

- **Horizontal Box**: no changes.
- **Border**:
  - Slot: Size Auto; Vertical Alignment Center
  - Brush › Draw As: Box
  - Brush › Image: `T_UI_Cell`
  - Brush › Image Size: 64 × 64
  - Brush › Margin: `0.25`
  - Brush › Tint: `FFFFFFFF`
  - Padding: `0`
- **Size Box**: Width 160; Height 96
- **Overlay**:
  - Clipping › Clipping: Clip to Bounds
- **`ItemWidget`** (W_SlotItem):
  - **Is Variable**
  - Slot: Fill/Fill; Padding 12
- **`EmptyMarker`** (Image):
  - **Is Variable**
  - Slot: H Fill, V Center; Padding Left -14, Right -14
  - Brush › Image Size: 32 × 2
  - Color and Opacity: `3A362FFF`
  - Render Transform › Angle: `-31`, corner to corner of a 160 × 96 box
- **`NameText`** (Common Text):
  - **Is Variable**
  - Slot: Size Fill; Vertical Alignment Center; Padding Left 16
  - Style: `TS_Body`
  - Text: `Item Name`

**Class Defaults**

- Equipped Item › **Empty Text**: empty

No graph.

**C++** (`UHadesEquippedItemWidget`). Finds `ItemWidget`, `NameText` and `EmptyMarker` (optional). Shows the item in hand and its name; with nothing in hand, hides the item, shows `EmptyMarker` and Empty Text. Refreshes when the shortcuts or what's in hand change.

### 6.11 `W_ItemMenu`

Register Shortcut, Use, Equip and the rest, beside the item.

| | |
| --- | --- |
| Parent class | `HadesItemMenu` |
| Used by | `W_InventoryScreen`, `W_ItemBoxScreen` (**Item Menu Class**) |

**Hierarchy**

```
[W_ItemMenu]
└─ Overlay
   ├─ ClickCatcher (Border)
   └─ Canvas Panel
      └─ MenuPanel (Border)
         └─ Vertical Box
            ├─ ShortcutButton (W_MenuButton)
            ├─ UseButton (W_MenuButton)
            ├─ EquipButton (W_MenuButton)
            ├─ UnequipButton (W_MenuButton)
            ├─ CombineButton (W_MenuButton)
            ├─ DiscardButton (W_MenuButton)
            ├─ StoreButton (W_MenuButton)
            └─ TakeButton (W_MenuButton)
```

**Properties**

- **Overlay**: no changes.
- **`ClickCatcher`** (Border):
  - Slot: Fill/Fill
  - Brush Color: `00000000`
  - Behavior › Visibility: Visible
- **Canvas Panel**: Slot: Fill/Fill
- **`MenuPanel`** (Border):
  - **Is Variable**
  - Slot (Canvas Panel Slot): Size To Content ticked; Anchors, Position and Alignment left as they are
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `1A1916FF`
  - Brush › Outline Settings › Corner Radii `2, 2, 2, 2`, Color `3A362FFF`, Width `1`, Rounding Type Fixed Radius
  - Padding: `6`
- **Vertical Box**: no changes.
- **The eight buttons** (W_MenuButton): each **Is Variable**; Slot: Horizontal Alignment Fill; **Label Text**:

  | Name | Label Text |
  | --- | --- |
  | `ShortcutButton` | Register Shortcut |
  | `UseButton` | Use |
  | `EquipButton` | Equip |
  | `UnequipButton` | Unequip |
  | `CombineButton` | Combine |
  | `DiscardButton` | Discard |
  | `StoreButton` | Store |
  | `TakeButton` | Take |

**Class Defaults**

- Input › **Confirm Action**: `DT_UIInputActions`, Row Name `Confirm`
- Back › **Is Back Action Displayed in Action Bar**: ticked
- Item Menu › **Descriptions**: the defaults ([UI 9](UI.md#9-item-menu))
- Set by the C++: Input › **Input Config** Menu; Back › **Is Back Handler** ticked

No graph.

**C++** (`UHadesItemMenu`, on `UHadesPopupMenu`). Finds `MenuPanel` (*required*), the eight buttons and `ItemNameText` (optional). Places the menu beside the item, shows only the buttons whose action can run, focuses the first, shows each button's description in the screen's hint while it's hovered, and on a click closes, then runs the action on the screen. A click outside `MenuPanel` closes it, except on the item.

### 6.12 `W_ShortcutEntry`

One shortcut in the shortcut menu.

| | |
| --- | --- |
| Parent class | `HadesShortcutEntry` |
| Used by | `W_ShortcutMenu` (**Entry Class**) |

**Hierarchy**

```
[W_ShortcutEntry]
└─ Horizontal Box
   ├─ KeyIcon (Common Action Widget)
   ├─ Size Box
   │  └─ ItemWidget (W_SlotItem)
   └─ NameText (Common Text)
```

**Properties**

- **Horizontal Box**: no changes.
- **`KeyIcon`** (Common Action Widget):
  - **Is Variable**
  - Slot: Size Auto; Vertical Alignment Center
- **Size Box**:
  - Slot: Size Auto; Vertical Alignment Center; Padding Left 12, Right 12
  - Width 128; Height 48
- **`ItemWidget`** (W_SlotItem):
  - **Is Variable**
  - Slot: Fill/Fill
- **`NameText`** (Common Text):
  - **Is Variable**
  - Slot: Size Fill; Vertical Alignment Center
  - Style: `TS_Button`
  - Text: `Item Name`

**Class Defaults**

- **Style**: `BS_MenuButton`
- Shortcut › **Empty Text**: `Empty`

No graph.

**C++** (`UHadesShortcutEntry`). Finds `KeyIcon`, `ItemWidget` and `NameText` (optional). Shows the shortcut's key, its item (hidden while empty), and the item's name or Empty Text.

### 6.13 `W_ShortcutMenu`

The list Register Shortcut opens.

| | |
| --- | --- |
| Parent class | `HadesShortcutMenu` |
| Used by | `W_InventoryScreen`, `W_ItemBoxScreen` (**Shortcut Menu Class**) |

**Hierarchy**

```
[W_ShortcutMenu]
└─ Overlay
   ├─ ClickCatcher (Border)
   └─ Canvas Panel
      └─ MenuPanel (Border)
         └─ Vertical Box
            ├─ Common Text "SHORTCUT"
            ├─ Image
            └─ EntryPanel (Vertical Box)
```

**Properties**

- **Overlay**: no changes.
- **`ClickCatcher`** (Border): as in 6.11.
- **Canvas Panel**: Slot: Fill/Fill
- **`MenuPanel`** (Border): as in 6.11.
- **Vertical Box**: no changes.
- **Common Text**:
  - Slot: Padding Left 10, Top 6, Right 10, Bottom 8
  - Style: `TS_Label`
  - Text: `SHORTCUT`
- **Image**:
  - Slot: Horizontal Alignment Fill; Padding Bottom 4
  - Brush › Image Size: 32 × 1
  - Color and Opacity: `3A362FFF`
- **`EntryPanel`** (Vertical Box):
  - **Is Variable**
  - No children; the C++ fills it.

**Class Defaults**

- Shortcut Menu › **Entry Class**: `W_ShortcutEntry`
- Input › **Confirm Action**: `DT_UIInputActions`, Row Name `Confirm`
- Back › **Is Back Action Displayed in Action Bar**: ticked

No graph.

**C++** (`UHadesShortcutMenu`, on `UHadesPopupMenu`). Finds `MenuPanel` and `EntryPanel` (*required*). Each time it opens, it fills `EntryPanel` with one entry per quick slot, focuses the one the item is on, and puts the item on the picked shortcut, or takes it off if it was already there.

---

## 7. Screens

### 7.1 `W_InventoryScreen`

The inventory, with tabs.

| | |
| --- | --- |
| Parent class | `HadesInventoryScreen` |
| Folder | `Content/UI/Inventory` |
| Used by | `W_HUD` (**Inventory Screen Class**) |

**Hierarchy**

```
[W_InventoryScreen]
└─ Overlay
   ├─ Background Blur
   ├─ Dim (Border)
   └─ Safe Zone
      └─ Scale Box
         └─ Frame (Size Box)
            └─ Vertical Box
               ├─ TabRow (Overlay)
               │  ├─ PreviousTabKey (Common Action Widget)
               │  ├─ TabBar (Horizontal Box)
               │  │  ├─ MapTab (W_TabButton)
               │  │  ├─ ItemsTab (W_TabButton)
               │  │  ├─ CraftingTab (W_TabButton)
               │  │  └─ FilesTab (W_TabButton)
               │  └─ NextTabKey (Common Action Widget)
               ├─ HeaderRule (Image)
               ├─ Pages (Widget Switcher)
               │  ├─ MapPage (Common Text)
               │  ├─ ItemsPage (Horizontal Box)
               │  │  ├─ LeftColumn (Size Box)
               │  │  │  └─ Vertical Box
               │  │  │     ├─ ItemsPanel (W_Panel)
               │  │  │     │  └─ Content
               │  │  │     │     └─ Size Box
               │  │  │     │        └─ MainGrid (W_InventoryGrid)
               │  │  │     └─ DetailsPanel (W_Panel)
               │  │  │        └─ Content
               │  │  │           └─ Size Box
               │  │  │              └─ ItemDetails (W_ItemDetails)
               │  │  └─ RightColumn (Size Box)
               │  │     └─ Vertical Box
               │  │        ├─ InHandPanel (W_Panel)
               │  │        │  └─ Content
               │  │        │     └─ EquippedItem (W_EquippedItem)
               │  │        └─ KeyItemsPanel (W_Panel)
               │  │           └─ Content
               │  │              └─ KeyItemList (W_InventoryList)
               │  ├─ CraftingPage (Common Text)
               │  └─ FilesPage (Common Text)
               ├─ ActionHint (Border)
               │  └─ ActionHintText (Common Text)
               ├─ FooterRule (Image)
               └─ ActionBar (Common Bound Action Bar)
```

**Properties: the background and the frame**

- **Overlay**: no changes.
- **Background Blur**:
  - Slot: Fill/Fill
  - Blur Strength: `2`
- **`Dim`** (Border):
  - Slot: Fill/Fill
  - Brush Color: `00000080`
  - Padding: `0`
- **Safe Zone**: Slot: Fill/Fill
- **Scale Box**:
  - Slot: Fill/Fill
  - Stretch: Scale To Fit
  - Stretch Direction: Down Only

  The screen is laid out for 1920 × 1080; on a narrower screen this shrinks it to fit.
- **`Frame`** (Size Box): Width 1448; Height 1000
- **Vertical Box**: no changes.

**Properties: the tab row**

- **`TabRow`** (Overlay): Slot: Horizontal Alignment Fill
- **`PreviousTabKey`** (Common Action Widget):
  - Slot: H Left, V Center
  - Input Actions: one element, Data Table `DT_UIInputActions`, Row Name `PreviousTab`
- **`TabBar`** (Horizontal Box):
  - **Is Variable**
  - Slot: H Center, V Top
- **`MapTab`**, **`ItemsTab`**, **`CraftingTab`**, **`FilesTab`** (W_TabButton): Tab › **Label Text** `MAP`, `ITEMS`, `CRAFTING`, `FILES`; Tab › **Icon Texture** your icon for each, or empty. Their order matches `Pages`.
- **`NextTabKey`** (Common Action Widget):
  - Slot: H Right, V Center
  - Input Actions: one element, Data Table `DT_UIInputActions`, Row Name `NextTab`
- **`HeaderRule`** (Image):
  - Slot: Horizontal Alignment Fill; Padding Top 8
  - Brush › Image Size: 32 × 1
  - Color and Opacity: `3A362FFF`

**Properties: the pages**

- **`Pages`** (Widget Switcher):
  - **Is Variable**
  - Slot: Size Fill; Horizontal Alignment Fill, Vertical Alignment Fill; Padding Top 24
  - Active Widget Index: `1`, so the designer shows the Items page
- **`MapPage`** (Common Text):
  - Slot: H Center, V Center
  - Style: `TS_Label`
  - Text: `THE MAP ISN'T BUILT YET`
- **`ItemsPage`** (Horizontal Box): Slot: H Fill, V Fill. The second page, matching **Items Page** `1`.
- **`LeftColumn`** (Size Box):
  - Slot: Size Auto; Vertical Alignment Top; Padding Left 80
  - Width 848
- **Vertical Box** (in `LeftColumn`): no changes.
- **`ItemsPanel`** (W_Panel):
  - Slot: Size Auto
  - Title: `ITEMS`
- **Size Box** (in `ItemsPanel`):
  - Min Desired Width: ticked, `768`
  - Min Desired Height: ticked, `384`

  The biggest pocket grid, 8 × 4 cells of 96, so the panel doesn't change size when a pouch grows the pockets.
- **`MainGrid`** (W_InventoryGrid):
  - **Is Variable**
  - Slot: H Center, V Center
- **`DetailsPanel`** (W_Panel):
  - Slot: Size Auto; Padding Top 24
  - Title: empty
- **Size Box** (in `DetailsPanel`): Height 120, so the panel keeps its height whatever the description
- **`ItemDetails`** (W_ItemDetails): no changes.
- **`RightColumn`** (Size Box):
  - Slot: Size Auto; Vertical Alignment Top; Padding Left 40
  - Width 400; Height 646, the height of the left column
- **Vertical Box** (in `RightColumn`): no changes.
- **`InHandPanel`** (W_Panel):
  - Slot: Size Auto
  - Title: `IN HAND`
- **`EquippedItem`** (W_EquippedItem): no changes.
- **`KeyItemsPanel`** (W_Panel):
  - Slot: Size Fill; Padding Top 24
  - Title: `KEY ITEMS`
- **`KeyItemList`** (W_InventoryList):
  - Container Tag: `Container.KeyItems`
  - Entry Class: `W_InventorySlot`
- **`CraftingPage`** (Common Text): as `MapPage`, with Text `CRAFTING ISN'T BUILT YET`
- **`FilesPage`** (Common Text): as `MapPage`, with Text `FILES AREN'T BUILT YET`

**Properties: the footer**

- **`ActionHint`** (Border):
  - **Is Variable**
  - Slot: Horizontal Alignment Right; Padding Bottom 8
  - Behavior › Visibility: Hidden
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `121110FF`
  - Brush › Outline Settings › Corner Radii `2, 2, 2, 2`, Color `2F2C28FF`, Width `1`, Rounding Type Fixed Radius
  - Padding: Left 16, Top 10, Right 16, Bottom 10
- **`ActionHintText`** (Common Text):
  - **Is Variable**
  - Style: `TS_Body`
  - Text: `What the option does.`
- **`FooterRule`** (Image):
  - Slot: Horizontal Alignment Fill
  - Brush › Image Size: 32 × 1
  - Color and Opacity: `3A362FFF`
- **`ActionBar`** (Common Bound Action Bar):
  - Slot: Horizontal Alignment Right; Padding Top 12
  - Action Button Class: `W_BoundActionButton`
  - Entry Spacing: X 16, Y 0
  - Entry Size Rule: Auto
  - Entry Vertical Alignment: Center

**Class Defaults**

- Inventory › **Item Menu Class**: `W_ItemMenu`
- Inventory › **Shortcut Menu Class**: `W_ShortcutMenu`
- Tabs › **Items Page**: `1`
- Hints: **Move Hint** and **Combine Hint** left as they are; **Idle Hint** empty
- Input, each a Data Table `DT_UIInputActions` and a Row Name:

  | Property | Row Name |
  | --- | --- |
  | Confirm Action | `Confirm` |
  | Move Action | `Move` |
  | Organize Action | `Organize` |
  | Rotate Action | `Rotate` |
  | Previous Tab Action | `PreviousTab` |
  | Next Tab Action | `NextTab` |
  | Split Action | empty |

- Back › **Is Back Action Displayed in Action Bar**: ticked
- Activation › **Auto Restore Focus**: ticked
- Set by the C++: Input › **Input Config** Menu; Pause › **Pause Game** ticked; Back › **Is Back Handler** ticked

No graph. Compiling notes that **Get Desired Focus Target** isn't implemented; the C++ focuses the first pocket cell.

**C++** (`UHadesInventoryScreen`). Finds `MainGrid`, `ItemBoxList`, `ActionHint`, `ActionHintText`, `TabBar` and `Pages` (all optional). See [UI 8](UI.md#8-inventory-screen).

### 7.2 `W_ItemBoxScreen`

The same screen with the item box on the left, and no tabs. It was made by duplicating `W_InventoryScreen`.

| | |
| --- | --- |
| Parent class | `HadesInventoryScreen` |
| Folder | `Content/UI/Inventory` |
| Used by | `BP_ItemBox` (**Screen Class**) |

**Hierarchy**

```
[W_ItemBoxScreen]
└─ Overlay
   ├─ Background Blur
   ├─ Dim (Border)
   └─ Safe Zone
      └─ Scale Box
         └─ Frame (Size Box)
            └─ Vertical Box
               ├─ TabRow (Overlay)
               │  └─ Common Text "ITEM BOX"
               ├─ HeaderRule (Image)
               ├─ ItemsPage (Horizontal Box)
               │  ├─ BoxColumn (Size Box)
               │  │  └─ ItemBoxPanel (W_Panel)
               │  │     └─ Content
               │  │        └─ ItemBoxList (W_InventoryList)
               │  ├─ LeftColumn (Size Box)
               │  │  └─ Vertical Box
               │  │     ├─ ItemsPanel (W_Panel)
               │  │     │  └─ Content
               │  │     │     └─ Size Box
               │  │     │        └─ MainGrid (W_InventoryGrid)
               │  │     └─ DetailsPanel (W_Panel)
               │  │        └─ Content
               │  │           └─ Size Box
               │  │              └─ ItemDetails (W_ItemDetails)
               │  └─ RightColumn (Size Box)
               │     └─ Vertical Box
               │        ├─ InHandPanel (W_Panel)
               │        │  └─ Content
               │        │     └─ EquippedItem (W_EquippedItem)
               │        └─ KeyItemsPanel (W_Panel)
               │           └─ Content
               │              └─ KeyItemList (W_InventoryList)
               ├─ ActionHint (Border)
               │  └─ ActionHintText (Common Text)
               ├─ FooterRule (Image)
               └─ ActionBar (Common Bound Action Bar)
```

**Properties.** As `W_InventoryScreen`, except:

- There is no `PreviousTabKey`, `TabBar`, `NextTabKey` or `Pages`, and no Map, Crafting or Files page.
- **`Frame`** (Size Box): Width 1840
- **Common Text** (in `TabRow`):
  - Slot: H Left, V Center
  - Style: `TS_Title`
  - Text: `ITEM BOX`
- **`ItemsPage`** (Horizontal Box): now in the Frame's Vertical Box, just below `HeaderRule`. Slot: Size Fill; Padding Top 24
- **`BoxColumn`** (Size Box), the first child of `ItemsPage`:
  - Slot: Size Auto; Vertical Alignment Top; Padding Left 80
  - Width 360; Height 646
- **`ItemBoxPanel`** (W_Panel): Title `ITEM BOX`
- **`ItemBoxList`** (W_InventoryList):
  - **Is Variable**
  - Container Tag: empty
  - Entry Class: `W_ItemBoxEntry`
- **`LeftColumn`** (Size Box): Slot Padding Left 32

**Class Defaults.** As `W_InventoryScreen`. The tab settings do nothing here, as there's no `Pages`.

No graph.

**C++.** Having `ItemBoxList` makes it the item box screen: the list shows the player controller's item box storage, and the item menu offers Store and Take ([UI 8.6](UI.md#86-item-box-mode)).

### 7.3 `W_EscapeMenu`

The pause menu.

| | |
| --- | --- |
| Parent class | `HadesActivatableWidget` |
| Folder | `Content/UI/Foundation` |
| Used by | `W_HUD` (**Escape Menu Class**) |

**Hierarchy**

```
[W_EscapeMenu]
└─ Overlay
   ├─ Background Blur
   ├─ Dim (Border)
   └─ Vertical Box
      ├─ Common Text "PAUSED"
      ├─ Image
      └─ ResumeButton (W_MenuButton)
```

**Properties**

- **Overlay**: no changes.
- **Background Blur**:
  - Slot: Fill/Fill
  - Blur Strength: `6`
- **`Dim`** (Border):
  - Slot: Fill/Fill
  - Brush Color: `000000C7`
- **Vertical Box**: Slot: H Center, V Center
- **Common Text**:
  - Slot: Horizontal Alignment Center
  - Style: `TS_Title`
  - Text: `PAUSED`
- **Image**:
  - Slot: Horizontal Alignment Fill; Padding Top 16, Bottom 24
  - Brush › Image Size: 32 × 1
  - Color and Opacity: `3A362FFF`
- **`ResumeButton`** (W_MenuButton):
  - **Is Variable**
  - Slot: Horizontal Alignment Center
  - Label Text: `Resume`

**Graph**

1. `ResumeButton`'s **On Button Base Clicked** (listed as On Clicked in some engine versions) → **Deactivate Widget**.
2. Function override **Get Desired Focus Target** → Return Value: `ResumeButton`.

**Class Defaults**

- Input › **Input Config**: Menu
- Pause › **Pause Game**: ticked
- Back › **Is Back Handler**: ticked

**C++** (`UHadesActivatableWidget`). Sets the input mode while active and adds a pause request ([UI 3](UI.md#3-screens)).

---

## 8. HUD

In `Content/UI/HUD`.

### 8.1 `W_InteractableMarker`

The dot over things nearby, and the prompt with the key over the one looked at.

| | |
| --- | --- |
| Parent class | `HadesInteractableMarkerWidget` |
| Used by | **Marker Widget Class** on the interactables of `BP_ItemPickup`, `BP_ItemBox` and `BP_LockedDoor` |

**Hierarchy**

```
[W_InteractableMarker]
└─ Overlay
   ├─ Indicator (Image)
   └─ Prompt (Border)
      └─ Horizontal Box
         ├─ InteractKey (Common Action Widget)
         └─ ActionText (Common Text)
```

**Properties**

- **Overlay**: no changes.
- **`Indicator`** (Image):
  - **Is Variable**
  - Slot: H Center, V Center
  - Brush › Image Size: 10 × 10
  - Brush › Draw As: Rounded Box
  - Brush › Outline Settings › Color `000000B3`, Width `1`, Rounding Type **Half Height Radius** (a circle), **Use Brush Transparency** ticked
  - Color and Opacity: `E8E2D4E6`
- **`Prompt`** (Border):
  - **Is Variable**
  - Slot: H Center, V Center
  - Brush › Draw As: Rounded Box
  - Brush › Tint: `0A0A0AC7`
  - Brush › Outline Settings › Color `2F2C28FF`, Width `1`, Rounding Type **Half Height Radius** (a pill), **Use Brush Transparency** ticked
  - Padding: Left 10, Top 5, Right 16, Bottom 5
- **Horizontal Box**: no changes.
- **`InteractKey`** (Common Action Widget):
  - Slot: Vertical Alignment Center
  - Input Actions: one element, Data Table `DT_UIInputActions`, Row Name `Interact`
- **`ActionText`** (Common Text):
  - **Is Variable**
  - Slot: Vertical Alignment Center; Padding Left 10
  - Style: `TS_Button`
  - Text: `Take Shotgun`

**Class Defaults**

- World Marker › Screen › **Anchor Alignment**: X 0.5, Y 0.5 (the default)

No graph.

**C++** (`UHadesInteractableMarkerWidget`). Finds `Indicator` and `Prompt` (*required*), and `ActionText` and `ProgressRing` (optional) and its three animations (optional; this one has none). Shows `Indicator` while nearby and `Prompt` while focused, and fills `ActionText` with the interaction text, such as "Take Shotgun", or the disabled text, such as "Not enough space" ([Interaction 5](Interaction.md#5-interactable-marker-widget)).

### 8.2 `W_HUD`

The HUD: the world markers and the four shortcuts.

| | |
| --- | --- |
| Parent class | `HadesHUDLayout` |
| Used by | `BP_HadesPlayerController` (**HUD Layout Class**) |

**Hierarchy**

```
[W_HUD]
└─ Overlay
   ├─ Hades World Marker Layer
   └─ QuickSlotGroup (Scale Box)
      └─ Grid Panel
         ├─ HudQuickSlot1 (W_QuickSlot)
         ├─ HudQuickSlot2 (W_QuickSlot)
         ├─ HudQuickSlot3 (W_QuickSlot)
         └─ HudQuickSlot4 (W_QuickSlot)
```

**Properties**

- **Overlay**: no changes.
- **Hades World Marker Layer**: Slot: Fill/Fill
- **`QuickSlotGroup`** (Scale Box):
  - **Is Variable**
  - Slot: H Left, V Bottom; Padding Left 56, Bottom 56
  - Stretch: User Specified
  - User Specified Scale: `1`. Change it to make every shortcut bigger or smaller.
  - Behavior › Visibility: Not Hit-Testable (Self & All Children)
  - Behavior › Render Opacity: `0`. The animation shows it.
- **Grid Panel**: no changes.
- **The four shortcuts** (W_QuickSlot): each Slot (Grid Slot) **Padding** 4, Interaction › **Is Focusable** unticked, and:

  | Name | Slot Index | Row | Column | D-pad |
  | --- | --- | --- | --- | --- |
  | `HudQuickSlot1` | 0 | 0 | 1 | Up |
  | `HudQuickSlot2` | 1 | 1 | 2 | Right |
  | `HudQuickSlot3` | 2 | 2 | 1 | Down |
  | `HudQuickSlot4` | 3 | 1 | 0 | Left |

  A cross with the center empty, laid out like the D-pad.

**Animation `ShowQuickSlotsAnimation`**: one track, `QuickSlotGroup` › **Render Opacity**, with keys 0.00 → `1`, 2.20 → `1`, 2.60 → `0`. The timeline ends at 2.60.

**Class Defaults**

- HUD › **Escape Menu Class**: `W_EscapeMenu`
- HUD › **Inventory Screen Class**: `W_InventoryScreen`
- Input › **Input Config**: Game and Menu (the default)

No graph. There's no message widget here: messages come from the root layout.

**C++** (`UHadesHUDLayout`). Finds the animation `ShowQuickSlotsAnimation` (optional) and plays it from the start whenever a shortcut key is pressed. Opens Escape Menu Class on `UI.Action.Escape` and Inventory Screen Class on `UI.Action.Inventory` ([UI 5](UI.md#5-hud)).

---

## 9. Where the widgets are used

The settings outside the UI that point at UI assets:

| Asset | Where | Field | Value |
| --- | --- | --- | --- |
| `BP_HadesPlayerController` | Class Defaults | UI › HUD Layout Class | `W_HUD` |
| `BP_HadesHero` | Quick Slot Component | Quick Slots › Slot Keys | `DT_UIInputActions`: `Shortcut1`, `Shortcut2`, `Shortcut3`, `Shortcut4` |
| `BP_ItemPickup` | Interactable | Interaction › Marker › Marker Widget Class | `W_InteractableMarker` |
| `BP_ItemBox` | Interactable | Interaction › Marker › Marker Widget Class | `W_InteractableMarker` |
| `BP_ItemBox` | Class Defaults | Item Box › Screen Class | `W_ItemBoxScreen` |
| `BP_LockedDoor` | Hades Interactable Component | Interaction › Marker › Marker Widget Class | `W_InteractableMarker` |
| `CL_KeyItems` | | Rules › Read Only In UI | ticked |
| `B_HadesUIPolicy` | Class Defaults | Layout Class | `W_PrimaryGameLayout` |
| Project Settings | Game › Common Input Settings | Input Data | `B_CommonInputData` |
| Project Settings | Game › Common Input Settings › Platform Input › Windows | Controller Data | `CID_KeyboardMouse`, `CID_Gamepad` |
| `Config/DefaultGame.ini` | `[/Script/Hades.HadesUIManagerSubsystem]` | DefaultUIPolicyClass | `/Game/UI/Foundation/B_HadesUIPolicy.B_HadesUIPolicy_C` |

And which widget holds which:

| Widget | Holds |
| --- | --- |
| `W_PrimaryGameLayout` | `W_Message` |
| `W_HUD` | `W_QuickSlot` × 4 |
| `W_InventoryScreen` | `W_TabButton` × 4, `W_Panel` × 4, `W_InventoryGrid`, `W_ItemDetails`, `W_EquippedItem`, `W_InventoryList`; `W_BoundActionButton` through the action bar; `W_ItemMenu` and `W_ShortcutMenu` through its Class Defaults |
| `W_ItemBoxScreen` | As `W_InventoryScreen` without the tabs, plus a fifth `W_Panel` and a second `W_InventoryList` |
| `W_InventoryGrid` | `W_InventoryCell` and `W_InventoryItem` through its Class Defaults |
| `W_InventoryList` | `W_InventorySlot`, or `W_ItemBoxEntry` in the item box, through Entry Class |
| `W_InventorySlot`, `W_ItemBoxEntry`, `W_QuickSlot`, `W_EquippedItem`, `W_ShortcutEntry` | `W_SlotItem` |
| `W_ItemMenu`, `W_EscapeMenu` | `W_MenuButton` |
| `W_ShortcutMenu` | `W_ShortcutEntry` through Entry Class |

---

## Appendix. Colors and sizes

**Palette.** Every color in the UI comes from this list.

| Name | Hex sRGB, or texture | Where |
| --- | --- | --- |
| Backdrop | `00000080` | The dim over the grey world behind the inventory |
| Pause backdrop | `000000C7` | The dim behind the pause menu |
| Panel | `T_UI_Panel` | Panel fill, outline and corner marks |
| Panel line | `2F2C28FF` | The line under panel titles, the hint's outline, the prompt's outline |
| Rule | `3A362FFF` | The lines under the tabs and above the action bar; the menu outlines; the slash across an empty box |
| Well | `T_UI_Cell` | Empty cells and slots, the in-hand box |
| Hover fill | `1C1A16FF` | A hovered or focused cell |
| Item | `T_UI_ItemTile` | The box under an item |
| Shortcut | `0A0A0AB3` | The boxes of the HUD's shortcuts |
| Menu | `1A1916FF` | The item and shortcut menus |
| Hint | `121110FF` | The hint above the action bar |
| Selected fill | `E8E2D429` | An item whose menu is open |
| Accent | `C8A25AFF` | Hover outlines, the in-hand corner and frame, the underline |
| Accent bright | `E2C27EFF` | Pressed outlines |
| Moving | `C8A25A6B` | Under an item being moved, where it can go |
| Invalid | `B02A2273` | Under an item being moved, where it can't go |
| Ammo full | `8CC152FF` | A full gun's rounds |
| Ammo empty | `D9534FFF` | An empty gun's rounds |
| Text bright | `E8E2D4FF` | Titles, names, counts, buttons, messages, tab icons; a partly loaded gun's rounds |
| Text | `BDB6A8FF` | Descriptions, action names, the open tab's frame |
| Text muted | `8A857BFF` | Small labels, tab labels |

**Sizes**, at 1920 × 1080:

| What | Size |
| --- | --- |
| Inventory frame | 1448 × 1000 (item box screen: 1840 × 1000) |
| Tab | 120 × 92, icon 48 |
| Columns | Left 848, right 400, item box 360; 40 between left and right, 32 before left on the item box screen; 80 from the frame's edge |
| Grid cells | 96 |
| Item panel | Holds up to 8 × 4 cells: 768 × 384 |
| Details panel | 120 tall |
| In-hand box | 160 × 96 |
| Key item slots | 64, with 8 between |
| Item box rows | 312 × 56, with 8 between |
| Shortcut menu items | 128 × 48 |
| Menu buttons | At least 208 wide |
| Key icons | 40 tall; 60% in badges |
| Panel padding | 20 |
| Message | 112 above the bottom of the screen |
| HUD shortcuts | 144 × 72, 4 around each, 56 from the bottom left corner; shown for 2.6 seconds after a shortcut key |
