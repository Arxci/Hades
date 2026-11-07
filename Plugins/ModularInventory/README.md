# Modular Inventory

A data-driven inventory for Unreal Engine 5.7. It supports grid, slot and list containers, containers nested inside items to any depth, equipment that is integrated with the Gameplay Ability System, crafting, and loot tables that Game Features can extend. It is singleplayer only and has no replication.

| Module | Type | Contents |
| --- | --- | --- |
| `ModularInventory` | Runtime | Data assets, the registry, components, equipment, loot, crafting |
| `ModularInventoryUI` | Runtime | Common UI widgets and the per-player UI subsystem, for mouse and gamepad |
| `ModularInventoryEditor` | Editor | Asset menu, thumbnails, footprint preview, item creation, Asset Manager setup |

## Getting started

These are the steps for a new project, in an order where everything a step refers to already exists. Every asset type is in the Content Browser under **Add > Inventory**.

1. **Register items with the Asset Manager.** Choose **Tools > Modular Inventory > Register Items With Asset Manager** once. It adds an `InventoryItem` entry to **Project Settings > Asset Manager**, which is saved in `Config/DefaultGame.ini`.
   - *Why:* the Asset Manager can then find any item by ID without loading it. Saving and `Inventory.Give` need this, and items that only a save game references still get cooked.
   - When the editor asks to add **GameFeatureData** to the Asset Manager, accept. The plugin turns on Game Features for its loot and recipe injection, and Game Features finds its data through the Asset Manager.
2. **Create Stack Rules and Equip Rules.**
   - **Stack Rules** set the stack size and whether stacks split. For example, `SR_Ammo` allows 30 and `SR_Consumable` allows 5.
   - **Equip Rules** list the slot tags an item fits, the slots it blocks, and what it grants through GAS.
   - *Why:* items of a kind share one rules asset, so tuning it tunes all of them. Rules are hard references because gameplay needs them as soon as an item exists.
3. **Create Container Layouts**, one per shape of storage:
   - pockets: **Grid**, for example 6 by 4;
   - equipment: **Slots** with **Equipment** ticked, one slot per tag, such as `Equipment.Slot.Hand.Main`;
   - a pickup lying in the world: **List**.
   - *Why:* a layout only describes a container. The real containers are built from it at runtime, so every chest of a kind shares one layout. Items can carry containers too: a backpack lists a Grid layout under **Containers**.
4. **Create items.** Right-click a static mesh or texture and choose **Create Inventory Item**, or add an **Item Definition** and an **Item Cosmetics** yourself. On the definition, set the display name, size, rules, tags and tag values, and any containers, sockets and fragments.
   - *Why two assets:* the definition is the gameplay data, and it loads with anything that references it. The cosmetics hold the icon, world mesh, equipment pieces and anim layers as soft references, so those load only when something shows them.
5. **Set up actors.**
   - Add an **Inventory Component** to everything that holds items: the player, chests and pickups. Give it containers (a tag plus a layout). Optionally add starting items, starting equipment or a starting loot table. On the player, tick **Preload Cosmetics**.
   - Add an **Inventory Equipment Component** to characters that wear or hold items. It applies whatever sits in their equipment slots: GAS grants, pieces and anim layers.
   - *Why two components:* holding items and showing them are separate jobs. A chest needs the first and never the second.
6. **Pick items up and drop them.** Any actor with an Inventory Component can be a pickup or a chest. To pick an item up, call `TakeItem` on the player's component with an item from the pickup's. To put one back, call `TransferItem`. Items keep their state and contents wherever they move.
7. **Build the UI.** Make Blueprints of the UI widgets and put a grid on a Common UI screen; see [UI](#ui).
8. **Add loot and crafting.** Create Loot Tables (use **Simulate** to check them), Recipes, and Recipe Books for crafting stations.
9. **Test.** In Play In Editor, `Inventory.Give DA_Battery 5` adds items. The Gameplay Debugger's **Inventory** category shows what everything holds.

## How it fits together

```
Data assets (rules are hard references, looks are soft)
  Item Definition ─┬─ Stack Rules
                   ├─ Equip Rules ───── slots, blocked slots, GAS grants
                   ├─ Cosmetics ─────── icon, world mesh, equipment pieces, anim layers (all soft)
                   ├─ Containers ────── Container Layout (grid / slots / list)
                   └─ tags, tag values, sockets, fragments
  Recipe, Recipe Book, Loot Table

World
  UInventorySubsystem
    └─ FInventoryRegistry   flat item and container records, generation-checked handles, transactions
  Actors
    UInventoryComponent           root containers: pockets, equipment slots, a chest, a pickup
    UInventoryEquipmentComponent  applies whatever sits in the equipment slots
```

Items are records in the registry, not UObjects. You refer to them with `FInventoryItemHandle` and `FInventoryContainerHandle`. A handle goes stale as soon as its record is destroyed, even if the slot is reused later. One registry serves the whole world, so a handle stays valid when its item moves between containers or components.

## Containers

| Type | Placement | Notes |
| --- | --- | --- |
| Grid | `Cell` (top-left) and `bRotated` | Items take `Size` cells; rotation is a quarter turn clockwise. |
| Slots | `Index` | Each slot has a tag and a filter. |
| List | `Index` (insert position) | Ordered, with an optional capacity; sizes are ignored. |

An item definition can list containers that every instance carries, such as a backpack's storage or a rifle's attachment slots. These are created and destroyed together with the item. Nesting has no depth limit; the only rule is that an item can't end up inside itself.

Filters use `FInventoryItemQuery`, which can match by definitions, a tag query and tag value conditions. The same query type is used for container filters, slot filters, recipe ingredients, socket targets and searches. Queries test an item's own tags and values; socket bonuses are ignored, so placing an item never changes what its neighbours accept.

## Item state

- **Tags**: the definition's tags plus the item's own.
- **Tag values**: numbers keyed by tag. Use them for durability, condition, charge or rarity. An item only stores a value once it differs from its definition's default, so changing a default updates every existing item. Items with the same state stack together.
- **Level**: the `Inventory.Item.Level` tag value. Equipment grants scale with it.
- **Fragments**: any struct, Blueprint structs included. Definitions carry static `Fragments`, and each item copies `InstanceFragments` as its own state. Containers work the same way. For save support later, mark fragment properties `SaveGame`.

## Operations

All operations are transactional. If an operation fails, everything it changed is undone. Listeners only hear about committed changes, once per transaction.

| Operation | Registry (C++) | Blueprint |
| --- | --- | --- |
| Add | `AddItem`, `AddItemToContainers` | Subsystem `AddItem` / `AddItemAt`, component `AddItem` |
| Remove | `RemoveItem` | Subsystem `RemoveItem`, component `RemoveItems` |
| Move / Transfer | `MoveItem`, `MoveItemToContainers` | Subsystem `MoveItem` / `MoveItemTo` / `MoveItemToContainers`, component `TakeItem` / `TransferItem` |
| Swap | `SwapItems` | Subsystem `SwapItems` |
| Split | `SplitStack` | Subsystem `SplitStack` / `SplitStackTo` |
| Merge | `MergeStacks` | Subsystem `MergeStacks` |
| Rotate | `RotateItem` | Subsystem `RotateItem` |
| Equip / Unequip | `EquipItem`, `UnequipItem` | Subsystem, or equipment component `EquipItem` / `UnequipItem` / `UnequipSlot` |

- **Batches**: `ExecuteBatch` runs a list of `FInventoryOperation`s and undoes all of them if any one fails.
- **Custom atomic logic in C++**: call `Registry.RunTransaction([&] { ...; return Error; })`. Transactions nest; a failure inside one only undoes its own part.
- **Previews**: `Registry.DryRun([&] { ...; return Error; })` runs the same code and always undoes it, so you get the real answer without changing anything.
- **Error reporting**: C++ calls return `EInventoryError`. Blueprint calls return a value and give the error as an output pin.

## Equipment

Equipping moves an item into a slot of a container whose layout has **Equipment** ticked. The item's equip rules must allow that slot's tag. `BlockedSlots` keeps slots in the same container empty, for example a two-handed weapon blocking the off hand. `OwnerRequirements` is checked against the owner's ability system tags.

The equipment component applies everything in those slots:

- **GAS**: abilities, effects, attribute sets and loose owner tags. Ability and effect levels follow the item level (`Level + ItemLevel - 1` when scaling) and update in place when the level changes. To find an ability's item, use `GetItemFromAbility` or `UInventoryEquipmentComponent::GetItemForAbility`.
- **Pieces**: Actor, Static Mesh and Skeletal Mesh pieces from the item's cosmetics. Their assets load asynchronously before spawning. Set `AttachComponent` to a component tag or name, such as the first-person arms mesh. `FirstPersonPrimitiveType` sets which view a piece renders in. Spawned actors that implement `InventoryEquipmentActorInterface` receive their item.
- **Mutable**: use an Actor piece whose actor builds its Customizable Object instance from the item it receives in `OnEquipped`. This keeps the plugin free of a hard Mutable dependency.
- **Linked anim layers**: linked on the character mesh or a named mesh. When an item is unequipped, the remaining items' layers are linked again, so removing one doesn't hide another.
- **Attachments**: if an equipped item has equipment containers of its own (a rifle's scope slot), what sits in them is equipped too. Their pieces attach to the parent item's first piece.

## Sockets

Sockets apply only in grid containers. A socket lists cells relative to its item's top-left cell (`(0,-1)` is the cell above), and the cells rotate with the item. Any item in those cells that matches the socket's filter gets the socket's value modifiers and tags. Read the results with `GetEffectiveTagValue` and `GetEffectiveItemTags`. `GetSocketSources` lists the items affecting one. The item definition's **Footprint** preview shows which cells each socket reaches.

## Crafting

A `UInventoryRecipe` has ingredients (a query, a count, and whether the ingredient is consumed or only needed as a tool), fixed outputs, an optional loot table for random outputs, a tag query against context tags, and fragments for anything else. For custom availability rules, subclass the recipe in Blueprint and override `IsAvailable`. A `UInventoryRecipeBook` groups recipes, for example per workbench.

Crafting is all or nothing: if the outputs don't fit, the ingredients are kept. No item counts toward two ingredients. The component's `Craft` takes ingredients from storage and adds the owner's ability system tags to the context.

## Loot and Game Features

A loot table has pools. Each pool rolls a number of times and picks entries by weight; an entry can be an item, another table or nothing. Entry and pool conditions are tag queries against context tags. Item entries can randomise starting tag values, for example durability between 20 and 60. Rolls are deterministic for a given `FRandomStream`.

The **Add Inventory Content** game feature action injects pools into existing loot tables and recipes into existing recipe books while the feature is active, and removes them when it deactivates. Use it for DLC.

## Loading and the Asset Manager

- **Item IDs**: every item definition is a primary asset of type `InventoryItem`, named after its asset: `InventoryItem:DA_Lantern`. Item definition asset names must be unique. Saves should store this ID rather than an object path, so moving assets between folders doesn't break them.
- **On-demand loading**: soft references load asynchronously through the Asset Manager's streamable manager, by whatever needs them. An item widget loads its icon, an equipped item its pieces and anim layers, and a pickup its world mesh. Each keeps its own handle, so an asset unloads once nothing uses it.
- **Preloading**: an Inventory Component with **Preload Cosmetics** keeps everything its items reference loaded while it holds them, so menus and equipping never wait. Turn it on for the player and leave it off for chests and pickups.
- **Game Feature plugins** that add items should list `InventoryItem` in their Game Feature Data's **Primary Asset Types to Scan**, with the plugin's content folder, so their items get IDs too.
- **Items made before the `InventoryItem` type**, or before registering, keep their old ID until they're saved again. The register action resaves them.

## UI

The `ModularInventoryUI` module holds the rules every inventory screen shares: picking items up, dragging, rotating, splitting, and previewing where an item would land. Layout and looks stay in Blueprint. It is built on Common UI, so focus, hover, Accept and Back work the same in the inventory as in every other menu. A project that doesn't use Common UI can drop the module and build its own on the runtime module.

```
UInventoryUISubsystem (one per local player)
  the held item (count and rotation), the target (where it would land), the hovered item

Widgets (abstract: make a Blueprint child of each)
  UInventoryGridWidget         a grid container: cell widgets, the items over them, the placement preview
  UInventorySlotWidget         one slot of a slot container, such as the main hand
  UInventoryCellWidget         a focusable grid cell; a Common UI button
  UInventoryItemWidget         an item's icon and count; the mouse drags it
  UInventoryItemDetailsWidget  name, description and count, as a tooltip or a panel
```

**Holding** an item changes nothing in the inventory; only placing it does. Closing the screen or a refused drop simply lets go. A placement does one of these, and widgets get the same answer beforehand through `OnPlacementPreviewChanged`:

| Action | When |
| --- | --- |
| Place | The spot is free, or only the held item is there. |
| Merge | A stack the held item can join is there. |
| Swap | Another item is there, and the two can trade places. Only whole stacks swap. |
| Invalid | Anything else. |

Previews use `FInventoryRegistry::DryRun`, so they can't disagree with the actual move.

### Input

- **Mouse**: drag an item and drop it; the item is centered on the cursor. Shift-drag takes half a stack. A click does what Accept does. Right click calls `OnContextRequested` on the item widget, for menus such as Use or Drop.
- **Gamepad and keyboard**: Common UI moves focus between cells and slots. Accept picks up the item under focus; Accept again puts it down with its top-left corner on the focused cell.
- **Rotate and split**: bind Common UI actions to the subsystem's `RotateHeld` and `SplitHeld`. Split halves the held count; at one, it goes back to the whole stack.
- **Screens**: your inventory screen should call `CancelHold` when Back is pressed while an item is held, and `ClearInteraction` when it closes.

### Building the widgets

Make a Blueprint child of each class you need, then add the named widgets in the designer. Required names are in bold.

| Parent class | Named widgets | Settings |
| --- | --- | --- |
| Inventory Item Widget | `Icon` (Image), `CountText` | Drag Visual Class, Drag Cell Size, Tooltip Class |
| Inventory Cell Widget | none; style it like any Common UI button | |
| Inventory Grid Widget | **`GridCanvas`** (Canvas Panel), `PlacementPreview` (any widget, inside Grid Canvas) | Container Tag, Cell Size, Cell Class, Item Class |
| Inventory Slot Widget | `ItemWidget` (your item widget Blueprint) | Container Tag, Slot Tag |
| Inventory Item Details Widget | `Icon`, `NameText`, `DescriptionText`, `CountText` | Follow Hovered Item, Collapse Without Item |

- **Icon**: in the item widget, center the icon instead of filling, so it can turn with the item. Grids size it to the item's cells.
- **Cell Class**: grids need one for gamepad focus. A grid without cells works only with the mouse, and Grid Canvas then needs a Size Box around it.
- **Container Tag**: grids and slots find that container on the owning player's inventory component. For a chest or a backpack, call `SetContainer` instead.
- **Blueprint events**: use these for looks and game-specific actions:
  - item widget: `OnItemChanged`, `OnHeldChanged`, `OnHoveredChanged`, `OnContextRequested`
  - grid: `OnContainerRefreshed`, `OnPlacementPreviewChanged`
  - slot: `OnSlotItemChanged`, `OnPlacementPreviewChanged`
  - details: `OnItemChanged`. Fill in stats from tag values here.

## Editor tools

- **Add > Inventory**: every plugin asset type in the Content Browser's Add menu.
- **Create Inventory Item**: right-click static meshes or textures. Each becomes an item definition and cosmetics in the same folder, named after it: `SM_Lantern` makes `DA_Lantern` and `DA_Lantern_Cosmetics`. Meshes become the world mesh and textures the icon. One mesh and one texture selected together make a single item.
- **Thumbnails**: item definitions and cosmetics show their icon in the Content Browser. Assets saved before the plugin's editor module existed show it once they're loaded or saved.
- **Footprint**: an item definition's **Sockets** section shows the item's cells and the cells each socket reaches, unrotated and rotated, numbered by socket. It updates as you edit.
- **Simulate**: a loot table's **Simulation** section rolls it many times with the given context tags and lists how often each item drops.
- **Register Items With Asset Manager**: under **Tools > Modular Inventory**; see [Loading and the Asset Manager](#loading-and-the-asset-manager).
- **Inventory.Give**: a console command for Play In Editor and development builds: `Inventory.Give <ItemAssetName> [Count]`. An unknown name lists the known items.
- **Validation**: every asset checks itself on save and with Data Validation, for example missing layouts, duplicate tags, impossible counts and unregistered items.

## Debugging, profiling and tests

- **Gameplay Debugger**: open it with the apostrophe key and enable the **Inventory** category. It shows the selected actor's containers as a tree, an ASCII map of each grid, every item's state and the equipped items with what they granted. Spawned pieces are marked in the world.
- **Unreal Insights**: operations appear as `Inventory_*` CPU scopes. With the `counters` channel enabled (`-trace=default,counters`), `Inventory/Items` and `Inventory/Containers` graph the live record counts.
- **Automation tests**: open Session Frontend, go to Automation and filter by `ModularInventory`. From the command line:
  ```
  UnrealEditor-Cmd.exe YourProject.uproject -ExecCmds="Automation RunTests ModularInventory; Quit" -unattended -nullrhi
  ```
  The tests run against a bare registry, so they need no world. They register `Inventory.Test.*` gameplay tags in non-shipping builds.

## Saving (not implemented yet)

Everything needed for saving is in place. Records are `UPROPERTY(SaveGame)` structs, definitions have Asset Manager IDs, and fragments are instanced structs. The plan for Easy Multi Save:

1. Add a `UPROPERTY(SaveGame)` snapshot to `UInventoryComponent` holding its subtree's records, with handles rewritten as indices into the snapshot and definitions stored as their `FPrimaryAssetId`.
2. Fill the snapshot in `IEMSCompSaveInterface::ComponentPreSave`. In `ComponentLoaded`, destroy the current containers and rebuild them from the snapshot.
3. Use the same export and import to carry the player's inventory across level transitions, since each world has its own registry.

## Decisions worth knowing

- **Fragments**: they are stored on each record rather than in a separate fragment registry. Instanced structs allocate their own memory anyway, so a third registry would only add indirection.
- **Durability, condition and ammo counts**: these are tag values rather than dedicated fragments, because loot ranges, recipes, sockets and queries can all read tag values. Fragments are for structured state.
- **Soft cosmetics**: definitions hard-reference rules and soft-reference visuals, so loading every item definition for a crafting list, a loot table or a save costs almost nothing.
- **No asset bundles**:
  - Bundles only collect soft references from the primary asset itself, but visuals live in a separate, shareable cosmetics asset.
  - A bundle's loaded state is global per asset. That suits loading a whole set at once, such as a level or a game mode, better than one widget needing one icon.
  - Per-use handles plus component preloading cover both cases without extra project settings.
  - Bundles become the better tool if cosmetics move onto the definition, or for a client/server split.
- **Equipping**: it is a move into an equipment slot, so equipping works inside batches and a worn backpack is simply an item in the Back slot.
- **UI widgets are Common UI**: cells and slots are Common UI buttons, so Common UI alone owns focus and Accept. Mouse dragging is the one exception, using UMG drag and drop. Both only call the UI subsystem, so the mouse and the gamepad can't drift apart.
- **No view models**: the widgets read the registry directly through handles. Item state is plain data that's cheap to look up, so view models would mostly copy it.
