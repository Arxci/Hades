# Hades Inventory: Editor Setup

This walks through everything the inventory needs inside the editor, from an empty `Content` folder to a playable test level. It assumes nothing exists yet. If a step describes something you already have, check that its settings match and move on.

At the end you will have:

- A 6 × 3 pocket grid that grows when you find hip pouches.
- A right hand and a left hand. Two-handed weapons empty the left hand. Equipped items leave the pockets.
- Four quick slots on keys 1–4 and the D-pad.
- A key item list that takes no pocket space. Keys are discarded on their own after their last door.
- An item menu (Use, Equip, Unequip, Combine, Discard, Store, Take) on right-click or a gamepad button.
- Linked item boxes, pickups that say "Not enough space", and a world that stops while the inventory is open.

Names in `code font` are exact. Type them exactly, because the C++ finds widgets by name.

---

## Part 0. Build and open

1. Close the editor.
2. Right-click `Hades.uproject` and choose **Generate Visual Studio project files**.
3. Open the solution, select the **Development Editor** configuration and the **Win64** platform, and build the **Hades** project.
4. Start the editor.
5. If the editor asks *"Asset Manager settings do not include an entry for assets of type GameFeatureData..."*, click **Yes**.
6. Open **Edit › Plugins** and check that these are enabled: **Modular Inventory**, **Common UI**, **Gameplay Abilities**, **Enhanced Input**, **Common Game**, **Common User**, **Async Mixin**, **Modular Gameplay Actors**, **UI Extension**. If you had to enable any, restart the editor.

---

## Part 1. Project foundation

The HUD, the inventory screen and the item menu are Common UI screens pushed onto layers. This part sets that up.

### 1.1 Default classes

1. Open **Edit › Project Settings**.
2. **Project › Maps & Modes › Game Instance**: set **Game Instance Class** to `HadesGameInstance`.
3. **Engine › General Settings › Default Classes**:
   - **Game Viewport Client Class**: `CommonGameViewportClient`
   - **Local Player Class**: `CommonLocalPlayer`
4. **Engine › Input › Default Classes**: check that **Default Player Input Class** is `EnhancedPlayerInput` and **Default Input Component Class** is `EnhancedInputComponent`.

### 1.2 Interaction collision channel

The C++ expects the interaction trace on the first custom trace channel.

1. **Engine › Collision › Trace Channels › New Trace Channel...**
2. **Name**: `Interaction`. **Default Response**: `Ignore`. Click **Accept**.
3. If you already had custom trace channels, `Interaction` must be the first one you ever created (`ECC_GameTraceChannel1`). Otherwise interactables won't be found.

### 1.3 Accept and Back for menus

1. In the Content Browser, create the folder `Content/Input`.
2. **Add › Miscellaneous › Data Table**. For **Pick Row Structure**, choose `CommonInputActionDataBase`. Click **OK**. Name it `DT_UIInputActions` and open it.
3. Click **Add** and rename the row to `Confirm`. Set:
   - **Display Name**: `Confirm`
   - **Keyboard Input Type Info › Key**: `Enter`
   - **Default Gamepad Input Type Info › Key**: `Gamepad Face Button Bottom`
4. Click **Add** again and rename the row to `Back`. Set:
   - **Display Name**: `Back`
   - **Keyboard Input Type Info › Key**: `Escape`
   - **Default Gamepad Input Type Info › Key**: `Gamepad Face Button Right`
5. Save.
6. **Add › Blueprint Class**. Under **All Classes**, search for `CommonUIInputData`, select it, and click **Select**. Name it `B_CommonInputData` and open it.
7. In **Class Defaults**:
   - **Default Click Action**: Data Table `DT_UIInputActions`, Row Name `Confirm`
   - **Default Back Action**: Data Table `DT_UIInputActions`, Row Name `Back`
8. Compile and save.
9. **Project Settings › Game › Common Input Settings › Input Data**: set it to `B_CommonInputData`.

### 1.4 UI actions

The screens bind these by tag. The tags already exist (they are defined in C++); this gives them keys.

Open **Project Settings › Game › Common UI Input Settings**. Under **Input Actions**, click **+** once per row below. In each new element, set **Action Tag**, **Default Display Name**, and add one **Key Mappings** element per key, setting its **Key**.

| Action Tag | Default Display Name | Key Mappings |
| --- | --- | --- |
| `UI.Action.Escape` | Pause | `Escape`, `Gamepad Special Right` |
| `UI.Action.Inventory` | Inventory | `Tab`, `Gamepad Special Left` |
| `UI.Action.RotateItem` | Rotate | `R`, `Gamepad Face Button Top` |
| `UI.Action.SplitStack` | Split | `Left Shift`, `Gamepad Left Shoulder` |
| `UI.Action.ItemMenu` | Item Menu | `E`, `Gamepad Face Button Left` |
| `UI.Action.Organize` | Organize | `O`, `Gamepad Left Thumbstick Button` |

### 1.5 Root layout

1. Create the folder `Content/UI/Foundation`.
2. **Add › User Interface › Widget Blueprint**. In **All Classes**, search for `PrimaryGameLayout` and select it. Name it `W_PrimaryGameLayout` and open it.
3. In **Designer**, from the **Palette**, drag an **Overlay** onto the root.
4. Drag four **Common Activatable Widget Stack** widgets into the Overlay, in this order:
   1. `GameLayer_Stack`
   2. `GameMenuLayer_Stack`
   3. `MenuLayer_Stack`
   4. `ModalLayer_Stack`
5. For each stack: rename it as listed, tick **Is Variable** at the top of the Details panel, and set its slot's **Horizontal Alignment** and **Vertical Alignment** to **Fill**.
6. Open **Graph**. Right-click and add **Event On Initialized**. Chain four **Register Layer** calls after it:

   | Layer Tag | Layer Widget |
   | --- | --- |
   | `UI.Layer.Game` | `GameLayer_Stack` |
   | `UI.Layer.GameMenu` | `GameMenuLayer_Stack` |
   | `UI.Layer.Menu` | `MenuLayer_Stack` |
   | `UI.Layer.Modal` | `ModalLayer_Stack` |

7. Compile and save.
8. **Add › Blueprint Class**, search for `GameUIPolicy`, and select it. Name it `B_HadesUIPolicy`. In **Class Defaults**, set **Layout Class** to `W_PrimaryGameLayout`. Compile and save.
9. Close the editor. Open `Config/DefaultGame.ini` in a text editor and add these two lines at the end:

   ```ini
   [/Script/Hades.HadesUIManagerSubsystem]
   DefaultUIPolicyClass=/Game/UI/Foundation/B_HadesUIPolicy.B_HadesUIPolicy_C
   ```

   If you saved `B_HadesUIPolicy` somewhere else, change the path to match. The part after the dot is the asset name followed by `_C`.
10. Start the editor again.

### 1.6 A button and a pause menu

Every menu uses this button.

1. **Add › Blueprint Class**, search for `CommonButtonStyle`, and select it. Name it `B_ButtonStyle` and open it. In **Class Defaults**, set the brushes. Set each **Draw As** to **Rounded Box**:
   - **Normal Base › Tint**: R 0.05, G 0.05, B 0.05, A 0.85
   - **Normal Hovered › Tint**: R 0.25, G 0.25, B 0.25, A 0.9
   - **Normal Pressed › Tint**: R 0.4, G 0.4, B 0.4, A 1
   - **Disabled › Tint**: R 0.05, G 0.05, B 0.05, A 0.4

   Compile and save.
2. **Add › User Interface › Widget Blueprint**, search for `CommonButtonBase`, and select it. Name it `W_MenuButton` and open it.
   - Add a **Common Text** (or a **Text**) to the root and name it `Label`. Set its slot's padding to 12 left/right and 6 top/bottom.
   - In **My Blueprint › Variables**, add a variable `LabelText` of type **Text**. Tick **Instance Editable**.
   - In **Graph**, from **Event Pre Construct**, call **Label › Set Text** with `LabelText`.
   - In **Class Defaults**, set **Style** to `B_ButtonStyle`.
   - Compile and save.
3. **Add › User Interface › Widget Blueprint**, search for `HadesActivatableWidget`, and select it. Name it `W_EscapeMenu` and open it.
   - Root: an **Overlay**. Inside it, add a **Border** with **Brush Color** A 0.6 black, Fill/Fill. Then add a **Vertical Box**, Center/Center, holding a **Text** "Paused" and a `W_MenuButton` named `ResumeButton` with **Label Text** "Resume".
   - In **Class Defaults**: **Input Config** `Menu`, **Pause Game** ticked, **Is Back Handler** ticked.
   - In **Graph**: select `ResumeButton` and click **+** next to **On Clicked** in its Details. From the event, call **Deactivate Widget**.
   - Override **Get Desired Focus Target** (**Functions › Override**) and return `ResumeButton`.
   - Compile and save.

---

## Part 2. Input

### 2.1 Input actions

In `Content/Input`, use **Add › Input › Input Action** for each:

| Asset | Value Type |
| --- | --- |
| `IA_Move` | Axis2D (Vector2D) |
| `IA_Look` | Axis2D (Vector2D) |
| `IA_Jump` | Digital (bool) |
| `IA_Interact` | Digital (bool) |
| `IA_QuickSlot1` | Digital (bool) |
| `IA_QuickSlot2` | Digital (bool) |
| `IA_QuickSlot3` | Digital (bool) |
| `IA_QuickSlot4` | Digital (bool) |

### 2.2 Mapping context

**Add › Input › Input Mapping Context**, named `IMC_Default`. Open it. For each row, click **+** next to **Mappings**, pick the action, then set its keys. Use the **+** beside the action for a second key, and the **Modifiers** array under each key.

| Action | Key | Modifiers |
| --- | --- | --- |
| `IA_Move` | `W` | Swizzle Input Axis Values (Order `YXZ`) |
| | `S` | Swizzle Input Axis Values (`YXZ`), then Negate |
| | `A` | Negate |
| | `D` | none |
| | `Gamepad Left Thumbstick 2D-Axis` | Dead Zone |
| `IA_Look` | `Mouse XY 2D-Axis` | Negate (untick X and Z, keep Y) |
| | `Gamepad Right Thumbstick 2D-Axis` | Dead Zone, then Negate (untick X and Z, keep Y) |
| `IA_Jump` | `Space Bar`, `Gamepad Face Button Bottom` | none |
| `IA_Interact` | `F`, `Gamepad Face Button Left` | none |
| `IA_QuickSlot1` | `One`, `Gamepad D-pad Up` | none |
| `IA_QuickSlot2` | `Two`, `Gamepad D-pad Right` | none |
| `IA_QuickSlot3` | `Three`, `Gamepad D-pad Down` | none |
| `IA_QuickSlot4` | `Four`, `Gamepad D-pad Left` | none |

If up and down feel inverted, tick or untick **Y** on that Negate modifier.

### 2.3 Input config

1. **Add › Miscellaneous › Data Asset**, choose `HadesInputConfig`, name it `DA_HeroInputConfig`, and open it.
2. Add one **Input Actions** element per row:

| Input Tag | Input Action |
| --- | --- |
| `Input.Move` | `IA_Move` |
| `Input.Look` | `IA_Look` |
| `Input.Jump` | `IA_Jump` |
| `Input.Interact` | `IA_Interact` |
| `Input.QuickSlot.1` | `IA_QuickSlot1` |
| `Input.QuickSlot.2` | `IA_QuickSlot2` |
| `Input.QuickSlot.3` | `IA_QuickSlot3` |
| `Input.QuickSlot.4` | `IA_QuickSlot4` |

3. Save.

---

## Part 3. Asset Manager

1. Choose **Tools › Modular Inventory › Register Items With Asset Manager**. This lets `Inventory.Give` find items and makes sure every item is cooked.
2. Restart the editor if it asks.

No other gameplay tags need adding: `Container.*`, `Equipment.Slot.Hand.Right/Left`, `Item.Type.KeyItem`, `Item.Type.Pouch`, `Item.Stat.Uses`, `Input.QuickSlot.*` and the `UI.*` tags are defined in C++.

---

## Part 4. Container layouts

Create `Content/Inventory/Layouts`. In it, create each layout with **Add › Inventory › Container Layout**.

**Editing a filter.** A layout's **Filter** has a **Tag Query**. Click the Tag Query field to open the query editor. Set the root expression type to the one listed below, add the tag to that expression, and confirm.

| Asset | Type | Settings | Filter › Tag Query |
| --- | --- | --- | --- |
| `CL_Pockets` | Grid | Grid Size X 6, Y 3 | No Tags Match: `Item.Type.KeyItem` |
| `CL_Pockets_2` | Grid | Grid Size 6 × 4 | No Tags Match: `Item.Type.KeyItem` |
| `CL_Pockets_3` | Grid | Grid Size 7 × 4 | No Tags Match: `Item.Type.KeyItem` |
| `CL_Pockets_4` | Grid | Grid Size 8 × 4 | No Tags Match: `Item.Type.KeyItem` |
| `CL_KeyItems` | List | Capacity 0 | Any Tags Match: `Item.Type.KeyItem` |
| `CL_Hands` | Slots | **Equipment** ticked. Slots: element 0 Tag `Equipment.Slot.Hand.Right`, Display Name "Right Hand"; element 1 Tag `Equipment.Slot.Hand.Left`, Display Name "Left Hand" | empty |
| `CL_ItemBox` | List | Capacity 0 | No Tags Match: `Item.Type.KeyItem` |
| `CL_Pickup` | List | Capacity 0 | empty |

Why these:
- The pockets refuse key items, so keys never take grid space. The key item list only takes key items, so new keys go there by themselves.
- Each pocket upgrade contains the one before it (6 × 3 fits inside 6 × 4, and so on), because items keep their cells when the pockets grow.
- The item box refuses key items, so a key is never left in a box. Keys don't use space, so nobody needs to store them.
- Lists with Capacity 0 are unlimited, like the item box in RE7 and the RE2 remake.

---

## Part 5. Rules, effects, items and recipes

### 5.1 Stack and equip rules

Create `Content/Inventory/Rules`.

**Add › Inventory › Stack Rules**:

| Asset | Max Stack Size | Can Split |
| --- | --- | --- |
| `SR_HandgunAmmo` | 30 | ticked |

**Add › Inventory › Equip Rules**:

| Asset | Slots | Blocked Slots |
| --- | --- | --- |
| `ER_OneHanded` | `Equipment.Slot.Hand.Right` | empty |
| `ER_TwoHanded` | `Equipment.Slot.Hand.Right` | `Equipment.Slot.Hand.Left` |
| `ER_Light` | `Equipment.Slot.Hand.Left` | empty |

A knife or tool that fits either hand would use Slots `Equipment.Slot.Hand`. It goes into whichever hand is empty.

### 5.2 Healing effects

The project has no Health attribute yet, so these do nothing for now. Once you have one, add a modifier for it.

Create `Content/Inventory/Effects`. For each, use **Add › Blueprint Class**, search for `GameplayEffect`, and select it. Set **Duration Policy** to **Instant**.

- `GE_Heal_Small` (green herb)
- `GE_Heal_Medium` (two green herbs)
- `GE_Heal_Full` (green + red)

### 5.3 Item meshes

Each item needs a World Mesh (for pickups) and an icon (for the UI). The plugin renders icons from meshes.

1. Create `Content/Inventory/Items`.
2. If you have no meshes yet: in the Content Browser, click **Settings** and tick **Show Engine Content**. Open `Engine/BasicShapes`. Drag `Cube` into `Content/Inventory/Items` and choose **Copy Here**. Do the same with `Cylinder`.
3. For each item below, right-click a mesh and choose **Create Inventory Item**, then rename the new `DA_...` and `DA_..._Cosmetics` assets to the item's names below, such as `DA_Pistol` and `DA_Pistol_Cosmetics`. One mesh can make several items: each run makes new assets with a number added to the name.

### 5.4 Item definitions

Open each item and set these fields. Leave the rest at their defaults.

| Asset | Display Name | Size (X × Y) | Stack Rules | Equip Rules | Tags | Tag Values |
| --- | --- | --- | --- | --- | --- | --- |
| `DA_Pistol` | Pistol | 2 × 1 | | `ER_OneHanded` | | |
| `DA_Shotgun` | Shotgun | 4 × 1 | | `ER_TwoHanded` | | |
| `DA_Flashlight` | Flashlight | 2 × 1 | | `ER_Light` | | |
| `DA_HandgunAmmo` | Handgun Ammo | 1 × 1 | `SR_HandgunAmmo` | | | |
| `DA_GreenHerb` | Green Herb | 1 × 1 | | | | |
| `DA_RedHerb` | Red Herb | 1 × 1 | | | | |
| `DA_MixedHerbGG` | Mixed Herb (G+G) | 1 × 1 | | | | |
| `DA_MixedHerbGR` | Mixed Herb (G+R) | 1 × 1 | | | | |
| `DA_Gunpowder` | Gunpowder | 1 × 1 | | | | |
| `DA_BasementKey` | Basement Key | 1 × 1 | | | `Item.Type.KeyItem` | `Item.Stat.Uses` = 2 |
| `DA_HipPouch` | Hip Pouch | 1 × 1 | | | `Item.Type.Pouch` | |

Then:

1. **Usable items.** In `DA_GreenHerb`, under **Fragments**, click **+**, open the element's struct picker, and choose **Hades Item Use**. Set **Effect** to `GE_Heal_Small`. Do the same in `DA_MixedHerbGG` with `GE_Heal_Medium` and in `DA_MixedHerbGR` with `GE_Heal_Full`. The red herb has no use on its own; it is only for combining.
2. **Descriptions.** Give each item a **Description**. It shows in the details panel.
3. **Icons.** Select all the item definitions, right-click, and choose **Generate Item Icons**. This makes each icon the right shape for the item's size. Do it again whenever you change an item's Size.
4. Save all.

Herbs and gunpowder don't stack, as in the RE games. Combine needs two separate items to pick from.

`Item.Stat.Uses = 2` means the basement key opens two locks before it is discarded. A key with no `Item.Stat.Uses` is never discarded.

### 5.5 Combine recipes

Create `Content/Inventory/Recipes`. Use **Add › Inventory › Recipe** for each:

**`RCP_MixedHerbGG`**
- Display Name: Mixed Herb (G+G)
- Ingredients: element 0: **Query › Definitions** `DA_GreenHerb`, **Count** 2, **Consume** ticked
- Outputs: element 0: **Definition** `DA_MixedHerbGG`, **Count** 1

**`RCP_MixedHerbGR`**
- Ingredients: element 0: Definitions `DA_GreenHerb`, Count 1. Element 1: Definitions `DA_RedHerb`, Count 1.
- Outputs: `DA_MixedHerbGR` × 1

**`RCP_HandgunAmmo`**
- Ingredients: element 0: Definitions `DA_Gunpowder`, Count 2
- Outputs: `DA_HandgunAmmo` × 15

Then **Add › Inventory › Recipe Book**, named `RB_Combine`, with **Recipes** `RCP_MixedHerbGG`, `RCP_MixedHerbGR` and `RCP_HandgunAmmo`.

Combine works with recipes of one or two ingredients. "Green + Red + Yellow" is done in two steps, as in the RE games: G+R, then the result + Y.

---

## Part 6. Actors

Create `Content/Inventory/Actors`.

### 6.1 The flashlight's light

1. **Add › Blueprint Class › Actor**, named `BP_FlashlightLight`.
2. Add a **Spot Light** component. Set **Intensity** 50 (cd), **Attenuation Radius** 2500, **Outer Cone Angle** 25, **Inner Cone Angle** 12.
3. Compile and save.
4. Open `DA_Flashlight_Cosmetics`. Under **Pieces**, click **+** and choose **Actor**. Set:
   - **Actor Class**: `BP_FlashlightLight`
   - **Attach Component**: `FirstPersonCamera`
   - **Relative Transform › Location**: X 10, Y -15, Z -10

### 6.2 Visible weapons (optional for testing)

In `DA_Pistol_Cosmetics` and `DA_Shotgun_Cosmetics`, add a **Static Mesh** piece with **Mesh** set to the item's mesh, **Attach Component** `FirstPersonCamera`, **Relative Transform › Location** X 40, Y 15, Z -15 (scale it down to 0.2 if it's a basic shape), and **First Person Primitive Type** **First Person**. Once you have arms, attach to `ArmsMesh` and a hand socket instead.

### 6.3 Pickup

1. **Add › Blueprint Class**, search for `HadesItemPickup`, select it, and name it `BP_ItemPickup`. Open it.
2. Select the **Inventory** component and set **Containers**: element 0, Tag `Container.Pickup`, Layout `CL_Pickup`.
3. Select the **Interactable** component and set **Marker Widget Class** to `W_InteractableMarker` (made in Part 8). Leave **Interaction Duration** at 0. **Disabled Text** is already "Not enough space".
4. Compile and save.

Discarded items are dropped in a `BP_ItemPickup`, and the same class is used for items placed in levels.

### 6.4 Item box

1. **Add › Blueprint Class**, search for `HadesItemBox`, and name it `BP_ItemBox`. Open it.
2. Select **Mesh** and set **Static Mesh** to any box, such as `Cube`. Set the scale to X 1, Y 1.5, Z 0.8.
3. Select **Interactable** and set **Marker Widget Class** to `W_InteractableMarker`.
4. In **Class Defaults**, set **Screen Class** to `W_ItemBoxScreen` (made in Part 8).
5. Compile and save.

### 6.5 Locked door

1. **Add › Blueprint Class › Actor**, named `BP_LockedDoor`. Open it.
2. Add a **Static Mesh** component and drag it onto **DefaultSceneRoot** in the Components panel, so it becomes the root. Set its mesh to `Cube`, scaled to X 0.2, Y 1.5, Z 2.5. The interactable uses the root as what the player looks at.
3. Add a **Hades Interactable Component**. Set **Interaction Text** to "Open" and **Marker Widget Class** to `W_InteractableMarker`.
4. Add a **Hades Lock Component**. Set **Key** to `DA_BasementKey`.
5. In the Event Graph: select the Hades Interactable Component, and in its Details under **Events**, click **+** next to **On Interaction Completed**. From that event: drag in the Hades Lock Component, call **Try Unlock** with the event's **Interactor**, add a **Branch** on its return value, and on **True** call **Destroy Actor**.
6. Compile and save.

The lock tells the player "It's locked." or "Used the Basement Key." by itself. On the key's last use, it removes the key and says so.

---

## Part 7. Player controller, hero and game mode

Create `Content/Core`.

### 7.1 Player controller

1. **Add › Blueprint Class**, search for `HadesPlayerController`, and name it `BP_HadesPlayerController`. Open it.
2. In **Class Defaults**:
   - **Default Mapping Contexts**: add `IMC_Default`
   - **HUD Layout Class**: `W_HUD` (made in Part 8)
3. Select the **Item Box Inventory** component and set **Containers**: element 0, Tag `Container.ItemBox`, Layout `CL_ItemBox`.
4. Compile and save.

Keeping the box's storage on the controller makes it a single storage, so every item box in the level shows the same items.

### 7.2 Hero

1. **Add › Blueprint Class**, search for `HadesHeroCharacter`, and name it `BP_HadesHero`. Open it.
2. In **Class Defaults**, set **Input Config** to `DA_HeroInputConfig`.
3. Select the **Inventory Component** (it is a Hades Inventory Component) and set:

   **Inventory**
   - **Containers**, in this order:
     1. Tag `Container.Pockets`, Layout `CL_Pockets`
     2. Tag `Container.KeyItems`, Layout `CL_KeyItems`
     3. Tag `Container.Hands`, Layout `CL_Hands`
   - **Preload Cosmetics**: ticked

   **Inventory › Starting Items**
   - **Starting Items**: `DA_Pistol` Count 1, `DA_HandgunAmmo` Count 12, `DA_GreenHerb` Count 1, `DA_RedHerb` Count 1
   - **Starting Equipment**: Slot `Equipment.Slot.Hand.Left`, Item › Definition `DA_Flashlight`

   **Inventory › Hades**
   - **Dropped Pickup Class**: `BP_ItemPickup`
   - **Combine Recipes**: `RB_Combine`
   - **Pocket Upgrades**: `CL_Pockets_2`, `CL_Pockets_3`, `CL_Pockets_4`
4. Select **Quick Slot Component**. Leave **Num Slots** at 4.
5. Compile and save.

The pockets come first in the container list, so new items fill the grid first. Key items skip it because of the filter.

### 7.3 Game mode

1. **Add › Blueprint Class**, search for `HadesGameMode`, and name it `BP_HadesGameMode`.
2. In **Class Defaults**: **Default Pawn Class** `BP_HadesHero`, **Player Controller Class** `BP_HadesPlayerController`.
3. Compile and save.
4. **Project Settings › Maps & Modes › Default GameMode**: `BP_HadesGameMode`.

---

## Part 8. The UI

Create `Content/UI/HUD` and `Content/UI/Inventory`. For each widget, use **Add › User Interface › Widget Blueprint**, search **All Classes** for the parent class, select it, and name it as given.

Unless a step says otherwise, a slot's alignment is set in the Details panel under **Slot**. "Fill/Fill" means Horizontal and Vertical Alignment both Fill.

### 8.1 `W_InteractableMarker` (parent `HadesInteractableMarkerWidget`, in UI/HUD)

1. Root: an **Overlay**.
2. Inside it, add an **Image** named `Indicator`. Size 12 × 12 (**Brush › Image Size**), white, Center/Center.
3. Inside the Overlay, add a **Horizontal Box** named `Prompt`, Center/Center. Inside it, add a **Text** named `ActionText`, font size 18, with a black **Shadow Offset** of 1, 1.
4. **Class Defaults › World Marker › Screen › Anchor Alignment**: X 0.5, Y 1.
5. Compile and save.

The prompt shows "Take Shotgun", or "Not enough space" when the player has no room. Without **Indicator** and **Prompt** the widget won't compile.

### 8.2 `W_Message` (parent `HadesMessageWidget`, in UI/HUD)

1. Root: a **Text** named `MessageText`, font size 20, justified **Center**, with a black shadow.
2. In the **Animations** panel, click **+ Animation** and name it exactly `ShowAnimation`. Select it.
3. Click **+ Track** and choose `MessageText`. On the `MessageText` track, click **+ Track** and choose **Render Opacity**.
4. Add keys on Render Opacity: time 0.0 → 0, 0.2 → 1, 2.5 → 1, 3.0 → 0.
5. Compile and save.

Each message plays this from the start. Widget animations run while the game is paused, so the fade works in the inventory too.

### 8.3 `W_InventoryItem` (parent `InventoryItemWidget`, in UI/Inventory)

1. Root: an **Overlay**.
2. Inside it, an **Image** named `Icon`, with **Horizontal Alignment Center** and **Vertical Alignment Center**. Not Fill: grids size the icon to the item's cells and turn it with the item.
3. Inside the Overlay, a **Text** named `CountText`, Right/Bottom, font size 14, with a black shadow.
4. **Graph**:
   - Add **Event On Held Changed**. **Set Render Opacity** on self: 0.35 when **Is Held** is true, else 1. Use a **Select Float** node.
   - Add **Event On Item Changed**. Call **Get Definition**, then **Is Valid**. If valid, read the definition's **Size**, convert it with **To Vector2D (IntPoint)**, multiply by 32, and call **Set Image Brush Size** (from Hades UI Utility) with Target `Icon`. This keeps long items in proportion outside grids, such as a shotgun in a quick slot.
5. Compile and save.

### 8.4 `W_InventoryCell` (parent `InventoryCellWidget`)

1. Leave the designer empty.
2. **Class Defaults › Style**: `B_ButtonStyle`.
3. Compile and save.

### 8.5 `W_InventoryGrid` (parent `InventoryGridWidget`)

1. Root: a **Canvas Panel** named `GridCanvas`. It must be the root and keep that name.
2. Inside it, a **Border** named `PlacementPreview`, with **Brush Color** white and A 0.35. Its position and size don't matter.
3. **Class Defaults**:
   - **Container Tag**: `Container.Pockets`
   - **Cell Size**: 64
   - **Cell Class**: `W_InventoryCell`
   - **Item Class**: `W_InventoryItem`
4. **Graph**: add **Event On Placement Preview Changed**, then **Switch on EInventoryDropAction**. On each output, call **PlacementPreview › Set Brush Color**:

   | Output | Color (R, G, B, A) |
   | --- | --- |
   | Place | 0.2, 0.8, 0.2, 0.35 |
   | Merge | 0.2, 0.4, 1.0, 0.35 |
   | Swap | 1.0, 0.8, 0.1, 0.35 |
   | Invalid | 1.0, 0.1, 0.1, 0.35 |

5. Compile and save.

### 8.6 `W_InventorySlot` (parent `InventorySlotWidget`)

This one widget serves the hands, the key item list and the item box list.

1. Root: a **Size Box**, Width Override 64, Height Override 64.
2. Inside it, a **Scale Box** with **Stretch** **Scale to Fit**.
3. Inside the Scale Box, a `W_InventoryItem` named `ItemWidget`.
4. **Class Defaults › Style**: `B_ButtonStyle`. Leave **Container Tag** and **Slot Tag** empty here; they are set where the widget is placed.
5. Compile and save.

### 8.7 `W_InventoryList` (parent `InventoryListWidget`)

1. Root: a **Scroll Box**.
2. Inside it, a **Wrap Box** named `EntryPanel`. Set **Inner Slot Padding** to X 4, Y 4, tick **Explicit Wrap Size**, and set **Wrap Size** to 352.
3. **Class Defaults › Entry Class**: `W_InventorySlot`. Leave **Container Tag** empty; it is set where the widget is placed.
4. Compile and save.

### 8.8 `W_ItemDetails` (parent `InventoryItemDetailsWidget`)

1. Root: a **Vertical Box**.
2. Inside it, in order:
   - an **Image** named `Icon`, with **Brush › Image Size** 128 × 128, Center alignment
   - a **Text** named `NameText`, font size 22
   - a **Text** named `DescriptionText`, font size 14, **Auto Wrap Text** ticked, **Wrap Text At** 320
   - a **Text** named `CountText`, font size 14
3. **Class Defaults**: **Follow Hovered Item** ticked, **Collapse Without Item** ticked.
4. Compile and save.

### 8.9 `W_QuickSlot` (parent `HadesQuickSlotWidget`)

1. Root: an **Overlay**.
2. Inside it, a **Size Box** (64 × 64) holding a **Scale Box** (**Scale to Fit**), holding a `W_InventoryItem` named `ItemWidget`.
3. Inside the Overlay, a **Text** named `NumberText`, Left/Top, font size 12.
4. **Class Defaults › Style**: `B_ButtonStyle`.
5. **Graph**: add **Event On In Hand Changed**. **Set Render Opacity** on self: 1 when **In Hand** is true, else 0.6.
6. Compile and save.

### 8.10 `W_ItemMenu` (parent `HadesItemMenu`)

1. Root: an **Overlay**.
2. Inside it, a **Border** with **Brush Color** A 0.25 black, Fill/Fill, **Visibility** **Visible**. It blocks clicks on the inventory behind the menu.
3. Inside the Overlay, a **Vertical Box**, Right/Center, with slot padding 0 right 420.
4. Inside the Vertical Box, a **Text** named `ItemNameText`, font size 20.
5. Below it, seven `W_MenuButton`s. Name each exactly as below, set its **Label Text**, and give it bottom padding 4:

   | Name | Label Text |
   | --- | --- |
   | `UseButton` | Use |
   | `EquipButton` | Equip |
   | `UnequipButton` | Unequip |
   | `CombineButton` | Combine |
   | `DiscardButton` | Discard |
   | `StoreButton` | Store |
   | `TakeButton` | Take |

6. Compile and save.

Each button only shows when its action can run. Back closes the menu.

### 8.11 `W_InventoryScreen` (parent `HadesInventoryScreen`)

1. Root: an **Overlay**.
2. Inside it, a **Border**, Fill/Fill, **Brush Color** R 0, G 0, B 0, A 0.85.
3. Inside the Overlay, a **Horizontal Box**, Center/Center. In it, three **Vertical Box** columns, each with slot padding 24 and **Size** **Auto**:

   **Left column**
   - **Text** "HANDS"
   - **Horizontal Box** holding two `W_InventorySlot`s:
     - `RightHandSlot`: **Container Tag** `Container.Hands`, **Slot Tag** `Equipment.Slot.Hand.Right`
     - `LeftHandSlot`: **Container Tag** `Container.Hands`, **Slot Tag** `Equipment.Slot.Hand.Left`
   - **Text** "QUICK SLOTS"
   - **Horizontal Box** holding four `W_QuickSlot`s with **Slot Index** 0, 1, 2 and 3

   **Middle column**
   - **Text** "POCKETS"
   - `W_InventoryGrid` named exactly `MainGrid`, with slot **Size** **Auto**

   **Right column**
   - `W_ItemDetails`
   - **Text** "KEY ITEMS"
   - **Size Box** (Width 360, Height 160) holding a `W_InventoryList` with **Container Tag** `Container.KeyItems`

4. Inside the Overlay, a `W_Message`, Center/Bottom, with bottom padding 60.
5. Optionally, add a **Common Bound Action Bar** at Center/Bottom. It lists Rotate, Split, Item Menu and Organize with their keys.
6. **Class Defaults**:
   - **Item Menu Class**: `W_ItemMenu`
   - Check that **Input Config** is `Menu`, **Pause Game** is ticked and **Is Back Handler** is ticked. C++ sets these.
7. Compile and save. A note about Get Desired Focus Target is expected: the C++ focuses the first pocket cell.

### 8.12 `W_ItemBoxScreen`

1. Right-click `W_InventoryScreen` and choose **Duplicate**. Name it `W_ItemBoxScreen` and open it.
2. In the Horizontal Box, add a fourth **Vertical Box** column before the others, with:
   - **Text** "ITEM BOX"
   - **Size Box** (Width 360, Height 520) holding a `W_InventoryList` named exactly `ItemBoxList`. Leave its **Container Tag** empty: the screen points it at the item box.
3. Compile and save.

Having `ItemBoxList` is what makes this the item box screen. It turns on Store and Take in the item menu, and items drag between the box and the pockets.

### 8.13 `W_HUD` (parent `HadesHUDLayout`, in UI/HUD)

1. Root: an **Overlay**.
2. Inside it, a **Hades World Marker Layer**, Fill/Fill. It draws interaction prompts.
3. Inside the Overlay, a **Horizontal Box**, Left/Bottom, with padding 24, holding four `W_QuickSlot`s with **Slot Index** 0, 1, 2, 3. Select each and set **Visibility** to **Not Hit-Testable (Self & All Children)**. The HUD only shows them.
4. Inside the Overlay, a `W_Message`, Center/Bottom, with bottom padding 120.
5. **Class Defaults**:
   - **Input Config**: `Game`. When a menu closes, this hands input back to the game.
   - **Escape Menu Class**: `W_EscapeMenu`
   - **Inventory Screen Class**: `W_InventoryScreen`
6. Compile and save.
7. Go back to `BP_HadesPlayerController` and check **HUD Layout Class** is `W_HUD`, and to `BP_ItemPickup`, `BP_ItemBox` and `BP_LockedDoor` and check their **Marker Widget Class**. Set `BP_ItemBox`'s **Screen Class** to `W_ItemBoxScreen` if you couldn't before.

---

## Part 9. Test level

1. Open or create a level with a floor and a **Player Start**.
2. **Window › World Settings › GameMode Override**: `BP_HadesGameMode`, or rely on the project default.
3. Place these, a few meters apart:
   - `BP_ItemPickup`: **Inventory › Starting Items** `DA_Shotgun` × 1
   - `BP_ItemPickup`: `DA_GreenHerb` × 1
   - `BP_ItemPickup`: `DA_Gunpowder` × 1
   - `BP_ItemPickup`: `DA_Gunpowder` × 1
   - `BP_ItemPickup`: `DA_HandgunAmmo` × 30
   - `BP_ItemPickup`: `DA_HipPouch` × 1
   - `BP_ItemPickup`: `DA_BasementKey` × 1
   - two `BP_ItemBox`es, far apart
   - two `BP_LockedDoor`s
4. Save the level.

Pickups must have a mesh with collision to be looked at. Basic shapes have it.

---

## Part 10. What to check in Play

| Do | Expect |
| --- | --- |
| Start | HUD quick slots: 1 Pistol, 2 Flashlight. The flashlight is lit in the left hand. |
| Press 1 | The pistol goes to the right hand and leaves the pockets. The flashlight stays lit. |
| Press 1 again | The pistol goes back to the pockets. |
| Take the shotgun | It takes quick slot 3. |
| Press 3 | The shotgun goes to the right hand. The flashlight goes to the pockets and the light goes out: it's the shotgun or the light. |
| Press 2 | The flashlight comes back. The shotgun goes to the pockets, because it blocks the left hand. |
| Tab | The inventory opens and the world stops. Tab or Back closes it. |
| Drag items, press R while holding | Items move and rotate. The preview is green, blue, yellow or red. |
| With the pistol in the right hand and the flashlight in the left, drag the shotgun onto the right hand slot | Both go to the pockets and the shotgun is equipped. |
| Right-click the green herb, Use | The herb is used up. |
| Take the green herb pickup. Right-click it, Combine, click the red herb | A Mixed Herb (G+R) appears. |
| Combine the two gunpowders | 15 handgun rounds appear. |
| Right-click an item, Discard | It drops in front of you as a pickup. Take it back. |
| Press O | The pockets repack, biggest items first. |
| Fill the pockets (console: `Inventory.Give DA_HandgunAmmo 300`), look at a pickup | Its prompt says "Not enough space" and it can't be taken. |
| Take the Hip Pouch | "Hip Pouch: you can carry more." The grid grows to 6 × 4. |
| Take the key | It appears under KEY ITEMS, not in the pockets. |
| Open a door | "Used the Basement Key." |
| Open the second door | "Used the Basement Key. It's no longer needed, so you discarded it." The key is gone. |
| Open an item box, Store an item, open the other box | The item is in both: one shared storage. |
| In the box screen, Take or drag an item back | It goes into your pockets. |
| Drag an item onto a quick slot, or hold it with Accept and press Accept on a slot | The slot now points at that item. Right-click a slot to empty it. |

To run the plugin's automated tests: **Tools › Session Frontend › Automation**, filter `ModularInventory`, tick it and click **Start Tests**.

---

## Design choices and how to change them

| Choice | Why | To change it |
| --- | --- | --- |
| Equipped items leave the pockets | Players often ask that equipped weapons not take slots. Your hands become a real, limited choice. | Built into how the plugin equips: equipping moves the item into a slot. Changing it would take code. |
| Swapping uses the space the incoming item leaves | Otherwise swapping weapons in full pockets fails even when the outgoing weapon would fit. | Built into the plugin's Equip Item. |
| Key items in an unlimited list, auto-discarded after their last use | Key items taking slots is the genre's top complaint. Village and the RE4 remake keep them off the grid. | Give `CL_KeyItems` a Capacity, or remove the pockets' filter to make keys take grid space again. |
| Unlimited linked item box | RE7, the RE2 remake and Requiem's Grace all do this. It keeps backtracking reasonable. | Give `CL_ItemBox` a Capacity. |
| Pouches grow one grid | Like the RE2 remake's hip pouches and the RE4 remake's cases. One grid is less fiddly than several. | Edit **Pocket Upgrades**. |
| Discard drops the item | Reversible, and players like being able to drop items anywhere. | Clear **Dropped Pickup Class** to destroy discarded items, as the RE games do. |
| Four quick slots | Matches the D-pad, as in RE7 and the RE2 remake. | **Num Slots**, plus more `Input.QuickSlot` tags and bindings in C++. |
| Organize (auto-sort) | The RE4 remake has it. It takes the tedium out of a full grid. | Remove the `UI.Action.Organize` mapping. |

Flashlight-or-gun was disliked in Doom 3 because it was forced all the time. Keep one-handed weapons usable with the light, make two-handed weapons the deliberate trade-off, and consider a weapon light attachment or a head lamp as a late reward. The plugin's attachment slots can do both.

## Not built yet

- **Saving.** Items, quick slots, lock states and the item box aren't saved. The plugin's README section 19 describes the plan.
- **Health.** The healing effects need a Health attribute.
- **Flashlight battery and weapon reloading.** See the plugin README's how-to 16.1.
- **Examine, files and map tabs.**
