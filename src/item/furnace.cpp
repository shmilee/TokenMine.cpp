#include "item/furnace.hpp"

#include <cmath>

namespace mc {

namespace {

// The substep duration (1/60 s) — the fixed per-frame step the furnace
// advances by (the day-cycle class, see the header).
constexpr double kStepSeconds = 1.0 / 60.0;

} // namespace

int smeltSubstepsFor(const Item& item) {
    if (item.smeltableTo.empty()) {
        return 0;
    }
    // round(t * 60): exact for the shipped data values (10.0 -> 600).
    return static_cast<int>(
        std::lround(static_cast<double>(item.smeltTime) / kStepSeconds));
}

int fuelSubstepsFor(const Item& item) {
    if (item.fuelTime <= 0.0f) {
        return 0;
    }
    return static_cast<int>(
        std::lround(static_cast<double>(item.fuelTime) / kStepSeconds));
}

void tickFurnace(FurnaceState& f, const ItemRegistry& items) {
    // -- fuel ------------------------------------------------------------
    // Spend one substep of the burning fire.
    if (f.fuelSubsteps > 0) {
        f.fuelSubsteps--;
    }
    // Light the fire when it is out and fuel stock remains. The item is
    // consumed at ignition (Minecraft); leftover burn time is kept, and a
    // non-fuel item (burn 0) is never consumed.
    if (f.fuelSubsteps == 0 && !f.fuel.empty()) {
        const int burn = fuelSubstepsFor(items.at(f.fuel.item));
        if (burn > 0) {
            f.fuelSubsteps = burn;
            f.fuel.count--;
            if (f.fuel.count <= 0) {
                f.fuel = ItemStack{};
            }
        }
    }
    const bool burning = f.fuelSubsteps > 0;

    // -- smelting ----------------------------------------------------------
    // The progress belongs to the current input item; a changed input
    // (including empty / non-smeltable) loses it.
    const bool inputSmeltable =
        !f.input.empty() && !items.at(f.input.item).smeltableTo.empty();
    const ItemID current = inputSmeltable ? f.input.item : ItemID::invalid();
    if (f.progressFor != current) {
        f.progressFor = current;
        f.progressSubsteps = 0;
    }
    if (!burning || !inputSmeltable) {
        return;
    }
    const Item& in = items.at(f.input.item);
    const Item& product = items.at(items.idForName(in.smeltableTo));
    // The output must be empty or hold the same product with room.
    const bool room =
        f.output.empty() ||
        (f.output.item == product.id && f.output.count < product.maxStack);
    if (!room) {
        // A full output pauses smelting (the fire keeps burning above).
        return;
    }
    f.progressSubsteps++;
    if (f.progressSubsteps < smeltSubstepsFor(in)) {
        return;
    }
    // One unit smelted.
    f.progressSubsteps = 0;
    f.input.count--;
    if (f.input.count <= 0) {
        f.input = ItemStack{};
    }
    if (f.output.empty()) {
        f.output = ItemStack{product.id, 0};
    }
    f.output.count++;
}

} // namespace mc
