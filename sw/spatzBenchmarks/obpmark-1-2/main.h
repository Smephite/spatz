
#ifndef OBPMARK_main_H
#define OBPMARK_main_H

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/obpmark-kernel.c"


size_t benchmark_get_instr() { return read_csr(minstret); }

unsigned int comp_cc_start = 0, comp_cc_end = 0, comp_cc_store = 0; 
unsigned int comp_ins_start = 0, comp_ins_end = 0, comp_ins_store = 0;

unsigned int dma_in_cc_start = 0,  dma_in_cc_end = 0, dma_in_cc_store = 0;
unsigned int dma_out_cc_start = 0, dma_out_cc_end = 0, dma_out_cc_store = 0; 

static inline void print_kernel_info(const unsigned int cid, int num, const char* name) {
    if (cid == 0) {
        printf("==================\n");
        printf("Kernel %d: %s\n", num, name);
        printf("==================\n");
    }
    snrt_cluster_hw_barrier();
}

static inline void start_dma_in_timer(void) {
    dma_in_cc_start = benchmark_get_cycle();
}

static inline void stop_dma_in_timer(void) {
    dma_in_cc_end = benchmark_get_cycle();
    dma_in_cc_store = dma_in_cc_end - dma_in_cc_start;
}

static inline void start_dma_out_timer(void) {
    dma_out_cc_start = benchmark_get_cycle();
}

static inline void stop_dma_out_timer(void) {
    dma_out_cc_end = benchmark_get_cycle();
    dma_out_cc_store = dma_out_cc_end - dma_out_cc_start;
}

static inline void start_comp_timer(void) {
    comp_cc_start = benchmark_get_cycle();
    comp_ins_start = (size_t)benchmark_get_instr();
}

static inline void stop_comp_timer(void) {
    comp_cc_end = benchmark_get_cycle();
    comp_ins_end = (size_t)benchmark_get_instr();
    comp_cc_store = comp_cc_end - comp_cc_start;
    comp_ins_store = comp_ins_end - comp_ins_start;
}

static inline void print_kernel_results(const unsigned int cid, const char* name) {
    if (cid == 0) {
        printf("%s: %u\n", name, comp_cc_store);
        printf("Compute: (cc: %u, instr: %u, IPC: %u (op/1000cc)), Data IN: %u, Data OUT: %u\n",
               comp_cc_store, comp_ins_store,
               comp_cc_store ? (comp_ins_store * 1000 / comp_cc_store) : 0,
               dma_in_cc_store, dma_out_cc_store);
        printf("Size, Compute CC, Compute Instr, DMA IN CC, DMA OUT CC\n"
               "%u,%u,%u,%u,%u\n",
               DATA_W, comp_cc_store, comp_ins_store,
               dma_in_cc_store, dma_out_cc_store);
    }
    snrt_cluster_hw_barrier();
}
// For DMA functions: no arguments
typedef void (*dma_phase_fn)(void);

// For compute functions: takes num_cores and cid
typedef void (*kernel_phase_fn)(const unsigned int num_cores, const unsigned int cid);

void run_kernel(int kernel_num, const char* kernel_name,
                dma_phase_fn init,
                dma_phase_fn dma_in,
                kernel_phase_fn compute,
                dma_phase_fn dma_out) 
{

  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();
    // Print kernel info
    print_kernel_info(cid, kernel_num, kernel_name);
    if (cid == 0) {
        if(init) init();
        // DMA IN
        start_dma_in_timer();
        if (dma_in) dma_in();
        stop_dma_in_timer();
    }

    snrt_cluster_hw_barrier();

    // Compute
    start_comp_timer();
    if (compute) compute(num_cores, cid);
    stop_comp_timer();

    snrt_cluster_hw_barrier();
    
    if (cid == 0) {
        // DMA OUT
        start_dma_in_timer();
        if (dma_out) dma_out();
        stop_dma_in_timer();
        snrt_cluster_hw_barrier();    
    }

    print_kernel_results(cid, kernel_name);
    
}


// DATA stuff
uint8_t *obpScratch;


void init_l1(){

  obpScratch = (uint8_t*) snrt_l1alloc(197632);
}

#ifndef COMPUTE_ONLY

#else

#endif

#endif