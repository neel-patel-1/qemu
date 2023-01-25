#ifndef QFLEX_CONFIG_H
#define QFLEX_CONFIG_H

#include "qemu/osdep.h"

extern QemuOptsList qflex_flexus_opts;
extern QemuOptsList qflex_phases_opts;
extern QemuOptsList qflex_ckpt_opts;

#ifdef CONFIG_DEVTEROFLEX
extern QemuOptsList qemu_devteroflex_opts;
#endif
 
int qflex_parse_opts(int index, const char *optarg, Error **errp);

#endif