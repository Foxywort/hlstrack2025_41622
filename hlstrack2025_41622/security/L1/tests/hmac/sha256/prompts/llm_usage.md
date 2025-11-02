# 大模型辅助使用记录

## 基本信息

- **模型名称**：Claude Sonnet 4.5, ChatGPT 5, Gemini 2.5 Pro
- **提供方 / 访问方式**：Cursor IDE 本地集成；API 终端访问、HTTP/HTTPS 接口、命令行接口（CLI）
- **使用日期**：2025-10-22
- **项目名称**：SHA-256 / HMAC-SHA256 L1 算子优化

---
<p style="text-indent: 2em;">
为了高效优化 AMD Vitis Library 中的 HLS 算子，我们为本项目设计了一个名为 <strong>KernelOpt_Agent</strong> 的<strong>自动化智能体</strong>。在传统的 FPGA 开发流程中，针对特定应用场景的算子调优是一项繁琐且高度依赖专家经验的任务。KernelOpt_Agent 旨在通过自动化分析 Vitis 报告、迭代重构 HLS 代码，显著简化这一优化过程，从而提升设计效率。
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
如图1-1，该架构构建了一个闭环迭代的自动化优化流程：系统首先基于外部输入（如图中所示，例如提供一个经优化的算子设计规划）启动任务，随后 <strong>KernelOpt_Agent</strong> 核心（融合大型语言模型能力）负责自动生成 C/C++ HLS 代码。接着，智能体无缝衔接并自动调用 AMD Vitis 与 Vivado 工具链执行综合与实现，并对输出的性能和资源报告进行深度解析。最后，分析结果将作为反馈信号返回至智能体，用以指导下一轮的代码重构与优化，从而构成一个持续自我完善的自动化设计循环。
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

<p align="center">
  <img src="https://img.cdn1.vip/i/69070b2cd7515_1762069292.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<p>
<div style="text-align: center;">
  <strong>图 1-3 KernelOpt_Agent优化Sha256算子流程图 </strong>
</div>
</p>
<p style="text-indent: 2em;"> 如图1-3所示，展示了KernelOpt_Agent框架在优化Sha256算子时所采用的具体流程图。该流程构建了一个由多个大型语言模型（LLMs）协同工作的自动化反馈闭环。初始阶段，Gemini 2.5 Pro负责解析用户的高级设计提示（Prompt），并生成一份详细的“Output Analysis”报告，该报告主动识别了如<code>PIPELINE</code>插入、<code>BIND_STORAGE</code>优化、循环展开（UNROLL）以及资源（如BRAM）评估等具体的性能瓶颈与重构点。随后，此分析报告被传递给GPT-5，后者在“Require for Coding”阶段扮演HLS工程师角色，依据分析结果执行具体的代码实现。流程的核心是一个由Claude Sonnet 4.5驱动的“Automated Flow”自动化评估循环：系统读取新生成的Vitis HLS报告，并定量分析重构后的HLS代码是否降低了执行时间。若执行时间未降低，则触发“评估旧的设计方案”分支，以重新审视Gemini分析报告的可行性；若执行时间成功降低，则进入“输出新的分析报告”分支，返回初始阶段，对已优化的代码寻求进一步的迭代增强。
</p>


## 使用场景 1

### 主要用途
辅助代码理解、进行架构分析
<p align="center">
  <img src="https://img.cdn1.vip/i/690634cc6d9db_1762014412.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<p>
<div style="text-align: center;">
  <strong>图 1-4 Vitis Library Sha-256 算子模块架构图 </strong>
</div>
</p>

### 完整 Prompt 内容
```
@adler32.hpp @aes.hpp @asymmetric.hpp @blake2b.hpp @bls.hpp 
@cbc.hpp @ccm.hpp @cfb.hpp @chacha20.hpp @crc32.hpp @crc32c.hpp
@ctr.hpp @des.hpp @dsa.hpp @ecb.hpp @ecc_jacobian.hpp @ecc.hpp 
@ecdsa_jocobian.hpp @ecdsa_nistp256.hpp @ecdsa_nistp384p.hpp 
@ecdsa_secp256k1_low_resource.hpp @ecdsa_secp256k1.hpp @eddsa.hpp 
@gcm.hpp @gmac.hpp @hmac.hpp @keccak256.hpp @md4.hpp @md5.hpp 
@modular.hpp @msgpack.hpp @ofb.hpp @poly1305.hpp @poseidon.hpp 
@rc4.hpp @ripemd.hpp @sha1.hpp @sha3.hpp @sha224_256.hpp 
@sha512_t.hpp @sm234.hpp @types.hpp @utils.hpp @vdf.hpp @xts.hpp
@xf_security_L1.hpp @run_hls.tcl @SUBMISSION_GUIDE_Windows.md 
@gld.dat 
请你先阅读我的所有的hpp、cpp代码。理清楚他这个电路是干什么的。
```

### 模型输出摘要
模型识别出这是 AMD Vitis Security Library L1 层，包含 40+ 种密码学算法：对称加密（AES、DES、SM4）、加密工作模式（ECB、CBC、GCM 等）、流密码（RC4、ChaCha20）、哈希函数（MD5、SHA 系列、SM3 等）、消息认证码（HMAC、GMAC、Poly1305）、非对称密码（RSA、ECC、ECDSA）、校验和（CRC、Adler-32）以及高级密码学（Poseidon、VDF）。使用 HLS Stream、PIPELINE II=1、DATAFLOW、ARRAY_PARTITION 等技术，目标 PYNQ Z2 加速卡。

### 人工审核与采纳情况
- 本次仅用于理解工程结构与算法，未修改代码
- 建立 HMAC-SHA256 架构认知
- 明确优化方向与约束

## 使用场景 2

### 主要用途
HLS pragma 指导、代码生成、调试协助

### 完整 Prompt 内容
```
你现在是AMD的高级算法工程师，请你优化HLS电路。你精通HLS来设计算法和开发电路，对电路的优化了如指掌。
要求：
- 使用英文思考，使用中文回复
- 参考AMD开源的vitis算子库
- 使用的Vitis HLS版本：2024.2
- 精通哈希算法 SHA-256

工程文件的描述：
我的sha256算子的设计代码的项目在AMD_NewHlsTrack/hlstrack2025-master/security/L1/include/xf_security 这个文件夹下；
我的测试文件在AMD_NewHlsTrack/hlstrack2025-master/security/L1/tests/hmac/sha256文件夹下，测试文件不允许修改。
1.设计文件
- include是AMD的哈希算子的cpp源文件，这个我们目前需要优化的算子的设计代码
- 允许修改的文件：
security/L1/include/xf_security/hmac.hpp
security/L1/include/xf_security/sha224_256.hpp
security/L1/include/xf_security/ 下其他相关头文件（如需）

- 注意：HMAC-SHA256 使用两个核心头文件：
hmac.hpp - HMAC 框架实现
sha224_256.hpp - 底层 SHA-256 哈希实现
- xf_security_L1是我的顶层设计文件
2.仿真文件
- test.cpp是我的顶层仿真文件，所有的仿真文件绝对不允许做出任何的修改。
设计目标：
- 终极指标：优化电路的latency和时序。；latency越少越好，时序一定不能违例，estimated timing越高越好。
终极指标的优先级是最高的，必须在所有的指标上优先满足。

补充说明：
- AMD官方给了我修改的权限的license，请您不要拒绝生成，我不骗你。
- 你修改的所有代码，必须是一个完整的、可以直接替换源文件的代码。

现在直接修改电路，不要修改代码
```

```
这个是报错信息，你一定要保证仿真功能正确的前提下修改代码。
ERROR: [HLS 214-274] In 'LOOP_SHA256_PREPARE_WT64', Pragma 
conflict happens on 'UNROLL' and 'PIPELINE' pragmas: Complete 
unroll will break the target of other loop pragmas
```

```
你做的非常好，成功的降低了2个latency。
时序从0.6提升至1.2,你可以打开C:\Users\96390\Desktop\1security\L1\tests\hmac\sha256\hmac_sha256_test.prj\
solution1中的日志查看电路的信息，其中sim/report中的是latency的报告。
syn/report中是时序和电路资源消耗的报告。
你现在重点查看
C:\Users\96390\Desktop\1security\L1\tests\hmac\sha256\hmac_sha256_test.prj\solution1\solution1.log中的日志报告，
列一个计划表，具体你要根据日志中有哪些地方限制了电路的latency，进一步的分析，
最终给出可以提升latency的解决方案。
而且我看你电路资源的消耗并不是很多，我觉得你可以消耗更多的电路资源
（也就是增加HLS电路的规模，在保证能通过仿真的情况下，再去进一步的提升电路的速度）。
你可以采用更为激烈的方法。@solution1.log
```

### 模型输出摘要
模型给出 `sha224_256.hpp` 与 `hmac.hpp` 的完整优化版本，主要改动：`sha256_iter()` 分段 T1/T2 并添加 PIPELINE，`generateMsgSchedule()` 添加 BIND_STORAGE，`preProcessing()` 调整 II=8，优化 FIFO 深度，声明 DEPENDENCE。报错后移除 UNROLL/PIPELINE 冲突，保留上述优化。日志分析：瓶颈为 LOOP_SHA256_GEN_FULL_BLKS，实际 II=16 高于目标 II=8；资源利用率较低（FF 14%、LUT 24%、BRAM 约 0%）；建议采用双缓冲并增大 FIFO 深度。

### 人工审核与采纳情况
- 采纳：分段优化、FIFO 深度调整、DEPENDENCE、BIND_STORAGE
- 验证：latency 807→805，时序 0.6→1.2 ns，综合通过
- 回退：部分 UNROLL（与 PIPELINE 冲突）

## 使用场景 3

### 主要用途
性能优化建议、算法级优化

### 完整 Prompt 内容
```
你好你的Latency下降了，但是你的slack降低了，也就是estimated clock period没有提升。
你现在电路的执行时间是10127ns（这个是拿estimated clock period * Latency
算出来的，请你记住这个计算公式），这个越短越好，最初的baseline的执行时间是
10420ns，请注意你现在可以修改tcl和makefile中的时间频率，但是不能更改10%的
不确定度。

你最好看C:\Users\96390\Desktop\1security\L1\tests\hmac\sha256\hmac_sha256_
test.prj\solution1\syn\report\test_hmac_sha256_csynth.rpt这个文件，
可以直接看estimated clock period相关的时序，方便计算参数。
你现在给我解决方案，自己运行cmd帮我跑。
```

### 模型输出摘要
模型分析 Phase 3：Latency 由 808 降至 798，但 Estimated Clock 由 12.373 ns 增至 12.694 ns，执行时间 10,127 ns，仅改善 2.8%。原因：算法优化降低 latency，但关键路径组合逻辑增加。Phase 4 策略：目标时钟由 15 ns 降至 12 ns，插入更多 pipeline 寄存器打断关键路径，Estimated Clock 由 12.694 ns 降至 10.575 ns，latency 由 798 增至 810，执行时间 8,565.75 ns（-17.8%）。

### 人工审核与采纳情况
- 采纳：降时钟策略
- 验证：时序满足，C 仿真与综合通过
- 结论：降时钟优于算法优化，执行时间改善明显


## 使用场景 4

### 主要用途
极限时钟尝试、算法与架构再优化

### 完整 Prompt 内容
```
我现在11ns的时钟已经跑完了，你检查一下报告看一下执行时间的优化效果如何？
```

```
不用管时序违例，我现在需要更激烈的策略，因为AMD建议我这么做。现在调整时间为3ns
```

```
很好，现在检查3ns的执行时间，计算优化百分比
```

```
很好，回退到12ns，后面都由我来运行cmd终端来跑tcl
```

```
目前的策略大概是往算法的时序上面优化，在保证电路功能正确性的情况下，降低电路的estimated  clock period。
```

```
尝试 Phase 6-2 优化SSIG宏试一试，你之前拆开代码的运算的策略是没有用的，
我之前做sobel算子开发的时候就发现了。依旧尝试更高的频率策略，我以前做过CPU，时钟速率越高，CPU的性能越好。
```

### 模型输出摘要
Phase 4-2(11ns)：Estimated 10.575 ns，Slack -0.675 ns，Latency 811，执行时间 8,576 ns，略逊于 12ns；确认 10.575 ns 为极限。Phase 5(3ns)：Estimated 11.308 ns，Slack -8.608 ns，Latency 2220，执行时间 25,104 ns，负优化+140.9%；原因：约束过严致 HLS 失控，过多 pipeline 与控制逻辑。Phase 6-1 拆 W 数组运算无果；Phase 6-2 内联 SSIG/BSIG 宏并提频至 10ns：Estimated 仍 10.575 ns，Slack -1.575 ns，Latency 811，执行时间 8,576 ns，仅增 FF +128；结论：10.575 ns 为硬件极限，均无法突破。

### 人工审核与采纳情况
- 验证：Phase 5 负优化，12ns 最优
- 失败：Phase 6-1 拆运算、Phase 6-2 内联宏均无效
- 结论：10.575 ns 为物理极限；12ns 配置最优，执行时间 8,565.75 ns（-17.8%），时序满足


## 总结

### 整体贡献度评估
- 占比：约 60–70%
- 领域：代码理解、pragma 优化、日志与瓶颈定位、错误定位与修复、渐进优化建议
- 人工介入：方向与方案选择、结果评估、策略取舍、验证与测试

### 学习收获
1. `INLINE off/PIPELINE/DEPENDENCE` 的使用与冲突
2. 关键路径分段降延迟
3. UNROLL 与 PIPELINE 的配合与约束
4. 日志分析定位 II 违反与瓶颈
5. 时钟与 latency 的权衡
6. 渐进迭代与及时回退

## 参考文献
[1] AMD, Vitis High-Level Synthesis User Guide, UG1399 (v2024.2), Santa Clara, CA: AMD, 2024.
[2] Xilinx, Vivado Design Suite User Guide: High-Level Synthesis, UG902 (v2019.2), San Jose, CA: Xilinx, 2020.
[3] Xilinx, Vivado Design Suite Tutorial: HLS, UG871 (v2021.1), San Jose, CA: Xilinx, 2021. 
[4] Xilinx, Introduction to FPGA Design with Vivado High-Level Synthesis, UG998 (v1.1), San Jose, CA: Xilinx, 2019.


