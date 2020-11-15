#include "data-structures/ptr-hashmap.h"
#include "data-structures/iterator.h"
#include "data-structures/string-builder.h"
#include "util.h"
#include "json/json.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    ptr_hashmap *map = ptr_hashmap_new((collection_item_hash_func) strhash, 
            (collection_item_ref_func) strdup, 
            free, 
            strequal, 
            (collection_item_ref_func) json_node_ref, 
            (collection_item_unref_func) json_node_unref);

    ptr_hashmap_insert(map, "key1", json_object_new());
    ptr_hashmap_insert(map, "key2", json_boolean_new(true));
    ptr_hashmap_insert(map, "key3", json_string_new("example"));
    ptr_hashmap_insert(map, "key1", json_integer_new(34));

    json_node *object = json_object_new();
    for (iterator it = ptr_hashmap_iterator_create(map);
            it.has_next;
            it = iterator_next(it)) {
        ptr_hashmap_entry *entry = iterator_get_item(it);

        json_object_set_member(object, entry->key, entry->value);
    }

    char *object_json = json_node_to_string(object, true);
    printf("%s\n", object_json);
    free(object_json);

    ptr_hashmap_destroy(map);
    return 0;
}
