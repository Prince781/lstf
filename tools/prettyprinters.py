# see https://sourceware.org/gdb/current/onlinedocs/gdb.html/Writing-a-Pretty_002dPrinter.html
from __future__ import print_function
import struct
import sys

import gdb.printing
import gdb.types

from typing import Union, Optional

# --- Utility functions ---
def is_lstf_array(value: gdb.Value):
    # see data-structures/array.h
    has_field_length = False
    has_field_nofree = False
    has_field_bufsiz = False
    has_field_elements = False
    for field in value.type.fields():
        if field.name == 'length':
            has_field_length = True
        elif field.name == 'nofree':
            has_field_nofree = True
        elif field.name == 'bufsiz':
            has_field_bufsiz = True
        elif field.name == 'elemsz':
            pass
        elif field.name == 'elements':
            has_field_elements = True
        else:
            return False
    return has_field_length and has_field_nofree \
            and has_field_bufsiz and has_field_elements

def try_deref_members(value: Optional[gdb.Value], *members: str):
    for member in members:
        if not value or not int(value):
            return
        if value.type.code == gdb.TYPE_CODE_PTR:
            value = value.referenced_value()[member]
        else:
            value = value[member]
    return value if (value and int(value)) else None

# --- Data Structures ---
class ArrayPrinter:
    """Print an array(T) object."""

    def __init__(self, val):
        self.val = val

    def num_children(self):
        return int(self.val['length'])

    def child(self, i):
        return f'[{i}]', (self.val['elements'] + int(i)).referenced_value()

    def to_string(self):
        length = self.val['length']
        capacity = self.val['bufsiz']
        elemty = self.val['elements'].type.target()
        return f'array<{elemty}> of length {length} capacity {capacity}'

    def display_hint(self):
        return 'array'

    def children(self):
        length = int(self.val['length'])
        for i in range(length):
            pointer = self.val['elements'] + i
            yield str(i), pointer.referenced_value()
        return None

# --- IO ---
class IOEventPrinter:
    """Print a struct _event object."""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        callback = self.val['callback']
        userdata = self.val['callback_data']
        status = '|READY' if bool(int(self.val['is_ready'])) else \
                ('|CANCELLED' if bool(int(self.val['is_canceled'])) else '')
        if str(self.val['type']) == 'event_type_default':
            return f'event<on manual trigger, callback: {callback}({userdata}){status}>'
        if str(self.val['type']) == 'event_type_bg_task':
            threadproc = self.val['thread_proc']
            threaddata = self.val['thread_data']
            return f'event<on finish thread: {threadproc}({threaddata}), callback: {callback}({userdata}){status}>'
        if str(self.val['type']) == 'event_type_subprocess':
            process_id = self.val['process']
            return f'event<on finish process: PID {process_id}, callback: {callback}({userdata}){status}>'
        if str(self.val['type']) == 'event_type_io_read':
            fd = self.val['fd']
            return f'event<on readable fd: {fd}, callback: {callback}({userdata}){status}>'
        if str(self.val['type']) == 'event_type_io_write':
            fd = self.val['fd']
            return f'event<on writable fd: {fd}, callback: {callback}({userdata}){status}>'
        return '[unknown event type] @ 0x%x'.format(int(self.val))

    def display_hint(self):
        return None

class IOEventListPrinter:
    """Print an event list (event *)."""

    def __init__(self, val):
        self.val = val
        self.node_type = val.type

    def to_string(self):
        if not int(self.val):
            return 'event list (empty)' # show something for empty

    def display_hint(self):
        return 'array'

    def children(self):
        current = self.val
        i = 0
        while int(current) != 0:
            yield f'[{i}]', current.referenced_value()
            current = current.referenced_value()['next']
            i = i+1
        return None

# --- JSON ---
class JSONNodePrinter:
    def __init__(self, val):
        self.val = val

    def _as(self, type_name: str):
        type_name = type_name.removeprefix('json_node_type_')
        return self.val.cast(gdb.lookup_type('json_' + type_name).pointer())

    def _asv(self, type_name: str):
        return self._as(type_name).referenced_value()

    def display_hint(self):
        type_name = self.val['node_type']
        if type_name == 'json_node_type_string':
            return 'string'
        if type_name == 'json_node_type_array':
            return 'array'
        if type_name == 'json_node_type_object':
            return 'map'
        # all other values use default printer

    def to_string(self):
        if not int(self.val):
            return
        type_name = str(self.val.referenced_value()['node_type'])
        if type_name == 'json_node_type_null':
            return 'null'
        if type_name == 'json_node_type_integer':
            return self._asv(type_name)['value']
        if type_name == 'json_node_type_double':
            return self._asv(type_name)['value']
        if type_name == 'json_node_type_boolean':
            return self._asv(type_name)['value']
        if type_name == 'json_node_type_string':
            return self._asv(type_name)['value']
        if type_name == 'json_node_type_ellipsis':
            return '...'
        if type_name == 'json_node_type_pointer':
            return self._asv(type_name)['value']
        if type_name == 'json_node_type_array':
            array_node = self._asv(type_name)
            num_elements = array_node['num_elements']
            if not int(num_elements):
                return '{}' # print empty array
            return f'array with {num_elements} elements'
        if type_name == 'json_node_type_object':
            object_node = self._as(type_name)
            num_members = try_deref_members(object_node, 'members', 'entries_list', 'length')
            if not num_members or not int(num_members):
                return '{}'
            return f'object with {num_members} members'
        return type_name

    def children(self):
        type_name = str(self.val.referenced_value()['node_type'])

        # iterate through JSON array
        if type_name == 'json_node_type_array':
            array_node = self._asv(type_name)
            for i in range(int(array_node['num_elements'])):
                yield f'[{i}]', (array_node['elements'] + i).referenced_value()

        # iterate through JSON object
        if type_name == 'json_node_type_object':
            keyval_type = gdb.lookup_type('ptr_hashmap_entry').pointer()
            key_type = gdb.lookup_type('char').pointer()
            val_type = gdb.lookup_type('json_node').pointer()
            object_node = self._as(type_name)
            list_head = try_deref_members(object_node, 'members', 'entries_list', 'head')
            list_tail = try_deref_members(object_node, 'members', 'entries_list', 'tail')
            list_node = list_head
            seen_head = False
            while (
                list_node
                and int(list_node) != 0
                and (not seen_head or int(list_node) != int(list_head))
            ):
                hashmap_node = list_node.referenced_value()['data'].cast(keyval_type)
                key = hashmap_node.referenced_value()['key'].cast(key_type)
                val = hashmap_node.referenced_value()['value'].cast(val_type)
                yield f'[{key}]', val
                list_node = list_node.referenced_value()['next']
                seen_head = True

        return None

def lookup_pointer_printer(value):
    if value.type.code == gdb.TYPE_CODE_STRUCT and is_lstf_array(value):
        return ArrayPrinter(value)
    if value.type.code == gdb.TYPE_CODE_PTR:
        if str(value.type) == 'event *' or str(value.type) == 'const event *':
            return IOEventListPrinter(value)
        if str(value.type) == 'json_node *' or str(value.type) == 'const json_node *':
            return JSONNodePrinter(value)

pp = gdb.printing.RegexpCollectionPrettyPrinter('lstf')
pp.add_printer('event', r'^_event$', IOEventPrinter)
gdb.printing.register_pretty_printer(gdb.current_objfile(), pp)
gdb.printing.register_pretty_printer(gdb.current_objfile(), lookup_pointer_printer)
