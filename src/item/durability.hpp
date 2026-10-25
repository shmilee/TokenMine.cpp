// SURV-3: the durability (item wear) system's pure helpers — the
// campfire / drown pattern: the decision + every wear computation lives
// here, unit-tested headless; the Game's event sites (break / melee /
// shot / ignite) and Player::damage (armor) are thin adapters that call
// in at the ACTION-COMPLETION points (the wear never cancels the action
// that caused it: the block already dropped, the arrow already flew, the
// damage already landed).
//
// The model (Minecraft, the wiki Durability page): each wearable item
// carries a float accumulated damage (ItemStack::damage, 0 = full);
// when the accumulated damage reaches the item's data `durability` the
// item BREAKS (its stack is removed — no drop entity, the modern
// Minecraft behavior). Wear events, all frame-locked (no real-dt):
//   - breaking a NON-instant block (hardness != 0) with a tool: +1
//   - a melee hit on a mob with a tool: +2
//   - firing the launcher (any charge, Infinity included): +1
//   - a successful ignition with an igniter: +1
//   - a damage event the armor actually reduced: each worn piece
//     +max(kArmorWearMin, incoming / kArmorWearPerIncoming) (incoming =
//     the PRE-reduction original amount; the float 10 -> 2.5, 1 -> 1
//     rule — armor that does not block a source does not wear from it,
//     the Player::damage bypassArmor gate).
#pragma once

#include "item/item.hpp"

namespace mc {

class ItemRegistry;

// -- the per-event wear amounts (Minecraft) ----------------------------------
// One non-instant block broken with a tool (instant = hardness 0, the
// "naturally breaks instantly" approximation — the documented deviation).
inline constexpr float kToolBlockWear = 1.0f;
// One melee hit that lands on a mob with a tool.
inline constexpr float kToolMeleeWear = 2.0f;
// One launcher shot (the charge is irrelevant; Infinity included).
inline constexpr float kLauncherShotWear = 1.0f;
// One successful ignition (flint and steel).
inline constexpr float kIgniteWear = 1.0f;
// Armor: the incoming-damage amount (HP) per 1 durability point of wear.
inline constexpr float kArmorWearPerIncoming = 4.0f;
// Armor: the per-piece wear floor (Minecraft: "never below 1").
inline constexpr float kArmorWearMin = 1.0f;

// -- the core wear step (registry-free) --------------------------------------
// Adds `wear` to the stack's accumulated damage and, when the accumulated
// reaches or exceeds `durability` (the item's max), empties the stack
// (the BREAK). Returns true exactly when the stack broke. A no-op
// (unchanged stack, false) when the stack is empty, `wear` <= 0, or
// `durability` <= 0 (the item has no durability — it never wears).
// Pure: no registry, no world — Player::damage (which holds no
// ItemRegistry, the setArmorSlot pattern) drives it with the per-slot
// durability data the game passed at equip time.
bool applyDurability(ItemStack& stack, float wear, float durability);

// -- the registry convenience -----------------------------------------------
// The same step with the item's DATA durability (items.at(stack.item).
// durability; 0 for a stack whose item has none — the no-op case).
bool applyDurability(ItemStack& stack, float wear, const ItemRegistry& items);

// -- the armor per-piece wear of one damage event ----------------------------
// max(kArmorWearMin, incoming / kArmorWearPerIncoming) (Minecraft's float
// rule: 1 durability for every 4 HP of incoming damage, never below 1 —
// 10 -> 2.5 exact, 1 -> 1). `incoming` = the PRE-reduction original
// damage amount.
float armorWearPerPiece(float incoming);

} // namespace mc
