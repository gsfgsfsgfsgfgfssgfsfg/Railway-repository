/*
 * skCrypt - Compile-time string encryption (C++17/20 compatible)
 * Stores encrypted bytes + XOR keys at compile time, decrypts at runtime.
 */

#ifndef SKCRYPT_HPP
#define SKCRYPT_HPP

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace skCrypt_Internal {
    constexpr const char* _time = __TIME__;
    constexpr unsigned seed =
        static_cast<unsigned>(_time[7]) +
        static_cast<unsigned>(_time[6]) * 10u +
        static_cast<unsigned>(_time[4]) * 60u +
        static_cast<unsigned>(_time[3]) * 600u +
        static_cast<unsigned>(_time[1]) * 3600u +
        static_cast<unsigned>(_time[0]) * 36000u;

    template <unsigned N>
    struct RNG {
    private:
        static constexpr unsigned a   = 16807u;
        static constexpr unsigned m   = 2147483647u;
        static constexpr unsigned s   = RNG<N - 1>::value;
        static constexpr unsigned lo  = a * (s & 0xFFFFu);
        static constexpr unsigned hi  = a * (s >> 16u);
        static constexpr unsigned lo2 = lo + ((hi & 0x7FFFu) << 16u);
        static constexpr unsigned lo3 = lo2 + (hi >> 15u);
    public:
        static constexpr unsigned value = lo3 > m ? lo3 - m : lo3;
    };

    template <>
    struct RNG<0> {
        static constexpr unsigned value = seed;
    };

    // Key byte for index I: always >= 1, fits in a char
    template <unsigned I>
    struct Key {
        static constexpr unsigned char value =
            static_cast<unsigned char>(1u + RNG<I + 1u>::value % 0x7Eu);
    };
}

// skCryptClass<N> - N includes the null terminator
template <unsigned N>
class skCryptClass {
    char _enc[N]{};   // encrypted bytes (compile-time)
    char _key[N]{};   // per-index keys   (compile-time)

    // Build encrypted array at compile time
    template <unsigned... Is>
    constexpr void _init(const char* str, std::integer_sequence<unsigned, Is...>) {
        ((_enc[Is] = static_cast<char>(str[Is] ^ skCrypt_Internal::Key<Is>::value),
          _key[Is] = static_cast<char>(skCrypt_Internal::Key<Is>::value)), ...);
    }

public:
    constexpr __forceinline skCryptClass(const char* str) {
        _init(str, std::make_integer_sequence<unsigned, N>{});
    }

    // Decrypt in-place and return pointer (call once)
    __forceinline char* decrypt() {
        for (unsigned i = 0; i < N - 1; ++i)
            _enc[i] = static_cast<char>(_enc[i] ^ _key[i]);
        _enc[N - 1] = '\0';
        return _enc;
    }

    __forceinline char*  get()  { return _enc; }
    __forceinline size_t size() const { return N; }
};

// Macro: deduces N automatically from string literal
#define skCrypt(str) (skCryptClass<sizeof(str)>(str))

#endif // SKCRYPT_HPP
