# CPU Scheduling Lab

C11 进程调度算法模拟器：FCFS、非抢占式 SJF、时间片轮转 RR。读取 CSV，输出合并后的调度区间、逐进程指标和同输入对照表。

## 项目目标

把操作系统中的调度概念转化为可核对的小实验，练习结构体、动态数组、循环队列、文件校验和自动化测试。程序采用离散事件推进时间，不实际等待，不创建或控制 Windows 进程。

## 支持的调度算法

| 算法 | 选择规则 | 抢占 |
|---|---|---|
| FCFS | 到达时间优先，同到达按输入顺序 | 无 |
| SJF | 从已到达任务选择 burst 最小者；相同按到达时间、输入顺序 | 无 |
| RR | FIFO 就绪队列，每次最多运行 quantum 个时间单位 | 时间片到期 |

名称只接受小写 `fcfs`、`sjf`、`rr`。不默认选择算法，必须指定 `--algorithm` 或 `--compare`。RR 与比较模式必须提供正整数 `--quantum`；单独 FCFS/SJF 不接受无作用的时间片参数。

## 输入文件格式

`examples/processes.csv`：

```csv
pid,arrival,burst
P1,0,5
P2,1,3
P3,2,1
P4,6,4
```

- 第一行必须为 `pid,arrival,burst`；允许字段两侧空白。
- PID 大小写敏感、不重复，1～31 个 ASCII 字母、数字、下划线或连字符。
- arrival 为十进制非负整数，burst 为十进制正整数；时间使用 `int64_t`。
- 支持 LF/CRLF 和末行无换行。最多 10,000 个进程，每行最多 511 字节（不含 LF）。
- 这是限定格式的 CSV，不支持引号字段、注释、BOM、空数据行和额外字段。
- 遇到重复 PID、损坏记录、负值、空文件、只有表头或数值溢出时立即报错，包含物理行号，退出码非零，不跳过坏数据继续模拟。

## 快速开始

需要 C11 编译器、CMake 3.20+ 和构建工具，没有第三方库依赖。所有命令从本项目目录执行。

Windows：打开 **x64 Native Tools Command Prompt for VS 2022**：

```bat
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
build\scheduler.exe examples/processes.csv --algorithm fcfs
build\scheduler.exe examples/processes.csv --algorithm sjf
build\scheduler.exe examples/processes.csv --algorithm rr --quantum 2
build\scheduler.exe examples/processes.csv --compare --quantum 2
build\scheduler.exe examples/idle_cpu.csv --compare --quantum 2
```

其他单配置 CMake 环境可执行：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/scheduler examples/processes.csv --compare --quantum 2
```

Visual Studio 多配置生成器使用另一个构建目录，构建加 `--config Debug`，CTest 加 `-C Debug`，程序位于 `Debug` 子目录。当前实际验证环境是 Windows x64、MSVC 19.44.35225、CMake 3.31.6、NMake Debug，`/W4 /WX` 严格警告通过；未在 Linux/macOS 上实际运行。

## 指标定义

每个进程：

```text
周转时间 = 完成时间 - 到达时间
等待时间 = 周转时间 - burst
响应时间 = 首次运行时间 - 到达时间
```

所有时间是抽象整数单位，不表示测得的毫秒。平均值使用 double，显示两位小数。输出中的 First 是首次运行时间，Completion 是完成时间。

- Finish：从时间 0 起完成全部任务的时刻，含初始和中途空闲。
- Idle：从时间 0 到 Finish 的 CPU 空闲总时间。
- Switches：时间线上相邻的两个不同进程之间的直接交接次数。首次启动、进出空闲、同一进程连续运行不计。此计数不模拟真实内核切换细节，也不增加时间开销。

例如 RR 中 P2 到达于 1、首次运行于 2、完成于 8、burst 为 3：周转为 7，等待为 4，响应为 1。等待时间包含首次运行之后再次排队的时间，不能直接当作响应时间。

## 算法设计

- `src/csv_reader.c`：严格读取与验证，动态增长输入数组。
- `src/scheduler.c`：复制输入并重置 remaining、first_run、completion，三个算法不共享可变状态；使用 arrival + 原输入索引作为排序键。
- `src/queue.c`：容量为进程数的循环队列。队尾通过 `(front + size) % capacity` 计算，避免维护冗余状态；满队列、空队列返回失败。
- `src/main.c`：参数解析、对照表、逐进程指标和时间线输出。

SJF 每次完成后扫描已到达的未完成任务；没有就绪任务时直接跳到下一次到达，不逐单位等待。

RR 按时间片或进程完成事件推进。运行期间到达的进程按到达时间、输入顺序入队，**恰在时间片终点到达的进程也先入队**，再将未完成的当前进程放到队尾。first_run 只在第一次派发时设置。

时间线使用 `[start, end)` 半开区间，每行标明进程或 `<IDLE>`；这是可精确读取的文本时间线，不按持续时间比例画宽度。相邻同进程区间自动合并，因此 RR 单进程即使经历多个时间片，也只显示一个区间，不产生虚假的切换。

## 对照实验

复现命令：

```bat
build\scheduler.exe examples/processes.csv --compare --quantum 2
```

固定上述四个进程、时间片 2、初始空队列、切换开销为 0。2026-09-28 实际运行结果：

| Algorithm | Avg Waiting | Avg Turnaround | Avg Response | Switches |
|---|---:|---:|---:|---:|
| FCFS | 3.25 | 6.50 | 3.25 | 3 |
| SJF | 2.75 | 6.00 | 2.75 | 3 |
| RR | 3.75 | 7.00 | 1.25 | 7 |

三者 Finish 均为 13、Idle 均为 0。可核对的运行顺序：

```text
FCFS: 0--P1--5--P2--8--P3--9--P4--13
SJF:  0--P1--5--P3--6--P2--9--P4--13
RR:   0--P1--2--P2--4--P3--5--P1--7--P2--8--P4--10--P1--11--P4--13
```

这组输入中，SJF 在时刻 5 先运行只需 1 单位的 P3，将其等待从 6 降为 3；P2 的等待从 4 增至 5，平均等待仍下降。它不会在时刻 1 打断 P1，因为这里实现的是非抢占式 SJF。

RR 让 P2、P3 更早获得首次运行机会，平均响应降至 1.25；但 P1 多次排队，平均周转变长、切换变多。这体现响应与周转的取舍，不能据此认定 RR 或 SJF 对所有输入都最好。SJF 还假设事先知道 burst，现实系统不一定有这个信息。

空闲示例 `examples/idle_cpu.csv` 实测三种算法都得到：平均等待 0、平均周转 1.50、平均响应 0、Finish 9、Idle 6、Switches 0。时间线为 `[0,3) IDLE → [3,5) P1 → [5,8) IDLE → [8,9) P2`。

## 自动化测试

2026-09-28 实测 **46 项核心检查 + 16 项命令行检查全部通过**，CTest 为 2 个测试入口，不是只有两个内部用例。

覆盖：单进程、FCFS 到达顺序与同时到达、初始/中途空闲、SJF 已到达限制与稳定规则、RR 多时间片与重新入队、时间片边界到达、零/负时间片、时间溢出和资源上限、队列空满和环绕、重复 PID、负到达、零 burst、空文件、额外/缺少字段、参数错误、输入不变及 remaining 最终归零。

其中一个核心检查生成 100 组固定种子的小输入，对三种算法进行 **300 次调度对照**：参考模型逐时间单位推进，使用独立数组队列，不调用实现中的排序或循环队列，逐时刻核对运行者、首次运行和完成时间。还核对服务时长守恒、时间线连续、空闲总量和非负指标。

```text
46 tests passed; 0 failed
16 CLI tests passed
100% tests passed, 0 tests failed out of 2
```

测试使用返回失败码的检查，不依赖可在 Release 中被关闭的 assert。

## 边界情况

- 空输入明确拒绝；损坏 CSV 不静默忽略。
- 输入可以乱序；排序不改变输出中逐进程表的原输入顺序。
- burst 小于时间片时只执行剩余时间；单进程的连续时间片合并。
- 使用 int64_t 并在运行前检查“最大到达时间 + 总 burst”是否溢出。这是保守上界，可能拒绝少数实际能够排完的极端大值。
- RR 最多允许 1,000,000 次派发，超出时提示增大时间片，不截断结果。合并后的时间线可能比派发数少。
- 内存分配失败会释放已有结果并返回非零。库调用者应传入经过校验的 PID；每次成功结果使用完调用 `result_destroy()`，再次使用同一结果对象前先释放。

## 时间复杂度

设 n 为进程数，k 为 RR 实际派发次数，s 为合并后的时间线区间数：

- CSV 重复 PID 使用简单扫描，验证总时间 O(n²)，存储 O(n)。
- FCFS：排序通常 O(n log n)，调度 O(n)。
- SJF：每次选择扫描任务，整体 O(n²)，加初始排序成本。
- RR：排序通常 O(n log n)，每次入队/出队 O(1)，调度 O(n + k)。
- 结果含进程数组和时间线，空间 O(n + s)；比较模式保留三份独立结果再统一输出。

算法在没有任务时跳过整个空闲区间，因此时间跨度很大不意味着循环次数按跨度增长。RR 的循环次数仍取决于派发次数。

## 当前限制

单 CPU、单段 CPU burst、无 I/O 阻塞、无优先级、多级反馈、真实线程或 GUI。不模拟切换耗时，不把任务时间当成实测操作系统性能。SJF 需要预先已知 burst，且没有防饥饿机制；当前输入是有限的任务集合。

`screenshots/` 暂为占位目录，可保存自己实际运行的比较表、RR 时间线、空闲示例和测试结果。

学习顺序：先用纸笔计算四个进程的 FCFS 指标，再阅读 `Process` 和 SJF 选择循环，最后跟踪 RR 的队列。重点回答：为什么响应时间只看第一次运行，而等待时间包含所有排队时间？
