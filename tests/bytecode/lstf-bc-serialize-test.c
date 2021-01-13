#include "bytecode/lstf-bc-function.h"
#include "bytecode/lstf-bc-instruction.h"
#include "bytecode/lstf-bc-program.h"
#include "bytecode/lstf-bc-serialize.h"
#include "io/inputstream.h"
#include "io/outputstream.h"
#include "vm/lstf-vm-loader.h"
#include "vm/lstf-vm-opcodes.h"
#include <stdio.h>
#include <string.h>

const uint8_t bytecode[] = {
    LSTFC_MAGIC_HEADER[0], LSTFC_MAGIC_HEADER[1], LSTFC_MAGIC_HEADER[2], LSTFC_MAGIC_HEADER[3],
    LSTFC_MAGIC_HEADER[4], LSTFC_MAGIC_HEADER[5], LSTFC_MAGIC_HEADER[6], LSTFC_MAGIC_HEADER[7],
    // entry point (offset in code section)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // sections
    'd', 'a', 't', 'a', '\0', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    'c', 'o', 'd', 'e', '\0', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C,
    '\0',
    // data section
    '"', 'h', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '\n', '"', '\0',
    // code section
    lstf_vm_op_load_dataoffset, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    lstf_vm_op_print,
    lstf_vm_op_exit, 0x00
};

int main(void)
{
    int retval = 0;
    outputstream *ostream = outputstream_new_from_buffer(NULL, 0, true);
    lstf_bc_program *program = lstf_bc_program_new(NULL);

    // create main function
    lstf_bc_function *main_function = lstf_bc_function_new("main");

    lstf_bc_function_add_instruction(main_function,
            lstf_bc_instruction_load_dataoffset_new(lstf_bc_program_add_data(program, "\"hello, world\n\"")));
    lstf_bc_function_add_instruction(main_function, lstf_bc_instruction_print_new());
    lstf_bc_function_add_instruction(main_function, lstf_bc_instruction_exit_new(0));
    lstf_bc_program_add_function(program, main_function);

    // serialize
    if (!lstf_bc_program_serialize_to_binary(program, ostream)) {
        retval = 1;
        goto end;
    }

    // compare
    if (ostream->buffer_offset != sizeof bytecode) {
        fprintf(stderr, "compiled bytecode differs in size (got %zu bytes; expected %zu bytes)\n", ostream->buffer_offset, sizeof bytecode);
        retval = 1;
    } else if (memcmp(ostream->buffer, bytecode, sizeof bytecode) != 0) {
        fprintf(stderr, "compiled bytecode differs\n");
        for (unsigned i = 0; i < sizeof bytecode; i++)
            if (bytecode[i] != ostream->buffer[i])
                fprintf(stderr, "compiled[%u] = 0x%hhX (expected 0x%hhX)\n", i, ostream->buffer[i], bytecode[i]);
        retval = 1;
    }

end:
    outputstream_unref(ostream);
    lstf_bc_program_destroy(program);
    return retval;
}
