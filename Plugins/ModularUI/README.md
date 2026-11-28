﻿﻿﻿﻿﻿# Modular UI

Common UI building blocks for any Unreal Engine 5.7 project: a layered root layout and the subsystem that pushes widgets onto its layers, screens that set the input mode and come in and go with transitions, context menus with prompts, prompt dialogs, buttons, tabs and key glyphs, a message system for toasts, banners, headings and prompts, world markers, keyboard navigation settings, and pause requests that fade a look over the world while a menu is open. It doesn't depend on any game, and other plugins, such as Modular Inventory, build on it.

## Contents

1. [Overview](#1-overview)
1. [Installation](#2-installation)
2. [Layers and the root layout](#3-layers-and-the-root-layout)
3. [Screens](#4-screens)
4. [Transitions, reveals and animations](#5-transitions-reveals-and-animations)
5. [Context menus](#6-context-menus)
6. [Prompts](#7-prompts)
7. [Buttons, tabs, keys and meters](#8-buttons-tabs-keys-and-meters)
8. [Messages](#9-messages)
9. [World markers](#10-world-markers)
10. [Keyboard navigation](#11-keyboard-navigation)
11. [Pausing](#12-pausing)
12. [Example: a layered opening](#13-example-a-layered-opening)
13. [C++ reference](#14-c-reference)
14. [Design notes](#15-design-notes)

---

## 1. Overview

| Piece | What it does |
| --- | --- |
| **Modular UI Subsystem** and **Modular Root Layout** | Each local player's layers, such as Game for the HUD, Game Menu, Menu and Modal, found by gameplay tag. Push a widget onto a layer by its tag from anywhere. Use Modular UI's own root layout, or hand it the one you have, such as CommonGame's. |
| **Modular Activatable Widget** | The base for screens, menus and popups: sets the input mode while it's active, comes in and goes with transitions and widget animations, closes with Back or a key of its own, can pause the game, and has a description line saying what the player can do now, such as what the highlighted option does. |
| **Transitions** and **reveals** | A fade, a slide and a growth, each optional, played on named parts one after another, in real time. |
| **Modular Context Menu Widget** | A small menu opened beside something, placed to stay on screen, dismissed as you choose, with one entry highlighted and prompts asked inline, over the menu or in a dialog. |
| **Modular Prompt Widget** | A confirmation or how-many dialog, and the **Show Modular Prompt** node that waits for its answer. |
| **Modular Button**, **Tab Button**, **Tab List**, **Page Switcher** and **Input Key Widget** | Common UI buttons with a label, an icon and a description, tabs that remember where focus was on each page and can each have a key, pages that come in with a quick transition of their own, and the key a player has mapped to an input action. |
| **Modular Bound Action Bar** and **Modular Bound Action Button** | The actions the player can press now, as Common UI's bound action bar shows them, without rebuilding every button each time a binding changes, so it doesn't flicker as menus open. Its buttons can take clicks or only show each key, and a Modular Bound Action Button shows which actions are holds, and how far along a hold is. |
| **Modular Segmented Meter** | A meter of separate segments, such as health, a battery or stamina, that leaves a trail behind what was just lost. |
| **Input actions** | Every key a Modular UI widget binds is an Enhanced Input action players can rebind, with its keys set in one place: an Input Mapping Context. A Hold trigger makes one a hold, its progress shown on its action bar button. `ModularUI.Input.Status` lists what takes input now, with its keys. |
| **Modular Key Binding List** | A keybinding menu: the actions players can rebind, under their categories, for keyboard and mouse or for gamepad, each key changed by pressing it and then the new one. |
| **Modular Message Subsystem** and **Message Feeds** | Messages sent to channels, shown by whichever feeds show those channels: toasts, banners, headings, hints, prompts. |
| **World markers** | Widgets that follow things in the world, such as interaction prompts and objective markers, kept on screen or faded at the edge. |
| **Navigation settings** | Whether Tab, the arrow keys and Space move focus and press buttons, and extra keys that go Back, such as the right mouse button. |
| **Pause subsystem** | Pauses the world while anything asks, and fades a look in over it, such as greying it out. |

Everything that animates runs from the core ticker in real time, so it plays while the game is paused. Timers, timelines and Delay nodes all wait for the world, so they can't animate anything a pause menu shows.

## 2. Installation

1. Copy the `ModularUI` folder into your project's `Plugins` folder, so you have `YourProject/Plugins/ModularUI/ModularUI.uplugin`.
1. If your C++ uses it, add `"ModularUI"` to your module's `PublicDependencyModuleNames`.
2. Close the editor, regenerate project files and build the **Development Editor** configuration from your IDE.

It needs **Common UI** and **Enhanced Input**, which it turns on for you. Its settings are in **Project Settings › Plugins › Modular UI**.

Its gameplay tags are declared in C++, so they're there without any setup:

| Tag | Use |
| --- | --- |
| `UI.Layer.Game`, `UI.Layer.GameMenu`, `UI.Layer.Menu`, `UI.Layer.Modal` | Layers, bottom to top: the HUD, screens over the game such as an inventory, menus such as pause, and dialogs. |
| `UI.Message.Toast`, `UI.Message.Banner`, `UI.Message.Heading`, `UI.Message.Hint`, `UI.Message.Prompt` | Message channels ([9](#9-messages)). Add your own under `UI.Message`. |

## 3. Layers and the root layout

A **layer** is a Common Activatable Widget Stack or Queue in the player's root layout, named by a gameplay tag. The **Modular UI Subsystem**, one per local player, finds a layer by its tag and pushes widgets onto it, so a screen, a menu or a dialog can be opened from anywhere without knowing the layout.

| Function | Description |
| --- | --- |
| Push Widget To Layer (Layer Tag, Widget Class) | Pushes a widget onto the layer and returns it. In C++, `PushWidgetToLayer<T>` takes a function that sets the widget up before it shows. |
| Push Streamed Widget To Layer (Layer Tag, Widget Class) | Loads the class first, without a hitch, then pushes it. |
| Get Layer (Layer Tag) | The layer's stack or queue. |
| Register Layer / Unregister Layer (Layer Tag, Layer) | Adds a stack or queue of your own as a layer, such as one inside a screen. |
| Remove Widget From Layers (Widget) | Takes a widget off whichever layer has it. |
| Get Root Layout | The Modular Root Layout the subsystem made, if it made one. |

The **Modular UI Function Library** has the same pushes by player controller, for Blueprints that have one at hand.

**A root layout of Modular UI's own.** Make a Widget Blueprint with parent **Modular Root Layout**. Lay out a Common Activatable Widget Stack for each layer, one above the other, filling the screen, and name each in **Layers** with its tag. Set it as **Root Layout Class** in the project settings, and the subsystem makes one for each local player and keeps it on their screen, at **Root Layout Z Order**. With **Hide Root Layout With HUD**, it hides while the player's HUD is hidden, such as with the `ShowHUD` console command. Lay anything that should show over every layer in it too, such as message feeds.

**A root layout you have already**, such as CommonGame's Primary Game Layout: leave Root Layout Class empty, and bind the subsystem's **On Find Layer** in C++ to find your layout's layer by tag. Everything that opens on Modular UI layers then opens on yours:

```cpp
UModularUISubsystem::OnFindLayer().BindStatic([](const ULocalPlayer* LocalPlayer, FGameplayTag LayerTag) -> UCommonActivatableWidgetContainerBase*
{
 // Find the player's root layout and return its layer with this tag.
 return FindMyRootLayout(LocalPlayer) ? FindMyRootLayout(LocalPlayer)->GetLayerWidget(LayerTag) : nullptr;
});
```

Give your stacks a **Transition Duration** of 0 when the widgets on them have transitions of their own, so the two don't play over each other.

## 4. Screens

Make your screens, menus and popups Widget Blueprints whose parent is **Modular Activatable Widget**, or a C++ class based on it.

| Setting | Group | Description |
| --- | --- | --- |
| Input Config | Input | Whether input reaches the game, the UI or both while it's active: **Default** leaves it to what's under it, **Game And Menu** gives both, such as for a HUD, **Game** only the game, **Menu** only the UI, with the cursor. Common UI ignores input actions while the input mode is Game, so a widget with keys of its own, such as a HUD whose key opens an inventory, needs Game And Menu or Menu. |
| Game Mouse Capture Mode | Input | How the mouse behaves while the game gets input. |
| Confirm Display Name | Input | What Accept does here, such as Select, shown in an action bar beside Accept's key. Accept stays Common UI's, so its key is set once, in your Common UI Input Data, and Accept itself still goes to the focused button. Empty shows nothing. |
| Close Input Actions | Input | Input actions that close it as Back does, such as the key that opened it, so the same key toggles it: I for an inventory, M for a map. |
| Show Close Input Actions | Input | Close Input Actions show in action bars too, each by its Description beside its key, such as Resume beside the gamepad's Start on a pause menu. Off, the default, they work without showing, as for the key that opened the screen. Common UI's own **Display in Action Bar**, under Input too, shows every action the widget binds when ticked, whatever this says, so leave it off. |
| Pause Game | Pause | The world stops while it's open. It starts again as the close begins, so the world comes back while it goes. |
| Open Reveal, Close Reverses Open, Close Transition | Transition | How its parts come in and go ([5](#5-transitions-reveals-and-animations)). |
| Open Sound, Close Sound | Sound | Played as it opens, and as its close starts. |

Every key a screen binds is an input action players can rebind, whose keys are set in an Input Mapping Context, not on the screen ([8.4](#84-input-actions)). In C++, **Register Input Action** binds one of your own, holds included. Compiling a screen warns about a Close Input Action shown in action bars without a Description, and Close Input Actions while Common UI's Enhanced Input support is off or with Input Config set to Game. It notes a Close Input Actions entry left empty, and Display in Action Bar ticked while Show Close Input Actions is off. A Confirm Display Name with no Accept in the project's Common UI Input Data to show it beside is logged as the game runs instead, once per screen class: in the editor, Common UI may not have loaded its Input Data when a Blueprint compiles.

Name a widget animation **OpenAnimation** and it plays as it opens; name one **CloseAnimation** and it plays as it closes.

**Closing.** Call **Close Widget**, not Deactivate Widget, so the close plays. Back and Close Input Actions do the same. While the close plays, it takes no clicks or keys, **Is Closing** is true, and **On Close Started** fires, so you can unbind anything that shouldn't work meanwhile. **Close Widget Now** cuts it short. In C++, `CloseWidgetThen` runs something once the close has finished.

**Focus.** **Focus When Drawn** gives a widget focus once it's drawn and can take it, such as a cell on a page a tab switcher is still bringing in: focus given to a widget that isn't drawn yet is lost. A Menu without a Get Desired Focus Target gets a compile warning, as gamepad focus would have nowhere to start.

> Widgets reuse their instances, so each opening starts the reveal afresh from hidden. Leave the render opacity and render transform of revealed parts to the reveal: it adds to the values you set in the designer, but anything else animating them at the same time will fight it.

### 4.1 The description line

Console menus keep a line along the bottom of the screen that says what the player can do right now, such as "Adjust mouse and keyboard control settings." beside Key Bindings. Every Modular Activatable Widget can have one: name a Text Block **`DescriptionText`**, and optionally put it in a **`DescriptionPanel`**, which hides, without collapsing, while there's nothing to say.

The line describes actions, never things. What a thing is, such as an item's name and description, belongs in a details panel. A console inventory, for example:

| The player is | The line says | It comes from |
| --- | --- | --- |
| Browsing the items | Nothing: the details panel shows the hovered item | Default Description, left empty |
| On an option of the item's menu | What the option does: "Use the item.", "Combine items to make something new." | The menu, saying it on the screen's line ([6](#6-context-menus)) |
| Choosing the second item after Combine | What to do: "Select an item to combine with." | The screen's hint, `GetHint`, or **Show Description** from Blueprint |
| Moving an item | Where to put it, or why it can't go there | The screen's hint |
| On a button of an options menu | What it changes: "Adjust mouse and keyboard control settings." | The button's **Description** |

| Setting | Group | Description |
| --- | --- | --- |
| Default Description | Description | What it says while nothing else does, such as what the screen is for. Empty, as an inventory's is, hides Description Panel then. |
| Keep Last Description | Description | It keeps describing the last thing highlighted until something else is, as console menus do, so the mouse crossing a gap leaves it as it was. Off, it goes back to Default Description as the mouse or focus leaves what it describes. |

What it says, the first that has something:

1. What's said on its behalf, the newest first: a [context menu](#6-context-menus) open over it saying what its highlighted entry does, or anything given **Show Description From**. **Show Description** does this for the widget itself, such as for a mode it's in. Each source clears only its own, with **Clear Description From** or empty text, so a menu closing never clears what a newer menu says, and as one stops, the one before it shows again.
1. What the widget asks of the player now, from C++ (`GetHint`), such as an inventory screen's hint for the item being moved. Call **Refresh Description** as it changes.
2. What's highlighted: the **Description** of the [Modular Button](#8-buttons-tabs-keys-and-meters) the mouse is over or focus is on, or whatever a widget of your own passes to **Highlight For Description**. In C++, `SetHighlightedDescription` describes something that isn't a widget of its own, such as a menu's entry; give it empty text for something that says nothing, such as an item under the cursor, so a button's description isn't kept for it.
3. Default Description.

A button reports itself to the nearest Modular Activatable Widget around it with a description line, so a button on a page inside a screen describes itself in the screen's line. **On Description Changed** fires as the line changes, such as to fade it.

Which word for which line:

| Term | What it is | In Modular UI |
| --- | --- | --- |
| Description | What the highlighted option does, along the bottom of a menu | Description Text |
| Hint | What to do now, such as "Select an area to place this item." | Description Text too, ahead of what's highlighted |
| Details | What a thing is: an item's name, picture and description | A details panel of your own, such as Modular Inventory's item details widgets |
| Tooltip | A small box beside the cursor, on hover | Unreal's tooltips, such as an item widget's Tooltip Class in Modular Inventory |
| Message | Something gameplay tells the player, that comes and goes, such as "It's locked." | [Messages](#9-messages) |

## 5. Transitions, reveals and animations

A screen can move in two ways, and they play together:

| | Transitions and reveals | Widget animations |
| --- | --- | --- |
| Made in | Class Defaults, as data | The designer's Animations tab |
| Good at | Motion that depends on where and how the widget opened, such as a menu sliding out of the item it opened beside, whichever side that is; motion many widgets share, set once in a base class; parts arriving one after another | Anything else: colours, materials, glows, a pulse, a one-off flourish |
| Playing backwards | A close sends the parts back from wherever they got to, faster the less was shown | CloseAnimation, or OpenAnimation backwards with Close Reverses Open |

A close waits for whichever ends last. Use both freely: a context menu's Open Transition slides it out of its anchor while its OpenAnimation brightens its frame.

**Reveals.** Each entry of **Open Reveal** names a widget in the designer and how it comes in:

| Field | Description |
| --- | --- |
| Widget | The widget's name, such as `ItemGrid`. Everything inside it comes in with it. None is the whole screen. |
| Transition | How it comes in. Give the parts different **Delay**s so they arrive one after another. |

A name that matches no widget is skipped, with a warning in the log. A widget named twice keeps its first entry. Group widgets that move together under one panel and reveal the panel. To bring in a **Background Blur**, reveal it with a fade: with **Apply Alpha to Blur** on, its default, the blur weakens with its opacity. An entry with a Duration of 0 and a Delay pops the widget in once the delay has passed.

**Transitions.**

| Setting | Description |
| --- | --- |
| Delay | How long it waits before it starts. Coming in, the widget stays hidden meanwhile. |
| Duration | How long it takes once it starts. |
| Easing, Easing Strength | How it speeds up and slows down. Ease Out starts quick and settles. |
| Curve | Used instead of Easing. Rise past 1 before settling on it to overshoot. |
| Fade, Fade From | Fades in from this opacity. |
| Slide, Slide Distance, Slide From | Slides in from this many pixels away. **From Origin** slides out from where the widget opened, such as away from what a menu opened beside; without an origin it slides up. |
| Slide Unit | What Slide Distance counts: **Pixels**, or **Own Size**, the widget's own width or height along the slide, so 1 slides it in from exactly its size away, such as out from behind what clips it. |
| Scale, Scale From, Scale Origin | Grows from this fraction of its size, from its middle or from its corner nearest the origin. |

A transition always starts and ends exactly where it should, whatever the curve does on the way.

## 6. Context menus

A **Modular Context Menu Widget** is a small menu opened beside something, such as Use, Equip and Drop beside an item, or Set Waypoint beside a map marker. Push it onto a layer, give it an anchor, and it places itself beside it, staying on screen. Modular Inventory's item menus are context menus.

**Layout.**

- **`MenuPanel`** (required): the menu, inside a Canvas Panel that fills the screen, with Size To Content ticked, so it can be moved beside its anchor.
- `MenuContent`, optional: everything the menu shows, its background included, inside Menu Panel, for **Slide From Behind Anchor**. Menu Panel then has no background of its own.
- The root: a full-screen, hit-testable Border, so clicks off the menu reach it and nothing behind it can be hovered while it's open. It can be a dim backdrop for a modal menu.
- Entries: the buttons inside Menu Panel, placed in the designer, whose On Clicked you bind, or made with **Add Entry** in `EntryPanel` from **Entry Button Class**, which **On Entry Chosen** reports. A Modular Button shows an entry's label and icon.
- `DescriptionText`, optional: what the highlighted entry does, the menu's own [description line](#41-the-description-line): an entry's Description, or what `GetEntryDescription` says for it. With **Show Description On Screen**, the screen the menu opened from says it too, along its bottom, so most menus need none of their own.
- For prompts asked in the menu: `PromptPanel` with `PromptText`, `ConfirmButton`, `CancelButton`, and for counts `QuantitySlider` and `QuantityText`; optionally `PromptTitleText` and `PromptBackdrop` ([7](#7-prompts)).

**Anchor.** **Set Anchor Widget** opens it beside a widget, **Set Anchor Box** beside a box on the screen, such as a click's position, and **Set Anchor To Focus Or Cursor** beside the focused widget on a gamepad, or the cursor. A menu pushed without one opens beside the focused widget on a gamepad, or at the cursor. Call them as the menu is pushed, before it shows.

**Settings**, grouped under Context Menu:

| Setting | Description |
| --- | --- |
| Placement | **Toward Center** (beside the anchor, toward the middle of the screen), **Right**, **Left**, **As Designed** (wherever Menu Panel is laid out, such as the middle of the screen for a modal picker) or **At Cursor** (down and to the right of the cursor, as desktop menus open). Each but As Designed moves in as far as it takes to stay on screen. |
| Menu Gap, Screen Padding | Space between the anchor and the menu, and kept between the menu and the screen's edges. |
| Outside Click | What a click off the menu does, with any button but the right: **Close** only closes it, **Pass Through** closes it and goes on to what's under it, where the menu knows how to reach it, and **Block** keeps it open until an entry is picked or Back. |
| Right Click | **Project Default**, the default, is Back while the project's **Extra Back Keys** have Right Mouse Button ([11](#11-keyboard-navigation)), and Pass Through otherwise. **Back** goes back a step wherever the cursor is, as Escape does: out of a prompt, then out of the menu, as in games where the right mouse button means Back. **Pass Through** does as desktop menus do: off the menu, it closes it and reaches what's under it, so right-clicking something else opens its menu instead; on the menu, nothing. **Ignore** does nothing. Whichever it comes to decides for the menu: Extra Back Keys don't go Back on it as well. **Get Effective Right Click** says which it is. |
| Keep Entry Highlighted | One entry always shows highlighted, as console-style menus keep one: the first as it opens, then whichever the mouse or focus reached last, with its button style's Selected look. Off, an entry looks highlighted only while hovered or focused. |
| Show Description On Screen | What the highlighted entry does shows in the [description line](#41-the-description-line) of the screen the menu opened from too, as console menus say it along the bottom of the screen, from as it opens until it starts to close. That screen is the nearest Modular Activatable Widget with a description line around the menu's anchor widget, or the one **Set Description Screen** gives, such as for a menu anchored to the cursor. On by default. |
| Prompt Presentation | Where an entry's question goes: **Inline**, **Overlay** or **Dialog** ([7](#7-prompts)). |
| Open Transition | How the menu comes in: the whole menu fades, and Menu Panel slides and grows out of its anchor. Leave the layer's own transition at 0. |
| Slide From Behind Anchor | The menu slides out from behind its anchor, as a drawer from under a desk, rather than appearing over it: Menu Panel clips what's inside it, and `MenuContent` slides in from the anchor's side by its own size, with Open Transition's timing, easing and fade. Without Menu Content it does nothing, with a warning in the log. |

Its **Input Config** is Menu, and it's modal: whatever is under it keeps its actions to itself while it's open, even in a stack inside a screen.

**Sliding out from behind.** A menu opened beside something, such as an item, reads best coming out from under it. Lay it out as Menu Panel (no background) › Menu Content (the background and everything else), tick **Slide From Behind Anchor**, and give Open Transition a short duration, such as 0.12 seconds with Ease Out, and a slight fade. With Placement Toward Center it comes out of whichever side it opens on. Menu Panel's clipping does the work, so the menu is never drawn over its anchor, whichever layer it's on: it's cut off at Menu Panel's edge beside the anchor until it has slid out. Keep **Menu Gap** small, such as 0 to 4 pixels, so it comes out right at the anchor's edge.

**Action bars.** Give it a **Confirm Display Name**, as any screen, and Back and Confirm show in an action bar while it's open, best a [Modular Bound Action Bar](#82-action-bars). Both are bound once and never change while the menu is open, so the bar doesn't flicker or reorder as the mouse moves over the entries. Confirm only shows what Accept does: Accept goes to the focused entry, or with the mouse on an entry, to that one; and should focus have left the menu, the entry the player sees takes it first.

**Which settings for which game?**

| Style | Outside Click | Right Click | Placement |
| --- | --- | --- | --- |
| Console-style, such as survival horror played with a mouse: right-click is Back | Close | Back, or Project Default with Right Mouse Button in Extra Back Keys | Toward Center |
| Desktop-style, such as a CRPG or a looter: right-click opens a menu | Pass Through | Pass Through, or Project Default without Right Mouse Button in Extra Back Keys | At Cursor |
| A modal picker that must be answered | Block | Back | As Designed, over a dim backdrop |

**Entries made in Blueprint.** **Add Entry** takes an entry with an **Id**, **Label**, **Description**, **Icon**, **Enabled**, **Ask First** with a **Prompt**, and **Keeps Menu Open**. On Entry Chosen gives its Id and the count chosen. **Clear Entries** removes them, and placed buttons stay.

From C++, override `PassPressThrough` to reach what's under the menu, `GetEntryDescription` for what Description Text says, `FindDescriptionScreen` for which screen says it too, and `GetEntriesWidget` for what an inline prompt hides. `DescribeHighlightedEntry` says something else for the highlighted entry, such as why it can't be picked, on both lines.

## 7. Prompts

A **prompt** asks the player something before an action: to confirm, or how many. Its request has a **Kind** (Confirm or Quantity), a **Title**, a **Message** (`{Count}` and `{Max}` are filled in as the count changes), **Confirm Label** and **Cancel Label** for Modular Buttons, and for counts **Min Count**, **Max Count** and **Default Count**.

A context menu asks where its **Prompt Presentation** says:

| Presentation | Where the question shows |
| --- | --- |
| Inline | In `PromptPanel`, in place of the entries: the menu becomes the question and goes back to its entries after. As compact as it gets, and the player's eyes stay where they were. Lay Prompt Panel out inside Menu Panel. |
| Overlay | In `PromptPanel` wherever it's laid out, such as the middle of the screen, over the menu, whose entries stay where they are, disabled. `PromptBackdrop`, such as a dim full-screen image, shows behind the question. Clicks off it don't close the menu. |
| Dialog | In a **Prompt Class** dialog of its own, pushed onto **Prompt Layer**, so one dialog serves every menu, as a game's confirmation dialog. Pick a layer above the menu's, as a stack only shows its top. Without the layer it asks inline. |

**Confirm Starts On Cancel** starts a confirmation on Cancel Button, so the quick answer is the safe one; a count to choose starts on Quantity Slider. Back, or a right-click while the menu's Right Click comes to Back, answers no and goes back to the entries.

**Prompt dialogs.** A Widget Blueprint with parent **Modular Prompt Widget**, with `PromptPanel`, `TitleText`, `PromptText`, `ConfirmButton`, `CancelButton`, `QuantitySlider` and `QuantityText`. **Right Click** decides what a right-click does, on the dialog or off it: **Cancel** answers no, as Back does, **Ignore** does nothing, and **Project Default**, the default, cancels while the project's Extra Back Keys have Right Mouse Button and does nothing otherwise. A dialog saved with the old Right Click Cancels off loads as Ignore. **Click Outside Cancels** makes a left-click off Prompt Panel answer no. To ask from anywhere, use the **Show Modular Prompt** node with a Prompt Class, a layer such as `UI.Layer.Modal` and a request: it fires **Confirmed** with the count chosen, or **Cancelled**.

## 8. Buttons, tabs, keys and meters

**Modular Button** is a Common UI button with a `Label` and an `Icon`: **Label Text**, **Icon Texture**, **Icon Color** and **Lit Icon Color** (while hovered or selected), and **Justification**, which moves the Horizontal Box holding Icon and Label, or with **As Designed** leaves it where the designer has it. Its **Description** shows in the [description line](#41-the-description-line) of the screen it's on while the mouse is over it or focus is on it; Set Description changes it. **Modular Tab Button** adds a `SelectedFrame`, a **Selected Icon Scale** and **Always Show Label**.

**A button's key.** A Common UI button can have a key of its own, its **Triggering Enhanced Input Action**, that presses it, such as an item action's key in an item menu. Name a [Modular Input Key Widget](#81-pages) `KeyWidget` in a Modular Button and the button shows that key there: only its own key, never Accept's, collapsed while it has none or is disabled, and following rebinding and the device in use. Its **Show On** shows the key only on a gamepad, or only on keyboard and mouse. Where it sits is the designer's. Give it a column of its own: an outer Horizontal Box holding the Overlay with the Icon and Label row, set to Fill, then a Size Box with the key widget. The keys of a list then line up at the button's edge whatever the labels' length, never cover a label, and a button without a key keeps its whole width, as a Size Box whose child is collapsed takes no space. The Size Box's size is the key's: wrap the key widget's `KeyIcon` in a Scale Box set to Scale To Fit, Down Only, and it shrinks to fit without making the button any bigger. Common UI's own `InputActionWidget`, a Common Action Widget, shows the same key; use one or the other. Without either, the key still presses the button, unseen.

**Modular Tab List** makes a Tab Button Class button in `TabButtonPanel` for each of its **Tabs**, each opening the page at the same index in the linked switcher: link a [Modular Page Switcher](#81-pages), or any Common Animated Switcher, holding the pages with Set Linked Switcher. Its **Next Tab Enhanced Input Action** and **Previous Tab Enhanced Input Action**, Common UI's own fields, change tabs ([8.4](#84-input-actions)); leave the row fields beside them empty.

| Tab field | Description |
| --- | --- |
| Tab Id | What Select Tab By ID and On Tab Selected know it as. None names it by its index. |
| Label, Icon | What its button shows. |
| Description | What its page is for, in the screen's description line while the mouse is over the tab. |
| Focus Widget | Where focus goes as its page shows the first time, by name, in the page's own designer when it's a widget of its own. None leaves it to the page. |
| Shortcut Input Action | An input action that opens this tab directly, such as M for a map tab or C for a crafting tab. With **Shortcut Toggles Screen**, pressed on its own open tab it closes the screen, as the key that opened it there would. |

With **Focus Pages**, as a page shows again focus goes back to where it was when the player left it, once the page is drawn, however long the switcher takes to bring it in. **On Tab Page Shown** fires then, for anything else that should happen, such as aiming at an item. Get Open Tab Index, Get Tab Index Of, Open Tab At Index and Get Tab Page help from Blueprint.

### 8.1 Pages

**Modular Page Switcher** holds a screen's pages, such as the pages of a tab list. Place it in place of a Common Animated Switcher or Common Activatable Widget Switcher; right-click one in the designer and **Replace With** keeps its pages. It changes page at once and brings the new page in with **Page Transition**: by default a 0.15 second fade and a 40 pixel slide from the side the page comes from, as console games turn the pages of their menus.

| Setting | Description |
| --- | --- |
| Page Transition | How each new page comes in. With Slide From set to From Origin, a later page slides in from after it along Page Order and an earlier one from before it, as if the pages lay side by side. No effect uses the Common Animated Switcher transition under Transition instead. |
| Page Order | Which way the pages lie, for a slide From Origin: side by side, or one above another. |
| Block Input While Showing | The page coming in takes no clicks or hover until it's in, so nothing it slides under a still cursor lights up on the way. |

**Set Page Transitions Enabled** off changes pages at once, such as for the page a screen opens on, which the screen's own open brings in. **On Page Shown** fires once a page is in. It's a Common Activatable Widget Switcher, so pages that are activatable widgets are activated as they show.

Why not the Common Animated Switcher's transition? It plays the old page out before the new one comes in, both within its Transition Duration, so the player waits on the old page leaving, nothing on the new page can take focus until it's switched to partway through, and a short duration squeezes both halves into a jolt. Its pages are the real widgets, moving under the cursor and lighting up as they pass. A tab change that feels instant shows the new page at once and lets it settle in for a tenth to a fifth of a second, with Ease Out, which is what Page Transition does.

**Modular Input Key Widget** shows the key the player has mapped to an Enhanced Input action in `KeyIcon`, using Common Input's controller data for the icon, and follows rebinding and the input method. Set **Input Action**, or call Set Input Action. **Show On** limits it to **Gamepad** or **Keyboard And Mouse**; on any other device it's collapsed, as it is without a key to show. In the designer it stays as laid out, so it can be seen and placed. Use it for any key that isn't a button's own, such as a tab list's Next Tab and Previous Tab beside it, a quick slot's key or a message's.

### 8.2 Action bars

**Modular Bound Action Bar** shows the actions the player can press now, each a button with its key and name, as Common UI's **Common Bound Action Bar** does: whatever the screens and menus taking input have bound to show in action bars. Use it in its place, with the same **Action Button Class**, such as a Common Bound Action Button, or a **Modular Bound Action Button** to show holds.

A Common Bound Action Bar makes every button again whenever any binding changes, even one it doesn't show, such as when a menu opens over a screen and binds its own Back: the whole bar blinks out and back. This bar only changes what changed. An action still shown keeps its button and its place, so a menu whose Back and Confirm the screen had already shown changes nothing on screen, and an action that comes or goes leaves the others where they are.

| Setting | Description |
| --- | --- |
| Action Button Class | Made for each action: a Common UI button that shows a bound action, such as a Common Bound Action Button, or a Modular Bound Action Button, which shows holds. |
| Ignore Duplicate Actions | An action bound more than once, such as Back by a screen and the menu over it, shows once. On by default. |
| Clickable Buttons | A click or tap on a button does its action, as Common UI's own action bar does, which suits menus played with the mouse. Off, the buttons only show each action's key and name, as console games show them: the mouse goes through them to whatever is under them, and they never light up under it. On by default. A hold action's button never takes clicks either way: only its key holds it. |
| Settle Time | How long what's bound must stay the same before the bar shows it, 0.05 seconds by default, so it skips what Common UI passes through for a frame or two as menus open and close. |
| Num Designer Preview Entries | Buttons drawn in the designer, to see the bar laid out. |

Its buttons never take gamepad focus, so focus stays on the menu: a gamepad presses the key a button shows. It follows the input method, showing only actions with a key for the device in use, and rebinding. Actions keep the order they were shown in; new ones go after them, earlier only for a lower Nav Bar Priority, as Common UI sorts them: an input action's comes from the Common Input Metadata in its Player Mappable Key Settings ([8.4](#84-input-actions)). **On Actions Changed** fires once the buttons are updated, such as to fade the bar in when it had nothing. **Refresh Actions** works it out again at once.

**Showing holds.** Any Common Bound Action Button gets a hold's progress through **On Action Progress**. Make the Action Button Class a Widget Blueprint with parent **Modular Bound Action Button**, and it also knows an action is a hold before it's held, so it can show an empty ring for the player to fill:

| Event or function | Description |
| --- | --- |
| On Represented Action Changed (Hold Action, Action Hold Time) | The button was given an action to show. Show an empty ring around its key for a hold, so the player knows to hold it, and hide the ring for anything else. Action Hold Time is the hold's length in seconds. |
| On Action Progress (Held Percent) | How far the hold is, 0 to 1, as the key is held: fill the ring from it. It reaches 1 as the hold completes, and drops to 0 at once as the hold ends or the key is let go early. |
| Is Hold Action, Get Action Hold Time | The same, from Blueprint, at any time. |

### 8.3 Segmented meters

**Modular Segmented Meter** shows a fraction as a row of separate segments, such as health, a battery's charge, stamina or rounds. It reads at a glance, and is less like a placeholder than a plain progress bar. A loss leaves a trail, in **Trail Color**, that waits **Trail Delay** and then drains away at **Trail Drain Rate**, so the player sees how much was just lost, as fighting and survival games show damage. Each new loss while the trail shows waits again, so a run of hits reads as one. A gain covers as much of the trail as it reaches. It animates in real time, so it moves behind a paused game's menus.

| Setting | Description |
| --- | --- |
| Percent | How full it is, from 0 to 1. **Set Percent** leaves a trail behind a loss; **Set Percent Immediately** doesn't, such as as it first shows. |
| Segment Count, Segment Spacing | How many segments, and the pixels between them. Segments too thin to tell apart lose their gaps before their size. |
| Fill From | Which end fills first: Left To Right, Right To Left, Bottom To Top or Top To Bottom. |
| Whole Segments | Segments light whole, a part-lit one counting as lit, as a ticking gauge does, so a sliver left still shows a segment. Off, the last lit segment fills partway. |
| Segment Brush | What each segment draws, tinted: a small rounded box by default, or a texture of your own, such as a worn tick. A part-lit segment is cut, not squashed, so a texture keeps its shape. Its Image Size is the size each segment wants; the meter fills the space it's given. |
| Fill Color, Empty Color | The lit segments and the rest. **Set Fill Color** changes it at runtime, such as to turn a low battery red. |
| Trail Color, Trail Delay, Trail Drain Rate | The trail behind a loss. A drain rate of 0 shows no trail. |

### 8.4 Input actions

Every key a Modular UI widget binds is an Enhanced Input action, so players can rebind all of them: a screen's Close Input Actions, a tab list's Next Tab, Previous Tab and tab shortcuts, keys your own widgets bind with Register Input Action, and, in plugins built on it, such as Modular Inventory, their actions. A widget field only says which action. The keys are set once, in an Input Mapping Context, never on a widget, so there's no data table row or UI action tag to keep in step. Accept and Back stay Common UI's own, as the end of this section explains.

**Once per project.**

1. Set **Project Settings › Engine › General Settings › Default Classes › Game Viewport Client Class** to `CommonGameViewportClient`, or a class based on it, and restart the editor. Common UI routes keys to the widgets that bound them from there, so without it no widget's key does anything.
1. Turn on **Project Settings › Game › Common Input Settings › Enable Enhanced Input Support**. While it's off, Common UI ignores the actions, and Modular UI logs a warning.
2. Make an Input Mapping Context for menus, such as `IMC_UI`, and add it for the player above the gameplay contexts, with a higher **Priority**, as your game adds those. Keep it added during play: Common UI only routes a menu action to the widget that bound it, so a menu's key does nothing while that menu isn't open, and a menu whose Input Config is Menu keeps its keys from the game.
3. Turn on **Project Settings › Engine › Enhanced Input › Enable User Settings**, and register the context with Enhanced Input's user settings (**Register Input Mapping Context**), so the keys players choose are the ones used.

**For each key.**

1. Make an Input Action, such as `IA_UI_Rotate`: **Value Type** Digital (bool), a **Description**, which action bars show as its name, such as Rotate, and **Consume Lower Priority Enhanced Input Mappings** unticked. For a key players can rebind, give it **Player Mappable Key Settings** with a unique **Name**, the **Display Name** and **Display Category** a keybinding menu shows ([8.5](#85-key-bindings)), and for its place in action bars, **Metadata** (below).
1. Map its keyboard and gamepad keys in the menu context, with no triggers on the mappings.
2. Set it in the widget field for that action.

A rebound key then works in menus at once, and every action bar and Modular Input Key Widget shows it. Why Consume Lower Priority Enhanced Input Mappings off and a higher priority: Enhanced Input drops a lower-priority context's mapping of a key that a higher-priority action consumes, so with both, a key shared by a menu action and a gameplay action, such as R for Rotate and Reload, stays mapped to both.

**One input action, many places.** Set the same input action wherever its key should work, such as an inventory key as the HUD's open key, the inventory screen's Close Input Action and a tab's Shortcut Input Action. Each widget that binds it gets it while that widget takes input, and nothing hands a key from one widget to another. A widget binds each input action once, so give each field of one widget its own.

**Holds.** A **Hold** trigger on an input action makes it a hold: it runs once its key has been held for the trigger's **Hold Time Threshold**. Put the trigger on the input action itself, under **Triggers**: a trigger on a mapping in the Input Mapping Context isn't read. **Hold And Release** is a trigger of its own, and doesn't make a hold. Common UI doesn't time holds for Enhanced Input actions, so each local player's **Modular UI Hold Subsystem** does, in real time, so holds work while a menu pauses the game:

- A hold is timed from its key's press. Letting go first cancels it, and so does the action no longer taking input meanwhile, such as when a menu opens over its screen; either way its progress drops to 0 at once.
- Each press holds once: keeping the key down after the hold completes doesn't run it again.
- Only the player's own keys hold: in split-screen, another player's press of the same key doesn't.
- The action bar button showing the action gets its progress, 0 to 1, through **On Action Progress**: 1 as the hold completes, then 0. A Modular Bound Action Button also shows which actions are holds before they're held ([8.2](#82-action-bars)).
- Only the key holds it: a click on its button in an action bar does nothing.
- Holds work for actions Modular UI binds with Register Input Action: a screen's Close Input Actions, a tab list's tab shortcuts, keys your own widgets bind that way, and plugins' actions, such as Modular Inventory's screen actions. Modular UI doesn't time a Hold trigger on a tab list's Next Tab and Previous Tab, which Common UI binds itself, or on keys that press a button, such as a Common UI button's **Triggering Enhanced Input Action**; such a button can have a hold of Common UI's own, its **Requires Hold**.

If the log says Common UI times a hold of its own for an action, the engine version holds Enhanced Input actions itself, and the key has to be held for both holds, one after the other. For an action you bind in C++, bind it with `ModularUI::MakeBindArgs` and the widget's Register UI Action Binding instead, which leaves the hold to Common UI.

**Action bar order.** Action bars order input actions by the **Nav Bar Priority** in their Common Input Metadata, lower first. Make a **Common Mapping Context Metadata** data asset, add each action that shows in action bars under **Per Action Enhanced Input Metadata** with its Nav Bar Priority, and set the asset as **Metadata** in each of those actions' **Player Mappable Key Settings** ([Using CommonUI with Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-commonui-with-enhnaced-input-in-unreal-engine)). Leave **Is Generic Input Action** ticked in every entry: unticked, Common UI sends the action's key to Enhanced Input instead, and the widget's action never runs.

**Accept and Back.** They stay Common UI's, set once in your **Common UI Input Data**: Accept presses the focused button through Slate's navigation keys, and Back is its **Default Back Action**, a data table row whose keys are fixed. A screen's **Confirm Display Name** only says what Accept does there, beside Accept's key: the **Default Click Action**, or the **Enhanced Input Click Action** when your Common UI Input Data has one. Keys that go Back as well, such as the right mouse button, are a project setting, **Extra Back Keys** ([11](#11-keyboard-navigation)).

Epic describes Common UI's Enhanced Input support as limited ([Using CommonUI with Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-commonui-with-enhnaced-input-in-unreal-engine)). What Modular UI relies on is binding an action and finding the keys mapped to it; holds it times itself. In C++, `ModularUI::RegisterInputAction`, in `ModularUIInput.h`, binds an action for any Common User Widget, holds included; a Modular Activatable Widget's **Register Input Action** calls it for you.

**When a key does nothing.** Run `ModularUI.Input.Status` in the console while playing. For each local player it logs whether Common UI's Enhanced Input support is on, then each action taking input now: the widget that bound it, its keys on keyboard and mouse and on gamepad, its name in action bars, whether it's a hold, and whether players can rebind it. An action missing from the list isn't bound now, or something modal is open over its widget; one listed without keys for a device isn't mapped for it in a context that's added for the player. Modular UI also logs warnings: once when Common UI's Enhanced Input support is off, and once for each action Common UI doesn't bind, such as one a widget binds twice, each action shown in action bars without a Description, and each action Common UI times a hold of its own for.

### 8.5 Key bindings

A keybinding menu lists the actions players can rebind, and lets them change each key: press its slot, then the new key. Make four Widget Blueprints, each with a Modular UI parent:

| Parent | What it is | Named widgets |
| --- | --- | --- |
| **Modular Key Binding List** | The list, for one kind of device: a row for each action, under a heading for each category | **`RowPanel`** (required): any panel, such as a Vertical Box inside a Scroll Box. Leave it empty: the rows are made for it |
| **Modular Key Binding Header** | A heading over one category's actions, such as Menus | `CategoryText` |
| **Modular Key Binding Row** | One action: its name and its keys | `DisplayNameText`, `ResetButton` (any Common UI button), and a Modular Key Binding Slot for each key the row shows |
| **Modular Key Binding Slot** | One key, as a Modular Button | `KeyIcon` (an Image), and the Modular Button's `Label` |

The list shows what Enhanced Input's user settings know, for the player's active key profile: each input action with **Player Mappable Key Settings**, mapped in an Input Mapping Context registered with the user settings ([8.4](#84-input-actions)). A row shows the action's **Display Name**, under its **Display Category**, both from its Player Mappable Key Settings, the categories in the order their first action was registered.

| List setting | Description |
| --- | --- |
| Device | **Keyboard And Mouse** or **Gamepad**: the keys it shows and takes. Make one list for each, such as on a Keyboard tab and a Gamepad tab. |
| Row Class, Header Class | Made for each action, and over each category's actions. Without a Header Class, every action is listed without headings. |
| Cancel Keys | Keys that stop the wait for a key, changing nothing, so players can't put them on actions. Escape and the gamepad's Start, Gamepad Special Right, by default. |
| Reserved Keys | Keys players can't put on actions. Pressed while waiting, they're ignored. |
| Allow Mouse Buttons | Mouse buttons can be put on actions, on a Keyboard And Mouse list. On by default. |
| Key Conflict | What happens when the key picked is another action's already: **Allow**, the default, puts it on both; **Swap** gives the other action this one's old key; **Refuse** leaves the keys as they were. |
| Conflicts Within Category | Only actions of the same category count as having the key already, so a key can be a menu action's and a gameplay action's, such as R for Rotate and Reload. On by default. |
| Save On Change | Saves the player's keys after each change, so they're used next time too. On by default. Off, call **Save Keys** yourself. |
| Num Designer Preview Rows | Rows drawn in the designer, after one heading when there's a Header Class. |

**Slots.** Place a Modular Key Binding Slot in the row's designer for each key it shows, and set each one's **Key Index**: 0 for the action's first key for the device, 1 for its second, in the order Enhanced Input keeps them. Players change the keys mapped in the Input Mapping Context and don't add more, so a slot whose Key Index is past the keys mapped for the device is hidden, keeping its place: map a second key for the device to give players a second key to change. A slot shows its key's icon from Common Input's controller data in `KeyIcon`, and the key's name in Label where there's no icon, or beside it with **Show Name With Icon**. **Waiting Text**, "Press a key" by default, shows while it waits, and **Empty Text**, "Not Bound" by default, while its key is unbound. A slot is a Modular Button, so its **Description** shows in the screen's description line while it's highlighted.

**Waiting for a key.** Pressing a slot waits for the next key or mouse button the player presses. Meanwhile nothing else sees that player's presses, so Back, the action bar and the button under the cursor don't react, and a key held since before the wait is let go as usual. One of Cancel Keys stops the wait; a key of the other device, a Reserved Key or a touch is ignored, so a gamepad button never lands on a keyboard list. **On Waiting Changed** (Waiting) fires as the wait starts and stops, such as to show "Press a key, or Esc to cancel". The list stops waiting by itself as it leaves the screen.

**Keys change at once, everywhere**: action bars, key widgets and holds use the new key straight away. **On Keys Changed** (Mapping Name) fires after each change, and **On Key Conflict** (Action Name, Key, Other Actions, Bound) when the key was another action's too, such as to say "Also used by Reload"; Bound says whether it was put on the action anyway. A row's `ResetButton` puts the action's keys for the device back as the game set them, and shows only while one differs; **Reset All To Default** does it for every action listed. The list follows keys changed anywhere else, and the gamepad in use, for its icons.

**Gamepads.** Put the list on a screen whose Get Desired Focus Target returns the list's **Get First Slot**, so focus starts on the first action's first key.

## 9. Messages

Anything can tell the player something without knowing what shows it: send a **message** to a **channel**, and every **message feed** that shows that channel shows it, each in its own way. One feed stacks toasts in a corner, another shows a banner across the screen, another a large heading.

**Sending.** The **Modular Message Subsystem**, one per local player: **Show Message** with a message, **Show Text** with a channel and text, **Hide Message** by channel and key, and **Clear Channel**. The function library has the same by player controller.

| Message field | Description |
| --- | --- |
| Channel | Which feeds show it: those whose Channels list this tag, or a parent of it. |
| Key | Tells it apart, so it can be changed or hidden later, such as what the player is looking at. A message with the key of one shown replaces it. None for a one-off. |
| Title, Body | Such as an item's name, and what it says. Their widgets collapse while empty. |
| Icon | Loaded as it shows. |
| Count | Such as how many were picked up. Repeats a feed merges add theirs up. |
| Input Action | An Enhanced Input action whose key it shows, such as the key that picks an item up. |
| Duration | Seconds, in real time. Below 0 uses the feed's; 0 shows it until hidden or replaced. |
| Priority | Higher comes first in a queue, and stays longest in a full stack. |
| Style | A tag for entries to look different, such as a warning. |
| Payload | Anything else an entry shows, such as an item definition. |

**Showing.** A Widget Blueprint with parent **Modular Message Feed**, placed in the HUD, or in the root layout to show over menus too:

| Setting | Description |
| --- | --- |
| Channels | The channels it shows, and their children. Empty shows every channel. |
| Mode | **Stack** shows several at once, each going by itself; **Queue** one at a time, each after the last, so none is missed; **Replace** one at a time, the newest at once. |
| Default Duration | For messages that don't say. 0 shows them until hidden. |
| Merge Repeats | A message the same as one showing joins it: its count adds up and it shows for its full time again. |
| Max Shown, Max Waiting | For stacks and queues. |
| Entry Class, `EntryPanel` | Each message is a **Modular Message Entry Widget** made in Entry Panel, with `TitleText`, `MessageText`, `IconImage`, `CountText` and `KeyWidget`, its own Show and Hide transitions and animations, and On Message Changed. |
| Without an Entry Class | The feed shows one message at a time in its own `TitleText`, `MessageText`, `IconImage`, `CountText` and `KeyWidget`, inside `MessagePanel`, with **Show Transition**, **Hide Transition** or **Hide Reverses Show**, and animations named `ShowAnimation` and `HideAnimation`. |

On Message Shown and On Message Hidden fire for sounds and the like. Messages time out in real time, so they go while the game is paused too.

**Messages that stay.** A message with a **Key** and a **Duration** of 0, or below 0 in a feed whose Default Duration is 0, stays until **Hide Message** with its key, such as what the player is looking at. A newer message can still take its place in a Replace feed, such as a hint, or push it out of a full Stack; it comes back once there's room again, unless it was hidden meanwhile. A message without a key can't be hidden, so it stays only until another replaces it.

**When a message goes too soon.** Run `log LogModularUI Verbose` in the console. The log then says as each message shows, for how long, and why each goes: its time was up, Hide Message was called for its key, a newer message took its place, more than Max Shown were showing, or it was cleared. Its time being up means the message had a Duration above 0, or below 0 in a feed with a Default Duration above 0. Hide Message means whatever sent it hid it, such as an interaction prompt as the player looked away.

**Where to place a feed.** A feed shows only while it's on screen, so place each where its messages belong:

| Place | Shows | For |
| --- | --- | --- |
| The HUD, such as on the Game layer | During play only: a HUD that hides while menus are open hides its feeds too | What the player looks at, pickups, objectives |
| The root layout, above every layer | Over everything, menus and the HUD alike | Hints that can come from a menu, such as "Not enough space." while an inventory is open |
| A screen | While the screen is open | Anything only that screen says |

**How many feeds.** One for each place on screen and way of showing, not one per kind of message: channels decide which feed shows what. Most games need three to five, such as a look-at prompt and pickup toasts in the HUD, a hint banner and a heading in the root layout, each showing its own channels. A channel two feeds show at once shows twice, so give each channel to one feed on screen at a time: hints in the root layout only, not in the HUD as well.

A message only says something; what's highlighted on a menu belongs in its [description line](#41-the-description-line), which follows focus and goes with the menu.

**A look-at banner, step by step.** What the player looks at, such as an item's icon and name over the key that takes it, while a world marker shows where it is:

1. Make a Widget Blueprint with parent **Modular Message Feed**. Set **Channels** to `UI.Message.Prompt`, **Mode** to Replace, **Default Duration** to 0, and leave Entry Class empty.
1. Lay it out: a **`MessagePanel`**, such as a Border with a dark brush, holding an Image **`IconImage`**, a Text Block **`TitleText`** for the name, a Text Block **`MessageText`** for what interacting does, such as Take, and a Modular Input Key Widget **`KeyWidget`**. Each collapses while the message has nothing for it.
2. Set **Show Transition**, such as a 0.15 second fade sliding up 12 pixels, and tick **Hide Reverses Show**.
3. Place it in the HUD, a quarter of the way up from the bottom, centered.
4. From the interaction code, as the player starts looking at something that should have a banner, such as an item to pick up, send a message with Channel `UI.Message.Prompt`, a **Key** unique to that thing, **Title**, **Icon**, **Body** and **Input Action**, and **Duration** 0; as they look away, **Hide Message** with the same channel and key. Sending it again with the same key updates it, such as when the thing can't be taken any more. Send nothing for things whose world marker already says enough, such as doors and boxes.

**Recipes.**

| What | Channel | Feed |
| --- | --- | --- |
| Pickup toasts: "Green Herb ×2" in a corner | `UI.Message.Toast` | Stack, Entry Class with Icon, Title and Count, Merge Repeats, 3 s |
| A new objective as a large heading | `UI.Message.Heading` | Queue, no Entry Class, a big Title Text, a slow fade, 4 s |
| A short hint banner a quarter of the way down the screen, such as "It's locked." or a key hint | `UI.Message.Hint` | Replace, a strip with Message Text and Key Widget, 3 s |
| What the player looks at, a quarter of the way up from the bottom: the item's icon and name, and the key to take it | `UI.Message.Prompt` | Replace, Icon Image, Title Text and Key Widget, Default Duration 0. Send it with a Key as the player looks at the thing, and hide it by that key as they look away |

## 10. World markers

World markers are widgets that follow things in the world, such as an interaction prompt over a door or an objective marker.

1. Add a **Modular World Marker Manager Component** to the player controller. It holds the player's markers.
1. Place a **Modular World Marker Layer** in the HUD, filling the screen. It draws a widget for each visible marker, from a pool.
2. Make the marker widgets: Widget Blueprints with parent **Modular World Marker Widget**.
3. For each thing, make a **Modular World Marker** object, set its **Anchor** (a scene component, and a socket), **World Offset**, **Widget Class** and **Context Object**, and add it to the manager. **Set Visible**, **Set State Tag**, **Set Progress** and **Notify Event** drive its widget; **Unregister** removes it.

| Marker widget setting | Description |
| --- | --- |
| Marker Scale, Anchor Alignment, Screen Offset | How big it draws, which point of it sits on the anchor, and how far it's pushed from there in pixels. |
| Edge Mode | **None** slides off the screen with its anchor, **Fade** fades it out as it nears the edge (Edge Fade Start and End), **Clamp** pins it to the edge while its anchor is off screen, Edge Padding in from it, behind the camera too. |
| Fade In Time, Fade Out Time, `ShowAnimation` | How it appears and goes. |
| `EdgeArrow` | Points toward the anchor while pinned to the edge; with Show Edge Arrow On Screen, always. |

Its events, On Context Changed, On State Tag Changed, On Progress Changed, On Event Received and On Edge Clamp Changed, are where a marker widget reacts, such as showing a prompt while its thing is focused.

## 11. Keyboard navigation

By default Slate moves focus with Tab and the arrow keys, and Space presses the focused button like Enter. The focused widget takes those keys before any UI action bound to them, so an input action mapped to Tab, an arrow key or Space never sees them while they move focus or press buttons. **Project Settings › Plugins › Modular UI**, under **Navigation**, turns each off, for games where those keys mean something else:

| Setting | Off |
| --- | --- |
| Tab Navigation | Tab doesn't move focus, so it can open and close an inventory, or open an item's menu. |
| Arrow Key Navigation | The arrow keys don't move focus; the gamepad's D-pad and stick still do. |
| Space Bar Accepts | Space doesn't press the focused button. |

**Extra Back Keys**, under **Input** on the same page, are keys that go Back as well as Common UI's own, such as **Right Mouse Button** for a game where the right mouse button means Back, as the gamepad's Back button does: Common UI's Back takes one key per device. They go Back where Common UI's Back would: on the Modular Activatable Widget taking input, when it handles Back, never on one under a menu or a dialog. Whatever is under the cursor, or has focus, gets the press first: a widget that uses it still does, such as a quick slot that clears on a right-click, and only a press nothing used goes Back. A quick second press goes Back too, and **Consume Pointer Input** doesn't stop them. A screen needs something under the cursor that takes clicks, such as a background image, for a right-click on its empty space to reach it. Context menus and prompt dialogs have a **Right Click** setting of their own, whose Project Default follows this list ([6](#6-context-menus), [7](#7-prompts)). Where nothing handles Back, such as in gameplay under a HUD, the keys keep their own use, such as aiming. Like Back's own keys, they're fixed: they aren't input actions players rebind.

## 12. Pausing

The **Modular UI Pause Subsystem** keeps the world paused while anything has asked it to. Screens with **Pause Game** ask for you. From anywhere else, call **Add Pause Request** and **Remove Pause Request** with any object as the requester; a requester that's destroyed stops counting. **On Pause Changed** fires as the world pauses and starts again, such as to hide the HUD.

To give the world a look while it's paused, such as greyed out, set **Paused Look** in **Project Settings › Modular UI › Pause**, with its weight, priority and fade times: it fades in over the whole world as it pauses. For a post process of your own instead, add a **Post Process Component** to your player controller (Infinite Extent ticked, Enabled unticked, a high Priority) and pass it to **Set Paused Post Process** for the local controller.

`ModularUI.Pause.ForceLook` shows the look without pausing, to check it reaches the screen, and `ModularUI.Pause.Status` logs who is asking for the pause. The subsystem pauses through the first local player, so it suits single player games.

## 13. Example: a layered opening

An inventory panel that opens in waves: the items first, then a frosted glass panel behind them, then the details.

Layout, inside the screen's root Canvas Panel:

```
GlassPanel (Overlay, anchored to the right edge, full height)
├─ GlassBackdrop (Overlay)
│  ├─ Background Blur (Blur Strength 15)
│  ├─ Image: black, alpha 0.35
│  └─ Image: a soft radial vignette
├─ PanelEdges (Overlay): two thin, bright Images on its left and right edges
└─ Content (Vertical Box)
   ├─ ItemsGroup: the tabs and the item grid
   ├─ ItemName
   ├─ Markers
   ├─ DescriptionBox
   └─ EquipmentStrip
```

Open Reveal, all with **Linear** or **Ease Out** easing:

| Widget | Delay | Duration | Fade | Slide |
| --- | --- | --- | --- | --- |
| `ItemsGroup` | 0 | 0.05 | On | Off |
| `PanelEdges` | 0.08 | 0.03 | On | Off |
| `GlassBackdrop` | 0.10 | 0.05 | On | Off |
| `EquipmentStrip` | 0.12 | 0.05 | On | Off |
| `ItemName` | 0.20 | 0.13 | On | From Right, 20 |
| `Markers` | 0.27 | 0.03 | On | Off |
| `DescriptionBox` | 0.33 | 0.07 | On | From Right, 60 |

Then untick **Close Reverses Open**, so everything fades out together over Close Transition's 0.2 seconds, and tick **Pause Game** with a Paused Look set, so the world greys out behind the panel as it opens and comes back as it closes. Set the layer the screen opens on to a **Transition Duration** of 0, so its own fade doesn't play over the reveal.

## 14. C++ reference

| Type | Use |
| --- | --- |
| `UModularUISubsystem` | Layers. `UModularUISubsystem::Get(LocalPlayer)`, `PushWidgetToLayer<T>(Tag, Class, Init)`, `PushStreamedWidgetToLayer(Tag, SoftClass, bSuspendInput, Init, OnPushed)`, whose Init sets the widget up before it shows, such as the page a screen opens on, and `OnFindLayer()` for a root layout of your own. |
| `UModularRootLayout` | Modular UI's root layout. |
| `UModularActivatableWidget` | Base class for screens. Override `NativeOnCloseStarted`, `AddRevealParts` to add parts of your own to the reveal, `RegisterConfirmAction` to bind Confirm yourself, and `GetHint` and `GetDescriptionOf` for its description line. `RegisterInputAction` binds an input action, holds included; `HasConfirmAction` and `MakeConfirmBindArgs` bind Accept with Confirm Display Name. `CloseWidgetThen`, `CallWhenDrawn`, `SetTransitionOrigin`, `ShowDescription`, `ShowDescriptionFrom`, `ClearDescriptionFrom`, `SetHighlightedDescription`, `ClearHighlightedDescription`. |
| `UModularPageSwitcher` | Pages that change at once and come in with a transition. `SetPageTransitionsEnabled`, `OnPageShown`, and `OnPageChanging`, which fires while the page being left is still drawn. |
| `UModularContextMenuWidget` | Context menus. `ShowPrompt(Request, Callback)`, `PlaceMenu`, `HighlightEntry`, `DescribeHighlightedEntry`, `PassPressThrough`, `GetEntryDescription`, `FindDescriptionScreen`, `GetEntriesWidget`, `IsOnMenu`, `SlidesFromBehindAnchor`, `GetEffectiveRightClick`. |
| `FModularPromptRequest`, `FModularPromptView` | A question, and the named widgets that show one. |
| `UModularPromptWidget`, `UAsyncAction_ShowModularPrompt` | Prompt dialogs. `SetOnAnswered` takes a callback in C++. |
| `UModularMessageSubsystem`, `FModularMessage`, `UModularMessageFeed`, `UModularMessageEntryWidget` | Messages. A message's Payload and Input Action are kept alive while it's waiting, showing, or kept to come back, so an object passed only in a message is safe to show. |
| `UModularBoundActionBar` | The action bar that keeps its buttons. `RefreshActions`, `GetShownActions`. |
| `UModularBoundActionButton` | An action bar button that shows holds. `IsHoldAction`, `GetActionHoldTime`, and the event `OnRepresentedActionChanged`. |
| `UModularSegmentedMeter`, `SModularSegmentedMeter` | The segmented meter, and its Slate widget for Slate UIs. |
| `ModularUI::RegisterInputAction`, `ModularUI::MakeBindArgs`, `ModularUI::GetHoldTime`, `ModularUI::IsKeyForInputType`, `ModularUI::GetAcceptInputAction`, `ModularUI::MakeAcceptBindArgs`, `ModularUI::HasAcceptAction`, `ModularUI::IsEnhancedInputSupportEnabled` | Binding input actions, named by their Description and holding with a Hold trigger, which keys belong to an input method, and Common UI's Accept, in `ModularUIInput.h`. |
| `UModularUIHoldSubsystem` | One per local player: times holds for input actions with a Hold trigger, from that player's own keys, and sends their progress to the binding's action bar button. `UModularUIHoldSubsystem::Get(Widget)`, `StartHold(InputAction, ActionBinding, HoldSeconds, OnComplete)`. |
| `UModularKeyBindingList`, `UModularKeyBindingRow`, `UModularKeyBindingHeader`, `UModularKeyBindingSlot` | The keybinding menu. `Refresh`, `ResetAllToDefault`, `SaveKeys`, `StopWaiting`, `IsWaitingForKey`, `GetFirstSlot`, `GetDeviceMappings(MappingName)`, `ResetMapping(MappingName)`. |
| `UModularWorldMarker`, `UModularWorldMarkerManagerComponent`, `UModularWorldMarkerLayer`, `UModularWorldMarkerWidget` | World markers. |
| `FModularUITransition`, `FModularUITransitionPlayer` | Transition settings, and the player that plays them on a fade target and a move target. Call `Advance` each frame, such as from `FTSTicker`. A slide's `SlideUnit` is pixels, or the move target's own size. |
| `FModularUIRevealStep`, `FModularUIReveal` | A reveal's entries, and the player that runs them. `Init` finds the named widgets in a user widget; `Add` takes widgets directly. |
| `ModularUI::CallWhenDrawn`, `ModularUI::FindFocusedWidgetIn`, `ModularUI::FindWidgetByName`, `ModularUI::FindEnclosing<T>`, `ModularUI::FindDescriptionOwner` | Focus and widget helpers, in `ModularUIFocus.h`. |
| `ModularUIGameplayTags` | The layer and message channel tags. |
| `UModularUIPauseSubsystem` | Pause requests and the paused look. `UModularUIPauseSubsystem::Get(WorldContextObject)`. |

Automation tests are under **ModularUI** in the Session Frontend.

## 15. Design notes

- **Layers by tag, not by layout.** Widgets ask for a layer by its tag, so the same screen opens on Modular UI's root layout, CommonGame's, or a stack of your own, and a plugin can open dialogs without knowing your game.
- **Code-driven transitions next to widget animations.** Transitions know things an animation can't, such as which side of its anchor a menu opened on, and many widgets share them through their base class; animations are better for everything artists tweak by eye. Both play from the same open and close, and a close waits for both.
- **Right-click is a choice.** Console-style games treat the right mouse button as Back; desktop games open menus with it. Context menus and prompts let each game decide, rather than one guessing for all, and their Project Default follows Extra Back Keys, so a game decides once.
- **Prompts are where the game wants them.** Inline keeps the player's eyes on the menu; an overlay or a dialog stands out for a question that matters. One request shows the same wherever it's asked.
- **Messages decouple saying from showing.** Gameplay sends to a channel; the HUD decides whether that's a toast, a banner or a heading, and can change its mind without touching gameplay.
- **Descriptions follow what's highlighted; messages come and go.** A menu's description line is state, kept by the menu and following focus and the mouse, so it's right as menus open over one another and close again. A message is an event, with its own time on screen.
- **The description line says what to do; details say what things are.** The line is about the player's next action: what an option does, or what the game is waiting for. Repeating what a details panel already shows only adds noise, so a screen being browsed says nothing.
- **Action bars change only what changed.** Common UI's bar rebuilds every button whenever anything is bound or unbound; a bar that keeps the buttons of actions still shown, and waits a moment for bindings to settle, never blinks as menus open over one another.
- **Every key is rebindable, and set once.** Every key a widget binds is an Enhanced Input action. Its keys live in an Input Mapping Context, not on the widget, so a rebind reaches every menu, action bar and key glyph at once, and no key is written down twice, as it would be in a data table row and a mapping. Accept and Back stay Common UI's, as Slate's navigation and Common UI's back handling own them.
- **Action bars show modes, not targets.** An action bar that changes as the cursor moves draws the eye away from what the player is doing. Bindings change as the player's mode does, such as when a menu opens, and an action that can't be done for what's aimed at does nothing.
- **Pages switch at once and settle in.** The new page is there the moment the player asks for it, focus goes back to it straight away, and a short transition only says which way they went.
- **Real time.** Everything that animates runs from the core ticker, so it plays while the game is paused.
