#![crate_name = "rust_aux"]

use std::collections::{HashMap, VecDeque};

#[repr(C)]
pub struct c_array_t {
    len: u64,
    value: *const u64,
}

// Temporal page table: from guest (VA, asid, permission) to the host VA.
static mut TPT: Option<HashMap<u64, u64>> = None;
static mut TPT_KEYS: Vec<u64> = vec![];
// Shadow page table: from the host VA to the FPGA PA.
static mut SPT: Option<HashMap<u64, u64>> = None;
static mut SPT_KEYS: Vec<u64> = vec![];
// Inverted page table: from host VA to multiple guest (VA, asid, permission)
static mut IPT: Option<HashMap<u64, Vec<u64>>> = None;
// Physical page table allocator
static mut FREE_FPPN: Option<VecDeque<u64>> = None;

#[no_mangle]
pub unsafe extern "C" fn rust_aux_init(fppn_total_count: u64, data_starting_addr: u64) {
    TPT = Some(HashMap::new());
    SPT = Some(HashMap::new());
    IPT = Some(HashMap::new());
    // initialize the Free FPGA PPN queue
    let effective_data_page_number = (fppn_total_count / 256) * 255;
    FREE_FPPN = Some(
        (0..effective_data_page_number)
            .map(|x| x * 4096 + data_starting_addr)
            .collect(),
    );
}

// Temporal page table.

#[no_mangle]
pub unsafe extern "C" fn tpt_key_exists(gid: u64) -> bool {
    return TPT.as_ref().unwrap().contains_key(&gid);
}

#[no_mangle]
pub unsafe extern "C" fn tpt_lookup(gid: u64) -> u64 {
    assert!(tpt_key_exists(gid));
    return *TPT.as_ref().unwrap().get(&gid).unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn tpt_remove(gid: u64) {
    assert!(tpt_key_exists(gid));
    TPT.as_mut().unwrap().remove(&gid);
}

#[derive(Debug)]
struct guest_translation_tag_t {
    vpn: u64,
    asid: u32,
    perm: u8,
    is_kernel: bool,
}

impl guest_translation_tag_t {
    pub fn parse(gid: u64) -> Self {
        return guest_translation_tag_t {
            vpn: ((gid >> 12) & 0xfffffffff),
            asid: ((gid >> 48) & 0x7fff) as u32,
            perm: (gid as u8) & 0x3,
            is_kernel: (gid >> 63) != 0,
        };
    }

    pub fn pack(&self) -> u64 {
        let perm = (self.perm & 0x3) as u64;
        let asid = (self.asid as u64 & 0x7fff) << 48;
        let kernel_bit: u64 = if self.is_kernel { 1 << 63 } else { 0 };
        let vpn = (self.vpn & 0xfffffffff) << 12;
        return kernel_bit | asid | vpn | perm;
    }
}

#[no_mangle]
pub unsafe extern "C" fn tpt_register(gid: u64, hva: u64) {
    if tpt_key_exists(gid) {
        println!(
            "Warning[TPT]: The Guest translation tag {:?} is already mapped to the HVA {}",
            guest_translation_tag_t::parse(gid),
            hva
        );
    }

    TPT.as_mut().unwrap().insert(gid, hva);
}

#[no_mangle]
pub unsafe extern "C" fn tpt_print_all() {
    println!("Temporal Page Table \n ---------------");
    for (&gid, &hva) in TPT.as_ref().unwrap().iter() {
        println!("{:?}: {}", guest_translation_tag_t::parse(gid), hva)
    }
}

#[no_mangle]
pub unsafe extern "C" fn tpt_all_keys() -> c_array_t {
    TPT_KEYS = TPT.as_ref().unwrap().keys().map(|x| *x).collect();
    return c_array_t {
        len: TPT_KEYS.len() as u64,
        value: TPT_KEYS.as_ptr(),
    };
}

// Shadow page table

#[no_mangle]
pub unsafe extern "C" fn spt_key_exists(hva: u64) -> bool {
    return SPT.as_ref().unwrap().contains_key(&hva);
}

#[no_mangle]
pub unsafe extern "C" fn spt_lookup(hva: u64) -> u64 {
    assert!(spt_key_exists(hva));
    return *(SPT.as_ref().unwrap().get(&hva)).unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn spt_remove(hva: u64) {
    assert!(spt_key_exists(hva));
    SPT.as_mut().unwrap().remove(&hva);
}

#[no_mangle]
pub unsafe extern "C" fn spt_register(hva: u64, fppn: u64) {
    if spt_key_exists(hva) {
        println!(
            "Warning[SPT]: the host VA {} is already mapped to the FPGA PA {}",
            hva, fppn
        );
    }
    SPT.as_mut().unwrap().insert(hva, fppn);
}

#[no_mangle]
pub unsafe extern "C" fn spt_print_all() {
    println!("Shadow page table \n ----------------");
    for (&hva, &fppn) in TPT.as_ref().unwrap().iter() {
        println!("{}: {}", hva, fppn);
    }
}

#[no_mangle]
pub unsafe extern "C" fn spt_all_keys() -> c_array_t {
    SPT_KEYS = SPT.as_ref().unwrap().keys().map(|x| *x).collect();
    return c_array_t {
        len: SPT_KEYS.len() as u64,
        value: SPT_KEYS.as_ptr(),
    };
}

// Inverted page table
#[no_mangle]
pub unsafe extern "C" fn ipt_register(hva: u64, gid: u64) -> u64 {
    let ipt = IPT.as_mut().unwrap();
    if !ipt.contains_key(&hva) {
        // create the mapping first.
        ipt.insert(hva, vec![gid]);
        // return 0 means there is no synonym.
        return 0;
    }
    // avoid multiple time registering
    let t_set = ipt.get_mut(&hva).unwrap();
    assert!(!t_set.contains(&gid));
    t_set.push(gid);
    // Now it's a synonym.
    return 1;
}

#[no_mangle]
pub unsafe extern "C" fn ipt_unregister(hva: u64, gid: u64) {
    let ipt = IPT.as_mut().unwrap();
    assert!(ipt.contains_key(&hva));
    let t_set = ipt.get_mut(&hva).unwrap();
    assert!(t_set.contains(&gid));
    let idx = t_set.iter().position(|&x| x == gid).unwrap();
    t_set.remove(idx);
    if t_set.is_empty() {
        ipt.remove(&hva);
    }
}

#[no_mangle]
pub unsafe extern "C" fn ipt_lookup(hva: u64) -> c_array_t {
    let ipt = IPT.as_mut().unwrap();
    if !ipt.contains_key(&hva) {
        return c_array_t {
            len: 0,
            value: 0 as *const u64,
        };
    }
    let entry = ipt.get(&hva).unwrap();
    return c_array_t {
        len: entry.len() as u64,
        value: entry.as_ptr(),
    };
}

// FPGA PPN Allocation
#[no_mangle]
pub unsafe extern "C" fn fppn_allocate() -> u64 {
    let q = FREE_FPPN.as_mut().unwrap();
    if q.is_empty() {
        println!("Warning[FPGA_PPN_ALLOCATION]: Physical Pages on FPGA are depleted. ");
        assert!(false);
    }

    return q.pop_front().unwrap();
}

#[no_mangle]
pub unsafe extern "C" fn fppn_recycle(free_ppn: u64) {
    let q = FREE_FPPN.as_mut().unwrap();
    // TODO: Check if the page is double recycled.
    q.push_front(free_ppn);
}
