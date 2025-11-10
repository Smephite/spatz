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

#include "obpmark-kernel.h"

#define UNUSED(x) (void)(x)

void f_offset(uint16_t *a, const uint16_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
  ){
    UNUSED(height);
    for(unsigned int y = y_start; y < y_end; ++y) {
      int todo = x_end - x_start;
      unsigned int vl;

      uint16_t *ptrA = a + y*width + x_start;
      uint16_t *ptrB = (uint16_t*)b + y*width + x_start; 

      while(todo > 0){
        asm volatile (
          "vsetvli %[RET], %[A],e16,m8, ta, ma;"
          : [RET] "=r" (vl)
          : [A] "r" (todo)
          );
      
        asm volatile (
          "vle16.v v0, (%0);"
          "vle16.v v16, (%1);"
          "vsub.vv v0, v0, v16;"
          "vse16.v v0, (%0)"
          :
          : "r"(ptrA), "r"(ptrB)
          : "v0", "v16", "memory"
        );
        
        ptrA += vl;
        ptrB += vl;

        todo -= vl;
      }
    }
  }

void f_coadd(uint32_t *a, const uint32_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
  ){
    UNUSED(height);
    for(unsigned int y = y_start; y < y_end; ++y) {
      int todo = x_end - x_start;
      unsigned int vl;

      uint32_t *ptrA = a + y*width + x_start;
      uint32_t *ptrB = (uint32_t*)b + y*width + x_start; 

      while(todo > 0){
        asm volatile (
          "vsetvli %[RET], %[A],e32,m8, ta, ma;"
          : [RET] "=r" (vl)
          : [A] "r" (todo)
          );
      
        asm volatile (
          "vle32.v v0, (%0);"
          "vle32.v v16, (%1);"
          "vadd.vv v0, v0, v16;"
          "vse32.v v0, (%0)"
          :
          : "r"(ptrA), "r"(ptrB)
          : "v0", "v16", "memory"
        );
        
        ptrA += vl;
        ptrB += vl;

        todo -= vl;
      }
    }
  }

void f_gain(uint16_t *a, const uint16_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
  ){
    UNUSED(height);
    for(unsigned int y = y_start; y < y_end; ++y) {
      int todo = x_end - x_start;
      unsigned int vl;

      uint16_t *ptrA = a + y*width + x_start;
      uint16_t *ptrB = (uint16_t*)b + y*width + x_start; 

      while(todo > 0){
        asm volatile (
          "vsetvli %[RET], %[A],e16,m8, ta, ma;"
          : [RET] "=r" (vl)
          : [A] "r" (todo)
          );
      
        asm volatile (
          "vle16.v v0, (%0);"
          "vle16.v v16, (%1);"
          "vmulh.vv v0, v0, v16;"
          "vse16.v v0, (%0)"
          :
          : "r"(ptrA), "r"(ptrB)
          : "v0", "v16", "memory"
        );
        
        ptrA += vl;
        ptrB += vl;

        todo -= vl;
      }
    }
  }

uint32_t f_neighbour_masked_sum(
	const uint16_t *frame,
	const uint8_t *mask,
	int x_mid,
	int y_mid,
  const unsigned int width,
  const unsigned int height
	)
{
	int x,y;
	unsigned int n_sum=0;
	uint32_t sum=0;
	uint32_t mean; 

	int x_start	= x_mid == 0 ? 0 : -1;
	int x_stop	= x_mid == ((int)width-1) ? 0 : 1;
	int y_start	= y_mid == 0 ? 0 : -1;
	int y_stop	= y_mid == ((int)height-1) ? 0 : 1;

	/* Calculate unweighted sum of good pixels in 3x3 neighbourhood (can be smaller if on edge or corner). */
	for(y=y_start; y<(y_stop+1); y++)
	{
		for(x=x_start; x<(x_stop+1); x++)
		{
        //unsigned int index = (y+y_mid)*width+(x+x_mid);
      
        /* Only include good pixels */
        if(mask[index] == 0)
        {
          sum += frame[index];
          ++n_sum;
        }
		}
	}

	/* Calculate mean of summed good pixels */
	//if (y_mid == 541 && x_mid == 140){printf("POS s x %d y %d value %u:%u\n",y_mid, x_mid, sum, n_sum);}
	mean = n_sum == 0 ? 0 : sum / n_sum;

	return mean;
}

void f_mask_replace(uint16_t *a, const uint8_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
){
    UNUSED(height);
    for(unsigned int y = y_start; y < y_end; ++y) {
      for(unsigned int x = x_start; x < x_end; ++x) {
        unsigned int index = y*width+x;
        if(b[index])
        {
          a[index] = f_neighbour_masked_sum(a, b, x, y, width, height);
        }
      }  
    }
}

void f_scrub(uint16_t *a, uint16_t** frames,
    unsigned int frame_i,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
){
  UNUSED(height);
    for(unsigned int y = y_start; y < y_end; ++y) {
      int todo = x_end - x_start;
      unsigned int vl;

      uint16_t* m2 = &frames[frame_i-2][y*width+x_start];
      uint16_t* m1 = &frames[frame_i-1][y*width+x_start];
      uint16_t* p1 = &frames[frame_i+1][y*width+x_start];
      uint16_t* p2 = &frames[frame_i+2][y*width+x_start];

      while(todo > 0){
        asm volatile (
          "vsetvli %[RET], %[A],e16,m4, ta, ma;"
          : [RET] "=r" (vl)
          : [A] "r" (todo)
          );
      
        // sum f[t-2]+f[t-1]+f[t+1]+f[t+2]
        asm volatile (
          "vle32.v v4, (%0);"
          "vle32.v v8, (%1);"
          "vle32.v v12, (%2);"
          "vle32.v v16, (%3);"
          "vle32.v v20, (%4);"

          // Fancy mean (vaaddu) is not supported in spatz..
          //"vaaddu.vv v24, v4, v8;" // (f(t-2) + f(t-1)) / 2
          //"vaaddu.vv v28, v12, v16;" // (f(t+2) + f(t+1)) / 2
          //"vadd.vv v24, v24, v28;" // ((f[t-2] + f[t-1])/2 + (f[t+2] + f[t+1])/2)/2
          //"vsrl.vi v28, v28, 1;" // ((f[t-2] + f[t-1])/2 + (f[t+2] + f[t+1])/2)/2
          // v24 contains mean * 2, which is prev call "thr"
          // v28 contains mean
          "vadd.vv v24, v4, v8;"
          "vadd.vv v28, v12, v16;"
          "vadd.vv v28, v24, v28;"
          "vsrl.vi v24, v28, 1;" // mean * 2 (/4 * 2)
          "vsrl.vi v28, v24, 1;" // mean  ( / 4)

          "vmsltu.vv v0, v20, v4;"     // mask v0 = (v4 > v20) -> f[i] > thr
          // Who needs this instruction in spatz anyhow?
          //"vmerge.vvm v4, v4, v28, v0;"   // v4[i] = (v4>v28)?v28[i]:v4[i]
          "vadd.vi v4, v28, 0, v0.t;"
          "vse32.v v16, (%0);"

          :
          :
          "r"(a), "r"(m1), "r"(m2), "r"(p1), "r"(p2)
          : "v0", "v4", "v8", "v12", "v16", "v20", "v24", "v28", "memory"
        );


        a  += vl;
        m2 += vl;
        m1 += vl;
        p2 += vl;
        p1 += vl;

        todo -= vl;
      }
    }
}

void f_2x2_bin(
    uint16_t *a, uint32_t *b, uint32_t * out_buf,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
	)
{


	if(width != 0 || height != 0) // must be multiple of 2
		return;
	
    for(unsigned int y = y_start; y < y_end; y+=2) {
      int todo = x_end - x_start;
      unsigned int vl;

      
      uint16_t *row0 = a + y * width;
      uint16_t *row1 = a + (y + 1) * width;
      uint32_t *out_row = b + (y / 2) * width/2;

      size_t x = 0;

      while(todo > 0){
        // Set vector length for 16-bit elements
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(todo));
        asm volatile(

          "vle16.v v0, (%0);"   // row0
          
          // Horizontal sum
			    "vslide1down.vx v8, v0, x0;"
			    "vwadd.vv v16, v8, v0;"
        
          // v16 constains first row

          "vle16.v v0, (%1);"   // row1
			  	"vslide1down.vx v8, v0, x0;"
			    "vwadd.vv v24, v8, v0;" 

          // v24 constains 2nd row
				  "vsetvli x0, %2, e32, m2, ta, ma;"
                
				  "vadd.vv v0, v16, v24;"     // every 2nd entry in v0 contains sum of 2x2 blocks
                :
                : "r"(row0 + x), "r"(row1 + x), "r"(vl)
                : "v0", "v8", "v16", "v24"
          );

			// v6 contains our wanted data but only every 2nd entry
			 asm volatile(
			 	"vse32.v v0, (%0)"
			 	:: "r"(out_buf)
			 );

			 for(uint32_t i = 0; i < vl; ++i){
			 	*(out_row + i) = out_buf[i*2];
			 }
        
        x += vl;
        todo -= vl;
      }
    }
}

