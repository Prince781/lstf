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

static json_node *json_internal_convert_node_to_pattern(json_node *node)
{
    if (node->is_pattern)
        return node;

    if (!node->floating)
        node = json_node_copy(node);

    node->is_pattern = true;
    switch (node->node_type) {
        case json_node_type_object:
        {
            for (iterator it = ptr_hashmap_iterator_create(((json_object *)node)->members); 
                    it.has_next; it = iterator_next(it)) {
                ptr_hashmap_entry *entry = iterator_get_item(it);
                json_node *member_value = entry->value;

                ptr_hashmap_entry_set_value(((json_object *)node)->members,
                        entry,
                        json_internal_convert_node_to_pattern(member_value));
            }
        }   break;
        case json_node_type_array:
        {
            for (unsigned i = 0; i < ((json_array *)node)->num_elements; i++) {
                json_node *element = ((json_array *)node)->elements[i];

                json_array_set_element(node, i, json_internal_convert_node_to_pattern(element));
            }
        }   break;
        case json_node_type_string:
        case json_node_type_integer:
        case json_node_type_boolean:
        case json_node_type_double:
        case json_node_type_ellipsis:
        case json_node_type_null:
            break;
    }

    return node;
}

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
    if (!node || node->visiting)
        return;
    assert(node->floating || node->refcount == 0);

    node->visiting = true;
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

        for (unsigned i = 0; i < array->num_elements; i++) {
            json_node_unref(array->elements[i]);
            array->elements[i] = NULL;
        }

        array->num_elements = 0;
        free(array->elements);
        array->elements = NULL;
        array->buffer_size = 0;
    }

    node->visiting = false;
    free(node);
}

void json_node_unref(json_node *node)
{
    if (!node || node->visiting)
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

static void json_node_build_string_tabulate(string *sb, unsigned tabulation) {
    for (unsigned i = 0; i < tabulation; i++)
        string_appendf(sb, "    "); 
}

static void json_node_build_string(json_node *root_node,
                                   json_node *node,
                                   bool pretty,
                                   unsigned tabulation,
                                   string *sb)
{
    if (node->visiting) {
        if (node == root_node)
            string_appendf(sb, "[Circular *1]");
        else
            string_appendf(sb, "[Object]"); 
        return;
    }

    node->visiting = true;
    switch (node->node_type) {
    case json_node_type_null:
        string_appendf(sb, "null");
        break;
    case json_node_type_integer:
        string_appendf(sb, "%"PRId64, ((json_integer *)node)->value); 
        break;
    case json_node_type_double:
        string_appendf(sb, "%lf", ((json_double *)node)->value); 
        break;
    case json_node_type_boolean:
        string_appendf(sb, "%s",((json_boolean *)node)->value ? "true" : "false"); 
        break;
    case json_node_type_string:
    {
        char *escaped = json_string_escape(((json_string *)node)->value);
        string_appendf(sb, "\"%s\"", escaped); 
        free(escaped);
    }   break;
    case json_node_type_array:
    {
        json_array *array = (json_array *)node;
        string_appendf(sb, "["); 
        for (unsigned i = 0; i < array->num_elements; i++) {
            if (pretty) {
                string_appendf(sb, "\n");
                json_node_build_string_tabulate(sb, tabulation + 1);
            } else if (i > 0) {
                string_appendf(sb, " ");
            }
            json_node_build_string(root_node, array->elements[i], pretty, tabulation + 1, sb);
            if (i < array->num_elements - 1)
                string_appendf(sb, ",");
        }
        if (pretty && array->num_elements > 0) {
            string_appendf(sb, "\n");
            json_node_build_string_tabulate(sb, tabulation);
        }
        string_appendf(sb, "]"); 
    }   break;
    case json_node_type_object:
    {
        json_object *object = (json_object *)node;
        string_appendf(sb, "{");
        for (iterator it = ptr_hashmap_iterator_create(object->members);
                it.has_next;
                it = iterator_next(it)) {
            const ptr_hashmap_entry *entry = iterator_get_item(it);
            char *member_name = entry->key;
            json_node *member_value = entry->value;
            if (pretty) {
                string_appendf(sb, "\n");
                json_node_build_string_tabulate(sb, tabulation + 1);
            } else if (!it.is_first) {
                string_appendf(sb, " ");
            }
            string_appendf(sb, "\"%s\": ", member_name);
            json_node_build_string(root_node, member_value, pretty, tabulation + 1, sb);
            if (iterator_next(it).has_next)
                string_appendf(sb, ",");
        }
        if (pretty && !ptr_hashmap_is_empty(object->members)) {
            string_appendf(sb, "\n");
            json_node_build_string_tabulate(sb, tabulation);
        }
        string_appendf(sb, "}");
    }   break;
    case json_node_type_ellipsis:
        string_appendf(sb, "...");
        break;
    default:
        fprintf(stderr, "%s: invalid node type `%u'", __func__, node->node_type);
        abort();
        break;
    }
    node->visiting = false;
}

char *json_node_to_string(json_node *node, bool pretty)
{
    string *sb = string_new();

    json_node_build_string(node, node, pretty, 0, sb);

    return string_destroy(sb);
}

size_t json_node_to_string_length(json_node *node, bool pretty)
{
    string *sb = string_new();

    json_node_build_string(node, node, pretty, 0, sb);
    size_t length = sb->length;

    string_unref(sb);
    return length;
}

bool json_node_equal_to(json_node *node1, json_node *node2)
{
    if (node1->node_type != node2->node_type)
        return false;

    if (node1 == node2)
        return true;

    if (node1->visiting && node2->visiting)
        return true;

    if (node1->visiting || node2->visiting)
        return false;

    switch (node1->node_type) {
    case json_node_type_null:
        return true;
    case json_node_type_integer:
        return ((json_integer *)node1)->value == ((json_integer *)node2)->value;
    case json_node_type_double:
        return ((json_double *)node1)->value == ((json_double *)node2)->value;
    case json_node_type_boolean:
        return ((json_boolean *)node1)->value == ((json_boolean *)node2)->value;
    case json_node_type_string:
        return strcmp(((json_string *)node1)->value, ((json_string *)node2)->value) == 0;
    case json_node_type_array:
    {
        json_array *array1 = (json_array *)node1;
        json_array *array2 = (json_array *)node2;

        node1->visiting = true;
        node2->visiting = true;
        if (node1->is_pattern || node2->is_pattern) {
            unsigned i = 0;
            unsigned j = 0;

            while (i < array1->num_elements && j < array2->num_elements) {
                if (json_node_equal_to(array1->elements[i], array2->elements[j])) {
                    i++;
                    j++;
                    continue;
                }

                if (array1->elements[i]->node_type == json_node_type_ellipsis)
                    j++;
                else if (array2->elements[j]->node_type == json_node_type_ellipsis)
                    i++;

                node1->visiting = false;
                node2->visiting = false;
                return false;
            }
        } else {
            if (array1->num_elements != array2->num_elements) {
                node1->visiting = false;
                node2->visiting = false;
                return false;
            }

            for (unsigned i = 0; i < array1->num_elements; i++) {
                if (!json_node_equal_to(array1->elements[i], array2->elements[i])) {
                    node1->visiting = false;
                    node2->visiting = false;
                    return false;
                }
            }
        }

        node1->visiting = false;
        node2->visiting = false;
        return true;
    }
    case json_node_type_object:
    {
        json_object *object1 = (json_object *)node1;
        json_object *object2 = (json_object *)node2;

        node1->visiting = true;
        node2->visiting = true;
        if (node1->is_pattern || node2->is_pattern) {
            if (!node1->partial_match && !node2->partial_match &&
                    ptr_hashmap_num_elements(object1->members) != ptr_hashmap_num_elements(object2->members)) {
                node1->visiting = false;
                node2->visiting = false;
                return false;
            }

            for (iterator it = ptr_hashmap_iterator_create(object1->members);
                    it.has_next;
                    it = iterator_next(it)) {
                const ptr_hashmap_entry *entry = iterator_get_item(it);
                const char *member_name = entry->key;
                json_node *member_value = entry->value;
                json_node *object2_member_value = json_object_get_member(node2, member_name);

                if (!object2_member_value) {
                    if (member_value->optional || node2->partial_match)
                        continue;
                    node1->visiting = false;
                    node2->visiting = false;
                    return false;
                }

                if (json_node_equal_to(member_value, object2_member_value))
                    continue;

                if (member_value->node_type == json_node_type_ellipsis ||
                        object2_member_value->node_type == json_node_type_ellipsis)
                    continue;

                node1->visiting = false;
                node2->visiting = false;
                return false;
            }

            // check whether there are any required fields in object 2 that
            // aren't present in object 1
            if (node2->partial_match && !node1->partial_match &&
                    ptr_hashmap_num_elements(object1->members) != ptr_hashmap_num_elements(object2->members)) {
                for (iterator it = ptr_hashmap_iterator_create(object2->members);
                        it.has_next;
                        it = iterator_next(it)) {
                    const ptr_hashmap_entry *entry = iterator_get_item(it);
                    const char *member_name = entry->key;
                    json_node *member_value = entry->value;

                    if (!json_object_get_member(node1, member_name)) {
                        if (member_value->optional)
                            continue;
                        node1->visiting = false;
                        node2->visiting = false;
                        return false;
                    }
                }
            }
        } else {
            if (ptr_hashmap_num_elements(object1->members) != ptr_hashmap_num_elements(object2->members)) {
                node1->visiting = false;
                node2->visiting = false;
                return false;
            }

            for (iterator it = ptr_hashmap_iterator_create(object1->members);
                    it.has_next;
                    it = iterator_next(it)) {
                const ptr_hashmap_entry *entry = iterator_get_item(it);
                const char *member_name = entry->key;
                json_node *member_value = entry->value;
                json_node *object2_member_value = json_object_get_member(node2, member_name);

                if (!object2_member_value) {
                    node1->visiting = false;
                    node2->visiting = false;
                    return false;
                }

                if (!json_node_equal_to(member_value, object2_member_value)) {
                    node1->visiting = false;
                    node2->visiting = false;
                    return false;
                }
            }
        }

        node1->visiting = false;
        node2->visiting = false;
        return true;
    }
    case json_node_type_ellipsis:
        return true;
    }

    fprintf(stderr, "%s: unexpected JSON node type `%u'\n", __func__, node1->node_type);
    abort();
}

static void json_node_internal_copy_flags(json_node *source_node, json_node *destination_node)
{
    destination_node->optional = source_node->optional;
    destination_node->is_pattern = source_node->is_pattern;
    destination_node->partial_match = source_node->partial_match;
}

static json_node *json_node_internal_copy(json_node *node, ptr_hashmap *seen_nodes)
{
    const ptr_hashmap_entry *node_copy_entry = ptr_hashmap_get(seen_nodes, node);

    if (node_copy_entry)
        return node_copy_entry->value;

    json_node *new_node = NULL;

    switch (node->node_type) {
    case json_node_type_null:
        new_node = json_null_new();
        json_node_internal_copy_flags(node, new_node);
        break;
    case json_node_type_integer:
        new_node = json_integer_new(((json_integer *)node)->value);
        json_node_internal_copy_flags(node, new_node);
        break;
    case json_node_type_double:
        new_node = json_double_new(((json_double *)node)->value);
        json_node_internal_copy_flags(node, new_node);
        break;
    case json_node_type_boolean:
        new_node = json_boolean_new(((json_boolean *)node)->value);
        json_node_internal_copy_flags(node, new_node);
        break;
    case json_node_type_string:
        new_node = json_string_new(((json_string *)node)->value);
        json_node_internal_copy_flags(node, new_node);
        break;
    case json_node_type_array:
        new_node = json_array_new();
        json_node_internal_copy_flags(node, new_node);
        for (unsigned i = 0; i < ((json_array *)node)->num_elements; i++)
            json_array_add_element(new_node, json_node_copy(((json_array *)node)->elements[i]));
        break;
    case json_node_type_object:
        new_node = json_object_new();
        json_node_internal_copy_flags(node, new_node);
        for (iterator it = ptr_hashmap_iterator_create(((json_object *)node)->members);
                it.has_next;
                it = iterator_next(it)) {
            const ptr_hashmap_entry *entry = iterator_get_item(it);
            const char *member_name = entry->key;
            json_node *member_value = entry->value;

            json_object_set_member(new_node, member_name, json_node_copy(member_value));
        }
        break;
    case json_node_type_ellipsis:
        new_node = json_ellipsis_new();
        json_node_internal_copy_flags(node, new_node);
        break;
    }

    ptr_hashmap_insert(seen_nodes, node, new_node);

    return new_node;
}

json_node *json_node_copy(json_node *node)
{
    ptr_hashmap *seen_nodes = ptr_hashmap_new(NULL, NULL, NULL, NULL, NULL, NULL);
    json_node *new_node = json_node_internal_copy(node, seen_nodes);

    ptr_hashmap_destroy(seen_nodes);
    return new_node;
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

json_node *json_array_pattern_new(void)
{
    json_node *array = json_array_new();

    array->is_pattern = true;

    return array;
}

json_node *json_array_add_element(json_node *node, json_node *element)
{
    assert(node->node_type == json_node_type_array);
    assert((node->is_pattern || !element->is_pattern) && "cannot add JSON pattern to non-pattern");

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

json_node *json_array_set_element(json_node *node, unsigned index, json_node *element)
{
    assert(node->node_type == json_node_type_array);
    assert(index < ((json_array *)node)->num_elements && "index out of range");
    assert((node->is_pattern || !element->is_pattern) && "cannot add JSON pattern to non-pattern");

    json_array *array = (json_array *)node;

    if (!element->is_pattern && node->is_pattern)
        element = json_internal_convert_node_to_pattern(element);

    array->elements[index] = json_node_ref(element);

    return element;
}

json_node *json_array_get_element(json_node *node, unsigned index)
{
    assert(node->node_type == json_node_type_array);
    if (index >= ((json_array *)node)->num_elements)
        return NULL;

    json_array *array = (json_array *)node;

    return array->elements[index];
}

char *json_member_name_canonicalize(const char *member_name)
{
    size_t cmember_size = strlen(member_name) + 1;
    size_t cmember_length = 0;
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

json_node *json_object_new(void)
{
    json_object *node = calloc(1, sizeof *node);

    if (!node) {
        perror("failed to create JSON object");
        abort();
    }

    ((json_node *)node)->node_type = json_node_type_object;
    ((json_node *)node)->floating = true;
    node->members = ptr_hashmap_new(
            /* keys */
            (collection_item_hash_func) strhash, 
            (collection_item_ref_func) strdup, 
            (collection_item_unref_func) free,
            (collection_item_equality_func) strequal,
            /* values */
            (collection_item_ref_func) json_node_ref,
            (collection_item_unref_func) json_node_unref);

    return (json_node *)node;
}

json_node *json_object_pattern_new(void)
{
    json_node *object = json_object_new();

    object->is_pattern = true;

    return object;
}

json_node *json_object_set_member(json_node *node, const char *member_name, json_node *member_value)
{
    assert(node->node_type == json_node_type_object);
    assert((node->is_pattern || !member_value->is_pattern) && "cannot add JSON pattern to non-pattern");

    json_object *object = (json_object *)node;
    char *canonicalized_member_name = json_member_name_canonicalize(member_name);

    if (!canonicalized_member_name) {
        fprintf(stderr, "%s: WARNING: could not canonicalize JSON property name `%s'\n",
                __func__, member_name);
        return NULL;
    }

    if (!member_value->is_pattern && node->is_pattern)
        member_value = json_internal_convert_node_to_pattern(member_value);

    ptr_hashmap_insert(object->members, canonicalized_member_name, member_value);
    free(canonicalized_member_name);

    return node;
}

json_node *json_object_get_member(json_node *node, const char *member_name)
{
    assert(node->node_type == json_node_type_object);

    json_object *object = (json_object *)node;
    char *canonicalized_member_name = json_member_name_canonicalize(member_name);

    if (!canonicalized_member_name) {
        fprintf(stderr, "%s: WARNING: could not canonicalize JSON property name `%s'\n", __func__, member_name);
        return NULL;
    }

    ptr_hashmap_entry *entry = ptr_hashmap_get(object->members, canonicalized_member_name);
    free(canonicalized_member_name);

    return entry ? entry->value : NULL;
}

void json_object_pattern_set_is_partial_match(json_node *node)
{
    assert(node->node_type == json_node_type_object);
    assert(node->is_pattern);

    node->partial_match = true;
}

json_node *json_ellipsis_new(void)
{
    json_node *node = calloc(1, sizeof *node);

    if (!node) {
        perror("failed to create JSON ellipsis node");
        abort();
    }

    node->node_type = json_node_type_ellipsis;
    node->floating = true;
    node->is_pattern = true;

    return node;
}
