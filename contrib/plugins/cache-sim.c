/*
 * Copyright (C) 2021, Mahmoud Mandour <ma.mandourr@gmail.com>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#include <inttypes.h>
#include <stdio.h>
#include <math.h>

#include <glib.h>

#include <qemu-plugin.h>

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static int cores;
static bool sys;

typedef struct {
    uint64_t pc;
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

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static uint64_t cache[16384][8] = {0};
static uint64_t lru[16384][8] = {0};
static uint64_t tot_access = 0;
static uint64_t hits = 0;
static uint64_t miss = 0;
static int n_ways = 8;
static int n_sets = 0;

static void simulate(uint64_t pc) {
    uint64_t cache_addr = pc;
    uint64_t cache_set_tag = cache_addr >> 6;
    uint64_t cache_set_baseaddr = (cache_set_tag & ((1 << n_sets) - 1));
    uint64_t *cache_set = &cache[cache_set_baseaddr][0];
    uint64_t *lru_set = &lru[cache_set_baseaddr][0];

    bool hit = false;
    uint32_t hit_idx = 0;
    uint32_t lru_idx = 0;
    uint32_t lru_of_set = 0;
    uint32_t tot_valid_set = 0;
    uint32_t invalid_idx = 0;

    tot_access +=1;
    // Check set
    for(int way = 0; way < n_ways; way++) {
        uint32_t entry_idx = way;
        uint64_t entry = cache_set[entry_idx];
        uint64_t entry_tag = entry & ~(1UL << 63);
        bool entry_is_valid = (entry & (1UL << 63)) != 0;
        uint64_t entry_lru = lru_set[entry_idx];

        if (entry_is_valid) {
            tot_valid_set += 1;
            // Update current lru
            if (entry_lru < lru_of_set) {
                lru_of_set = lru_set[entry_idx];
                lru_idx = entry_idx;
            }

            // Check for hit
            if (entry_tag == cache_set_tag) {
                hit_idx = entry_idx;
                hit = true;
                break;
            }
		} else {
			invalid_idx = entry_idx;
		}
    }

    // Perform operation
    if (hit) {
        hits += 1;
        lru_set[hit_idx] = tot_access;
        cache_set[hit_idx] = cache_set_tag | (1UL << 63);
    } else {
        miss += 1;
		if(tot_valid_set == n_ways) {
        	lru_set[lru_idx] = tot_access;
        	cache_set[lru_idx] = cache_set_tag | (1UL << 63);
		} else {
        	lru_set[invalid_idx] = tot_access;
        	cache_set[invalid_idx] = cache_set_tag | (1UL << 63);
		}
    }
}

static void vcpu_insn_exec(unsigned int vcpu_index, void *userdata)
{
    uint64_t pc;
    InsnData *insn_data = (InsnData *) userdata;
    pc = insn_data->pc;
    if(vcpu_index == 1) {
        simulate(pc);
    }

    //g_autoptr(GString) insn_log = g_string_new("\nPC");
    //g_string_append_printf(insn_log, "[%04x][%016lx][%08x]", insn_vcpu_n_userland, insn_addr, insn_code);
    //qemu_plugin_outs(insn_log->str);
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
            data->pc = haddr;
            g_hash_table_insert(eg_hashtable, GUINT_TO_POINTER(haddr),
                               (gpointer) data);
        }
        g_mutex_unlock(&hashtable_lock);


        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, data);
    }
}

static void log_stats(void)
{
    g_autoptr(GString) line = g_string_new("cores, scalar example, percent example"
                                          " description text here");
    g_string_append(line, "results:");
    g_string_append_printf(line, "%lu %lu %lu", tot_access, hits, miss);
    g_string_append(line, "\n");

    // Actual print out
    qemu_plugin_outs(line->str);
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

        if (g_strcmp0(tokens[0], "ways") == 0) {
            n_ways = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "sets") == 0) {
            n_sets = STRTOLL(tokens[1]);
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

// static int main(int argc, char *argv[]) {
// 
//     n_sets = atoi(argv[1]);
//     printf("sets: %u", n_sets);
//     FILE *fp;
//     fp = fopen("./log", "r");
// 
//     char hex[16] = {0};
//     int ret = 0;
//     for(int ite = 0; ite < 100000; ite++) {
//         ret = fread(hex, 16*sizeof(char), 1, fp);
//         if (ret <= 0) {
//             break;
//         }
// 	    uint64_t number = (uint64_t) strtol(hex, NULL, 16);
//         simulate(number);
//     }
//     printf("tot:%lu, miss:%lu, hit:%lu", tot_access, miss, hits);
// 
//     return 1;
// }

