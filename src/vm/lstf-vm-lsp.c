#include "data-structures/string-builder.h"
#include "io/io-common.h"
#include "lstf-vm-lsp.h"
#include "lstf-vm-opcodes.h"
#include "vm/lstf-vm-stack.h"
#include <stdio.h>

static lstf_vm_status
lstf_vm_vmcall_memory_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    string *content = NULL;

    if ((status = lstf_vm_stack_pop_string(cr->stack, &content)))
        return status;

    // associate the content with a new URI
    string *uri =
        string_newf("content:///buffer/%zu", (size_t)vm->client->docs.length++);
    lsp_textdocument document = {.uri = strdup(uri->buffer),
                                 .content = string_ref(content)};

    array_add(&vm->client->docs, document);
    if ((status = lstf_vm_stack_push_string(cr->stack, uri)))
        string_unref(uri);

    string_unref(content);
    return status;
}

typedef struct {
    lstf_virtualmachine *vm;
    lstf_vm_coroutine *cr;
    union {
        // for initialize_server_cb()
        string *server_path;
        // for td_open_cb()
        string *text_document_uri;
    };
} server_data;

static void 
lstf_vm_vmcall_connect_exec_initialize_server_cb(const event *ev, void *user_data)
{
    server_data *data = user_data;
    lstf_virtualmachine *vm = data->vm;
    lstf_vm_coroutine *cr = data->cr;
    string *server_path = data->server_path;

    free(data);
    data = NULL;

    // get the server's response
    int errnum = 0;
    json_node *response = lsp_client_initialize_server_finish(ev, &errnum);

    if (!response) {
        fprintf(stderr, "error: could not initialize server at `%s': %s\n",
                server_path->buffer, strerror(errnum));
        // print JSON parser/scanner errors
        for (iterator msg_it = json_parser_get_messages(super(vm->client)->parser);
                msg_it.has_next; msg_it = iterator_next(msg_it)) {
            if (msg_it.counter == 0)
                fprintf(stderr, "parser/scanner messages: ... \n");
            const char *msg = iterator_get_item(msg_it);
            fprintf(stderr, "  %lu. %s\n", (unsigned long)(msg_it.counter + 1), msg);
        }
        // queue an exception
        lstf_virtualmachine_raise(vm, lstf_vm_status_initialize_failed);
    } else {
        // discard the result
        json_node_unref(response);
        response = NULL;
    }

    // the coroutine is done
    --cr->outstanding_io;

    // cleanup
    string_unref(server_path);
}

static lstf_vm_status
lstf_vm_vmcall_connect_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    string *path = NULL;
    // the stdin, stdout, and stderr of the server
    outputstream *stdin_os = NULL;
    inputstream *stdout_is = NULL;
    inputstream *stderr_is = NULL;

    if ((status = lstf_vm_stack_pop_string(cr->stack, &path)))
        return status;

    // we can only connect once
    if (vm->client) {
        status = lstf_vm_status_already_connected;
        goto cleanup_on_error;
    }

    // start the server and connect
    if (!io_communicate(path->buffer, (const char *[]){path->buffer, NULL},
                        &stdin_os, &stdout_is, &stderr_is)) {
        char stderr_buf[1024] = {0};
        if (stderr_is &&
            inputstream_read(stderr_is, stderr_buf, sizeof(stderr_buf) - 1)) {
          // print child's stderr
          fprintf(stderr, "error: child `%s' failed with message:\n%s\n",
                  path->buffer, stderr_buf);
        } else {
          fprintf(stderr, "error: could not launch child `%s'\n", path->buffer);
        }
        status = lstf_vm_status_could_not_connect;
        goto cleanup_on_error;
    }

    // now create the language client
    vm->client = lsp_client_new(stdout_is, stdin_os);

    // now initialize the client
    server_data *data;
    box(server_data, data) {
        .vm = vm,
        .cr = cr,
        .server_path = path
    };
    path = NULL;
    // set outstanding I/O to suspend the coroutine
    ++cr->outstanding_io;
    lsp_client_initialize_server_async(vm->client,
                                       io_get_current_dir(),
                                       vm->event_loop,
                                       lstf_vm_vmcall_connect_exec_initialize_server_cb,
                                       data);

    if (stderr_is)
        inputstream_unref(stderr_is);
    return status;

cleanup_on_error:
    string_unref(path);
    if (stdin_os)
        outputstream_unref(stdin_os);
    if (stdout_is)
        inputstream_unref(stdout_is);
    if (stderr_is)
        inputstream_unref(stderr_is);
    return status;
}

static void
lstf_vm_vmcall_td_open_exec_td_open_cb(const event *ev, void *user_data)
{
    server_data *data = user_data;
    lstf_virtualmachine *vm = data->vm;
    lstf_vm_coroutine *cr = data->cr;
    string *text_document_uri = data->text_document_uri;

    free(data);
    data = NULL;

    int errnum = 0;
    json_node *response = jsonrpc_server_call_remote_finish(ev, &errnum);

    if (!response) {
        fprintf(stderr, "error: could not open text document `%s': %s\n",
                text_document_uri->buffer, strerror(errnum));
        lstf_virtualmachine_raise(vm, lstf_vm_status_could_not_communicate);
    } else {
        // discard the result
        json_node_unref(response);
        response = NULL;

        // success: the text document is now open
    }

    // the coroutine is done
    --cr->outstanding_io;

    // cleanup
    string_unref(text_document_uri);
}

static lstf_vm_status
lstf_vm_vmcall_td_open_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    string *text_document_uri = NULL;

    if ((status = lstf_vm_stack_pop_string(cr->stack, &text_document_uri)))
        return status;

    if (!vm->client) {
        status = lstf_vm_status_not_connected;
        goto cleanup_on_error;
    }

    // communicate with server
    json_node *request = json_object_new();
    server_data *data;
    box(server_data, data) {
        .vm = vm,
        .cr = cr,
        .text_document_uri = text_document_uri
    };
    // set outstanding I/O to suspend the coroutine
    ++cr->outstanding_io;
    jsonrpc_server_call_remote_async(super(vm->client),
                                     "textDocument/didOpen",
                                     request,
                                     vm->event_loop,
                                     lstf_vm_vmcall_td_open_exec_td_open_cb,
                                     data);

    return status;

cleanup_on_error:
    string_unref(text_document_uri);
    return status;
}

lstf_vm_status (*const vmcall_table[256])(lstf_virtualmachine *, lstf_vm_coroutine *) = {
    [lstf_vm_vmcall_memory]         = lstf_vm_vmcall_memory_exec,
    [lstf_vm_vmcall_connect]        = lstf_vm_vmcall_connect_exec,
    [lstf_vm_vmcall_td_open]        = lstf_vm_vmcall_td_open_exec,
    [lstf_vm_vmcall_diagnostics]    = NULL /* TODO */,
    [lstf_vm_vmcall_change]         = NULL /* TODO */,
    [lstf_vm_vmcall_completion]     = NULL /* TODO */,
};
