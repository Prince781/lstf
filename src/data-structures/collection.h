#pragma once

#include <stdbool.h>

typedef void *(*collection_item_ref_func)(void *pointer);
typedef void (*collection_item_unref_func)(void *pointer);
typedef bool (*collection_item_equality_func)(const void *pointer1, const void *pointer2);
typedef unsigned (*collection_item_hash_func)(const void *pointer);
