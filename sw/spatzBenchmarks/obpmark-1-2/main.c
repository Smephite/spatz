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


#define EN_SAR_range_ref
//#define EN_SAR_DCE
//#define EN_SAR_rcmc_table
//#define EN_SAR_azimuth_ref
//#define EN_SAR_range_compression
//#define EN_SAR_complex_transpose
//#define EN_SAR_rcmc
//#define EN_SAR_azimuth_compression
//#define EN_SAR_multilook
//#define EN_SAR_quantize

//#define COMPUTE_ONLY
//#define VERIFY_RESULTS

#define KERNEL_ITERATIONS 1




#include "main.h"

// Seems to require multiple of 3
const uint32_t snrt_stack_size __attribute__((section(".rodata"))) = 90;

uint32_t patch_index;

float rrf[DATA_W];

void _SAR_range_ref(const unsigned int num_cores, const unsigned int cid){
  SAR_range_ref(rrf, DATA_W);
}
void _SAR_DCE(const unsigned int num_cores, const unsigned int cid){}
void _SAR_rcmc_table(const unsigned int num_cores, const unsigned int cid){}
void _SAR_azimuth_ref(const unsigned int num_cores, const unsigned int cid){}
void _SAR_range_compression(const unsigned int num_cores, const unsigned int cid){}
void _SAR_complex_transpose(const unsigned int num_cores, const unsigned int cid){}
void _SAR_rcmc(const unsigned int num_cores, const unsigned int cid){}
void _SAR_azimuth_compression(const unsigned int num_cores, const unsigned int cid){}
void _SAR_multilook(const unsigned int num_cores, const unsigned int cid){}
void _SAR_quantize(const unsigned int num_cores, const unsigned int cid){}

void _empty_compute(const unsigned int num_cores, const unsigned int cid){}
void _empty_dma(){}
void _empty_init(){}



int main() {

  // Allocate the matrices in the local tile
  if (snrt_cluster_core_idx() == 0) {
    printf("==== OBPMark Kernel 1-2 Benchmark ====\n");
    printf("Data Width: %u\n", DATA_W);
    #ifdef COMPUTE_ONLY
    printf("Compute only!\n");
    #endif
    printf("Starting Memory allocation\n");
    init_l1();
  }
  

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();

  #ifdef EN_SAR_range_ref
    run_kernel(1, "SAR_range_ref", _empty_init, _empty_dma, _SAR_range_ref, _empty_dma);
  #endif
  
  #ifdef EN_SAR_DCE
    run_kernel(2, "SAR_DCE", _empty_init, _empty_dma, _SAR_DCE, _empty_dma);
  #endif
  
  #ifdef EN_SAR_rcmc_table
    run_kernel(3, "SAR_rcmc_table", _empty_init, _empty_dma, _SAR_rcmc_table, _empty_dma);
  #endif
  
  #ifdef EN_SAR_azimuth_ref
    run_kernel(4, "SAR_azimuth_ref", _empty_init, _empty_dma, _SAR_azimuth_ref, _empty_dma);
  #endif

  for(patch_index = 0; patch_index < N_PATCH; ++patch_index){
    #ifdef EN_SAR_range_compression
      run_kernel(5, "range_compression", _empty_init, _empty_dma, _SAR_range_compression, _empty_dma);
    #endif
  
    #ifdef EN_SAR_complex_transpose
      run_kernel(6, "complex_transpose", _empty_init, _empty_dma, _SAR_complex_transpose, _empty_dma);
    #endif
  
    #ifdef EN_SAR_rcmc
      run_kernel(7, "SAR_rcmc", _empty_init, _empty_dma, _SAR_rcmc, _empty_dma);
    #endif
  
    #ifdef EN_SAR_azimuth_compression
      run_kernel(8, "SAR_azimuth_compression", _empty_init, _empty_dma, _SAR_azimuth_compression, _empty_dma);
    #endif
  
    #ifdef EN_SAR_complex_transpose
      run_kernel(9, "complex_transpose", _empty_init, _empty_dma, _SAR_complex_transpose, _empty_dma);
    #endif
  
    #ifdef EN_SAR_multilook
      run_kernel(10, "SAR_multilook", _empty_init, _empty_dma, _SAR_multilook, _empty_dma);
    #endif
  }
  #ifdef EN_SAR_quantize
    run_kernel(11, "SAR_quantize", _empty_init, _empty_dma, _SAR_quantize, _empty_dma);
  #endif  

  // Done
  if(snrt_cluster_core_idx() == 0){
    printf("All kernels completed\n");
  }

  // Wait for all cores to finish
  snrt_cluster_hw_barrier();
  return 0;
}
