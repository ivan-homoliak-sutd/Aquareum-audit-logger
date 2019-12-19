// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2013-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

/// @file
/// Very common stuff (i.e. that every other header needs except vector_ref.h).
#pragma once

// // way too many unsigned to size_t warnings in 32 bit build
// #ifdef _M_IX86
// #pragma warning(disable : 4244)
// #endif

#if _MSC_VER && _MSC_VER < 1900
#define _ALLOW_KEYWORD_MACROS
#define noexcept throw()
#endif

// #ifdef __INTEL_COMPILER
// #pragma warning(disable:3682) //call through incomplete class
// #endif


#include "intx/intx.hpp"

#include "vector_ref.h"
#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// CryptoPP defines byte in the global namespace, so must we.
using byte = uint8_t;

#define DEV_IGNORE_EXCEPTIONS(X) \
    try {                        \
        X;                       \
    } catch (...) {              \
    }

#define DEV_IF_THROWS(X) \
    try {                \
        X;               \
    } catch (...)

namespace dev
{
extern std::string const EmptyString;

typedef struct {
    uint8_t data[20];
} eth_addr_t;

// Binary data types.
using byte = uint8_t;
using bytes = std::vector<byte>;
using bytesRef = vector_ref<byte>;
using bytesConstRef = vector_ref<byte const>;

template <class T>
class secure_vector {
public:
    secure_vector() {}
    secure_vector(secure_vector<T> const& /*_c*/) = default;  // See https://github.com/ethereum/libweb3core/pull/44
    explicit secure_vector(size_t _size)
      : m_data(_size) {}
    explicit secure_vector(size_t _size, T _item)
      : m_data(_size, _item) {}
    explicit secure_vector(std::vector<T> const& _c)
      : m_data(_c) {}
    explicit secure_vector(vector_ref<T> _c)
      : m_data(_c.data(), _c.data() + _c.size()) {}
    explicit secure_vector(vector_ref<const T> _c)
      : m_data(_c.data(), _c.data() + _c.size()) {}
    ~secure_vector() { ref().cleanse(); }

    secure_vector<T>& operator=(secure_vector<T> const& _c) {
        if (&_c == this)
            return *this;

        ref().cleanse();
        m_data = _c.m_data;
        return *this;
    }
    std::vector<T>& writable() {
        clear();
        return m_data;
    }
    std::vector<T> const& makeInsecure() const { return m_data; }

    void clear() { ref().cleanse(); }

    vector_ref<T> ref() { return vector_ref<T>(&m_data); }
    vector_ref<T const> ref() const { return vector_ref<T const>(&m_data); }

    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }

    void swap(secure_vector<T>& io_other) { m_data.swap(io_other.m_data); }

private:
    std::vector<T> m_data;
};

using bytesSec = secure_vector<byte>;

// Numeric types.
using u512 = intx::uint512;
using u256 = intx::uint256;
using u160 = intx::uint256;  //eth_addr_t; // TODO: let see whether it will work with eEVM, or we will need to use u256
using u256s = std::vector<u256>;
using u160s = std::vector<u160>;
using u256Set = std::set<u256>;
using u160Set = std::set<u160>;

using bigint = u256;

// Map types.
using StringMap = std::map<std::string, std::string>;
using BytesMap = std::map<bytes, bytes>;
using u256Map = std::map<u256, u256>;
using HexMap = std::map<bytes, bytes>;

// Hash types.
using StringHashMap = std::unordered_map<std::string, std::string>;
using u256HashMap = std::unordered_map<u256, u256>;

// String types.
using strings = std::vector<std::string>;

// Null/Invalid values for convenience.
extern bytes const NullBytes;

/// Converts given int to a string and appends one of a series of units according to its size.
std::string inUnits(bigint const& _b, strings const& _units);

/// @returns the smallest n >= 0 such that (1 << n) >= _x
inline unsigned int toLog2(u256 _x) {
    unsigned ret;
    for (ret = 0; _x >>= 1; ++ret) {
    }
    return ret;
}


/// RAII utility class whose destructor calls a given function.
class ScopeGuard {
public:
    ScopeGuard(std::function<void(void)> _f)
      : m_f(_f) {}
    ~ScopeGuard() { m_f(); }

private:
    std::function<void(void)> m_f;
};

/// Inheritable for classes that have invariants.
class HasInvariants {
public:
    /// Reimplement to specify the invariants.
    virtual bool invariants() const = 0;
};

/// RAII checker for invariant assertions.
class InvariantChecker {
public:
    InvariantChecker(HasInvariants* _this, char const* _fn, char const* _file, int _line)
      : m_this(_this), m_function(_fn), m_file(_file), m_line(_line) {
        checkInvariants(_this, _fn, _file, _line, true);
    }
    ~InvariantChecker() { checkInvariants(m_this, m_function, m_file, m_line, false); }
    /// Check invariants are met, throw if not.
    static void checkInvariants(
        HasInvariants const* _this, char const* _fn, char const* _file, int line, bool _pre);

private:
    HasInvariants const* m_this;
    char const* m_function;
    char const* m_file;
    int m_line;
};


/// Simple scope-based timer helper.
class TimerHelper {
public:
    TimerHelper(std::string const& _id, unsigned _msReportWhenGreater = 0)
      : m_t(std::chrono::high_resolution_clock::now()), m_id(_id), m_ms(_msReportWhenGreater) {}
    ~TimerHelper();

private:
    std::chrono::high_resolution_clock::time_point m_t;
    std::string m_id;
    unsigned m_ms;
};

class Timer {
public:
    Timer() { restart(); }

    std::chrono::high_resolution_clock::duration duration() const {
        return std::chrono::high_resolution_clock::now() - m_t;
    }
    double elapsed() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(duration()).count() /
               1000000.0;
    }
    void restart() { m_t = std::chrono::high_resolution_clock::now(); }

private:
    std::chrono::high_resolution_clock::time_point m_t;
};


/// Get the current time in seconds since the epoch in UTC
int64_t utcTime();

void setDefaultOrCLocale();

static constexpr unsigned c_lineWidth = 160;

static const auto c_steadyClockMin = std::chrono::steady_clock::time_point::min();

class ExitHandler {
public:
    static void exitHandler(int) { s_shouldExit = true; }
    bool shouldExit() const { return s_shouldExit; }

private:
    static bool s_shouldExit;
};

bool isTrue(std::string const& _m);
bool isFalse(std::string const& _m);
}  // namespace dev
