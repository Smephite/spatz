
#ifndef OBPMARK_main_H
#define OBPMARK_main_H

#include <benchmark.h>
#include <debug.h>
#include <snrt.h>
#include <stdio.h>

#include DATAHEADER
#include "kernel/obpmark-kernel.c"

uint8_t *obpScratch;

uint32_t *frame32_a;
uint32_t *frame32_b;

uint16_t* frames16[5];


uint8_t  *frame8_a;

uint32_t *out_buf;


uint16_t tempFrame16[FRAME_W*FRAME_H];
uint32_t tempFrame32[FRAME_W*FRAME_H/4];

static uint16_t out_frame[FRAME_W*FRAME_H];

uint32_t zero32[FRAME_W*FRAME_H] = {0};


size_t benchmark_get_instr() { return read_csr(minstret); }

void init_l1(){

  // Biggest Kernel requires 5*IMAGE_AREA*(uint_16_t = 2 bytes)
  // L1 is 256 KiB thus IMAGE_AREA_max = 256/10 Ki Pixel = 26214 pixel^2 (161.9 pixel pow 2)
  // As we do not want to occupy the whole L1 at the same time we assume max 80%
  // utilization. (204.8 KiB)
  // 
  // We will allocate a "scratchpad" memory with max 204.8 KiB which will be interpreted differently
  // depending on the currently running kernel.
  //
  // | Name      |   Footprint   | Max Pixel with 204.8KiB   | Working Size | Memory Usage | Pixel Size |
  // |-----------|---------------|---------------------------|--------------|--------------|------------|
  // | Kernel 1  | 4*Area        |       228 x 228           |     128      |    65'536    | 16 bit     |
  // | Kernel 2  | 3*Area        |       264 x 264           |     256      |    196'608   | 16 bit     |
  // | Kernel 3  | 10*Area       |       144 x 144           |     128      |    163'840   | 16 bit     |
  // | Kernel 4  | 2*Area        |       323 x 323           |     256      |    131'072   | 16 bit     |
  // | Kernel 5  | 3*Area + 1024 |       263 x 263           |     256      |    197'632   | 16->32 bit |
  // | Kernel 6  | 2*Area        |       161 x 161           |     128      |    131'072   | 32 bit     |
  //
  // The biggest memory user is Kernel 5 with 197'632 bytes (193 KiB, 75.49% L1).
  // This is the size we size our scratchpad to.
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Biggest Kernel requires 5*IMAGE_AREA*(uint_16_t = 2 bytes)
  // L1 is 128 KiB thus IMAGE_AREA_max = 128/10 Ki Pixel = 13107.2 pixel^2 (114.48 pixel pow 2)
  // As we do not want to occupy the whole L1 at the same time we assume max 80%
  // utilization. (102.4KiB)
  // 
  // We will allocate a "scratchpad" memory with max 204.8 KiB which will be interpreted differently
  // depending on the currently running kernel.
  //
  // | Name      |   Footprint   | Max Pixel with 102.4KiB   | Working Size | Memory Usage | Pixel Size |
  // |-----------|---------------|---------------------------|--------------|--------------|------------|
  // | Kernel 1  | 4*Area        |       161 x 161           |     128      |    65'536    | 16 bit     |
  // | Kernel 2  | 3*Area        |       186 x 186           |     128      |    49'152    | 16 bit     |
  // | Kernel 3  | 10*Area       |       102 x 102           |     64       |    40'960    | 16 bit     |
  // | Kernel 4  | 2*Area        |       228 x 228           |     128      |    32'768    | 16 bit     |
  // | Kernel 5  | 3*Area + 1024 |       186 x 186           |     128      |    50'176    | 16->32 bit |
  // | Kernel 6  | 2*Area        |       228 x 228           |     128      |    32'768    | 32 bit     |
  //
  // The biggest memory user is Kernel 1 with 65'536 bytes (64 KiB, 50% L1).
  // This is the size we size our scratchpad to.
  obpScratch = (uint8_t*) snrt_l1alloc(197632);
}

const unsigned int K1_SIZE     = 90;
const unsigned int K2_SIZE     = 90;
const unsigned int K3_SIZE     = 44;
const unsigned int K4_SIZE     = 90;
const unsigned int K5_SIZE_IN  = 90;
const unsigned int K5_SIZE_OUT = 90;
const unsigned int K6_SIZE     = 90;

void init_f_offset(){
  // Uses frames16[0] and frames16[1]
  frames16[0] = (uint16_t*) obpScratch + 0;
  frames16[1] = (uint16_t*) obpScratch + K1_SIZE*K1_SIZE*sizeof(uint16_t);
}

void init_f_mask_replace(){
  // Uses frames16[0] and frame8_a
  frames16[0] = (uint16_t*) obpScratch + 0;
  frame8_a    = (uint8_t*)  obpScratch + K2_SIZE*K2_SIZE*sizeof(uint16_t);
}

void init_f_scrub(uint32_t i){
  // assert i >= 2
  if(i < 2){
    printf("Error: init_f_scrub: assertion i >= 2");
    return;
  }

  // Uses frames16[i-2] to frames16[i+2]

    frames16[i-2] = (uint16_t*) obpScratch + 1*K3_SIZE*K3_SIZE*sizeof(uint16_t);
    frames16[i-1] = (uint16_t*) obpScratch + 2*K3_SIZE*K3_SIZE*sizeof(uint16_t);
    frames16[i  ] = (uint16_t*) obpScratch + 0*K3_SIZE*K3_SIZE*sizeof(uint16_t);
    frames16[i+1] = (uint16_t*) obpScratch + 3*K3_SIZE*K3_SIZE*sizeof(uint16_t);
    frames16[i+2] = (uint16_t*) obpScratch + 4*K3_SIZE*K3_SIZE*sizeof(uint16_t);
}


void init_f_gain(){
  // Uses frames16[0] and frames16[1]
  frames16[0] = (uint16_t*) obpScratch + 0;
  frames16[1] = (uint16_t*) obpScratch + K4_SIZE*K4_SIZE*sizeof(uint16_t);

}

void init_f_2x2_bin(){
  // Uses frames16[0], frame32_a and out_buf
  out_buf     = (uint32_t*) obpScratch + 0;
  frames16[0] = (uint16_t*) obpScratch + 1024;
  frame32_a   = (uint32_t*) obpScratch + K5_SIZE_IN*K5_SIZE_IN*sizeof(uint16_t) + 1024;
}

void init_f_coadd(){
  // Uses frame32_a and frame32_b

  frame32_a = (uint32_t*) obpScratch + 0;
  frame32_b = (uint32_t*) obpScratch + K6_SIZE*K6_SIZE*sizeof(uint32_t);
}

#ifndef COMPUTE_ONLY

void cpy_f_offset_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    size_t offset_x = sf_x * K1_SIZE;
    size_t offset_y = sf_y * K1_SIZE;

    const uint16_t *src1 = &input_frames[frame][offset_y + offset_x];
    const uint16_t *src2 = &offset_map[offset_y + offset_x];

    size_t row_bytes = K1_SIZE * sizeof(uint16_t);

    size_t src_stride = FRAME_W * sizeof(uint16_t);
    size_t dst_stride = row_bytes;
    size_t repeat = K1_SIZE;
    
    snrt_dma_txid_t a = snrt_dma_start_2d(frames16[0], src1, row_bytes, dst_stride, src_stride, repeat);
    snrt_dma_wait(a);
    snrt_dma_txid_t b = snrt_dma_start_2d(frames16[1], src2, row_bytes, dst_stride, src_stride, repeat);
    snrt_dma_wait(b);
}

void cpy_f_mask_replace_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    size_t offset_x = sf_x * K2_SIZE ;
    size_t offset_y = sf_y * K2_SIZE * FRAME_W;
    
    const uint16_t *src1 = &f_offset_gm[frame][offset_y + offset_x];
    const uint8_t *src2 = &bad_pixel_map[offset_y + offset_x];

    size_t row_bytes = K2_SIZE * sizeof(uint16_t);

    size_t src_stride = FRAME_W * sizeof(uint16_t);
    size_t dst_stride = row_bytes;
    size_t repeat = K2_SIZE;
    
    snrt_dma_start_2d(frames16[0], src1, row_bytes, dst_stride, src_stride, repeat);
    snrt_dma_start_2d(frame8_a, src2, row_bytes/2, dst_stride/2, src_stride/2, repeat/2);

    snrt_dma_wait_all();
}

void cpy_f_scrub_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    size_t offset_x = sf_x * K3_SIZE;
    size_t offset_y = sf_y * K3_SIZE * FRAME_W;

    size_t row_bytes = K3_SIZE * sizeof(uint16_t);

    size_t src_stride = FRAME_W * sizeof(uint16_t);
    size_t dst_stride = row_bytes;
    size_t repeat = K3_SIZE;

    snrt_dma_start_2d(frames16[2], &f_mask_replace_gm[frame][offset_y + offset_x], row_bytes, dst_stride, src_stride, repeat);
    snrt_dma_wait_all();

  for(uint32_t i = 0; i < 5; ++i){
    if(i == 2)
        continue;

    uint32_t f = i;
    
    if(i>2)
        f--;

    const uint16_t *src1 = &scrub_frames[f][offset_y + offset_x];
    
    snrt_dma_start_2d(frames16[i], src1, row_bytes, dst_stride, src_stride, repeat);
    snrt_dma_wait_all();
}
    
}

void cpy_f_gain_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    size_t offset_x = sf_x * K4_SIZE;
    size_t offset_y = sf_y * K4_SIZE * FRAME_W;

    const uint16_t *src1 = &f_scrub_gm[frame][offset_y + offset_x];
    const uint16_t *src2 = &gain_map[ offset_y + offset_x];

    size_t row_bytes = K4_SIZE * sizeof(uint16_t);

    size_t src_stride = FRAME_W * sizeof(uint16_t);
    size_t dst_stride = row_bytes;
    size_t repeat = K4_SIZE;
    
    snrt_dma_start_2d(frames16[0], src1, row_bytes, dst_stride, src_stride, repeat);
    snrt_dma_start_2d(frames16[1], src2, row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

void cpy_f_2x2_bin_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    size_t offset_x = sf_x * K5_SIZE_IN;
    size_t offset_y = sf_y * K5_SIZE_IN * FRAME_W;

    const uint16_t *src1 = &f_gain_gm[frame][offset_y + offset_x];

    size_t row_bytes = K5_SIZE_IN * sizeof(uint16_t);

    size_t src_stride = FRAME_W * sizeof(uint16_t);
    size_t dst_stride = row_bytes;
    size_t repeat = K5_SIZE_IN;
    
    snrt_dma_start_2d(frames16[0], src1, row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

void cpy_f_coadd_in  (uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    size_t offset_x = sf_x * K6_SIZE;
    size_t offset_y = sf_y * K6_SIZE * FRAME_W/2;

    const uint32_t *src1 = &zero32[offset_y + offset_x];
    const uint32_t *src2 = &f_2x2_bin_gm[frame][offset_y + offset_x];

    size_t row_bytes = K6_SIZE * sizeof(uint32_t);

    size_t src_stride = FRAME_W * sizeof(uint32_t);
    size_t dst_stride = row_bytes;
    size_t repeat = K6_SIZE;
    
    snrt_dma_start_2d(frame32_a, src1, row_bytes, dst_stride, src_stride, repeat);
    snrt_dma_start_2d(frame32_b, src2, row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

void cpy_f_offset_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    size_t offset_x = sf_x * K1_SIZE;
    size_t offset_y = sf_y * K1_SIZE * FRAME_W;

    uint16_t *dst = &out_frame[offset_y + offset_x];
    
    size_t row_bytes = K1_SIZE * sizeof(uint16_t);

    size_t dst_stride = FRAME_W * sizeof(uint16_t);
    size_t src_stride = row_bytes;
    size_t repeat = K1_SIZE;
    
//    printf("Copying 0x%0x to 0x%0x src_stride %u dst_stride %u len %u repeat %u\n", frames16[0], dst, src_stride, dst_stride, row_bytes, repeat);

    snrt_dma_txid_t a = snrt_dma_start_2d(dst, frames16[0], row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait(a);
}

void cpy_f_mask_replace_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    size_t offset_x = sf_x * K2_SIZE;
    size_t offset_y = sf_y * K2_SIZE * FRAME_W;

    uint16_t *dst = &out_frame[offset_y + offset_x];
    
    size_t row_bytes = K2_SIZE * sizeof(uint16_t);

    size_t dst_stride = FRAME_W * sizeof(uint16_t);
    size_t src_stride = row_bytes;
    size_t repeat = K2_SIZE;
    
    snrt_dma_start_2d(dst, frames16[0], row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

void cpy_f_scrub_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    size_t offset_x = sf_x * K3_SIZE;
    size_t offset_y = sf_y * K3_SIZE * FRAME_W;

    uint16_t *dst = &tempFrame16[offset_y + offset_x];
    
    size_t row_bytes = K3_SIZE * sizeof(uint16_t);

    size_t dst_stride = FRAME_W * sizeof(uint16_t);
    size_t src_stride = row_bytes;
    size_t repeat = K3_SIZE;
    
    snrt_dma_start_2d(dst, frames16[frame], row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

void cpy_f_gain_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    size_t offset_x = sf_x * K4_SIZE;
    size_t offset_y = sf_y * K4_SIZE * FRAME_W;

    uint16_t *dst = &tempFrame16[offset_y + offset_x];
    
    size_t row_bytes = K4_SIZE * sizeof(uint16_t);

    size_t dst_stride = FRAME_W * sizeof(uint16_t);
    size_t src_stride = row_bytes;
    size_t repeat = K4_SIZE;
    
    snrt_dma_start_2d(dst, frames16[0], row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

void cpy_f_2x2_bin_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    size_t offset_x = sf_x * K5_SIZE_OUT;
    size_t offset_y = sf_y * K5_SIZE_OUT * FRAME_W/2;

    uint32_t *dst = &tempFrame32[offset_y + offset_x];
    
    size_t row_bytes = K5_SIZE_OUT * sizeof(uint32_t);

    size_t dst_stride = FRAME_W/2 * sizeof(uint32_t);
    size_t src_stride = row_bytes;
    size_t repeat = K5_SIZE_OUT;
    
    snrt_dma_start_2d(dst, frame32_a, row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

void cpy_f_coadd_out  (uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    size_t offset_x = sf_x * K6_SIZE;
    size_t offset_y = sf_y * K6_SIZE * FRAME_W/2;

    uint32_t *dst = &tempFrame32[offset_y + offset_x];
    
    size_t row_bytes = K6_SIZE * sizeof(uint32_t);

    size_t dst_stride = FRAME_W/2 * sizeof(uint32_t);
    size_t src_stride = row_bytes;
    size_t repeat = K6_SIZE;
    
    snrt_dma_start_2d(dst, frame32_a, row_bytes, dst_stride, src_stride, repeat);

    snrt_dma_wait_all();
}

#else

void cpy_f_offset_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_mask_replace_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_scrub_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_gain_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_2x2_bin_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_coadd_in(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}

void cpy_f_offset_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_mask_replace_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_scrub_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_gain_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_2x2_bin_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}
void cpy_f_coadd_out(uint32_t frame, uint32_t sf_x, uint32_t sf_y){
    UNUSED(frame);
    UNUSED(sf_x);
    UNUSED(sf_y);
}

#endif

#endif