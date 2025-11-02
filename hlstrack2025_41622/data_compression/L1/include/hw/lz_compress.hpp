/*
 * (c) Copyright 2019-2022 Xilinx, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef _XFCOMPRESSION_LZ_COMPRESS_HPP_
#define _XFCOMPRESSION_LZ_COMPRESS_HPP_

/**
 * @file lz_compress.hpp
 * @brief Header for modules used in LZ4 and snappy compression kernels.
 *
 * This file is part of Vitis Data Compression Library.
 */
#include "compress_utils.hpp"
#include "hls_stream.h"

#include <ap_int.h>
#include <assert.h>
#include <stdint.h>

namespace xf {
namespace compression {

/**
 * @brief This module reads input literals from stream and updates
 * match length and offset of each literal.
 * OPTIMIZED VERSION for higher performance and lower latency
 *
 * @tparam MATCH_LEN match length
 * @tparam MIN_MATCH minimum match
 * @tparam LZ_MAX_OFFSET_LIMIT maximum offset limit
 * @tparam MATCH_LEVEL match level
 * @tparam MIN_OFFSET minimum offset
 * @tparam LZ_DICT_SIZE dictionary size
 *
 * @param inStream input stream
 * @param outStream output stream
 * @param input_size input size
 */
template <int MATCH_LEN,
          int MIN_MATCH,
          int LZ_MAX_OFFSET_LIMIT,
          int MATCH_LEVEL = 6,
          int MIN_OFFSET = 1,
          int LZ_DICT_SIZE = 1 << 12,
          int LEFT_BYTES = 64>
void lzCompress(hls::stream<ap_uint<8> >& inStream, hls::stream<ap_uint<32> >& outStream, uint32_t input_size) {
    const int c_dictEleWidth = (MATCH_LEN * 8 + 24);
    typedef ap_uint<MATCH_LEVEL * c_dictEleWidth> uintDictV_t;
    typedef ap_uint<c_dictEleWidth> uintDict_t;

    if (input_size == 0) return;
    
    // Dictionary + epoch-based lazy invalidation
    static uintDictV_t dict[LZ_DICT_SIZE];
    static ap_uint<8> dictEpoch[LZ_DICT_SIZE];
    static ap_uint<8> epochId = 0;
#pragma HLS BIND_STORAGE variable = dict type = ram_2p impl = bram
    // compute resetValue for logical empty state
    uintDictV_t resetValue = 0;
    for (int i = 0; i < MATCH_LEVEL; i++) {
#pragma HLS UNROLL
        resetValue.range((i + 1) * c_dictEleWidth - 1, i * c_dictEleWidth + MATCH_LEN * 8) = -1;
    }
    // start of a new block: advance epoch, rarely fall back to full clear on wrap
    epochId++;
    if (epochId == 0) {
        // epoch wrapped, perform a real clear to avoid stale resurrection
    dict_flush:
        for (int i = 0; i < LZ_DICT_SIZE; i++) {
#pragma HLS PIPELINE II = 1
            dict[i] = resetValue;
            dictEpoch[i] = 0;
        }
        epochId = 1;
    }

    uint8_t present_window[MATCH_LEN];
#pragma HLS ARRAY_PARTITION variable = present_window complete
    // OPTIMIZED: Parallel initialization to reduce latency
    for (uint8_t i = 1; i < MATCH_LEN; i++) {
#pragma HLS UNROLL  // Changed from PIPELINE off to UNROLL for parallel init
        present_window[i] = inStream.read();
    }
    
lz_compress:
    for (uint32_t i = MATCH_LEN - 1; i < input_size - LEFT_BYTES; i++) {
#pragma HLS PIPELINE II = 1
#pragma HLS dependence variable = dict inter false
        uint32_t currIdx = i - MATCH_LEN + 1;
        
        // OPTIMIZATION: Pre-read next character to reduce critical path
        ap_uint<8> nextChar = inStream.read();
        
        // OPTIMIZATION: Parallel window shift - unroll for better timing
        for (int m = 0; m < MATCH_LEN - 1; m++) {
#pragma HLS UNROLL
            present_window[m] = present_window[m + 1];
        }
        present_window[MATCH_LEN - 1] = nextChar;

        // OPTIMIZED: Direct hash calculation without intermediate variables
        uint32_t hash = ((present_window[0] ^ (present_window[1] << 3)) ^ 
                        ((present_window[2] << 6) ^ ((MIN_MATCH >= 4) ? (present_window[3] << 9) : 0))) 
                        & (LZ_DICT_SIZE - 1);

        // OPTIMIZED: Dictionary lookup with epoch gating and write combined
        uintDictV_t dictReadRaw = dict[hash];
        uintDictV_t dictReadValue = (dictEpoch[hash] == epochId) ? dictReadRaw : resetValue;
        uintDictV_t dictWriteValue = dictReadValue << c_dictEleWidth;
        for (int m = 0; m < MATCH_LEN; m++) {
#pragma HLS UNROLL
            dictWriteValue.range((m + 1) * 8 - 1, m * 8) = present_window[m];
        }
        dictWriteValue.range(c_dictEleWidth - 1, MATCH_LEN * 8) = currIdx;
        dict[hash] = dictWriteValue;
        dictEpoch[hash] = epochId;

        // Match search and Filtering - LATENCY OPTIMIZED
        uint8_t match_length = 0;
        uint32_t match_offset = 0;
        
        // OPTIMIZED: Full unroll to eliminate loop latency
        for (int l = 0; l < MATCH_LEVEL; l++) {
#pragma HLS UNROLL  // Changed from factor=3 to full unroll
            uintDict_t compareWith = dictReadValue.range((l + 1) * c_dictEleWidth - 1, l * c_dictEleWidth);
            uint32_t compareIdx = compareWith.range(c_dictEleWidth - 1, MATCH_LEN * 8);
            
            // Parallel byte comparison for latency
            ap_uint<MATCH_LEN> match_bits = 0;
            for (int m = 0; m < MATCH_LEN; m++) {
#pragma HLS UNROLL
                match_bits[m] = (present_window[m] == compareWith.range((m + 1) * 8 - 1, m * 8));
            }
            
            // Priority encoder for match length
            uint8_t len = 0;
            if (match_bits[0]) {
                len = 1;
                if (match_bits[1] && match_bits[2] && match_bits[3]) {
                    len = 4;
                    if (match_bits[4] && match_bits[5]) {
                        len = 6;
                    } else if (match_bits[4]) {
                        len = 5;
                    }
                } else if (match_bits[1] && match_bits[2]) {
                    len = 3;
                } else if (match_bits[1]) {
                    len = 2;
                }
            }
            
            // OPTIMIZED: Direct validation to reduce latency
            uint32_t offset_diff = currIdx - compareIdx;
            bool valid = (len >= MIN_MATCH) && (compareIdx < currIdx) && 
                        (offset_diff < LZ_MAX_OFFSET_LIMIT) && (offset_diff > MIN_OFFSET);
            
            // Optimize nested condition check - direct calculation
            if (valid && (len == 3) && ((offset_diff - 1) > 4096)) {
                len = 0;
            } else if (!valid) {
                len = 0;
            }
            
            if ((len > match_length)) {
                match_length = len;
                match_offset = offset_diff - 1;
            }
        }
        
        // OUTPUT generation
        ap_uint<32> outValue = 0;
        outValue.range(7, 0) = present_window[0];
        outValue.range(15, 8) = match_length;
        outValue.range(31, 16) = match_offset;
        outStream << outValue;
    }

lz_compress_leftover:
    for (int m = 1; m < MATCH_LEN; m++) {
#pragma HLS PIPELINE
        ap_uint<32> outValue = 0;
        outValue.range(7, 0) = present_window[m];
        outStream << outValue;
    }

lz_left_bytes:
    for (int l = 0; l < LEFT_BYTES; l++) {
#pragma HLS PIPELINE
        ap_uint<32> outValue = 0;
        outValue.range(7, 0) = inStream.read();
        outStream << outValue;
    }
}

/**
 * @brief This is stream-in-stream-out module used for lz compression. It reads input literals from stream and updates
 * match length and offset of each literal.
 * OPTIMIZED VERSION for lower latency
 *
 * @tparam MATCH_LEN match length
 * @tparam MIN_MATCH minimum match
 * @tparam LZ_MAX_OFFSET_LIMIT maximum offset limit
 * @tparam MATCH_LEVEL match level
 * @tparam MIN_OFFSET minimum offset
 * @tparam LZ_DICT_SIZE dictionary size
 *
 * @param inStream input stream
 * @param outStream output stream
 */
template <int MAX_INPUT_SIZE = 64 * 1024,
          class SIZE_DT = uint32_t,
          int MATCH_LEN,
          int MIN_MATCH,
          int LZ_MAX_OFFSET_LIMIT,
          int CORE_ID = 0,
          int MATCH_LEVEL = 6,
          int MIN_OFFSET = 1,
          int LZ_DICT_SIZE = 1 << 12,
          int LEFT_BYTES = 64>
void lzCompress(hls::stream<IntVectorStream_dt<8, 1> >& inStream, hls::stream<IntVectorStream_dt<32, 1> >& outStream) {
    const uint16_t c_indxBitCnts = 24;
    const uint16_t c_fifo_depth = LEFT_BYTES + 2;
    const int c_dictEleWidth = (MATCH_LEN * 8 + c_indxBitCnts);
    typedef ap_uint<MATCH_LEVEL * c_dictEleWidth> uintDictV_t;
    typedef ap_uint<c_dictEleWidth> uintDict_t;
    const uint32_t totalDictSize = (1 << (c_indxBitCnts - 1));

#ifndef AVOID_STATIC_MODE
    static bool resetDictFlag = true;
    static uint32_t relativeNumBlocks = 0;
#else
    bool resetDictFlag = true;
    uint32_t relativeNumBlocks = 0;
#endif

    static uintDictV_t dict[LZ_DICT_SIZE];
    static ap_uint<8> dictEpoch[LZ_DICT_SIZE];
    static ap_uint<8> epochId = 0;
#pragma HLS bind_storage variable = dict type = ram_2p impl = bram

    uint8_t present_window[MATCH_LEN];
#pragma HLS ARRAY_PARTITION variable = present_window complete
    hls::stream<uint8_t> lclBufStream("lclBufStream");
#pragma HLS STREAM variable = lclBufStream depth = c_fifo_depth
#pragma HLS BIND_STORAGE variable = lclBufStream type = fifo impl = srl

    IntVectorStream_dt<8, 1> inVal;
    IntVectorStream_dt<32, 1> outValue;

    // Build a constant resetValue to represent logically empty dict lanes
    ap_uint<MATCH_LEVEL* c_dictEleWidth> resetValueConst = 0;
    for (int i = 0; i < MATCH_LEVEL; i++) {
#pragma HLS UNROLL
        resetValueConst.range((i + 1) * c_dictEleWidth - 1, i * c_dictEleWidth + MATCH_LEN * 8) = -1;
    }

    while (true) {
        uint32_t iIdx = 0;
        
        if (resetDictFlag) {
            ap_uint<MATCH_LEVEL* c_dictEleWidth> resetValue = 0;
            for (int i = 0; i < MATCH_LEVEL; i++) {
#pragma HLS UNROLL
                resetValue.range((i + 1) * c_dictEleWidth - 1, i * c_dictEleWidth + MATCH_LEN * 8) = -1;
            }
            // epoch advance at block-start; on wrap, perform full clear once
            epochId++;
            if (epochId == 0) {
            dict_flush:
                for (int i = 0; i < LZ_DICT_SIZE; i++) {
#pragma HLS PIPELINE II = 1
                    dict[i] = resetValue;
                    dictEpoch[i] = 0;
                }
                epochId = 1;
            }
            resetDictFlag = false;
            relativeNumBlocks = 0;
        } else {
            relativeNumBlocks++;
        }
        
        auto nextVal = inStream.read();
        if (nextVal.strobe == 0) {
            outValue.strobe = 0;
            outStream << outValue;
            break;
        }

    // fill buffer and present_window
    lz_fill_present_win:
        while (iIdx < MATCH_LEN - 1) {
#pragma HLS PIPELINE II = 1
            inVal = nextVal;
            nextVal = inStream.read();
            present_window[++iIdx] = inVal.data[0];
        }

    lz_fill_circular_buf:
        for (uint16_t i = 0; i < LEFT_BYTES; ++i) {
#pragma HLS PIPELINE II = 1
            inVal = nextVal;
            nextVal = inStream.read();
            lclBufStream << inVal.data[0];
        }

        outValue.strobe = 1;

    lz_compress:
        for (; nextVal.strobe != 0; ++iIdx) {
#pragma HLS PIPELINE II = 1
#ifndef DISABLE_DEPENDENCE
#pragma HLS dependence variable = dict inter false
#endif
            // Balanced complex index calculation for reduced latency
            uint32_t base_offset = iIdx + (relativeNumBlocks * MAX_INPUT_SIZE);
            uint32_t currIdx = base_offset - MATCH_LEN + 1;
            
            // Read from FIFO buffer
            auto inValue = lclBufStream.read();
            lclBufStream << nextVal.data[0];
            nextVal = inStream.read();
            
            // Parallel window shift
            for (uint8_t m = 0; m < MATCH_LEN - 1; m++) {
#pragma HLS UNROLL
                present_window[m] = present_window[m + 1];
            }
            present_window[MATCH_LEN - 1] = inValue;

            // OPTIMIZED: Direct hash calculation without intermediate variables
            uint32_t hash = ((present_window[0] ^ (present_window[1] << 3)) ^ 
                            ((present_window[2] << 6) ^ ((MIN_MATCH >= 4) ? (present_window[3] << 9) : 0))) 
                            & (LZ_DICT_SIZE - 1);

            // OPTIMIZED: Dictionary lookup with epoch gating and write combined
            uintDictV_t dictReadRaw = dict[hash];
            uintDictV_t dictReadValue = (dictEpoch[hash] == epochId) ? dictReadRaw : (uintDictV_t)resetValueConst;
            uintDictV_t dictWriteValue = dictReadValue << c_dictEleWidth;
            for (int m = 0; m < MATCH_LEN; m++) {
#pragma HLS UNROLL
                dictWriteValue.range((m + 1) * 8 - 1, m * 8) = present_window[m];
            }
            dictWriteValue.range(c_dictEleWidth - 1, MATCH_LEN * 8) = currIdx;
            dict[hash] = dictWriteValue;
            dictEpoch[hash] = epochId;

            // Match search and Filtering - LATENCY OPTIMIZED
            uint8_t match_length = 0;
            uint32_t match_offset = 0;
            
            // OPTIMIZED: Full unroll to eliminate loop latency
            for (int l = 0; l < MATCH_LEVEL; l++) {
#pragma HLS UNROLL  // Changed from factor=3 to full unroll
                uintDict_t compareWith = dictReadValue.range((l + 1) * c_dictEleWidth - 1, l * c_dictEleWidth);
                uint32_t compareIdx = compareWith.range(c_dictEleWidth - 1, MATCH_LEN * 8);
                
                // Parallel byte comparison for latency
                ap_uint<MATCH_LEN> match_bits = 0;
                for (uint8_t m = 0; m < MATCH_LEN; m++) {
#pragma HLS UNROLL
                    match_bits[m] = (present_window[m] == compareWith.range((m + 1) * 8 - 1, m * 8));
                }
                
                // Priority encoder for match length
                uint8_t len = 0;
                if (match_bits[0]) {
                    len = 1;
                    if (match_bits[1] && match_bits[2] && match_bits[3]) {
                        len = 4;
                        if (match_bits[4] && match_bits[5]) {
                            len = 6;
                        } else if (match_bits[4]) {
                            len = 5;
                        }
                    } else if (match_bits[1] && match_bits[2]) {
                        len = 3;
                    } else if (match_bits[1]) {
                        len = 2;
                    }
                }
                
                // OPTIMIZED: Direct validation to reduce latency
                uint32_t offset_diff = currIdx - compareIdx;
                bool valid = (len >= MIN_MATCH) && (currIdx > compareIdx) && 
                            (compareIdx >= (relativeNumBlocks * MAX_INPUT_SIZE)) &&
                            (offset_diff < LZ_MAX_OFFSET_LIMIT) && (offset_diff > MIN_OFFSET);
                
                // Optimize nested condition check - direct calculation
                if (valid && (len == 3) && ((offset_diff - 1) > 4096)) {
                    len = 0;
                } else if (!valid) {
                    len = 0;
                }
                
                if ((len > match_length)) {
                    match_length = len;
                    match_offset = offset_diff - 1;
                }
            }
            
            outValue.data[0].range(7, 0) = present_window[0];
            outValue.data[0].range(15, 8) = match_length;
            outValue.data[0].range(31, 16) = match_offset;
            outStream << outValue;
        }

        outValue.data[0] = 0;
    lz_compress_leftover:
        for (uint8_t m = 1; m < MATCH_LEN; ++m) {
#pragma HLS PIPELINE II = 1
            outValue.data[0].range(7, 0) = present_window[m];
            outStream << outValue;
        }
        
    lz_left_bytes:
        for (uint16_t l = 0; l < LEFT_BYTES; ++l) {
#pragma HLS PIPELINE II = 1
            outValue.data[0].range(7, 0) = lclBufStream.read();
            outStream << outValue;
        }

        resetDictFlag = ((relativeNumBlocks * MAX_INPUT_SIZE) >= (totalDictSize)) ? true : false;
        outValue.strobe = 0;
        outStream << outValue;
    }
}

} // namespace compression
} // namespace xf
#endif // _XFCOMPRESSION_LZ_COMPRESS_HPP_