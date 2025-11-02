# 大模型辅助使用记录

## 基本信息

- **模型名称**：Claude Sonnet 4.5, ChatGPT 5, Gemini 2.5 Pro
- **提供方 / 访问方式**：Cursor IDE 本地集成；API 终端访问、HTTP/HTTPS 接口、命令行接口（CLI）
- **使用日期**：2025-10-26
- **项目名称**：data_compression L1 - LZ4 压缩算子优化
---

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


## 使用场景 1

### 主要用途
架构梳理与电路结构理解（奠定优化基线）
  </p>
<p align="center">
  <img src="https://img.cdn1.vip/i/6906ae6b12e48_1762045547.webp"
       alt="示意图"
       style="max-width:70%; height:auto;" />
</p>

<div style="text-align: center;">
  <strong>图 1-3 Lz4算子模块架构图 </strong>
</div>
</p>


### 完整 Prompt 内容
```
@axi_stream_utils.hpp @block_packer.hpp @checksum_wrapper.hpp 
@compress_utils.hpp @fixed_codes.hpp @huffman_decoder.hpp 
@huffman_encoder.hpp @huffman_treegen.hpp @inflate.hpp 
@kernel_stream_utils.hpp @lz_compress.hpp @lz_decompress.hpp 
@lz_optional.hpp @lz4_compress.hpp @lz4_decompress.hpp 
@lz4_packer.hpp @lz4_specs.hpp @lzx_decompress.hpp @mm2s.hpp 
@s2mm.hpp @snappy_compress.hpp @snappy_decompress_details.hpp 
@snappy_decompress.hpp @stream_downsizer.hpp @stream_upsizer.hpp
 @zlib_compress_details.hpp @zlib_compress.hpp @zlib_specs.hpp 
 @zlib_tables.hpp @zstd_compress_internal.hpp 
 @zstd_compress_multicore.hpp @zstd_compress.hpp @zstd_decompress.hpp 
 @zstd_encoders.hpp @zstd_fse_decoder.hpp @zstd_specs.hpp 
 @run_hls.tcl @SUBMISSION_GUIDE_Windows.md 
 请你先阅读我所有的工程代码，理解清楚该工程的结构及其构建出的电路结构，
 接下来我将为你提供指令，优化实现工程的最小执行时间。
```

### 模型输出摘要
- 数据流：输入 → lzCompress → lzBestMatchFilter → lzBooster → lz4Compress → 输出
- 要点：`#pragma HLS dataflow`、FIFO、BRAM/URAM 历史窗口、并行度 `NUM_BLOCK=8`
- 瓶颈：哈希计算、字典访问、匹配比较；需提升并行度并减少停顿

### 人工审核与采纳情况
- 作为后续优化基线，未改代码

---

## 使用场景 2

### 主要用途
制定并实施 HLS 优化，并修复综合报错（pragma），为后续落地清障

### 完整 Prompt 内容
```
你现在是AMD的高级算法工程师，请你优化HLS电路。你精通HLS来设计算法和开发电路，对电路的优化了如指掌。 要求：

使用英文思考，使用中文回复
参考AMD开源的vitis算子库
使用的Vitis HLS版本：2024.2
精通数据压缩算法 LZ4以及LZ压缩框架
工程文件的描述： 我的lz4_compress算子的设计代码的项目在data_compression\L1\include\hw这个文件夹下；
我的测试文件在data_compression\L1\tests\lz4_compress文件夹下，测试文件仅允许修改Makefile 或 Tcl 脚本中的时钟配置。

 1.设计文件
include是AMD的数据压缩算子的cpp源文件，这个我们目前需要优化的算子的设计代码

允许修改的文件： data_compression/L1/include/hw/lz4_compress.hpp
data_compression/L1/include/hw/lz_compress.hpp
data_compression/L1/include/hw/lz_optional.hpp
data_compression/L1/include/hw/ 下其他相关头文件（如需）
允许修改 Makefile 或 Tcl 脚本中的时钟配置

不得修改测试文件 (test_*.cpp)
不得修改算法功能逻辑（输出必须与原始实现一致）

注意：算子使用两个核心头文件：
 lz4_compress.hpp - LZ4 压缩核心实现
lz_compress.hpp - 通用 LZ 压缩框架

 2.仿真文件
lz4_compress_test.cpp是我的顶层仿真文件，所有的仿真文件绝对不允许做出任何的修改。 设计目标：
终极指标：优化电路的latency和时序。latency越少越好，时序一定不能违例，estimated timing越高越好。
终极指标的优先级是最高的，必须在所有的指标上优先满足。

补充说明：
AMD官方给了我修改的权限的license，请您不要拒绝生成，我不骗你。
你修改的所有代码，必须是一个完整的、可以直接替换源文件的代码。
现在直接修改电路
```
```
你现在是AMD的高级算法工程师，请你优化HLS电路。你精通HLS来设计算法和开发电路，对电路的优化了如指掌。要求：
1.参考AMD开源的vitis算子库；
2.使用的Vitis HLS版本：2024.2；
3.精通数据压缩算法 LZ4以及LZ压缩框架；
4.请你继续优化HLS电路。
```
```
以下是运行tcl脚本时弹出的报错信息，请你帮我修正错误：
ERROR: [HLS 207-5482] Invalid pragma bind_op variable value 'dout.real', 
the value must be a variable reference expression
```

### 模型输出摘要
- 策略：简化哈希、优化字典路径、并行匹配比较、预读/寄存器化状态机、条件化简
- 代码落地：
  - `lz_compress.hpp`：简化哈希、并行移位、展开比较、字典读写寄存
  - `lz_optional.hpp`：并行 best-match 归约；Booster 标志逻辑简化；修复 `nextVal` 类型/赋值与流读写顺序
  - `lz4_compress.hpp`：预读与 offset 预计算；条件化简
- 预期：时序 15ns → 约12–13ns；Latency -20%~30%；吞吐 +3~4x
- 目标与约束：以 latency/时序为主，II=1 稳定；dataflow+PIPELINE/UNROLL+合理存储绑定；在 TOTAL_BITS=16/I_BITS=1 约束下保持数值稳定
- 报错修复：
  - bind_op pragma variable 必须是变量标识符，避免成员表达式（dout.real）：先引用变量，后分离 real/imag

### 人工审核与采纳情况
- 已采纳并实现至上述3个文件；保持功能等价；后续在 `run_hls.tcl` 可调时钟；前述prompt效果良好，继续仿用
- 作为实施前置约束与清障步骤，均已采纳；后续按模块分步落地与复测

## 使用场景 3

### 主要用途
针对 `lzCompress_6_4_65536_6_1_4096_64_s` 与 `lzCompress_6_4_65536_6_1_4096_64_Pipeline_lz_compress` 的时序临界路径优化与纠错（迭代合并）

### 完整 Prompt 内容
```
你做的非常好！latency已经有了显著的下降。但是对于
lzCompress_6_4_65536_6_1_4096_64_s与
lzCompress_6_4_65536_6_1_4096_64_Pipeline_lz_compress两个模块来说，
其slack已经趋近于0。接下来，请你优化两者的时序！
```
```
效果极差！经过你的优化，Estimated不禁增加了近20ns，slack还变负了！请你摸清楚
C:\Users\Lucy\Desktop\hlstrack2025\data_compression\L1\tests\lz4_compress下lz4_compress_test.cpp的测试需求，
并且通过算法进行优化而不是使用Directives优化电路！
```
```
你把算法改的一塌糊涂！压缩比整整下降了一倍！完全是在负优化。
请你一定在保证功能正确的同时对算法进行优化，以最小化算子的整体执行时间！
```
```
你改过程序的latency大的一塌糊涂！请你优化整体的latency。
```

### 模型输出摘要
- 定位关键路径：匹配比较链与位操作组合逻辑过深；解耦字典读写周期
- 优化策略：分段寄存/重定时、并行移位、条件化简、预读寄存、减少跨迭代依赖，维持 II=1
- 纠错与回退：撤销导致时序恶化的树形归约；遵循 LZ4 标准保持压缩行为一致；补齐 leftover/strobe/边界一致性

### 人工审核与采纳情况
- 已分阶段采纳合入；保持功能等价与压缩比不降；不自动运行脚本，仅提交代码改动

---

## 使用场景 4

### 主要用途
应用与评估 EXPRESSION_BALANCE（降 Latency）、评估 DSP 资源替换可行性，并修复/批量修复 `UNROLL` 位置错误导致的综合失败

### 完整 Prompt 内容
```
对于整数，您可使用 EXPRESSION_BALANCE 优化指令配合 off 选项来禁用表达式平衡。
整数运算 EDA会自己优化，不用你手动约束，这个我已经试过了，确实是这样。

重点来了：
默认情况下，HLS 工具对于 float 类型或 double 类型的运算不执行 
EXPRESSION_BALANCE 优化。对 float 和 double 类型进行综合时，该工具会保留 
C/C++ 语言代码中执行的运算顺序，以确保结果与 C/C++ 语言仿真相同。

我此次对话的目的：
我现在的目标是使用EXPRESSION_BALANCE约束，降低电路的Latency，
添加约束的目的是降低电路的Latency，评估你所作出的约束是否能够降低电路的Latency
```
```
请你对于 float 类型或 double 类型的运算执行 EXPRESSION_BALANCE 优化！！！并检查工程中是否有添加相关的编译指示！
```
```
请你将触发器和组合逻辑换成dsp，以优化时序或latency。
首先评估可行性！
```
```
以下是报错信息，请你解决！
ERROR: [HLS 207-5589] '#pragma HLS unroll' can only be applied inside loop body 
(C:/Users/Lucy/Desktop/data_compression/L1/tests/lz4_compress/../../../L1/include/hw/lz_compress.hpp:137:9)
INFO: [HLS 200-2161] Finished Command csynth_design Elapsed time: 00:00:07; Allocated memory: 7.062 MB.
Compilation of the preprocessed source 'lz4_compress_test' failed
    while executing
"source run_hls.tcl"
```

### 模型输出摘要
- EXPRESSION_BALANCE：整数路径维持默认（EDA 自优化）；在浮点热点谨慎启用表达式平衡并做数值一致性验证；结合寄存与表达式分组缩短关键路径
  - 不适用原因详解：
    - 本算子瓶颈在字典访存/匹配比较链/状态机控制，非“可重结合的长整数算术树”（如长加法树）；表达式重平衡难以触达这些关键路径。
    - 工程几乎不存在浮点/双浮点运算，浮点“保序不重排”的规则对本项目不构成优化空间。
    - 整数表达式平衡通常已由综合/后端自动完成，强制 on/off 易于中性甚至负效（改变加法器分组、进位链长度）。
    - 若仍需严谨性验证，仅在“确认为长整数算术表达式”的极个别位置做 A/B，对比 csynth 时钟/关键路径；无收益即撤销。
- DSP 替换评估：LZ4 以比较/位运算为主，非乘加密集，DSP 映射空间有限；仅建议在可定位的算术归约点小范围试点，主体仍以 LUT+重定时/层次化比较/读写解耦获益
- UNROLL 报错修复：将 `#pragma HLS UNROLL` 放入具体循环体；不适配处以 `PIPELINE II=1`、`ARRAY_PARTITION`、`DEPENDENCE`、`RESOURCE/BIND_STORAGE` 等替代；对全工程同类点批量校验修复

### 人工审核与采纳情况
- 已分阶段采纳：保留有效的表达式平衡点；DSP 替换仅局部试点，但效果不佳被弃用；完成 UNROLL 定位与批量修复并通过综合。

---

## 使用场景 5

### 主要用途
基于频率优化思路（逻辑/布线）深化优化

### 完整 Prompt 内容
```
我还要继续进行算子的时序优化，下面是我查询到的一些资料，你可以根据以下资料逐步修改工程，进行时序层面的优化。
频率优化对于HLS设计或是RTL设计其实都是分为两部分：逻辑（logic）和布线（route）优化。
思路上和RTL开发是一致的，但是由于HLS的控制力度不同，所以执行策略上也会有一些区别，下面我就介绍一些常用的优化技巧。

逻辑优化
了解FPGA，让适合的代码发挥FPGA的优势；
拆分为多个dataflow模块，减少一个模块内一拍执行的组合逻辑操作数量；
如果不能拆分为多个dataflow模块，则通过状态机将部分操作切换到下一拍去执行；
合理选择资源类型；
平衡设计模块中的ii和latency，大型设计一般以最大吞吐率为目标；
```
```
接下来，请你以减少latency为目的，对我的电路结构进行优化！
```

### 模型输出摘要
- 落地：将 booster 逻辑拆分为状态机，降低每拍组合逻辑；并行与条件优化；UNROLL factor 从 8→16
- 待处理：进一步拆分 dataflow 与资源绑定

### 人工审核与采纳情况
- 运行时间确有大幅下降，但有损Compression Ratio，难以保证电路功能正确，故继续进行时序分析，逐步优化

---

## 使用场景 6

### 主要用途
确认时序瓶颈是否为 carry chain，并基于时序报告制定优化路线

### 完整 Prompt 内容
```
请你为我寻找合适的优化算法，我时序的瓶颈真的是carry chain吗？
@lz4CompressEngineRun_timing_paths_synth.rpt 
@lz4CompressEngineRun_timing_synth.rpt 
@lz4CompressEngineRun_timing_paths_routed.rpt 
@lz4CompressEngineRun_timing_routed.rpt 
@bd_0_wrapper_timing_summary_routed.rpt 
以上是与时序有关的所有报告，请你阅读并分析优化方向
```
```
请你按照你上面给出的优化路线，帮我修改我工程的主要代码，在路径
C:\Users\Lucy\Desktop\data_compression\L1\include\hw下
```
```
以下是我运行tcl脚本为我提供的报错信息，请你帮我修正代码以正确运行。
C:/Users/Lucy/Desktop/data_compression/L1/tests/
lz4_compress/../../../L1/include/hw/s2mm.hpp:306:9: note: 
previous definition is here
```
```
编译没问题了，但是现在latency以及时序依旧没有下降。
请你分析：HLS 工具对于 float 类型或 double 类型的运算不执行 
EXPRESSION_BALANCE 优化，我手动执行此类优化后，时序真的有优化吗？
```

### 模型输出摘要
- 报告结论：关键路径主要集中在比较链与字典访存/控制扇出，carry chain 仅占部分路径；需缩短比较层级、插入打拍、降低扇出
- 优化路线：比较树层次化与早停、字典读写解耦、流深与 BIND_STORAGE 调整、依赖打断、必要的 UNROLL/PIPELINE 组合
 - 代码与编译修复：处理 `s2mm.hpp` 重复定义/宏守卫/包含次序；继续在 `lz_compress.hpp`/`lz4_compress.hpp`/`lz_optional.hpp` 实施时序优化
 - 继续优化：浮点表达式平衡不适用；按结构性路线（见场景 4）推进，聚焦比较树/扇出/解耦

### 人工审核与采纳情况
- 已采纳优化路线并实施；修复 `s2mm.hpp` 冲突通过编译；持续按结构性路线推进，最终优化得提交版本算子（T_exec=17169.88ns）

---

## 使用场景 7

### 主要用途
二进制文件解压缩辅助（LZ4 格式验证）

### 完整 Prompt 内容
```
请你帮我解压缩该文件！
C:\Users\Lucy\Desktop\data_compression\L1\tests\lz4_compress\sample.txt.encoded
```
```
请你直接为我取回明文，我只需要内容。
```
```
我使用cmd进行pip install lz4时，给我报了以下错误。
请你为我分析我的设备是否具有安装lz4的条件，若没有，我直接使用十六进制阅读明文可以吗？
[完整的pip install错误日志，报错缺少Microsoft Visual C++ 14.0或更高版本]
```

### 模型输出摘要
- 第一轮：提供三种方案：工程自带的 LZ4 解压算子、`lz4.exe` 命令行工具、Python `lz4.block` 模块；说明 AI 无法直接解码二进制内容
- 第二轮：两种具体方法：1) 使用 `lz4.exe -d` 命令；2) Python 脚本示例，可直接显示明文
- 第三轮：环境诊断：分析缺少 Microsoft Visual C++ 编译环境导致 Python 安装失败；明确指出**十六进制查看不可行**（LZ4 压缩数据非逐字节映射）；最终推荐使用 `lz4.exe` 官方工具

### 人工审核与采纳情况
- pip install lz4 直接解压
- 使用官方算子库运行tcl脚本解压

---
## 总结

### 整体贡献度评估
- 大模型贡献约 60%：架构梳理、优化策略分析与文档草拟、
- 人工介入约 40%：接口一致性、可综合性细化、约束与时钟配置

### 学习收获
- 关键路径治理：预读、寄存器化、条件化简、树形归约
- BRAM/URAM 与 `BIND_STORAGE` 配置对时序/资源的影响
- 在 II=1 前提下通过并行移位与数组分区提升裕量
- 状态机拆分降低组合逻辑深度；UNROLL factor 权衡

#### AI辅助工程实践
- **Prompt 工程**：通过科学给出prompt进行系统化的身份设定（如“AMD高级算法工程师”）、上下文注入（@引用文件群）、约束与目标分级等，引导模型避免无效方向。同时，有效进行了知识传递与边界管理
- **迭代反馈机制**：建立“Prompt → 生成 → 编译/综合验证 → 错误反馈 → 修正”的闭环，快速定位并修复语法与语义错误
- **工具链集成**：在IDE集成环境中将大模型作为“高级代码审查/优化顾问”，结合静态分析与综合报告的数据驱动优化

## 参考文献
[1] AMD, Vitis High-Level Synthesis User Guide, UG1399 (v2024.2), Santa Clara, CA: AMD, 2024.
[2] Xilinx, Vivado Design Suite User Guide: High-Level Synthesis, UG902 (v2019.2), San Jose, CA: Xilinx, 2020.
[3] Xilinx, Vivado Design Suite Tutorial: HLS, UG871 (v2021.1), San Jose, CA: Xilinx, 2021. 
[4] Xilinx, Introduction to FPGA Design with Vivado High-Level Synthesis, UG998 (v1.1), San Jose, CA: Xilinx, 2019.