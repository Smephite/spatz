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

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/obpmark-kernel.c"

// if true each core works on a whole row at once
#define ROW_MULTICORE true

//
#define RANDOM_DATA

#define EN_F_OFFSET
#define EN_F_MASK_REPLACE
#define EN_F_SCRUB
#define EN_F_GAIN
#define EN_F_2x2_BIN
#define EN_F_COADD


const uint32_t snrt_stack_size __attribute__((section(".rodata"))) = 30;

uint32_t *frame32_a;
uint32_t *frame32_b;

uint16_t* frames16[5];


uint8_t  *frame8_a;

uint32_t *out_buf;

// Verify the matrices
int verify32(uint32_t *frame, const uint32_t *gm,
                  const unsigned int num_rows, const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    for (unsigned int j = 0; j < num_columns; ++j) {
      int diff = ((int)gm[i*num_columns+j]) - ((int)frame[i*num_columns+j]);
      if (diff > 0) {
        return i*num_columns+j == 0 ? -1 : (int)i*num_columns+j; 
      }
    }
  }
  return 0;
}

int verify16(uint16_t *frame, const uint16_t *gm,
                  const unsigned int num_rows, const unsigned int num_columns) {
  for (unsigned int i = 0; i < num_rows; ++i) {
    for (unsigned int j = 0; j < num_columns; ++j) {
      int diff = ((int)gm[i*num_columns+j]) - ((int)frame[i*num_columns+j]);
      if (diff > 0) {
        return i*num_columns+j == 0 ? -1 : (int)i*num_columns+j; 
      }
    }
  }
  return 0;
}

void init_l1(){
  // 32bit frames are after 2x2 binning -> half the original dimension
  frame32_a  = (uint32_t*) snrt_l1alloc(FRAME_W*FRAME_H*sizeof(uint32_t) / 4);
  frame32_b  = (uint32_t*) snrt_l1alloc(FRAME_W*FRAME_H*sizeof(uint32_t) / 4);
  out_buf    = (uint32_t*) snrt_l1alloc(256*sizeof(uint32_t));

  for(uint32_t i = 0; i < 5; ++i){
    frames16[i]  = (uint16_t*) snrt_l1alloc(FRAME_W*FRAME_H*sizeof(uint16_t));
  }

  frame8_a = (uint8_t*) snrt_l1alloc(FRAME_W*FRAME_H*sizeof(uint8_t));
}

void cpy_f_offset(uint32_t frame){
    snrt_dma_start_1d(frames16[0], input_frames[frame], FRAME_W * FRAME_H * sizeof(uint32_t));
    snrt_dma_start_1d(frames16[1], offset_map, FRAME_W * FRAME_H * sizeof(uint32_t));
    snrt_dma_wait_all();
}

void cpy_f_mask_replace(uint32_t frame){
    snrt_dma_start_1d(frames16[0], f_offset_gm[frame], FRAME_W * FRAME_H * sizeof(uint32_t));
    snrt_dma_start_1d(frame8_a, bad_pixel_map, FRAME_W * FRAME_H * sizeof(uint8_t));
    snrt_dma_wait_all();
}

void cpy_f_scrub(uint32_t frame){
    

  for(uint32_t i = 0; i < 5; ++i){
    snrt_dma_start_1d(frames16[i], f_mask_replace_gm[frame-2+i], FRAME_W * FRAME_H * sizeof(uint32_t));
  }
    
    snrt_dma_wait_all();
}

void cpy_f_gain(uint32_t frame){
    snrt_dma_start_1d(frames16[0], f_scrub_gm[frame], FRAME_W * FRAME_H * sizeof(uint32_t));
    snrt_dma_start_1d(frames16[1], gain_map, FRAME_W * FRAME_H * sizeof(uint16_t));
    snrt_dma_wait_all();
}

void cpy_f_2x2_bin(uint32_t frame){
    snrt_dma_start_1d(frames16[0], f_gain_gm[frame], FRAME_W * FRAME_H * sizeof(uint32_t));
    snrt_dma_wait_all();
}

void cpy_f_coadd  (uint32_t frame){
    snrt_dma_start_1d(frame32_a, image_outputs[frame-2], FRAME_W * FRAME_H * sizeof(uint32_t) / 4);
    snrt_dma_start_1d(frame32_b, f_2x2_bin_gm[frame], FRAME_W * FRAME_H * sizeof(uint32_t) / 4);
    snrt_dma_wait_all();
}


int main() {
  const unsigned int num_cores = snrt_cluster_core_num();
  const unsigned int cid = snrt_cluster_core_idx();

  const unsigned int measure_iterations = 1;

  unsigned int timer_start, timer_end, timer, global_timer, timer_temp;
  unsigned int k_times[6] = {0};

  unsigned int m_start, m_end;
  unsigned int p_start, p_end;
  unsigned int kernel_size;

  // Allocate the matrices in the local tile
  if (cid == 0) {
    printf("==== OBPMark Kernel Benchmark ====\n");
    printf("Height: %u, Width: %u, Frames %u\n", FRAME_H, FRAME_W, FRAME_NUM);
    printf("Starting Memory allocation\n");
    init_l1();
  }

  // Reset timer
  global_timer = (unsigned int)0;

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  #if ROW_MULTICORE
    const unsigned int x_start16 = 0;
    const unsigned int x_end16 = FRAME_W;
    const unsigned int y_start16 = (FRAME_H / num_cores) * cid;
    const unsigned int y_end16 = (FRAME_H / num_cores) * (cid + 1);
    
    const unsigned int x_start32 = 0;
    const unsigned int x_end32 = FRAME_H / 2;
    const unsigned int y_start32 = (FRAME_W / 2 / num_cores) * cid;
    const unsigned int y_end32 = (FRAME_W / 2 / num_cores) * (cid + 1);
  #else
    const unsigned int x_start16 = (FRAME_W / num_cores) * cid;
    const unsigned int x_end16 = (FRAME_W / num_cores) * (cid + 1);
    const unsigned int y_start16 = 0;
    const unsigned int y_end16 = FRAME_H;

    const unsigned int x_start32 = (FRAME_W / 2 / num_cores) * cid;
    const unsigned int x_end32 = (FRAME_W / 2 / num_cores) * (cid + 1);
    const unsigned int y_start32 = 0;
    const unsigned int y_end32 = FRAME_H / 2;
  #endif


for (unsigned int i = 2; i < FRAME_NUM+2; ++i) {
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
  
  if (cid == 0) {
  #if !defined RANDOM_DATA
  // Initialize f_offset data
      printf("Copying frame %u to L1\n", i);
      cpy_f_offset(i);
  #endif

      printf("Computing frame %u\n", i-2);
    }
    
  snrt_cluster_hw_barrier();

    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    f_offset(frames16[0], frames16[1], FRAME_W, FRAME_H, x_start16, x_end16, y_start16, y_end16);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    if (cid == 0) {
      #if !defined RANDOM_DATA 
        int pixel = verify16(frames16[0], f_offset_gm[i], FRAME_W, FRAME_H);
        if(pixel != 0){
          printf("f_offset: Mismatch! @ %i\n", pixel);
        }
      #endif
        timer += timer_temp;
    }

  // Check and display results
  if (cid == 0) {
    global_timer += timer;
    k_times[0] += timer;
    printf("f_offset: %u\n", timer);
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
    // Initialize f_offset data
  if (cid == 0) {
  #if !defined RANDOM_DATA
  // Initialize f_offset data
      printf("Copying frame %u to L1\n", i);
      cpy_f_mask_replace(i);
  #endif

      printf("Computing frame %u\n", i-2);
    }
    snrt_cluster_hw_barrier();

    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    f_mask_replace(frames16[0], frame8_a, FRAME_W, FRAME_H, x_start16, x_end16, y_start16, y_end16);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    if (cid == 0) {
      #if !defined RANDOM_DATA 
        int pixel = verify16(frames16[0], f_mask_replace_gm[i], FRAME_W, FRAME_H);
        if(pixel != 0){
          printf("f_offset: Mismatch! @ %i\n", pixel);
        }
      #endif
        timer += timer_temp;
    }


  // Check and display results
  if (cid == 0) {
    k_times[1] += timer;
    global_timer += timer;
    printf("f_mask_replace: %u \n", timer);
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

    // Initialize f_offset data
  if (cid == 0) {
  #if !defined RANDOM_DATA
  // Initialize f_offset data
      printf("Copying frame %u to L1\n", i);
      cpy_f_scrub(i);
  #endif

      printf("Computing frame %u\n", i-2);
    }
    snrt_cluster_hw_barrier();

    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    f_scrub(frames16[0], frames16, i, FRAME_W, FRAME_H, x_start16, x_end16, y_start16, y_end16);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    if (cid == 0) {
      #if !defined RANDOM_DATA 
        int pixel = verify16(frames16[0], f_scrub_gm[i], FRAME_W, FRAME_H);
        if(pixel != 0){
          printf("f_scrub: Mismatch! @ %i\n", pixel);
        }
      #endif
        timer += timer_temp;
    }

  // Check and display results
  if (cid == 0) {
    global_timer += timer;
    k_times[2] += timer;
    printf("f_scrub: %u \n", timer);
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

    // Initialize f_offset data
  if (cid == 0) {
  #if !defined RANDOM_DATA
  // Initialize f_offset data
      printf("Copying frame %u to L1\n", i);
      cpy_f_gain(i);
  #endif

      printf("Computing frame %u\n", i-2);
    }
    snrt_cluster_hw_barrier();

    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    f_gain(frames16[0], frames16[1], FRAME_W, FRAME_H, x_start16, x_end16, y_start16, y_end16);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    if (cid == 0) {
      #if !defined RANDOM_DATA 
        int pixel = verify16(frames16[0], f_gain_gm[i], FRAME_W, FRAME_H);
        if(pixel != 0){
          printf("f_gain: Mismatch! @ %i\n", pixel);
        }
      #endif
        timer += timer_temp;
    }

  // Check and display results
  if (cid == 0) {
    global_timer += timer;
    k_times[3] += timer;
    printf("f_gain: %u \n", timer);
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

    // Initialize f_2x2_bin data
  if (cid == 0) {
  #if !defined RANDOM_DATA
  // Initialize f_2x2_bin data
      printf("Copying frame %u to L1\n", i);
      cpy_f_2x2_bin(i);
  #endif

      printf("Computing frame %u\n", i-2);
    }
    snrt_cluster_hw_barrier();

    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    f_2x2_bin(frames16[0], frame32_a, out_buf, FRAME_W, FRAME_H, x_start16, x_end16, y_start16, y_end16);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    if (cid == 0) {
      #if !defined RANDOM_DATA 
        int pixel = verify32(frame32_a, f_2x2_bin_gm[i], FRAME_W/2, FRAME_H/2);
        if(pixel != 0){
          printf("f_2x2_bin: Mismatch! @ %i\n", pixel);
        }
      #endif
        timer += timer_temp;
    }


  // Check and display results
  if (cid == 0) {
    global_timer += timer;
    k_times[4] += timer;
    printf("f_2x2_bin: %u\n", timer);
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
    // Initialize f_2x2_bin data
  if (cid == 0) {
  #if !defined RANDOM_DATA
  // Initialize f_2x2_bin data
      printf("Copying frame %u to L1\n", i);
      cpy_f_coadd(i);
  #endif

      printf("Computing frame %u\n", i-2);
    }
    snrt_cluster_hw_barrier();

    // Start timer
    timer_start = benchmark_get_cycle();

    // Start dump
    if (cid == 0)
      start_kernel();

    f_coadd(frame32_a, frame32_b, FRAME_W, FRAME_H, x_start32, x_end32, y_start32, y_end32);

    // Wait for all cores to finish
    snrt_cluster_hw_barrier();

    // End dump
    if (cid == 0)
      stop_kernel();

    // End timer and check if new best runtime
    timer_end = benchmark_get_cycle();
    timer_temp = timer_end - timer_start;
    if (cid == 0) {
      #if !defined RANDOM_DATA 
        int pixel = verify32(frame32_a, f_coadd_gm[i], FRAME_W/2, FRAME_H/2);
        if(pixel != 0){
          printf("f_coadd: Mismatch! @ %i\n", pixel);
        }
      #endif
        timer += timer_temp;
    }

  // Check and display results
  if (cid == 0) {
    global_timer += timer;
    k_times[5] += timer;
    printf("f_coadd: %u\n", timer);
  }
}
  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
#endif

  if (cid == 0) {
    printf("All kernels completed in %u cycles (per frame %.02f)\n", global_timer, (float)global_timer/FRAME_NUM);
    for(uint32_t i = 0; i < 6; ++i){
      printf("Kernel %u: %u (%.02f per image)\n", i, k_times[i], (float)k_times[i]/FRAME_NUM);
    }

  } 

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  return 0;
}
