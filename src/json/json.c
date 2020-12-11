#include "json.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

json_node *json_node_ref(json_node *node)
{
    if (node) {
        if (node->floating) {
            node->floating = false;
            node->refcount = 1;
        } else
            node->refcount++;
    }
    return node;
}

static void json_node_destroy(json_node *node)
{
    if (!node)
        return;
    assert(node->floating || node->refcount == 0);

    if (node->node_type == json_node_type_string) {
        json_string *string_node = (json_string *)node;
        free(string_node->value);
        string_node->value = NULL;
    } else if (node->node_type == json_node_type_object) {
        json_object *object = (json_object *)node;
        ptr_hashmap_destroy(object->members);
        object->members = NULL;
    } else if (node->node_type == json_node_type_array) {
        json_array *array = (json_array *)node;

        for (int i = 0; i < array->num_elements; i++) {
            json_node_unref(array->elements[i]);
            array->elements[i] = NULL;
        }

        array->num_elements = 0;
        free(array->elements);
        array->elements = NULL;
        array->buffer_size = 0;
    }

    free(node);
}

void json_node_unref(json_node *node)
{
    if (!node)
        return;
    assert(node->floating || node->refcount > 0);
    if (node->floating || --node->refcount == 0)
        json_node_destroy(node);
}

static char *json_string_escape(const char *unescaped)
{
    string *escaped_sb = string_new();

    for (const char *p = unescaped; *p; ++p) {
        switch (*p) {
        case '\\':
        case '/':
        case '"':
            string_appendf(escaped_sb, "\\%c", *p); 
            break;
        case '\b':
            string_appendf(escaped_sb, "\\b");
            break;
        case '\f':
            string_appendf(escaped_sb, "\\f");
            break;
        case '\n':
            string_appendf(escaped_sb, "\\n");
            break;
        case '\r':
            string_appendf(escaped_sb, "\\r");
            break;
        case '\t':
            string_appendf(escaped_sb, "\\t");
            break;
        default:
            // TODO: convert special characters to unicode escape sequences
            string_appendf(escaped_sb, "%c", *p); 
            break;
        }
    }

    return string_destroy(escaped_sb);
}

static void tabulate(string *sb, int tabulation) {
    for (int i = 0; i < tabulation; i++)
        string_appendf(sb, "    "); 
}

static void json_node_build_string(const json_node *node, bool pretty, int tabulation, string *sb)
{
    switch (node->node_type) {
    case json_node_type_null:
        string_appendf(sb, "null");
        break;
    case json_node_type_integer:
        string_appendf(sb, "%"PRId64, ((const json_integer *)node)->value); 
        break;
    case json_node_type_double:
        string_appendf(sb, "%lf", ((const json_double *)node)->value); 
        break;
    case json_node_type_boolean:
        string_appendf(sb, "%s",((const json_boolean *)node)->value ? "true" : "false"); 
        break;
    case json_node_type_string:
    {
        char *escaped = json_string_escape(((const json_string *)node)->value);
        string_appendf(sb, "\"%s\"", escaped); 
        free(escaped);
    }   break;
    case json_node_type_array:
    {
        const json_array *array = (const json_array *)node;
        string_appendf(sb, "["); 
        for (int i = 0; i < array->num_elements; i++) {
            if (pretty) {
                string_appendf(sb, "\n");
                tabulate(sb, tabulation + 1);
            } else if (i > 0) {
                string_appendf(sb, " ");
            }
            json_node_build_string(array->elements[i], pretty, tabulation + 1, sb);
            if (i < array->num_elements - 1)
                string_appendf(sb, ",");
        }
        if (pretty && array->num_elements > 0) {
            string_appendf(sb, "\n");
            tabulate(sb, tabulation);
        }
        string_appendf(sb, "]"); 
    }   break;
    case json_node_type_object:
    {
        const json_object *object = (const json_object *)node;
        string_appendf(sb, "{");
        for (iterator it = ptr_hashmap_iterator_create(object->members);
                it.has_next;
                it = iterator_next(it)) {
            const ptr_hashmap_entry *entry = iterator_get_item(it);
            char *member_name = entry->key;
            json_node *member_value = entry->value;
            if (pretty) {
                string_appendf(sb, "\n");
                tabulate(sb, tabulation + 1);
            } else if (!it.is_first) {
                string_appendf(sb, " ");
            }
            string_appendf(sb, "\"%s\": ", member_name);
            json_node_build_string(member_value, pretty, tabulation + 1, sb);
            if (iterator_next(it).has_next)
                string_appendf(sb, ",");
        }
        if (pretty && !ptr_hashmap_is_empty(object->members)) {
            string_appendf(sb, "\n");
            tabulate(sb, tabulation);
        }
        string_appendf(sb, "}");
    }   break;
    default:
        fprintf(stderr, "invalid node type `%d'", node->node_type);
        abort();
        break;
    }
}

char *json_node_to_string(const json_node *node, bool pretty)
{
    string *sb = string_new();

    json_node_build_string(node, pretty, 0, sb);

    return string_destroy(sb);
}

bool json_node_equal_to(const json_node *node1, const json_node *node2)
{
    if (node1->node_type != node2->node_type)
        return false;

    switch (node1->node_type) {
    case json_node_type_null:
        return true;
    case json_node_type_integer:
        return ((const json_integer *)node1)->value == ((const json_integer *)node2)->value;
    case json_node_type_double:
        return ((const json_double *)node1)->value == ((const json_double *)node2)->value;
    case json_node_type_boolean:
        return ((const json_boolean *)node1)->value == ((const json_boolean *)node2)->value;
    case json_node_type_string:
        return strcmp(((const json_string *)node1)->value, ((const json_string *)node2)->value) == 0;
    case json_node_type_array:
    {
        const json_array *array1 = (const json_array *)node1;
        const json_array *array2 = (const json_array *)node2;

        if (array1->num_elements != array2->num_elements)
            return false;

        for (int i = 0; i < array1->num_elements; i++)
            if (!json_node_equal_to(array1->elements[i], array2->elements[i]))
                return false;

        return true;
    }
    case json_node_type_object:
    {
        const json_object *object1 = (const json_object *)node1;
        const json_object *object2 = (const json_object *)node2;

        if (ptr_hashmap_num_elements(object1->members) != ptr_hashmap_num_elements(object2->members))
            return false;

        for (iterator it = ptr_hashmap_iterator_create(object1->members);
                it.has_next;
                it = iterator_next(it)) {
            ptr_hashmap_entry *entry = iterator_get_item(it);
            char *member_name = entry->key;
            json_node *member_value = entry->value;
            json_node *object2_member_value = json_object_get_member(node2, member_name);

            if (!object2_member_value)
                return false;

            if (!json_node_equal_to(member_value, object2_member_value))
                return false;
        }

        return true;
    }
    }

    fprintf(stderr, "%s: unexpected JSON node type `%d'\n", __func__, node1->node_type);
    abort();
}

json_node *json_null_new(void)
{
    json_node *node = calloc(1, sizeof *node);

    node->node_type = json_node_type_null;
    node->floating = true;

    return node;
}

json_node *json_integer_new(int64_t value)
{
    json_integer *node = calloc(1, sizeof *node);

    ((json_node *)node)->node_type = json_node_type_integer;
    ((json_node *)node)->floating = true;
    node->value = value;

    return (json_node *)node;
}

json_node *json_double_new(double value)
{
    json_double *node = calloc(1, sizeof *node);

    ((json_node *)node)->node_type = json_node_type_double;
    ((json_node *)node)->floating = true;
    node->value = value;

    return (json_node *)node;
}

json_node *json_boolean_new(bool value)
{
    json_boolean *node = calloc(1, sizeof *node);

    ((json_node *)node)->node_type = json_node_type_boolean;
    ((json_node *)node)->floating = true;
    node->value = value;

    return (json_node *)node;
}

json_node *json_string_new(const char *value)
{
    json_string *node = calloc(1, sizeof *node);

    ((json_node *)node)->node_type = json_node_type_string;
    ((json_node *)node)->floating = true;
    node->value = strdup(value);

    return (json_node *)node;
}

json_node *json_array_new(void)
{
    json_array *node = calloc(1, sizeof *node);

    ((json_node *)node)->node_type = json_node_type_array;
    ((json_node *)node)->floating = true;
    node->buffer_size = 64;
    node->elements = calloc(node->buffer_size, sizeof *node->elements);

    return (json_node *)node;
}

json_node *json_array_add_element(json_node *node, json_node *element)
{
    assert(node->node_type == json_node_type_array);

    json_array *array = (json_array *)node;

    if (array->num_elements >= array->buffer_size) {
        array->buffer_size = array->buffer_size + array->buffer_size / 2;
        array->elements = realloc(array->elements, array->buffer_size * sizeof *array->elements);
        if (!array->elements) {
            perror("failed to resize JSON array");
            abort();
        }
    }

    array->elements[array->num_elements] = json_node_ref(element);
    array->num_elements++;

    return element;
}

json_node *json_object_new(void)
{
    json_object *node = calloc(1, sizeof *node);

    ((json_node *)node)->node_type = json_node_type_object;
    ((json_node *)node)->floating = true;
    node->members = ptr_hashmap_new((collection_item_hash_func) strhash, 
            (collection_item_ref_func) strdup, 
            free,
            (collection_item_equality_func) strequal, 
            (collection_item_ref_func) json_node_ref,
            (collection_item_unref_func) json_node_unref);

    return (json_node *)node;
}

/**
 * Canonicalize to camelCase. Return value must be free()'d when done with.
 */
static char *json_member_name_canonicalize(const char *member_name)
{
    int cmember_size = strlen(member_name) + 1;
    int cmember_length = 0;
    char *cmember_name = calloc(1, cmember_size);

    for (const char *p = member_name; *p; ++p) {
        if (cmember_length + 1 >= cmember_size) {
            cmember_size *= 2;
            cmember_name = realloc(cmember_name, cmember_size);
            if (!cmember_name) {
                perror("could not allocate string");
                abort();
            }
        }

        // convert from kebab-case or snake_case
        if (*p == '-' || *p == '_') {
            ++p;
            if (!isalnum(*p)) {
                free(cmember_name);
                return NULL;
            }
            cmember_name[cmember_length++] = toupper(*p);
        } else {
            cmember_name[cmember_length++] = *p;
        }
    }

    cmember_name[cmember_length] = '\0';
    return cmember_name;
}

json_node *json_object_set_member(json_node *node, const char *member_name, json_node *member_value)
{
    assert(node->node_type == json_node_type_object);

    json_object *object = (json_object *)node;
    char *canonicalized_member_name = json_member_name_canonicalize(member_name);

    if (!canonicalized_member_name) {
        fprintf(stderr, "%s: WARNING: could not canonicalize JSON property name `%s'\n",
                __func__, member_name);
        return NULL;
    }

    ptr_hashmap_insert(object->members, canonicalized_member_name, member_value);
    free(canonicalized_member_name);

    return node;
}

json_node *json_object_get_member(const json_node *node, const char *member_name)
{
    assert(node->node_type == json_node_type_object);

    const json_object *object = (const json_object *)node;
    char *canonicalized_member_name = json_member_name_canonicalize(member_name);

    if (!canonicalized_member_name) {
        fprintf(stderr, "%s: WARNING: could not canonicalize JSON property name `%s'\n", __func__, member_name);
        return NULL;
    }

    ptr_hashmap_entry *entry = ptr_hashmap_get(object->members, canonicalized_member_name);
    free(canonicalized_member_name);

    return entry ? entry->value : NULL;
}
