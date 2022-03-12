#pragma once
#include <stdint.h>
#include <x86intrin.h>
#include <smmintrin.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float  f32;
typedef double f64;

#pragma pack(push, 1)

typedef union u32x2 {
    struct {
        u32 x;
        u32 y;
    };

    #ifdef __cplusplus
    u32x2() { };
    u32x2(u32 x, u32 y) : x(x), y(y) { };
    #endif
} u32x2;

typedef union u32x3 {
    struct {
        u32 x;
        u32 y;
        u32 z;
    };

    #ifdef __cplusplus
    u32x3() { };
    u32x3(u32 x, u32 y, u32 z) : x(x), y(y), z(z) { };
    #endif
} u32x3;

typedef union u32x4 {
    struct {
        u32 x;
        u32 y;
        u32 z;
        u32 w;
    };

    __m128i m128i;

    #ifdef __cplusplus
    u32x4() { };
    u32x4(u32 x, u32 y, u32 z, u32 w) : x(x), y(y), z(z), w(w) { };
    u32x4(__m128i a) : m128i(a) { };
    operator __m128i() { return m128i; };
    #endif
} u32x4;

typedef union s32x2 {
    struct {
        s32 x;
        s32 y;
    };

    #ifdef __cplusplus
    s32x2() { };
    s32x2(int x, int y) : x(x), y(y) { };
    #endif
} s32x2;

union f32x4;

typedef union s32x4 {
    struct {
        s32 x;
        s32 y;
        s32 z;
        s32 w;
    };
    struct {
        s32x2 xy;
        s32x2 zw;
    };

    __m128i m128i;

    #ifdef __cplusplus
    s32x4() { };
    s32x4(s32 x, s32 y, s32 z, s32 w) : x(x), y(y), z(z), w(w) { };
    s32x4(s32 x, s32 y, s32x2 b) : x(x), y(y), z(b.x), w(b.y) { };
    s32x4(__m128i a) : m128i(a) { };
    operator __m128i() { return m128i; };

    s32x4 operator -(s32x4 b) {
        return _mm_sub_epi32(m128i, b.m128i);
    }

    s32x4 &operator-=(s32x4 b) {
        return *this = *this - b;
    }

    bool operator==(s32x4 b) {
        return x == b.x && y == b.y && z == b.z && w == b.w;
    }

    #endif
} s32x4;

typedef union s16x2 {
    struct {
        short x;
        short y;
    };

    #ifdef __cplusplus
    s16x2() { };
    s16x2(s16 x, s16 y) : x(x), y(y) { };
    #endif
} s16x2;

typedef union s16x4 {
    struct {
        s16 x;
        s16 y;
        s16 z;
        s16 w;
    };
    struct {
        s16x2 xy;
        s16x2 zw;
    };

    #ifdef __cplusplus
    s16x4() { };
    s16x4(s16 x, s16 y, s16 z, s16 w) : x(x), y(y), z(z), w(w) { };
    s16x4(s16 x, s16 y, s16x2 b) : x(x), y(y), z(b.x), w(b.y) { };
    //s16x4(f32x4 b) : x((short)b.x), y((short)b.y), z((short)b.z), w((short)b.w) { };
    #endif
} s16x4;

typedef union s16x8 {
    struct {
        s16 _0;
        s16 _1;
        s16 _2;
        s16 _3;
        s16 _4;
        s16 _5;
        s16 _6;
        s16 _7;
    };

    struct {
        s16x4 e0;
        s16x4 e1;
    };

    __m128i m128i;

    __m128  m128;
    __m128d m128d;


    #ifdef __cplusplus
    s16x8() { };
    s16x8(s16 e0_x, s16 e0_y, s16 e0_z, s16 e0_w, s16 e1_x, s16 e1_y, s16 e1_z, s16 e1_w) : _0(e0_x), _1(e0_y), _2(e0_z), _3(e0_w), _4(e1_x), _5(e1_y), _6(e1_z), _7(e1_w) { };
    s16x8(__m128i a) : m128i(a) { };
    operator __m128i() { return m128i; }

    s16x8 operator -(s16x8 b) {
        return _mm_sub_epi16(m128i, b.m128i);
    }

    s16x8 &operator-=(s16x8 b) {
        return *this = *this - b;
    }
    #endif
} s16x8;


typedef union f32x2 {
    struct {
        f32 x;
        f32 y;
    };

    struct {
        f32 u;
        f32 v;
    };

    #ifdef __cplusplus
    constexpr f32x2() : x(0), y(0) { };
    constexpr f32x2(f32 x, f32 y) : x(x), y(y) { };
    #endif
} f32x2;

typedef union f32x4 {
    struct {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    struct {
        f32x2 xy;
        f32x2 zw;
    };

    __m128i m128i;

    __m128  m128;
    __m128d m128d;

    #ifdef __cplusplus
    constexpr f32x4() : x(0), y(0), z(0), w(0) { };
    constexpr f32x4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) { };
    explicit operator s32x4() { return s32x4(x, y, z, w); };
    #endif
} f32x4;

#pragma pack(pop)

#ifdef __cplusplus
static f32x4 operator *(f32x4 a, f32x4 b) {
    return f32x4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}
#endif


static s32 clamp(s32 x, s32 min, s32 max) {
    if (x < min)
        return min;
    if (x > max)
        return max;
    return x;
}