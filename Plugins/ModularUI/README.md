# Modular UI

Common UI building blocks for any Unreal Engine project: screens that come in and go with transitions, layered openings where each part arrives on its own beat, and pause requests that fade a post process over the world while a menu is open.

## Contents

1. [Overview](#1-overview)
2. [Installation](#2-installation)
3. [Screens](#3-screens)
4. [Reveals](#4-reveals)
5. [Transitions](#5-transitions)
6. [Pausing](#6-pausing)
7. [Example: a layered opening](#7-example-a-layered-opening)
8. [C++ reference](#8-c-reference)

---

## 1. Overview

| Piece | What it does |
| --- | --- |
| **Modular Activatable Widget** | A Common Activatable Widget that plays its Open Reveal and an OpenAnimation as it opens, plays them back out before it closes, and can pause the game while it's open. |
| **Reveal** | A list of named widgets, each with its own transition and delay, so a screen's parts arrive one after another and leave together. |
| **Transition** | A fade, a slide and a growth, each optional, with a delay, a duration and easing or a curve. |
| **Pause subsystem** | Pauses the world while anything asks, and fades a post process in while it's paused, such as one that greys the world out. |

Everything runs from the core ticker in real time, so it plays while the game is paused. Timers, timelines and Delay nodes all wait for the world, so they can't animate anything a pause menu shows.

## 2. Installation

1. Copy the `ModularUI` folder into your project's `Plugins` folder, so you have `YourProject/Plugins/ModularUI/ModularUI.uplugin`.
2. If your C++ uses it, add `"ModularUI"` to your module's `PublicDependencyModuleNames`.
3. Close the editor, regenerate project files and build the **Development Editor** configuration from your IDE.

It needs Common UI, which it turns on for you. It doesn't depend on any game or other plugin of yours.

## 3. Screens

Make your screens Widget Blueprints whose parent is **Modular Activatable Widget**, or a C++ class based on it.

| Setting | Description |
| --- | --- |
| Pause Game | The world stops while the screen is open. It starts again as the close begins, so the world comes back while the screen goes. |
| Open Reveal | The parts that come in as the screen opens. See [Reveals](#4-reveals). |
| Close Reverses Open | Closing sends every part back the way it came, all at once, and plays OpenAnimation backwards unless there's a CloseAnimation. |
| Close Transition | With Close Reverses Open off, how every part goes, from wherever it is. A 0.2 second fade by default. |
| Open Sound, Close Sound | Played as the screen opens, and as its close starts. |

Name a widget animation **OpenAnimation** and it plays as the screen opens. Name one **CloseAnimation** and it plays as the screen closes.

**Closing.** Call **Close Widget**, not Deactivate Widget, so the close plays. Back does the same. While the close plays, the screen takes no clicks or keys, **Is Closing** is true, and **On Close Started** fires so you can unbind anything that shouldn't work meanwhile. **Close Widget Now** cuts it short.

> Widgets reuse their instances, so each opening starts the reveal afresh from hidden. Leave the render opacity and render transform of revealed parts to the reveal: it adds to the values you set in the designer, but anything else animating them at the same time will fight it.

## 4. Reveals

Each entry of **Open Reveal** names a widget in the designer and says how it comes in:

| Field | Description |
| --- | --- |
| Widget | The widget's name, such as `ItemGrid`. Everything inside it comes in with it. None is the whole screen. |
| Transition | How it comes in. Give the parts different **Delay**s so they arrive one after another. |

A name that matches no widget is skipped, with a warning in the log. A widget named twice keeps its first entry.

Tips:

- Group widgets that move together under one panel, and reveal the panel.
- To bring in a **Background Blur**, reveal it with a fade. With **Apply Alpha to Blur** on, its default, the blur weakens with its opacity, so the frosted glass fades in with it.
- A reveal entry with a Duration of 0 and a Delay pops the widget in once the delay has passed.

## 5. Transitions

| Setting | Description |
| --- | --- |
| Delay | How long it waits before it starts. Coming in, the widget stays hidden meanwhile. |
| Duration | How long it takes once it starts. |
| Easing, Easing Strength | How it speeds up and slows down. Ease Out starts quick and settles. |
| Curve | Used instead of Easing. Rise past 1 before settling on it to overshoot. |
| Fade, Fade From | Fades in from this opacity. |
| Slide, Slide Distance, Slide From | Slides in from this many pixels away. **From Origin** slides out from where the widget opened, such as away from the item a menu opened beside; without an origin it slides up. |
| Scale, Scale From, Scale Origin | Grows from this fraction of its size, from its middle or from its corner nearest the origin. |

A transition always starts and ends exactly where it should, whatever the curve does on the way. Closing a widget that's still coming in sends it back from wherever it got to, faster the less of it was shown.

## 6. Pausing

The **Modular UI Pause Subsystem** keeps the world paused while anything has asked it to. Screens with **Pause Game** ask for you. From anywhere else, call **Add Pause Request** and **Remove Pause Request** with any object as the requester; a requester that's destroyed stops counting.

To grey the world out, or give it any other look, while it's paused:

1. Add a **Post Process Component** to your player controller. Tick **Infinite Extent (Unbound)**, untick **Enabled**, and give it a high **Priority**, such as 1000.
2. Set the look in its settings, such as **Color Saturation** at 0 and a lower **Gain**. Its **Blend Weight** is how strong the look gets.
3. In the controller's Begin Play, for the local controller, call the subsystem's **Set Paused Post Process** with the component and the fade times.

**On Pause Changed** fires as the world pauses and starts again, such as to hide the HUD.

The subsystem pauses through the first local player, so it suits single player games.

## 7. Example: a layered opening

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

Then untick **Close Reverses Open**, so everything fades out together over Close Transition's 0.2 seconds, and tick **Pause Game** with a paused post process set up as in [Pausing](#6-pausing), so the world greys out behind the panel as it opens and comes back as it closes.

Set the layer stack the screen opens on to a **Transition Duration** of 0, so its own fade doesn't play over the reveal.

## 8. C++ reference

| Type | Use |
| --- | --- |
| `UModularActivatableWidget` | Base class for screens. Override `NativeOnCloseStarted` to react to a close starting. |
| `FModularUITransition` | The transition settings, a `USTRUCT` for your own properties. |
| `FModularUITransitionPlayer` | Plays transitions on a fade target and a move target, such as a popup's root and its panel. `SetOrigin` gives From Origin its direction, such as away from what the popup opened beside. Call `Advance` each frame, such as from `FTSTicker`. |
| `FModularUIRevealStep`, `FModularUIReveal` | A reveal's entries, and the player that runs them. `Init` finds the named widgets in a user widget; `Add` takes widgets directly. |
| `UModularUIPauseSubsystem` | Pause requests and the paused post process. `UModularUIPauseSubsystem::Get(WorldContextObject)`. |

Automation tests are under **ModularUI** in the Session Frontend.
