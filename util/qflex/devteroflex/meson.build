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

fpga_ss = ss.source_set()
subdir('fpga')
devteroflex_ss.add_all(fpga_ss) 
