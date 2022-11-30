use std::fmt::format;
use std::fs::File;
use std::io::{self, BufReader, BufWriter, Read};

use capstone::arch::arm64::{
    Arm64InsnDetail, Arm64OpMem, Arm64Operand, Arm64OperandIterator, Arm64OperandType,
};
use capstone::arch::x86::{
    X86InsnDetail, X86OpMem, X86Operand, X86OperandIterator, X86OperandType,
};
use capstone::arch::ArchOperand;
use capstone::{prelude::*, Insn, Instructions, RegAccessType};
use regex::Regex;
use std::collections::HashMap;

#[derive(Debug)]
pub struct Breakdown {
    pub tot: usize,
    pub tot_insn: [usize; 2],

    // Standard
    pub tot_load: [usize; 2],
    pub tot_store: [usize; 2],
    pub tot_uops: [usize; 2],

    // type
    pub is_br: [usize; 2],
    pub is_mem: [usize; 2],
    pub is_fp: [usize; 2],
    pub is_simd: [usize; 2],
    pub is_crypto: [usize; 2],
    pub is_priviledge: [usize; 2],

    // Freak cases
    pub with_both: [usize; 2],
    pub mem_branch: [usize; 2],
    pub has_multi_mem: [usize; 2],

    pub byte_sizes: [[usize; 16]; 2],
}

impl Default for Breakdown {
    fn default() -> Breakdown {
        Breakdown {
            tot: 0,
            tot_insn:[0; 2],
            tot_load: [0; 2],
            tot_store: [0; 2],
            tot_uops: [0; 2],
            is_br: [0; 2],
            is_priviledge: [0; 2],
            is_mem: [0; 2],
            is_fp: [0; 2],
            is_simd: [0; 2],
            is_crypto: [0; 2],
            with_both: [0; 2],
            mem_branch: [0; 2],
            has_multi_mem: [0; 2],
            byte_sizes: [[0usize; 16]; 2],
        }
    }
}

pub struct BreakdownData {
    pub mnemonic: String,
    pub groups: String,
    pub group: GroupTypeEnum,
    pub is_user: bool,
    pub byte_len: usize,
    pub loads: usize,
    pub stores: usize,
    pub uops: usize,
    pub is_br: bool,
    pub is_priviledge: bool,
    pub is_mem: bool,
    pub is_fp: bool,
    pub is_simd: bool,
    pub is_crypto: bool,
    pub has_both_mem: bool,
    pub has_mem: bool,
    pub has_multi_mem: bool,
    pub is_rep: bool,
}

impl Default for BreakdownData {
    fn default() -> Self {
        BreakdownData {
            mnemonic: "".to_string(),
            groups: "".to_string(),
            group: GroupTypeEnum::CAT_OTHERS,
            is_user: false,
            byte_len: 0,
            loads: 0,
            stores: 0,
            uops: 0,
            is_br: false,
            is_priviledge: false,
            is_mem: false,
            is_fp: false,
            is_simd: false,
            is_crypto: false,
            has_both_mem: false,
            has_mem: false,
            has_multi_mem: false,
            is_rep: false,
        }
    }
}

impl Breakdown {
    fn print_stats_tot(&self) -> String {
        return format!(
            "tot,{},{},{},{},{},{},{},{},{},{},{},{},",
            self.tot,
            self.tot_load[0] + self.tot_load[1],
            self.tot_store[0] + self.tot_store[1],
            self.tot_uops[0] + self.tot_uops[1],
            self.is_br[0] + self.is_br[1],
            self.is_mem[0] + self.is_mem[1],
            self.is_fp[0] + self.is_fp[1],
            self.is_crypto[0] + self.is_crypto[1],
            self.is_priviledge[0] + self.is_priviledge[1],
            self.with_both[0] + self.with_both[1],
            self.mem_branch[0] + self.mem_branch[1],
            self.has_multi_mem[0] + self.has_multi_mem[1],
        );
    }

    fn print_stats(&self, op: &str, idx: usize) -> String {
        return format!(
            "{},{},{},{},{},{},{},{},{},{},{},{},{},",
            op,
            self.tot_insn[idx],
            self.tot_load[idx],
            self.tot_store[idx],
            self.tot_uops[idx],
            self.is_br[idx],
            self.is_mem[idx],
            self.is_fp[idx],
            self.is_crypto[idx],
            self.is_priviledge[idx],
            self.with_both[idx],
            self.mem_branch[idx],
            self.has_multi_mem[idx],
        );
    }

    fn print_stats_user(&self) -> String {
        return self.print_stats("user", 0);
    }

    fn print_stats_sys(&self) -> String {
        return self.print_stats("os", 1);
    }

    fn print_byte_dist(&self, idx: usize) -> String {
        let str = if idx == 0 { "user" } else { "os" };
        return format!(
            "{},{},{},{},{},{},{},{},{},{},{},{},{},",
            str,
            self.byte_sizes[idx][1],
            self.byte_sizes[idx][2],
            self.byte_sizes[idx][3],
            self.byte_sizes[idx][4],
            self.byte_sizes[idx][5],
            self.byte_sizes[idx][6],
            self.byte_sizes[idx][7],
            self.byte_sizes[idx][8],
            self.byte_sizes[idx][9],
            self.byte_sizes[idx][10],
            self.byte_sizes[idx][11],
            self.byte_sizes[idx][12],
        );
    }

    fn update(&mut self, data: &BreakdownData, count: usize) {

        let idx = if data.is_user {
            0
        } else {
            1
        };

        if !data.is_rep {
            self.tot += count;
            self.tot_insn[idx] += count;
            self.byte_sizes[idx][data.byte_len] += count;
        }

        self.tot_load[idx] += data.loads * count;
        self.tot_store[idx] += data.stores * count;
        self.tot_uops[idx] += data.uops * count;

        if data.is_br {
            self.is_br[idx] += count;
        }

        if data.is_mem {
            self.is_mem[idx] += count;
        }

        if data.is_fp {
            self.is_fp[idx] += count;
        }

        if data.is_simd {
            self.is_simd[idx] += count;
        }

        if data.is_crypto {
            self.is_crypto[idx] += count;
        }

        if data.is_priviledge {
            self.is_priviledge[idx] += count;
        }

        if data.has_both_mem {
            self.with_both[idx] += count;
        }

        if data.has_mem && data.is_br {
            self.mem_branch[idx] += count;
        }

        if data.has_multi_mem {
            self.has_multi_mem[idx] += count;
        }
    }
}

/// Print instruction group names
fn group_names(cs: &Capstone, regs: &[InsnGroupId]) -> String {
    let mut names: Vec<String> = regs
        .iter()
        .map(|&x| cs.group_name(x).unwrap())
        .filter(|s| !s.starts_with("mode"))
        .collect();
    names.sort();
    names.join(", ")
}

pub fn get_capstone(arch: &String) -> Capstone {
    if arch == "x86" {
        return Capstone::new()
            .x86()
            .mode(arch::x86::ArchMode::Mode64)
            .detail(true)
            .build()
            .unwrap();
    } else if arch == "arm" {
        return Capstone::new()
            .arm64()
            .mode(arch::arm64::ArchMode::Arm)
            .detail(true)
            .build()
            .unwrap();
    } else {
        panic!("Not supported arch");
    }
}

// Maps
pub struct BreakdownCategories {
    pub map_groups: HashMap<String, HashMap<String, Breakdown>>,
    pub map_mnen: [HashMap<String, Breakdown>; 8],
}

impl BreakdownCategories {
    pub fn new() -> BreakdownCategories {
        let map_mnen = [
            HashMap::new(),
            HashMap::new(),
            HashMap::new(),
            HashMap::new(),
            HashMap::new(),
            HashMap::new(),
            HashMap::new(),
            HashMap::new(),
        ];
        BreakdownCategories {
            map_groups: HashMap::new(),
            map_mnen: map_mnen,
        }
    }

    pub fn update_breakdown(&mut self, data: &BreakdownData, count: usize) {

        let mnem_hashmap = &mut self.map_mnen[data.group.clone() as usize];
        let mnem_breakdown = mnem_hashmap
            .entry(data.mnemonic.clone())
            .or_insert(Breakdown::default());
        
        let groups_map= self
            .map_groups
            .entry(data.groups.clone())
            .or_insert(HashMap::new());
        let groups_mnem = groups_map
            .entry(data.mnemonic.clone())
            .or_insert(Breakdown::default());
 
        mnem_breakdown.update(&data, count);
        groups_mnem.update(&data, count);
    }

    pub fn get_log_stats(&self, with_byte_dist: bool) -> String {
        let mut str = "".to_string();
        str += "Groups Capstone:\n";
        let mut group_rates: Vec<(&String, usize)> = Vec::new();
        let mut group_rates_user: Vec<(&String, usize)> = Vec::new();
        let mut group_rates_os: Vec<(&String, usize)> = Vec::new();
        for (grouptype, map) in &self.map_groups {
            let (mut tot_insn, mut tot_insn_user, mut tot_insn_sys) = (0, 0, 0);
            str += &format!("{}:\n", &grouptype);
            for (mnemonic, breakdown) in map {
                tot_insn += breakdown.tot;
                tot_insn_user += breakdown.tot_insn[0];
                tot_insn_sys += breakdown.tot_insn[1];
                str += &format!("{mnemonic:?},");
                str += &breakdown.print_stats_tot();
                str += &breakdown.print_stats_user();
                str += &breakdown.print_stats_sys();
                str += "\n";
            }
            group_rates.push((grouptype, tot_insn));
            group_rates_user.push((grouptype, tot_insn_user));
            group_rates_os.push((grouptype, tot_insn_sys));
       }

        str += "Groups rates:\n";
        for (group, count) in group_rates {
            str += &format!("{:<14}:{}\n", count, group);
        }

        str += "Groups rates User:\n";
        for (group, count) in group_rates_user {
            str += &format!("{:<14}:{}\n", count, group);
        }

        str += "Groups rates OS:\n";
        for (group, count) in group_rates_os {
            str += &format!("{:<14}:{}\n", count, group);
        }

        str += "Stats:\n";
        let mut idx = 0;
        for hash_table in &self.map_mnen {
            let group = GroupTypeEnum::from(idx);
            str += &format!("{}:\n", group.get_str());
            for (mnemonic, breakdown) in hash_table {
                str += &format!("{mnemonic:?},");
                str += &breakdown.print_stats_tot();
                str += &breakdown.print_stats_user();
                str += &breakdown.print_stats_sys();
                str += "\n";
            }
            idx += 1;
        }

        if with_byte_dist {
            idx = 0;
            str += &format!("ByteDist:\n");
            for hash_table in &self.map_mnen {
                let group = GroupTypeEnum::from(idx);
                str += &format!("{}:\n", group.get_str());
                for (mnemonic, breakdown) in hash_table {
                    str += &format!("{mnemonic:?},");
                    str += &breakdown.print_byte_dist(0);
                    str += &breakdown.print_byte_dist(1);
                    str += "\n";
                }
                idx += 1;
            }
        }
        return str;
    }
}

#[derive(Clone, Eq, Hash, PartialEq)]
pub enum GroupTypeEnum {
    CAT_MEM = 0,
    CAT_BR = 1,
    CAT_LOGIC = 2,
    CAT_PRIV = 3,
    CAT_FP = 4,
    CAT_SIMD = 5,
    CAT_CRYPTO = 6,
    CAT_OTHERS = 7,
}

impl GroupTypeEnum {
    fn get_str(&self) -> String {
        let str = match *self {
            GroupTypeEnum::CAT_MEM => "MEM   ",
            GroupTypeEnum::CAT_BR => "BR    ",
            GroupTypeEnum::CAT_LOGIC => "LOGIC ",
            GroupTypeEnum::CAT_PRIV => "PRIV  ",
            GroupTypeEnum::CAT_FP => "FP    ",
            GroupTypeEnum::CAT_SIMD => "SIMD  ",
            GroupTypeEnum::CAT_CRYPTO => "CRYPTO",
            GroupTypeEnum::CAT_OTHERS => "OTHERS",
        };
        return str.to_string();
    }
    fn from(idx: i32) -> GroupTypeEnum {
        match idx {
            0 => GroupTypeEnum::CAT_MEM,
            1 => GroupTypeEnum::CAT_BR,
            2 => GroupTypeEnum::CAT_LOGIC,
            3 => GroupTypeEnum::CAT_PRIV,
            4 => GroupTypeEnum::CAT_FP,
            5 => GroupTypeEnum::CAT_SIMD,
            6 => GroupTypeEnum::CAT_CRYPTO,
            7 => GroupTypeEnum::CAT_OTHERS,
            _ => panic!("Wrong group enum"),
        }
    }
}

// Group definitions
static X86_GROUPS_BRANCH: [&str; 4] = ["jump", "ret", "branch_relative", "call"];
static X86_GROUPS_SIMD: [&str; 5] = ["sse1", "sse2", "sse41", "sse42", "ssse3"];
static X86_GROUPS_FP: [&str; 1] = ["fpu"];
static X86_GROUPS_CRYPTO: [&str; 3] = ["adx", "aes", "pclmul"];
static X86_GROUPS_OTHERS: [&str; 2] = ["not64bitmode", "fsgsbse"];
static X86_MEM_MNEMONICS: [&str; 6] = ["mov", "ins", "stosd", "push", "pop", "leave"];
static X86_FP_MNEMONICS: [&str; 1] = ["mmx"];

// Group definitions
static ARM_GROUPS_BRANCH: [&str; 4] = ["return", "branch_relative", "call", "jump"];
static ARM_GROUPS_SIMD: [&str; 1] = ["neon"];
static ARM_GROUPS_FP: [&str; 1] = ["fparmv8"];
static ARM_GROUPS_CRYPTO: [&str; 1] = ["crypto"];
static ARM_GROUPS_OTHERS: [&str; 1] = ["pointer"];
static ARM_MNEMONICS_MEM: [&str; 7] = ["stp", "ldp", "ld", "st", "cas", "prfm", "swp"];

pub fn execute(
    arch: &String,
    cs: &Capstone,
    inst_bytes: &[u8],
    is_user: bool,
) -> Vec<BreakdownData> {
    let insts = cs.disasm_all(inst_bytes, 0).unwrap();
    if insts.len() == 0 {
        log::warn!("FAIL DISASM: {:?}", inst_bytes);
    }

    let mut breaks: Vec<BreakdownData> = Vec::new();
    let is_single = insts.len() == 1;
    for insn in insts.iter() {
        let detail: InsnDetail = cs.insn_detail(&insn).expect("Failed to get insn detail");
        let mnemonic = insn.mnemonic().unwrap().to_string();
        let arch_detail = detail.arch_detail();
        let groups = group_names(&cs, detail.groups());
        if arch == "arm" {
            breaks.push(execute_arm(insn, arch_detail, mnemonic, groups, is_user));
        } else if arch == "x86" {
            breaks.push(execute_x86(
                insn,
                arch_detail,
                mnemonic,
                groups,
                is_user,
                insn.len(),
                is_single,
            ));
        }
    }
    return breaks;
}

fn extract_memops_x86(insn: &Insn, ops: X86OperandIterator, mnemonic: &String) -> (usize, usize) {
    // push and pop do not contain Mem for some reason
    let mut loads = 0;
    let mut stores = 0;
    let insn_str = format!("{}", insn);
    let ptr_matches = insn_str.match_indices("ptr");
    let ptr_count = ptr_matches.count();
    let contains_ptr = ptr_count > 0;

    let mut is_mem = false;

    if mnemonic.contains("push") {
        stores += 1;
    } else if mnemonic.contains("pop") {
        loads += 1;
    } else if mnemonic.contains("nop") {
        // nop can contain ptr call back but doesn't actually perform the call    
        return (0, 0);
    } if mnemonic.contains("rep") && 
        (mnemonic.contains("cmp") || mnemonic.contains("scas")) {
        if mnemonic.contains("cmp") {
            loads += 2;
        } else if mnemonic.contains("scas") {
            loads += 1;
        } else {
            panic!("Did not find what kind of rep: {}", insn);
        }
    } else {
        for op in ops.clone() {
            match op.op_type {
                X86OperandType::Mem(_) => {
                    is_mem = true;
                   if contains_ptr {
                        if mnemonic.contains("test") {
                            loads += 1;
                        } else if mnemonic.contains("leave") {
                            loads += 1;
                        } else if mnemonic.contains("outs") {
                            stores += 1;
                        } else {
                            match op.access {
                                Some(RegAccessType::ReadOnly) => {
                                    loads += 1;
                                }
                                Some(RegAccessType::WriteOnly) => {
                                    stores += 1;
                                }
                                Some(RegAccessType::ReadWrite) => {
                                    if ptr_count == 1 {
                                        return (1, 1);
                                    } else if ptr_count == 2 {
                                        return (2, 2);
                                    } else {
                                        panic!("Too many pointers: {}", insn);
                                    }
                                }
                                _ => {
                                    // For some reason `ins` and `movzx` didn't have ld/st operation
                                    if ptr_count > 1 {
                                        loads += 2;
                                        stores += 1;
                                        log::error!("Carefull, word with multiple ptr: {:}", insn);
                                    } else if mnemonic.contains("ins") || mnemonic.contains("movzx") {
                                        loads += 1;
                                    } else if mnemonic.contains("cvtsi2s") {
                                        loads += 1;
                                    } else if mnemonic.contains("outs") {
                                        stores += 1;
                                    } else if mnemonic.contains("palignr") {
                                        loads += 1;
                                    } else if mnemonic.contains("cldemote") {
                                        loads += 1;
                                    } else {
                                        
                                        log::error!(
                                            "Did not find what kind of memory operation: {}",
                                            insn
                                        );
                                    }
                                }
                            }
                        }
                    } else if mnemonic.contains("sgdt") {
                        stores += 1;
                    } else if mnemonic.contains("lea") {
                        // loads += 1;
                        is_mem = false;
                    } else if mnemonic.contains("fldenv") {
                        loads += 1;
                    } else if mnemonic.contains("fxsave") {
                        stores += 1;
                    } else if mnemonic.contains("fxrstor") {
                        loads += 1;
                    } else if mnemonic.contains("fstenv") {
                        stores += 1;
                    } else if mnemonic.contains("sidt") {
                        stores += 1;
                    } else if mnemonic.contains("ljmp") {
                        loads += 1;
                    } else {
                        log::error!("Not known memory access: {:}", insn);
                    }
                }
                _ => (),
            }
        }
    }

    let has_mem = loads + stores > 0;

    if !(mnemonic.contains("push") || mnemonic.contains("pop")) {
        if ptr_count == 2 && loads + stores != 2 {
            log::error!("insnPtr2:{}:{}:{}:{}", insn, loads, stores, ptr_count);
        } else if ptr_count == 1 && loads + stores != 1 {
            log::error!("insnPtr1:{}:{}:{}:{}", insn, loads, stores, ptr_count);
        }
    }

    if contains_ptr && !has_mem {
        log::error!("Ptr with no mem access: {:}", insn);
    }

    if is_mem && !has_mem {
        log::error!("Mem with no accesses: {:}", insn);
    }

    if !is_mem
        && has_mem
        && !insn_str.contains("ptr")
        && !(mnemonic.contains("push") || mnemonic.contains("pop"))
    {
        log::error!("Non memory with mem access: {:}", insn);
    }

    return (loads, stores);
}

fn extract_memops_arm(insn: &Insn, ops: Arm64OperandIterator, mnemonic: &String) -> (usize, usize) {
    let mut loads = 0;
    let mut stores = 0;
    if mnemonic.contains("stp") {
        stores += 2;
    } else if mnemonic.contains("st") {
        stores += 1;
    } else if mnemonic.contains("ldp") {
        loads += 2;
    } else if mnemonic.contains("ld") {
        loads += 1;
    } else if mnemonic.contains("cas") || mnemonic.contains("swp") {
        stores += 1;
        loads += 1;
    } else if mnemonic.contains("prf") {
        loads += 1;
    } else {
        for op in ops {
            match op.op_type {
                Arm64OperandType::Mem(value) => {
                    panic!("Failed to detect memory type: {}", insn);
                }
                _ => (),
            }
        }
    }
    return (loads, stores);
}

fn extract_uops_mem_arm(insn: &Insn, mnemonic: &String) -> usize {
    let mut uops = 0;
    let insn_str = format!("{}", insn).to_lowercase();
    let re_match_preidx = Regex::new(r"\[.*\]!").unwrap();
    let re_match_postidx = Regex::new(r"\[.*\],.*[#xrw]").unwrap();
    if re_match_preidx.is_match(&insn_str) {
        log::warn!("Found preidx: {}", insn);
        uops += 1;
    } else if re_match_postidx.is_match(&insn_str) {
        log::warn!("Found postidx: {}", insn);
        uops += 1;
    }
    return uops;
}

fn extract_uops_extra_others(insn: &Insn, mnemonic: &String) -> usize {
    let mut uops = 0;
    let insn_str = format!("{}", insn).to_lowercase();
    let re_match_decodebitmask = Regex::new(r"(and|orr|ands|tst|eor).*#0x").unwrap();
    let shifted_regs_mnemonics = ["ror", "lsr", "asr", "lsl"];
    let mut is_shift = false;
    let mut has_shift = false;
    for mnem in shifted_regs_mnemonics {
        if mnemonic.contains(mnem) {
            is_shift = true;
        }
    }

    if !is_shift {
        let no_mnem = insn_str
            .split(" ")
            .skip(1)
            .fold("".to_string(), |acc, word| acc + word);
        for str_shift in shifted_regs_mnemonics {
            if no_mnem.contains(str_shift) {
                log::warn!("Found shifted reg: {}", insn);
                uops += 1;
                has_shift = true;
            }
        }

        let re_match_with_shift = Regex::new("(ror|lsr|asr|lsl)").unwrap();
        if !has_shift && re_match_with_shift.is_match(&insn_str) {
            panic!("Matched shift without extra op: {}", insn);
        }
    }

    if re_match_decodebitmask.is_match(&insn_str) {
        log::warn!("Found with DecodeBitMask imm: {}", insn);
        uops += 1;
    }

    if mnemonic.contains("bfm") {
        // BFM like Logic Immediate uses DecodeBitMask
        log::warn!("Found with DecodeBitMask BFM: {}", insn);
        uops += 1
    }
    return uops;
}

fn extract_data(
    is_user: bool,
    byte_len: usize,
    groups: &String,
    mnemonic: &String,
    loads: usize,
    stores: usize,
    branch_groups: Vec<&str>,
    fp_groups: Vec<&str>,
    simd_groups: Vec<&str>,
    crypto_groups: Vec<&str>,
    others_groups: Vec<&str>,
    mem_mnemonics: Vec<&str>,
    fp_mnemonics: Vec<&str>,
) -> (GroupTypeEnum, bool, bool, bool, bool, bool, bool) {
    let mut is_br = false;
    let mut is_mem = false;
    let mut is_fp = false;
    let mut is_simd = false;
    let mut is_crypto = false;
    let mut is_priviledge = false;
    let mut is_others = false;

    for cate in branch_groups {
        if groups.contains(cate) {
            is_br = true;
        }
    }

    for cate in fp_groups {
        if groups.contains(cate) {
            is_fp = true;
        }
    }

    for cate in simd_groups {
        if groups.contains(cate) {
            is_simd = true;
        }
    }

    for cate in others_groups {
        if groups.contains(cate) {
            is_others = true;
        }
    }

    for cate in crypto_groups {
        if groups.contains(cate) {
            is_crypto = true;
        }
    }

    for mnem in mem_mnemonics {
        if mnemonic.contains(mnem) {
            is_mem = true;
        }
    }

    if groups.contains("priviledge") {
        is_priviledge = true;
    }

    let group: GroupTypeEnum = if is_priviledge {
        GroupTypeEnum::CAT_PRIV
    } else if is_crypto {
        GroupTypeEnum::CAT_CRYPTO
    } else if is_fp {
        GroupTypeEnum::CAT_FP
    } else if is_simd {
        GroupTypeEnum::CAT_SIMD
    } else if is_br {
        GroupTypeEnum::CAT_BR
    } else if is_mem {
        GroupTypeEnum::CAT_MEM
    } else if is_others {
        GroupTypeEnum::CAT_OTHERS
    } else {
        GroupTypeEnum::CAT_LOGIC
    };

    let ret = (
        group,
        is_br,
        is_priviledge,
        is_mem,
        is_fp,
        is_simd,
        is_crypto,
    );
    return ret;
}

fn execute_x86(
    insn: &Insn,
    arch_detail: ArchDetail,
    mnemonic: String,
    groups: String,
    is_user: bool,
    byte_len: usize,
    is_single: bool,
) -> BreakdownData {
    let x86_detail = arch_detail.x86().unwrap();
    let ops = x86_detail.operands();

    let (mut loads, mut stores) = extract_memops_x86(insn, ops, &mnemonic);

    let tot_mem = loads + stores;
    let has_mem = tot_mem >= 1;
    let has_both_mem = loads >= 1 && stores >= 1;


    let (group, is_br, is_priviledge, is_mem, is_fp, is_simd, is_crypto) = extract_data(
        is_user,
        byte_len,
        &groups,
        &mnemonic,
        loads,
        stores,
        X86_GROUPS_BRANCH.to_vec(),
        X86_GROUPS_FP.to_vec(),
        X86_GROUPS_SIMD.to_vec(),
        X86_GROUPS_CRYPTO.to_vec(),
        X86_GROUPS_OTHERS.to_vec(),
        X86_MEM_MNEMONICS.to_vec(),
        X86_FP_MNEMONICS.to_vec(),
    );

    let is_rep = mnemonic.contains("rep") && is_single;
    if is_rep && !has_mem {
        panic!("All REP must access memory");
    }

    let uops = if is_rep && (mnemonic.contains("cmp") || mnemonic.contains("scas")) {
        1
    } else if mnemonic.contains("leave") { 
        1 
    } else {
        0
    };

    if tot_mem > 2 {
        log::warn!("Tot mems too high: {}:loads{}:stores:{}", insn, loads, stores);
        loads = loads/2;
        stores = stores/2;
    }

    let mnemonic_stage1 = if has_mem {
        "mem ".to_string() + &mnemonic
    } else {
        "".to_string() + &mnemonic
    };

    let mut mnemonic_packed = mnemonic_stage1.clone();
    if is_simd {
        mnemonic_packed = "simd ".to_string() + &mnemonic_stage1;
    }
    if is_fp {
        mnemonic_packed = "fp ".to_string() + &mnemonic_stage1;
    }

    if byte_len <= 4 {
        log::warn!("Nbytes:{}:{}", byte_len, insn);
    }

    let data = BreakdownData {
        mnemonic: mnemonic_packed,
        groups,
        group,
        is_user,
        byte_len,
        loads,
        stores,
        uops,
        is_br,
        is_priviledge,
        is_mem,
        is_fp,
        is_simd,
        is_crypto,
        has_both_mem,
        has_mem,
        has_multi_mem: tot_mem > 1,
        is_rep,
    };

    return data;
}

fn execute_arm(
    insn: &Insn,
    arch_detail: ArchDetail,
    mnemonic: String,
    groups: String,
    is_user: bool,
) -> BreakdownData {
    let arm64_detail = arch_detail.arm64().unwrap();
    let ops = arm64_detail.operands();

    let loads = 0;
    let stores = 0;

    let (loads, stores) = extract_memops_arm(insn, ops, &mnemonic);
    let mut is_mem = false;
    for mnem in ARM_MNEMONICS_MEM.iter() {
        if mnemonic.contains(mnem) {
            is_mem = true;
        }
    }

    let has_multi_mem = loads + stores >= 2;
    let has_mem = loads + stores >= 1;
    let has_both_mem = loads >= 1 && stores >= 1;
 
    let uops = if is_mem {
        extract_uops_mem_arm(insn, &mnemonic)
    } else {
        extract_uops_extra_others(insn, &mnemonic)
    };

    let mut mnemonic_packed = mnemonic.clone();
    let (group, is_br, is_priviledge, is_mem, is_fp, is_simd, is_crypto) = extract_data(
        is_user,
        4,
        &groups,
        &mnemonic,
        loads,
        stores,
        ARM_GROUPS_BRANCH.to_vec(),
        ARM_GROUPS_FP.to_vec(),
        ARM_GROUPS_SIMD.to_vec(),
        ARM_GROUPS_CRYPTO.to_vec(),
        ARM_GROUPS_OTHERS.to_vec(),
        ARM_MNEMONICS_MEM.to_vec(),
        [].to_vec(),
    );

    if is_simd {
        let simd_sizes = ["b", "h", "s", "d"];
        let insn_str = format!("{}", insn).to_lowercase();
        let no_mnem = insn_str
            .split(" ")
            .skip(1)
            .fold("".to_string(), |acc, str| acc + str);
        let mut size_str = "";
        for size in simd_sizes {
            if insn_str.contains(size) {
                size_str = size;
            }
        }
        mnemonic_packed = "simd ".to_string() + &mnemonic + "." + size_str;
        if size_str == "" {
            panic!("SIMD size not detected: {}", insn);
        } else {
            log::warn!("Found SIMD: {}|{}", mnemonic_packed, insn);
        }
    }


    if is_fp {
        mnemonic_packed = "fp ".to_string() + &mnemonic;
    }

    let data = BreakdownData {
        mnemonic: mnemonic_packed,
        groups,
        group,
        is_user,
        byte_len: 4,
        loads,
        stores,
        uops,
        is_br,
        is_priviledge,
        is_mem,
        is_fp,
        is_simd,
        is_crypto,
        has_both_mem,
        has_mem,
        has_multi_mem,
        is_rep: false,
    };

    return data;
}
