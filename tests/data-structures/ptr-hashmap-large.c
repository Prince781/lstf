#include "data-structures/collection.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/string-builder.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define N 512
#define HASHMAP_SIZE (N * (N + N/2))

int main(void) {
    int retval = 0;
    ptr_hashmap *map = ptr_hashmap_new((collection_item_hash_func) strhash,
            (collection_item_ref_func) strdup, 
            free, 
            strequal,
            (collection_item_ref_func) string_ref, (collection_item_unref_func) string_unref);

    for (unsigned i = 0; i < HASHMAP_SIZE; i++) {
        string *sb = string_new();
        string_appendf(sb, "%u", i); 
        ptr_hashmap_entry *entry = ptr_hashmap_insert(map, sb->buffer, sb);
        assert(ptr_hashmap_get(map, sb->buffer) == entry);
    }

    assert(ptr_hashmap_num_elements(map) == HASHMAP_SIZE);

    bool *seen = calloc(HASHMAP_SIZE, sizeof *seen);
    for (iterator it = ptr_hashmap_iterator_create(map); it.has_next; it = iterator_next(it)) {
        ptr_hashmap_entry *entry = iterator_get_item(it);
        string *sb = entry->value;
        unsigned u = 0;

        sscanf(sb->buffer, "%u", &u); 
        seen[u] = true;
    }
    for (unsigned i = 0; i < HASHMAP_SIZE; i++) {
        if (!seen[i]) {
            retval = 1;
            fprintf(stderr, "ERROR: %u'th element not seen in iteration\n", i);
            break;
        }
    }
    free(seen);

    printf("there are %lu buckets in the map for %lu elements\n",
            map->buckets_list->length, ptr_hashmap_num_elements(map));
    printf("current buffer size is %u * sizeof(ptr_hashmap_entry *)\n", map->num_bucket_places);
    double occupancy = map->buckets_list->length / (double) map->num_bucket_places;
    printf("%.1lf%% of the bucket places in the buffer are used\n", occupancy * 100);

    if (ptr_hashmap_num_elements(map) / (double) map->buckets_list->length > 2.0)
        retval = 1;

    ptr_hashmap_destroy(map);
    return retval;
}
