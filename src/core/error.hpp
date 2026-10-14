// Minimal error-handling utilities for TokenMine.cpp.
//
// Design: a function either succeeds and returns its value, or fails with an
// Error carrying a message plus the source location where the failure was
// reported. This keeps "what happened, where it happened" explicit, per the
// project's debugging requirements.
#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace mc {

struct Error {
    std::string message;
    std::string location; // "file:line" of the failing call site

    Error() = default;
    explicit Error(std::string msg) : message(std::move(msg)) {}
    Error(std::string msg, std::string loc)
        : message(std::move(msg)), location(std::move(loc)) {}

    std::string describe() const {
        return location.empty() ? message : location + ": " + message;
    }
};

// Build an Error tagged with the current source location.
inline Error makeError(std::string message, const char* file = nullptr, int line = 0) {
    Error e(std::move(message));
    if (file) {
        e.location = std::string(file) + ":" + std::to_string(line);
    }
    return e;
}

// Result<T>: holds either a successful value or an Error.
//
//   auto r = doWork();
//   if (!r.ok()) { log(r.error().describe()); }
//   T v = std::move(r).value();
template <typename T>
class Result {
public:
    Result(T value) : m_data(std::in_place_index<0>, std::move(value)) {}
    Result(Error err) : m_data(std::in_place_index<1>, std::move(err)) {}

    bool ok() const { return m_data.index() == 0; }
    explicit operator bool() const { return ok(); }

    T& value() & { assert(ok()); return std::get<0>(m_data); }
    const T& value() const& { assert(ok()); return std::get<0>(m_data); }
    T&& value() && { assert(ok()); return std::move(std::get<0>(m_data)); }

    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

    const Error& error() const& { assert(!ok()); return std::get<1>(m_data); }

private:
    std::variant<T, Error> m_data;
};

// Specialization for void-returning functions (Status-like usage).
template <>
class Result<void> {
public:
    Result() = default;
    Result(Error err) : m_error(std::move(err)) {}

    bool ok() const { return !m_error.has_value(); }
    explicit operator bool() const { return ok(); }

    const Error& error() const { assert(!ok()); return *m_error; }

private:
    std::optional<Error> m_error;
};

// Propagate a failure from a Result-returning expression:
//
//   MC_TRY(loadChunk(x, z));
//
// In a function returning Result<T> this returns the Error (implicit
// conversion). In a function returning Result<void> the same applies.
// Const reference (no copy, no move): works for both temporaries and named
// Results, and leaves the original Result fully usable on the success path
// (e.g. `auto r = f(); MC_TRY(r); use(r.value());`).
#define MC_TRY(expr)                                                             \
    do {                                                                         \
        const auto& mc_try_result_ = (expr);                                     \
        if (!mc_try_result_.ok()) {                                              \
            return mc_try_result_.error();                                       \
        }                                                                        \
    } while (0)

// Build a location-tagged error at the call site. Use inside any function
// whose return type is Result<T> or Result<void>:
//
//   if (x < 0) return MC_FAIL("x must be non-negative, got " + std::to_string(x));
#define MC_FAIL(msg) mc::makeError((msg), __FILE__, __LINE__)

} // namespace mc
