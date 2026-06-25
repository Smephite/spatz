// Copyright 2020 ETH Zurich and University of Bologna.
// Solderpad Hardware License, Version 0.51, see LICENSE for details.
// SPDX-License-Identifier: SHL-0.51

// Author: Fabian Schuiki <fschuiki@iis.ee.ethz.ch>
// Author: Florian Zaruba <zarubaf@iis.ee.ethz.ch>

#pragma once
#include "sim.hh"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace sim {

struct GlobalMemory {
    static constexpr size_t ADDR_SHIFT = 12;
    static constexpr size_t PAGE_SIZE = (size_t)1 << ADDR_SHIFT;

    std::unordered_map<uint64_t, std::unique_ptr<uint8_t[]>> pages;
    std::set<uint64_t> touched;

    // A mapping of host memory into Manticore memory.
    struct Mapping {
        uint64_t base;  // manticore memory
        size_t size;
        uint8_t *into;  // host memory
    };
    std::vector<Mapping> mappings;

    uint8_t *find_mapping(uint64_t addr) const {
        for (const auto &m : mappings) {
            if (m.base <= addr && m.base + m.size > addr) {
                return m.into + (addr - m.base);
            }
        }
        return nullptr;
    }

    // Copy a chunk of data into memory.
    void write(size_t addr, size_t len, const uint8_t *data,
               const uint8_t *strb) {
        // std::cout << "[GlobalMemory] Write " << std::hex << addr << std::dec
        //           << " (" << len << " bytes)\n";
        size_t end = addr + len;
        size_t data_idx = 0;
        while (addr < end) {
            size_t byte_start = addr;
            addr >>= ADDR_SHIFT;
            auto &page = pages[addr];
            uint64_t page_idx = addr;
            if (!page) {
                // std::cout << "[TB] Allocate page " << std::hex << (addr <<
                // ADDR_SHIFT) << "\n";
                page = std::make_unique<uint8_t[]>(PAGE_SIZE);
                std::fill(&page[0], &page[PAGE_SIZE], 0);
            }
            // std::cout << "[TB] Write to page " << std::hex << (addr <<
            // ADDR_SHIFT)
            // << "\n";
            addr += 1;
            addr <<= ADDR_SHIFT;
            size_t byte_end = std::min(addr, end);
            bool any_changed = false;
            for (size_t i = byte_start; i < byte_end; i++, data_idx++) {
                if (!strb || strb[data_idx]) {
                    // std::cout << "[TB] Write byte " << std::hex << i << " = "
                    // << (uint32_t)data[data_idx] << "\n";
                    auto host = find_mapping(i);
                    if (host) {
                        *host = data[data_idx];
                    } else {
                        page[i % PAGE_SIZE] = data[data_idx];
                        any_changed = true;
                    }
                }
            }
            if (any_changed) touched.insert(page_idx);
        }
        std::cout << std::dec;
    }

    // Dump all written pages to a binary file.
    // Format: repeated records of { uint64_t base_addr, uint8_t[PAGE_SIZE] data },
    // sorted ascending by address, little-endian.
    void dump(const char *path) {
        std::vector<uint64_t> sorted_pages(touched.begin(), touched.end());
        std::sort(sorted_pages.begin(), sorted_pages.end());
        FILE *f = fopen(path, "wb");
        if (!f) {
            fprintf(stderr, "[TB] Failed to open dump file: %s\n", path);
            return;
        }
        for (uint64_t page_idx : sorted_pages) {
            uint64_t base_addr = page_idx << ADDR_SHIFT;
            fwrite(&base_addr, sizeof(base_addr), 1, f);
            fwrite(pages.at(page_idx).get(), PAGE_SIZE, 1, f);
        }
        fclose(f);
        fprintf(stderr, "[TB] Dumped %zu pages (%zu KiB) to %s\n",
                sorted_pages.size(), sorted_pages.size() * PAGE_SIZE / 1024,
                path);
    }

    // Copy a chunk of data out of the memory.
    void read(size_t addr, size_t len, uint8_t *data) {
        // std::cout << "[GlobalMemory] Read " << std::hex << addr << std::dec
        //           << " (" << len << " bytes)\n";
        size_t end = addr + len;
        size_t data_idx = 0;
        while (addr < end) {
            size_t byte_start = addr;
            addr >>= ADDR_SHIFT;
            auto &page = pages[addr];
            // std::cout << "[TB] Read from page " << std::hex << (addr <<
            // ADDR_SHIFT)
            // << "\n";
            addr += 1;
            addr <<= ADDR_SHIFT;
            size_t byte_end = std::min(addr, end);
            for (size_t i = byte_start; i < byte_end; i++, data_idx++) {
                auto host = find_mapping(i);
                if (host) {
                    data[data_idx] = *host;
                } else {
                    if (page) {
                        // std::cout << "[TB] Read byte " << std::hex << i <<
                        // "\n";
                        data[data_idx] = page[i % PAGE_SIZE];
                    } else {
                        data[data_idx] = 0;
                    }
                }
            }
        }
        std::cout << std::dec;
    }
};

// The global memory all memory ports write into.
extern GlobalMemory MEM;

// Accumulates TCDM words written bank-by-bank from SV and flushes them in
// address order.  Format: uint64_t base_addr, uint64_t total_bytes, then the
// flat byte image in address order.
struct TcdmDump {
    uint64_t base_addr  = 0;
    uint32_t nr_banks   = 0;
    uint32_t depth      = 0;
    uint32_t data_bytes = 0;
    std::string path;
    std::vector<uint8_t> buf;

    void open(const char *p, uint64_t ba, uint32_t nb, uint32_t d,
              uint32_t db) {
        path = p; base_addr = ba; nr_banks = nb; depth = d; data_bytes = db;
        buf.assign((size_t)nb * d * db, 0);
    }

    // bank_idx: global bank index (superbank * banks_per_sb + bank_within_sb)
    void write_word(uint32_t bank_idx, uint32_t word_idx, uint64_t word) {
        size_t off = ((size_t)word_idx * nr_banks + bank_idx) * data_bytes;
        std::memcpy(buf.data() + off, &word, data_bytes);
    }

    void close() {
        FILE *f = fopen(path.c_str(), "wb");
        if (!f) {
            fprintf(stderr, "[TB] Failed to open TCDM dump file: %s\n",
                    path.c_str());
            return;
        }
        uint64_t total = buf.size();
        fwrite(&base_addr, sizeof(base_addr), 1, f);
        fwrite(&total,     sizeof(total),     1, f);
        fwrite(buf.data(), 1, buf.size(), f);
        fclose(f);
        fprintf(stderr, "[TB] Dumped %zu KiB of TCDM to %s\n",
                buf.size() / 1024, path.c_str());
    }
};

extern TcdmDump TCDM_DUMP;

// The boot data generated along with the system RTL.
struct BootData {
    uint64_t boot_addr;
    uint64_t core_count;
    uint64_t hartid_base;
    uint64_t tcdm_start;
    uint64_t tcdm_size;
    uint64_t tcdm_offset;
    uint64_t global_mem_start;
    uint64_t global_mem_end;
};
extern const BootData BOOTDATA;

}  // namespace sim
