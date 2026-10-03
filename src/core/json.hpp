// Minimal, dependency-free JSON parser/serializer.
//
// Supports the full JSON grammar (RFC 8259): objects, arrays, strings (with
// \uXXXX escapes including surrogate pairs), numbers, booleans, null.
//
// Used for: game configuration, and (later phases) data-driven content such
// as block/item/recipe definitions. Parsed values are immutable views;
// mutation helpers are provided only where needed.
#pragma once

#include <cassert>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/error.hpp"

namespace mc {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    Json() = default;
    Json(std::nullptr_t) : m_value(nullptr) {}
    Json(bool b) : m_value(b) {}
    Json(int v) : m_value(static_cast<double>(v)) {}
    Json(long long v) : m_value(static_cast<double>(v)) {}
    Json(unsigned long long v) : m_value(static_cast<double>(v)) {}
    Json(double v) : m_value(v) {}
    Json(const char* s) : m_value(std::string(s)) {}
    Json(std::string s) : m_value(std::move(s)) {}
    Json(Array a) : m_value(std::move(a)) {}
    Json(Object o) : m_value(std::move(o)) {}

    static Json object() { return Json(Object {}); }
    static Json array() { return Json(Array {}); }

    // -- type checks -------------------------------------------------------
    bool isNull() const { return std::holds_alternative<std::nullptr_t>(m_value); }
    bool isBool() const { return std::holds_alternative<bool>(m_value); }
    bool isNumber() const { return std::holds_alternative<double>(m_value); }
    bool isString() const { return std::holds_alternative<std::string>(m_value); }
    bool isArray() const { return std::holds_alternative<Array>(m_value); }
    bool isObject() const { return std::holds_alternative<Object>(m_value); }

    // -- typed access (returns the default when the type does not match) ----
    bool asBool(bool def = false) const {
        return isBool() ? std::get<bool>(m_value) : def;
    }
    double asDouble(double def = 0.0) const {
        return isNumber() ? std::get<double>(m_value) : def;
    }
    int asInt(int def = 0) const {
        return isNumber() ? static_cast<int>(std::get<double>(m_value)) : def;
    }
    long long asInt64(long long def = 0) const {
        return isNumber() ? static_cast<long long>(std::get<double>(m_value)) : def;
    }
    std::string asString(const std::string& def = "") const {
        return isString() ? std::get<std::string>(m_value) : def;
    }

    // -- object access -------------------------------------------------------
    // Returns a pointer to the member, or nullptr if this is not an object
    // or the key is absent.
    const Json* find(const std::string& key) const {
        if (!isObject()) {
            return nullptr;
        }
        const auto& obj = std::get<Object>(m_value);
        const auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }
    Json* find(const std::string& key) {
        if (!isObject()) {
            return nullptr;
        }
        auto& obj = std::get<Object>(m_value);
        auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }
    bool contains(const std::string& key) const { return find(key) != nullptr; }
    std::size_t memberCount() const {
        return isObject() ? std::get<Object>(m_value).size() : 0;
    }

    // -- array access --------------------------------------------------------
    std::size_t size() const { return isArray() ? std::get<Array>(m_value).size() : 0; }
    const Json& at(std::size_t index) const {
        // Indexing an out-of-range or non-array value is a programming error.
        assert(isArray() && index < std::get<Array>(m_value).size());
        return std::get<Array>(m_value)[index];
    }

    // -- mutation ------------------------------------------------------------
    Json& operator[](const std::string& key) {
        assert(isObject());
        return std::get<Object>(m_value)[key];
    }
    void push_back(Json v) {
        assert(isArray());
        std::get<Array>(m_value).push_back(std::move(v));
    }

    // -- I/O -----------------------------------------------------------------
    // Serialize. indent < 0: compact; indent >= 0: pretty-printed.
    std::string dump(int indent = -1) const;

    // Parse. On failure the Result carries a message including line:column.
    static Result<Json> parse(std::string_view text);

private:
    void dumpTo(std::string& out, int indent, int depth) const;
    static void escapeString(std::string& out, std::string_view s);

    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> m_value;
};

} // namespace mc
