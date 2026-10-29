// SAVE-3: save slot management — see save_slot.hpp for the contract.
#include "core/save_slot.hpp"

#include <algorithm>
#include <fstream>
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

namespace {

const std::filesystem::path slotPath(const std::filesystem::path& dir,
                                     const std::string& name) {
    return dir / (name + ".json");
}

std::string readFileBytes(const std::filesystem::path& path,
                          std::string& error) {
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
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec) || ec) {
            continue; // subdirectories and special files are not slots
        }
        const std::string p = entry.path().filename().string();
        constexpr const char* kExt = ".json";
        constexpr std::size_t kExtLen = 5;
        if (p.size() <= kExtLen ||
            p.compare(p.size() - kExtLen, kExtLen, kExt) != 0) {
            continue; // not a slot file
        }
        names.push_back(p.substr(0, p.size() - kExtLen));
    }
    std::sort(names.begin(), names.end()); // deterministic order
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
    const std::filesystem::path path = slotPath(dir, name);
    std::string openError;
    const std::string data = readFileBytes(path, openError);
    if (!openError.empty()) {
        out.problem = openError;
        return out;
    }
    auto parsed = Json::parse(data);
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
    const std::filesystem::path from = slotPath(dir, src);
    const std::filesystem::path to = slotPath(dir, dst);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(from, ec)) {
        return MC_FAIL("no such save slot '" + src + "' (" +
                       from.string() + " is not a file)");
    }
    if (ec) {
        return MC_FAIL("cannot stat save slot '" + src + "': " + ec.message());
    }
    if (std::filesystem::exists(to, ec)) {
        return MC_FAIL("save slot '" + dst + "' already exists (delete it "
                       "first or pick another name)");
    }
    std::string openError;
    const std::string data = readFileBytes(from, openError);
    if (!openError.empty()) {
        return MC_FAIL("copying save slot '" + src + "': " + openError);
    }
    // SAVE-1 atomic write: the destination holds either nothing (a
    // failed copy) or the complete source bytes — never half.
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
    const std::filesystem::path from = slotPath(dir, src);
    const std::filesystem::path to = slotPath(dir, dst);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(from, ec)) {
        return MC_FAIL("no such save slot '" + src + "' (" +
                       from.string() + " is not a file)");
    }
    if (ec) {
        return MC_FAIL("cannot stat save slot '" + src + "': " + ec.message());
    }
    if (std::filesystem::exists(to, ec)) {
        return MC_FAIL("save slot '" + dst + "' already exists");
    }
    std::filesystem::rename(from, to, ec);
    if (ec) {
        return MC_FAIL("cannot rename save slot '" + src + "' to '" + dst +
                       "': " + ec.message());
    }
    return {};
}

Result<void> deleteSaveSlot(const std::filesystem::path& dir,
                            const std::string& name) {
    if (!isValidSlotName(name)) {
        return MC_FAIL("invalid save slot name '" + name +
                       "' (letters, digits, '_' or '-')");
    }
    const std::filesystem::path path = slotPath(dir, name);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return MC_FAIL("no such save slot '" + name + "' (" + path.string() +
                       " is not a file)");
    }
    if (ec) {
        return MC_FAIL("cannot stat save slot '" + name + "': " + ec.message());
    }
    std::filesystem::remove(path, ec);
    if (ec) {
        return MC_FAIL("cannot delete save slot '" + name + "': " +
                       ec.message());
    }
    return {};
}

} // namespace mc
