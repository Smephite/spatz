// Copyright 2023 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Matheus Cavalcante, ETH Zurich

//#define EN_F_OFFSET
//#define EN_F_MASK_REPLACE
#define EN_F_SCRUB
// #define EN_F_GAIN
// #define EN_F_2x2_BIN
// #define EN_F_COADD

#define KERNEL_ITERATIONS 1

//
//#define COMPUTE_ONLY
//#define VERIFY_RESULTS

#include "main.h"

// Seems to require multiple of 3
const uint32_t snrt_stack_size __attribute__((section(".rodata"))) = 90;


// Verify the matrices
int verify32(uint32_t *frame, const uint32_t *gm,
                  const unsigned int num_rows, const unsigned int num_columns) {
  uint32_t err = 0;
  for (unsigned int i = 0; i < num_rows; i++) {
    for (unsigned int j = 0; j < num_columns; j++) {
      int diff = ((int)gm[i*num_columns+j]) - ((int)frame[i*num_columns+j]);
      if (diff > 0) {
        err++;
        printf("Error @ %u %u: %u != %u\n", j, i, frame[i*num_columns+j], gm[i*num_columns+j]);
      }
    }
  }
  return err;
}

int verify16(uint16_t *frame, const uint16_t *gm,
                  const unsigned int num_rows, const unsigned int num_columns) {
  uint32_t err = 0;
  for (unsigned int i = 0; i < num_rows; i++) {
    for (unsigned int j = 0; j < num_columns; j++) {
      int diff = ((int)gm[i*num_columns+j]) - ((int)frame[i*num_columns+j]);
      if (diff > 0) {
        err++;
        printf("Error @ X: %u Y: %u: \t %u != %u\n", j, i, frame[i*num_columns+j], gm[i*num_columns+j]);
      }
    }
  }
  return err;
}


int main() {

  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int measure_iterations = 1;

  unsigned int timer_start, timer_end, timer, global_timer, timer_temp;
  unsigned int instr_start, instr_end, instr_tmp;
  
  unsigned int compute_timer_tmp, compute_timer_store;
  unsigned int load_timer_tmp, load_timer_store;
  
  unsigned int compute_instr_tmp, compute_instr_store;
  unsigned int load_instr_tmp, load_instr_store; 

  unsigned int k_times[6]  = {0};
  unsigned int k_instr[6]  = {0};
  unsigned int k_stalls[6] = {0};

  unsigned int tiles_x, tiles_y;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    printf("==== OBPMark Kernel Benchmark ====\n");
    printf("Height: %u, Width: %u, Frames %u\n", FRAME_H, FRAME_W, FRAME_NUM);
    #ifdef COMPUTE_ONLY
    printf("Compute only!\n");
    #endif
    printf("Starting Memory allocation\n");
    init_l1();
  }

  // Reset timer
  global_timer = (unsigned int)0;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  // #if ROW_MULTICORE
  //   const unsigned int x_start16 = 0;
  //   const unsigned int x_end16 = FRAME_W;
  //   const unsigned int y_start16 = (FRAME_H / num_cores) * cid;
  //   const unsigned int y_end16 = (FRAME_H / num_cores) * (cid + 1);
    
  //   const unsigned int x_start32 = 0;
  //   const unsigned int x_end32 = FRAME_H / 2;
  //   const unsigned int y_start32 = (FRAME_W / 2 / num_cores) * cid;
  //   const unsigned int y_end32 = (FRAME_W / 2 / num_cores) * (cid + 1);
  // #else
  //   const unsigned int x_start16 = (FRAME_W / num_cores) * cid;
  //   const unsigned int x_end16 = (FRAME_W / num_cores) * (cid + 1);
  //   const unsigned int y_start16 = 0;
  //   const unsigned int y_end16 = FRAME_H;

  //   const unsigned int x_start32 = (FRAME_W / 2 / num_cores) * cid;
  //   const unsigned int x_end32 = (FRAME_W / 2 / num_cores) * (cid + 1);
  //   const unsigned int y_start32 = 0;
  //   const unsigned int y_end32 = FRAME_H / 2;
  // #endif


for (unsigned int i = 0; i < FRAME_NUM; i++) {
#ifdef EN_F_OFFSET
  ////////////////// 
  //              //
  //   f_offset   //              
  //              //
  //////////////////
if (cid == 0)
  printf(
    "==================\n"
    "Kernel 1: f_offset\n"
    "==================\n");

  timer = (unsigned int)0;
  
  snrt_cluster_hw_barrier();

  
  timer_start = benchmark_get_cycle();
  
  tiles_x = FRAME_W / K1_SIZE;
  tiles_y = FRAME_H / K1_SIZE;

  compute_timer_store = compute_timer_tmp = 0;
  compute_instr_store = compute_instr_tmp = 0;

  // Start dump
  if (cid == 0)
    start_kernel();

  for (uint32_t ty = 0; ty < tiles_y; ty++)
    for (uint32_t tx = 0; tx < tiles_x; tx++) {
      
      init_f_offset();
      

      load_timer_tmp = benchmark_get_cycle();
      // Start kernel
      if (cid == 0){
        cpy_f_offset_in(i, tx, ty);
      }
      snrt_cluster_hw_barrier();
      load_timer_store = benchmark_get_cycle() - load_timer_tmp;
      compute_timer_tmp = benchmark_get_cycle();
      compute_instr_tmp = benchmark_get_instr();


      
      f_offset(
        frames16[0], frames16[1],
        K1_SIZE, K1_SIZE,
        0, K1_SIZE,
        (K1_SIZE / num_cores) * cid, (K1_SIZE / num_cores) * (cid+1)
      );
      
      snrt_cluster_hw_barrier();
      // This is not optimal, as the instructions for the cycle timer are still calculated in the instr counter
      compute_timer_store = benchmark_get_cycle() - compute_timer_tmp;
      compute_instr_store = benchmark_get_instr() - compute_instr_tmp;
      
      load_timer_tmp = benchmark_get_cycle();
      
      if (cid == 0){
        cpy_f_offset_out(i, tx, ty);
      }

    
        snrt_cluster_hw_barrier();
    load_timer_store += benchmark_get_cycle() - load_timer_tmp;
    }
    // End dump
    if (cid == 0)
      stop_kernel();

    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;

    snrt_cluster_hw_barrier();

    // Check and display results
    if (cid == 0) {
      timer += timer_temp;
      global_timer += timer;
      k_times[0] += timer;
      printf("f_offset: %u\n", timer);
      printf("Compute: (cc: %u, instr: %u, IPC: %u (op/1000cc)), Data: %ucc\n", compute_timer_store, compute_instr_store, compute_instr_store*1000/compute_timer_store, load_timer_store);
      printf(
        "Size, Total CC, Compute CC, Compute Instr, DMA CC\n"
        "%u,%u,%u,%u,%u\n",
      FRAME_W, timer, compute_timer_store, compute_instr_store, load_timer_store);
    }
    
    if (cid == 0) {
      #ifdef VERIFY_RESULTS
      printf("Starting verification\n");
        int errs = verify16(out_frame, f_offset_gm[i], FRAME_W, FRAME_H);
        if(errs != 0){
          printf("f_offset: %u Errors\n", errs);
        } else {
          printf("f_offset: Correct\n");
        }
      #endif
    }


  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
#endif
#ifdef EN_F_MASK_REPLACE
  //////////////////////// 
  //                    //
  //   f_mask_replace   //              
  //                    //
  ////////////////////////
if (cid == 0){
  printf(
    "========================\n"
    "Kernel 2: f_mask_replace\n"
    "========================\n");
}
  timer = (unsigned int)0;
  
  snrt_cluster_hw_barrier();

  if(cid == 0)
      init_f_mask_replace();
  
  timer_start = benchmark_get_cycle();
  
  tiles_x = FRAME_W / K1_SIZE;
  tiles_y = FRAME_H / K1_SIZE;

  compute_timer_store = compute_timer_tmp = 0;
  compute_instr_store = compute_instr_tmp = 0;

  // Start dump
  if (cid == 0)
    start_kernel();

  for (uint32_t ty = 0; ty < tiles_y; ty++)
    for (uint32_t tx = 0; tx < tiles_x; tx++) {

      load_timer_tmp = benchmark_get_cycle();
      // Start kernel
      if (cid == 0)
        cpy_f_mask_replace_in(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store = benchmark_get_cycle() - load_timer_tmp;
      compute_timer_tmp = benchmark_get_cycle();
      compute_instr_tmp = benchmark_get_instr();



      f_mask_replace(
        frames16[0], frame8_a,
        K2_SIZE, K2_SIZE,
        0, K2_SIZE,
        (K2_SIZE / num_cores) * cid, (K2_SIZE / num_cores) * (cid+1)
      );

      snrt_cluster_hw_barrier();
      // This is not optimal, as the instructions for the cycle timer are still calculated in the instr counter
      compute_timer_store = benchmark_get_cycle() - compute_timer_tmp;
      compute_instr_store = benchmark_get_instr() - compute_instr_tmp;

      load_timer_tmp = benchmark_get_cycle();

      if (cid == 0)
        cpy_f_mask_replace_out(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store += benchmark_get_cycle() - load_timer_tmp;
    }
    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    // Check and display results
    
    if (cid == 0) {
      timer += timer_temp;
      k_times[1] += timer;
      global_timer += timer;
      printf("f_mask_replace: %u \n", timer);
      printf("Compute: (cc: %u, instr: %u, IPC: %u (op/1000cc)), Data: %ucc\n", compute_timer_store, compute_instr_store, compute_instr_store*1000/compute_timer_store, load_timer_store);
      printf(
        "Size, Total CC, Compute CC, Compute Instr, DMA CC\n"
        "%u,%u,%u,%u,%u\n",
      FRAME_W, timer, compute_timer_store, compute_instr_store, load_timer_store);
    }
    if (cid == 0) {
      #ifdef VERIFY_RESULTS
      printf("Starting verification\n");
        int errs = verify16(out_frame, f_mask_replace_gm[i], FRAME_W, FRAME_H);
        if(errs != 0){
          printf("f_mask_replace: %u Errors\n", errs);
        } else {
          printf("f_mask_replace: Correct\n");
        }
      #endif
    }



  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
#endif
#ifdef EN_F_SCRUB

  /////////////////
  //             //
  //   f_scrub   //              
  //             //
  /////////////////
if (cid == 0){
  printf(
    "=================\n"
    "Kernel 3: f_scrub\n"
    "=================\n");
}
  timer = (unsigned int)0;
  
  snrt_cluster_hw_barrier();

  if(cid == 0)
      init_f_scrub(2);
  
  timer_start = benchmark_get_cycle();
  
  tiles_x = FRAME_W / K1_SIZE;
  tiles_y = FRAME_H / K1_SIZE;

  compute_timer_store = compute_timer_tmp = 0;
  compute_instr_store = compute_instr_tmp = 0;

  // Start dump
  if (cid == 0)
    start_kernel();

  for (uint32_t ty = 0; ty < tiles_y; ty++)
    for (uint32_t tx = 0; tx < tiles_x; tx++) {

      load_timer_tmp = benchmark_get_cycle();
      // Start kernel
      if (cid == 0)
        cpy_f_scrub_in(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store = benchmark_get_cycle() - load_timer_tmp;
      compute_timer_tmp = benchmark_get_cycle();
      compute_instr_tmp = benchmark_get_instr();




      f_scrub(
        frames16[0], frames16,
        2,
        K3_SIZE, K3_SIZE,
        0, K3_SIZE,
        (K3_SIZE / num_cores) * cid, (K3_SIZE / num_cores) * (cid+1)
      );

      snrt_cluster_hw_barrier();
      // This is not optimal, as the instructions for the cycle timer are still calculated in the instr counter
      compute_timer_store = benchmark_get_cycle() - compute_timer_tmp;
      compute_instr_store = benchmark_get_instr() - compute_instr_tmp;

      load_timer_tmp = benchmark_get_cycle();

      if (cid == 0)
        cpy_f_scrub_out(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store += benchmark_get_cycle() - load_timer_tmp;

    }
    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    
    // Check and display results
    if (cid == 0) {
      timer += timer_temp;
      global_timer += timer;
      k_times[2] += timer;
      printf("f_scrub: %u \n", timer);
      printf("Compute: (cc: %u, instr: %u, IPC: %u (op/1000cc)), Data: %ucc\n", compute_timer_store, compute_instr_store, compute_instr_store*1000/compute_timer_store, load_timer_store);
      printf(
        "Size, Total CC, Compute CC, Compute Instr, DMA CC\n"
        "%u,%u,%u,%u,%u\n",
      FRAME_W, timer, compute_timer_store, compute_instr_store, load_timer_store);
    }
    
    if (cid == 0) {
      #ifdef VERIFY_RESULTS 
        int pixel = verify16(frames16[0], f_scrub_gm[i], FRAME_W, FRAME_H);
        if(pixel != 0){
          printf("f_scrub: Mismatch! @ %i\n", pixel);
        }
      #endif
    }


  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
#endif
#ifdef EN_F_GAIN

  //////////////// 
  //            //
  //   f_gain   //              
  //            //
  ////////////////
if (cid == 0){
  printf(
    "================\n"
    "Kernel 4: f_gain\n"
    "================\n");
}
  timer = (unsigned int)0;
  
  snrt_cluster_hw_barrier();

  if(cid == 0)
      init_f_gain();
  
  timer_start = benchmark_get_cycle();
  
  tiles_x = FRAME_W / K1_SIZE;
  tiles_y = FRAME_H / K1_SIZE;

  compute_timer_store = compute_timer_tmp = 0;
  compute_instr_store = compute_instr_tmp = 0;

  // Start dump
  if (cid == 0)
    start_kernel();

  for (uint32_t ty = 0; ty < tiles_y; ty++)
    for (uint32_t tx = 0; tx < tiles_x; tx++) {
      
      load_timer_tmp = benchmark_get_cycle();
      // Start kernel
      if (cid == 0)
        cpy_f_gain_in(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store = benchmark_get_cycle() - load_timer_tmp;
      compute_timer_tmp = benchmark_get_cycle();
      compute_instr_tmp = benchmark_get_instr();



      f_gain(
        frames16[0], frames16[1],
        K4_SIZE, K4_SIZE,
        0, K4_SIZE,
        (K4_SIZE / num_cores) * cid, (K4_SIZE / num_cores) * (cid+1)
      );

      snrt_cluster_hw_barrier();
      // This is not optimal, as the instructions for the cycle timer are still calculated in the instr counter
      compute_timer_store = benchmark_get_cycle() - compute_timer_tmp;
      compute_instr_store = benchmark_get_instr() - compute_instr_tmp;

      load_timer_tmp = benchmark_get_cycle();

      if (cid == 0)
        cpy_f_gain_out(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store += benchmark_get_cycle() - load_timer_tmp;

    }
    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    // Check and display results
    if (cid == 0) {
      timer += timer_temp;
      global_timer += timer;
      k_times[3] += timer;
      printf("f_gain: %u \n", timer);
      printf("Compute: (cc: %u, instr: %u, IPC: %u (op/1000cc)), Data: %ucc\n", compute_timer_store, compute_instr_store, compute_instr_store*1000/compute_timer_store, load_timer_store);
      printf(
        "Size, Total CC, Compute CC, Compute Instr, DMA CC\n"
        "%u,%u,%u,%u,%u\n",
      FRAME_W, timer, compute_timer_store, compute_instr_store, load_timer_store);
    }

    if (cid == 0) {
      #ifdef VERIFY_RESULTS 
        int pixel = verify16(frames16[0], f_gain_gm[i], FRAME_W, FRAME_H);
        if(pixel != 0){
          printf("f_gain: Mismatch! @ %i\n", pixel);
        }
      #endif
    }


  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
#endif
#ifdef EN_F_2x2_BIN

  /////////////////// 
  //               //
  //   f_2x2_bin   //              
  //               //
  ///////////////////
if (cid == 0){
  printf(
    "===================\n"
    "Kernel 5: f_2x2_bin\n"
    "===================\n");
}
  timer = (unsigned int)0;
  
  snrt_cluster_hw_barrier();

  if(cid == 0)
      init_f_2x2_bin();
  
  timer_start = benchmark_get_cycle();
  
  tiles_x = FRAME_W / K1_SIZE;
  tiles_y = FRAME_H / K1_SIZE;

  compute_timer_store = compute_timer_tmp = 0;
  compute_instr_store = compute_instr_tmp = 0;

  // Start dump
  if (cid == 0)
    start_kernel();

  for (uint32_t ty = 0; ty < tiles_y; ty++)
    for (uint32_t tx = 0; tx < tiles_x; tx++) {
      load_timer_tmp = benchmark_get_cycle();
      // Start kernel
      if (cid == 0)
        cpy_f_mask_replace_in(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store = benchmark_get_cycle() - load_timer_tmp;
      compute_timer_tmp = benchmark_get_cycle();
      compute_instr_tmp = benchmark_get_instr();



      f_2x2_bin(
        frames16[0], frame32_a,
        out_buf,
        K5_SIZE_IN, K5_SIZE_IN,
        0, K5_SIZE_IN,
        (K5_SIZE_IN / num_cores) * cid, (K5_SIZE_IN / num_cores) * (cid+1)
      );

      snrt_cluster_hw_barrier();
      // This is not optimal, as the instructions for the cycle timer are still calculated in the instr counter
      compute_timer_store = benchmark_get_cycle() - compute_timer_tmp;
      compute_instr_store = benchmark_get_instr() - compute_instr_tmp;

      load_timer_tmp = benchmark_get_cycle();

      if (cid == 0)
        cpy_f_2x2_bin_out(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store += benchmark_get_cycle() - load_timer_tmp;

    }
    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    // Check and display results
    if (cid == 0) {
      timer += timer_temp;
      global_timer += timer;
      k_times[4] += timer;
      printf("f_2x2_bin: %u\n", timer);
      printf("Compute: (cc: %u, instr: %u, IPC: %u (op/1000cc)), Data: %ucc\n", compute_timer_store, compute_instr_store, compute_instr_store*1000/compute_timer_store, load_timer_store);
      printf(
        "Size, Total CC, Compute CC, Compute Instr, DMA CC\n"
        "%u,%u,%u,%u,%u\n",
      FRAME_W, timer, compute_timer_store, compute_instr_store, load_timer_store);
    }
    if (cid == 0) {
      #ifdef VERIFY_RESULTS 
        int pixel = verify32(frame32_a, f_2x2_bin_gm[i], FRAME_W/2, FRAME_H/2);
        if(pixel != 0){
          printf("f_2x2_bin: Mismatch! @ %i\n", pixel);
        }
      #endif
    }



  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
#endif
#ifdef EN_F_COADD
  ///////////////// 
  //             //
  //   f_coadd   //              
  //             //
  /////////////////
if (cid == 0){
  printf(
    "=================\n"
    "Kernel 6: f_coadd\n"
    "=================\n");
}
  timer = (unsigned int)0;
  
  snrt_cluster_hw_barrier();

  if(cid == 0)
      init_f_coadd();
  
  timer_start = benchmark_get_cycle();
  
  tiles_x = FRAME_W / K1_SIZE;
  tiles_y = FRAME_H / K1_SIZE;

  compute_timer_store = compute_timer_tmp = 0;
  compute_instr_store = compute_instr_tmp = 0;


  // Start dump
  if (cid == 0)
    start_kernel();

  for (uint32_t ty = 0; ty < tiles_y; ty++)
    for (uint32_t tx = 0; tx < tiles_x; tx++) {
      load_timer_tmp = benchmark_get_cycle();
      // Start kernel
      if (cid == 0)
        cpy_f_coadd_in(i, tx, ty);

      snrt_cluster_hw_barrier();
      load_timer_store = benchmark_get_cycle() - load_timer_tmp;
      compute_timer_tmp = benchmark_get_cycle();
      compute_instr_tmp = benchmark_get_instr();



      f_coadd(
        frame32_a, frame32_b,
        K6_SIZE, K6_SIZE,
        0, K6_SIZE,
        (K6_SIZE / num_cores) * cid, (K6_SIZE / num_cores) * (cid+1)
      );

      snrt_cluster_hw_barrier();
      // This is not optimal, as the instructions for the cycle timer are still calculated in the instr counter
      compute_timer_store = benchmark_get_cycle() - compute_timer_tmp;
      compute_instr_store = benchmark_get_instr() - compute_instr_tmp;

      load_timer_tmp = benchmark_get_cycle();

      if (cid == 0)
        cpy_f_coadd_out(i, tx, ty);

        
      snrt_cluster_hw_barrier();
      load_timer_store += benchmark_get_cycle() - load_timer_tmp;

    }
    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    // Check and display results
    if (cid == 0) {
      timer += timer_temp;
      global_timer += timer;
      k_times[5] += timer;
      printf("f_coadd: %u\n", timer);
      printf("Compute: (cc: %u, instr: %u, IPC: %u (op/1000cc)), Data: %ucc\n", compute_timer_store, compute_instr_store, compute_instr_store*1000/compute_timer_store, load_timer_store);
      printf(
        "Size, Total CC, Compute CC, Compute Instr, DMA CC\n"
        "%u,%u,%u,%u,%u\n",
      FRAME_W, timer, compute_timer_store, compute_instr_store, load_timer_store);
    }
    if (cid == 0) {
      #ifdef VERIFY_RESULTS 
        int pixel = verify32(frame32_a, f_coadd_gm[i], FRAME_W/2, FRAME_H/2);
        if(pixel != 0){
          printf("f_coadd: Mismatch! @ %i\n", pixel);
        }
      #endif
    }

#endif
}
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  if (cid == 0) {
    printf("All kernels completed in %u cycles (per frame %.02f)\n", global_timer, (float)global_timer/FRAME_NUM);
    for(uint32_t i = 0; i < 6; i++){
      printf("Kernel %u: %u (%.02f per image)\n", i, k_times[i], (float)k_times[i]/FRAME_NUM);
    }

  } 

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  return 0;
}
