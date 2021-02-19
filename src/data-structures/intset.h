#pragma once

#include "data-structures/iterator.h"
#include "data-structures/string-builder.h"
#include "util.h"
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/**
 * A very lightweight representation of a set of unsigned integers.
 */
typedef struct {
    unsigned set_size;

    /**
     * num_buckets = ceil(set_size / (sizeof(buckets[0]) * CHAR_BIT))
     */
    uintmax_t buckets[];
} intset;

/**
 * Call `free` when done.
 */
static inline intset *intset_new(unsigned set_size)
{
    intset *set = NULL;
    const unsigned bucket_size = sizeof(set->buckets[0]) * CHAR_BIT;

    set = calloc(1, offsetof(intset, buckets) + ((set_size + bucket_size) / bucket_size) * sizeof(set->buckets[0]));
    set->set_size = set_size;

    return set;
}

static inline unsigned intset_num_buckets(const intset *set)
{
    const unsigned bucket_size = sizeof(set->buckets[0]) * CHAR_BIT;
    return (set->set_size + bucket_size) / bucket_size;
}

static inline void intset_set(intset *set, unsigned n)
{
    assert(n < set->set_size);

    const unsigned bucket_size = sizeof(set->buckets[0]) * CHAR_BIT;
    const unsigned bucket = n / bucket_size;
    const unsigned idx = n - bucket * bucket_size;

    set->buckets[bucket] |= 1ull << (bucket_size - 1 - idx);
}

static inline void intset_unset(intset *set, unsigned n)
{
    assert(n < set->set_size);

    const unsigned bucket_size = sizeof(set->buckets[0]) * CHAR_BIT;
    const unsigned bucket = n / bucket_size;
    const unsigned idx = n - bucket * bucket_size;

    set->buckets[bucket] &= ~(1ull << (bucket_size - 1 - idx));
}

static inline void intset_fill(intset *set)
{
    memset(set->buckets, -1, sizeof(set->buckets[0]) * intset_num_buckets(set));
}

static inline void intset_clear(intset *set)
{
    memset(set->buckets, 0, sizeof(set->buckets[0]) * intset_num_buckets(set));
}

static inline unsigned intset_count(const intset *set)
{
    const unsigned num_buckets = intset_num_buckets(set);
    unsigned n = 0;

    for (unsigned b = 0; b < num_buckets; b++) {
        // only partially count the last bucket
        if (b == num_buckets - 1) {
            const unsigned bucket_size = sizeof(set->buckets[0]) * CHAR_BIT;
            const unsigned rem = set->set_size - bucket_size * (set->set_size / bucket_size);
            uintmax_t flag = 0;

            if (rem == bucket_size - 1)
                flag = ~((uintmax_t)0u);
            else
                flag = ~((((uintmax_t)1u) << (bucket_size - 1 - rem)) - 1u);

            n += popcount(set->buckets[b] & flag);
        } else {
            n += popcount(set->buckets[b]);
        }
    }

    return n;
}

static inline void intset_copy(intset *set, const intset *source_set)
{
    const unsigned buckets1 = intset_num_buckets(set);
    const unsigned buckets2 = intset_num_buckets(source_set);
    memcpy(set->buckets, source_set->buckets, sizeof(set->buckets[0]) * (buckets1 < buckets2 ? buckets1 : buckets2));
}

static inline void intset_union(intset *set, const intset *other_set)
{
    for (unsigned b = 0; b < intset_num_buckets(set) && b < intset_num_buckets(other_set); b++)
        set->buckets[b] |= other_set->buckets[b];
}

static inline void intset_minus(intset *set, const intset *other_set)
{
    for (unsigned b = 0; b < intset_num_buckets(set) && b < intset_num_buckets(other_set); b++)
        set->buckets[b] &= ~other_set->buckets[b];
}

static inline void intset_intersect(intset *set, const intset *other_set)
{
    for (unsigned b = 0; b < intset_num_buckets(set) && b < intset_num_buckets(other_set); b++)
        set->buckets[b] &= other_set->buckets[b];
}

static inline void intset_invert(intset *set)
{
    for (unsigned b = 0; b < intset_num_buckets(set); b++)
        set->buckets[b] = ~set->buckets[b];
}

static inline char *intset_to_string(const intset *set)
{
    string *repr = string_new();

    string_appendf(repr, "{");
    const unsigned bucket_size = sizeof(set->buckets[0]) * CHAR_BIT;
    bool is_first = true;
    for (unsigned i = 0; i < set->set_size; i++) {
        const unsigned b = i / bucket_size;
        const unsigned n = i - b * bucket_size;

        if (set->buckets[b] & (((uintmax_t)1u) << (bucket_size - 1 - n))) {
            if (!is_first)
                string_appendf(repr, ", ");
            string_appendf(repr, "%u", i);
            is_first = false;
        }
    }
    string_appendf(repr, "}");

    return string_destroy(repr);
}
