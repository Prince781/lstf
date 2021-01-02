# LSTF Virtual Machine
- stack-based VM
- `lstf_vm_program` contains `code`, `data`, and `debug_info` sections
	- `data` organized into series of constants
	- `code` organized into list of functions
- `lstf_vm_value` contains one field for value type and another field for the actual value
- `lstf_vm_stack` contains slots for many instances of `lstf_vm_value`, and grows dynamically

## Program Layout

### Program header
| byte range | description
| ---------- | -----------
| 0 - 7      | `\x89LSTF\x01\x0A\x00` (magic value)
| 8 - n      | sections: list of [`[section name]` followed by `\x00` followed by 8-byte-length `[section size]`]

- header terminated with one `\x00` (NUL) byte

### `debug_info` section
- `source_file` - is a string containing the path to the source file, terminated with a NUL byte
- `n_debug_entries` - number of debug entries
- `debug_entries` - is a list of `(uint64_t instruction_offset, uint32_t line, uint32_t column)`
- `n_debug_syms` - number of debug symbols
- `debug_syms` - debug symbols `(uint64_t instruction_offset, char[] name)`

### `data` section
- essentially a contiguous block of NUL-terminated strings

### `code` section
- contains only instruction opcodes and immediate values

## Memory

### `object`, `array`, and `pattern`
- references are passed around; objects can only be copied explicitly
- garbage collector used to break rare cyclic references
	- each `json_node` is registered with the garbage collector (`lstf_vm_collector`)

## ISA

### Manipulating Values on the Stack
- `load frame(<n>)` - loads the `n`'th item in the current stack frame
	- addressed from the base of the stack frame, so `load frame(0)` 
 means "load the 0th element in the current stack frame"
    - if the value is an array, object, string, or pattern, a reference to it
      is copied, not the underlying thing
	- `<n>` is encoded as a 8-byte immediate
- `load data(<n>)` - loads the `<n>`'th element from the `data` section
	- `<n>` is encoded as a 8-byte immediate
- `load <function>` - loads an offset to the function in the code
    - encoded as a 8-byte immediate value that is an offset in the `code`
      section to the function
- `load <expr>` - loads an expression onto the stack
	- if the expression is an object expression, a new object is created
	- if the expression is an array expression, a new array is created
    - examples for `<expr>`: `"Hello, world\n"`, `3`, `3.0`, `true`, `{x: 3}`,
      `[1, "Hello, world\n"]`, or `foo` (where `foo` is a function)
	- the expression must be NUL-terminated
- `store frame(<n>)` - pops the value at the top of the stack frame and stores
  it into the `<n>`'th item on the stack frame
	- `<n>` is encoded as a 8-byte immediate

### Accessing Members of Structured Types
- `get <object|array> <index: string|integer>` - accesses (reads) the member
  `<index>` of the object/array
- `set <object|array> <index: string|integer> <value: any>` - writes the value
  `<value>` to the member `<index>` of the object/array

### Calling and Returning from Functions
- `call <label>` - calls a function
	1. `<label>` is encoded as a 8-byte immediate integer for the instruction
	2. sets up a new stack frame
	3. pushes the current instruction pointer onto the stack
	4. jumps to the function pointer
- `indirect` - indirectly calls a function
	1. pops an offset relative to the beginning of the `code` section from the current stack frame
	2. sets up a new stack frame
	3. pushes the current instruction pointer onto the stack (as the return address)
	4. jumps to the function pointer (computed from the start of `code`
- `return` - returns from a function
	1. pops an address (the return address) from the current stack
	2. tears down the existing stack frame
	3. jumps to the return address
- `vmcall <name>` - calls a special function provided by the virtual machine
	- function codes/names (names are converted to immediate codes in assembly):
| code | name          | signature                                                      | description
| ---- | ------------- | -------------------------------------------------------------- | -----------
| `01` | `connect`     | `connect(path_to_server: string): void`                        | Connect to LSP server. On failure throws a fatal exception.
| `02` | `td_open`     | `td_open(filename: string): void`                              | Call `textDocument/open` with a file. Will fail if a connection has not been established already.
| `03` | `diagnostics` | `async diagnostics(string filename): PublishDiagnosticsParams` | Will wait for diagnostics as they are expected to come in.

### Control Flow
- `if <label>` - jumps to the label if the previous expression evaluated to `true`
	- this gets encoded as an offset in the bytecode
- `jump <label>` - unconditional jump; doesn't change the stack
	- this gets encoded as an offset in the bytecode

### Logical Operations
- `bool` - converts the previous expression to a boolean
- `land` - pops two elements off the stack and pushes the logical AND of their values
- `lor` - logical OR
- `lnot` - logical NOT; pops one element

### Input/Output
- `print <value>` - pops the stack and prints the value to standard output
- `exit <integer: 0-255>` exits with the exit code `<integer>`
