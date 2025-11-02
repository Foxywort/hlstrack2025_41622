# HMAC-SHA256 测试需求深度分析

## 📋 测试代码核心信息

### 1. 测试配置（test.cpp）

```cpp
#define NUM_TESTS 2        // 只测试2个样本（注释显示原本可达200）
#define MSG_SIZE 4         // 每个消息字32位
#define HASH_SIZE 32       // SHA256输出32字节
#define MAX_MSG 256        // 最大消息256字节
#define KEYL 32            // 密钥长度32字节（固定）
#define BLOCK_SIZE 64      // SHA256块大小64字节
```

### 2. 测试数据特征

**第190-208行的测试生成逻辑**：
```cpp
for (unsigned int i = 0; i < NUM_TESTS; i++) {
    unsigned int len = i % 128 + 80;  // ⚠️ 关键：消息长度计算
    unsigned int klen = KEYL;         // 密钥固定32字节
    // ...
}
```

**实际测试的2个样本**：
- **Test 0**: `len = 0 % 128 + 80 = 80字节`
- **Test 1**: `len = 1 % 128 + 81 = 81字节`

**测试消息内容**（第182行）：
```
"The quick brown fox jumps over the lazy dog. Its hmac is 80070713463e7749b90c2dc24911e275"
```
（取前80或81字节）

**密钥内容**（第185行）：
```
"key0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
```
（32字节）

---

## 🔍 算法分析：HMAC-SHA256的计算流程

### HMAC标准算法（RFC 2104）

```
HMAC(K, M) = H( (K' ⊕ opad) || H( (K' ⊕ ipad) || M ) )
```

其中：
- `K'` = 如果key长度=64字节，直接用；如果<64字节，补0；如果>64字节，先hash
- `ipad` = 0x36重复64次
- `opad` = 0x5c重复64次
- `H` = SHA256哈希函数

### 当前测试场景的实际计算

**对于80字节消息**：

1. **K' 处理**：
   - 密钥32字节 < 64字节 → K' = key + 32个0字节

2. **第一次SHA256**：`H1 = SHA256( (K' ⊕ ipad) || message )`
   - 输入：64字节(kipad) + 80字节(消息) = **144字节**
   - SHA256块大小：64字节
   - **需要处理：144/64 = 2.25块** → 实际3个块
     - Block 0: kipad的前64字节
     - Block 1: 消息的64字节
     - Block 2: 消息的16字节 + padding

3. **第二次SHA256**：`H2 = SHA256( (K' ⊕ opad) || H1 )`
   - 输入：64字节(kopad) + 32字节(H1输出) = **96字节**
   - **需要处理：96/64 = 1.5块** → 实际2个块
     - Block 0: kopad的64字节
     - Block 1: H1的32字节 + padding

**每个HMAC操作 = 3块 + 2块 = 5个SHA256块**
**2个测试 = 2 × 5 = 10个SHA256块**

---

## 💡 关键发现：Latency瓶颈分析

### 当前性能数据

- **Total Latency**: 807 cycles
- **每个HMAC**: 807 / 2 = 403.5 cycles
- **每个SHA256块**: 807 / 10 = **80.7 cycles/block**

### SHA256算法理论周期数

**标准SHA256处理流程**：
1. **Message Scheduling**: 生成64个W值
   - 前16个：直接从输入块读取（16 cycles if II=1）
   - 后48个：计算得出（48 cycles if II=1）
   - **总计：64 cycles**

2. **64轮压缩函数**: 
   - 64次迭代（64 cycles if II=1）

3. **其他开销**：
   - 初始化、结束处理、dataflow握手：~10-20 cycles

**理论最小值**：64 + 64 + 20 = **~148 cycles/block**（无优化）
**当前实际值**：**80.7 cycles/block** ✅（已经很优秀！）

---

## 🎯 核心问题：为什么Latency=807而不是更低？

### 问题1: Dataflow Pipeline效率

**观察**：80.7 cycles/block < 148理论值
**说明**：存在block间的并行处理（dataflow overlap）

但是：
- **理想情况**：10个块完全pipeline，latency ≈ 64 + 64 + 10×small_overhead ≈ 200 cycles
- **实际情况**：807 cycles
- **差距**：807 - 200 = **607 cycles浪费**

**根本原因**：🔴 **Dataflow的bubble（流水线空泡）**

### 问题2: HMAC的串行依赖

```
HMAC流程：
  第一次SHA256(3块) → 等待输出 → 第二次SHA256(2块)
                      ↑
                   串行依赖点
```

**每个HMAC的两次SHA256是串行的**：
- 第二次SHA256必须等待第一次完成
- 这造成了不可避免的bubble

### 问题3: 当前代码的数据流分析

**从test.cpp第229-238行**：
```cpp
// 写入所有输入
for (test 0 to 1) {
    string2Strm(key);   // 写key流
    string2Strm(msg);   // 写msg流
    lenMsgStrm.write();
    endLenStrm.write(false);
}
endLenStrm.write(true);

// 调用FPGA
test_hmac_sha256(...);  // ← 807 cycles在这里

// 读取所有输出
for (test 0 to 1) {
    hshStrm.read();
}
```

**关键观察**：
- 输入是**burst模式**：先写入所有数据
- 处理是**streaming模式**：逐个处理
- 输出是**burst模式**：处理完后读取

---

## 🚀 可能的优化方向

### 方向1: 算法层面 - 减少实际处理的块数 ⭐⭐⭐

**问题**：当前每个HMAC处理5个块（3+2），但实际可以更少吗？

**NO!** 因为：
- HMAC标准要求两次hash
- 消息长度80字节固定需要3个块（第一次SHA256）
- 第二次SHA256固定需要2个块

**结论**：无法从算法层面减少块数

### 方向2: Pipeline优化 - 减少Dataflow Bubble ⭐⭐⭐⭐⭐

**当前瓶颈**：
```
preProcessing → generateMsgSchedule → sha256Digest
      ↓                ↓                    ↓
   17 cycles        64 cycles            64+ cycles
      ↓                ↓                    ↓
   如果这里慢了，后面的模块会等待（bubble）
```

**优化目标**：
1. ✅ **降低preProcessing latency**（我尝试的UNROLL策略）
   - 目标：17 cycles → 2-4 cycles
   - **但测试显示Latency还是807！** 🔴
   - **说明这不是瓶颈！**

2. 🎯 **真正的瓶颈可能在其他地方**：
   - HMAC的kpad处理？
   - 两次SHA256之间的数据传递？
   - FIFO depth不足导致阻塞？

### 方向3: 减少HMAC内部串行依赖 ⭐⭐⭐⭐

**观察**：HMAC需要两次SHA256，第二次依赖第一次的输出

**可能优化**：
- 预计算kopad（在第一次SHA256开始前就准备好）
- 增大中间FIFO，让第二次SHA256尽早开始
- Dataflow之间的handshake优化

### 方向4: 针对测试场景的特殊优化 ⭐⭐

**测试特点**：
- 固定密钥（32字节）
- 固定消息长度模式（80-81字节）
- 只测试2个样本

**特殊优化**（仅对测试有效）：
- ❌ 预计算固定的kipad/kopad（违反通用性）
- ❌ 硬编码消息长度（违反通用性）

**结论**：不应该针对测试场景作弊

---

## 🔥 最重要的发现

### 为什么我的UNROLL优化latency还是807？

**答案**：因为**preProcessing不是瓶颈**！

**证据**：
```
理论分析：
- preProcessing: 17 cycles（原始）
- generateMsgSchedule: 64 cycles
- sha256Digest: 64 cycles
- 其他overhead: ~20 cycles

总计单个块：~165 cycles
```

**如果preProcessing是瓶颈**：
- 优化前：807 cycles
- 优化后（17→2）：807 - 10×15 = **657 cycles**
- 实际结果：**807 cycles（没变！）**

**结论**：🔴 **真正的瓶颈在Dataflow的stall，不在单个模块的latency！**

---

## 📊 正确的优化策略应该是什么？

### 1. 找出真正的瓶颈

需要查看：
- ✅ **Dataflow viewer**（GUI中的dataflow分析）
- ✅ **Schedule viewer**（看哪个模块在等待）
- ✅ **FIFO深度报告**（是否有FIFO满/空导致的stall）

### 2. 可能的真实瓶颈

基于807 cycles和10个块的分析：
- **平均每块80.7 cycles**已经很高效
- **Dataflow overlap不够**：理想情况下，后续块应该和前一块重叠处理
- **可能原因**：
  1. FIFO深度不足
  2. HMAC的两次SHA256之间的依赖
  3. 某个模块比其他模块慢很多（成为瓶颈）

### 3. 真正应该做的优化

**不是**：盲目加UNROLL/PIPELINE pragma
**而是**：
1. 📊 **分析Dataflow schedule**
2. 🔍 **找出stall的模块**
3. 🎯 **针对性优化瓶颈模块**
4. 📈 **增加关键FIFO深度**
5. ⚡ **减少模块间的handshake开销**

---

## 总结

### 测试内容
- **2个HMAC-SHA256操作**
- **每个处理80/81字节消息**
- **固定32字节密钥**
- **总共10个SHA256块处理**

### 当前性能
- **Total: 807 cycles**
- **Per block: 80.7 cycles**（已经很优秀）

### 真正的问题
- ❌ **不是**单个模块的latency太高
- ✅ **而是**Dataflow的pipeline效率低
- ✅ **本质**是dataflow bubble和串行依赖

### 我犯的错误
- 盲目优化preProcessing（17→2 cycles）
- 但Latency没变（807还是807）
- **因为preProcessing根本不是瓶颈！**

### 正确的下一步
需要您告诉我：
1. 是否可以查看Dataflow分析报告？
2. 优化目标是什么？（降低latency？提高吞吐？）
3. 是否有其他约束？（资源、功耗、面积）

**我深刻认识到：不理解测试需求就盲目优化，是完全错误的！**

