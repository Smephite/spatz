#!/usr/bin/env python3
# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Tim Fischer <fischeti@iis.ee.ethz.ch>

import numpy as np
import argparse
import pathlib
import hjson


SIZE = 1024
SUB_FRAME = 128
np.random.seed(42)


global verbose

def array_to_cstring(a):
    out = "{"

    for u in a.flatten():
        out += "0x{:02x}, ".format(u)

    out = out[:-2] + "}"
    return out

def k1(inputs, offset):
    out = inputs.copy()

    for o in out:
        o -= offset
    return out

def f_neighbour_masked_sumf_neighbour_masked_sum(inputs, mask, i):
    x = i % SUB_FRAME
    y = int(np.floor(i/SUB_FRAME))

    x_start = 0 if x == 0 else -1
    x_end   = 0 if x == SUB_FRAME-1 else 1
    y_start = 0 if y == 0 else -1
    y_end   = 0 if y == SUB_FRAME-1 else 1

    print(f"Replacing at {x} {y}")

    s = 0
    c = 0
    for yy in range(y_start, y_end+1):
        for xx in range(x_start, x_end+1):
            index = (y+yy)*SUB_FRAME+x+xx
            if mask[index] == 0:
                s += inputs[index] 
                c += 1

    return 0 if c == 0 else int(np.floor(s / c))

def k2(inputs, mask):
    out = []
    for i in range(len(inputs)):
        inp = inputs[i].copy().flatten()
        mask = mask.flatten()
        for x in range(len(inp)):
            if mask[x]:
                inp[x] = f_neighbour_masked_sumf_neighbour_masked_sum(inp, mask, x)
        out.append(inp) 
    return out

def read_file(name, dtype, shape = (SIZE, SIZE)):
    file = pathlib.Path(__file__).parent.parent / "data" / "raw" / name
    
    return np.fromfile(file, dtype=dtype).reshape(shape)

def emit_1024_1(**kwargs):
    emit_str = f"""
#pragma once
#include <stdint.h>

#define FRAME_W {SUB_FRAME}
#define FRAME_H {SUB_FRAME}
#define FRAME_NUM 1

    """

    input_frame0 = read_file("1.1-image-data_1024x1024_frame0.bin", np.uint16)[0:SUB_FRAME, 0:SUB_FRAME]
    bad_pixels = read_file("1.1-image-bad_pixels_1024x1024.bin", np.uint8)[0:SUB_FRAME, 0:SUB_FRAME]
    gains = read_file("1.1-image-gains_1024x1024.bin", np.uint16)[0:SUB_FRAME, 0:SUB_FRAME]
    offsets = read_file("1.1-image-offsets_1024x1024.bin", np.uint16)[0:SUB_FRAME, 0:SUB_FRAME]
    scrub = [
        read_file("1.1-image-scrub_t-2_1024x1024.bin", np.uint16)[0:SUB_FRAME, 0:SUB_FRAME],
        read_file("1.1-image-scrub_t-1_1024x1024.bin", np.uint16)[0:SUB_FRAME, 0:SUB_FRAME],
        read_file("1.1-image-scrub_t+1_1024x1024.bin", np.uint16)[0:SUB_FRAME, 0:SUB_FRAME],
        read_file("1.1-image-scrub_t+2_1024x1024.bin", np.uint16)[0:SUB_FRAME, 0:SUB_FRAME],
    ]

    print(max(offsets.flatten()))
    
    gm_k1 = k1([input_frame0], offsets)
    gm_k2 = k2(gm_k1, bad_pixels)

    emit_str += f"""
static uint16_t input_frames[FRAME_NUM][FRAME_W*FRAME_H] __attribute__((section(".data"))) = {{ {array_to_cstring(input_frame0)} }};
static uint16_t scrub_frames[4][FRAME_W*FRAME_H] __attribute__((section(".data"))) = {{
    {array_to_cstring(scrub[0])},
    {array_to_cstring(scrub[1])},
    {array_to_cstring(scrub[2])},
    {array_to_cstring(scrub[3])}
     }};

static uint8_t bad_pixel_map[FRAME_W*FRAME_H] __attribute__((section(".data"))) = {array_to_cstring(bad_pixels)};
static uint16_t offset_map[FRAME_W*FRAME_H] __attribute__((section(".data"))) = {array_to_cstring(offsets)};
static uint16_t gain_map[FRAME_W*FRAME_H] __attribute__((section(".data"))) = {array_to_cstring(gains)};

static uint16_t f_offset_gm[FRAME_NUM][FRAME_W*FRAME_H] __attribute__((section(".data"))) = {{ {array_to_cstring(gm_k1[0])} }};
static uint16_t f_mask_replace_gm[FRAME_NUM][FRAME_W*FRAME_H] __attribute__((section(".data"))) = {{ {array_to_cstring(gm_k2[0])} }};
static uint16_t f_scrub_gm[FRAME_NUM][FRAME_W*FRAME_H] __attribute__((section(".data"))) = {{ {{0}} }};
static uint16_t f_gain_gm[FRAME_NUM][FRAME_W*FRAME_H] __attribute__((section(".data"))) = {{ {{0}} }};
static uint32_t f_2x2_bin_gm[FRAME_NUM][FRAME_W*FRAME_H/4] __attribute__((section(".data"))) = {{ {{0}} }};
static uint32_t f_coadd_gm[FRAME_NUM][FRAME_W*FRAME_H/4] __attribute__((section(".data")))= {{ {{0}} }};
"""

    return emit_str

def emit_header_file(**kwargs):

    file_path = pathlib.Path(__file__).parent.parent / "data"
    emit_str = (
        "// Copyright 2025 ETH Zurich and University of Bologna.\n"
        + "// Licensed under the Apache License, Version 2.0, see LICENSE for details.\n"
        + "// SPDX-License-Identifier: Apache-2.0\n\n"
        + "// This file was generated automatically.\n\n"
    )

    file = file_path / "data_1024_1.h"
    emit_str += emit_1024_1(**kwargs)


    with file.open("w") as f:
        f.write(emit_str)



def main():
    emit_header_file()


if __name__ == "__main__":
    main()
