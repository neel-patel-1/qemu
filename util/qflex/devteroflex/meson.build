devteroflex_ss.add([files(
  'file-ops.c',
  'demand-paging.c',
  'mmu.c',
  'custom-instrumentation.c',
  'devteroflex.c',
  'emulation-logger.c',
 ), devteroflex])

fpga_ss = ss.source_set()
subdir('fpga')
devteroflex_ss.add_all(fpga_ss) 

add_languages('rust')

librust_aux = static_library('rust_aux', 'rust_aux.rs', rust_crate_type: 'staticlib', install: true)
rust_aux = declare_dependency(link_with: librust_aux)
devteroflex_ss.add(rust_aux)
