# 🔍 HMAC-SHA256 Dataflow深度分析报告

## 执行摘要

通过分析Vitis HLS的schedule viewer和synthesis报告，我发现了**807 cycles的latency瓶颈根本原因**。

**关键发现**：
- ✅ Latency: 807 cycles (实测)
- ✅ 资源利用率: FF 14%, LUT 24% (**远未饱和，有巨大优化空间**)
- ✅ Timing slack: 1.04ns (满足时序要求)
- 🔴 **真正的瓶颈**: LOOP_SHA256_GEN_FULL_BLKS 存在 II=16 违例

---

## 📊 PART 1: 整体Dataflow结构分析

### 层级1: HMAC顶层 (hmacDataflow)
```
test_hmac_sha256
└── hmacDataflow (dataflow, 6 states)
    ├── kpad                    [生成kipad/kopad]
    ├── msgHash (dataflow)      [第一次SHA256: (kipad || msg)]
    │   ├── mergeKipad          [合并kipad和msg]
    │   └── hash → sha256_top   [SHA256计算]
    └── resHash (dataflow)      [第二次SHA256: (kopad || H1)]
        ├── mergeKopad          [合并kopad和第一次hash结果]
        └── hash_1 → sha256_top [SHA256计算]
```

**Dataflow FSM States**:
- State 1-2: `kpad` 执行
- State 3-4: `msgHash` 执行
- State 5-6: `resHash` 执行

**问题**: 这3个阶段是**串行执行**的（State 1→2→3→4→5→6），没有overlap！

---

## 📊 PART 2: SHA256内部Dataflow分析 (sha256_top)

### SHA256 Dataflow Pipeline (8 states)
```
sha256_top (dataflow)
├── preProcessing          → blk_strm (512-bit, depth=4)
├── dup_strm               → nblk_strm1, nblk_strm2
├── generateMsgSchedule    → w_strm (32-bit, depth=64)
└── sha256Digest           ← w_strm
```

**Dataflow FSM States** (from sha256_top.verbose.sched.rpt):
- State 1-2: `preProcessing` 执行
- State 3-4: `dup_strm` 执行
- State 5-6: `generateMsgSchedule` 执行
- State 7-8: `sha256Digest` 执行

**这里也是串行执行！** 理论上dataflow应该允许这4个模块overlap，但实际没有发生。

---

## 🔥 PART 3: 关键瓶颈分析

### 瓶颈1: **LOOP_SHA256_GEN_FULL_BLKS的II违例** ⭐⭐⭐⭐⭐

**位置**: `preProcessing → LOOP_SHA256_GEN_FULL_BLKS`

**csynth.rpt Line 40**:
```
o LOOP_SHA256_GEN_FULL_BLKS    |    II|  13.50|       16|    240.000|        17|       16|     1|       yes
```

**关键信息**:
- **Issue Type: II** ← **这是问题的根源！**
- **Latency**: 16 cycles (处理1个block)
- **Iteration Latency**: 17 cycles
- **Target II**: **应该是1**
- **Achieved II**: **16** ← 🔴 **违例！**
- **Trip Count**: 1 (只处理1个块)

**实际影响**:
- 目标: 1个block在17 cycles内完成
- 实际: 因为II=16，每个block需要 **17 + (1-1)×16 = 17 cycles**
- **如果有多个连续块，延迟会线性增加！**

**为什么II=16？**
根据代码分析 (`sha224_256.hpp:547-569`):
```cpp
LOOP_SHA256_GEN_FULL_BLKS:
    for (uint64_t j = 0; j < uint64_t(len >> 6); ++j) {
#pragma HLS loop_tripcount min = 0 max = 1
        SHA256Block b0;
#pragma HLS array_partition variable = b0.M complete
        
    // AGGRESSIVE: Fully unroll inner loop for maximum parallelism
    LOOP_SHA256_GEN_ONE_FULL_BLK:
        for (int i = 0; i < 16; ++i) {
#pragma HLS UNROLL
            uint32_t l = msg_strm.read();  // ← 这里！
            // byte swap
            l = ((0x000000ffUL & l) << 24) | ...;
            b0.M[i] = l;
        }
        blk_strm.write(b0);  // ← FIFO写入
    }
```

**根本原因**:
1. **内层循环完全展开** (`UNROLL`)，产生16个并行的 `msg_strm.read()`
2. **FIFO读取冲突**: msg_strm是单端口FIFO，**一个周期只能读1个数据**
3. **资源限制**: 虽然展开了，但FIFO不支持16路并行读，导致**16个读操作串行化**
4. **II=16**: 工具自动插入stall，每16个周期才能启动下一次迭代

**结论**: **我的UNROLL优化适得其反！** ❌

---

### 瓶颈2: **sha256Digest的70 cycles latency**

**位置**: `sha256Digest_256_s → LOOP_SHA256_DIGEST_NBLK`

**csynth.rpt Line 55**:
```
o LOOP_SHA256_DIGEST_NBLK    |     -|  13.50|       70|  1.050e+03|        70|        -|     1|        no
```

**分析**:
- **Latency**: 70 cycles (处理1个block)
- **Not pipelined** (无pipeline)
- 内部有 `LOOP_SHA256_UPDATE_64_ROUNDS` (67 cycles, pipelined II=1)
- **结构**:
  ```
  LOOP_SHA256_DIGEST_NBLK (70 cycles):
    - 初始化: ~3 cycles
    - LOOP_SHA256_UPDATE_64_ROUNDS: 67 cycles (64 rounds × II=1 + 启动开销)
      └── sha256_iter (1 cycle per call, pipelined)
    - 累加hash: ~0 cycles (pipeline overlap)
  ```

**为什么70 cycles？**
- 64轮压缩函数: 理论64 cycles (II=1)
- 实际67 cycles: pipeline启动需要3个cycle的填充
- 总计70 cycles: 67 + 3 (初始化和结束处理)

**这部分已经优化得很好！** ✅

---

### 瓶颈3: **generateMsgSchedule的73 cycles latency**

**位置**: `generateMsgSchedule → VITIS_LOOP_527_2`

**csynth.rpt Line 48**:
```
o VITIS_LOOP_527_2    |     -|  13.50|        -|          -|        73|        -|     -|        no
```

**内部结构**:
- `LOOP_SHA256_PREPARE_WT16`: 18 cycles (16个W值, II=1 → 16+2启动)
- `LOOP_SHA256_PREPARE_WT64`: 50 cycles (48个W值, II=1 → 48+2启动)
- **总计**: 18 + 50 + 5 (overhead) = **73 cycles**

**这部分也已经很好！** ✅ (II=1已达到最优)

---

## 📈 PART 4: Latency分布计算

### 单个SHA256操作 (1个block)

**理想情况 (完美dataflow)**:
```
preProcessing:      17 cycles (不含II违例)
generateMsgSchedule: 73 cycles } 可以overlap
sha256Digest:       70 cycles }
------------------------
总计: Max(73, 70) + 17 + overhead ≈ 90-100 cycles
```

**实际情况 (串行执行)**:
```
preProcessing:      17 cycles (但II=16影响后续块)
dup_strm:            6 cycles
generateMsgSchedule: 73 cycles } 部分overlap
sha256Digest:       70 cycles }
------------------------
总计: 17 + 6 + 73 + 70 = 166 cycles per block
```

### HMAC-SHA256完整流程 (Test 0: 80字节消息)

**第一次SHA256** (kipad + msg = 144字节):
- Block 0: kipad (64字节) → 166 cycles
- Block 1: msg (64字节) → 166 cycles
- Block 2: msg (16字节 + padding) → 166 cycles
- **小计**: 3 × 166 = **498 cycles**

**第二次SHA256** (kopad + H1 = 96字节):
- Block 0: kopad (64字节) → 166 cycles
- Block 1: H1 (32字节 + padding) → 166 cycles
- **小计**: 2 × 166 = **332 cycles**

**HMAC overhead**:
- kpad: ~20 cycles
- mergeKipad: ~50 cycles
- mergeKopad: ~40 cycles
- **小计**: ~110 cycles

**总计 (单个HMAC)**:
```
498 + 332 + 110 = 940 cycles (单个HMAC)
```

**2个HMAC (测试中)**:
```
940 × 2 = 1880 cycles (如果完全串行)
```

**实际测量**: **807 cycles** ← **为什么这么少？**

---

## 🤔 PART 5: 为什么实际Latency是807而不是1880？

**答案**: **有部分dataflow overlap！**

**分析**:
1. **HMAC层的dataflow**: 虽然kpad/msgHash/resHash在FSM上是串行的，但：
   - msgHash和resHash可以**部分overlap** (kopad可以提前准备)
   - 2个HMAC之间也有**部分overlap** (第二个HMAC的kpad可以在第一个HMAC的resHash执行时启动)

2. **实际overlap估算**:
   - 理想串行: 2 × 940 = 1880 cycles
   - 实际测量: 807 cycles
   - **Overlap效率**: (1880 - 807) / 1880 = **57%**

3. **为什么overlap不完美？**
   - **FIFO depth不足**: 导致生产者等待消费者
   - **II违例**: `LOOP_SHA256_GEN_FULL_BLKS` 的II=16拖慢了preProcessing
   - **dataflow handshake开销**: 模块启动和同步需要额外cycles

---

## 🎯 PART 6: 优化方案

### ❌ 错误的优化 (我之前做的)

1. **完全展开 `LOOP_SHA256_GEN_ONE_FULL_BLK`**
   - 导致II=16违例
   - 反而增加了latency (如果处理多块)
   - **应该回退！**

2. **盲目增加FIFO depth**
   - w_strm从32→64: **没必要** (generateMsgSchedule和sha256Digest已经很好地overlap)
   - 反而增加资源消耗

---

### ✅ 正确的优化方向

#### **方案1: 修复II违例** ⭐⭐⭐⭐⭐ (最高优先级)

**问题根源**: `LOOP_SHA256_GEN_ONE_FULL_BLK`完全展开导致FIFO读冲突

**解决方案**:
```cpp
// 方案1A: 部分展开 (factor=4)
LOOP_SHA256_GEN_ONE_FULL_BLK:
    for (int i = 0; i < 16; i+=4) {
#pragma HLS UNROLL factor=4
        uint32_t l0 = msg_strm.read();
        uint32_t l1 = msg_strm.read();
        uint32_t l2 = msg_strm.read();
        uint32_t l3 = msg_strm.read();
        // byte swap (4个并行)
        b0.M[i] = SWAP(l0);
        b0.M[i+1] = SWAP(l1);
        b0.M[i+2] = SWAP(l2);
        b0.M[i+3] = SWAP(l3);
    }
```
- **目标II**: 4 (from 16)
- **Latency减少**: 从17变为 17/4 ≈ 5-6 cycles
- **每个block节省**: 11 cycles
- **总体节省**: 10块 × 11 = **110 cycles**

**方案1B: 完全移除UNROLL pragma (回退)**
```cpp
LOOP_SHA256_GEN_ONE_FULL_BLK:
    for (int i = 0; i < 16; ++i) {
#pragma HLS PIPELINE II=1
        uint32_t l = msg_strm.read();
        l = SWAP(l);
        b0.M[i] = l;
    }
```
- **目标II**: 1
- **Latency**: 16 + 2 = 18 cycles (from 17 but better for multi-block)
- **更稳定，无II违例风险**

**推荐**: **方案1B** (简单且安全)

---

#### **方案2: 双SHA256核心并行** ⭐⭐⭐⭐⭐

**核心思想**: HMAC需要2次SHA256，可以**复制SHA256核心**实现真正的并行

**架构**:
```cpp
// hmac.hpp 修改
void hmacDataflow(...) {
#pragma HLS DATAFLOW
    
    // 原有的kpad
    kpad(...);
    
    // 🔥 新增: 双SHA256核心
    #pragma HLS DATAFLOW
    {
        // 第一次SHA256 (kipad || msg)
        hash_core1(kipadStrm, msgStrm, ...);
        
        // 第二次SHA256 (kopad || H1) - 并行执行！
        hash_core2(kopadStrm, msgHashStrm, ...);
    }
}
```

**预期效果**:
- **串行执行**: 498 + 332 = 830 cycles
- **并行执行**: Max(498, 332) = **498 cycles**
- **节省**: 830 - 498 = **332 cycles** per HMAC
- **总节省**: 2 × 332 = **664 cycles**

**代价**:
- **资源翻倍**: SHA256核心占用 ~5400 FF + 4900 LUT
- **新资源**: 10800 FF (10%) + 9800 LUT (18%)
- **总资源**: 15093 + 5400 = 20493 FF (19%) ← **仍在目标内！**

**可行性**: ✅ **非常可行** (当前资源只用24% LUT)

---

#### **方案3: 消息预处理并行化** ⭐⭐⭐

**核心思想**: 将`mergeKipad`和第一个SHA256 block的`preProcessing`并行

**当前架构**:
```
mergeKipad (50 cycles) → preProcessing (17 cycles)
       ↓ (串行)                 ↓
   SHA256开始
```

**优化后架构**:
```
mergeKipad (50 cycles)
       ↓ (生成到深FIFO)
   preProcessing (17 cycles) ← 可以提前开始！
       ↓
   SHA256核心
```

**方法**: 增加`mergeKipadStrm` FIFO depth (从128→256)，允许`hash`模块提前启动

**预期节省**: ~30-40 cycles (减少stall时间)

---

#### **方案4: SHA256 64轮循环2路展开** ⭐⭐⭐

**当前代码**:
```cpp
LOOP_SHA256_UPDATE_64_ROUNDS:
    for (short t = 0; t < 64; ++t) {
#pragma HLS pipeline II = 1
        sha256_iter(a, b, c, d, e, f, g, h, w_strm, Kt, K, t);
    }
```

**优化方案**: 2路并行处理
```cpp
LOOP_SHA256_UPDATE_32_ROUNDS:
    for (short t = 0; t < 64; t += 2) {
#pragma HLS pipeline II = 1
        // Round t
        sha256_iter_dual(a, b, c, d, e, f, g, h, 
                         w_strm, Kt, K, t, t+1);
        // 内部计算t和t+1两轮
    }
```

**挑战**:
- Round t+1依赖Round t的结果 ← **数据依赖！**
- 需要打破依赖链

**解决方案**: 使用**寄存器复制**和**前瞻计算**
```cpp
inline void sha256_iter_dual(..., short t, short t_next) {
#pragma HLS INLINE
    // Round t
    uint32_t T1_t = h + BSIG1(e) + CH(e, f, g) + K[t] + Wt;
    uint32_t T2_t = BSIG0(a) + MAJ(a, b, c);
    
    // 更新中间状态
    uint32_t h_new = g;
    uint32_t g_new = f;
    uint32_t f_new = e;
    uint32_t e_new = d + T1_t;
    uint32_t d_new = c;
    uint32_t c_new = b;
    uint32_t b_new = a;
    uint32_t a_new = T1_t + T2_t;
    
    // Round t+1 (使用新状态)
    uint32_t Wt_next = w_strm.read();
    uint32_t T1_next = h_new + BSIG1(e_new) + CH(e_new, f_new, g_new) + K[t_next] + Wt_next;
    uint32_t T2_next = BSIG0(a_new) + MAJ(a_new, b_new, c_new);
    
    // 最终更新
    h = g_new;
    g = f_new;
    f = e_new;
    e = d_new + T1_next;
    d = c_new;
    c = b_new;
    b = a_new;
    a = T1_next + T2_next;
}
```

**预期效果**:
- **Latency**: 67 cycles → 67/2 = **34 cycles**
- **每个block节省**: 33 cycles
- **总体节省**: 10块 × 33 = **330 cycles**

**代价**:
- **组合逻辑路径加倍** → 可能无法满足时序
- **需要仔细pipeline插入和寄存器复制**

**可行性**: ⚠️ **有风险** (时序可能违例，需要尝试)

---

## 📊 PART 7: 优化方案对比

| 方案 | 预期Latency减少 | 资源增加 | 实现难度 | 时序风险 | 推荐度 |
|------|----------------|---------|---------|---------|--------|
| **方案1B: 修复II违例** | -110 cycles | 0% | ⭐ 简单 | ✅ 低 | ⭐⭐⭐⭐⭐ |
| **方案2: 双SHA256核心** | -332 cycles | +10% FF/LUT | ⭐⭐⭐ 中等 | ✅ 低 | ⭐⭐⭐⭐⭐ |
| **方案3: 预处理并行化** | -40 cycles | <1% | ⭐ 简单 | ✅ 低 | ⭐⭐⭐ |
| **方案4: 2路展开64轮** | -330 cycles | +5% LUT | ⭐⭐⭐⭐ 复杂 | ⚠️ 高 | ⭐⭐ |

**组合方案 (推荐)**:
- **Phase 1**: 方案1B (修复II违例) → 807 - 110 = **697 cycles** ✅
- **Phase 2**: 方案1 + 方案2 (双核心) → 697 - 332 = **365 cycles** ✅✅
- **Phase 3**: 方案1+2+3 (加预处理) → 365 - 40 = **325 cycles** ✅✅✅

**预计最终Latency**: **325-400 cycles** (远低于600目标！)

---

## 🎯 PART 8: 行动计划

### Phase 1: 快速修复 (今天完成)
1. ✅ **回退错误的UNROLL优化** (最优先)
   - 移除 `LOOP_SHA256_GEN_ONE_FULL_BLK` 的 `#pragma HLS UNROLL`
   - 改为 `#pragma HLS PIPELINE II=1`
2. ✅ **验证II修复效果**
   - 目标: Latency降至 ~700 cycles

### Phase 2: 架构级优化 (明天)
1. 🔥 **实现双SHA256核心**
   - 复制`sha256_top`为两个独立模块
   - 修改dataflow连接
2. ✅ **增加中间FIFO depth**
   - `mergeKipadStrm`: 128 → 256
3. ✅ **测试验证**
   - 目标: Latency降至 ~400 cycles

### Phase 3: 高级优化 (后天，如需要)
1. ⚠️ **尝试2路展开64轮**
   - 仅当Phase 2未达到600目标时
2. ✅ **时序收敛**
   - 调整pipeline插入策略

---

## 📌 总结

### 关键发现
1. 🔴 **II=16违例是主要瓶颈** (我的错误优化导致)
2. 🔴 **Dataflow overlap不足** (只有57%效率)
3. ✅ **资源利用率极低** (24% LUT → 可以消耗到80%)
4. ✅ **SHA256核心已经很高效** (70 cycles/block已接近理论极限)

### 根本问题
> **"不是单个模块太慢，而是模块间没有并行起来！"**

### 正确方向
> **"通过复制硬件实现真正的并行，而不是盲目优化单个模块"**

---

**准备好进入Phase 1优化了吗？** 🚀

