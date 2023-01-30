#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/config-file.h"
#include "qemu-options.h"
#include "qemu/main-loop.h"
#include "sysemu/sysemu.h"

#include "qflex/qflex.h"
#include "qflex/qflex-config.h"

#ifdef CONFIG_EXTSNAP
bool exton = false;
void configure_extsnap(bool enable) {
    exton = enable;
}
#endif

#if defined(CONFIG_QUANTUM) || defined(CONFIG_FLEXUS) || defined(CONFIG_EXTSNAP)
#define KIL 1E3
#define MIL 1E6
#define BIL 1E9
void processLetterforExponent(uint64_t *val, char c, Error **errp)
{
    switch(c) {
        case 'K': case 'k' :
        *val *= KIL;
        break;
        case 'M':case 'm':
        *val  *= MIL;
        break;
        case 'B':case 'b':
        *val  *= BIL;
        break;
        default:
        error_setg(errp, "the suffix you used is not valid: valid suffixes are K,k,M,m,B,b");
        exit(1);
        break;
    }
}

void processForOpts(uint64_t *val, const char* qopt, Error **errp)
{
    size_t s = strlen(qopt);
    char c = qopt[s-1];

    if (isalpha(c)) {
        char* temp= strndup(qopt,  strlen(qopt)-1);
        *val = atoi(temp);
        free(temp);
        if (*val <= 0){
            *val = 0;
            return;
        }

        processLetterforExponent(&(*val), c, errp);
    } else {
        *val = atoi(qopt);
    }
}
#endif

#ifdef CONFIG_FLEXUS
QemuOptsList qflex_flexus_opts = {
    .name = "flexus",
    .implied_opt_name = "mode",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qflex_flexus_opts.head),
    .desc = {
        {
            .name = "mode",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "length",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "simulator",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "config",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "debug",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "nb_cores",
            .type = QEMU_OPT_NUMBER,
        },
        { /* end of list */ }
    },
};

void configure_flexus(QemuOpts *opts, Error **errp)
{
    const char* mode_opt, *sim_dynlib_path, *config_opt, *debug_opt;
    mode_opt = qemu_opt_get(opts, "mode");
    uint64_t sim_cycles = qemu_opt_get_number(opts, "length", 0);
    sim_dynlib_path = qemu_opt_get(opts, "simulator");
    config_opt = qemu_opt_get(opts, "config");
    debug_opt = qemu_opt_get(opts, "debug");
    int nb_cores = qemu_opt_get_number(opts, "nb_cores", smp_cpus);

    // Parse MANDATORY simulation length, simulator, and config options
    if (!sim_dynlib_path || !config_opt || !mode_opt) {
        error_setg(errp, "ERROR: Not all flexus options need to be defined. Mandatory: mode=,length=,simulator=,config=");
    }

    // Set trace or timing
    flexus_state.mode = strcmp(mode_opt, "timing") == 0 ? TIMING : TRACE;
    if (sim_cycles == 0) {
        error_setg(errp, "Refusing to run with ZERO simulation length.");
    }
    
    // Load simulator dynamic library
    flexus_state.dynlib_path = strdup(sim_dynlib_path);
    if ( access( sim_dynlib_path, F_OK ) != -1 ) {
        bool success = flexus_dynlib_load( sim_dynlib_path );
        if (success){
            fprintf(stderr, "<%s:%i> Flexus Simulator set!.\n", basename(__FILE__), __LINE__);
        } else {
            error_setg(errp, "simulator could not be set.!.\n");
        }
    } else {
        error_setg(errp, "simulator path contains no simulator!.\n");
    }

    flexus_state.config_file = strdup(config_opt);
    if(! (access( flexus_state.config_file, F_OK ) != -1) ) {
        error_setg(errp, "no config file (user_postload) at this path %s\n", config_opt);
    }

    if (debug_opt) {
        flexus_state.debug_mode = strdup(debug_opt);
    }

    flexus_state.nb_cores = nb_cores;

    // Init QFlex values
    QFLEX_API_Interface_Hooks_t hooks;
    qflex_api_init((flexus_state.mode == TIMING) ? true : false, sim_cycles);
    QFLEX_API_get_Interface_Hooks(&hooks);
    flexus_dynlib_fns.qflex_sim_init(&hooks, flexus_state.nb_cores, flexus_state.config_file);

    // If ext-snap dir is set
    if (flexus_state.load_dir) {
        flexus_doLoad(flexus_state.load_dir, NULL);
    }
    // If debug flag is set
    if (flexus_state.debug_mode) {
        flexus_setDebug(flexus_state.debug_mode, NULL);
    }
}

void set_flexus_snap_dir(const char* dir_name) {
    DIR* dir = opendir(dir_name);
    if (dir) {
        /* Directory exists. */
        flexus_state.load_dir = strdup(dir_name);
        closedir(dir);
    } else if (ENOENT == errno) {
        /* Directory does not exist. */
    } else {
        /* opendir() failed for some other reason. */
    }
}
#endif /* CONFIG_FLEXUS */

#ifdef CONFIG_EXTSNAP
QemuOptsList qflex_phases_opts = {
    .name = "phases",
    .implied_opt_name = "steps",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qflex_phases_opts.head),
    .desc = {
        {
            .name = "steps",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "name",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

QemuOptsList qflex_ckpt_opts = {
    .name = "ckpt",
    .implied_opt_name = "every",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qflex_ckpt_opts.head),
    .desc = {
        {
            .name = "every",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "end",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};
char* phases_prefix;
 

void configure_phases(QemuOpts *opts, Error **errp) {
    const char* step_opt, *name_opt;
    step_opt = qemu_opt_get(opts, "steps");
    name_opt = qemu_opt_get(opts, "name");

    int id = 0;
    if (!step_opt) {
        error_setg(errp, "no distances for phases defined");
    }
    if (!name_opt) {
        fprintf(stderr, "no naming prefix  given for phases option. will use prefix phase_00X");
        phases_prefix = strdup("phase");
    } else {
        phases_prefix = strdup(name_opt);
    }

    phases_state_t * head = calloc(1, sizeof(phases_state_t));

    char* token = strtok((char*) step_opt, ":");
    processForOpts(&head->val, token, errp);
    head->id = id++;

    extsnap_insert_phases(head);
    //QLIST_INSERT_HEAD(&phases_head, head, next);
    while (token) {
        token = strtok(NULL, ":");

        if (token) {
            phases_state_t* phase = calloc(1, sizeof(phases_state_t));
            processForOpts(&phase->val, token, errp);
            phase->id= id++;
            QLIST_INSERT_AFTER(head, phase, next);
            head = phase;
        }
    }

}

void configure_ckpt(QemuOpts *opts, Error **errp) {
    const char* every_opt, *end_opt;
    every_opt = qemu_opt_get(opts, "every");
    end_opt = qemu_opt_get(opts, "end");

    if (!every_opt) {
        error_setg(errp, "no interval given for ckpt option. cant continue");
    }
    if (!end_opt) {
        error_setg(errp, "no end given for ckpt option. cant continue");
    }

    processForOpts(&ckpt_state.ckpt_interval, every_opt, errp);
    processForOpts(&ckpt_state.ckpt_end, end_opt, errp);

    if (ckpt_state.ckpt_end < ckpt_state.ckpt_interval) {
        error_setg(errp, "ckpt end cant be smaller than ckpt interval");
    }
}
#endif