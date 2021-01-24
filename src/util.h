#pragma once

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

unsigned strhash(const char *str);

bool strequal(const void *str1, const void *str2);

unsigned ptrhash(const void *ptr);

/**
 * Determines whether host byte order is network byte order.
 */
static inline bool is_network_byte_order(void)
{
    volatile uint32_t i=0x01234567;
    // return 1 for big endian, 0 for little endian.
    return (*((volatile uint8_t*)(&i))) != 0x67;
}

static inline uint64_t swap_uint64(uint64_t integer)
{
    uint64_t swapped = 0;

    swapped = (swapped << CHAR_BIT) | ((integer >> (0*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (1*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (2*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (3*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (4*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (5*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (6*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (7*CHAR_BIT)) & 0xFF);

    return swapped;
}

static inline uint32_t swap_uint32(uint32_t integer)
{
    uint32_t swapped = 0;

    swapped = (swapped << CHAR_BIT) | ((integer >> (0*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (1*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (2*CHAR_BIT)) & 0xFF);
    swapped = (swapped << CHAR_BIT) | ((integer >> (3*CHAR_BIT)) & 0xFF);

    return swapped;
}

static inline uint64_t ntohll(uint64_t netint) {
    if (!is_network_byte_order())
        return swap_uint64(netint);
    return netint;
}

static inline uint32_t ntohl(uint32_t netint) {
    if (!is_network_byte_order())
        return swap_uint32(netint);
    return netint;
}

static inline uint64_t htonll(uint64_t hostint) {
    if (!is_network_byte_order())
        return swap_uint64(hostint);
    return hostint;
}

static inline uint32_t htonl(uint32_t hostint) {
    if (!is_network_byte_order())
        return swap_uint32(hostint);
    return hostint;
}

#if defined(_WIN32) || defined(_WIN64)
static inline char *strndup(const char *str, size_t n)
{
    size_t string_length = strlen(str);
    if (string_length < n)
        n = string_length;
    char *new_str = malloc(n + 1);
    memcpy(new_str, str, n);
    new_str[n] = '\0';
    return new_str;
}
#endif
