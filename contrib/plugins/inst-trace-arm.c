/*
 * Copyright (C) 2021, Mahmoud Mandour <ma.mandourr@gmail.com>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <glib.h>

#include <qemu-plugin.h>

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static int cores;
static bool sys;

typedef struct {
    uint64_t pc_phys;
    size_t n_insns;
    uint32_t *insn_bytes;
} InsnData;

enum OpExample {
    OP1,
    OP2,
    OP3,
    OPDEF,
};

static GMutex *eg_locks_1;
static GMutex *eg_locks_2;
static GMutex hashtable_lock;
static GHashTable *eg_hashtable;

static long tot_insn = 0;

FILE *fp;

static void vcpu_tb_exec(unsigned int cpu_index, void *udata)
{
    InsnData *insn_data = (InsnData *) udata;
    if (cpu_index == 1) {
        size_t n_insns = insn_data->n_insns;
        fwrite(insn_data, sizeof(uint64_t) + sizeof(size_t), 1, fp);
        fwrite(insn_data->insn_bytes, sizeof(uint32_t)*n_insns, 1, fp);
        tot_insn += insn_data->n_insns;
        if((tot_insn % 1000000000) <= insn_data->n_insns) {
            g_autoptr(GString) rep = g_string_new("tot_insn:");
            g_string_append_printf(rep, "%016ld\n", tot_insn); 
            qemu_plugin_outs(rep->str);
        }
    }
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n_insns;
    InsnData *data;

    n_insns = qemu_plugin_tb_n_insns(tb);
    struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, 0);
    uint64_t haddr = (uint64_t) qemu_plugin_insn_haddr(insn);

    g_mutex_lock(&hashtable_lock);
    data = g_hash_table_lookup(eg_hashtable, GUINT_TO_POINTER(haddr));
    if (data == NULL) {
        data = g_new0(InsnData, 1);
        bool is_user = qemu_plugin_is_userland(insn);
        data->pc_phys = haddr | (size_t) is_user << 63;
        data->n_insns = n_insns;
        data->insn_bytes = calloc(n_insns, sizeof(uint32_t));
        memcpy(data->insn_bytes, (uint32_t *) haddr, n_insns*sizeof(uint32_t));
        g_hash_table_insert(eg_hashtable, GUINT_TO_POINTER(haddr),
                            (gpointer) data);
    }
    g_mutex_unlock(&hashtable_lock);

    qemu_plugin_register_vcpu_tb_exec_cb(tb, vcpu_tb_exec,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         (void *)data);
}

static void log_stats(void)
{

}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    log_stats();

    g_free(eg_locks_1);
    g_free(eg_locks_2);

    for (int i = 0; i < 8; i++) {
        fclose(fp);
    }

    g_hash_table_destroy(eg_hashtable);
}

static void hashtable_data_free(gpointer data)
{
    InsnData *data_ptr = (InsnData *) data;
    g_free(data_ptr->insn_bytes);
    g_free(data_ptr);
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    int i;

    int eg_setting_1, eg_setting_3;
    bool eg_setting_2;

    sys = info->system_emulation;

    cores = sys ? qemu_plugin_n_vcpus() : 1;

    eg_setting_1 = 0xdeadbeef;
    eg_setting_2 = false;
    eg_setting_3 = OPDEF;

    for (i = 0; i < argc; i++) {
        char *opt = argv[i];
        g_autofree char **tokens = g_strsplit(opt, "=", 2);

        if (g_strcmp0(tokens[0], "tot_insn") == 0) {
            tot_insn = STRTOLL(tokens[1]);
        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }

    tot_insn = 0;
    if (tot_insn < 0) {
        abort();
    }

    g_autoptr(GString) path = g_string_new("insn-trace-arm");
    fp = fopen(path->str, "w");

    eg_locks_1 = g_new0(GMutex, cores);
    eg_locks_2 = g_new0(GMutex, cores);

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    eg_hashtable = g_hash_table_new_full(NULL, g_direct_equal, NULL, hashtable_data_free);
    
    printf("Plugin setup done\n"
           " eg_setting_1:%i \n"
           " eg_setting_2:%i \n"
           " eg_setting_3:%i \n",
           eg_setting_1, eg_setting_2, eg_setting_3
    );

    return 0;
}
