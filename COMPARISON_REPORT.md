# sfrcpp5 与 sfrmat5_dist 本地对比测试

测试日期：2026-09-29。

## 结论

共 102 组：94 组通过一致性检查，8 组 SFR50 不一致。所有用例均正常计算（双方 status=0），完整 SFR 曲线最大绝对误差为 8.92e-15，频率轴、采样效率与最终通道 ESF 一致。差异集中在高频边缘的 SFR50 报告规则，不是 SFR 曲线计算差异。

## 测试对象与方法

- C++：`/home/hylin/projects/sfrcpp5`，HEAD `051e6bc7c7f4b67f93b1a0a8d5caec24fc0f14fe`。直接编译该目录源码，C++17、O2、double，未修改被测代码。
- MATLAB：`/home/hylin/projects/sfrmat5_dist/sfrmat5`，直接调用本地 `sfrmat5(1,del,double(a),npol,wflag)`，未修改被测代码。
- 输入来自 dist 的灰度/RGB TIFF。MATLAB 无损导出 PNG 并验证像素相等，C++ 读取该 PNG，避免格式和解码差异。RGB 使用默认亮度权重，标量 SFR50/MTF30 对比首个颜色通道（R），曲线对比全部通道。
- 原版没有 MTF30 返回值，基准调用其 `findfreq(dat,0.3,size(dat,1),0)`，取第一通道。
- 一致性阈值：SFR、SFR50、MTF30 绝对误差 <1e-6；频率轴误差 <1e-8；采样效率相等；双方 status=0。拟合系数、ESF 另行记录。
- `sfrcpp5 --selftest` 通过。此处未覆盖 float、GUI、性能或所有异常输入。

## 测试覆盖

示例图 90 组：灰度、RGB、灰度旋转90度、灰度反相、灰度裁剪，共5种输入 × 拟合阶数1/3/5 × Tukey/Hamming × del=1/0.005/300。del=1 时频率单位为 cy/pixel；del=0.005 为毫米采样间隔；del=300 按 DPI 处理，后两者输出 cy/mm。

补充合成边缘 12 组：锐利阶跃、sigma=0.35像素的轻微模糊边缘、sigma=1.2像素的曲线边缘 × 拟合阶数1/5 × Tukey/Hamming，del=1。生成公式保存在 `compare_edges.m`。

## 数值结果

| 输入（5阶、Tukey、del=1） | MATLAB SFR50 | C++ SFR50 | 结果 |
|---|---:|---:|---|
| 灰度示例 | 0.275311298814052 | 0.275311298814052 | 一致 |
| RGB示例（R） | 0.269805177218860 | 0.269805177218860 | 一致 |
| 锐利阶跃 | 0.495000000000000 | 1.004982164554242 | 不一致 |
| 轻微模糊边缘 | 0.495000000000000 | 0.523914459667671 | 不一致 |

90组示例测试最大误差：SFR曲线 8.9165e-15；SFR50 3.1264e-13；MTF30 1.2790e-13；拟合系数 7.1054e-14。频率轴、采样效率和ESF误差均为0。SFR50/MTF30的最大误差来自毫米单位用例。

12组合成测试：曲线边缘4组通过；锐利和轻微模糊边缘8组因SFR50失败。曲线最大误差6.6613e-16，MTF30最大误差2.2204e-16，采样效率仍一致。

## 差异根因

MATLAB `sfrmat5.m:639` 调用 `sampeff`，并在 `sfrmat5.m:641` 取其 `freqval(2)` 作为 SFR50。`sampeff.m` 定义 `hs=0.495/del`，将阈值交点裁剪到 `[0,hs]`。

C++ `cpp/sfrmat5.cpp:792` 直接调用 `findfreq(dat,0.5,...)`，在第793行取首通道频率，没有应用上述 `0.495/delimage` 上限。

因此，当50%交点超过该上限时，两者的SFR50不同；若未找到交点，C++还可能返回频率表末端（锐利阶跃用例即如此），不能将该返回值解读成实际已找到的50%交点。

若目标是严格复现MATLAB，应在C++的SFR50上应用与 `sampeff` 一致的范围限制，并增加这些回归用例。本次仅完成对比测试，未修改算法。

## 复现和产物

运行：`/home/hylin/projects/sfrmat5/comparison/run_comparison.sh`（需要本地 g++ 与可正常启动的 MATLAB）。脚本直接使用上述绝对路径，被测目录保持不变。MATLAB在本次沙箱内无法启动，测试通过沙箱外运行完成。

- `results/summary.csv`：90组示例测试逐项误差。
- `edge_results/summary.csv`：12组合成边缘逐项误差。
- 两个结果目录内的JSON：C++完整输出；`matlab_reference.mat`：MATLAB完整输出与汇总表；PNG：共同输入。
- `results/source_sha256.txt`：被测源文件及原始TIFF的校验值。
- `results/matlab.log`、`edge_cases.log`：MATLAB运行日志。
- `compare_local.m`、`compare_edges.m`、`run_comparison.sh`：可复现脚本。

## 本地结果链接

原始测试产物保存在 `/home/hylin/projects/sfrmat5/comparison/`：

- [90组示例测试数据](../sfrmat5/comparison/results/summary.csv)
- [12组合成边缘测试数据](../sfrmat5/comparison/edge_results/summary.csv)
- [复现脚本](../sfrmat5/comparison/run_comparison.sh)
- [示例测试脚本](../sfrmat5/comparison/compare_local.m)
- [合成边缘测试脚本](../sfrmat5/comparison/compare_edges.m)
- [源文件校验值](../sfrmat5/comparison/results/source_sha256.txt)
