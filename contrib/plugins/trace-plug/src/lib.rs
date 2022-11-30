#![allow(non_camel_case_types)]
#![allow(unused_variables)]
#![allow(unused_imports)]
use breakdown::{get_capstone, BreakdownData};
use capstone::Capstone;
use core::slice;
use lazy_static::lazy_static;
use log::LevelFilter;
use log4rs::append::file::FileAppender;
use log4rs::config::{Appender, Config, Root};
use log4rs::encode::pattern::PatternEncoder;
use once_cell::sync::OnceCell;
use qemu_plugin::*;
use std::collections::{HashMap, HashSet};
use std::ffi::{self, c_void, CStr, CString};
use std::fs::File;
use std::io::{BufWriter, Write};
use std::iter::Once;
use std::ops::Deref;
use std::sync::mpsc::{sync_channel, SyncSender};
use std::sync::{Arc, Mutex, RwLock};
use std::thread;
#[warn(non_snake_case)]
use std::time::Duration;
use std::{mem, ptr};

use crate::breakdown::BreakdownCategories;

mod breakdown;
mod qemu_plugin;

// Bypassing Rsut Checker will illegal hacks for TransBlockContext and Capstone
struct TransBlockContext {
    pub count: Vec<usize>,
    pub breaks: Vec<BreakdownData>,
}
struct TransBlockContextPtr {
    pub d: *mut TransBlockContext,
}
impl TransBlockContextPtr {
    fn new(o: TransBlockContext) -> Self {
        return TransBlockContextPtr {
            d: Box::into_raw(Box::new(o)),
        };
    }
    pub fn get(&self) -> *mut TransBlockContext {
        return self.d;
    }
}

impl Deref for TransBlockContextPtr {
    type Target = TransBlockContext;

    fn deref(&self) -> &Self::Target {
        return unsafe { &*self.d };
    }
}
unsafe impl Send for TransBlockContextPtr {}
unsafe impl Sync for TransBlockContextPtr {}

#[derive(Debug)]
struct CapstonePtr {
    pub value: *mut Capstone,
}
impl CapstonePtr {
    fn new(o: Capstone) -> Self {
        return CapstonePtr {
            value: Box::into_raw(Box::new(o)),
        };
    }
    pub fn get(&self) -> *mut Capstone {
        return self.value;
    }
}
unsafe impl Send for CapstonePtr {}
unsafe impl Sync for CapstonePtr {}

// List of global variables
lazy_static! {
    static ref TB_HASHMAP: RwLock<HashMap<u64, TransBlockContextPtr>> = RwLock::new(HashMap::new());
    static ref CATEGORIES_BREAKDOWNS: RwLock<Vec<BreakdownCategories>> = RwLock::new(Vec::new());
}

static N_CORES: OnceCell<usize> = OnceCell::new();
static ARCH: OnceCell<String> = OnceCell::new();
static CS: OnceCell<CapstonePtr> = OnceCell::new();

pub struct LogDumper {
    thid: usize,
    writer: BufWriter<File>,
    logs: u32,
}

impl LogDumper {
    pub fn new(thid: usize, logfile_path_name: &String) -> Self {
        let f = File::create(logfile_path_name).expect("Unable to open file");
        let writer = BufWriter::new(f);
        Self {
            thid,
            writer,
            logs: 0,
        }
    }

    pub fn dump(&mut self) {
        self.logs += 1;
        let curr_ite = &format!("Iteration:{}", self.logs);
        writeln!(self.writer, "{}", curr_ite).unwrap();
        println!("{}", curr_ite);

        let context_map = TB_HASHMAP.read().unwrap();
        let mut breakdowns = BreakdownCategories::new();
        for (haddr, ptr) in context_map.iter() {
            let tb_ctx = unsafe { &*ptr.get() };
            let count = tb_ctx.count.get(self.thid).unwrap();
            for data in &tb_ctx.breaks {
                breakdowns.update_breakdown(data, *count);
            }
        }
        drop(context_map);

        let log = breakdowns.get_log_stats(true);
        writeln!(self.writer, "{}", log).unwrap();

        self.writer.flush().unwrap();
    }

    pub fn launch_dumpers(cores_to_trace: Vec<usize>, timer_ms: u64, logfile_path_base: String) {
        println!("Launching dumpers");
        for thid in cores_to_trace {
            let logfile_path = logfile_path_base.clone() + &thid.to_string();
            let mut worker = LogDumper::new(thid, &logfile_path);
            println!("Dumper for core[{}] to file[{}]", thid, &logfile_path);
            thread::spawn(move || loop {
                thread::sleep(Duration::from_millis(timer_ms));
                worker.dump();
            });
        }
    }
}

#[no_mangle]
pub static qemu_plugin_version: u32 = QEMU_PLUGIN_VERSION;

#[no_mangle]
unsafe extern "C" fn vcpu_tb_exec(vcpu_index: u32, udata_raw_ptr: *mut ffi::c_void) {
    let count = udata_raw_ptr as *mut usize;
    let ptr = slice::from_raw_parts_mut(count, *N_CORES.get().unwrap());
    ptr[vcpu_index as usize] += 1;
}

#[no_mangle]
unsafe extern "C" fn vcpu_tb_trans(
    id: qemu_plugin::qemu_plugin_id_t,
    tb: *mut qemu_plugin::qemu_plugin_tb,
) {
    let n_inst = qemu_plugin_tb_n_insns(tb);
    let base_insn = qemu_plugin_tb_get_insn(tb, 0);
    let hva = qemu_plugin_insn_haddr(base_insn) as u64;
    let is_user = qemu_plugin_is_userland(base_insn);

    let context_map = TB_HASHMAP.read().unwrap();
    if !context_map.contains_key(&hva) {
        drop(context_map);
        let mut total_bytes = 0;
        for idx in 0..n_inst {
            let insn = qemu_plugin_tb_get_insn(tb, idx);
            let insn_size = qemu_plugin_insn_size(insn) as usize;
            total_bytes += insn_size;
        }

        let insn_ptr = hva as *const u8;
        let insn_bytes: &[u8] = slice::from_raw_parts(insn_ptr, total_bytes);
        let arch = ARCH.get_unchecked();
        let capstone = CS.get_unchecked().get();

        let breaks = breakdown::execute(arch, &*capstone, insn_bytes, is_user);

        let count = vec![0usize; *N_CORES.get().unwrap()];
        let tb_ctx = TransBlockContextPtr::new(TransBlockContext { count, breaks });

        let mut context_map = TB_HASHMAP.write().unwrap();
        context_map.insert(hva, tb_ctx);
    }

    let context_map = TB_HASHMAP.read().unwrap();
    let metadata = context_map.get(&hva).unwrap().get();

    let count_ptr = (*metadata).count.as_mut_ptr() as *mut c_void;

    qemu_plugin_register_vcpu_tb_exec_cb(
        tb,
        Some(vcpu_tb_exec),
        qemu_plugin_cb_flags_QEMU_PLUGIN_CB_NO_REGS,
        count_ptr,
    );
}

#[no_mangle]
unsafe extern "C" fn plugin_exit(id: qemu_plugin::qemu_plugin_id_t, p: *mut ffi::c_void) {
    // release the memory in the map.
    let context_map = TB_HASHMAP.write().unwrap();

    for (k, v) in context_map.iter() {
        // this will release all the blocks in the context map.
        drop(Box::from_raw(v.d));
    }
}

#[no_mangle]
unsafe extern "C" fn qemu_plugin_install(
    id: qemu_plugin::qemu_plugin_id_t,
    info: *const qemu_plugin::qemu_info_t,
    argc: i32,
    argv: *const *const u8,
) -> i32 {
    let max_cores = qemu_plugin_n_vcpus() as usize;
    let args_n = argc as usize;
    let base_ptr = slice::from_raw_parts(argv, args_n);
    let mut cores: Vec<usize> = Vec::new();
    let mut logfile_path_base: String = "./inst_break_stats.".to_string();
    for arg in 0..argc as usize {
        let arg_str = CStr::from_ptr(base_ptr[arg] as *mut i8);
        println!("arg{}:{}", arg, arg_str.to_str().unwrap());
        let args_tokens: Vec<&str> = arg_str.to_str().unwrap().split("=").collect();
        let arg_token = args_tokens[0].to_string();
        let arg_value = args_tokens[1].to_string();
        if arg_token == "arch" {
            assert!(&arg_value == "x86" || &arg_value == "arm");
            ARCH.set(arg_value).unwrap();
        } else if arg_token == "cores" {
            for core in arg_value.split("-") {
                cores.push(core.parse().unwrap());
            }
        } else if arg_token == "path" {
            logfile_path_base = arg_value;
        }
    }
    N_CORES.set(max_cores).unwrap();

    let arch = ARCH.get_unchecked();
    CS.set(CapstonePtr::new(get_capstone(arch))).unwrap();
    let timer_s = 300;
    let timer_ms = timer_s * 1000;
    LogDumper::launch_dumpers(cores, timer_ms, logfile_path_base);
    let mut categories_maps = CATEGORIES_BREAKDOWNS.write().unwrap();
    for core in 0..max_cores {
        categories_maps.push(BreakdownCategories::new());
    }

    let logfile = FileAppender::builder()
        .encoder(Box::new(PatternEncoder::new("{l} - {m}\n")))
        .build("log/output.log")
        .expect("Open logfile failed");

    let config = Config::builder()
        .appender(Appender::builder().build("logfile", Box::new(logfile)))
        .build(Root::builder().appender("logfile").build(LevelFilter::Info))
        .expect("Log Config init");

    log4rs::init_config(config).expect("Log failed to init");

    // register translation block function
    qemu_plugin_register_vcpu_tb_trans_cb(id, Some(vcpu_tb_trans));
    // register exit function
    qemu_plugin_register_atexit_cb(id, Some(plugin_exit), ptr::null_mut());

    return 0;
}
