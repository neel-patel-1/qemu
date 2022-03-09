#include "qflex/qflex-log.h"

int qflex_loglevel = 0;

static const QEMULogItem qflex_log_items[] = {
    { QFLEX_LOG_GENERAL, "gen",
      "show general QFLEX actions" },
    { QFLEX_LOG_MAGIC_INST, "magic",
      "log when magic instructions are hit" },
    { QFLEX_LOG_MEM_TRACE, "mem-trace",
      "log every memory instruction helper executed" },
    { QFLEX_LOG_TB_EXEC, "tb",
      "log every translation block executed" },
    { QFLEX_LOG_LDST, "ldst",
       "log every memory translation block executed" },
    { QFLEX_LOG_FILE_ACCESS, "file",
      "log qflex related file access" },
    { QFLEX_LOG_MEM_TRACE_INST, "mem-trace-inst",
      "log instruction traces" },
    { 0, NULL, NULL },
};

/* takes a comma separated list of log masks. Return 0 if error. */
int qflex_str_to_log_mask(const char *str)
{
    const QEMULogItem *item;
    int mask = 0;
    char **parts = g_strsplit(str, ",", 0);
    char **tmp;

    for (tmp = parts; tmp && *tmp; tmp++) {
        if (g_str_equal(*tmp, "all")) {
            for (item = qflex_log_items; item->mask != 0; item++) {
                mask |= item->mask;
            }
        } else {
            for (item = qflex_log_items; item->mask != 0; item++) {
                if (g_str_equal(*tmp, item->name)) {
                    goto found;
                }
            }
            goto error;
        found:
            mask |= item->mask;
        }
    }

    g_strfreev(parts);
    return mask;

 error:
    g_strfreev(parts);
    return 0;
}

void qflex_print_log_usage(const char *str, FILE *f)
{
    const QEMULogItem *item;
    fprintf(f, "Error QFLEX logging passed as argument: %s\n", str);
    fprintf(f, "Log items options (comma separated):\n");
    for (item = qflex_log_items; item->mask != 0; item++) {
        fprintf(f, "%-15s %s\n", item->name, item->help);
    }
}
