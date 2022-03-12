#pragma once
#include <stdbool.h>
#include <stdint.h>

#define STR(x) (x).length, (x).data

typedef struct ROString {
    int64_t length;
    const char *data;

    #ifdef __cplusplus
    constexpr ROString() : length(0), data(0) { };
    constexpr ROString(int64_t length, const char *data) : length(length), data(data) { };

    template<size_t _len>
    constexpr ROString(const char(&data)[_len]) : length(_len - 1), data(data) { };

    char operator[](int64_t index) const {
        if (index < length)
            return data[index];
        return 0;
    }

    const char *begin() {
        return data;
    }

    const char *end() {
        return data + length;
    }
    #endif
} ROString;

typedef struct ROString_Literal {
    int64_t length;
    const char *data;

    #ifdef __cplusplus
    constexpr ROString_Literal() : length(0), data(0) { };
    constexpr ROString_Literal(int64_t length, const char *data) : length(length), data(data) { };

    template<size_t _len>
    constexpr ROString_Literal(const char(&data)[_len]) : length(_len - 1), data(data) { };

    operator ROString &() const { return *(ROString *)this; }
    operator const char *() const { return data; }

    char operator[](int64_t index) {
        if (index < length)
            return data[index];
        return 0;
    }

    const char *begin() {
        return data;
    }

    const char *end() {
        return data + length;
    }
    #endif
} ROString_Literal;

typedef struct String {
    int64_t length;
    char *data;

    #ifdef __cplusplus
    constexpr String() : length(0), data(0) { };
    constexpr String(int64_t length, char *data) : length(length), data(data) { };
    //template<size_t _len> constexpr string(const char(&data)[_len]) : length(_len - 1), data((char *)data) { };

    operator ROString&() { return *(ROString *)this; }

    char operator[](int64_t index) {
        if (index < length)
            return data[index];
        return 0;
    }

    char *begin() {
        return data;
    }

    char *end() {
        return data + length;
    }
    #endif
} String;

#ifdef __cplusplus
inline bool operator==(const ROString a, const ROString b) {
    if (a.length == b.length) {
        if (a.data == b.data)
            return true;
        for (int i = 0; i < a.length; i++)
            if (a[i] != b[i])
                return false;
        return true;
    }
    return false;
}

inline bool operator!=(ROString a, ROString b) {
    return !(a == b);
}

inline ROString_Literal operator""_s(const char *str, size_t length) {
    return { (int64_t)length, str };
}


bool starts_with(const ROString str, const ROString start);
bool ends_with(const ROString str, const ROString end);
bool contains(const ROString str, const ROString search);

bool starts_with_ignoring_case(const ROString str, const  ROString start);
bool ends_with_ignoring_case(const ROString str, const  ROString end);
bool contains_ignoring_case(const ROString str, const ROString search);

void advance(ROString &str, int64_t amount = 1);

String concat(const ROString a, const ROString b);

bool is_whitespace(char c);
bool is_decimal_digit(char c);
bool equal_ignoring_case(const ROString a, const ROString b);

void consume_whitespace(ROString &str);
void consume_whitespace_except_newline(ROString &str);
void consume_whitespace_and_comments(ROString &str);
void consume_whitespace_and_comments_except_newline(ROString &str);
void consume_utf8_byte_order_mark(ROString &str);

inline char to_upper(char c);
String copy(ROString a);

template<typename T>
bool read(ROString &data, T *result);
#endif