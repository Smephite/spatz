// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51
//
// Author: Matteo Perotti <mperotti@iis.ee.ethz.ch>
//         Basile Bougenot <bbougenot@student.ethz.ch>

#include "float_macros.h"
#include "vector_macros.h"

int main(void) {
  
    enable_vec();
    enable_fp();

    volatile float a = 1.0f;
    volatile float b = -2.0f;
    
    return a / b == -0.5f ? 0 : 1;

}
