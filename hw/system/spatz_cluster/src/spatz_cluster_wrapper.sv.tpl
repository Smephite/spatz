// Copyright 2021 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

${disclaimer}

<%def name="core_cfg(prop)">\
  % for c in cfg['cores']:
${c[prop]}${', ' if not loop.last else ''}\
  % endfor
</%def>\

<%def name="core_cfg_flat(prop)">\
${cfg['nr_cores']}'b\
  % for c in cfg['cores'][::-1]:
${int(c[prop])}\
  % endfor
</%def>\

<%def name="core_isa(isa)">\
${cfg['nr_cores']}'b\
  % for c in cfg['cores'][::-1]:
${int(getattr(c['isa_parsed'], isa))}\
  % endfor
</%def>\

`include "axi/typedef.svh"

// verilog_lint: waive-start package-filename
package ${cfg['pkg_name']};

  ///////////
  //  AXI  //
  ///////////

  // General AXI Parameters
  localparam int unsigned AddrWidth = ${cfg['addr_width']};
  localparam int unsigned AtomicIdWidth = ${cfg['atomic_id_width']};
  
  // Cluster AXI Parameters
  localparam int unsigned NarrowDataWidth = ${cfg['data_width']};
  localparam int unsigned NarrowStrbWidth = NarrowDataWidth / 8;
  localparam int unsigned NarrowIdWidthIn = ${cfg['id_width_in']};
  localparam int unsigned NarrowUserWidth = ${cfg['user_width']};

  // DMA Axi Parameters
  localparam int unsigned WideDataWidth = ${cfg['dma_data_width']};
  localparam int unsigned WideStrbWidth = WideDataWidth / 8;
  localparam int unsigned WideIdWidthIn = ${cfg['dma_id_width_in']};
  localparam int unsigned WideUserWidth = ${cfg['dma_user_width']};

  // Internal AXI parameters

  // Core Request, SoC Request
  localparam int unsigned NrNarrowMasters = 2;
  localparam int unsigned NarrowIdWidthOut = NarrowIdWidthIn + $clog2(NrNarrowMasters);

  // Core Request, DMA, I$
  localparam int unsigned NrWideMasters  = 3;
  localparam int unsigned WideIdWidthOut = WideIdWidthIn + $clog2(NrWideMasters);

  // AXI typedef
  typedef logic [AddrWidth-1:0]     axi_addr_t;
  typedef logic [AtomicIdWidth-1:0] axi_atomic_id_t;

  typedef logic [NarrowDataWidth-1:0]  axi_narrow_data_t;
  typedef logic [NarrowStrbWidth-1:0]  axi_narrow_strb_t;
  typedef logic [NarrowUserWidth-1:0]  axi_narrow_user_t;
  typedef logic [NarrowIdWidthIn-1:0]  axi_narrow_id_in_t;
  typedef logic [NarrowIdWidthOut-1:0] axi_narrow_id_out_t;

  typedef logic [WideDataWidth-1:0]  axi_wide_data_t;
  typedef logic [WideStrbWidth-1:0]  axi_wide_strb_t;
  typedef logic [WideUserWidth-1:0]  axi_wide_user_t;
  typedef logic [WideIdWidthIn-1:0]  axi_wide_id_in_t;
  typedef logic [WideIdWidthOut-1:0] axi_wide_id_out_t;

  // TODO propergate new types down throughout this file
  // TODO propergate new types into spatz_cluster.sv
  // Add ID (+ User?) Width conversion inside cluster wrapper to match spatz_cluster outgoing/incoming (as just defined)
  //   and wrapper outgoing/incoming ID (+User?) width with top level.
  //  

  `AXI_TYPEDEF_ALL(spatz_axi_narrow_in, axi_addr_t, axi_narrow_id_in_t, axi_narrow_data_t, axi_narrow_strb_t, axi_narrow_user_t)
  `AXI_TYPEDEF_ALL(spatz_axi_narrow_out, axi_addr_t, axi_narrow_id_out_t, axi_narrow_data_t, axi_narrow_strb_t, axi_narrow_user_t)

  `AXI_TYPEDEF_ALL(spatz_axi_wide_in, axi_addr_t, axi_wide_id_in_t, axi_wide_data_t, axi_wide_strb_t, axi_wide_user_t)
  `AXI_TYPEDEF_ALL(spatz_axi_wide_out, axi_addr_t, axi_wide_id_out_t, axi_wide_data_t, axi_wide_strb_t, axi_wide_user_t)

  ////////////////////
  //  Spatz Cluster //
  ////////////////////

  localparam int unsigned NumCores = ${cfg['nr_cores']};

  localparam int unsigned ICacheLineWidth = ${cfg['icache']['cacheline']};
  localparam int unsigned ICacheLineCount = ${cfg['icache']['depth']};
  localparam int unsigned ICacheWays = ${cfg['icache']['ways']};

  localparam int unsigned TCDMStartAddr = ${to_sv_hex(cfg['cluster_base_addr'], cfg['addr_width'])};
  localparam int unsigned TCDMSize      = ${to_sv_hex(cfg['tcdm']['size'] * 1024, cfg['addr_width'])};

  localparam int unsigned PeriStartAddr = TCDMStartAddr + TCDMSize;

  localparam int unsigned BootAddr      = ${to_sv_hex(cfg['boot_addr'], cfg['addr_width'])};

  function automatic snitch_pma_pkg::rule_t [snitch_pma_pkg::NrMaxRules-1:0] get_cached_regions();
    automatic snitch_pma_pkg::rule_t [snitch_pma_pkg::NrMaxRules-1:0] cached_regions;
    cached_regions = '{default: '0};
% for i, cp in enumerate(cfg['pmas']['cached']):
    cached_regions[${i}] = '{base: ${to_sv_hex(cp[0], cfg['addr_width'])}, mask: ${to_sv_hex(cp[1], cfg['addr_width'])}};
% endfor
    return cached_regions;
  endfunction

  localparam snitch_pma_pkg::snitch_pma_t SnitchPMACfg = '{
      NrCachedRegionRules: ${len(cfg['pmas']['cached'])},
      CachedRegion: get_cached_regions(),
      default: 0
  };

  localparam fpnew_pkg::fpu_implementation_t FPUImplementation [NumCores] = '{
  % for c in cfg['cores']:
    '{
        PipeRegs: // FMA Block
                  '{
                    '{  ${cfg['timing']['lat_comp_fp32']}, // FP32
                        ${cfg['timing']['lat_comp_fp64']}, // FP64
                        ${cfg['timing']['lat_comp_fp16']}, // FP16
                        ${cfg['timing']['lat_comp_fp8']}, // FP8
                        ${cfg['timing']['lat_comp_fp16_alt']}, // FP16alt
                        ${cfg['timing']['lat_comp_fp8_alt']}  // FP8alt
                      },
                    '{1, 1, 1, 1, 1, 1},   // DIVSQRT
                    '{${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']},
                      ${cfg['timing']['lat_noncomp']}},   // NONCOMP
                    '{${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']},
                      ${cfg['timing']['lat_conv']}},   // CONV
                    '{${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']},
                      ${cfg['timing']['lat_sdotp']}}    // DOTP
                    },
        UnitTypes: '{'{fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED,
                       fpnew_pkg::MERGED},  // FMA
                    '{fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED}, // DIVSQRT
//                    '{fpnew_pkg::MERGED,
//                        fpnew_pkg::MERGED,
//                        fpnew_pkg::MERGED,
//                        fpnew_pkg::MERGED,
//                        fpnew_pkg::MERGED,
//                        fpnew_pkg::MERGED}, // DIVSQRT                        
                    '{fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL,
                        fpnew_pkg::PARALLEL}, // NONCOMP
                    '{fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED},   // CONV
% if c["xfdotp"]:
                    '{fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED,
                        fpnew_pkg::MERGED}},  // DOTP //should be removed for 32-bit version
% else:
                    '{fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED,
                        fpnew_pkg::DISABLED}}, // DOTP
% endif
        PipeConfig: fpnew_pkg::${cfg['timing']['fpu_pipe_config']}
    }${',\n' if not loop.last else '\n'}\
  % endfor
  };

endpackage
// verilog_lint: waive-stop package-filename

//package fpu0_cfg_pkg;
//  import fpnew_pkg::*;
//  import spatz_cluster_pkg::*;
//
//  function automatic fpu_implementation_t with_merged_ut1
//      (input fpu_implementation_t base);
//    fpu_implementation_t tmp = base;
//    tmp.UnitTypes[1] = '{MERGED, MERGED, MERGED, MERGED, MERGED, MERGED};
//    return tmp;
//  endfunction
//
//  // Build a constant variant once
//  localparam fpu_implementation_t FPUImplementation0 =
//      with_merged_ut1(FPUImplementation[0]);
//endpackage

module ${cfg['name']}_wrapper
 import ${cfg['pkg_name']}::*;
 import fpnew_pkg::fpu_implementation_t;
 import snitch_pma_pkg::snitch_pma_t;
 #(
)(
  input  logic                       clk_i,
  input  logic                       rst_ni,
  input  logic [NumCores-1:0]        debug_req_i,
  input  logic [NumCores-1:0]        meip_i,
  input  logic [NumCores-1:0]        mtip_i,
  input  logic [NumCores-1:0]        msip_i,
  input  logic [9:0]                 hart_base_id_i,
  input  logic [AddrWidth-1:0]       cluster_base_addr_i,
  output logic                       cluster_probe_o,
  input  spatz_axi_narrow_in_req_t   axi_narrow_in_req_i,
  output spatz_axi_narrow_in_resp_t  axi_narrow_in_resp_o,
  output spatz_axi_narrow_out_req_t  axi_narrow_out_req_o,
  input  spatz_axi_narrow_out_resp_t axi_narrow_out_resp_i,

  input  spatz_axi_wide_in_req_t     axi_wide_in_req_i,
  output spatz_axi_wide_in_resp_t    axi_wide_in_resp_o,
  output spatz_axi_wide_out_req_t    axi_wide_out_req_o,
  input  spatz_axi_wide_out_resp_t   axi_wide_out_resp_i
);

  localparam int unsigned NumIntOutstandingLoads   [NumCores] = '{${core_cfg('num_int_outstanding_loads')}};
  localparam int unsigned NumIntOutstandingMem     [NumCores] = '{${core_cfg('num_int_outstanding_mem')}};
  localparam int unsigned NumSpatzOutstandingLoads [NumCores] = '{${core_cfg('num_spatz_outstanding_loads')}};
  localparam int unsigned NumSpatzFPUs             [NumCores] = '{default: ${cfg['n_fpu']}};
  localparam int unsigned NumSpatzIPUs             [NumCores] = '{default: ${cfg['n_ipu']}};


  // Spatz cluster under test.
  spatz_cluster #(
    .PhysicalAddrWidth (AddrWidth),
    .NarrowDataWidth (NarrowDataWidth),
    .NarrowIdWidthIn (NarrowIdWidthIn),
    .NarrowUserWidth (NarrowUserWidth),
    .WideDataWidth (WideDataWidth),
    .WideIdWidthIn (WideIdWidthIn),
    .WideUserWidth (WideUserWidth),
    .BootAddr (${to_sv_hex(cfg['boot_addr'], 32)}),
    .ClusterPeriphSize (${cfg['cluster_periph_size']}),
    .NrCores (${cfg['nr_cores']}),
    .TCDMDepth (${cfg['tcdm']['depth']}),
    .NrBanks (${cfg['tcdm']['banks']}),
    .ICacheLineWidth (${cfg['pkg_name']}::ICacheLineWidth),
    .ICacheLineCount (${cfg['pkg_name']}::ICacheLineCount),
    .ICacheWays (${cfg['pkg_name']}::ICacheWays),
    .FPUImplementation (${cfg['pkg_name']}::FPUImplementation),
    .SnitchPMACfg (${cfg['pkg_name']}::SnitchPMACfg),
    .NumIntOutstandingLoads (NumIntOutstandingLoads),
    .NumIntOutstandingMem (NumIntOutstandingMem),
    .NumSpatzOutstandingLoads (NumSpatzOutstandingLoads),
    .NumSpatzFPUs (NumSpatzFPUs),
    .NumSpatzIPUs (NumSpatzIPUs),
    .Xdma (${core_cfg_flat('xdma')}),
    .DMAAxiReqFifoDepth (${cfg['dma_axi_req_fifo_depth']}),
    .DMAReqFifoDepth (${cfg['dma_req_fifo_depth']}),
    .RegisterOffloadRsp (${int(cfg['timing']['register_offload_rsp'])}),
    .RegisterCoreReq (${int(cfg['timing']['register_core_req'])}),
    .RegisterCoreRsp (${int(cfg['timing']['register_core_rsp'])}),
    .RegisterTCDMCuts (${int(cfg['timing']['register_tcdm_cuts'])}),
    .RegisterExtNarrow (${int(cfg['timing']['register_ext'])}),
    .RegisterExtWide (${int(cfg['timing']['register_ext'])}),
    .XbarLatency (axi_pkg::${cfg['timing']['xbar_latency']}),
    .MaxMstTrans (${cfg['trans']}),
    .MaxSlvTrans (${cfg['trans']}),
    .axi_narrow_in_req_t   ( spatz_axi_narrow_in_req_t   ),
    .axi_narrow_in_resp_t  ( spatz_axi_narrow_in_resp_t  ),
    .axi_narrow_out_req_t  ( spatz_axi_narrow_out_req_t  ),
    .axi_narrow_out_resp_t ( spatz_axi_narrow_out_resp_t ),
    .axi_wide_in_req_t     ( spatz_axi_wide_in_req_t     ),
    .axi_wide_in_resp_t    ( spatz_axi_wide_in_resp_t    ),
    .axi_wide_out_req_t    ( spatz_axi_wide_out_req_t    ),
    .axi_wide_out_resp_t   ( spatz_axi_wide_out_resp_t   )
  ) i_cluster (
    .clk_i,
    .rst_ni,
% if cfg['enable_debug']:
    .debug_req_i,
% else:
    .debug_req_i ('0),
% endif
    .meip_i,
    .mtip_i,
    .msip_i,
% if cfg['tie_ports']:
    .hart_base_id_i (${to_sv_hex(cfg['cluster_base_hartid'], 10)}),
    .cluster_base_addr_i (${to_sv_hex(cfg['cluster_base_addr'], cfg['addr_width'])}),
% else:
    .hart_base_id_i,
    .cluster_base_addr_i,
% endif
    .cluster_probe_o,
    // AXI Slave Port
    .axi_wide_in_req_i,
    .axi_wide_in_resp_o,
    .axi_narrow_in_req_i,
    .axi_narrow_in_resp_o,
    // AXI Master Port
    .axi_wide_out_req_o,
    .axi_wide_out_resp_i,
    .axi_narrow_out_req_o,
    .axi_narrow_out_resp_i
  );

endmodule
