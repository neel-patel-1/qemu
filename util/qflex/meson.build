qflex_ss.add([files(
  'qflex-log.c',
  'qflex.c',
  'qflex-config.c',
  'qflex-hmp.c',
  'qflex-traces.c'
), qflex])

if 'CONFIG_DEVTEROFLEX' in config_host
  devteroflex_ss = ss.source_set()
  subdir('devteroflex')
  qflex_ss.add_all(devteroflex_ss)
endif 
 