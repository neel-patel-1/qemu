/*
 * Copyright (C) 2021, Mahmoud Mandour <ma.mandourr@gmail.com>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#include <inttypes.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;

static int cores;
static bool sys;

typedef struct {
    bool is_userland;
    uint16_t asid;
    uint64_t vaddr;
    uint64_t haddr;
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

static void vcpu_mem_access(unsigned int vcpu_index, qemu_plugin_meminfo_t info,
                            uint64_t vaddr, void *userdata)
{
    uint64_t effective_addr;
    struct qemu_plugin_hwaddr *hwaddr;
    bool is_store = qemu_plugin_mem_is_store(info);

    hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
    if (hwaddr && qemu_plugin_hwaddr_is_io(hwaddr)) {
        return;
    }

    effective_addr = hwaddr ? qemu_plugin_hwaddr_phys_addr(hwaddr) : vaddr;
    g_autoptr(GString) mem_access_log = g_string_new(":");
    g_string_append_printf(mem_access_log, "%s[%016lx]PA[%016lx]", is_store ? "WR" : "LD", vaddr, effective_addr);
//    qemu_plugin_outs(mem_access_log->str);
}

static void vcpu_insn_exec(unsigned int vcpu_index, void *userdata)
{
    uint64_t insn_addr;
    uint32_t insn_code, insn_vcpu_n_userland;
    InsnData *insn_data = (InsnData *) userdata;


    insn_vcpu_n_userland = (insn_data->is_userland << 15) | vcpu_index | (insn_data->asid << 16);
    insn_addr = insn_data->vaddr;
    insn_code = *((uint32_t *) insn_data->haddr);
    g_autoptr(GString) insn_log = g_string_new("");
    g_string_append_printf(insn_log, "%016x", insn_data->haddr);
    qemu_plugin_outs(insn_log->str);
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n_insns;
    size_t i;
    InsnData *data;

    n_insns = qemu_plugin_tb_n_insns(tb);
    for (i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t haddr = (uint64_t) qemu_plugin_insn_haddr(insn);

        /*
         * Instructions might get translated multiple times, we do not create
         * new entries for those instructions. Instead, we fetch the same
         * entry from the hash table and register it for the callback again.
         */
        g_mutex_lock(&hashtable_lock);
        data = g_hash_table_lookup(eg_hashtable, GUINT_TO_POINTER(haddr));
        if (data == NULL) {
            data = g_new0(InsnData, 1);
            data->is_userland = qemu_plugin_is_userland(insn);
            data->asid = qemu_plugin_get_asid(insn);
            data->vaddr = (uint64_t) qemu_plugin_insn_vaddr(insn);
            data->haddr = haddr;
            g_hash_table_insert(eg_hashtable, GUINT_TO_POINTER(haddr),
                               (gpointer) data);
        }
        g_mutex_unlock(&hashtable_lock);


        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem_access,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, data);

        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, data);
    }
}

static void log_stats(void)
{
    g_autoptr(GString) line = g_string_new("cores, scalar example, percent example"
                                          " description text here");
    g_string_append(line, "results:");
    g_string_append_printf(line, "%i %-14lu %9.4lf%%", cores, 12500LU, ((double) 15) / (150) * 100.0);
    g_string_append(line, "\n");

    // Actual print out
    //qemu_plugin_outs(line->str);
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    log_stats();

    g_free(eg_locks_1);
    g_free(eg_locks_2);

    g_hash_table_destroy(eg_hashtable);
}

static void hashtable_data_free(gpointer data)
{
    InsnData *data_ptr = (InsnData *) data;
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

        if (g_strcmp0(tokens[0], "setting1") == 0) {
            eg_setting_1 = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "setting2") == 0) {
            if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &eg_setting_2)) {
                fprintf(stderr, "boolean argument parsing failed: %s\n", opt);
                return -1;
            }
        } else if (g_strcmp0(tokens[0], "setting3") == 0) {
            if (g_strcmp0(tokens[1], "op1") == 0) {
                eg_setting_3 = OP1;
            } else if (g_strcmp0(tokens[1], "op2") == 0) {
                eg_setting_3 = OP2;
            } else if (g_strcmp0(tokens[1], "op3") == 0) {
                eg_setting_3 = OP3;
            } else {
                fprintf(stderr, "invalid setting3 op: %s\n", opt);
                return -1;
            }
        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }

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
