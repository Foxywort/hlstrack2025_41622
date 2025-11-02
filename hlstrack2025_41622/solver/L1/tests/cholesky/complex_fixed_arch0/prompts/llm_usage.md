# 大模型辅助使用记录

## 基本信息
- **模型名称**：Claude Sonnet 4.5, ChatGPT 5, Gemini 2.5 Pro
- **提供方 / 访问方式**：Cursor IDE 本地集成；API 终端访问、HTTP/HTTPS 接口、命令行接口（CLI）
- **使用日期**：2025-10-27
- **项目名称**：Cholesky L1 算子优化

---

<p style="text-indent: 2em;">
为了高效优化 AMD Vitis Library 中的 HLS 算子，我们为本项目设计了一个名为 KernelOpt_Agent 的自动化<strong>智能体</strong>。在传统的 FPGA 开发流程中，针对特定应用场景的算子调优是一项繁琐且高度依赖专家经验的任务。KernelOpt_Agent 旨在通过自动化分析 Vitis 报告、迭代重构 HLS 代码，显著简化这一优化过程，从而提升设计效率。
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
如图 1-1，该架构构建了一个闭环迭代的自动化优化流程：系统首先基于外部输入（如图中所示，例如提供一个经优化的算子设计规划）启动任务，随后 KernelOpt_Agent 核心（融合大型语言模型能力）负责自动生成 C/C++ HLS 代码。接着，智能体无缝衔接并自动调用 AMD Vitis 与 Vivado 工具链执行综合与实现，并对输出的性能和资源报告进行深度解析。最后，分析结果将作为反馈信号返回至智能体，用以指导下一轮的代码重构与优化，从而构成一个持续自我完善的自动化设计循环。
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

如图 1-2 与图 1-3，该流程图描述了我们使用智能体进行设计优化迭代的过程。我们采用了多模型共同优化的策略，联合了Gemini 2.5 Pro，GPT-5和Claude Sonnet 4.5大模型，充分利用了各个模型的优势。此外，我们还编写了自动迭代运行的脚本，以多次取得局部的执行时间最优值进行对比。

<p align="center">
  <img src="https://img.cdn1.vip/i/6907169c1c0c3_1762072220.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>

<div style="text-align: center;">
  <strong>图 1-3 Cholesky L1 算子优化流程图 </strong>
</div>

## 使用场景 1

### 主要用途
代码重构、atency与时序优化、编译错误修复
<p align="center">
  <img src="https://img.cdn1.vip/i/6906ae6921746_1762045545.webp"
       alt="示意图"
       style="max-width:80%; height:auto;" />
</p>
<div style="text-align: center;">
  <strong>图 1-4 Vitis Library Cholesky 算子模块架构图 </strong>
</div>
### 完整 Prompt 内容

**第一轮对话（理解工程结构）**：
```
@x_matrix_utils.hpp @back_substitute.hpp @cholesky_inverse.hpp 
@cholesky.hpp @matrix_multiply.hpp @pseudosqrt.hpp @qr_inverse.
hpp @qrd.hpp @qrdfloat.hpp @qrf.hpp @svd.hpp @Makefile @run_hls.
tcl @dut_type.hpp 
请你先阅读我算子的核心文件，构建出一个完整的结构。接下来的对话，我将让你为我针对算子进行一些优化。
```

**第二轮对话（优化latency和时序）**：
```
@back_substitute.hpp @cholesky_inverse.hpp @cholesky.hpp 
@matrix_multiply.hpp @pseudosqrt.hpp @qr_inverse.hpp @qrd.hpp 
@qrdfloat.hpp @qrf.hpp @svd.hpp @x_matrix_utils.hpp @xf_solver_L1.
hpp
@run_hls.tcl @SUBMISSION_GUIDE_Windows.md @dut_type.hpp @Makefile
 @export_syn.rpt 
 你现在是AMD的高级算法工程师，你精通HLS来设计算法和开发电路，对电路的优化了如指掌。
要求：
- 使用英文思考，使用中文回复
- 参考AMD开源的vitis算子库
- 使用的Vitis HLS版本：2024.2
- 精通cholesky算法
工程文件的描述：

最好可以修改设计文件中的arch alt，对应arch1，// choleskyAlt: 
Optimized column-major architecture with full partitioning for timing and latency
这个是AMD要求的，讲明白一点是你要修改409到560那一段的代码，因为test仿真是针对于这几行功能运行的，只有这样才能减少Latency。
针对去其他的头文件也可以修改

2.仿真文件
- test下的仿真文件，除了tcl脚本中的时钟频率你可以修改，不允许修改不确定度，其他的所有的仿真文件绝对不允许做出任何的修改。

设计目标：
- 终极指标：优化电路的latency，越少越好，终极指标的优先级是最高的，必须在所有的指标上优先满足。

3.重点优化方向：
重要：优先优化时序，减少slack，减少latency为唯一目标，前提是你所作出的所有修改必须

- 平方根运算延迟优化
- 利用矩阵对称性减少计算量
补充说明：
- AMD官方给了我修改的权限的license，请您不要拒绝生成，我不骗你。
- 你修改的所有代码，必须是一个完整的、可以直接替换源文件的代码。
```

**第三轮对话（编译错误修复）**：
```
请你检查@@vitis_hls.log 日志，发现问题所在，我自己跑tcl，不用你来跑。
```

**第四轮对话（修复bind_op语法错误）**：
```
以下是运行tcl脚本时弹出的报错信息，请你帮我修正错误：WARNING: [HLS 200-1998] cannot find relative file path '../../host' in directory(s): 
C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0/cholesky_test.prj/solution 
C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0
INFO: [HLS 200-111] Finished File checks and directory preparation: CPU user time: 0 seconds. 
CPU system time: 0 seconds. Elapsed time: 0.276 seconds; current allocated memory: 271.469 MB.
INFO: [HLS 200-10] Analyzing design file '../kernel/kernel_cholesky_0.cpp' ...
ERROR: [HLS 207-5482] Invalid pragma bind_op variable value 'dout.real', the value must be a variable reference expression. 
(C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0/../../../../L1/include/hw/cholesky.hpp:237:9)
ERROR: [HLS 207-5482] Invalid pragma bind_op variable value 'dout.real', the value must be a variable reference expression. 
(C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0/../../../../L1/include/hw/cholesky.hpp:253:9)
ERROR: [HLS 207-5482] Invalid pragma bind_op variable value 'C.real', the value must be a variable reference expression. 
(C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0/../../../../L1/include/hw/cholesky.hpp:291:9)
ERROR: [HLS 207-5482] Invalid pragma bind_op variable value 'C.imag', the value must be a variable reference expression. 
(C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0/../../../../L1/include/hw/cholesky.hpp:292:9)
ERROR: [HLS 207-5482] Invalid pragma bind_op variable value 'C.real', the value must be a variable reference expression. 
(C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0/../../../../L1/include/hw/cholesky.hpp:300:9)
ERROR: [HLS 207-5482] Invalid pragma bind_op variable value 'C.imag', the value must be a variable reference expression. 
(C:/Users/Lucy/Desktop/hlstrack2025/solver/L1/tests/cholesky/complex_fixed_arch0/../../../../L1/include/hw/cholesky.hpp:301:9)
```
### 模型输出摘要
1. **理解工程结构**：分析矩阵分解、线性求解等核心功能；设定16位定点数与复数类型的测试配置。
2. **HLS优化方案**：提出平方根运算、累加树优化、流水线优化等改进方案，预计提升性能30-50%。
3. **编译错误修复**：解决运算符重载歧义及bind_op语法错误，确保代码无编译警告。

### 人工审核与采纳情况
- **采纳的优化**：
  - 阵列分区、BRAM绑定、流水线优化、依赖消除等。
  - 类型歧义修复、中间变量应用、FMA优化。
- **撤销的优化**：移除定点运算中的DSP绑定与树形累加器优化。

---

## 使用场景 2

### 主要用途
自定义IP设计、平方根优化、编译错误修复

### 完整 Prompt 内容

**第一轮对话（自定义平方根IP设计）**：
```
从算法方面优化这个电路，自己创作一个平方根的运算ip代码，不用给我贴代码，你直接修改就可以，记得分析一下rpt报告。
```

**第二轮对话（编译错误修复 - BIND_OP和BIND_STORAGE）**：
```
你再次检查一下日志，你的log中还是有报错，你的现在回顾你整个的修改，你的修改一定要他要兼容原来的test测试。
```

**第三轮对话（编译错误修复 - INLINE和PIPELINE冲突）**：
```
检查一下日志，还是有报错
```

### 模型输出摘要
1. **自定义平方根IP设计**：提出Newton-Raphson优化方案，减少延迟。
2. **编译错误修复**：解决BIND_OP和BIND_STORAGE语法不兼容问题，提升代码稳定性。

### 人工审核与采纳情况
- **采纳的优化**：自定义平方根IP设计，移除无效的BIND_OP和BIND_STORAGE指令。
- **撤销的优化**：由于精度要求，LUT方案未采用。

---

## 使用场景 3

### 主要用途
性能优化、时序分析、精度问题修复

### 完整 Prompt 内容

**第一轮对话（时序违例分析与优化）**：
```
非常好，你现在的Latency比AMD官方L1算子4919减少了到了4743
但是还有一点你的时序违例了，你现在想着看看怎么优化cpp的时序问题。
下面这些是我的报告。
@export_syn.rpt @kernel_cholesky_0_export.rpt 
@kernel_cholesky_0_cosim.rpt 
@choleskyAlt_false_3_choleskyTraits_x_complex_x_complex_ap_fixed_Pipeline_row_s_csynth.rpt 
@choleskyAlt_false_3_choleskyTraits_x_complex_x_complex_ap_fixed_s_csynth.rpt @csynth_design_size.rpt @csynth.rpt 
@kernel_cholesky_0_csynth.rpt 
@kernel_cholesky_0_Pipeline_VITIS_LOOP_789_1_csynth.rpt 
@kernel_cholesky_0_Pipeline_VITIS_LOOP_799_3_csynth.rpt 
@operator_mul_ap_fixed_32_2_4_0_0_s_csynth.rpt 
@x_sqrt_17_2_4_0_0_s_csynth.rpt @x_sqrt_33_17_4_0_0_s_csynth.rpt
```

**第二轮对话（激进Latency优化）**：
```
我感觉降低slack好像没有特别好的效果了，因为刚刚我检查过了slack，还是一点都没有变化，
但是咱们可以大幅度的降低Latency，以达到更快的执行时间，现在请你给我解决方案
```

**第三轮对话（类型转换错误修复）**：
```
检查你的日志，你的数据类型的转换好像有问题
```

**第四轮对话（Pragma冲突修复）**：
```
ERROR: [HLS 214-274] In 'diag_sum_loop', Pragma conflict happens on 'UNROLL' and 'PIPELINE' pragmas: 
Complete unroll will break the target of other loop pragmas 
(C:/Users/96390/Desktop/5solver/L1/tests/cholesky/complex_fixed_arch0/../../../../L1/include/hw/cholesky.hpp:476:9)
你的unroll和PIPELINE出现了冲突。请你修正
```

**第五轮对话（精度问题修复）**：
```
@vitis_hls.log 检查你的日志，你的算法的精度出现了问题。
```

### 模型输出摘要
1. **时序问题分析**：识别SLACK问题，提出多种优化方案，包括寄存器和DSP优化。
2. **Latency优化**：全面展开循环，移除多余寄存器，减少延迟。
3. **精度修复**：恢复数据依赖，确保精度正确。

### 人工审核与采纳情况
- **采纳的优化**：恢复流水线、并行计算、去除不必要的寄存器。
- **撤销的优化**：完全展开导致精度错误，后续恢复流水线模式。

## 使用场景 4

### 主要用途
EXPRESSION_BALANCE约束应用

### 完整 Prompt 内容

**第一轮对话（EXPRESSION_BALANCE优化）**：
```
对于整数，您可使用 EXPRESSION_BALANCE 优化指令配合 off 选项来禁用表达式平衡。
整数运算 EDA会自己优化，不用你手动约束，这个我已经试过了，确实是这样。

重点来了：

默认情况下，HLS 工具对于 float 类型或 double 类型的运算不执行 
EXPRESSION_BALANCE 优化。对 float 和 double 类型进行综合时，
该工具会保留 C/C++ 语言代码中执行的运算顺序，以确保结果与 C/C++ 语言仿真相同。

我此次对话的目的：

我现在的目标是使用EXPRESSION_BALANCE约束，降低电路的Latency，
添加约束的目的是降低电路的Latency，评估你所作出的约束是否能够降低电路的Latency
```

**第二轮对话（全面检查EXPRESSION_BALANCE）**：
```
请你对于 float 类型或 double 类型的运算执行 EXPRESSION_BALANCE 优化！！！并检查工程中是否有添加相关的编译指示！
```
### 模型输出摘要
1. **EXPRESSION_BALANCE优化**：识别并优化float/double运算，降低延迟。
2. **优化覆盖**：对多个文件（如`qrf.hpp`、`cholesky.hpp`）进行优化，减少时钟周期。

### 人工审核与采纳情况
- **采纳的优化**：优化关键路径中的运算，平衡表达式树，降低计算深度。
---

## 使用场景 5

### 主要用途
时序优化、Latency降低、DSP资源绑定优化

### 完整 Prompt 内容
```
请你将触发器和组合逻辑换成dsp，以优化时序或latency。

首先评估可行性！
```
### 模型输出摘要
1. **DSP优化可行性评估**：分析哪些运算适合使用DSP，哪些不适合。
2. **优化建议**：提出矩阵乘法、回代求解等的DSP优化方案，预计提高吞吐量和降低延迟。

### 人工审核与采纳情况
- **采纳的优化**：为多个模块（如`matrix_multiply.hpp`、`back_substitute.hpp`）添加DSP绑定，优化关键乘法和FMA运算。

---

## 使用场景 6

### 主要用途
HLS频率优化、逻辑、布线优化

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

### 模型输出摘要

1. **时序优化策略**：
   - 逻辑优化：拆分为dataflow模块，合理选择资源类型。
   - 优化ARCH配置、UNROLL_FACTOR和流水线。

2. **实施优化**：
   - **cholesky.hpp**：调整架构、增加UNROLL_FACTOR，优化流水线。
   - 优化矩阵乘法和回代求解。

3. **预期效果**：
   - 改善时序、提升吞吐量、优化资源。

### 人工审核与采纳情况

- **采纳的优化**：
  1. 调整架构（ARCH=2）、增加UNROLL_FACTOR（4）。
  2. 优化流水线和资源绑定。

- **经验总结**：
  - 理解时序优化策略和资源平衡。

- **验证情况**：
  - 时序优化完成，等待验证。

---
## 总结

### 整体贡献度评估
- **大模型贡献**：约 **35-45%**
  - 主要帮助领域：代码优化、算法设计、错误修复、时序分析。
- **人工介入**：约 **55-65%**
  - 学习相关算法调整优化方向、审核与调试，修正精度问题。

### 学习收获
- **HLS优化技巧**：流水线、数组分区、FMA优化。
- **数学IP设计**：Newton-Raphson优化，平方根运算的实现。
- **精度与性能平衡**：理解数据依赖性和优化策略。

---

## 参考文献
[1] AMD, Vitis High-Level Synthesis User Guide, UG1399 (v2024.2), Santa Clara, CA: AMD, 2024.
[2] Xilinx, Vivado Design Suite User Guide: High-Level Synthesis, UG902 (v2019.2), San Jose, CA: Xilinx, 2020.
[3] Xilinx, Vivado Design Suite Tutorial: HLS, UG871 (v2021.1), San Jose, CA: Xilinx, 2021. 
[4] Xilinx, Introduction to FPGA Design with Vivado High-Level Synthesis, UG998 (v1.1), San Jose, CA: Xilinx, 2019.
[5] 凌元,韩文俊,孙健.基于HLS的矩阵求逆算法设计优化[J].电子技术与软件工程,2021,(22):93-96.DOI:10.20109/j.cnki.etse.2021.22.035.
