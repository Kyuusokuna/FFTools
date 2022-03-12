#include "string.h"
#include <string.h>
#include <stdlib.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

bool compare_char_ignoring_case(char a, char b) {
    if ('a' <= a && a <= 'z')
        a ^= 0x20;
    if ('a' <= b && b <= 'z')
        b ^= 0x20;

    return a == b;
}

bool starts_with(ROString str, ROString start) {
    if (str.length < start.length)
        return false;

    for (int i = 0; i < start.length; i++)
        if (str[i] != start[i])
            return false;

    return true;
}

bool ends_with(ROString str, ROString end) {
    if (str.length < end.length)
        return false;

    for (int i = 0; i < end.length; i++)
        if (str[str.length - end.length + i] != end[i])
            return false;

    return true;
}

bool contains(ROString str, ROString search) {
    if (str.length < search.length)
        return false;

    ROString tmp = str;
    while (tmp.length >= search.length)
        if (starts_with(tmp, search))
            return true;
        else
            advance(tmp);

    return false;
}

bool starts_with_ignoring_case(ROString str, ROString start) {
    if (str.length < start.length)
        return false;

    for (int i = 0; i < start.length; i++)
        if (!compare_char_ignoring_case(str[i], start[i]))
            return false;

    return true;
}

bool ends_with_ignoring_case(ROString str, ROString end) {
    if (str.length < end.length)
        return false;

    for (int i = 0; i < end.length; i++)
        if (!compare_char_ignoring_case(str[str.length - end.length + i], end[i]))
            return false;

    return true;
}

bool contains_ignoring_case(ROString str, ROString search) {
    if (str.length < search.length)
        return false;

    ROString tmp = str;
    while (tmp.length >= search.length)
        if (starts_with_ignoring_case(tmp, search))
            return true;
        else
            advance(tmp);

    return false;
}

void advance(ROString &str, int64_t amount) {
    int64_t to_advance = min(str.length, amount);
    str.data += to_advance;
    str.length -= to_advance;
}

String concat(ROString a, ROString b) {
    int64_t len = a.length + b.length;
    char *data = (char *)malloc(len + 1);

    memcpy(data, a.data, a.length);
    memcpy(data + a.length, b.data, b.length);
    data[a.length + b.length] = 0;

    return { len, data };
}

bool is_whitespace(char c) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        return true;
    return false;
}

bool is_decimal_digit(char c) {
    if ('0' <= c && c <= '9')
        return true;
    return false;
}

void consume_whitespace(ROString &str) {
    while (is_whitespace(str[0]))
        advance(str);
}

void consume_whitespace_except_newline(ROString &str) {
    while (is_whitespace(str[0]) && str[0] != '\r' && str[0] != '\n')
        advance(str);
}

void consume_whitespace_and_comments(ROString &str) {
    for (;;) {
        while (is_whitespace(str[0]))
            advance(str);
        if (str[0] == '/' && str[1] == '/')
            while (str[0] != '\r' && str[0] != '\n' && str.length)
                advance(str);
        else if (str[0] == '/' && str[1] == '*') {
            while ((str[0] != '*' || str[1] != '/') && str.length)
                advance(str);
            advance(str, 2);
        } else
            break;
    }
}

void consume_whitespace_and_comments_except_newline(ROString &str) {
    for (;;) {
        while (is_whitespace(str[0]) && str[0] != '\r' && str[0] != '\n')
            advance(str);
        if (str[0] == '/' && str[1] == '/' && str.length)
            while (str[0] != '\r' && str[0] != '\n')
                advance(str);
        else if (str[0] == '/' && str[1] == '*') {
            while ((str[0] != '*' || str[1] != '/') && str.length)
                advance(str);
            advance(str, 2);
        } else
            break;
    }
}

void consume_utf8_byte_order_mark(ROString &str) {
    if (str.length < 3)
        return;

    if (str[0] == '\xEF' && str[1] == '\xBB' && str[2] == '\xBF')
        advance(str, 3);
}

char to_upper(char c) {
    if ('a' <= c && c <= 'z')
        return c & 0xDF;
    return c;
}

bool equal_ignoring_case(ROString a, ROString b) {
    if (a.length == b.length) {
        if (a.data == b.data)
            return true;
        for (int i = 0; i < a.length; i++)
            if (to_upper(a[i]) != to_upper(b[i]))
                return false;
        return true;
    }
    return false;
}

String copy(ROString a) {
    String result = { a.length, 0 };
    result.data = (char *)malloc(a.length);
    memcpy(result.data, a.data, a.length);
    return result;
}
