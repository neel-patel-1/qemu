#ifndef QFLEX_HMP_H
#define QFLEX_HMP_H

void hmp_qflex_log(Monitor *mon, const QDict *qdict);

void hmp_qflex_singlestep_start(Monitor *mon, const QDict *qdict);
void hmp_qflex_singlestep_stop(Monitor *mon, const QDict *qdict);

void hmp_qflex_mem_trace_start(Monitor *mon, const QDict *qdict);
void hmp_qflex_mem_trace_stop(Monitor *mon, const QDict *qdict);
void hmp_qflex_mem_trace_end(Monitor *mon, const QDict *qdict);
void hmp_qflex_mem_trace_log_stats(Monitor *mon, const QDict *qdict);

#ifdef CONFIG_DEVTEROFLEX
void hmp_devteroflex_start(Monitor *mon, const QDict *qdict);
void hmp_devteroflex_gen_verification_start(Monitor *mon, const QDict *qdict);
void hmp_devteroflex_gen_example(Monitor *mon, const QDict *qdict);
#endif

#endif
