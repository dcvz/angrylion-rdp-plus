#pragma once

// inlining
#define INLINE inline

#ifdef _MSC_VER
#define STRICTINLINE __forceinline
#elif defined(__GNUC__)
#define STRICTINLINE __attribute__((always_inline))
#else
#define STRICTINLINE inline
#endif

// thread-local storage
#if defined(_MSC_VER)
    #define TLS __declspec(thread)
#elif defined(__GNUC__)
    #define TLS __thread
#else
    #define TLS _Thread_local // C11
#endif

// endianness
#define LSB_FIRST 1 
#ifdef LSB_FIRST
    #define BYTE_ADDR_XOR       3
    #define WORD_ADDR_XOR       1
    #define BYTE4_XOR_BE(a)     ((a) ^ BYTE_ADDR_XOR)
#else
    #define BYTE_ADDR_XOR       0
    #define WORD_ADDR_XOR       0
    #define BYTE4_XOR_BE(a)     (a)
#endif

#ifdef LSB_FIRST
    #define BYTE_XOR_DWORD_SWAP 7
    #define WORD_XOR_DWORD_SWAP 3
#else
    #define BYTE_XOR_DWORD_SWAP 4
    #define WORD_XOR_DWORD_SWAP 2
#endif

#define DWORD_XOR_DWORD_SWAP 1

// RGBA5551 to RGBA8888 helper
#define GET_LOW(x)  (((x) & 0x3e) << 2)
#define GET_MED(x)  (((x) & 0x7c0) >> 3)
#define GET_HI(x)   (((x) >> 8) & 0xf8)
