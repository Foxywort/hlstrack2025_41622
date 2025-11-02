/*
 * Copyright 2019 Xilinx, Inc.
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
 */

/**
 * @file adler32.hpp
 * @brief header file for adler32.
 * This file part of Vitis Security Library.
 *
 */

#ifndef _XF_SECURITY_ADLER32_HPP_
#define _XF_SECURITY_ADLER32_HPP_

#include <ap_int.h>
#include <hls_stream.h>
#include <hls_math.h>
#if !defined(__SYNTHESIS__)
#include <iostream>
#endif

namespace xf {
namespace security {
namespace internal {

const ap_uint<21> BASE[] = {
    65521,  131042, 196563, 262084, 327605, 393126, 458647,  524168, 589689,
    655210, 720731, 786252, 851773, 917294, 982815, 1048336, 1113857}; /* largest prime smaller than 65536 */

template <int IW, int NW>
struct treeAdd {
    static ap_uint<IW + NW> f(ap_uint<IW> input[1 << NW]) {
#pragma HLS inline
        ap_uint<IW + 1> tmp[1 << (NW - 1)];
#pragma HLS array_partition variable = tmp dim = 1 complete
        for (int i = 0; i < (1 << (NW - 1)); i++) {
#pragma HLS unroll
            tmp[i] = input[i * 2] + input[i * 2 + 1];
        }
        return treeAdd<IW + 1, NW - 1>::f(tmp);
    }
};

template <int IW>
struct treeAdd<IW, 0> {
    static ap_uint<IW + 1> f(ap_uint<IW> input[1]) {
#pragma HLS inline
        return input[0];
    }
};

} // end of namespace internal

/**
 * @brief adler32 computes the Adler-32 checksum of an input data.
 * @tparam W byte number of input data, the value of W includes 1, 2, 4, 8, 16.
 * @param adlerStrm initialize adler32 value
 * @param inStrm messages to be checked
 * @param inLenStrm length of messages to be checked.
 * @param endInLenStrm end flag of inLenStrm
 * @param outStrm checksum result
 * @param end flag of outStrm
 */
template <int W>
void adler32(hls::stream<ap_uint<32> >& adlerStrm,
             hls::stream<ap_uint<W * 8> >& inStrm,
             hls::stream<ap_uint<32> >& inLenStrm,
             hls::stream<bool>& endInLenStrm,
             hls::stream<ap_uint<32> >& outStrm,
             hls::stream<bool>& endOutStrm) {
    bool e = endInLenStrm.read();
    while (!e) {
        ap_uint<32> adler = adlerStrm.read();
        ap_uint<32> len = inLenStrm.read();
        e = endInLenStrm.read();

        ap_uint<32> s1 = adler & 0xffff;
        ap_uint<32> s2 = ((adler >> 16) & 0xffff);
        ap_uint<W * 8> inData;
        for (ap_uint<32> i = 0; i < len / W; i++) {
#pragma HLS PIPELINE II = 1
#pragma HLS EXPRESSION_BALANCE off
#pragma HLS loop_tripcount max = 100 min = 100
            inData = inStrm.read();
            ap_uint<12> sTmp0 = inData.range(7, 0);
            ap_uint<16> sTmp1 = inData.range(7, 0);
            for (int i = 1; i < W; i++) {
                sTmp0 += inData.range(i * 8 + 7, i * 8);
                sTmp1 += sTmp0;
            }

            // DSP48E1 optimization: Use DSP for multiplication and addition in Adler32
            ap_uint<32> mul_result = s1 * W;
#pragma HLS bind_op variable=mul_result op=mul impl=dsp
            ap_uint<32> sTmp2 = mul_result + sTmp1;
#pragma HLS bind_op variable=sTmp2 op=add impl=dsp
            for (int j = 0; j <= W; j++) {
                if (sTmp2 >= internal::BASE[W - j]) {
                    sTmp2 -= internal::BASE[W - j];
                    break;
                }
            }

            ap_uint<32> s2_sum = s2 + sTmp2;
#pragma HLS bind_op variable=s2_sum op=add impl=dsp
            if (s2_sum >= internal::BASE[0]) {
                s2 = s2_sum - (internal::BASE[0] - sTmp2);
#pragma HLS bind_op variable=s2 op=sub impl=dsp
            } else {
                s2 = s2_sum;
            }

            ap_uint<32> s1_sum = s1 + sTmp0;
#pragma HLS bind_op variable=s1_sum op=add impl=dsp
            if (s1_sum >= internal::BASE[0]) {
                s1 = s1_sum - (internal::BASE[0] - sTmp0);
#pragma HLS bind_op variable=s1 op=sub impl=dsp
            } else {
                s1 = s1_sum;
            }
        }

        for (int j = 0; j < len % W; j++) {
#pragma HLS PIPELINE II = 1
#pragma HLS EXPRESSION_BALANCE off
#pragma HLS loop_tripcount max = W min = W
            if (j == 0) inData = inStrm.read();
            s1 += inData(j * 8 + 7, j * 8);
            if (s1 >= internal::BASE[0]) s1 -= internal::BASE[0];
            s2 += s1;
            if (s2 >= internal::BASE[0]) s2 -= internal::BASE[0];
        }

        ap_uint<32> res = (s2 << 16) + s1;
        outStrm.write(res);
        endOutStrm.write(false);
    }
    endOutStrm.write(true);
}

/**
 * @brief adler32 computes the Adler-32 checksum of an input data.
 * @tparam W byte number of input data, the value of W includes 1, 2, 4, 8, 16.
 * @param adlerStrm initialize adler32 value
 * @param inStrm messages to be checked
 * @param inPackLenStrm effective length of each pack from inStrm. inPackLen.range(4,0) = effective len of pack,
 * inPackLen.range(6,5) = 0x1 means end of one message, inPackLen.range(6,5) = 0x2 means end of all message.
 * messages.
 * @param outStrm checksum result
 * @param end flag of outStrm
 */
template <int W>
void adler32(hls::stream<ap_uint<32> >& adlerStrm,
             hls::stream<ap_uint<W * 8> >& inStrm,
             hls::stream<ap_uint<7> >& inPackLenStrm,
             hls::stream<ap_uint<32> >& outStrm,
             hls::stream<bool>& endOutStrm) {
    ap_uint<7> inPackLen = inPackLenStrm.read();
    while (inPackLen[6] != 1) {
        ap_uint<32> adler = adlerStrm.read();

        ap_uint<32> s1 = adler & 0xffff;
        ap_uint<32> s2 = ((adler >> 16) & 0xffff);
        ap_uint<W * 8> inData;

        while (inPackLen[5] == 0) {
#pragma HLS PIPELINE II = 1
#pragma HLS EXPRESSION_BALANCE off
#pragma HLS loop_tripcount max = 100 min = 100
            inData = inStrm.read();
            ap_uint<12> sTmp0 = inData.range(7, 0);
            ap_uint<16> sTmp1 = inData.range(7, 0);
            for (int i = 1; i < W; i++) {
                sTmp0 += inData.range(i * 8 + 7, i * 8);
                sTmp1 += sTmp0;
            }

            // DSP48E1 optimization: Use DSP for multiplication and addition in Adler32
            ap_uint<32> mul_result = s1 * W;
#pragma HLS bind_op variable=mul_result op=mul impl=dsp
            ap_uint<32> sTmp2 = mul_result + sTmp1;
#pragma HLS bind_op variable=sTmp2 op=add impl=dsp
            for (int j = 0; j <= W; j++) {
                if (sTmp2 >= internal::BASE[W - j]) {
                    sTmp2 -= internal::BASE[W - j];
                    break;
                }
            }

            ap_uint<32> s2_sum = s2 + sTmp2;
#pragma HLS bind_op variable=s2_sum op=add impl=dsp
            if (s2_sum >= internal::BASE[0]) {
                s2 = s2_sum - (internal::BASE[0] - sTmp2);
#pragma HLS bind_op variable=s2 op=sub impl=dsp
            } else {
                s2 = s2_sum;
            }

            ap_uint<32> s1_sum = s1 + sTmp0;
#pragma HLS bind_op variable=s1_sum op=add impl=dsp
            if (s1_sum >= internal::BASE[0]) {
                s1 = s1_sum - (internal::BASE[0] - sTmp0);
#pragma HLS bind_op variable=s1 op=sub impl=dsp
            } else {
                s1 = s1_sum;
            }

            inPackLen = inPackLenStrm.read();
        }

        for (int j = 0; j < inPackLen.range(4, 0); j++) {
#pragma HLS PIPELINE II = 1
#pragma HLS EXPRESSION_BALANCE off
#pragma HLS loop_tripcount max = W min = W
            if (j == 0) inData = inStrm.read();
            s1 += inData(j * 8 + 7, j * 8);
            if (s1 >= internal::BASE[0]) s1 -= internal::BASE[0];
            s2 += s1;
            if (s2 >= internal::BASE[0]) s2 -= internal::BASE[0];
        }
        inPackLen = inPackLenStrm.read();

        ap_uint<32> res = (s2 << 16) + s1;
        outStrm.write(res);
        endOutStrm.write(false);
    }
    endOutStrm.write(true);
}

/**
 * @brief adler32 computes the Adler-32 checksum of an input data.
 * @tparam W byte number of input data, the value of W includes 1, 2, 4, 8, 16.
 * @param adlerStrm initialize adler32 value
 * @param inStrm messages to be checked
 * @param inPackLenStrm effective length of each pack from inStrm, 1~W. 0 means end of message
 * @param endInPackLenStrm end flag of inPackLenStrm, 1 "false" for 1 message, 1 "true" means no message anymore.
 * @param outStrm checksum result
 * @param end flag of outStrm
 */
template <int W>
void adler32(hls::stream<ap_uint<32> >& adlerStrm,
             hls::stream<ap_uint<W * 8> >& inStrm,
             hls::stream<ap_uint<5> >& inPackLenStrm,
             hls::stream<bool>& endInPackLenStrm,
             hls::stream<ap_uint<32> >& outStrm,
             hls::stream<bool>& endOutStrm) {
    bool e = endInPackLenStrm.read();
    while (!e) {
        ap_uint<32> adler = adlerStrm.read();
        ap_uint<5> inPackLen = inPackLenStrm.read();

        ap_uint<32> s1 = adler & 0xffff;
        ap_uint<32> s2 = ((adler >> 16) & 0xffff);
        ap_uint<W * 8> inData;

        while (inPackLen == W) {
#pragma HLS PIPELINE II = 1
#pragma HLS EXPRESSION_BALANCE off
#pragma HLS loop_tripcount max = 100 min = 100
            inPackLen = inPackLenStrm.read();
            inData = inStrm.read();
            ap_uint<12> sTmp0 = inData.range(7, 0);
            ap_uint<16> sTmp1 = inData.range(7, 0);
            for (int i = 1; i < W; i++) {
                sTmp0 += inData.range(i * 8 + 7, i * 8);
                sTmp1 += sTmp0;
            }

            // DSP48E1 optimization: Use DSP for multiplication and addition in Adler32
            ap_uint<32> mul_result = s1 * W;
#pragma HLS bind_op variable=mul_result op=mul impl=dsp
            ap_uint<32> sTmp2 = mul_result + sTmp1;
#pragma HLS bind_op variable=sTmp2 op=add impl=dsp
            for (int j = 0; j <= W; j++) {
                if (sTmp2 >= internal::BASE[W - j]) {
                    sTmp2 -= internal::BASE[W - j];
                    break;
                }
            }

            ap_uint<32> s2_sum = s2 + sTmp2;
#pragma HLS bind_op variable=s2_sum op=add impl=dsp
            if (s2_sum >= internal::BASE[0]) {
                s2 = s2_sum - (internal::BASE[0] - sTmp2);
#pragma HLS bind_op variable=s2 op=sub impl=dsp
            } else {
                s2 = s2_sum;
            }

            ap_uint<32> s1_sum = s1 + sTmp0;
#pragma HLS bind_op variable=s1_sum op=add impl=dsp
            if (s1_sum >= internal::BASE[0]) {
                s1 = s1_sum - (internal::BASE[0] - sTmp0);
#pragma HLS bind_op variable=s1 op=sub impl=dsp
            } else {
                s1 = s1_sum;
            }
        }

        if (inPackLen != 0) {
            for (int j = 0; j < inPackLen; j++) {
#pragma HLS PIPELINE II = 1
#pragma HLS EXPRESSION_BALANCE off
#pragma HLS loop_tripcount max = W min = W
                if (j == 0) inData = inStrm.read();
                s1 += inData(j * 8 + 7, j * 8);
                if (s1 >= internal::BASE[0]) s1 -= internal::BASE[0];
                s2 += s1;
                if (s2 >= internal::BASE[0]) s2 -= internal::BASE[0];
            }
            inPackLen = inPackLenStrm.read();
        }

        ap_uint<32> res = (s2 << 16) + s1;
        outStrm.write(res);
        endOutStrm.write(false);
        e = endInPackLenStrm.read();
    }
    endOutStrm.write(true);
}

} // end of namespace security
} // end of namespace xf
#endif // _XF_SECURITY_ADLER32_HPP_
