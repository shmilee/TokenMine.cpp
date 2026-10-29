// SAVE-3: save slot management (the data/saves/*.json files).
//
// The game's save is opt-in (`--save <slot>`), and several slots are just
// several files switched by hand on the command line. This module gives
// that surface proper management: a deterministic list (name-sorted; the
// summary is a function of the FILE CONTENTS alone — no mtime, no wall
// clock, so repeated runs print byte-identically), a byte-faithful copy
// (branch a save), a rename, and a delete. All operations are strict
// (Result + named errors, the project discipline) and POSIX-atomic where
// a write is involved (copy = same-directory temp + rename, the SAVE-1
// writeAtomicFile twin; rename = the atomic std::filesystem::rename).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/error.hpp"

namespace mc {

// The summary of one save slot, for `--list-saves`. Everything comes
// from the file contents (format / seed / time / player pos+health);
// a file that cannot be parsed (or lacks the summary fields) still
// appears, marked unreadable with the naming (the management surface
// shows the problem instead of hiding the file).
struct SaveSlotSummary {
    std::string name; // slot name (the file name without .json)
    bool readable = false;
    std::string problem; // why unreadable ("" when readable)
    int format = 0;
    std::int64_t seed = 0;
    double time = 0.0; // timeOfDay [0, 1)
    double posX = 0.0;
    double posY = 0.0;
    double posZ = 0.0;
    double health = 0.0;

    bool operator==(const SaveSlotSummary& o) const {
        return name == o.name && readable == o.readable &&
               problem == o.problem && format == o.format &&
               seed == o.seed && time == o.time && posX == o.posX &&
               posY == o.posY && posZ == o.posZ && health == o.health;
    }
};

// A slot name is a non-empty string of letters, digits, '_' and '-'
// (the exact `--save` charset — main.cpp's argument check). The charset
// also kills path separators / traversal, so a slot name is always a
// plain file name inside data/saves.
bool isValidSlotName(const std::string& name);

// Lists the save slots in `dir` (file names minus .json, sorted).
// Non-JSON entries and subdirectories are not slots.
Result<std::vector<std::string>> listSaveSlots(
    const std::filesystem::path& dir);

// Reads one slot's summary from `dir / (name + ".json")`. The slot name
// must be valid; a missing file is a naming; a file that does not parse
// (or lacks the summary fields) comes back readable=false with the
// naming (the list must show the problem, not hide the file).
Result<SaveSlotSummary> readSaveSlotSummary(
    const std::filesystem::path& dir, const std::string& name);

// Byte-faithful copy (branch a save). The destination must NOT exist
// (named); the write is atomic (same-directory temp + rename — a failed
// copy never leaves a half-written destination).
Result<void> copySaveSlot(const std::filesystem::path& dir,
                          const std::string& src, const std::string& dst);

// Rename (move). The destination must NOT exist (named); atomic
// (std::filesystem::rename on the same directory).
Result<void> renameSaveSlot(const std::filesystem::path& dir,
                            const std::string& src, const std::string& dst);

// Delete. The slot must exist (named).
Result<void> deleteSaveSlot(const std::filesystem::path& dir,
                            const std::string& name);

} // namespace mc
