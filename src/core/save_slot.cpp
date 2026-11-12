// SAVE-3: save slot management — see save_slot.hpp for the contract.
#include "core/save_slot.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

#include "core/json.hpp"
#include "core/save.hpp"

namespace mc {

bool isValidSlotName(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::filesystem::path slotWritePath(const std::filesystem::path& dir,
                                    const std::string& name) {
    return dir / (name + ".json.gz");
}

std::filesystem::path slotReadPath(const std::filesystem::path& dir,
                                   const std::string& name) {
    // V3-18: the two-stage find — the current write form (.json.gz)
    // first, the legacy name (.json — the V3-7-era file, its content
    // already the gzip container) after = zero migration (a legacy slot
    // loads and the NEXT write re-names it to the new form).
    const std::filesystem::path current = slotWritePath(dir, name);
    std::error_code ec;
    if (std::filesystem::is_regular_file(current, ec) && !ec) {
        return current;
    }
    const std::filesystem::path legacy = dir / (name + ".json");
    if (std::filesystem::is_regular_file(legacy, ec) && !ec) {
        return legacy;
    }
    return {};
}

namespace {

std::string readBytes(const std::filesystem::path& path, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open save file '" + path.string() + "'";
        return {};
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

Result<std::vector<std::string>> listSaveSlots(
    const std::filesystem::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        return names; // no directory = no slots (a fresh checkout)
    }
    if (ec) {
        return MC_FAIL("cannot stat save directory '" + dir.string() +
                       "': " + ec.message());
    }
    if (!std::filesystem::is_directory(dir, ec) || ec) {
        return MC_FAIL("save path '" + dir.string() +
                       "' is not a directory");
    }
    // V3-18: BOTH extensions enumerate (the new .json.gz form + the
    // legacy .json); a slot holding both surfaces ONCE (the std::set's
    // dedup — the .json.gz wins the summary find via slotReadPath).
    std::set<std::string> seen;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec) || ec) {
            continue; // subdirectories and special files are not slots
        }
        const std::string p = entry.path().filename().string();
        std::string stem;
        constexpr const char* kGzExt = ".json.gz";
        constexpr const char* kLegacyExt = ".json";
        if (p.size() > std::char_traits<char>::length(kGzExt) &&
            p.compare(p.size() - std::char_traits<char>::length(kGzExt),
                      std::char_traits<char>::length(kGzExt), kGzExt) == 0) {
            stem = p.substr(0, p.size() -
                                 std::char_traits<char>::length(kGzExt));
        } else if (p.size() > std::char_traits<char>::length(kLegacyExt) &&
                   p.compare(p.size() - std::char_traits<char>::length(kLegacyExt),
                             std::char_traits<char>::length(kLegacyExt),
                             kLegacyExt) == 0) {
            stem = p.substr(0, p.size() -
                                 std::char_traits<char>::length(kLegacyExt));
        } else {
            continue; // not a slot file
        }
        seen.insert(stem);
    }
    names.assign(seen.begin(), seen.end()); // sorted (the set's order)
    return names;
}

Result<SaveSlotSummary> readSaveSlotSummary(
    const std::filesystem::path& dir, const std::string& name) {
    if (!isValidSlotName(name)) {
        return MC_FAIL("invalid save slot name '" + name +
                       "' (letters, digits, '_' or '-')");
    }
    SaveSlotSummary out;
    out.name = name;
    const std::filesystem::path path = slotReadPath(dir, name);
    if (path.empty()) {
        out.problem = "no such save slot '" + name + "'";
        return out;
    }
    std::string openError;
    const std::string data = readBytes(path, openError);
    if (!openError.empty()) {
        out.problem = openError;
        return out;
    }
    // V3-7: the on-disk encoding probe (the readSaveFile twin): a gzip
    // slot (this build's write default) decodes before the JSON parse;
    // a legacy plaintext slot (format 28 and older) reads straight
    // through (zero migration). A decode failure is a NAMED problem —
    // the file surfaces in the list, it is never hidden (the existing
    // problem-naming discipline).
    std::string text = data;
    if (looksLikeGzip(data)) {
        auto decoded = gzipDecode(data, path.string());
        if (!decoded.ok()) {
            out.problem = "invalid gzip: " + decoded.error().message;
            return out;
        }
        text = std::move(*decoded);
    }
    auto parsed = Json::parse(text);
    if (!parsed.ok()) {
        out.problem = "invalid JSON: " + parsed.error().message;
        return out;
    }
    const Json* fmt = parsed->find("format");
    if (fmt == nullptr || !fmt->isNumber()) {
        out.problem = "no numeric format field";
        return out;
    }
    out.readable = true;
    out.format = fmt->asInt();
    if (const Json* seed = parsed->find("seed")) {
        out.seed = seed->asInt64();
    }
    if (const Json* time = parsed->find("time")) {
        out.time = time->asDouble();
    }
    if (const Json* player = parsed->find("player")) {
        if (const Json* pos = player->find("pos")) {
            if (pos->isArray() && pos->size() == 3) {
                out.posX = pos->at(0).asDouble();
                out.posY = pos->at(1).asDouble();
                out.posZ = pos->at(2).asDouble();
            }
        }
        if (const Json* hp = player->find("health")) {
            out.health = hp->asDouble();
        }
    }
    return out;
}

Result<void> copySaveSlot(const std::filesystem::path& dir,
                          const std::string& src, const std::string& dst) {
    if (!isValidSlotName(src)) {
        return MC_FAIL("invalid save slot name '" + src +
                       "' (letters, digits, '_' or '-')");
    }
    if (!isValidSlotName(dst)) {
        return MC_FAIL("invalid save slot name '" + dst +
                       "' (letters, digits, '_' or '-')");
    }
    const std::filesystem::path from = slotReadPath(dir, src);
    if (from.empty()) {
        return MC_FAIL("no such save slot '" + src + "'");
    }
    const std::filesystem::path to = slotWritePath(dir, dst);
    if (!slotReadPath(dir, dst).empty()) {
        return MC_FAIL("save slot '" + dst + "' already exists (delete it "
                       "first or pick another name)");
    }
    std::string openError;
    const std::string data = readBytes(from, openError);
    if (!openError.empty()) {
        return MC_FAIL("copying save slot '" + src + "': " + openError);
    }
    // SAVE-1 atomic write: the destination holds either nothing (a
    // failed copy) or the complete source bytes — never half. V3-18:
    // the destination is written in the NEW form (the copy re-names a
    // legacy slot — zero-migration's gentle path).
    return writeAtomicFile(to, data);
}

Result<void> renameSaveSlot(const std::filesystem::path& dir,
                            const std::string& src, const std::string& dst) {
    if (!isValidSlotName(src)) {
        return MC_FAIL("invalid save slot name '" + src +
                       "' (letters, digits, '_' or '-')");
    }
    if (!isValidSlotName(dst)) {
        return MC_FAIL("invalid save slot name '" + dst +
                       "' (letters, digits, '_' or '-')");
    }
    const std::filesystem::path from = slotReadPath(dir, src);
    if (from.empty()) {
        return MC_FAIL("no such save slot '" + src + "'");
    }
    if (!slotReadPath(dir, dst).empty()) {
        return MC_FAIL("save slot '" + dst + "' already exists");
    }
    const std::filesystem::path to = slotWritePath(dir, dst);
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec) {
        return MC_FAIL("cannot rename save slot '" + src + "' to '" + dst +
                       "': " + ec.message());
    }
    // V3-18: the slot is consumed WHOLE — if the source held BOTH forms
    // (a .json.gz + a legacy .json), the moved one is gone but the twin
    // lingers: delete it (the rename moves the slot, not one of its
    // files).
    const std::filesystem::path twin = slotReadPath(dir, src);
    if (!twin.empty()) {
        std::filesystem::remove(twin, ec);
        if (ec) {
            return MC_FAIL("cannot delete the renamed slot's twin file '" +
                           twin.string() + "': " + ec.message());
        }
    }
    return {};
}

Result<void> deleteSaveSlot(const std::filesystem::path& dir,
                            const std::string& name) {
    if (!isValidSlotName(name)) {
        return MC_FAIL("invalid save slot name '" + name +
                       "' (letters, digits, '_' or '-')");
    }
    const std::filesystem::path found = slotReadPath(dir, name);
    if (found.empty()) {
        return MC_FAIL("no such save slot '" + name + "'");
    }
    std::error_code ec;
    std::filesystem::remove(found, ec);
    if (ec) {
        return MC_FAIL("cannot delete save slot '" + name + "': " +
                       ec.message());
    }
    // V3-18: the slot is consumed WHOLE (the twin form, if present).
    const std::filesystem::path twin = slotReadPath(dir, name);
    if (!twin.empty()) {
        std::filesystem::remove(twin, ec);
        if (ec) {
            return MC_FAIL("cannot delete save slot '" + name + "' twin '" +
                           twin.string() + "': " + ec.message());
        }
    }
    return {};
}

} // namespace mc
