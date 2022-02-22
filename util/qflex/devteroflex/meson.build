devteroflex_ss.add([files(
  'devteroflex.pb-c.c',
  'file-ops.c',
  'demand-paging.c',
  'inverted-page-table.c',
  'temporal-page-table.c',
  'physical-addr-manager.c',
  'verification.c',
  'mmu.c',
  'custom-instrumentation.c',
  'devteroflex.c',
  'emulation-logger.c',
 ), devteroflex])

if 'AWS_FPGA' in config_host
  aws_ss = ss.source_set()
  devteroflex_ss.add_all(aws_ss) 
  subdir('aws')
else
  simulation_ss = ss.source_set()
  subdir('simulation')
  devteroflex_ss.add_all(simulation_ss) 
endif
