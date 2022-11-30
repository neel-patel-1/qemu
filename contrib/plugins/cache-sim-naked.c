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
#include <stdbool.h>
#include "cache-parallel.h"

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

/*
 * A CacheSet is a set of cache blocks. A memory block that maps to a set can be
 * put in any of the blocks inside the set. The number of block per set is
 * called the associativity (assoc).
 *
 * Each block contains the the stored tag and a valid bit. Since this is not
 * a functional simulator, the data itself is not stored. We only identify
 * whether a block is in the cache or not by searching for its tag.
 *
 * In order to search for memory data in the cache, the set identifier and tag
 * are extracted from the address and the set is probed to see whether a tag
 * match occur.
 *
 * An address is logically divided into three portions: The block offset,
 * the set number, and the tag.
 *
 * The set number is used to identify the set in which the block may exist.
 * The tag is compared against all the tags of a set to search for a match. If a
 * match is found, then the access is a hit.
 *
 * The CacheSet also contains bookkeaping information about eviction details.
 */

typedef struct {
    uint64_t tag;
    bool valid;
} CacheBlock;

typedef struct {
    CacheBlock *blocks;
    uint64_t *lru_priorities;
    uint64_t lru_gen_counter;
} CacheSet;

typedef struct {
    CacheSet *sets;
    int num_sets;
    int cachesize;
    int assoc;
    int blksize_shift;
    uint64_t set_mask;
    uint64_t tag_mask;
    uint64_t access;
    uint64_t misses;

    uint64_t access_user;
    uint64_t misses_user;

    uint64_t access_kernel;
    uint64_t misses_kernel;

    uint64_t l2_daccess;
    uint64_t l2_iaccess;
    uint64_t l2_dmisses;
    uint64_t l2_imisses;

    uint64_t l2_imisses_user;
    uint64_t l2_dmisses_user;
    uint64_t l2_iaccess_user;
    uint64_t l2_daccess_user;

    uint64_t l2_imisses_kernel;
    uint64_t l2_dmisses_kernel;
    uint64_t l2_iaccess_kernel;
    uint64_t l2_daccess_kernel;
} Cache;

static __thread int cores;
static __thread Cache **l1_dcaches, **l1_icaches;

static __thread Cache **l2_ucaches;

static __thread FILE *logfile;

static __thread uint64_t iaccess = 0;

static __thread uint64_t l1_daccess;
static __thread uint64_t l1_iaccess;
static __thread uint64_t l1_imisses;
static __thread uint64_t l1_dmisses;

static __thread uint64_t l1_daccess_user;
static __thread uint64_t l1_iaccess_user;
static __thread uint64_t l1_imisses_user;
static __thread uint64_t l1_dmisses_user;

static __thread uint64_t l1_daccess_kernel;
static __thread uint64_t l1_iaccess_kernel;
static __thread uint64_t l1_imisses_kernel;
static __thread uint64_t l1_dmisses_kernel;


static __thread uint64_t l2_mem_access;
static __thread uint64_t l2_misses;
static __thread uint64_t l2_imisses;
static __thread uint64_t l2_dmisses;
static __thread uint64_t l2_iaccess;
static __thread uint64_t l2_daccess;

static __thread uint64_t l2_imisses_user;
static __thread uint64_t l2_dmisses_user;
static __thread uint64_t l2_iaccess_user;
static __thread uint64_t l2_daccess_user;

static __thread uint64_t l2_imisses_kernel;
static __thread uint64_t l2_dmisses_kernel;
static __thread uint64_t l2_iaccess_kernel;
static __thread uint64_t l2_daccess_kernel;
 
static void log_stats(void);

static int pow_of_two(int num)
{
    g_assert((num & (num - 1)) == 0);
    int ret = 0;
    while (num /= 2) {
        ret++;
    }
    return ret;
}

/*
 * LRU evection policy: For each set, a generation counter is maintained
 * alongside a priority array.
 *
 * On each set access, the generation counter is incremented.
 *
 * On a cache hit: The hit-block is assigned the current generation counter,
 * indicating that it is the most recently used block.
 *
 * On a cache miss: The block with the least priority is searched and replaced
 * with the newly-cached block, of which the priority is set to the current
 * generation number.
 */

static void lru_priorities_init(Cache *cache)
{
    int i;

    for (i = 0; i < cache->num_sets; i++) {
        cache->sets[i].lru_priorities = g_new0(uint64_t, cache->assoc);
        cache->sets[i].lru_gen_counter = 0;
    }
}

static void lru_update_blk(Cache *cache, int set_idx, int blk_idx)
{
    CacheSet *set = &cache->sets[set_idx];
    set->lru_priorities[blk_idx] = cache->sets[set_idx].lru_gen_counter;
    set->lru_gen_counter++;
}

static int lru_get_lru_block(Cache *cache, int set_idx)
{
    int i, min_idx, min_priority;

    min_priority = cache->sets[set_idx].lru_priorities[0];
    min_idx = 0;

    for (i = 1; i < cache->assoc; i++) {
        if (cache->sets[set_idx].lru_priorities[i] < min_priority) {
            min_priority = cache->sets[set_idx].lru_priorities[i];
            min_idx = i;
        }
    }
    return min_idx;
}

static void lru_priorities_destroy(Cache *cache)
{
    int i;

    for (i = 0; i < cache->num_sets; i++) {
        g_free(cache->sets[i].lru_priorities);
    }
}

static inline uint64_t extract_tag(Cache *cache, uint64_t addr)
{
    return addr & cache->tag_mask;
}

static inline uint64_t extract_set(Cache *cache, uint64_t addr)
{
    return (addr & cache->set_mask) >> cache->blksize_shift;
}

static const char *cache_config_error(int blksize, int assoc, int cachesize)
{
    if (cachesize % blksize != 0) {
        return "cache size must be divisible by block size";
    } else if (cachesize % (blksize * assoc) != 0) {
        return "cache size must be divisible by set size (assoc * block size)";
    } else {
        return NULL;
    }
}

static bool bad_cache_params(int blksize, int assoc, int cachesize)
{
    return (cachesize % blksize) != 0 || (cachesize % (blksize * assoc) != 0);
}

static Cache *cache_init(int blksize, int assoc, int cachesize)
{
    Cache *cache;
    int i;
    uint64_t blk_mask;

    /*
     * This function shall not be called directly, and hence expects suitable
     * parameters.
     */
    g_assert(!bad_cache_params(blksize, assoc, cachesize));

    cache = g_new(Cache, 1);
    cache->assoc = assoc;
    cache->cachesize = cachesize;
    cache->num_sets = cachesize / (blksize * assoc);
    cache->sets = g_new(CacheSet, cache->num_sets);
    cache->blksize_shift = pow_of_two(blksize);
    cache->access = 0;
    cache->misses = 0;

    cache->access_user = 0;
    cache->misses_user = 0;
    cache->access_kernel = 0;
    cache->misses_kernel = 0;



    cache->l2_dmisses = 0;
    cache->l2_imisses = 0;
    cache->l2_daccess = 0;
    cache->l2_iaccess = 0;

    cache->l2_dmisses_user = 0;
    cache->l2_imisses_user = 0;
    cache->l2_daccess_user = 0;
    cache->l2_iaccess_user = 0;

    cache->l2_dmisses_kernel = 0;
    cache->l2_imisses_kernel = 0;
    cache->l2_daccess_kernel = 0;
    cache->l2_iaccess_kernel = 0;



    for (i = 0; i < cache->num_sets; i++) {
        cache->sets[i].blocks = g_new0(CacheBlock, assoc);
    }

    blk_mask = blksize - 1;
    cache->set_mask = ((cache->num_sets - 1) << cache->blksize_shift);
    cache->tag_mask = ~(cache->set_mask | blk_mask);

    lru_priorities_init(cache);

    return cache;
}

static Cache **caches_init(int blksize, int assoc, int cachesize)
{
    Cache **caches;
    int i;

    if (bad_cache_params(blksize, assoc, cachesize)) {
        return NULL;
    }

    caches = g_new(Cache *, cores);

    for (i = 0; i < cores; i++) {
        caches[i] = cache_init(blksize, assoc, cachesize);
    }

    return caches;
}

static int get_invalid_block(Cache *cache, uint64_t set)
{
    int i;

    for (i = 0; i < cache->assoc; i++) {
        if (!cache->sets[set].blocks[i].valid) {
            return i;
        }
    }

    return -1;
}

static int get_replaced_block(Cache *cache, int set)
{
    return lru_get_lru_block(cache, set);
}

static int in_cache(Cache *cache, uint64_t addr)
{
    int i;
    uint64_t tag, set;

    tag = extract_tag(cache, addr);
    set = extract_set(cache, addr);

    for (i = 0; i < cache->assoc; i++) {
        if (cache->sets[set].blocks[i].tag == tag &&
                cache->sets[set].blocks[i].valid) {
            return i;
        }
    }

    return -1;
}

/**
 * access_cache(): Simulate a cache access
 * @cache: The cache under simulation
 * @addr: The address of the requested memory location
 *
 * Returns true if the requsted data is hit in the cache and false when missed.
 * The cache is updated on miss for the next access.
 */
static bool access_cache(Cache *cache, uint64_t addr)
{
    int hit_blk, replaced_blk;
    uint64_t tag, set;

    tag = extract_tag(cache, addr);
    set = extract_set(cache, addr);

    hit_blk = in_cache(cache, addr);
    if (hit_blk != -1) {
        lru_update_blk(cache, set, hit_blk);
        return true;
    }

    replaced_blk = get_invalid_block(cache, set);

    if (replaced_blk == -1) {
        replaced_blk = get_replaced_block(cache, set);
    }

    lru_update_blk(cache, set, replaced_blk);

    cache->sets[set].blocks[replaced_blk].tag = tag;
    cache->sets[set].blocks[replaced_blk].valid = true;

    return false;
}

static bool inclusive_access_cache(Cache *cache, uint64_t addr, uint64_t *replaced_blk_addr)
{
    int hit_blk, replaced_blk;
    uint64_t tag, set;

    tag = extract_tag(cache, addr);
    set = extract_set(cache, addr);

    hit_blk = in_cache(cache, addr);
    if (hit_blk != -1) {
        lru_update_blk(cache, set, hit_blk);
        return true;
    }

    replaced_blk = get_invalid_block(cache, set);

    if (replaced_blk == -1) {
        replaced_blk = get_replaced_block(cache, set);
        *replaced_blk_addr = cache->sets[set].blocks[replaced_blk].tag | (set << cache->blksize_shift);
    }

    lru_update_blk(cache, set, replaced_blk);

    cache->sets[set].blocks[replaced_blk].tag = tag;
    cache->sets[set].blocks[replaced_blk].valid = true;
 
    return false;
}

static bool evict_l1_after_l2(Cache *cache, uint64_t replaced_blk_addr)
{
    uint64_t addr = replaced_blk_addr;
    int hit_blk;
    uint64_t set;

    set = extract_set(cache, addr);

    hit_blk = in_cache(cache, addr);
    if (hit_blk != -1) {
        cache->sets[set].blocks[hit_blk].valid = false;
        return true;
    }
    return false;
}


static void mem_access(unsigned int vcpu_index, uint64_t addr, bool is_user)
{
    int cache_idx;
    bool hit_in_l1 = false, hit_in_l2 = false;

    cache_idx = vcpu_index % cores;
    hit_in_l1 = access_cache(l1_dcaches[cache_idx], addr);
    if (!hit_in_l1) {
        l1_dcaches[cache_idx]->misses++;
        if(is_user) {
            l1_dcaches[cache_idx]->misses_user++;
        } else {
            l1_dcaches[cache_idx]->misses_kernel++;
        }
    }
    l1_dcaches[cache_idx]->access++;
    if(is_user) {
        l1_dcaches[cache_idx]->access_user++;
    } else {
        l1_dcaches[cache_idx]->access_kernel++;
    }
 
    if (hit_in_l1) {
        /* No need to access L2 */
        return;
    }

    uint64_t replaced_blk_addr = -1;
    hit_in_l2 = inclusive_access_cache(l2_ucaches[cache_idx], addr, &replaced_blk_addr);
    if (!hit_in_l2) {
        l2_ucaches[cache_idx]->misses++;
        l2_ucaches[cache_idx]->l2_dmisses++;

        if(is_user) {
            l2_ucaches[cache_idx]->l2_dmisses_user++;
        } else {
            l2_ucaches[cache_idx]->l2_dmisses_kernel++;
        }

        evict_l1_after_l2(l1_dcaches[cache_idx], replaced_blk_addr);
        evict_l1_after_l2(l1_icaches[cache_idx], replaced_blk_addr);
    }
    l2_ucaches[cache_idx]->access++;
    l2_ucaches[cache_idx]->l2_daccess++;
    if(is_user) {
        l2_ucaches[cache_idx]->l2_daccess_user++;
    } else {
        l2_ucaches[cache_idx]->l2_daccess_kernel++;
    }
}

static void insn_access(unsigned int vcpu_index, uint64_t insn_addr, bool is_user)
{
    if((iaccess % 1000000000) == 0) {
        log_stats();
    }
    iaccess++;

    int cache_idx;
    bool hit_in_l1 = false, hit_in_l2 = false;

    cache_idx = vcpu_index % cores;
    hit_in_l1 = access_cache(l1_icaches[cache_idx], insn_addr);
    if (!hit_in_l1) {
        l1_icaches[cache_idx]->misses++;
        if(is_user) {
            l1_icaches[cache_idx]->misses_user++;
        } else {
            l1_icaches[cache_idx]->misses_kernel++;
        }
    }
    l1_icaches[cache_idx]->access++;
    if(is_user) {
        l1_icaches[cache_idx]->access_user++;
    } else {
        l1_icaches[cache_idx]->access_kernel++;
    }
 
    if (hit_in_l1) {
        /* No need to access L2 */
        return;
    }

    uint64_t replaced_blk_addr = -1;
    hit_in_l2 = inclusive_access_cache(l2_ucaches[cache_idx], insn_addr, &replaced_blk_addr);
    if (!hit_in_l2) {
        l2_ucaches[cache_idx]->misses++;
        l2_ucaches[cache_idx]->l2_imisses++;
        if(is_user) {
            l2_ucaches[cache_idx]->l2_imisses_user++;
        } else {
            l2_ucaches[cache_idx]->l2_imisses_kernel++;
        }

        evict_l1_after_l2(l1_dcaches[cache_idx], replaced_blk_addr);
        evict_l1_after_l2(l1_icaches[cache_idx], replaced_blk_addr);
    }
    l2_ucaches[cache_idx]->access++;
    l2_ucaches[cache_idx]->l2_iaccess++;
    if(is_user) {
        l2_ucaches[cache_idx]->l2_iaccess_user++;
    } else {
        l2_ucaches[cache_idx]->l2_iaccess_kernel++;
    }
}

static void cache_free(Cache *cache)
{
    for (int i = 0; i < cache->num_sets; i++) {
        g_free(cache->sets[i].blocks);
    }

    lru_priorities_destroy(cache);

    g_free(cache->sets);
    g_free(cache);
}

static void caches_free(Cache **caches)
{
    int i;

    for (i = 0; i < cores; i++) {
        cache_free(caches[i]);
    }
}

static void append_stats_line(GString *line, 
                              uint64_t l1_daccess, uint64_t l1_dmisses, 
                              uint64_t l1_iaccess, uint64_t l1_imisses, 
                              uint64_t l1_daccess_user, uint64_t l1_dmisses_user,
                              uint64_t l1_iaccess_user, uint64_t l1_imisses_user,
                              uint64_t l1_daccess_kernel, uint64_t l1_dmisses_kernel, 
                              uint64_t l1_iaccess_kernel, uint64_t l1_imisses_kernel, 
 
                              uint64_t l2_dmisses, uint64_t l2_daccess,
                              uint64_t l2_imisses, uint64_t l2_iaccess,
                              uint64_t l2_dmisses_user, uint64_t l2_daccess_user,
                              uint64_t l2_imisses_user, uint64_t l2_iaccess_user,
                              uint64_t l2_dmisses_kernel, uint64_t l2_daccess_kernel,
                              uint64_t l2_imisses_kernel, uint64_t l2_iaccess_kernel,

                              uint64_t l2_access,  uint64_t l2_misses)
{
    double l1_dmiss_rate, l1_imiss_rate, l2_miss_rate;
    double l2_dmiss_rate, l2_imiss_rate;
    double l2_dmiss_rate_user, l2_imiss_rate_user;
    double l2_dmiss_rate_kernel, l2_imiss_rate_kernel;

    l1_dmiss_rate = ((double) l1_dmisses) / (l1_daccess) * 100.0;
    l1_imiss_rate = ((double) l1_imisses) / (l1_iaccess) * 100.0;

    g_string_append_printf(line, "%-14lu %-12lu %9.4lf%%  %-14lu %-12lu %9.4lf%%",
                           l1_daccess,
                           l1_dmisses,
                           l1_dmiss_rate,
                           l1_iaccess,
                           l1_imisses,
                           l1_imiss_rate);

    l2_dmiss_rate = ((double) l2_dmisses) / (l2_daccess) * 100.0;
    l2_imiss_rate = ((double) l2_imisses) / (l2_iaccess) * 100.0;
    l2_miss_rate =  ((double) l2_misses) / (l2_access) * 100.0;
    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_daccess,
                           l2_dmisses,
                           l2_dmiss_rate);
    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_iaccess,
                           l2_imisses,
                           l2_imiss_rate);
    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_access,
                           l2_misses,
                           l2_miss_rate);
    g_string_append(line, "\n");

    g_string_append(line, " kernel");


    double l1_dmiss_rate_kernel = ((double) l1_dmisses_kernel) / (l1_daccess_kernel) * 100.0;
    double l1_imiss_rate_kernel = ((double) l1_imisses_kernel) / (l1_iaccess_kernel) * 100.0;

    g_string_append_printf(line, "%-14lu. %-12lu, %9.4lf%%,  %-14lu, %-12lu, %9.4lf%%,",
                           l1_daccess_kernel,
                           l1_dmisses_kernel,
                           l1_dmiss_rate_kernel,
                           l1_iaccess_kernel,
                           l1_imisses_kernel,
                           l1_imiss_rate_kernel);


    uint64_t l2_access_user = l2_daccess_user + l2_iaccess_user;
    uint64_t l2_access_kernel = l2_daccess_kernel + l2_iaccess_kernel;
    uint64_t l2_misses_user = l2_dmisses_user + l2_imisses_user;
    uint64_t l2_misses_kernel = l2_dmisses_kernel + l2_imisses_kernel;
    l2_dmiss_rate_kernel = ((double) l2_dmisses_kernel) / (l2_daccess_kernel) * 100.0;
    l2_imiss_rate_kernel = ((double) l2_imisses_kernel) / (l2_iaccess_kernel) * 100.0;
    l2_dmiss_rate_user = ((double) l2_dmisses_user) / (l2_daccess_user) * 100.0;
    l2_imiss_rate_user = ((double) l2_imisses_user) / (l2_iaccess_user) * 100.0;
    double l2_miss_rate_kernel =  ((double) l2_misses_kernel) / (l2_access_kernel) * 100.0;
    double l2_miss_rate_user =  ((double) l2_misses_user) / (l2_access_user) * 100.0;

    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_daccess_kernel,
                           l2_dmisses_kernel,
                           l2_dmiss_rate_kernel);
    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_iaccess_kernel,
                           l2_imisses_kernel,
                           l2_imiss_rate_kernel);
    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_access_kernel,
                           l2_misses_kernel,
                           l2_miss_rate_kernel);

    g_string_append(line, "\n");

    g_string_append(line, " user   ");

    double l1_dmiss_rate_user = ((double) l1_dmisses_user) / (l1_daccess_user) * 100.0;
    double l1_imiss_rate_user = ((double) l1_imisses_user) / (l1_iaccess_user) * 100.0;

    g_string_append_printf(line, "%-14lu %-12lu %9.4lf%%  %-14lu %-12lu"
                           " %9.4lf%%",
                           l1_daccess_user,
                           l1_dmisses_user,
                           l1_dmiss_rate_user,
                           l1_iaccess_user,
                           l1_imisses_user,
                           l1_imiss_rate_user);


    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_daccess_user,
                           l2_dmisses_user,
                           l2_dmiss_rate_user);
    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_iaccess_user,
                           l2_imisses_user,
                           l2_imiss_rate_user);
    g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                           l2_access_user,
                           l2_misses_user,
                           l2_miss_rate_user);

    g_string_append(line, "\n");


}

static void sum_stats(void)
{
    int i;
    l1_imisses = 0;
    l1_dmisses = 0;
    l1_iaccess = 0;
    l1_daccess = 0;

    l1_imisses_user = 0;
    l1_dmisses_user = 0;
    l1_iaccess_user = 0;
    l1_daccess_user = 0;

    l1_imisses_kernel = 0;
    l1_dmisses_kernel = 0;
    l1_iaccess_kernel = 0;
    l1_daccess_kernel = 0;


    l2_misses = 0;
    l2_mem_access = 0;
    l2_imisses = 0;
    l2_dmisses = 0;
    l2_iaccess = 0;
    l2_daccess = 0;

    l2_imisses_user = 0;
    l2_dmisses_user = 0;
    l2_iaccess_user = 0;
    l2_daccess_user = 0;
    
    l2_imisses_kernel = 0;
    l2_dmisses_kernel = 0;
    l2_iaccess_kernel = 0;
    l2_daccess_kernel = 0;
 
    g_assert(cores > 1);
    for (i = 0; i < cores; i++) {
        l1_imisses += l1_icaches[i]->misses;
        l1_dmisses += l1_dcaches[i]->misses;
        l1_iaccess += l1_icaches[i]->access;
        l1_daccess += l1_dcaches[i]->access;

        l1_imisses_user += l1_icaches[i]->misses_user;
        l1_dmisses_user += l1_dcaches[i]->misses_user;
        l1_iaccess_user += l1_icaches[i]->access_user;
        l1_daccess_user += l1_dcaches[i]->access_user;

        l1_imisses_kernel += l1_icaches[i]->misses_kernel;
        l1_dmisses_kernel += l1_dcaches[i]->misses_kernel;
        l1_iaccess_kernel += l1_icaches[i]->access_kernel;
        l1_daccess_kernel += l1_dcaches[i]->access_kernel;


        l2_misses += l2_ucaches[i]->misses;
        l2_mem_access += l2_ucaches[i]->access;
        l2_imisses += l2_ucaches[i]->l2_imisses;
        l2_dmisses += l2_ucaches[i]->l2_dmisses;
        l2_iaccess += l2_ucaches[i]->l2_iaccess;
        l2_daccess += l2_ucaches[i]->l2_daccess;

        l2_imisses_user += l2_ucaches[i]->l2_imisses_user;
        l2_dmisses_user += l2_ucaches[i]->l2_dmisses_user;
        l2_iaccess_user += l2_ucaches[i]->l2_iaccess_user;
        l2_daccess_user += l2_ucaches[i]->l2_daccess_user;
                  
        l2_imisses_kernel += l2_ucaches[i]->l2_imisses_kernel;
        l2_dmisses_kernel += l2_ucaches[i]->l2_dmisses_kernel;
        l2_iaccess_kernel += l2_ucaches[i]->l2_iaccess_kernel;
        l2_daccess_kernel += l2_ucaches[i]->l2_daccess_kernel;
    }
}

static void log_stats(void)
{
    int i;
    Cache *icache, *dcache, *l2_cache;

    g_autoptr(GString) rep = g_string_new("core #, data access, data misses,"
                                          " dmiss rate, insn access,"
                                          " insn misses, imiss rate");

    g_string_append(rep, ", l2 daccess, l2 dmisses, l2 dmiss rate"
                         ", l2 iaccess, l2 imisses, l2 imiss rate"
                         ", l2 access, l2 misses, l2 miss rate");

    g_string_append(rep, "\n");
    g_string_append(rep, 
                         ", l1 daccess user, l1 dmisses user, l1 dmiss rate user"
                         ", l1 iaccess user, l1 imisses user, l1 imiss rate user"
                         ", l1 access user, l1 misses user, l1 miss rate user"
                         ", l2 daccess user, l2 dmisses user, l2 dmiss rate user"
                         ", l2 iaccess user, l2 imisses user, l2 imiss rate user"
                         ", l2 access user, l2 misses user, l2 miss rate user");
    g_string_append(rep, 
                         ", l1 daccess user, l1 dmisses user, l1 dmiss rate user"
                         ", l1 iaccess user, l1 imisses user, l1 imiss rate user"
                         ", l1 access user, l1 misses user, l1 miss rate user"
                         ", l2 daccess kernel, l2 dmisses kernel, l2 dmiss rate kernel"
                         ", l2 iaccess kernel, l2 imisses kernel, l2 imiss rate kernel"
                         ", l2 access kernel, l2 misses kernel, l2 miss rate kernel");
    g_string_append(rep, "\n");



    for (i = 0; i < cores; i++) {
        g_string_append_printf(rep, "%-8d", i);
        dcache = l1_dcaches[i];
        icache = l1_icaches[i];
        l2_cache = l2_ucaches[i];
        append_stats_line(rep, 
                dcache->access, dcache->misses,
                icache->access, icache->misses,
                dcache->access_user, dcache->misses_user,
                icache->access_user, icache->misses_user,
                dcache->access_kernel, dcache->misses_kernel,
                icache->access_kernel, icache->misses_kernel,
 
                l2_cache->l2_dmisses, l2_cache->l2_daccess,
                l2_cache->l2_imisses, l2_cache->l2_iaccess,
                l2_cache->l2_dmisses_user, l2_cache->l2_daccess_user,
                l2_cache->l2_imisses_user, l2_cache->l2_iaccess_user,
                l2_cache->l2_dmisses_kernel, l2_cache->l2_daccess_kernel,
                l2_cache->l2_imisses_kernel, l2_cache->l2_iaccess_kernel,
                l2_cache->access, l2_cache->misses);
    }

    if (cores > 1) {
        sum_stats();
        g_string_append_printf(rep, "%-8s", "sum");
        append_stats_line(rep, 
                l1_daccess, l1_dmisses,
                l1_iaccess, l1_imisses,
                l1_daccess_user, l1_dmisses_user,
                l1_iaccess_user, l1_imisses_user,
                l1_daccess_kernel, l1_dmisses_kernel,
                l1_iaccess_kernel, l1_imisses_kernel,
 
                l2_dmisses, l2_daccess,
                l2_imisses, l2_iaccess,
                l2_dmisses_user, l2_daccess_user,
                l2_imisses_user, l2_iaccess_user,
                l2_dmisses_kernel, l2_daccess_kernel,
                l2_imisses_kernel, l2_iaccess_kernel,
                l2_mem_access, l2_misses);
    }

    g_string_append(rep, "\n");
//    qemu_plugin_outs(rep->str);
    fprintf(logfile, "%s", rep->str);
    fflush(logfile);
}

static void sim_clearup(void)
{
    log_stats();

    caches_free(l1_dcaches);
    caches_free(l1_icaches);
    caches_free(l2_ucaches);
    fclose(logfile);
}

void *sim_init(void *config_)
{
    CacheConfig config = *((CacheConfig *) config_);
    FifoQueue *queue = config.queue;

    int l1_dassoc = config.l1_dassoc;
    int l1_dblksize = config.l1_dblksize;
    int l1_dcachesize = config.l1_dcachesize;

    int l1_iassoc = config.l1_iassoc;
    int l1_iblksize = config.l1_iblksize;
    int l1_icachesize = config.l1_icachesize;

    int l2_assoc = config.l2_assoc;
    int l2_blksize = config.l2_blksize;
    int l2_cachesize = config.l2_cachesize;

    cores = config.cores;

    l1_dcaches = caches_init(l1_dblksize, l1_dassoc, l1_dcachesize);
    if (!l1_dcaches) {
        const char *err = cache_config_error(l1_dblksize, l1_dassoc, l1_dcachesize);
        fprintf(stderr, "dcache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return NULL;
    }

    l1_icaches = caches_init(l1_iblksize, l1_iassoc, l1_icachesize);
    if (!l1_icaches) {
        const char *err = cache_config_error(l1_iblksize, l1_iassoc, l1_icachesize);
        fprintf(stderr, "icache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return NULL;
    }

    l2_ucaches = caches_init(l2_blksize, l2_assoc, l2_cachesize);
    if (!l2_ucaches) {
        const char *err = cache_config_error(l2_blksize, l2_assoc, l2_cachesize);
        fprintf(stderr, "L2 cache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return NULL;
    }

    g_autoptr(GString) rep = g_string_new("cache-results-");
    g_string_append_printf(rep, "%04dKB", l2_cachesize/1024);
    logfile = fopen(rep->str, "w");
    assert(logfile);

    AccessReq req;
    while(1) {
        if (queue_can_pop(queue)) {
            req = queue_pop(queue);
            if(req.type == INSN_FETCH) {
                insn_access(req.vcpu_idx, req.addr, req.is_user);
            } else {
                mem_access(req.vcpu_idx, req.addr, req.is_user);
            }
        }
    }
    sim_clearup();
    return NULL;
}

void queue_init(FifoQueue *queue, size_t size) {
    queue->size = size;
    queue->enq_ptr = 0;
    queue->deq_ptr = 0;
    queue->fifo = calloc(size, sizeof(AccessReq));
}

bool queue_can_push(FifoQueue *queue) {
    return (queue->enq_ptr - queue->deq_ptr) < queue->size;
}

bool queue_can_pop(FifoQueue *queue) {
    return (queue->enq_ptr - queue->deq_ptr) > 1;
}

void queue_push(FifoQueue *queue, AccessReq req) {
    assert(queue_can_push(queue));
    size_t index = queue->enq_ptr % queue->size;
    queue->fifo[index] = req;
    queue->enq_ptr++;
}

AccessReq queue_pop(FifoQueue *queue) {
    assert(queue_can_pop(queue));
    size_t index = queue->deq_ptr % queue->size;
    AccessReq value = queue->fifo[index];
    queue->deq_ptr++;
    return value;
}