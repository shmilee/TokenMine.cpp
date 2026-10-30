#include "item/crafting_grid.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_set>

namespace mc {

// -- CraftingGrid -----------------------------------------------------------

const ItemStack& CraftingGrid::at(int slot) const {
    assert(slot >= 0 && slot < kSize);
    return m_slots[slot];
}

ItemStack& CraftingGrid::at(int slot) {
    assert(slot >= 0 && slot < kSize);
    return m_slots[slot];
}

bool CraftingGrid::isEmpty(int slot) const {
    return m_slots[slot].empty();
}

void CraftingGrid::setSlot(int slot, const ItemStack& stack) {
    assert(slot >= 0 && slot < kSize);
    m_slots[slot] = stack;
}

int CraftingGrid::take(int slot, int n) {
    assert(slot >= 0 && slot < kSize && n >= 0);
    if (n <= 0 || m_slots[slot].empty()) {
        return 0;
    }
    const int removed = std::min(n, m_slots[slot].count);
    m_slots[slot].count -= removed;
    if (m_slots[slot].count <= 0) {
        m_slots[slot] = ItemStack {};
    }
    return removed;
}

bool CraftingGrid::empty() const {
    for (const ItemStack& s : m_slots) {
        if (!s.empty()) {
            return false;
        }
    }
    return true;
}

void CraftingGrid::clear() {
    for (ItemStack& s : m_slots) {
        s = ItemStack {};
    }
}

// -- matching ----------------------------------------------------------------

bool matchesShaped(const CraftingGrid& grid, const Recipe& recipe, int* dx,
                   int* dy) {
    assert(recipe.type == RecipeType::Shaped);
    const int rows = static_cast<int>(recipe.pattern.size());
    const int cols = static_cast<int>(recipe.pattern.front().size());
    for (int oy = 0; oy + rows <= CraftingGrid::kColumns; ++oy) {
        for (int ox = 0; ox + cols <= CraftingGrid::kColumns; ++ox) {
            // Which cell does the pattern cover at this offset?
            std::unordered_set<int> covered;
            bool ok = true;
            for (int r = 0; r < rows && ok; ++r) {
                for (int c = 0; c < cols && ok; ++c) {
                    const char ch = recipe.pattern[static_cast<std::size_t>(r)][c];
                    const int slot = (oy + r) * CraftingGrid::kColumns + (ox + c);
                    if (ch == ' ') {
                        continue;
                    }
                    covered.insert(slot);
                    const ItemStack& cell = grid.at(slot);
                    const ItemID want = recipe.key.at(ch);
                    ok = cell.item == want && cell.count > 0;
                }
            }
            if (!ok) {
                continue;
            }
            // Every cell the pattern does not cover must be empty.
            for (int slot = 0; slot < CraftingGrid::kSize && ok; ++slot) {
                if (covered.count(slot) == 0) {
                    ok = grid.isEmpty(slot);
                }
            }
            if (ok) {
                if (dx != nullptr) {
                    *dx = ox;
                }
                if (dy != nullptr) {
                    *dy = oy;
                }
                return true;
            }
        }
    }
    return false;
}

bool matchesShapeless(const CraftingGrid& grid, const Recipe& recipe) {
    assert(recipe.type == RecipeType::Shapeless);
    // Every occupied cell must hold an ingredient item.
    for (int slot = 0; slot < CraftingGrid::kSize; ++slot) {
        const ItemStack& cell = grid.at(slot);
        if (cell.empty()) {
            continue;
        }
        bool isIngredient = false;
        for (const auto& [item, count] : recipe.ingredients) {
            if (cell.item == item) {
                isIngredient = true;
                break;
            }
        }
        if (!isIngredient) {
            return false;
        }
    }
    // The total of each required ingredient must reach its count.
    for (const auto& [item, need] : recipe.ingredients) {
        int have = 0;
        for (int slot = 0; slot < CraftingGrid::kSize; ++slot) {
            if (grid.at(slot).item == item) {
                have += grid.at(slot).count;
            }
        }
        if (have < need) {
            return false;
        }
    }
    return true;
}

GridMatch matchGridRecipe(const CraftingGrid& grid,
                          const RecipeRegistry& recipes) {
    const std::vector<Recipe>& all = recipes.all();
    // Shaped first (more specific), then shapeless; registry order within
    // a kind (deterministic — the registry loads files sorted by name).
    for (int pass = 0; pass < 2; ++pass) {
        const RecipeType wanted =
            pass == 0 ? RecipeType::Shaped : RecipeType::Shapeless;
        for (const Recipe& recipe : all) {
            if (recipe.type != wanted) {
                continue;
            }
            if (wanted == RecipeType::Shaped) {
                int dx = 0, dy = 0;
                if (matchesShaped(grid, recipe, &dx, &dy)) {
                    GridMatch m;
                    m.recipe = &recipe;
                    m.dx = dx;
                    m.dy = dy;
                    return m; // the repair fallback stays nullopt (UX-5)
                }
            } else if (matchesShapeless(grid, recipe)) {
                GridMatch m;
                m.recipe = &recipe;
                return m; // (0, 0) offset + no repair fallback (UX-5)
            }
        }
    }
    return GridMatch {};
}

void consumeGrid(CraftingGrid& grid, const GridMatch& match) {
    assert(match.matched());
    const Recipe& recipe = *match.recipe;
    if (recipe.type == RecipeType::Shaped) {
        // One item from each covered cell at the matched offset.
        const int rows = static_cast<int>(recipe.pattern.size());
        const int cols = static_cast<int>(recipe.pattern.front().size());
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const char ch =
                    recipe.pattern[static_cast<std::size_t>(r)][c];
                if (ch == ' ') {
                    continue;
                }
                const int slot =
                    (match.dy + r) * CraftingGrid::kColumns + (match.dx + c);
                grid.take(slot, 1);
            }
        }
        return;
    }
    // Shapeless: each ingredient in recipe order, from the lowest-index
    // occupied cell upward.
    for (const auto& [item, need] : recipe.ingredients) {
        int remaining = need;
        for (int slot = 0; slot < CraftingGrid::kSize && remaining > 0;
             ++slot) {
            if (grid.at(slot).item != item) {
                continue;
            }
            const int taken = grid.take(slot, remaining);
            remaining -= taken;
        }
    }
}

} // namespace mc
