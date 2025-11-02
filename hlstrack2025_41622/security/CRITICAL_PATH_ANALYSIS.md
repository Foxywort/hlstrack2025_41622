# 🔬 关键路径分析与优化方案

## 🎯 当前问题

**Estimated Clock Period = 10.575 ns（物理极限）**

无论 Target Clock 设置为 12ns、11ns 还是 3ns，Estimated Clock Period 都在 10.5-11 ns 左右，说明电路中存在**无法进一步优化的关键路径**。

---

## 📊 关键路径识别

### 1. SHA256 核心宏定义（组合逻辑深度）

**当前实现** (`sha224_256.hpp:40-48`):

```cpp
#define ROTR(n, x) ((x >> n) | (x << (32 - n)))        // 2级逻辑（SHIFT + OR）
#define CH(x, y, z) ((x & y) ^ ((~x) & z))              // 3级逻辑（NOT+AND, AND, XOR）
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))      // 5级逻辑（3×AND + 2×XOR）
#define BSIG0(x) (ROTR(2, x) ^ ROTR(13, x) ^ ROTR(22, x))  // 6级逻辑（3×ROTR + 2×XOR）
#define BSIG1(x) (ROTR(6, x) ^ ROTR(11, x) ^ ROTR(25, x))  // 6级逻辑（3×ROTR + 2×XOR）
#define SSIG0(x) (ROTR(7, x) ^ ROTR(18, x) ^ SHR(3, x))    // 6级逻辑
#define SSIG1(x) (ROTR(17, x) ^ ROTR(19, x) ^ SHR(10, x))  // 6级逻辑
```

### 2. sha256_iter 函数中的关键路径

**当前实现** (`sha224_256.hpp:589-614`):

```cpp
// T1 计算链
uint32_t ch = CH(e, f, g);           // 3级逻辑
uint32_t bsig1_e = BSIG1(e);         // 6级逻辑
uint32_t T1_part1 = h + bsig1_e;    // 加法器（32-bit，6-7级逻辑）
uint32_t T1_part2 = T1_part1 + ch;  // 加法器
uint32_t T1 = T1_part2 + Kt + Wt;   // 2个加法器串联

// T2 计算链（可能的最长关键路径！）
uint32_t maj = MAJ(a, b, c);         // 5级逻辑
uint32_t bsig0_a = BSIG0(a);         // 6级逻辑
uint32_t T2 = bsig0_a + maj;         // 加法器（依赖前两个）

// 最后的更新
e = d + T1;                           // 加法器
a = T1 + T2;                          // 加法器（依赖T1和T2）
```

### 3. 关键路径估算

**最长路径分析**（假设一个时钟周期内执行）：

```
路径1: a → BSIG0(a) → T2 → (T1+T2) → 新a
  BSIG0: 6级逻辑
  MAJ: 5级逻辑（并行）
  T2加法: 6-7级逻辑
  最终加法: 6-7级逻辑
  总计: ~18-20级逻辑

路径2: e → BSIG1(e) → T1 → (T1+T2) → 新a
  BSIG1: 6级逻辑
  CH: 3级逻辑（并行）
  T1加法链: 3个加法器 = 18-21级逻辑
  最终加法: 6-7级逻辑
  总计: ~30-34级逻辑 ← 可能是关键路径！

路径3: W数组生成（generateMsgSchedule）
  SSIG1(W[t-2]): 6级逻辑
  SSIG0(W[t-15]): 6级逻辑
  4个加法器: 24-28级逻辑
  总计: ~36-40级逻辑 ← 最长关键路径！
```

**结论**：
> **W数组生成中的 Wt 计算是最长关键路径！**
> ```cpp
> uint32_t Wt = SSIG1(w_t_minus_2) + w_t_minus_7 + 
>               SSIG0(w_t_minus_15) + w_t_minus_16;
> ```
> 这一行包含：2个复杂函数（各6级）+ 3个加法器串联（18-21级）
> 总计：~36-40级逻辑 → 估算时序 ~10-12 ns

---

## 🚀 优化方案

### 方案1: 优化 W 数组生成的关键路径 ⭐⭐⭐⭐⭐

**当前代码** (`sha224_256.hpp:563`):
```cpp
uint32_t Wt = SSIG1(w_t_minus_2) + w_t_minus_7 + SSIG0(w_t_minus_15) + w_t_minus_16;
```

**问题**：
- SSIG1 和 SSIG0 各有 6 级逻辑
- 3 个加法器串联（每个 6-7 级逻辑）
- 总关键路径：~36-40 级逻辑

**优化方案 A: 拆分加法链**
```cpp
// 拆分成两级加法，插入中间寄存器
uint32_t ssig1_val = SSIG1(w_t_minus_2);
uint32_t ssig0_val = SSIG0(w_t_minus_15);

// 第一组并行加法
uint32_t sum1 = ssig1_val + w_t_minus_7;
uint32_t sum2 = ssig0_val + w_t_minus_16;

// 第二级加法
uint32_t Wt = sum1 + sum2;
```

**预期效果**：
- 关键路径从 3 个串联加法 → 2 个并行 + 1 个串联
- 减少 1 个加法器的延迟（~6-7 级逻辑）
- **预计 Clock Period: 10.575 → 9.5-10 ns**

---

### 方案2: 优化 BSIG0/BSIG1 宏 ⭐⭐⭐⭐

**当前实现**（宏定义）:
```cpp
#define BSIG0(x) (ROTR(2, x) ^ ROTR(13, x) ^ ROTR(22, x))
// 展开后是一个巨大的组合逻辑表达式
```

**优化方案 B: 改用 inline 函数 + 中间变量**
```cpp
static inline uint32_t BSIG0_opt(uint32_t x) {
#pragma HLS INLINE
    // 拆分 XOR 链，插入中间变量
    uint32_t r1 = ROTR(2, x);
    uint32_t r2 = ROTR(13, x);
    uint32_t r3 = ROTR(22, x);
    
    uint32_t xor1 = r1 ^ r2;      // 第一次 XOR
    uint32_t result = xor1 ^ r3;  // 第二次 XOR
    
    return result;
}
```

**预期效果**：
- HLS可以在中间变量处插入寄存器
- 将 6 级逻辑拆分成 2-3 级
- **预计减少 3-4 级逻辑深度**

同理优化 BSIG1, SSIG0, SSIG1

---

### 方案3: 优化 MAJ 函数 ⭐⭐⭐

**当前实现**:
```cpp
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
// 5级逻辑：3个AND + 2个XOR
```

**问题**：
- 3个AND操作需要并行执行
- 2个XOR串联
- 扇出可能较大

**优化方案 C: 逻辑简化**
```cpp
// 利用布尔代数简化
static inline uint32_t MAJ_opt(uint32_t x, uint32_t y, uint32_t z) {
#pragma HLS INLINE
    // MAJ(x,y,z) = (x&y) | (x&z) | (y&z)
    // 可以简化为：(x&y) | ((x|y)&z)
    uint32_t xy = x & y;
    uint32_t x_or_y = x | y;
    uint32_t xyz = x_or_y & z;
    return xy | xyz;
}
```

**预期效果**：
- 从 3个AND+2个XOR → 2个AND+1个OR+1个OR
- 可能略微降低逻辑深度
- **预计减少 1-2 级逻辑**

---

### 方案4: 优化 T1/T2 计算链 ⭐⭐⭐

**当前 T2 计算**:
```cpp
uint32_t maj = MAJ(a, b, c);      // 5级
uint32_t bsig0_a = BSIG0(a);      // 6级
uint32_t T2 = bsig0_a + maj;      // 6-7级（串联）
```

**优化方案 D: 预计算和重排序**
```cpp
// 利用 a, b, c 在多个cycle中相对稳定的特点
// 可以预先计算部分结果

// 在上一个cycle的末尾预计算
uint32_t maj_next = MAJ(b, c, d+T1);  // 预计算下一个cycle的MAJ
// 存储到临时变量

// 当前cycle直接使用
uint32_t bsig0_a = BSIG0(a);
uint32_t T2 = bsig0_a + maj_precalc;  // 使用预计算的值
```

**预期效果**：
- 将 MAJ 的计算提前，与其他计算并行
- 关键路径只剩 BSIG0 + 加法器
- **预计减少 5 级逻辑**

---

## 📊 优化效果预估

### 综合优化方案（方案A + 方案B）

**当前最长路径**（W数组生成）:
```
SSIG1(6级) + SSIG0(6级) + 3个加法器(18-21级) = ~36-40级 ≈ 10.575 ns
```

**优化后**（方案A：拆分加法）:
```
SSIG1(6级) + SSIG0(6级) + 2个并行加法(6-7级) + 1个串联加法(6-7级) 
= ~24-26级 ≈ 7-8 ns
```

**优化后**（方案A + 方案B：拆分加法 + 优化SSIG）:
```
SSIG1_opt(3级) + SSIG0_opt(3级) + 2个并行加法(6-7级) + 1个串联加法(6-7级)
= ~18-20级 ≈ 5.5-6.5 ns
```

**预期最终 Estimated Clock Period**:
- **保守估计**: 9.5-10 ns（改善 ~5-10%）
- **预期**: 8.5-9.5 ns（改善 ~10-20%）
- **乐观**: 7.5-8.5 ns（改善 ~20-30%）

**预期执行时间**（假设 Latency 不变或略增）:
```
当前: 10.575 × 810 = 8,565.75 ns

保守: 9.5 × 820 = 7,790 ns (-9.1%) ✅
预期: 9.0 × 830 = 7,470 ns (-12.8%) ✅✅
乐观: 8.0 × 850 = 6,800 ns (-20.6%) ✅✅✅
```

---

## 🎯 推荐实施顺序

### Phase 6-1: 优化 W 数组生成（最优先）⭐⭐⭐⭐⭐

**修改位置**: `sha224_256.hpp:563`

**风险**: 低（只是重排序加法）

**预期效果**: Clock Period 降低 5-10%

---

### Phase 6-2: 优化 SSIG0/SSIG1 函数（高优先级）⭐⭐⭐⭐

**修改位置**: `sha224_256.hpp:47-48`

**风险**: 中（需要改宏为inline函数）

**预期效果**: Clock Period 再降低 5-10%

---

### Phase 6-3: 优化 BSIG0/BSIG1 函数（中优先级）⭐⭐⭐

**修改位置**: `sha224_256.hpp:45-46`

**风险**: 中

**预期效果**: Clock Period 再降低 3-5%

---

### Phase 6-4: 优化 MAJ/CH 函数（低优先级）⭐⭐

**修改位置**: `sha224_256.hpp:43-44`

**风险**: 中（逻辑简化需要验证正确性）

**预期效果**: Clock Period 再降低 1-3%

---

## ⚠️ 注意事项

### 1. 功能正确性优先

**每次修改后必须验证**:
- C仿真 pass
- CoSim pass
- 输出与 golden reference 一致

### 2. Latency 可能增加

**优化时序可能导致**:
- 插入更多寄存器 → Latency 增加
- 只要 Clock 降低 > Latency 增加比例即可

**可接受范围**:
```
如果 Clock 降低 20%，Latency 增加 < 15% → 净效果正面
```

### 3. HLS Pragma 配合

**关键 inline 函数需要正确配置**:
```cpp
#pragma HLS INLINE      // 让HLS决定是否inline
// 或
#pragma HLS INLINE off  // 强制不inline，独立优化
```

### 4. 保留回退方案

**每个 Phase 都应该**:
- 保存当前版本
- 测试新版本
- 对比效果
- 如果失败，立即回退

---

## 🔬 诊断工具

### 查看关键路径

**查看 HLS 报告**:
```
L1/tests/hmac/sha256/hmac_sha256_test.prj/solution1/.autopilot/db/
  - <module>_csynth.rpt : 查看各模块的时序
  - schedule_viewer.json : 查看调度详情
```

**关注指标**:
- 各函数的 Estimated Clock
- II (Initiation Interval) 违例
- 资源使用（FF增加说明插入了寄存器）

---

## 📈 预期最终效果

**Phase 6 目标**:
- Estimated Clock Period: < 9.0 ns（从 10.575 降低 15%+）
- Latency: < 900 cycles（可接受增加 < 11%）
- **执行时间: < 8,000 ns**（vs Baseline 改善 > 23%）✅✅✅

**如果成功**:
```
执行时间从 8,565 ns → 7,500-8,000 ns
相比 Baseline 改善从 17.8% → 23-28%
这是一个巨大的突破！
```

---

**Phase 6: 算法层面的时序优化，目标是突破 10.575 ns 的物理极限！** 🚀

