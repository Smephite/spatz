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

#ifndef OBPMARK_KERN_H
#define OBPMARK_KERN_H

#include <stddef.h>
#include <stdint.h>

inline void f_offset(uint16_t *a, const uint16_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
  )    __attribute__((always_inline));

  inline void f_coadd(uint32_t *a, const uint32_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
  )   __attribute__((always_inline));

  inline void f_gain(uint16_t *a, const uint16_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
  )    __attribute__((always_inline));

inline uint32_t f_neighbour_masked_sum(
	const uint16_t *frame,
	const uint8_t *mask,
	int x_mid,
	int y_mid,
  const unsigned int width,
  const unsigned int height
	) __attribute__((always_inline));

  inline void f_mask_replace(uint16_t *a, const uint8_t *b,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
)    __attribute__((always_inline));

inline void f_scrub(uint16_t *a, uint16_t** frames,
    unsigned int frame_i,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
)    __attribute__((always_inline));

inline void f_2x2_bin(
    uint16_t *a, uint32_t *b, uint32_t * out_buf,
    const unsigned int width, const unsigned int height,
    const unsigned int x_start, const unsigned int x_end,
    const unsigned int y_start, const unsigned int y_end
	)    __attribute__((always_inline));

#endif