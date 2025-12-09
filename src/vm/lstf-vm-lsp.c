#include "data-structures/string-builder.h"
#include "io/io-common.h"
#include "lstf-vm-lsp.h"
#include "lstf-vm-opcodes.h"
#include "vm/lstf-vm-stack.h"
#include "json/json.h"
#include <stdio.h>

// FIXME: What schema to use for in-memory buffers? See discussion:
// https://github.com/microsoft/language-server-protocol/issues/1676
#define LSTF_VM_CONTENT_URI_FMT "untitled:///buffer%zu"

// handlers for server notifications
static void lstf_vm_handle_window_show_message(
    lsp_client *client, const lsp_showmessageparams *params, void *user_data)
{
    (void) client;
    lstf_virtualmachine *vm = user_data;
    (void) vm;
    const char *domain;

    switch (params->type) {
    case lsp_message_type_error:
        domain = "ERROR";
        break;
    case lsp_message_type_warning:
        domain = "WARNING";
        break;
    case lsp_message_type_info:
        domain = "INFO";
        break;
    case lsp_message_type_log:
        domain = "LOG";
        break;
    default:
        fprintf(stderr, "%s: unreachable code (unexpected type %u)\n", __func__,
                params->type);
        abort();
    }

    fprintf(stderr, "SERVER %s: %s\n", domain, params->message);
}

// handlers for server calls and VM calls

static lstf_vm_status
lstf_vm_vmcall_memory_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    string *filetype = NULL;
    string *content = NULL;

    if (!vm->client)
        return lstf_vm_status_not_connected;

    if ((status = lstf_vm_stack_pop_string(cr->stack, &content)))
        goto cleanup;

    if ((status = lstf_vm_stack_pop_string(cr->stack, &filetype)))
        goto cleanup;

    // Associate the content with a new URI.
    // For example, a Vala document may be "file:///tmp/0.vala"
    string *uri =
        string_newf(LSTF_VM_CONTENT_URI_FMT ".%s",
                    (size_t)vm->client->docs.length, filetype->buffer);
    lsp_textdocument document = {.uri  = strdup(uri->buffer),
                                 .language_id = strdup(filetype->buffer),
                                 .text = string_ref(content)};

    array_add(&vm->client->docs, document);
    if ((status = lstf_vm_stack_push_string(cr->stack, uri)))
        string_unref(uri);

cleanup:
    string_unref(filetype);
    string_unref(content);
    return status;
}

typedef struct {
    lstf_virtualmachine *vm;
    lstf_vm_coroutine   *cr;
    union {
        // for initialize_server_cb()
        string *server_path;
        // for td_open_cb()
        json_node *text_document_uri;
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
        json_parser_messages_foreach(super(vm->client)->parser, msg, {
            unsigned long index = index_of(msg);
            if (index == 0)
                fprintf(stderr, "parser/scanner messages: ... \n");
            fprintf(stderr, " %2lu. %s\n", index + 1, msg);
        });
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
    io_process process = {0};

    if ((status = lstf_vm_stack_pop_string(cr->stack, &path)))
        return status;

    // we can only connect once
    if (vm->client) {
        status = lstf_vm_status_already_connected;
        goto cleanup_on_error;
    }

    // start the server and connect
    if (!io_communicate(path->buffer, (const char *[]){path->buffer, NULL},
                        &stdin_os, &stdout_is, &stderr_is, &process)) {
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
    vm->client = lsp_client_new(vm->event_loop, stdout_is, stdin_os, process);

    // setup notification handlers
    lsp_client_on_window_show_message(
        vm->client, lstf_vm_handle_window_show_message, vm, NULL);

    // now initialize the client
    server_data *data;
    box(server_data, data, vm, cr, .server_path = path);
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
    json_node *text_document_uri = data->text_document_uri;

    free(data);
    data = NULL;

    int errnum = 0;

    if (!lsp_client_text_document_open_finish(ev, &errnum)) {
        fprintf(stderr, "error: could not open text document `%s': %s\n",
                json_node_cast(text_document_uri, string)->value, strerror(errnum));
        lstf_virtualmachine_raise(vm, lstf_vm_status_could_not_communicate);
    } else {
        // success: the text document open request was sent, but this may not be open
    }

    // the coroutine is done
    --cr->outstanding_io;

    // cleanup
    json_node_unref(text_document_uri);
}

static lstf_vm_status
lstf_vm_vmcall_td_open_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    json_node *uri_array = NULL;

    if ((status = lstf_vm_stack_pop_array(cr->stack, &uri_array)))
        return status;

    if (!vm->client) {
        status = lstf_vm_status_not_connected;
        goto cleanup_on_error;
    }

    json_array_foreach(uri_array, element, {
        json_string *uri = json_node_cast(element, string);

        if (!uri) {
            status = lstf_vm_status_invalid_operand_type;
            goto cleanup_on_error;
        }

        // communicate with server...
        size_t doc_idx = -1;
        if (sscanf(uri->value, LSTF_VM_CONTENT_URI_FMT, &doc_idx) != 1 ||
            doc_idx >= json_node_cast(uri_array, array)->num_elements) {
            status = lstf_vm_status_invalid_document_id;
            goto cleanup_on_error;
        }

        // 1. save server data
        server_data *data;
        box(server_data, data, vm, cr,
            .text_document_uri = json_node_ref(element));

        // 2. set outstanding I/O to suspend the coroutine
        ++cr->outstanding_io;

        // 3. send notification
        lsp_client_text_document_open_async(vm->client,
                                            &vm->client->docs.elements[doc_idx],
                                            vm->event_loop,
                                            lstf_vm_vmcall_td_open_exec_td_open_cb,
                                            data);
    });

    return status;

cleanup_on_error:
    json_node_unref(uri_array);
    return status;
}

static void lstf_vm_vmcall_diagnostics_exec_cb(const event *ev, void *user_data)
{
    server_data *data = user_data;
    lstf_virtualmachine *vm = data->vm;
    lstf_vm_coroutine *cr = data->cr;
    int errnum = 0;

    json_node *params = lsp_client_wait_for_diagnostics_finish(ev, &errnum);
    if (errnum) {
        fprintf(stderr, "error: could not deserialize diagnostics: %s\n",
                strerror(errnum));
        lstf_virtualmachine_raise(vm, lstf_vm_status_could_not_communicate);
    } else {
        // success. now add the result to the stack
        lstf_vm_status status = lstf_vm_status_continue;
        if ((status = lstf_vm_stack_push_json(cr->stack, params)))
            lstf_virtualmachine_raise(vm, status);
    }

    // the coroutine is done
    --cr->outstanding_io;
}

static lstf_vm_status
lstf_vm_vmcall_diagnostics_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    string *text_document_uri = NULL;

    if ((status = lstf_vm_stack_pop_string(cr->stack, &text_document_uri)))
        return status;

    if (!vm->client) {
        status = lstf_vm_status_not_connected;
        goto cleanup;
    }


    // 1. save server data
    server_data *data;
    box(server_data, data, .vm = vm, .cr = cr);

    // 2. set outstanding I/O to suspend the coroutine
    ++cr->outstanding_io;

    // 3. wait for diagnostics
    lsp_client_wait_for_diagnostics_async(vm->client, vm->event_loop,
                                          lstf_vm_vmcall_diagnostics_exec_cb, data);

cleanup:
    string_unref(text_document_uri);
    return status;
}

lstf_vm_status (*const vmcall_table[256])(lstf_virtualmachine *, lstf_vm_coroutine *) = {
    [lstf_vm_vmcall_memory]         = lstf_vm_vmcall_memory_exec,
    [lstf_vm_vmcall_connect]        = lstf_vm_vmcall_connect_exec,
    [lstf_vm_vmcall_td_open]        = lstf_vm_vmcall_td_open_exec,
    [lstf_vm_vmcall_diagnostics]    = lstf_vm_vmcall_diagnostics_exec,
    [lstf_vm_vmcall_change]         = NULL /* TODO */,
    [lstf_vm_vmcall_completion]     = NULL /* TODO */,
};
