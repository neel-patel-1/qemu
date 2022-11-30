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

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;

#define TOT_SIM 6

static GHashTable *miss_ht;

static GMutex hashtable_lock;

static size_t l2_sizes[TOT_SIM] = {
    262144, // 256KB
    524288, // 512KB
    1048576, // 1MB
    2097152, // 2MB
    4194304, // 3MB
    8388608 // 4MB
};

static CacheConfig configs[TOT_SIM];
static pthread_t thread_ids[TOT_SIM];
static FifoQueue sim_queues[TOT_SIM];

static size_t totInsn = 0;
static size_t byteSizeDist[2][32][16] = {0};

typedef struct {
    uint64_t addr;
    bool is_user;
    size_t size;
} InsnData;

static uint64_t last_haddr = 0;


static void vcpu_mem_access(unsigned int vcpu_index, qemu_plugin_meminfo_t info,
                            uint64_t vaddr, void *userdata)
{
    struct qemu_plugin_hwaddr *hwaddr;
#ifdef CONFIG_4_7
    if(!(vcpu_index == 4 || vcpu_index == 5 || vcpu_index == 6 || vcpu_index == 7)) { return; }
#elif CONFIG_1_3
    if(!(vcpu_index == 1 || vcpu_index == 2 || vcpu_index == 3)) { return; }
#elif CONFIG_2_3
    if(!(vcpu_index == 2 || vcpu_index == 3)) { return; }
#elif CONFIG_5_7
    if(!(vcpu_index == 5 || vcpu_index == 6 || vcpu_index == 7)) { return; }
#elif CONFIG_1
    if(vcpu_index != 1) { return; }
#endif
    hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
    if (hwaddr && qemu_plugin_hwaddr_is_io(hwaddr)) {
        return;
    }
    uint64_t haddr = qemu_plugin_hwaddr_phys_addr(hwaddr);
    bool is_user = ((InsnData *) userdata)->is_user;

    for (int idx = 0; idx < TOT_SIM; idx++) {
        while(!queue_can_push(&sim_queues[idx])) {
            g_autoptr(GString) rep = g_string_new("spinning on idx");
            g_string_append_printf(rep, "%i", idx);
            qemu_plugin_outs(rep->str);
        }
        AccessReq req = {.addr = haddr, .type = DATA_LOAD, .is_user = is_user, .vcpu_idx = vcpu_index};
        queue_push(&sim_queues[idx], req);
    }
}

static void vcpu_insn_exec(unsigned int vcpu_index, void *userdata)
{
#ifdef CONFIG_4_7
    if(!(vcpu_index == 4 || vcpu_index == 5 || vcpu_index == 6 || vcpu_index == 7)) { return; }
#elif CONFIG_1_3
    if(!(vcpu_index == 1 || vcpu_index == 2 || vcpu_index == 3)) { return; }
#elif CONFIG_2_3
    if(!(vcpu_index == 2 || vcpu_index == 3)) { return; }
#elif CONFIG_5_7
    if(!(vcpu_index == 5 || vcpu_index == 6 || vcpu_index == 7)) { return; }
#elif CONFIG_1
    if(vcpu_index != 1) { return; }
#endif

 
    size_t byteSize = ((InsnData *) userdata)->size;
    uint64_t haddr = ((InsnData *) userdata)->addr;
    bool is_user = ((InsnData *) userdata)->is_user;

    if(haddr == last_haddr) { return; }
    last_haddr = haddr;

    if(is_user) {
        byteSizeDist[0][vcpu_index][byteSize]++;
    } else {
        byteSizeDist[1][vcpu_index][byteSize]++;
    }

    totInsn++;
    if((totInsn % 1000000000) == 0) {
        g_autoptr(GString) rep = g_string_new("cpu,byte,user,kernel");
        g_string_append_printf(rep, "[%016ld]\n", totInsn); 
#ifdef CONFIG_1
        for(int cpu = 1; cpu < 2; cpu++) {
#else
        for(int cpu = 0; cpu < 16; cpu++) {
#endif
            for(int insnSize = 0; insnSize < 16; insnSize++) {
                g_string_append_printf(rep, "%u,%u,%016ld,%016ld\n", 
                                   cpu, insnSize, byteSizeDist[0][cpu][insnSize],
                                   byteSizeDist[1][cpu][insnSize]);
            }
        }
        qemu_plugin_outs(rep->str);
    }

    for (int idx = 0; idx < TOT_SIM; idx++) {
        while(!queue_can_push(&sim_queues[idx])) {
            g_autoptr(GString) rep = g_string_new("spinning on idx");
            g_string_append_printf(rep, "%i", idx);
            qemu_plugin_outs(rep->str);
        }
        AccessReq req = {.addr = haddr, .type = INSN_FETCH, .is_user = is_user, .vcpu_idx = vcpu_index};
        queue_push(&sim_queues[idx], req);
    }
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n_insns;
    size_t i;
    InsnData *data;

    n_insns = qemu_plugin_tb_n_insns(tb);
    for (i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t effective_addr;

        effective_addr = (uint64_t) qemu_plugin_insn_haddr(insn);

        /*
         * Instructions might get translated multiple times, we do not create
         * new entries for those instructions. Instead, we fetch the same
         * entry from the hash table and register it for the callback again.
         */
        g_mutex_lock(&hashtable_lock);
        data = g_hash_table_lookup(miss_ht, GUINT_TO_POINTER(effective_addr));
        if (data == NULL) {
            data = g_new0(InsnData, 1);
            data->addr = effective_addr;
            data->is_user = qemu_plugin_is_userland(insn);
            data->size = qemu_plugin_insn_size(insn);
            g_hash_table_insert(miss_ht, GUINT_TO_POINTER(effective_addr),
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

static void insn_free(gpointer data)
{
    InsnData *insn = (InsnData *) data;
    g_free(insn);
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    g_hash_table_destroy(miss_ht);
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    int i;
    int l1_iassoc, l1_iblksize, l1_icachesize;
    int l1_dassoc, l1_dblksize, l1_dcachesize;

    int sys = info->system_emulation;

    l1_dassoc = 8;
    l1_dblksize = 64;
    l1_dcachesize = l1_dblksize * l1_dassoc * 32;

    l1_iassoc = 8;
    l1_iblksize = 64;
    l1_icachesize = l1_iblksize * l1_iassoc * 32;

    int cores = sys ? qemu_plugin_n_vcpus() : 1;

    for (i = 0; i < argc; i++) {
        char *opt = argv[i];
        g_autofree char **tokens = g_strsplit(opt, "=", 2);

        if (g_strcmp0(tokens[0], "iblksize") == 0) {
            l1_iblksize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "iassoc") == 0) {
            l1_iassoc = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "icachesize") == 0) {
            l1_icachesize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "dblksize") == 0) {
            l1_dblksize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "dassoc") == 0) {
            l1_dassoc = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "dcachesize") == 0) {
            l1_dcachesize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "cores") == 0) {
            cores = STRTOLL(tokens[1]);
        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }

    for (int idx = 0; idx < TOT_SIM; idx++) {
        configs[idx].cores = cores;
        configs[idx].l1_dassoc     = l1_dassoc;
        configs[idx].l1_dblksize   = l1_dblksize;
        configs[idx].l1_dcachesize = l1_dcachesize;
        configs[idx].l1_iassoc     = l1_iassoc;
        configs[idx].l1_iblksize   = l1_iblksize;
        configs[idx].l1_icachesize = l1_icachesize;
        configs[idx].l2_assoc      = 16; 
        configs[idx].l2_blksize    = 64; 
        queue_init(&sim_queues[idx], 16384);
        configs[idx].l2_cachesize = l2_sizes[idx];
        configs[idx].queue = &sim_queues[idx];
        pthread_create(&thread_ids[idx], NULL, &sim_init, (void *) &configs[idx]);
    }

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    miss_ht = g_hash_table_new_full(NULL, g_direct_equal, NULL, insn_free);

    return 0;
}
