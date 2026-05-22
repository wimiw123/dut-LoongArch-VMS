# 最新正式对齐结果

## 1. 官方 compare 入口汇总

本次正式运行使用的命令：

```bash
cd /home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main
toolchain/run_c_tests_compare.sh

cd /home/wimiw/ics2025/ics2025
make -C am-kernels/tests/cpu-tests compare-run ARCH=riscv32-nemu
```

官方 compare 入口的最新汇总结果如下：

### LoongArch compare

```text
35 / 35 passed
Scored time: 9.811 ms
Total time: 10818.285 ms
```

### PA compare

```text
35 / 35 passed
Scored time: 29.616 ms
Total time: 23989.984 ms
```

## 2. 逐项详细对比

下面这张表是在官方 compare 跑完后，按同样的对齐配置对每个程序再次提取出的详细数据。

说明：

- `delta_us = loong_us - pa_us`
- `delta_us < 0` 表示 PA 更慢
- `delta_us > 0` 表示 LoongArch 更慢
- `delta_inst = loong_inst - pa_inst`
- 由于 `host time spent` 本身受运行噪声影响，逐项重提取得到的 LoongArch 总和会和官方 compare 汇总值有轻微波动，这是正常现象

```text
name            loong_status    loong_us  loong_ms  loong_inst  pa_status       pa_us  pa_ms   pa_inst  delta_us  time_higher         delta_inst  inst_higher
add-longlong    HIT GOOD TRAP   71        0.071     5663        HIT GOOD TRAP   478    0.478   5377     -407      PA +407 us          286         LoongArch +286
add             HIT GOOD TRAP   53        0.053     4399        HIT GOOD TRAP   633    0.633   4410     -580      PA +580 us          -11         PA +11
bit             HIT GOOD TRAP   20        0.020     783         HIT GOOD TRAP   80     0.080   839      -60       PA +60 us           -56         PA +56
bubble-sort     HIT GOOD TRAP   168       0.168     13212       HIT GOOD TRAP   1615   1.615   13223    -1447     PA +1447 us         -11         PA +11
crc32           HIT GOOD TRAP   455       0.455     36873       HIT GOOD TRAP   4104   4.104   36884    -3649     PA +3649 us         -11         PA +11
div             HIT GOOD TRAP   58        0.058     4840        HIT GOOD TRAP   546    0.546   4751     -488      PA +488 us          89          LoongArch +89
dummy           HIT GOOD TRAP   2         0.002     18          HIT GOOD TRAP   5      0.005   29       -3        PA +3 us            -11         PA +11
fact            HIT GOOD TRAP   44        0.044     2306        HIT GOOD TRAP   210    0.210   2317     -166      PA +166 us          -11         PA +11
fib             HIT GOOD TRAP   28        0.028     2289        HIT GOOD TRAP   238    0.238   2300     -210      PA +210 us          -11         PA +11
goldbach        HIT GOOD TRAP   63        0.063     4867        HIT GOOD TRAP   439    0.439   4653     -376      PA +376 us          214         LoongArch +214
hello-str       HIT GOOD TRAP   29        0.029     1793        HIT GOOD TRAP   342    0.342   3555     -313      PA +313 us          -1762       PA +1762
if-else         HIT GOOD TRAP   14        0.014     941         HIT GOOD TRAP   89     0.089   952      -75       PA +75 us           -11         PA +11
leap-year       HIT GOOD TRAP   79        0.079     6815        HIT GOOD TRAP   662    0.662   6699     -583      PA +583 us          116         LoongArch +116
load-store      HIT GOOD TRAP   17        0.017     1219        HIT GOOD TRAP   201    0.201   1254     -184      PA +184 us          -35         PA +35
matrix-mul      HIT GOOD TRAP   731       0.731     65166       HIT GOOD TRAP   6568   6.568   65177    -5837     PA +5837 us         -11         PA +11
max             HIT GOOD TRAP   55        0.055     4491        HIT GOOD TRAP   599    0.599   4502     -544      PA +544 us          -11         PA +11
mersenne        HIT GOOD TRAP   5910      5.910     434629      HIT GOOD TRAP   1498   1.498   14679    4412      LoongArch +4412 us  419950      LoongArch +419950
min3            HIT GOOD TRAP   69        0.069     5603        HIT GOOD TRAP   612    0.612   5550     -543      PA +543 us          53          LoongArch +53
mov-c           HIT GOOD TRAP   5         0.005     211         HIT GOOD TRAP   22     0.022   222      -17       PA +17 us           -11         PA +11
movsx           HIT GOOD TRAP   6         0.006     338         HIT GOOD TRAP   51     0.051   355      -45       PA +45 us           -17         PA +17
mul-longlong    HIT GOOD TRAP   18        0.018     1135        HIT GOOD TRAP   165    0.165   1146     -147      PA +147 us          -11         PA +11
pascal          HIT GOOD TRAP   151       0.151     12289       HIT GOOD TRAP   1115   1.115   12300    -964      PA +964 us          -11         PA +11
prime           HIT GOOD TRAP   183       0.183     14504       HIT GOOD TRAP   1462   1.462   13259    -1279     PA +1279 us         1245        LoongArch +1245
quick-sort      HIT GOOD TRAP   116       0.116     8811        HIT GOOD TRAP   824    0.824   8822     -708      PA +708 us          -11         PA +11
recursion       HIT GOOD TRAP   104       0.104     8642        HIT GOOD TRAP   788    0.788   8622     -684      PA +684 us          20          LoongArch +20
select-sort     HIT GOOD TRAP   122       0.122     10280       HIT GOOD TRAP   982    0.982   10291    -860      PA +860 us          -11         PA +11
shift           HIT GOOD TRAP   13        0.013     1019        HIT GOOD TRAP   94     0.094   1030     -81       PA +81 us           -11         PA +11
shuixianhua     HIT GOOD TRAP   393       0.393     29370       HIT GOOD TRAP   2762   2.762   27781    -2369     PA +2369 us         1589        LoongArch +1589
string          HIT GOOD TRAP   49        0.049     3476        HIT GOOD TRAP   357    0.357   3497     -308      PA +308 us          -21         PA +21
sub-longlong    HIT GOOD TRAP   70        0.070     5661        HIT GOOD TRAP   534    0.534   5377     -464      PA +464 us          284         LoongArch +284
sum             HIT GOOD TRAP   15        0.015     1050        HIT GOOD TRAP   99     0.099   1061     -84       PA +84 us           -11         PA +11
switch          HIT GOOD TRAP   15        0.015     1042        HIT GOOD TRAP   127    0.127   966      -112      PA +112 us          76          LoongArch +76
to-lower-case   HIT GOOD TRAP   80        0.080     6846        HIT GOOD TRAP   617    0.617   6831     -537      PA +537 us          15          LoongArch +15
unalign         HIT GOOD TRAP   18        0.018     221         HIT GOOD TRAP   25     0.025   232      -7        PA +7 us            -11         PA +11
wanshu          HIT GOOD TRAP   70        0.070     5334        HIT GOOD TRAP   673    0.673   4939     -603      PA +603 us          395         LoongArch +395
```

## 3. 详细对比汇总

```text
LOONG_TOTAL_US=9294
LOONG_TOTAL_MS=9.294
PA_TOTAL_US=29616
PA_TOTAL_MS=29.616
TOTAL_DELTA_US=-20322
LOONG_TOTAL_INST=706136
PA_TOTAL_INST=283882
```

解释：

- 从最新详细对比结果看，总时间上 **PA 更高**，比 LoongArch 多 `20322 us`
- 绝大多数程序中，`delta_us` 都是负值，表示 **PA 更慢**
- 最大例外是 `mersenne`，LoongArch 明显更慢
- 指令数上两边仍然有显著差异，这再次说明两边已经是“框架对齐”，不是“机器级相同工作负载”
