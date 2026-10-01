# 一次 C++ 崩溃 Bug 的完整排查记录

> 案例：多机编队演示运行约 30~40 秒后随机崩溃（提交 `09bec63` 修复的 `TrailRecorder::channelOf` 悬空引用）。
> 本文面向不熟悉 C++ 调试的读者，逐层还原"现象 → 证据 → 根因 → 修复 → 验证"的完整过程，
> 每一步都解释**用了什么工具、为什么用它、输出怎么读**。

---

## 0. 现象与难度所在

用户操作：点击「多机编队演示」，**等一会程序就崩溃消失**。

这个 Bug 的恶心之处在于三个特征：

| 特征 | 含义 |
|------|------|
| 时有时无 | 多数时候能正常跑，偶尔崩 |
| 延迟崩溃 | 不是点按钮立刻崩，而是运行 30~40 秒后崩 |
| 无报错信息 | Windows 直接弹"程序已停止工作"，没有任何提示 |

这三个特征指向的是典型的 **内存类 Bug**（悬空指针/引用、越界、释放后使用）——
它们不像逻辑错误那样每次都稳定复现，而是"碰运气"：读到还没被覆盖的内存就侥幸正常，
读到被覆盖过的就崩。**排查这类 Bug 的核心思路只有一条：想尽办法拿到崩溃瞬间的调用栈。**

---

## 1. 第一轮：静态代码审计（必要但不充分）

先不跑任何工具，通读崩溃路径上所有代码：编队 tick 槽 → `SetUAVPos` → UAV 图元 → 弧线图元。

这一轮找到并修了一个**真 Bug 但不是本案根因**的问题：`EnsureUAV` 给所有 id 的机都设了
地图跟随，编队三架机每 200ms 互相拉扯地图中心（每秒 15 次中心跳变）。

**为什么明知可能不是根因还要修**：它是确定存在的错误行为（地图乱跳），先消除变量。
但用户复测仍然崩——证明另有根因。

**教训**：静态审计的价值是缩小嫌疑范围 + 排除低级错误，但内存类 Bug 的元凶往往不在
"看起来可疑"的代码里，必须拿到运行时证据。

---

## 2. 第二轮：gdb 抓用户侧崩溃栈

### 2.1 gdb 是什么

**gdb**（GNU Debugger）是 MinGW 工具链自带的调试器，能以"监视运行"的方式启动程序：
程序崩的那一瞬间，gdb 会拦下来，告诉你**崩在哪条 CPU 指令、当时的函数调用链**。

我用的是批处理模式（不需要交互界面，跑完自动退出）：

```powershell
C:\Qt\Qt5.12.12\Tools\mingw730_64\bin\gdb.exe -batch -ex run -ex bt --args .\opmapcontrol_example.exe
```

逐段解释：

| 片段 | 含义 |
|------|------|
| `-batch` | 批处理模式：执行完所有命令自动退出，不需要人守着敲命令 |
| `-ex run` | `-ex` 表示"启动后先执行这条 gdb 命令"；`run` 就是运行被调试的程序 |
| `-ex bt` | `bt` 是 backtrace（回溯）的缩写：**程序崩溃后打印函数调用栈**——这是整个排查里最重要的命令 |
| `--args xxx.exe` | `--args` 后面跟的是要传给被调试程序自己的参数 |

### 2.2 第一次尝试失败：0xc0000135

用户第一次跑 gdb，程序根本没启动：

```
During startup program exited with code 0xc0000135.
No stack.
```

`0xc0000135` 是 Windows 错误码 **STATUS_DLL_NOT_FOUND**——程序要加载的 DLL（Qt5Core.dll 等）找不到。
平时双击能跑，是因为资源管理器/Qt Creator 的环境里配置了 Qt 的 DLL 路径（PATH 环境变量）；
gdb 继承的是 PowerShell 当前 shell 的 PATH，里面没有。

**解法**：跑 gdb 前先把 Qt 的 bin 目录注入 PATH：

```powershell
$env:PATH = "C:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin;C:\Qt\Qt5.12.12\Tools\mingw730_64\bin;" + $env:PATH
```

> 认识错误码：`0xc0000135` 这类 `0xC0000xxx` 开头的是 Windows NT 内核状态码，
> 崩溃时常见的还有 `0xc0000005`（ACCESS_VIOLATION，访问了非法内存地址——本案例最终崩的就是它）。

### 2.3 拿到了栈，但一半是 "??"

第二次运行成功抓到崩溃栈：

```
Thread 1 received signal SIGSEGV, Segmentation fault.
#0  0x00000000004277e4 in ?? ()
#1  0x000000000040e35a in ?? ()
#2  0x0000000000416b0f in ?? ()
#3  QMetaObject::activate(QObject*, int, int, void**)   ← Qt5Core.dll
#4  QTimer::timerEvent(QTimerEvent*)                    ← Qt5Core.dll
...
```

**怎么读栈**：调用栈像一叠盘子，`#0` 是**最上面（崩点）**，往下每层是"谁调用了它"。
`SIGSEGV`（段错误）= 访问了没有权限的内存地址，即 `0xc0000005` 的 Linux 叫法。

关键信息：
- `#3`、`#4` 能看到函数名，是因为它们在 **Qt5Core.dll** 里，Qt 的官方 DLL 自带调试符号；
- `#0~#2` 是 `??`，说明这几层在我们的 exe 里，**exe 被剥离了调试信息**。

从仅有的符号已经能推理：崩在 **某个 QTimer 超时 → 信号发射 → 槽函数** 的路径里——
但槽函数是谁（编队 tick？弧线动画？），栈里看不到。

### 2.4 旁支：nm 尝试直接解符号

一个加速技巧：**不重新跑程序，直接查 exe 的符号表**把地址翻译成函数名。
`nm` 是 MinGW 自带的符号表查看工具：

```powershell
nm -C --numeric-sort opmapcontrol_example.exe
```

| 选项 | 含义 |
|------|------|
| `-C` | demangle：把 C++ 编译器修饰过的名字（如 `_ZN5opmap12OPMapWidget9SetUAVPosEiRKNS_11PointLatLngEi`）还原成可读的 `opmap::OPMapWidget::SetUAVPos(int, ...)` |
| `--numeric-sort` | 按内存地址升序排列，方便按地址查找 |

结果输出 `no symbols`——qmake 的 release 构建默认把符号表剥离了（减小体积）。
此路不通，但排除了捷径，也明确了下一步的方向：**必须拿到带调试信息的构建**。

---

## 3. 第三轮：自动复现程序（破局的关键）

### 3.1 为什么要写复现程序

崩溃需要"点击演示按钮后等 30~40 秒"——但 gdb 是命令行程序，**没人能替它点 GUI 按钮**。
与其人肉操作，不如写一个**十几行的控制台程序，用库 API 精确复现 demo 编队演示做的事**：

- 建 3 条弧线（`AddArcLine`）
- 三道轨迹同时开始记录（`StartTrailRecording(0/1/2)`）
- `QTimer` 每 200ms 喂 `SetUAVPos(id, 弧上点, 500)` + `SetUAVHeading`（与 demo `onSwarmTick` 逐行等价）
- 55 秒后自动退出

这个程序不需要人工参与，gdb 可以全自动跑它——**崩了就抓栈，不崩就说明嫌疑链路没问题**。

### 3.2 复现工程的构建

在仓库外建临时目录（不污染仓库），写一个 `.pro`（qmake 工程文件），核心三行：

```pro
INCLUDEPATH += $$OPMAPCONTROL_DIR/src/...   # 头文件路径
LIBS += $$OPMAPCONTROL_DIR/libopmapwidget.a # 链接编好的静态库
RESOURCES += .../mapresources.qrc           # 图标资源（UAV 图标用）
```

然后 `qmake` 生成 Makefile、`mingw32-make` 编译——与 demo 的构建流程完全一致。

> **踩坑记录**：PowerShell 的 `Set-Content` 和部分写文件工具会给文本文件加 **UTF-8 BOM**
> （文件头 3 个隐藏字节 `EF BB BF`），qmake 识别不了 BOM 会直接拒读 `.pro`。
> 症状是"文件明明存在 qmake 却说找不到"。解法是用字节流写文件并去掉前 3 字节。

### 3.3 复现成功：栈指向 SetUAVPos

复现程序在 gdb 下几十秒内 SIGSEGV，这次的栈：

```
#0  opmap::OPMapWidget::SetUAVPos(int const&, PointLatLng const&, int const&)  ← 崩在库的喂点入口
#1  MainWindow::... lambda at main.cpp:47                                       ← 编队 tick 的喂点调用
```

**嫌疑范围瞬间从"整个程序"缩小到 `SetUAVPos` 这一个函数体**（约 10 行代码）。

---

## 4. 第四轮：debug 版库，让栈精确到行

### 4.1 为什么现在的栈还不够

`#0` 只显示函数名 `SetUAVPos`，但函数内部有 6~7 行代码（EnsureUAV / 图标 / 轨迹 / 围栏 / 任务机），
到底崩在哪一行？

函数名和行号的对应关系来自编译器写进二进制的 **DWARF 调试信息**（一张"机器码地址 ↔ 源码文件行号"
的映射表，由编译选项 `-g` 生成）。release 构建没有 `-g`，所以 gdb 只能给函数名。

### 4.2 编一份带 -g 的库

尝试一：给 qmake 传 `CONFIG+=debug` —— **失败**。因为 `opmapcontrol.pro` 里写死了
`CONFIG += staticlib release`，pro 文件里的赋值会**覆盖**命令行传参。

尝试二（成功）：直接强加编译器旗标，绕过 CONFIG 体系：

```powershell
qmake C:\...\opmapcontrol.pro "QMAKE_CXXFLAGS+=-g -O0" "QMAKE_CFLAGS+=-g -O0" "DESTDIR=C:/Users/zhao2/AppData/Local/Temp/swarmdbg"
mingw32-make clean
mingw32-make -j4
```

| 片段 | 含义 |
|------|------|
| `QMAKE_CXXFLAGS+=-g -O0` | `-g` 生成 DWARF 调试信息；`-O0` 关闭优化（优化会把代码行重排/内联，导致行号错乱）；CXXFLAGS 是 C++ 编译旗标、CFLAGS 是 C 的 |
| `DESTDIR=...` | 把产出的 `.a` 输出到临时目录，**避免覆盖仓库里被 git 跟踪的 release 库** |
| `mingw32-make clean` | 先清干净旧目标文件（编译旗标变了必须全量重编，否则新旧 .o 混链） |
| `-j4` | 4 线程并行编译 |

产物从 1.7MB 涨到 **49MB**——多出来的全是 DWARF 调试信息，这正是我们要的。
（这个库只用于调试，验证完删除，不进仓库。）

### 4.3 行号到手

复现程序改链 debug 库、重新链接（`Remove-Item swarmrepro.exe` 后 `mingw32-make` 强制重链），
再跑一次 gdb，栈变成：

```
#0  opmap::OPMapWidget::SetUAVPos(...) at opmapwidget.cpp:588   ← 精确到行！
#1  ... lambda at main.cpp:47
```

打开 [opmapwidget.cpp:588](../src/ui/opmapwidget.cpp#L588)：

```cpp
if (trailRecorder->IsRecording(id))        // ← 崩在这行
    trailRecorder->AddPoint(id, pos, alt);
```

再顺藤摸瓜到 `IsRecording` → `TrailRecorder::channelOf`，**根因浮出水面**。

---

## 5. 根因：一段教科书级的 C++ 悬空引用

`channelOf` 原来的写法：

```cpp
const Channel &channelOf(int uavId) const
{
    static const Channel emptyChannel;
    return channels.contains(uavId) ? channels.value(uavId) : emptyChannel;   // ← BUG
}
```

三层知识叠加才能看懂它为什么是错的：

1. **`QMap::value(key)` 返回的是"值拷贝"**，不是 map 内部元素的引用——它会新建一个临时
   `Channel` 对象，把 map 里的数据复制一份，然后返回这个**临时对象**。
2. **C++ 生命周期规则**：函数返回引用时，如果引用绑定的是函数内部创建的临时对象，
   临时对象**在 return 执行完的那一刻析构**（"跨 return 的临时不延长生命周期"）。
   于是调用方拿到的引用指向一块**已经回收的栈内存**——这就是"悬空引用"（dangling reference）。
3. **悬空读是未定义行为（UB）**：编译器和操作系统不保证给你报错——
   读到的多数时候是"侥幸还没被复用的旧值"（表现完全正常），
   一旦那块栈内存被别的函数调用覆盖成垃圾值，程序就崩。

这与现象**完美吻合**：时有时无（侥幸 vs 被覆盖）、延迟崩溃（栈被反复复用后终于读到垃圾）、
崩点随机（谁踩到垃圾谁崩）。

**修复**（[trailrecorder.cpp](../src/engine/trailrecorder.cpp#L55-L62)）——用迭代器取 map 内
**真实对象的左值引用**，而不是值拷贝：

```cpp
QMap<int, Channel>::const_iterator it = channels.constFind(uavId);
return it != channels.constEnd() ? it.value() : emptyChannel;
```

`constFind` 迭代器的 `it.value()` 是 map 内部那个真实元素的引用（左值），
map 活着它就活着，返回引用完全合法；没找到时返回 `static` 空对象（程序全生命周期存活）。

---

## 6. 验证闭环：修复前 vs 修复后

修复必须用**同一把尺子**量：跑同一个复现程序对比。

| 场景 | 修复前 | 修复后 |
|------|--------|--------|
| 复现程序 + gdb | 约 40 秒内 SIGSEGV，多次稳定复现 | 无 SIGSEGV |
| 复现程序正常运行 65 秒 | — | 完整跑完正常退出，退出码 0 |
| demo 重编 + 冒烟 | 崩 | 零警告 + 6 秒冒烟通过 |

退出码 0 = 程序按 `quit()` 计划正常结束（没有崩溃）。
这一步是排查的"结案陈词"：**同一输入、同一环境，修复前必崩、修复后必不崩**——因果链闭合。

---

## 7. 方法论总结

### 7.1 排查漏斗（每一步都在缩小嫌疑范围）

```
整个程序
  → 静态审计：排除低级错误，消除确定 bug（僚机跟随互拉）
  → 用户侧 gdb：确认是 SIGSEGV + QTimer 信号槽路径
  → 自动复现程序：把"需要人工点击"变成全自动，嫌疑缩到 SetUAVPos 一个函数
  → debug 版库：嫌疑缩到具体某一行
  → 读代码 + C++ 规则推理：找到悬空引用
  → 同尺验证：修复前必崩 / 修复后必不崩，结案
```

### 7.2 本次使用的工具速查表

| 工具/命令 | 用途 | 什么时候用 |
|-----------|------|-----------|
| `gdb -batch -ex run -ex bt --args app.exe` | 运行程序并在崩溃时打印调用栈 | 程序崩溃且能稳定复现时 |
| `bt`（backtrace） | 打印函数调用链，#0 是崩点 | 抓到崩溃后的第一件事 |
| `$env:PATH = "Qt\bin;" + $env:PATH` | 注入 DLL 路径 | gdb/命令行启动报 0xc0000135 |
| `nm -C --numeric-sort app.exe` | 查看 exe 符号表 | 想把栈里的裸地址翻译成函数名 |
| 编译选项 `-g -O0` | 生成调试信息 + 关优化 | 需要行号级栈时；`-O0` 保证行号不因优化错乱 |
| `qmake "QMAKE_CXXFLAGS+=-g -O0"` | 给 qmake 工程强加编译旗标 | pro 里写死 CONFIG 无法用命令行改时 |
| `mingw32-make clean` | 清空编译产物 | 编译选项变更后必须全量重编 |
| 自动复现程序 | 用库 API 代码级复现 GUI 操作 | 崩溃依赖 UI 操作、gdb 无法人工干预时 |
| `Start-Process -PassThru` + `HasExited` | 启动进程 N 秒后检查存活 | 冒烟测试/验证修复 |

### 7.3 三条可迁移的经验

1. **内存类 Bug 别靠读代码硬猜**——静态审计能排除嫌疑，但拿不到证据；
   `bt` 调用栈是唯一硬证据，一切手段围绕"拿到带行号的栈"展开。
2. **无法人工操作的场景，写自动复现程序**——用与业务等价的 API 序列 + 定时退出，
   让 gdb 可以无人值守跑，复现从"碰运气"变成"确定性实验"。
3. **修复必须有对照验证**——用完全相同的复现条件跑修复前/后两个版本，
   "必崩 → 必不崩"才算闭合，单看"修复后没崩"不充分（本来就不每次都崩）。

### 7.4 本次踩坑清单（引以为戒）

- `QMap::value()` 返回**值拷贝**而非内部元素引用——与 `operator[]`/迭代器语义完全不同
- 函数返回引用**不得绑定内部临时对象**（跨 return 生命周期不延长），
  条件表达式一侧是临时（纯右值）时整个表达式结果都是纯右值
- 悬空读内存不报错、不崩是常态——**没崩 ≠ 没错**，UB 的表现就是不可预测
- qmake 的 pro 里 `CONFIG += release` 会覆盖命令行 `CONFIG+=debug`；
  需要调试构建时直接传 `QMAKE_CXXFLAGS+=-g`
- PowerShell/部分工具写出的 UTF-8 BOM 会让 qmake 拒读工程文件
