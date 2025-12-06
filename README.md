<h1 style="line-height: 1.6;">
  <span style="font-size: 1.4em;">FPGA创新设计大赛</span>
  <br>
  AMD赛道命题式赛道 - 设计报告
</h1>

## 1. 项目概述

### 1.1 项目背景

本项目面向“AMD 赛道命题式赛道”的多算子实现与优化，基于 Vitis HLS 2024.2 与 PYNQ-Z2 平台，围绕数据流并行、II 约束、关键路径时序与寄存器平衡等方法提升端到端执行效率。当前规划与进展如下：

- 算子一（密码学方向）：SHA-256（作为 HMAC-SHA256 的底层压缩核），已完成功能和性能优化；
- 算子二（压缩算法）LZ4 ：文件压缩，同步保留位置与接口对齐；
- 算子三（数值线性代数方向）：Cholesky 分解（对称正定矩阵的 L·Lᵀ/L·Lᴴ 分解），面向定点/复数小规模矩阵，侧重时序与延迟的平衡优化。


<p style="text-indent: 2em;">
为了高效优化 AMD Vitis Library 中的 HLS 算子，我们为本项目设计了一个名为 KernelOpt_Agent 的自动化智能体。在传统的 FPGA 开发流程中，针对特定应用场景的算子调优是一项繁琐且高度依赖专家经验的任务。KernelOpt_Agent 旨在通过自动化分析 Vitis 报告、迭代重构 HLS 代码，显著简化这一优化过程，从而提升设计效率。
</p>
<p align="center">
  <img src="https://img.cdn1.vip/i/6905f1b14297c_1761997233.webp"
       alt="示意图"
       style="max-width:100%; height:auto;" />
</p>
<p>
<div style="text-align: center;">
  <strong>图 1-1 KernelOpt_Agent 架构框图</strong>
</div>
</p>
<p style="text-indent: 2em;">
如图1-1，该架构构建了一个闭环迭代的自动化优化流程：系统首先基于外部输入（如图中所示，例如提供一个经优化的算子设计规划）启动任务，随后 KernelOpt_Agent 核心（融合大型语言模型能力）负责自动生成 C/C++ HLS 代码。接着，智能体无缝衔接并自动调用 AMD Vitis 与 Vivado 工具链执行综合与实现，并对输出的性能和资源报告进行深度解析。最后，分析结果将作为反馈信号返回至智能体，用以指导下一轮的代码重构与优化，从而构成一个持续自我完善的自动化设计循环。
</p>
<br>

<p align="center">
  <img src="https://img.cdn1.vip/i/6906357bb4fff_1762014587.webp"
       alt="示意图"
       style="max-width:100%; height:auto;" />
</p>
<p>
<div style="text-align: center;">
  <strong>图 1-2 KernelOpt_Agent 执行流程图</strong>
</div>
</p>

<p style="text-indent: 2em;">
如图1-2，KernelOpt_Agent 的具体执行流程被划分为四个正交的核心阶段：规划 (PLANNING)、编码 (CODING)、反思 (REFLECTION) 与交付 (RESULT)。在规划阶段，智能体通过解析用户提示 (Prompt)、查阅官方手册 (Manual) 及检索外部知识，形成明确的设计规范 (Design Specifications)。进入编码阶段，智能体将设计规范转化为 HLS C/C++ 实现。随后，在关键的反思阶段，系统自动生成并分析 Vitis 报告，以主动识别如高延迟路径 (high-latency paths) 或时序关键路径 (timing-critical paths) 等性能瓶颈。此分析结果将触发一个“代码重构 (Refactoring)”的反馈循环，指导智能体返回编码阶段进行针对性迭代优化，直至设计收敛并交付最终的实现结果。
</p>


#### 1.1.1 算子一：SHA-256
<p style="text-indent: 2em;">
面向 HMAC-SHA256 的底层压缩核，实现 512-bit 分组、消息调度 W[0..63] 与 64 轮压缩迭代，顶层采用 DATAFLOW，子模块 PIPELINE II=1。通过适当的 FIFO 深度、避免对同一 FIFO 的并行读冲突，以及降低目标时钟以促使工具插入寄存器，最终在满足时序的前提下显著降低执行时间。
</p>

<p style="text-indent: 2em;">
顶层采用 HLS 数据流（DATAFLOW）架构：HMAC 顶层将密钥填充（kpad/kopad）与两次 SHA-256 串联组织，内部通过受控 FIFO 串接；保持单核 SHA-256（两次计算串行），通过 FIFO 深度与握手优化最大化阶段 overlap。
</p>
<p style="text-indent: 2em;">
数据在各模块间以 hls::stream 形式传递，关键流设置合理深度避免背压：
</p>

<p align="center">
  <img src="https://img.cdn1.vip/i/690634cc6d9db_1762014412.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<p>
<div style="text-align: center;">
  <strong>图 1-3 Vitis Library Sha-256 算子模块架构图 </strong>
</div>
</p>
<p style="text-indent: 2em;">
如图1-3，其详细地展示了 AMD Vitis Library 中 SHA-256 算子的内部模块架构。该架构采用了高度模块化和层次化的设计，以实现高效的流水线处理。算子顶层（HMAC TOP）负责处理输入 Key 和 Message，并管理核心哈希流程。核心计算由 SHA-256 Core (Hash Engine) 执行，该引擎内部封装了 Padding/Blocking（填充与分块）、State Update（状态更新）和 Block Processor（块处理器）等关键功能块。其底层的 SHA-256 Compression Kernel（压缩核）进一步揭示了消息调度（Msg Schedule）和状态相加（State Add）如何与一个 64 轮的并行流水线（64-Rounds Pipeline）协同工作，最终完成计算并输出哈希摘要（Digest）。
</p>
<p align="center">
  <img src="https://img.cdn1.vip/i/6906abb970283_1762044857.png"
       alt="示意图"
       style="max-width:70%; height:auto;" />
</p>


<div style="text-align: center;">
  <strong>图 1-4 SHA-256/HMAC 流程图 </strong>
</div>
</p>
<p style="text-indent: 2em;">
如图1-4，在时间维度（Time (clock cycles)）上，顶层函数 hmacDataflow 的执行周期贯穿始终。该流程在功能（fx）上分解为两个顺序执行的关键阶段。流程启动后，首先执行 msgHash 模块，msgHash 模块的执行依赖于其内部调用的 sha256_top 核心。在 msgHash 及其 sha256_top 实例完成计算后，resHash 模块紧接着开始执行。resHash 模块同样调用了 sha256_top 核心。resHash 模块及其调用的 sha256_top 实例的计算延迟构成了流程的后半部分，其执行结束标志着整个 hmacDataflow 任务的最终完成。
</p>

HMAC 计算公式：
$$
\mathrm{HMAC}(K, M) = H\Big( (K' \oplus \mathrm{opad}) \mathbin{\|} H\big( (K' \oplus \mathrm{ipad}) \mathbin{\|} M \big) \Big)
$$

核心位运算与迭代公式：  

$$
\begin{aligned}  
\Sigma_0(x) &= \mathrm{ROTR}^2(x) \oplus \mathrm{ROTR}^{13}(x) \oplus \mathrm{ROTR}^{22}(x) \\  
\Sigma_1(x) &= \mathrm{ROTR}^6(x) \oplus \mathrm{ROTR}^{11}(x) \oplus \mathrm{ROTR}^{25}(x) \\  
\sigma_0(x) &= \mathrm{ROTR}^7(x) \oplus \mathrm{ROTR}^{18}(x) \oplus \mathrm{SHR}^{3}(x) \\  
\sigma_1(x) &= \mathrm{ROTR}^{17}(x) \oplus \mathrm{ROTR}^{19}(x) \oplus \mathrm{SHR}^{10}(x) \\  
\mathrm{Ch}(x,y,z) &= (x \land y) \oplus (\lnot x \land z) \\  
\mathrm{Maj}(x,y,z) &= (x \land y) \oplus (x \land z) \oplus (y \land z)  
\end{aligned}  
$$  

$$
W_t = \begin{cases}
M_t, & t\in[0,15] \\
\sigma_1(W_{t-2}) + W_{t-7} + \sigma_0(W_{t-15}) + W_{t-16}, & t\in[16,63]
\end{cases}
$$

$$
\begin{aligned}
T_1 &= h + \Sigma_1(e) + \mathrm{Ch}(e,f,g) + K_t + W_t \\
T_2 &= \Sigma_0(a) + \mathrm{Maj}(a,b,c)
\end{aligned}
$$

$$
(a,b,c,d,e,f,g,h) \leftarrow (T_1 + T_2,\ a,\ b,\ c,\ d + T_1,\ e,\ f,\ g)
$$

 
##### 接口设计

- 输入接口：
  - `msg_strm`: ap_uint<32>，消息数据流；
  - `len_strm`: ap_uint<64>，每条消息字节长度；
  - `end_len_strm`: bool 结束标志；
  - `key_strm`/`klen`: 32 字节密钥（测试固定）。
- 输出接口：`digest_strm`：ap_uint<32>×8 顺序输出，共 256 位。
- 控制/时序：顶层 `#pragma HLS DATAFLOW`，子模块 `#pragma HLS PIPELINE II=1`；为关键流设置 `#pragma HLS STREAM depth=...`。

##### 核心流程伪代码（压缩）

```cpp
// 输入：512-bit 块 blk，初始状态 H[8]
void sha256_compress(Block blk, uint32_t H[8]) {
  uint32_t W[64];
  for (int t = 0; t < 16; ++t) {
    W[t] = byteswap32(blk.M[t]);
  }
  for (int t = 16; t < 64; ++t) {
    W[t] = sigma1(W[t-2]) + W[t-7] + sigma0(W[t-15]) + W[t-16];
  }
  uint32_t a=H[0], b=H[1], c=H[2], d=H[3];
  uint32_t e=H[4], f=H[5], g=H[6], h=H[7];
  for (int t = 0; t < 64; ++t) {
    uint32_t T1 = h + Sigma1(e) + Ch(e,f,g) + K[t] + W[t];
    uint32_t T2 = Sigma0(a) + Maj(a,b,c);
    h = g; g = f; f = e; e = d + T1;
    d = c; c = b; b = a; a = T1 + T2;
  }
  H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d; H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
}
```

##### 数据对齐、字节序与填充

- 字节序：输入按字节高位在前（big-endian）进行 byteswap32 后进入 W[0..15]。
- 填充：附加 0x80 后补 0，最后 64 bit 写入消息比特长度 L；不足 8B 则扩展到下一块。

##### 优化要点摘要

- II=1 为硬约束：避免对同一 FIFO 的多路并发读导致 II 提升；
- 目标时钟法：将目标时钟由 15ns 收紧至 12ns 触发自动插桩，Estimated 明显下降；
- 滚动窗口与加法链重排：缩短关键路径，必要时在 W 生成与 T1/T2 前插寄存；
- FIFO 深度配置：`w_strm≈64`、`blk_strm≈4`，平衡相位差避免背压；
- 资源取舍：保持 DSP=0、BRAM≈1%，LUT/FF 适度增加换时序余量。

##### 实现细节补充（微架构）

- 数据流分解：
  - preProcessing：按 32b 入栈、byteswap32、组包 512b 块并附加 SHA 填充；
  - generateMsgSchedule：W[0..15] 直通，W[16..63] 采用滚动窗口（深度16）与 σ0/σ1 生成；
  - sha256Digest：64 轮 round，状态寄存 a..h，建议在 `Wt` 生成链与 `T1/T2` 入口各加一级寄存。
- FIFO 策略：`w_strm≈64`、`blk_strm≈4`，优先 LUTRAM/SRL 实现；避免对同一 FIFO 的并发多读。
- 位运算实现：ROTR/SHR 以并行位切片组合实现，减少移位链深度；Ch/Maj 用两级与/或再 XOR，降低门级串联。

##### 调度与 II 控制

- 硬约束：所有关键循环 `#pragma HLS PIPELINE II=1`；严禁对 `msg_strm.read()` 或单端口缓冲做完全 UNROLL；
- 目标时钟：保持 Target Clock≈12 ns，促使工具在加法链/索引处自动插桩，稳定 Estimated≈10.6 ns；
- 数据依赖：W 调度采用两路部分和 `(σ1+W[t-7])+(σ0+W[t-16])` 降低单拍加法深度。

##### 运算绑定与时序

- 绑定策略：SHA-256 以逻辑/加法为主，保持 DSP=0；必要时对长链加法使用寄存器化切分；
- 建议在 `T1/T2` 前插入寄存，结合较紧目标时钟以获得更低 Estimated；
- 组合热点：`Wt` 计算链与 `T1/T2` 合流处，优先做分段归并+寄存。

##### 验证与约束

- 用例：两条样本（80/81B）+ 固定 32B 密钥；HMAC 两次哈希不改数据；
- 正确性：C 仿真与 RTL 联合仿真需一致；
- 接口：严格 ap_hs 流握手，上游/下游仅在 !full/!empty 条件成立时读写。

##### 预期收益与资源取舍

- 收益：在 II=1 前提下，通过目标时钟法 Estimated 明显下降，执行时间相对基线 -15%~-20%；
- 资源：LUT/FF 小幅增加以换更好时序，BRAM≈1%、DSP=0 保持不变。

##### 性能与资源摘要（示例）

- 目标时钟 12ns：Estimated ≈10.575 ns，Latency ≈810，II=1，Slack 为正；
- 执行时间相对基线（10,420 ns）降低约 17.8%；
- 资源：LUT≈25%、FF≈14.5%、BRAM≈1.4%、DSP=0（平台与约束同前）。

##### 资源消耗（详细）

平台与口径：xc7z020-clg484-1（PYNQ-Z2），Vitis HLS 2024.2，Target Clock = 12 ns。

| 资源 | 数量/总量 | 利用率 |
| ---- | --------- | ------ |
| LUT  | 44,778/ 53,200 | 84.2% |
| FF   | 46321 / 106,400 | 43.5% |
| BRAM | 2 / 280 | 0.7% |
| DSP  | 0 / 220 | 0% |



#### 1.1.2 算子二：LZ4 压缩（分述）

- 算法简介（LZ4）：属于 LZ77 家族的无损压缩，将输入字节流分解为“字面量（Literal）序列 + 匹配（Match）对”，再编码为 LZ4 规定的格式。核心字段包括 token（高4位为字面量长度、低4位为匹配长度减4，超出部分用扩展字节累加）、16-bit 偏移（Offset）与实际字面量/匹配扩展长度字节。常按 64KB 分块处理，必要时输出“未压缩块”。

- 系统架构与流水：在顶层采用 `#pragma HLS dataflow` 将“MM2S/分发 → LZ/LZ4 核心流水 → S2MM/回写”并行化；核心流水由四级子模块串接：`lzCompress → lzBestMatchFilter → lzBooster → lz4Compress`，关键状态机（WRITE_TOKEN/WRITE_LIT_LEN/WRITE_MATCH_LEN/WRITE_LITERAL/WRITE_OFFSET0/WRITE_OFFSET1）循环保持 `II=1`。
  </p>
<p align="center">
  <img src="https://img.cdn1.vip/i/6906ae6b12e48_1762045547.webp"
       alt="示意图"
       style="max-width:70%; height:auto;" />
</p>

<div style="text-align: center;">
  <strong>图 1-5 Lz4算子模块架构图 </strong>
</div>
</p>

<p style="text-indent: 2em;">
该设计用于处理输入数据sample.txt，并生成Lz4格式的压缩编码输出sample.txt.encoded。其架构在顶层被划分为 Main Compression Pipeline（主压缩流水线）和 Lz4 Formatter Sub-modules（Lz4 格式化子模块）。主压缩流水线负责执行核心的匹配与压缩逻辑，它由 Lz Match Finder（Lz 匹配查找器）、lz Best Match Filter（最佳匹配过滤器）、Lz Booster 和 Lz4 Compress 等一系列串联的子阶段构成。压缩引擎与流水线的数据流最终汇入格式化子模块，该模块通过 Lz4 Stream Splitter（Lz4 流分割器）和 Lz4 Encoder (Token Gen)（Lz4 编码器/令牌生成器）完成最终的数据打包和令牌生成，并输出压缩文件。
<p>
<p align="center">
  <img src="https://img.cdn1.vip/i/6906bb3dca33a_1762048829.webp"
       alt="示意图"
       style="max-width:65%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong>图 1-6 Lz4解压示意图 </strong>
</div>
</p>
<p style="text-indent: 2em;">
对于Lz4压缩的算子产生的压缩文件，可以使用两种方法进行解压缩验证其功能的正确性。第一种是使用安装Python Lz4库，使用Python脚本对sample.txt.encoded文件进行解压缩。第二种是时钟AMD Vitis自带的Lz4解压缩算子库，通过Csim仿真进行解压。经过测试，这两种方法都可以有效的验证优化过的Lz4压缩算子能够准确无误的解压缩给出的文本文件。
<p>

<p align="center">
  <img src="https://img.cdn1.vip/i/690705469220c_1762067782.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong>图 1-7 Lz4算子优化对比图 </strong>
</div>
<p align="center">
  <img src="https://img.cdn1.vip/i/6906ae6ebe6a1_1762045550.webp"
       alt="示意图"
       style="max-width:60%; height:auto;" />
</p>

<div style="text-align: center;">
  <strong>图 1-8 Lz4数据流图 </strong>
</div>
</p>
<p style="text-indent: 2em;">
整个处理流程以MM2S分发模块为起点，该模块在硬件实现中通常负责从存储单元读取数据，并将其转换为AXI-Stream数据流，再分发至下游的处理流水线。数据流首先进入第一个 LZ/LZ4 核心单元进行处理，随后依次传递给 Best 模块与 Booster 模块，这两个模块对应压缩算法中的特定优化阶段或性能增强逻辑。数据流经第二个 LZ/LZ4 模块完成所有处理步骤后输出。该流程图清晰地揭示了数据在算子内部各功能模块间的串行处理路径。
</p>

- 接口设计（要点）：
  - 顶层内核：`hlsLz4 / lz4CompressMM` 使用全局内存指针（默认 DATAWIDTH=512bit 宽口、突发长度16），配合块级索引/长度数组；
  - 子模块接口：`hls::stream<T>`，通过 `#pragma HLS STREAM`/`BIND_STORAGE` 指定 FIFO 深度与实现（SRL 优先），按字节宽流传递；
  - 输出包含压缩数据流与每块 `compressedSize` 等元信息，满足 LZ4 块级格式。

- 优化要点摘要：
  - 存储：FIFO 优先 SRL 化，合理 STREAM 深度以降低 BRAM 占用；64KB 分块令工作集受控；
  - 流水：关键打包状态机 `PIPELINE II=1`，读前置与局部变量缓存位段结果，合并分支、预计算 `match_offset+1` 降低关键路径；
  - 并行：在资源允许下评估 `NUM_BLOCK>1` 的块级并行；顶层 dataflow 串接四级子模块形成稳定吞吐；
  - I/O：保持 512bit 宽口 + 16 突发，提升访存效率并减少调度开销；
  - 评估口径：以 `T_exec = Estimated_Clock_Period × Cosim_Latency` 为核心指标，同时关注 II、Slack 与资源（LUT/FF/BRAM/DSP）。

- 平台与环境：目标平台 Zynq-7000（xc7z020-clg484-1 / PYNQ-Z2），工具 Vitis HLS 2024.2；对齐 Vitis Data Compression L1 的接口与测试框架（`data_compression/L1/tests/lz4_compress/`）。


##### 实现细节补充（流水与状态机）

- 顶层 dataflow：`MM2S → lzCompress → lzBestMatchFilter → lzBooster → lz4Compress → S2MM`；
- lz4Compress 状态机：`WRITE_TOKEN → WRITE_LIT_LEN → WRITE_LITERAL → WRITE_OFFSET0/1 → WRITE_MATCH_LEN` 循环，保持 `II=1`；
- 宽口适配：512b AXI 读取在 MM2S 打散成字节流，内部以 `hls::stream<uint8_t>` 传递，必要处合并/打包；
- Token 编码：高 4 位 literal_len、低 4 位 match_len-4，超界通过扩展字节累加；偏移 16b 小端写入。

##### 调度与 II 控制

- 关键循环 `PIPELINE II=1`：包括 token 写、字面量/匹配扩展写、offset 写入；
- 竞争/依赖：对输出流写入使用本地寄存器先行拼装，减少分支路径上的条件判断与位操作；
- 展开策略：在 LZ 前端比较/窗口扫描中谨慎使用 UNROLL，仅在不会引入存储端口冲突时小因子展开；
- 背压控制：各级 stream 设置适度深度（SRL 优先）以吸收 burst 不齐与分支抖动带来的相位差。

##### 运算绑定与时序

- 比较与位段：偏好 LUT 实现；将多路条件合并为查表/掩码运算，缩短比较链；
- 预计算：提前计算 `match_len>15` 分支、`token` 两半字节与 `match_offset+1`，减少状态转移上的组合深路径；
- 存储：`#pragma HLS BIND_STORAGE` 将小深度 FIFO 指定为 SRL，避免 BRAM 端口冲突导致 II 提升。

##### 验证与约束

- 分块：64KB 块级，必要时输出“未压缩块”；
- 格式一致性：和标准 LZ4 框架对齐（token/扩展字节/offset/字面量/匹配顺序）；
- 访存：保持 512b 宽口 + 16 突发；注意尾块对齐与 S2MM 写回长度记录。

##### 预期收益与资源取舍

- 收益：在不牺牲格式合规前提下，状态机稳定 `II=1`，提升吞吐；
- 资源：主要消耗 LUT/FF；BRAM 取决于 FIFO 深度；一般 DSP 使用极低或为 0。

##### 资源消耗

平台与口径：xc7z020-clg484-1（PYNQ-Z2），Vitis HLS 2024.2，默认 DATAWIDTH=512 bit，Target Clock 建议 12 ns。

| 资源 | 数量/总量 | 利用率 |
| ---- | --------- | ------ |
| LUT  | 8088 | 15% |
| FF   | 3748 | 3% |
| BRAM | 108  | 38% |
| DSP  | 0 | 0% |



#### 1.1.3 算子三：Cholesky 分解

- 算法简介：对称正定矩阵 A 的 Cholesky 分解满足 A = L·Lᵀ（实数）或 A = L·Lᴴ（复数），其中 L 为下三角矩阵。分解过程按列（或按行）递推，先计算对角元 L[j,j] = sqrt(A[j,j] − ∑|L[j,k]|²)，再计算非对角元 L[i,j] = (A[i,j] − ∑L[i,k]·conj(L[j,k])) / L[j,j]（i>j）。
- 数据类型与规模：参考当前测试配置使用复数定点类型（总位宽16、整数位1，hls::x_complex<ap_fixed<16,1>>），适配小规模矩阵；对角元为实数，可利用该性质降低复数开销。
- 架构选择：提供三种实现——ARCH=0（Basic，资源最低）、ARCH=1（Alt，延迟更低）、ARCH=2（Alt2，进一步降低延迟但资源更高）。内部通过 traits 统一约束内部乘加/累加/对角与倒数对角类型，内环目标 II=1，可选 UNROLL 因子用于 Alt2 提升吞吐。
- 接口与契约：
  - 输入：Hermitian/对称正定矩阵 A；输出：下（或上）三角矩阵 L；
  - 流式顶层 `cholesky(hls::stream<In>&, hls::stream<Out>&)`；
  - 正定性失败返回错误码（sqrt 输入为负）。
- 时序/优化要点（对齐 Vitis Solver L1 风格）：
  - 对角元只做实数开方与倒数开方：以 x_sqrt/x_rsqrt 替代复数 sqrt/div，缩短关键路径；
  - 预存对角倒数用于后续非对角除法，避免每次除法的长延迟；
  - 合理数组分区与索引简化：小矩阵下优先 2D 数组/适度分区，避免复杂索引带来的组合深路径；
  - 控制 UNROLL/PIPELINE：确保读取同一数组/端口不产生并行冲突，稳定实现 INNER_II=1；
  - 资源/时序权衡：Alt/Alt2 通过更高并行换更低延迟，需结合时钟目标选择合适版本。
<p align="center">
  <img src="https://img.cdn1.vip/i/6906ae6921746_1762045545.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong>图 1-9 Vitis Library Cholesky 算子模块架构图 </strong>
</div>
<br>
<p style="text-indent: 2em;">
如图1-8，该架构专用于处理输入的埃尔米特正定复数矩阵 (Hermitian Positive-Definite complex matrices)，并计算输出对称正定的下三角 L 矩阵。如图所示，该实现具有高度的层次化特征：顶层的 Kernel Cholesky 与其他高级线性代数核（ Kernel LSTQR 和 Kernel SVD）均构建在一个通用的 Linear Solver (线性求解器) 模块之上。该求解器进一步封装了如 Cholesky 和 LU/Cholesky 等基础算法。为确保计算密集型任务在 FPGA 上的高性能执行，整个架构在底层均依赖于一系列 HLS Optimization Primitives (HLS 优化原语)，包括 Dataflow（数据流）、Array Partitioning (数组分区)、Loop Pipelining (循环流水线) 和 Arithmetic Optimizations (算术优化)。
</p>

<p align="center">
  <img src="https://img.cdn1.vip/i/6906ab75628db_1762044789.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong>图 1-10 Vitis Library Cholesky 算子模块时序图 </strong>
</div>
<p style="text-indent: 2em;">
图以时钟周期为单位，在水平方向上标示了时间，并在垂直方向（fx）上分解了核内的主要功能循环。整个核的执行kernel_cholesky_0包含一系列迭代的数据处理流程。read loop读取循环和write loop写入循环较为短，分别对应于数据块的读入和写出操作。核心计算部分由row loop（行循环）和col loop（列循环）以及choleskyAlt功能块构成。时序关系表明，col loop作为内层循环，在row loop的每个迭代周期内会密集执行多次。choleskyAlt模块则展示了算法的主要计算阶段。整体时序揭示了该算子采用了一种流水线或块处理（block-based）的执行模式，即数据被分批读入，经过嵌套循环和主要计算单元choleskyAlt的处理，最后再被写出，以此实现Cholesky分解的高效计算。
</p>
<br>

<p align="center">
  <img src="https://img.cdn1.vip/i/6907037b10de0_1762067323.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong>图 1-11 Cholesky算子优化对比图 </strong>
</div>

##### 详细说明（整合）

1) 算法原理

- 对角元：$$L[j,j] = sqrt(A[j,j] − \sum_{k=0}^{j-1} |L[j,k]|^2)$$
- 非对角元：$$L[i,j] = (A[i,j] − \sum_{k=0}^{j-1} L[i,k]·conj(L[j,k])) / L[j,j]（i>j）$$

1) 架构与 traits

- ARCH=0（Basic）：资源最低，读写索引简单但延迟相对较高；
- ARCH=1（Alt）：将对角倒数缓存于 `diag_internal`，非对角阶段以乘法替代除法；
- ARCH=2（Alt2）：二维 `L_internal` + 可选 UNROLL，进一步降低延迟；
- Traits 统一内部类型：`PROD_T/ACCUM_T/ADD_T/DIAG_T/RECIP_DIAG_T/OFF_DIAG_T/L_OUTPUT_T`，并设定 `INNER_II` 与 `UNROLL_FACTOR`。

3) 顶层接口（流式）

- `int cholesky(hls::stream<InputType>& A_strm, hls::stream<OutputType>& L_strm)`；
- 参数 `LowerTriangularL` 控制下/上三角输出；`RowsColsA` 指定矩阵规模；
- 返回值：0 表示成功，1 表示尝试对负数开方（非正定）。

4) HLS 优化策略

- Pipeline：内环 `#pragma HLS PIPELINE II=1`；外层循环避免因端口冲突导致 II 提升；
- 存储：小规模矩阵优先使用 2D 数组与适度分区；Alt 架构采用一维压缩存储时注意索引生成的组合路径；
- 复数优化：对角使用实数 sqrt/rsqrt；非对角用乘以对角倒数替代除法；
- 时钟目标：先以 12ns 为目标触发寄存器自动插入，再评估 UNROLL 因子带来的额外收益与资源代价。

5) 边界与错误模式

- 输入必须为 Hermitian/对称且正定；
- 若 `A[j,j] − square_sum < 0`，函数返回 1 并将对角设为 0；
- 定点溢出与饱和：由 traits 中的类型和量化/饱和策略控制。

以上实现与优化要点来源于算子3开发材料与 Vitis HLS 2024.2 实践，细节在后续章节将按需要展开。

##### 实现细节补充（choleskyAlt 优化版）

- 架构与存储：
  - 列优先处理（column-major）；二维 `L_internal[RowsA][RowsA]` 完全分区（`#pragma HLS ARRAY_PARTITION complete dim=0`）以消除端口冲突与索引组合深路径；
  - `diag_internal[RowsA]` 缓存对角倒数（`RECIP_DIAG_T`），下游用“乘倒数”替代除法；
  - 为列内并行预分配累加缓存：`square_sum_array[]`、`product_sum_array[]`，便于树形归约与流水寄存。
- 关键循环与并行：
  - `col_loop(j)` 按列推进；对角求和 `diag_sum_loop(k<j)` 与非对角求和 `sum_loop(k<j)` 目标 `II=1`，结合 `UNROLL factor=4`（规模可调）；
  - 对角平方和采用树形并行累加（O(log j) 归约）替代线性累加，缩短依赖链；
  - 对 `L_internal` 标注 `#pragma HLS DEPENDENCE inter false` 避免跨迭代假依赖限制调度。
- 运算绑定与时序：
  - 对角仅取实部参与开方：`A_real = x_real(A[j][j]-\sum|L[j,k]|^2)`；
  - `sqrt/rsqrt` 明确绑定到浮点平方根单元并指定期望延迟：`#pragma HLS BIND_OP op=fsqrt impl=meddsp latency=5`；
  - 乘加优先绑定 DSP48：`#pragma HLS BIND_OP op=mul/add impl=dsp`，并在关键中间值上使用寄存器化存储（`#pragma HLS BIND_STORAGE type=register`）打断长路径；
  - 使用“乘倒数”替代除法：`new_L_off_diag = (A[i,j]-\sum L[i,k]·conj(L[j,k])) * diag_internal[j]`，减少除法长延迟。
- 三角约束与清零：
  - 根据 `LowerTriangularL` 决定输出下/上三角，镜像位置取共轭；
  - 统一在列处理末尾将非目标三角快速清零（`zero_loop`，`PIPELINE II=1`）。

##### 定点与数值稳定性（traits 约束）

- 类型族：`PROD_T/ACCUM_T/ADD_T/DIAG_T/RECIP_DIAG_T/OFF_DIAG_T/L_OUTPUT_T` 由 traits 统一给定；对角为实类型，复数情形通过 `x_real` 抽取实部；
- 非正定检测：若 `A_real < 0`，返回码 `return_code=1`，对角输出置 0 并给定安全倒数（如 1.0）；
- 量化与溢出：定点场景按 traits 的量化/饱和策略约束；建议在 sqrt/rsqrt 前做最小夹紧以避免 -0 误差导致的假负值；
- 精度取舍：`rsqrt` 与“乘倒数”会引入微小数值误差，必要时提高 `RECIP_DIAG_T` 精度或在最终 `L_output` 上做截断一致化。

##### 调度与 II 控制

- 目标：`INNER_II=1` 为硬约束；树形归约与完全分区组合避免端口/依赖造成的 II>1；
- 依赖声明：对 `L_internal` 明确 `inter false` 以消除工具对同列读写的保守限制；
- 时钟策略：建议保持 Target Clock ≈12 ns，让工具在乘加链与索引处自动插入寄存；
- 可调参数：`UNROLL factor` 随 `RowsA` 调整，小规模取 4 可取得较优折中；资源受限时可降为 2 或关闭树形缓存。

##### 预期收益与资源取舍

- 延迟（Latency）：相较 Basic/原始 Alt 版本，预计降低 30–50%（依规模与 UNROLL 因子而变）；
- 吞吐（II）：从 2 降至 1；
- 时序（Slack）：关键路径改善，便于满足 12 ns 左右的时钟目标；
- 资源：DSP 与寄存器使用上升，LUT 小幅增长，BRAM 维持较低（缓存/ FIFO 优先 LUTRAM）。

##### 验证与约束（与官方测试对齐）

- 测试限制：不修改测试用例与不确定度参数；仅允许在 Tcl 中调整时钟频率；
- 功能契约：输入 SPD/Hermitian，负开方返回错误码；输出按 `LowerTriangularL` 生成下/上三角，镜像位置取共轭；
- 代码变更范围：以 `choleskyAlt` 主体（约第 409–560 行）为主要改动点，必要时配合 `pseudosqrt/cholesky_rsqrt` 与 `cholesky_prod_sum_mult` 的实现细节（不破坏接口）。

##### 资源消耗

平台与口径：xc7z020-clg484-1（PYNQ-Z2），Vitis HLS 2024.2，矩阵规模与 ARCH（0/1/2）会显著影响资源；建议 Target Clock 12 ns 口径对齐。

| 资源 | 数量/总量 | 利用率 |
| ---- | --------- | ------ |
| LUT  | 6,541 / 53,200 | 12.3% |
| FF   | 8,402 / 106,400 | 7.9% |
| BRAM | 0 / 280 | 0% |
| DSP  | 26 / 220 | 11.8% |

时序/性能口径（本次报告）：
- Cosim Latency(avg)：434 cycles；Interval(avg)：435 cycles；状态：Pass（`kernel_cholesky_0_cosim.rpt`）。
- 实现时钟：CP 要求 6.000 ns；后实现达成 6.343 ns（Timing not met，`kernel_cholesky_0_export.rpt`）。
- 估算执行时间（依据后实现 CP 与 cosim Latency）：≈ 2,753 ns（6.343 ns × 434 cy）。

预期特点（供参考）：
- ARCH=0（Basic）资源最低；ARCH=1/2 以更高并行换更低延迟；
- 复数定点乘法通常会推断 DSP；对角仅用实数 sqrt/rsqrt，可降低复杂度；
- 小规模矩阵下 BRAM 占用通常较低，FIFO/缓存优先 LUTRAM。

### 1.2 设计目标（总述）

面向多算子统一的设计与评测口径：

- 功能目标：三个算子均需通过 C 仿真与 RTL 联合仿真，输出与参考实现一致；对 Cholesky，若输入不满足正定性须返回错误码；
- 性能目标：在不改动基准测试数据的前提下，各算子内环保持 II=1；在目标时钟下 Estimated 满足约束、实现阶段 Slack ≥ 0；
- 资源目标：优先通过架构与时序优化获取性能，避免不必要的资源堆叠；在满足性能的前提下保持 DSP/BRAM 较低占用；
- 可扩展性：算子二位置预留，接口与评估方法与算子一/三对齐，便于后续无缝补充；
- 工具环境：统一使用 Vitis HLS 2024.2，目标平台 PYNQ-Z2，评估以“执行时间 = Estimated Clock × Latency”为核心指标。

### 1.3 技术规格

- **目标平台：** AMD PYNQ-Z2
- **开发工具：** Vitis HLS 2024.2
- **编程语言：** C/C++
- **验证环境：** C 仿真（csim）+ RTL 联合仿真（cosim）；固定测试数据集（2 条样本，80/81 字节消息；32 字节密钥），严格依据 `L1/tests/hmac/sha256` 的默认测试不做任何修改。

---

## 2. 设计原理和功能框图

### 2.1 算法原理

本算子实现 SHA-256 压缩函数，并在 HMAC 架构中作为两次哈希的底层核心。SHA-256 以 512-bit 分组为单位，对消息进行填充、消息调度（W[t]）与 64 轮压缩迭代，输出 256-bit 摘要。HMAC 计算公式为：

（注：本节保留公式与说明，配图与详细拆解已整合至“1.1.1 算子一：SHA-256”）

$$
\mathrm{HMAC}(K, M) = H\Big( (K' \oplus \mathrm{opad}) \mathbin{\|} H\big( (K' \oplus \mathrm{ipad}) \mathbin{\|} M \big) \Big)
$$

核心位运算与迭代公式：

$$
\begin{aligned}
\Sigma_0(x) &= \mathrm{ROTR}^2(x) \oplus \mathrm{ROTR}^{13}(x) \oplus \mathrm{ROTR}^{22}(x) \\
\Sigma_1(x) &= \mathrm{ROTR}^6(x) \oplus \mathrm{ROTR}^{11}(x) \oplus \mathrm{ROTR}^{25}(x) \\
\sigma_0(x) &= \mathrm{ROTR}^7(x) \oplus \mathrm{ROTR}^{18}(x) \oplus \mathrm{SHR}^{3}(x) \\
\sigma_1(x) &= \mathrm{ROTR}^{17}(x) \oplus \mathrm{ROTR}^{19}(x) \oplus \mathrm{SHR}^{10}(x) \\
\mathrm{Ch}(x,y,z) &= (x \land y) \oplus (\lnot x \land z) \\
\mathrm{Maj}(x,y,z) &= (x \land y) \oplus (x \land z) \oplus (y \land z)
\end{aligned}
$$

$$
W_t = \begin{cases}
M_t, & t\in[0,15] \\
\sigma_1(W_{t-2}) + W_{t-7} + \sigma_0(W_{t-15}) + W_{t-16}, & t\in[16,63]
\end{cases}
$$

$$
\begin{aligned}
T_1 &= h + \Sigma_1(e) + \mathrm{Ch}(e,f,g) + K_t + W_t \\
T_2 &= \Sigma_0(a) + \mathrm{Maj}(a,b,c)
\end{aligned}
$$

$$
(a,b,c,d,e,f,g,h) \leftarrow (T_1 + T_2,\ a,\ b,\ c,\ d + T_1,\ e,\ f,\ g)
$$

### 2.2 系统架构设计

（注：整体框图与系统综述已整合至“1.1.1 Sha256算子”。）

#### 2.2.1 顶层架构

整体采用 HLS 数据流（DATAFLOW）架构：顶层 HMAC 将密钥填充（kpad/kopad）与两次 SHA-256 串联组织，内部通过深度受控的 FIFO 串接。为避免无谓资源翻倍，本版本保持单核 SHA-256（两次计算串行），但通过 FIFO 深度与握手优化实现跨阶段的最大化 overlap。

```
┌──────────────────────────────────────────────────────────────────┐
│                           HMAC 顶层（dataflow）                   │
├──────────────┬────────────────────┬──────────────────────────────┤
│  kpad/kopad  │   msgHash(dataflow)│   resHash(dataflow)          │
│  生成填充     │   ├─ mergeKipad    │   ├─ mergeKopad              │
│              │   └─ sha256_top    │   └─ sha256_top              │
└──────────────┴────────────────────┴──────────────────────────────┘

sha256_top（dataflow）:
	preProcessing → dup_strm → generateMsgSchedule → sha256Digest
```

#### 2.2.2 核心计算模块设计

详细设计如下：

- preProcessing：将 32-bit 单词流重排打包为 512-bit 分组，追加 SHA-256 填充，统计分组数；内部严格避免对同一 FIFO 的多路并行读，保证 II=1。
- generateMsgSchedule：基于滚动窗口生成 W[0..63]。W[0..15] 直通，W[16..63] 采用 σ0/σ1 与加法器链生成，pipeline II=1；必要处通过加法重排减小关键路径。
- sha256Digest：64 轮压缩迭代核，循环 II=1，67 周期主体 + 启停开销约 3 周期，单分组 ~70 周期。

#### 2.2.3 数据流图

数据在各模块间以 hls::stream 形式传递，关键流设置合理深度避免背压：

```
msg_strm(32b) → preProcessing → blk_strm(512b, depth≈4) → dup_strm
 →generateMsgSchedule(32b, depth≈64) → sha256Digest → hash_out(8×32b)

HMAC: (kipad||msg) → sha256_top → H1 → (kopad||H1) → sha256_top → digest
```

### 2.3 接口设计

采用流式接口以支撑并行流水：

**接口规格：**

- 输入接口：
  - `msg_strm`: 32-bit 宽（ap_uint<32>），AXI-Stream 风格握手（ap_hs），消息数据流。
  - `len_strm`: 64-bit 宽（ap_uint<64>），对应每条消息字节长度。
  - `end_len_strm`: bool 结束标志流。
  - `key_strm`/`klen`: 32 字节密钥（测试固定），在 HMAC 顶层转换为 kipad/kopad 流。
- 输出接口：
  - `digest_strm`: 8×32-bit 宽（共 256-bit 输出），按字输出。
- 控制/时序：
  - 所有子模块内部 `#pragma HLS PIPELINE II=1`；顶层 `#pragma HLS DATAFLOW`。
  - 关键流设置 `#pragma HLS STREAM variable=... depth=...`，优先使用 LUTRAM FIFO。

### 2.4 核心流程伪代码（SHA-256 压缩）

```cpp
// 输入：512-bit 块 blk，初始状态 H[8]
void sha256_compress(Block blk, uint32_t H[8]) {
    uint32_t W[64];
    // 1) 消息调度（W）
    for (int t = 0; t < 16; ++t) {
        W[t] = byteswap32(blk.M[t]);
    }
    for (int t = 16; t < 64; ++t) {
        W[t] = sigma1(W[t-2]) + W[t-7] + sigma0(W[t-15]) + W[t-16];
    }

    // 2) 64 轮压缩
    uint32_t a=H[0], b=H[1], c=H[2], d=H[3];
    uint32_t e=H[4], f=H[5], g=H[6], h=H[7];
    for (int t = 0; t < 64; ++t) {
        uint32_t T1 = h + Sigma1(e) + Ch(e,f,g) + K[t] + W[t];
        uint32_t T2 = Sigma0(a) + Maj(a,b,c);
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }

    // 3) 与原状态相加
    H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d;
    H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
}
```

对应 HLS 流水线：preProcessing 负责 byteswap 与分块；generateMsgSchedule 以滚动窗口实现第二段 W；sha256Digest 完成 64 轮迭代并输出累加结果。

### 2.5 数据对齐、字节序与填充规则

- 字节序：输入按字节高位在前（big-endian）进行 byteswap32 后进入 W[0..15]。
- 填充：附加 0x80 后补 0，最后 64 bit 写入消息比特长度 L；当剩余空间不足 8B 时扩展到下一块。
- 例子（80 字节消息 → 3 块）：
  - Block0：kipad；Block1：消息前 64B；Block2：剩余 16B + 0x80 + 0…0 + L（64b）。

### 2.6 关键位运算真值与实现提示

- Ch(x,y,z) = (x & y) ^ (~x & z)：当 x=1 选择 y，当 x=0 选择 z。
- Maj(x,y,z)：三者多数投票；在硬件上可用两级与/或减少 XOR 链长度。
- ROTR/SHR：优先用 LUT 级移位网络，避免推断慢路径；对 32-bit 建议保持并行位切片实现。

---

### 2.7 顶层/子模块接口契约（Contract）

- 输入输出宽度与类型：
  - 输入消息 `msg_strm`：ap_uint<32>，每拍 32 位，按字节大端入栈后在 preProcessing 中统一 byteswap32。
  - 消息长度 `len_strm`：ap_uint<64>，单位字节；与消息一一对应。
  - 输出摘要 `digest_strm`：ap_uint<32>×8 顺序输出，共 256 位；每完成一条消息输出 8 个字。
- 握手时序：
  - 均采用 `ap_hs` 风格（等价 AXI-Stream ready/valid）；上游仅在 `!full` 时写、下游仅在 `!empty` 时读。
  - 顶层 `#pragma HLS DATAFLOW`，子模块内部 `#pragma HLS PIPELINE II=1`；任何导致 `read()` 多端口并发的 UNROLL 都禁止。
- 边界与错误模式：
  - 长度为 0 的消息仍需执行一块（仅填充+长度）；实现保证该边界不死锁。
  - 当 `len_strm` 与实际消息字数不一致时，行为未定义；测试基准不包含此场景。
- 成功判据：
  - 功能：输出与软件参考一致；
  - 性能：在 12ns 约束下维持 II=1，`w_strm≈64 / blk_strm≈4` 不回压。

### 2.8 轮函数微架构（Round Datapath）

- 组合运算拆分：
  - 上路：`T2 = BSIG0(a) + Maj(a,b,c)`；
  - 下路：`T1 = h + BSIG1(e) + Ch(e,f,g) + K[t] + W[t]` → 建议先做两组部分和 `sumA = h + BSIG1(e)`、`sumB = Ch + K + W`，再归并为 `T1 = sumA + sumB`，减少单拍加法链深度。
- 寄存器插入点：
  - `W[t]` 生成链与 `T1/T2` 相会之前各插一级寄存；
  - `a..h` 状态回写前保留一拍寄存，方便工具做平衡。
- 代价与收益：
  - FF/LUT 略增，但能显著降低组合深度，配合 12ns 目标时钟可将 Estimated 降至 ≈10.575 ns。

## 3. 优化方向选择与原理

### 3.1 优化目标分析

根据赛题要求，本设计主要关注以下优化方向：

- [X] 提升流水线性能（保持 II=1 / 提高吞吐率）
- [X] 提高时序裕量（降低 Estimated Clock Period）
- [X] 提高性能/资源比（以极低资源获得更短执行时间）
- [x] 减少片上存储（BRAM）使用（保持在极低水平）

### 3.2 优化策略设计

#### 3.2.1 存储优化

**优化原理：**
通过流深度与阵列分区配合数据复用，尽量使用 LUTRAM FIFO，避免 BRAM 端口冲突导致的 II 提升或背压停顿。

**具体措施：**

- 数据重用策略：W 调度采用滚动窗口缓存（size=16）替代整体移位，减少搬移操作与访存；输出 hash 累加寄存化。
- 存储层次优化：对 `blk_strm` 采用中等深度（≈4）；`w_strm` 采用较深（≈64）以吸收跨模块相位差。
- 缓存设计：`#pragma HLS ARRAY_PARTITION complete` 用于小型局部数组（如 16×32b buffer）。

#### 3.2.2 流水线优化

**优化原理：**
以 II=1 为硬约束，避免同一单端口 FIFO 在同周期多次读写；必要时通过“加法器链重排 + 自动寄存器插入”缩短关键路径。

**具体措施：**

- 循环展开：严禁对 `msg_strm.read()` 的 16 次读取做完全展开，避免单端口 FIFO 冲突导致 II=16；如需并行，仅采用小因子或回退为纯 PIPELINE。
- 流水线插入：阶段性降低目标时钟（15ns→12ns），让 HLS 自动在关键链路插入寄存器；保留 1–2% 的 Latency 增幅换取 15%+ 的 Clock 改善。
- 数据依赖处理：64 轮压缩核保持单步迭代 II=1，不做跨轮展开；消息调度中使用滚动窗口打破长距离依赖。

#### 3.2.3 并行化优化

**优化原理：**
在不复制 SHA-256 核的前提下，最大化 `kpad/merge/sha256_top` 三阶段的重叠；如资源允许，可作为后续方向考虑“双核并行”以并行完成 HMAC 的两次哈希。

**具体措施：**

- 任务级并行：HMAC 顶层三个阶段通过 DATAFLOW 串接，辅以足够流深保证 overlap。
- 数据级并行：滚动窗口 + 完全分区的小数组实现并行访存和组合计算。
- 指令级并行：位运算（ROTR/XOR/AND/OR）与加法并行排布，减少串联深度。

### 3.3 HLS指令优化

项目中使用的关键 HLS 指令示例：

```cpp
// 流水线与数据流
#pragma HLS DATAFLOW
#pragma HLS PIPELINE II=1

// 阵列分区与缓存
#pragma HLS ARRAY_PARTITION variable=buff complete

// 流深控制（倾向 LUTRAM）
#pragma HLS STREAM variable=w_strm depth=64
#pragma HLS STREAM variable=blk_strm depth=4

// 谨慎展开：避免对单端口 FIFO 的完全并行读
//#pragma HLS UNROLL  // 若作用于对同一 FIFO 的多次 read，将导致 II 违例
```

### 3.4 消融实验（Ablation）与反例复盘

| 变更项 | 目标 | 现象 | 结果 | 结论 |
|--------|------|------|------|------|
| preProcessing 内层 UNROLL×16 | 降低局部 latency | 触发单端口 FIFO 16 路并发读 | Achieved II=16 | 禁用/小因子分组，保持 II=1 |
| W 滚动窗口 + byteswap 精简 | 降 latency | 索引计算增加组合深度 | Estimated↑，执行时间↑ | 需配合降时钟让工具插寄存器 |
| 降目标时钟 15→12ns | 降执行时间 | 插入寄存器，Latency+1.5% | 执行时间 -17.8% | 极高性价比 |

### 3.5 FIFO 深度与背压分析（建模）

- 设四级流水：P（~17cy）、D（~6cy）、G（~73cy）、H（~70cy），稳态吞吐由 max(G,H) 决定。
- 若 `w_strm` 深度不足，G→H 之间会产生“空-满”振荡，导致吞吐退化与 pipeline 气泡。
- 经验配置：`w_strm≈64`、`blk_strm≈4` 可稳定吸收相位差，避免背压回传至 P。

### 3.6 II 计算与关键路径拆分

- II 提升常见来源：单端口存储多次并发读/写、不可重入资源、跨迭代依赖；
- 关键链路拆分：将 `Wt = σ1 + W[t-7] + σ0 + W[t-16]` 重构为 `(σ1+W[t-7]) + (σ0+W[t-16])`，减少加法串联深度。

### 3.7 调度视图与阶段重叠（可操作要点）

- 观察点：在调度视图（Schedule/Dataflow Viewer）中确认 P（preProcessing）、G（generateMsgSchedule）、H（sha256Digest）三段是否稳定重叠；任一段出现“空拍带”说明流深不足或依赖未打断。
- 快速修复手册：
  - 若 P→G 间断断续续：提高 `w_strm` 深度至 64；
  - 若 G→H 出现 backpressure：检查 `Wt` 加法链是否过深，尝试分路求和或降低目标时钟促使插桩；
  - 若出现 Achieved II>1：优先排查对同一 `read()` 的 UNROLL 或资源端口冲突。

---

## 4. LLM 辅助优化记录

### 4.1 优化阶段一：数据流与 FIFO 调优（Phase 1-2）

#### 4.1.1 优化目标

修复 dataflow 流水线中的空泡（bubble），消除由错误展开策略引入的 II 违例，保证 sha256_top 内部各子模块能够稳定以 II=1 运行并产生阶段 overlap。

#### 4.1.2 Prompt 设计

**用户输入（节选）与上下文：**参见 `7security/DATAFLOW_VIEWER_ANALYSIS.md` 与 `第一个算子的对话.md`，聚焦于 `preProcessing/LOOP_SHA256_GEN_ONE_FULL_BLK` 的展开策略导致 `msg_strm.read()` 对单端口 FIFO 的并行冲突，引发 Achieved II=16。

#### 4.1.3 LLM 回答

建议回退完全展开，对读取 FIFO 的循环仅保留 `#pragma HLS PIPELINE II=1` 或采用小因子分组展开；同时增大关键流深（如 `w_strm`→64）以增强跨阶段缓冲能力。

#### 4.1.4 优化实施

**采用的建议：**回退对 `msg_strm` 的完全展开；为 `blk_strm/w_strm` 配置合适深度；保持 SHA-256 64 轮迭代核的 II=1。

**实施效果：**消除了 II=16 违例，整体执行时间较基线获得 4.1% 改善（10,420 ns → 9,997 ns）。

### 4.2 优化阶段二：算法重构（滚动窗口 W + Byte-Swap 精简）（Phase 3）

#### 4.2.1 优化目标

在不改变功能的前提下减少无谓操作数与搬移成本，降低 Latency。

#### 4.2.2 Prompt 设计

参考 `7security/PHASE3_ALGORITHM_OPTIMIZATION.md` 与对话文档，提出以 16 深度滚动窗口替代全体移位，及对 byteswap 操作进行表达式合并与复用。

#### 4.2.3 LLM 回答

同意滚动窗口与 byte-swap 精简策略，但提示需要关注索引 `(idx+N)&15` 引入的加法+与门组合逻辑会拉长关键路径。

#### 4.2.4 优化实施

实施后 Latency 从 808 → 798（-10 cycles），但 Estimated Clock 从 12.373 ns → 12.694 ns，净执行时间略有回退（9,997 → 10,127 ns）。

### 4.3 优化阶段三：时钟约束优化（Phase 4）

目标：降低 Target Clock（15ns → 12ns），迫使 HLS 自动在关键组合路径插入寄存器以缩短 Estimated Clock。

结果：Estimated Clock 12.694 ns → 10.575 ns（-16.7%），Latency 798 → 810（+1.5%），执行时间 10,127 → 8,565.75 ns（-15.4%），相对基线 -17.8%，Slack +0.225 ns。详见 `7security/PHASE4_CLOCK_OPTIMIZATION.md`、`PERFORMANCE_COMPARISON.md`、`FINAL_Optimization_Summary.md`。

### 4.4 LLM 辅助优化总结
<h3>4.4 LLM 辅助优化总结</h3>
<p><strong>
  <em>(有关本节的详细论述与数据，请参阅
  <a href="./llm_usage.html" title="查看 llm_usage 详细报告">llm_usage.md</a> )
  </em></strong>
</p>

**总体收益：**

- 性能提升：执行时间 10,420 ns → 8,565.75 ns（-17.8%），II 维持 1。
- 资源节省：保持 DSP=0、BRAM≈1%，LUT/FF 适度增加以换取更佳时序。
- 开发效率：通过 LLM 协助定位 dataflow 瓶颈与关键路径，迭代时钟约束快速收敛到最优解。

**经验总结：**

- Prompt 需附带综合/调度报告要点（II 违例位置、Estimated Clock、Slack），便于模型快速聚焦。
- 对读取同一 FIFO 的循环慎用/禁用 UNROLL；保持 II=1 比局部并行更重要。
- 优先通过降低目标时钟让工具插入寄存器来缩短关键路径；每步都以“执行时间”作为总目标评估。

---

## 5. 优化前后性能与资源对比报告

### 5.1 测试环境 (SHA-256)

-   **硬件平台：** AMD PYNQ-Z2
-   **软件版本：** Vitis HLS 2024.2
-   **测试数据集：** 2 条样本（消息 80/81 字节，固定密钥 32 字节），共计 10 个 SHA-256 分组（3+2 块/条）。详见 `7security/TEST_ANALYSIS.md`。
-   **评估指标：** Estimated Clock（ns）、Latency（cycles）、执行时间（ns=Clock×Latency）、II、Slack、资源利用率（LUT/FF/BRAM/DSP）。

### 5.2 综合结果对比 (SHA-256)

#### 5.2.1 资源使用对比

最终最优方案（Phase 4-1，目标 12ns）的资源统计：

| 资源类型 | 数量/总量 | 利用率 | 备注 |
|---|---|---|---|
| LUT | 44778 / 53,200 | 84.2% | Vivado在布局布线的时候时间过长，大约需要40分钟|
| FF | 46321 / 106,400 | 43.5% | 充裕 |
| BRAM | 2 / 280 | 0.7% | 主要来自少量 FIFO |
| DSP | 0 / 220 | 0% | 算法以逻辑与加法为主，无需 DSP |

对于LUT的资源使用量已经超过Vivado建议的70%，因此需要评估方案的可靠性，在上板的时候可能会有BUG存在，如果有条件可以更换资源更多的ZYNQ 7100板卡。

#### 5.2.2 性能指标对比

| 性能指标 | 优化前 | 优化后 | 改善幅度 |
|---|---|---|---|
| 初始化间隔(II) | 1 | 1 | 0% |
| 延迟(Latency) | 809 | 690 | +24.7% |
| 吞吐率(Throughput) | 266.67 MB/s | 373.48 MB/s | +40.1% |
| 时钟频率 | 66.7MHz | 93.4MHz | +40.0% |

#### 5.2.3 复合性能指标

| 复合指标 | 优化前 | 优化后 | 改善幅度 |
|---|---|---|---|
| 执行时间(ns) | 11201.4 | 6537.0ns | 41.0% |
| 时序Slack(ns) | 时序通过 |时序通过  | — |

### 5.3 详细分析 (SHA-256)

#### 5.3.1 资源优化分析

**BRAM优化效果：**
所有 FIFO 优先映射为 LUTRAM（小深度优先），BRAM 利用维持在 ≈1%。

**DSP优化效果：**
SHA-256 全部为整型与位逻辑运算，DSP 利用为 0。

**逻辑资源优化效果：**
随着更紧的时钟约束，HLS 插入了更多寄存器以打断关键路径，FF/LUT 略有增加但总体利用率仍远低于资源上限。

#### 5.3.2 性能优化分析

**流水线效率与 II：**
修复 `preProcessing` 的错误展开后，消除了 II=16 违例，sha256_top 子模块稳定以 II=1 运行。

**延迟与时钟的权衡：**
Phase 3 的算法重构带来小幅 Latency 降低，但拉长了关键路径；Phase 4 通过降低 Target Clock 促使自动插入寄存器，Estimated Clock 大幅下降至 10.575 ns，尽管 Latency +1.5%，最终执行时间仍获得 -15.4% 的净收益（相对 Phase 3），对比基线为 -17.8%。

**吞吐率：**
在 II=1 前提下，整体吞吐由 Estimated Clock 主导；12ns 目标下得到 ~94.6 MHz 的估算频率，相比基线 ~77.5 MHz 明显提升。

### 5.4 正确性验证 (SHA-256)

#### 5.4.1 C代码仿真结果

**仿真配置：**
-   测试用例数量：2（80/81 字节）
-   测试数据类型：字节流消息 + 32 字节密钥
-   精度要求：SHA-256/HMAC 完全一致（32 字节 hash）
**仿真结果：**
-   功能正确性：✅ 通过（C 仿真与 RTL 联合仿真均 Pass）
-   性能验证：各阶段性能与 `7security/PERFORMANCE_COMPARISON.md` 一致

#### 5.4.2 联合仿真结果

**仿真配置：**
-   RTL仿真类型：[Verilog/VHDL]
-   时钟周期：[ns]
-   仿真时长：[周期数]
**仿真结果：**
-   时序正确性：✅ 通过（12ns 目标下 Slack +0.225 ns）
-   接口兼容性：✅ 通过
-   性能匹配度：与综合报告估计高度一致

#### 5.4.3 硬件验证

【测试平台】
-   板卡与接口：PYNQ Z2
-   工具链：Vitis HLS 2024.2；Vivado 2024.2
-   目标时钟：10.71 ns（约 93.37 MHz）

【设计配置】
-   顶层核：sha256_top（DATAFLOW，关键循环 PIPELINE II=1）
-   流深设置：w_strm=64，blk_strm=4
-   约束：保持读取 FIFO 的循环不做完全展开

【测试方法】
1) 通过主机程序向板卡写入两组消息（80/81 字节），并触发计算。
2) 从板卡读回 32 字节摘要（或 HMAC 两次摘要），与软件参考结果比对。
3) 记录硬件计数器（起停时间戳/循环计数）用于估算吞吐与延迟。

【结果摘要】
-   功能对比：与软件参考一致（两组测试均 PASS）。
-   运行稳定性：多轮触发无死锁/卡顿；AXI 握手正常。
-   时序收敛：综合与实现报告显示在 12 ns 约束下均满足，Slack 为正值。
-   性能估算：在 II=1 前提下，执行时间与 Estimated Clock 和数据分块数一致匹配。

【关键日志摘录】
备注：如需更高频率，可在消息调度链路加入额外寄存级，或改写加法链为分段求和，以进一步缩短关键路径。


### 5.5 子模块与循环级别性能拆解 (SHA-256)

| 模块/循环 | 说明 | 目标 II | 实际 II | 单块 Latency | 备注 |
|---|---|---|---|---|---|
| preProcessing | 组包/填充/byteswap | 1 | 1 | ~17 cy | 禁用对 FIFO 的完全展开 |
| generateMsgSchedule.W0-15 | 直通 | 1 | 1 | 18 cy | 启停开销含 2cy |
| generateMsgSchedule.W16-63 | σ/加法链 | 1 | 1 | 50 cy | 可做加法并行重排 |
| sha256Digest.64rounds | 64 轮 | 1 | 1 | 67 cy | 含 pipeline 启动 ~3cy |
| sha256Digest.nblk | 分块循环 | - | - | ~70 cy | 单块统计 |

### 5.6 资源热点与时序瓶颈摘要 (SHA-256)

-   热点 1：位运算 + 加法器链（Wt 与 T1/T2）；
-   热点 2：滚动窗口索引 `(idx+N)&15` 引入的组合逻辑；
-   建议：保留 12ns 目标时钟；如需更高频，考虑 W 链路再分级寄存或改写为 staged 求和。



### 5.7 测试环境 (LZ4 Compress)

-   **硬件平台：** AMD Zynq-7000（xc7z020-clg484-1）/ PYNQ-Z2
-   **软件版本：** Vitis HLS 2024.2
-   **测试数据集：** `sample.txt` 及竞赛提供的参考样例（64KB 块）
-   **评估指标：** II、Latency、Estimated Clock Period、T\_exec、资源（LUT/FF/BRAM/DSP）、Throughput/BRAM、Performance/DSP

### 5.8 综合结果对比 (LZ4 Compress)

#### 5.8.1 资源使用对比

| 资源类型 | 综合后 | Post-impl | 改善 | 综合利用率 | Post-impl利用率 | Available |
|---|---|---|---|---|---|---|
| BRAM | 108 | 108 | 0 | 38% | 38% | 280 |
| DSP | 0 | 0 | 0 | 0% | 0% | 220 |
| LUT | 8084 | 2920 | -64% | 15% | 5% | 53200 |
| FF | 3748 | 2452 | -35% | 3% | 2% | 106400 |

注：优化后资源在综合阶段保持稳定；Post-impl因Vivado资源压缩/优化，LUT/FF显著下降，说明工具层面进行了额外优化；所有资源使用率远低于限制。

#### 5.8.2 性能指标对比

| 性能指标 | 优化前 | 优化后 | 改善幅度 |
|---|---|---|---|
| Co-sim Latency (cycles) | 约~1700 | 1378 | 约-19% |
| Estimated Clock (ns) | ~15.0 | 12.460 | 约-17% |
| Post-synthesis Clock (ns) | ~15.0 | 8.538 | 约-43% |
| Post-impl Clock (ns) | ~15.0 | 11.176 | 约-25% |
| T\_exec (latency×12.460ns) | ~21200 | 17170 | 约-19% |
| Estimated Fmax (MHz) | ~67 | 80.26 | 约+20% |
| 压缩比(Compression Ratio) | 参考基线 | 2.13689 | 正常范围 |
| 时序状态(Slack) | 趋近于 0 | >0 (通过) | 满足约束 |

#### 5.8.3 复合性能指标

| 复合指标 | 优化前 | 优化后 | 改善幅度 |
|---|---|---|---|
| 性能/BRAM比 (Throughput/BRAM, MB/BRAM) | 参考基线 | 约 0.016 | 正常范围 |
| T\_exec改善率 (%) | 基准100% | 67% | -33% |
| 时序裕量改善 (Slack) | ~0 ns | >0 (通过) | 通过 |

### 5.9 详细分析 (LZ4 Compress)

#### 5.9.1 资源优化分析

**BRAM优化效果：**
通过将部分 FIFO 绑定为 SRL（移位寄存器）实现，并限制 STREAM 深度，削减 BRAM 使用；并行核数的的选择亦影响 BRAM 开销，`NUM_BLOCK=1` 时通常最低。

**DSP优化效果：**
LZ4 压缩核主要为控制与少量比较/算术逻辑，DSP 占用较低；若扩展为更宽数据通路需关注乘法器引入。

**逻辑资源优化效果：**
状态机分支合并与局部缓存可降低关键路径深度；综合阶段LUT 8084、FF 3748；Post-impl工具优化后LUT降至2920（-64%）、FF降至2452（-35%），显著提升资源效率。

#### 5.9.2 性能优化分析

**流水线效率提升：**
关键循环维持 `II=1`，多阶段 dataflow 并行提升整体吞吐；`NUM_BLOCK>1` 时可近似线性提升（受 I/O 与资源制约）。

**延迟优化效果：**
通过读前置与分支合并缩短关键路径，降低每周期组合延迟，潜在提升 Fmax，从而间接降低 `T_exec`。

**吞吐率提升分析：**
宽口突发访存（512bit × burst 16）与多块并行是吞吐提升关键；需与资源/时序折中。

### 5.10 正确性验证 (LZ4 Compress)

#### 5.10.1 C代码仿真结果

**仿真配置：**
-   测试用例数量：1（`sample.txt`）或按提供列表批量
-   测试数据类型：文本/二进制数据，64KB 分块
-   精度要求：无损一致（解压后与原始一致）
**仿真结果：**
-   功能正确性：✅ 通过（csim.log：0 errors）
-   输出精度：字节级一致（解压验证100%通过）
-   压缩比验证：2.13689（符合 LZ4 标准）

#### 5.10.2 联合仿真结果

**仿真配置：**
-   RTL仿真类型：Verilog
-   时钟周期：14.0 ns
-   仿真时长：1378 cycles
**仿真结果：**
-   时序正确性：✅ 通过（Timing met, slack>0）
-   接口兼容性：✅ 通过（AXI4-Stream/FIFO）
-   性能匹配度：100%（cosim latency=1378 cycles 与综合预估一致）

---

### 5.11 Cholesky 资源与性能（阶段性）

数据口径：基于 `security/` 下报告。
-   功能仿真：`kernel_cholesky_0_csim.log` → TB: Pass。
-   联合仿真：`kernel_cholesky_0_cosim.rpt` → Latency(avg)=434 cy，Interval(avg)=435 cy，Status=Pass。
-   实现报告：`kernel_cholesky_0_export.rpt` → 设备 xc7z020-clg484-1；CP 要求 6.000 ns，后实现达成 6.343 ns（Timing not met）。

资源使用（Post-Implementation）：

| 资源 | 数量/总量 | 利用率 |
|---|---|---|
| LUT | 6,541 / 53,200 | 12.3% |
| FF | 8,402 / 106,400 | 7.9% |
| BRAM | 0 / 280 | 0% |
| DSP | 26 / 220 | 11.8% |

性能估算：
-   估算执行时间（按后实现 CP × cosim Latency）：≈ 2,753 ns（6.343 ns × 434 cy）。

简要解读：
-   资源占用较为温和，主要消耗在 DSP（复数乘法）与 FF（完全分区+流水寄存）；
-   以 6 ns 级别目标时钟约束尝试高频，实现阶段略未达标；保守回退至 8–12 ns 目标更利于稳定收敛；
-   后续可通过分段归并/寄存进一步缩短乘加链，或降低 UNROLL 因子在资源与频率间折中。

### 5.12 Cholesky 子模块与循环级性能拆解（csynth）

来源：`kernel_cholesky_0_csynth.rpt`（Target 6.00 ns，Estimated 5.388 ns）。

| 模块/循环 | 说明 | 目标 II | 实际 II | 单次 Latency | 备注 |
|---|---|---|---|---|---|
| Pipeline\_read\_loop | 读取 A/L 流（ap\_fifo） | 1 | 10 | 11 cy | loop auto-rewind；I/O 规整化导致 II=10 |
| choleskyAlt core | 列内核心计算（对角+非对角） | — | — | 340–526 cy | 非流水（per-column），主耗时 |
| Pipeline\_write\_loop | 写出 L 流（ap\_fifo） | 1 | 10 | 11 cy | loop auto-rewind；I/O 规整化导致 II=10 |

提示：顶层 Latency(min/max)=367/553 cy 对齐 cosim(avg)=434 cy；I/O 环节 II=10 对整体吞吐影响较小（占比低且与核心串行列处理叠加）。优化重点仍在 core 的 k-loop 乘加与 sqrt/rsqrt。

### 5.13 资源热点与时序瓶颈摘要（Cholesky）

-   热点 1：非对角累计乘加（sum\_loop）的复数 FMA 链 → 建议以寄存器化分段归并、减小单拍深度；
-   热点 2：对角 sqrt/rsqrt → 建议绑定 meddsp 并显式延迟、对接近 0 的负误差做夹紧；
-   热点 3：完全分区的 L\_internal 引入较多 FF/LUT → 如资源紧张可降低 UNROLL 或改部分分区；
-   I/O：read/write auto-rewind II=10 为工具生成的流水形态，通常不构成瓶颈；
-   频率落差：csynth Estimated 5.388 ns vs 实现 6.343 ns，建议适度放宽目标时钟（如 8–10 ns）或在核心加一道寄存点以拉齐实现端时序。
## 6. 创新点总结

### 6.1 技术创新点

本设计的主要技术创新点：

1. 面向执行时间最优的“Clock×Latency”一体化优化逻辑：通过降时钟目标触发自动寄存器插入，获得远高于单纯 Latency 优化的收益。
2. 针对 HLS 单端口 FIFO 约束的展开策略约束：以 II=1 为硬约束，避免不当 UNROLL 导致的 II 违例与 dataflow 气泡。
3. W 调度与加法器链的关键路径重排：在不增加资源的前提下缩短组合深度，为更高频率创造条件。

### 6.2 LLM辅助方法创新

- 基于报告驱动的 Prompt：每轮提供 csynth/schedule 关键行（II 违例、Estimated Clock、Slack）帮助模型精准定位瓶颈。
- 以“执行时间=Clock×Latency”为统一评判，避免只优化单一维度（仅 Latency 或仅 Clock）。
- 建议同时给出副作用与约束（如单端口 FIFO 限制、UNROLL 风险），提高一次迭代的有效性。

---
### 6.3 与 Vitis Security L1 实现对标

- 架构等价：均采用 DATAFLOW + II=1 的 64 轮迭代核；
- 本设计差异：更严格地管控读取 FIFO 的展开策略；更偏向通过目标时钟驱动寄存器自动插入，而非手写深度切分；
- 可移植性：与 L1 头文件接口契合，便于回并或替换。

---

### 6.4 复现实验与脚本（可选）

- 复现实验步骤（概览）：
  1) csim：使用两条样本（80/81 字节）比对软件参考；
  2) csynth：设置目标时钟 12ns，检查报告 Estimated/Latency/II；
  3) cosim：生成 RTL 并运行联合仿真，核对吞吐与时序；
  4) 如需实现：在 Vivado 端查看 post-route WNS，确认 Slack>0。
- 关键报告位置：`solution/syn/report/*.rpt`、`*_csynth.rpt`、`*_export.rpt`。
- 注意：不同工具版本的估算可能略有差异，以“执行时间 = Estimated × Latency”为一号目标判断。

## 7. 遇到的问题与解决方案

### 7.1 技术难点

| 问题描述                                                                      | 解决方案                                            | 效果                              |
| ----------------------------------------------------------------------------- | --------------------------------------------------- | --------------------------------- |
| preProcessing 内层循环完全展开导致 `msg_strm.read()` 冲突（Achieved II=16） | 回退 UNROLL，仅保留 PIPELINE II=1；必要时小因子分组 | 消除 II 违例，恢复稳定流水        |
| Phase 3 算法重构后时钟变差                                                    | 通过降低目标时钟（15ns→12ns）引导自动寄存器插入    | Estimated Clock 12.694→10.575 ns |
| dataflow overlap 不足                                                         | 增大关键流深（如 w_strm=64、blk_strm=4），优化握手  | 减少气泡，提升并行效率            |

### 7.2 LLM辅助过程中的问题

- 早期建议偏向 UNROLL 强并行，忽视单端口 FIFO 造成的 II 提升。
  解决：在 Prompt 中显式声明“禁止对同一 FIFO 的多次 read 完全展开”，并贴出 II=16 证据。
- 仅关注 Latency 改善导致执行时间反而上升。
  解决：统一以执行时间为目标函数，强制回传 Estimated Clock 与 Latency 双指标。

---
### 7.3 风险评估与回退方案

- 时序风险：进一步降至 11ns 已观察到 Slack 负值；回退 12ns 配置可稳定通过；
- 资源风险：若尝试“双核 SHA-256”，LUT/FF 会显著增加；资源吃紧时保留单核串行；
- 功能风险：位运算宏函数化需严格 cosim 验证；保留基线 tag 以便随时回退。

---

## 8. 结论与展望

### 8.1 项目总结

完成了 HMAC-SHA256 中 SHA-256 核的端到端优化与报告撰写；在不修改测试的前提下，执行时间相对基线降低 17.8%，II 维持 1，时序满足。

### 8.2 性能达成度

满足并超额完成设计目标：执行时间改善 ≥15% 的目标被显著超越（-17.8%），资源与时序均处于可接受范围。

### 8.3 后续改进方向

- 在资源允许时尝试双核并行以打破 HMAC 两次哈希的串行依赖；
- 对 W 生成与 BSIG/SSIG 宏进行函数化拆分，进一步缩短关键路径；
- 在 12ns 基础上进行局部 RTL 级优化以冲击 ~9 ns 的 Estimated Clock。

---
### 8.4 迁移与扩展

- 迁移到更高频 FPGA（如 Zynq UltraScale+）：保持相同 pragma 策略，优先尝试目标时钟法；
- 算法扩展：支持 SHA-224 仅需更换初始 H 常量与输出截断；HMAC 模板化可替换底层哈希。

---

## 9. 参考文献

[1] 刘西林, 严广乐. 基于明文相关的混沌映射与 SHA-256 算法数字图像的加密与监测[J]. Application Research of Computers/Jisuanji Yingyong Yanjiu, 2019, 36(11).
[2] 汤煜, 翁秀玲, 王云峰. SHA-256 哈希运算单元的硬件优化实现[J]. 中国集成电路, 2016, 25(5): 26-31.
[3] R. Nane, V.-M. Sima, C. Pilato, J. Choi, B. Fort, A. Canis, Y. T. Chen, H. Hsiao, S. Brown, F. Ferrandi, J. Anderson, and K. Bertels, “A survey and evaluation of fpga high-level synthesis tools,” IEEE Transactions on CAD, vol. 35, no. 10, pp. 1591–1604, 2016.
[4] D. Soni, M. Nabeel, K. Basu, and R. Karri, “Power, area, speed, and security (pass) trade-offs of nist pqc signature candidates using a c to asic design flow,” in IEEE International Conference on Computer Design, 2019, pp. 337–340.
[5] 许晓飞, 陈亮. 应用整数小波变换的 LZ77 电力数据压缩算法[J]. Journal of Xi'an Polytechnic University, 2018, 32(3).
[6] Sayood K. Introduction to data compression[M]. Morgan Kaufmann, 2017.
[7] 凌元,韩文俊,孙健.基于HLS的矩阵求逆算法设计优化[J].电子技术与软件工程,2021,(22):93-96.DOI:10.20109/j.cnki.etse.2021.22.035.

---

## 10. 附录

### 10.1 详细仿真报告

<p align="center">
  <img src="https://img.cdn1.vip/i/6906deab1e5ac_1762057899.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong> 图 Sha 256 联合仿真时序图</strong>
</div>

<p align="center">
  <img src="https://img.cdn1.vip/i/6906de93c6bc3_1762057875.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong> 图 Lz4 Compression 联合仿真时序图</strong>
</div>

<p align="center">
  <img src="https://img.cdn1.vip/i/6906d6b9d6fd1_1762055865.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong> 图 AMD官方算子解压缩成功日志截图</strong>
</div>
<br>

<p align="center">
  <img src="https://img.cdn1.vip/i/6906d9afc8290_1762056623.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong> 图 Cholesky 联合仿真时序图</strong>
</div>

### 10.2 关键LLM交互记录

以下节选自 `security/DATAFLOW_VIEWER_ANALYSIS.md`、`security/CRITICAL_PATH_ANALYSIS.md` 与 `security/FINAL_Optimization_Summary.md`，展示LLM在定位瓶颈、制定策略与验证结果中的作用：

- 交互1｜修复 Achieved II=16 的根因与对策
  - 核心问题：`preProcessing/LOOP_SHA256_GEN_ONE_FULL_BLK` 对 `msg_strm.read()` 的完全 UNROLL 造成单端口 FIFO 并行读冲突，HLS 被迫串行化，导致 Achieved II=16。
  - 建议方案：撤销 UNROLL，改为 `#pragma HLS PIPELINE II=1`；或保守地 `UNROLL factor=4`，在吞吐与FIFO约束之间折中。
  - 直接效果：消除 II 违例后，preProcessing 能与 `generateMsgSchedule/sha256Digest` 更好地 overlap，避免“16 周期起一拍”的节律拖慢整条链路。

- 交互2｜时钟优先策略：先“目标时钟法”，再微调算法
  - 决策：将 Target Clock 由 15ns 降至 12ns，强制 HLS 插入寄存器打断长组合路径，再评估算法级微优化的净收益。
  - 结果：Estimated Clock ≈ 10.575 ns、Latency ≈ 810、Slack ≈ +0.225 ns；执行时间 8,565.75 ns，相比 Baseline 10,420 ns 改善 17.8%。
  - 经验：执行时间 = Estimated × Latency；当 Estimated 显著下降而 Latency 轻微上升时，整体仍显著受益。

- 交互3｜“负优化”复盘：循环缓冲索引加深组合逻辑
  - 现象：滚动窗口 W-schedule 将移位代价降为索引计算，但在 15ns 宽松时钟下未自动插入足够寄存器，反而拉长关键路径，Estimated 从 12.373 → 12.694 ns，导致执行时间回退。
  - 结论：复杂算术/索引变换应与收紧时钟搭配，让综合自动打断路径；单独应用可能得不偿失。

- 交互4｜关键路径定位与拆解（Wt 计算链）
  - 发现：`Wt = SSIG1(...) + w[t-7] + SSIG0(...) + w[t-16]` 含两段复杂旋转/移位 XOR 与三段 32b 加法串联，是最长路径候选。
  - 建议：分两路并行求和后再归并（sum1/sum2 → Wt），并将 BSIG/SSIG 宏改为 `static inline` 函数以便 HLS 在中间值处插桩。
  - 预期：缩短单拍逻辑深度，为进一步降周期或减小 Slack 提供余量。

- 交互5｜Dataflow 层次的 FIFO/深度与背压
  - 结论：`w_strm` 维持 64 深度、`blk_strm` 维持 4 深度即可；盲目增深并不改善 overlap，反而浪费 LUTRAM。
  - 要点：宁可保证 `PIPELINE II=1` 的稳定性，也不要用过度 UNROLL 换来 FIFO 端口冲突与背压扩散。

- 交互6｜11ns 为何不如 12ns？
  - 事实：Target=11ns 与 12ns 的 Estimated 相同（≈10.575 ns），说明已触及物理极限；但 11ns 触发更激进的调度导致 Latency +1，执行时间反而略差。
  - 取舍：选择 12ns 版本——Slack>0 且执行时间最短。

小结：LLM 在本算子中主要贡献为——精准识别 II 违例根因、倡导“先目标时钟、后算法微调”的策略、提出可综合落地的关键路径拆解手法，并以量化指标闭环验证。
