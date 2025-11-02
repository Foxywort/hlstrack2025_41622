# ZYNQ7020 时序优化摘要

## 已实施的关键优化

1. **SHA256 核心迭代 (`sha256_iter`)**
   - 保持 `II=1` 并启用 `#pragma HLS PIPELINE style=flp`
   - 所有 32 位加法绑定到 DSP48E1 (`#pragma HLS bind_op ... impl=dsp`)
   - 使用 `#pragma HLS EXPRESSION_BALANCE off` 防止EDA重新组合表达式，确保DSP加速链稳定

2. **消息调度 (`generateMsgSchedule`)**
   - 同样启用 `style=flp` 以允许工具自动插入寄存器
   - 固定索引访问 + 移位寄存器结构，避免 sparse mux
   - 关键加法使用 DSP48E1，加上 `EXPRESSION_BALANCE off`

## 预期收益

- 将 SHA256 关键路径从 ~9.6 ns 降至 7-8 ns（估计）
- DSP48 使用预计保持在 14-16 个，占总资源 <10%
- 不改变算法逻辑，测试向量无需更新

## 建议的验证步骤

1. 重新运行 Vitis HLS 综合
   ```bash
   cd L1/tests/hmac/sha256
   vitis_hls -f run_hls.tcl
   ```
2. 查看 `solution1/syn/report/sha256_iter_csynth.rpt` 与 `sha256Digest_256_*` 报告中的 Estimated Clock
3. 如需进一步收紧时序，建议：
   - 检查 FIFO 深度与 back-pressure
   - 考虑在上层数据流加 `#pragma HLS DATAFLOW`
   - 若 Vivado 实现仍有 7ns 以上路径，可在实现阶段使用 `phys_opt_design`

## 后续跟踪

- 若综合结果仍 >8ns，可继续分析 Expression LUT 占比，确定是否需要额外的逻辑重构
- 如需更多 DSP 资源，可限定 `set_directive_allocation` 避免过度占用
