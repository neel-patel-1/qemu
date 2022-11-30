/*
 * Copyright (C) 2021, Mahmoud Mandour <ma.mandourr@gmail.com>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <glib.h>
#include <pthread.h>
#include "cache-parallel.h"

#include <qemu-plugin.h>

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
int cores = 0;

static void vcpu_insn_exec(unsigned int vcpu_index, void *encoded)
{
    if(vcpu_index != 1) {return;}
    uint8_t *addr = (uint8_t *) ((uint64_t) encoded & ~(0xFL << 60));
    uint64_t bytesize = (uint64_t) encoded >> 60;
    g_autoptr(GString) rep = g_string_new("");
    g_string_append_printf(rep, "%lu:", bytesize);
    for(int byte = 0; byte < bytesize; byte++) {
        g_string_append_printf(rep, " %02x", addr[byte]);
    }
    g_string_append_printf(rep, "\n");
    qemu_plugin_outs(rep->str);
}

static void vcpu_tb_exec(unsigned int vcpu_index, void *encoded)
{
    if(vcpu_index != 1) {return;}
    uint8_t *addr = (uint8_t *) ((uint64_t) encoded & ~(0xFL << 60));
    uint64_t bytesize = (uint64_t) encoded >> 60;
    g_autoptr(GString) rep = g_string_new("");
    g_string_append_printf(rep, "%lu:", bytesize);
    for(int byte = 0; byte < bytesize; byte++) {
        g_string_append_printf(rep, " %02x", addr[byte]);
    }
    g_string_append_printf(rep, "\n");
    qemu_plugin_outs(rep->str);
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n_insns;
    size_t i;

    struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, 0);
    uint64_t basic_block_addr = (uint64_t) qemu_plugin_insn_haddr(insn);
    bool is_user = qemu_plugin_is_userland(insn);
    n_insns = qemu_plugin_tb_n_insns(tb);
    bool print_block = false;
    size_t tot_bytes = 0;
    if (!is_user) {
        print_block = true;
        for (i = 0; i < n_insns; i++) {
            insn = qemu_plugin_tb_get_insn(tb, i);
            size_t bytesize = qemu_plugin_insn_size(insn);
            tot_bytes += bytesize;
        }
    }

    if(print_block) {
        uint64_t encoded = tot_bytes << 60 | basic_block_addr;
        qemu_plugin_register_vcpu_tb_exec_cb(tb, vcpu_tb_exec,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         (void *) encoded);
        //qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
        //                                   QEMU_PLUGIN_CB_NO_REGS, 
        //                                   (void *) encoded);
    }

}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    cores = qemu_plugin_n_vcpus();

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
