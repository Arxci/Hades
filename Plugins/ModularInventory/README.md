# Modular Inventory

A data-driven inventory for Unreal Engine 5.7. It supports grid, slot and list containers, containers nested inside items to any depth, equipment that is integrated with the Gameplay Ability System, crafting, and loot tables that Game Features can extend. A separate UI module adds Common UI widgets that work with both mouse and gamepad. It is singleplayer only and has no replication.

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
- **Error reporting**: C++ calls return `EInventoryError`. Blueprint calls return a value and give the error as an output pin.

## Equipment

Equipping moves an item into a slot of a container whose layout has **Equipment** ticked. The item's equip rules must allow that slot's tag. `BlockedSlots` keeps slots in the same container empty, for example a two-handed weapon blocking the off hand. `OwnerRequirements` is checked against the owner's ability system tags.

The equipment component applies everything in those slots:

- **GAS**: abilities, effects, attribute sets and loose owner tags. Ability and effect levels follow the item level (`Level + ItemLevel - 1` when scaling) and update in place when the level changes. To find an ability's item, use `GetItemFromAbility` or `UInventoryEquipmentComponent::GetItemForAbility`.
- **Pieces**: Actor, Static Mesh and Skeletal Mesh pieces from the item's cosmetics. Their assets load asynchronously before spawning. Set `AttachComponent` to a component tag or name, such as `ArmsMesh` for the first-person view. `FirstPersonPrimitiveType` sets which view a piece renders in. Spawned actors that implement `InventoryEquipmentActorInterface` receive their item.
- **Mutable**: use an Actor piece whose actor builds its Customizable Object instance from the item it receives in `OnEquipped`. This keeps the plugin free of a hard Mutable dependency.
- **Linked anim layers**: linked on the character mesh or a named mesh. When an item is unequipped, the remaining items' layers are linked again, so removing one doesn't hide another.
- **Attachments**: if an equipped item has equipment containers of its own (a rifle's scope slot), what sits in them is equipped too. Their pieces attach to the parent item's first piece.

## Sockets

Sockets apply only in grid containers. A socket lists cells relative to its item's top-left cell (`(0,-1)` is the cell above), and the cells rotate with the item. Any item in those cells that matches the socket's filter gets the socket's value modifiers and tags. Read the results with `GetEffectiveTagValue` and `GetEffectiveItemTags`. `GetSocketSources` lists the items affecting one.

## Crafting

A `UInventoryRecipe` has ingredients (a query, a count, and whether the ingredient is consumed or only needed as a tool), fixed outputs, an optional loot table for random outputs, a tag query against context tags, and fragments for anything else. For custom availability rules, subclass the recipe in Blueprint and override `IsAvailable`. A `UInventoryRecipeBook` groups recipes, for example per workbench.

Crafting is all or nothing: if the outputs don't fit, the ingredients are kept. No item counts toward two ingredients. The component's `Craft` takes ingredients from storage and adds the owner's ability system tags to the context.

## Loot and Game Features

A loot table has pools. Each pool rolls a number of times and picks entries by weight; an entry can be an item, another table or nothing. Entry and pool conditions are tag queries against context tags. Item entries can randomise starting tag values, for example durability between 20 and 60. Rolls are deterministic for a given `FRandomStream`.

The **Add Inventory Content** game feature action injects pools into existing loot tables and recipes into existing recipe books while the feature is active, and removes them when it deactivates. Use it for DLC.

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

Previews run the real operation with `FInventoryRegistry::DryRun` and undo it, so they can't disagree with the actual move.

### Input

- **Mouse**: drag an item and drop it; the item is centered on the cursor. Shift-drag takes half a stack. A click does what Accept does. Right click calls `OnContextRequested` on the item widget, for menus such as Use or Drop.
- **Gamepad and keyboard**: Common UI moves focus between cells and slots. Accept picks up the item under focus; Accept again puts it down with its top-left corner on the focused cell.
- **Rotate and split**: bind input to the subsystem's `RotateHeld` and `SplitHeld`. Split halves the held count; at one, it goes back to the whole stack.
- **Screens**: while an item is held, Back should call `CancelHold`. When the screen closes, it should call `ClearInteraction`. `UHadesInventoryScreen` does both.

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

## Debugging, profiling and tests

- **Gameplay Debugger**: open it with the apostrophe key and enable the **Inventory** category. It shows the selected actor's containers as a tree, an ASCII map of each grid, every item's state and the equipped items with what they granted. Spawned pieces are marked in the world.
- **Unreal Insights**: operations appear as `Inventory_*` CPU scopes. With the `counters` channel enabled (`-trace=default,counters`), `Inventory/Items` and `Inventory/Containers` graph the live record counts.
- **Automation tests**: open Session Frontend, go to Automation and filter by `ModularInventory`. From the command line:
  ```
  UnrealEditor-Cmd.exe Hades.uproject -ExecCmds="Automation RunTests ModularInventory; Quit" -unattended -nullrhi
  ```
  The tests run against a bare registry, so they need no world. They register `Inventory.Test.*` gameplay tags in non-shipping builds.

## Saving (not implemented yet)

Everything needed for saving is in place. Records are `UPROPERTY(SaveGame)` structs, definitions are primary data assets, and fragments are instanced structs. The plan for Easy Multi Save:

1. Add a `UPROPERTY(SaveGame)` snapshot to `UInventoryComponent` holding its subtree's records, with handles rewritten as indices into the snapshot.
2. Fill the snapshot in `IEMSCompSaveInterface::ComponentPreSave`. In `ComponentLoaded`, destroy the current containers and rebuild them from the snapshot.
3. Use the same export and import to carry the player's inventory across level transitions, since each world has its own registry.

## Setting it up in Hades

1. **Layouts**: create Container Layout assets. For example, a `Grid` for the pockets, a `Slots` layout with **Equipment** ticked for the hands and body (tags such as `Equipment.Slot.Hand.Main`), and a `List` for pickups.
2. **Rules**: create Stack Rules and Equip Rules assets and share them between items.
3. **Items**: create Item Definitions. Give each one Cosmetics with an icon, a world mesh and the pieces to show when equipped. For the first-person view, attach pieces to `ArmsMesh`.
4. **Hero**: in the hero Blueprint, add the layouts to the Inventory Component's containers. Starting Equipment can put a flashlight straight into the hand.
5. **Pickups**: create a Blueprint of `AHadesItemPickup`, give its inventory a list container, and set its starting items. Interacting moves as much as fits into the interactor's inventory, keeping each item's state and contents. The pickup is destroyed once it is empty.
6. **Input actions**: in **Project Settings > Common UI Input Settings > Input Actions**, add `UI.Action.Inventory` (for example Tab and the gamepad's Special Left), `UI.Action.RotateItem` (R, and a face button) and `UI.Action.SplitStack` (Shift, and a shoulder button). Give the last two display names, since they show in the action bar.
7. **Widgets**: build the widgets as described under **UI**. For the screen, make a Blueprint of `UHadesInventoryScreen` and set its Input Config to **Menu**. Name its main grid `MainGrid`; its first cell takes focus when the screen opens. Then set **Inventory Screen Class** on the HUD layout Blueprint. The Inventory action opens the screen on the GameMenu layer and also closes it.

## Decisions worth knowing

- **Fragments**: they are stored on each record rather than in a separate fragment registry. Instanced structs allocate their own memory anyway, so a third registry would only add indirection.
- **Durability, condition and ammo counts**: these are tag values rather than dedicated fragments, because loot ranges, recipes, sockets and queries can all read tag values. Fragments are for structured state.
- **Soft cosmetics**: there is no dedicated server, but soft cosmetics still keep memory and load times down when many definitions are loaded.
- **Equipping**: it is a move into an equipment slot, so equipping works inside batches and a worn backpack is simply an item in the Back slot.
- **UI widgets are Common UI**: cells and slots are Common UI buttons, so Common UI alone owns focus and Accept. Mouse dragging is the one exception, using UMG drag and drop. Both only call the UI subsystem, so the mouse and the gamepad can't drift apart.
- **No view models**: the widgets read the registry directly through handles. Item state is plain data that's cheap to look up, so view models would mostly copy it.
