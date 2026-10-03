#include "core/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mc {

// ---------------------------------------------------------------------------
// Parser (recursive descent)
// ---------------------------------------------------------------------------
namespace {

// Forward declaration (defined below the Parser).
void appendUtf8(std::string& out, unsigned int cp);

struct Parser {
    std::string_view src;
    std::size_t pos = 0;

    // -- helpers ------------------------------------------------------------
    [[nodiscard]] std::string position() const {
        std::size_t line = 1, col = 1;
        for (std::size_t i = 0; i < pos && i < src.size(); ++i) {
            if (src[i] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        return "at line " + std::to_string(line) + " column " + std::to_string(col);
    }

    // Returns a bare Error; Result<T> converts from it implicitly, so
    // `return fail("...")` works in every parser function.
    [[nodiscard]] Error fail(std::string what) const {
        return mc::Error(what + " " + position());
    }

    void skipWs() {
        while (pos < src.size() &&
               (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' ||
                src[pos] == '\r')) {
            ++pos;
        }
    }

    [[nodiscard]] bool consume(char c) {
        if (pos < src.size() && src[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool literal(const char* lit) {
        const std::size_t n = std::strlen(lit);
        if (src.substr(pos, n) == lit) {
            pos += n;
            return true;
        }
        return false;
    }

    // -- grammar ------------------------------------------------------------
    [[nodiscard]] Result<Json> parseValue() {
        skipWs();
        if (pos >= src.size()) {
            return fail("unexpected end of input, expected a JSON value");
        }
        const char c = src[pos];
        switch (c) {
            case '{': {
                auto o = parseObject();
                if (!o.ok()) {
                    return o.error();
                }
                return Json(std::move(*o));
            }
            case '[': {
                auto a = parseArray();
                if (!a.ok()) {
                    return a.error();
                }
                return Json(std::move(*a));
            }
            case '"': {
                auto s = parseString();
                if (!s.ok()) {
                    return s.error();
                }
                return Json(std::move(*s));
            }
            case 't':
                if (!literal("true")) {
                    return fail("invalid literal, expected 'true'");
                }
                return Json(true);
            case 'f':
                if (!literal("false")) {
                    return fail("invalid literal, expected 'false'");
                }
                return Json(false);
            case 'n':
                if (!literal("null")) {
                    return fail("invalid literal, expected 'null'");
                }
                return Json(nullptr);
            default:
                if (c == '-' || (c >= '0' && c <= '9')) {
                    return parseNumber();
                }
                return fail(std::string("unexpected character '") + c + "'");
        }
    }

    [[nodiscard]] Result<Json::Object> parseObject() {
        ++pos; // '{'
        Json::Object obj;
        skipWs();
        if (consume('}')) {
            return obj;
        }
        while (true) {
            skipWs();
            if (!consume('"')) {
                return fail("expected object key string");
            }
            auto key = parseStringRaw();
            if (!key.ok()) {
                return key.error();
            }
            skipWs();
            if (!consume(':')) {
                return fail("expected ':' after object key");
            }
            auto val = parseValue();
            if (!val.ok()) {
                return val.error();
            }
            obj.emplace(std::move(*key), std::move(*val));
            skipWs();
            if (consume(',')) {
                continue;
            }
            if (consume('}')) {
                return obj;
            }
            return fail("expected ',' or '}' in object");
        }
    }

    [[nodiscard]] Result<Json::Array> parseArray() {
        ++pos; // '['
        Json::Array arr;
        skipWs();
        if (consume(']')) {
            return arr;
        }
        while (true) {
            auto val = parseValue();
            if (!val.ok()) {
                return val.error();
            }
            arr.push_back(std::move(*val));
            skipWs();
            if (consume(',')) {
                continue;
            }
            if (consume(']')) {
                return arr;
            }
            return fail("expected ',' or ']' in array");
        }
    }

    // String body without the opening quote (caller consumed it).
    [[nodiscard]] Result<std::string> parseStringRaw() {
        std::string out;
        while (pos < src.size()) {
            const char c = src[pos++];
            if (c == '"') {
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("unescaped control character in string");
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos >= src.size()) {
                return fail("unterminated escape sequence");
            }
            const char e = src[pos++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    auto cp = parseHex4();
                    if (!cp.ok()) {
                        return cp.error();
                    }
                    unsigned int code = *cp;
                    // Combine UTF-16 surrogate pair if present.
                    if (code >= 0xD800 && code <= 0xDBFF &&
                        src.substr(pos, 2) == "\\u") {
                        pos += 2;
                        auto lo = parseHex4();
                        if (!lo.ok()) {
                            return lo.error();
                        }
                        if (*lo < 0xDC00 || *lo > 0xDFFF) {
                            return fail("invalid low surrogate in string");
                        }
                        code = 0x10000 + ((code - 0xD800) << 10) + (*lo - 0xDC00);
                    }
                    appendUtf8(out, code);
                    break;
                }
                default:
                    return fail(std::string("invalid escape character '\\'") + e);
            }
        }
        return fail("unterminated string");
    }

    [[nodiscard]] Result<std::string> parseString() {
        if (!consume('"')) {
            return fail("expected string");
        }
        return parseStringRaw();
    }

    [[nodiscard]] Result<unsigned int> parseHex4() {
        if (pos + 4 > src.size()) {
            return fail("truncated \\u escape");
        }
        unsigned int v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = src[pos++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                return fail("invalid hex digit in \\u escape");
            }
        }
        return v;
    }

    [[nodiscard]] Result<Json> parseNumber() {
        const std::size_t start = pos;
        if (consume('-')) {}
        while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') {
            ++pos;
        }
        bool isFloat = false;
        if (pos < src.size() && src[pos] == '.') {
            isFloat = true;
            ++pos;
            if (pos >= src.size() || src[pos] < '0' || src[pos] > '9') {
                return fail("expected digit after decimal point");
            }
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') {
                ++pos;
            }
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            isFloat = true;
            ++pos;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) {
                ++pos;
            }
            if (pos >= src.size() || src[pos] < '0' || src[pos] > '9') {
                return fail("expected digit in exponent");
            }
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') {
                ++pos;
            }
        }
        (void)isFloat;
        const std::string num(src.substr(start, pos - start));
        if (num.size() == 1 && num[0] == '-') {
            return fail("invalid number");
        }
        char* end = nullptr;
        const double v = std::strtod(num.c_str(), &end);
        if (end == nullptr || *end != '\0') {
            return fail("invalid number '" + num + "'");
        }
        return Json(v);
    }
};

// Encode a code point (max 0x10FFFF) as UTF-8.
void appendUtf8(std::string& out, unsigned int cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Parser entry point
// ---------------------------------------------------------------------------

Result<Json> Json::parse(std::string_view text) {
    Parser p{text};
    auto v = p.parseValue();
    if (!v.ok()) {
        return v.error();
    }
    p.skipWs();
    if (p.pos != p.src.size()) {
        return p.fail("trailing characters after JSON value");
    }
    // Return v directly: NRVO constructs the result in place, which also
    // avoids a GCC -Wmaybe-uninitialized false positive that appears in
    // std::variant's in_place_index constructor path at -O3.
    return v;
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

void Json::escapeString(std::string& out, std::string_view s) {
    out.push_back('"');
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

void Json::dumpTo(std::string& out, int indent, int depth) const {
    const std::string pad = indent >= 0 ? std::string(static_cast<std::size_t>(indent) * depth, ' ') : "";
    const std::string padInner = indent >= 0 ? std::string(static_cast<std::size_t>(indent) * (depth + 1), ' ') : "";
    const char* nl = indent >= 0 ? "\n" : "";
    const char* sep = indent >= 0 ? ": " : ":";

    if (isNull()) {
        out += "null";
    } else if (isBool()) {
        out += std::get<bool>(m_value) ? "true" : "false";
    } else if (isNumber()) {
        const double v = std::get<double>(m_value);
        if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 9.007199254740992e15) {
            out += std::to_string(static_cast<long long>(v));
        } else if (!std::isfinite(v)) {
            // JSON has no inf/nan; emit null (parser will see it as null).
            out += "null";
        } else {
            // Shortest representation that round-trips to the same double.
            char buf[40];
            for (int prec = 1; prec <= 17; ++prec) {
                std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
                if (std::strtod(buf, nullptr) == v) {
                    break;
                }
            }
            out += buf;
        }
    } else if (isString()) {
        escapeString(out, std::get<std::string>(m_value));
    } else if (isArray()) {
        const auto& arr = std::get<Array>(m_value);
        if (arr.empty()) {
            out += "[]";
            return;
        }
        out.push_back('[');
        out += nl;
        for (std::size_t i = 0; i < arr.size(); ++i) {
            out += padInner;
            arr[i].dumpTo(out, indent, depth + 1);
            if (i + 1 < arr.size()) {
                out.push_back(',');
            }
            out += nl;
        }
        out += pad;
        out.push_back(']');
    } else {
        const auto& obj = std::get<Object>(m_value);
        if (obj.empty()) {
            out += "{}";
            return;
        }
        out.push_back('{');
        out += nl;
        std::size_t i = 0;
        for (const auto& [k, v] : obj) {
            out += padInner;
            escapeString(out, k);
            out += sep;
            v.dumpTo(out, indent, depth + 1);
            if (++i < obj.size()) {
                out.push_back(',');
            }
            out += nl;
        }
        out += pad;
        out.push_back('}');
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

} // namespace mc
