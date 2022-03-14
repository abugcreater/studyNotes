# 28 | 基准测试框架JMH

### 性能测试的坑

电源管理,CPU缓存,分支预测,以及超线程技术都会对测试结果造成影响.

就 CPU 缓存而言，如果程序的数据本地性较好，那么它的性能指标便会非常好；如果程序存在 伪共享 (false sharing) 的问题，即几个线程写入内存中属于同一缓存行的不同部分，那么它的性能指标便会非常糟糕。

超线程技术将为每个物理核心虚拟出两个虚拟核心，从而尽可能地提高物理核心的利用率。如果性能测试的两个线程被安排在同一物理核心上，那么得到的测试数据显然要比被安排在不同物理核心上的数据糟糕得多。

JMH 是一个面向 Java 语言或者其他 Java 虚拟机语言的性能基准测试框架。它针对的是纳秒级别（出自官网介绍，个人觉得精确度没那么高）、微秒级别、毫秒级别，以及秒级别的性能测试。



### 生成 JMH 项目

通过再pom文件中添加依赖的方式可以生成项目.或者通过预设模版也可以生成.

```
$ mvn archetype:generate \
          -DinteractiveMode=false \
          -DarchetypeGroupId=org.openjdk.jmh \
          -DarchetypeArtifactId=jmh-java-benchmark-archetype \
          -DgroupId=org.sample \
          -DartifactId=test \
          -Dversion=1.21
$ cd test
//生成类
public class MyBenchmark {
    @Benchmark  //JMH基准测试的测试方法
    public void testMethod() {
        // This is a demo/sample template for building your JMH benchmarks. Edit as needed.
        // Put your benchmark code here.
    }
 
}

```

JMH项目架构图

![image-20211228153716372](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20211228153716372.png)

### 编译和运行 JMH 项目

JMH 正是利用注解处理器  来自动生成性能测试的代码。实际上，除了`@Benchmark`之外，JMH 的注解处理器还将处理所有位于`org.openjdk.jmh.annotations`包 下的注解。

处理器生成的源码在`generated-sources`目录下,源代码中类都继承自`BenchMark`(测试类).继承关系`Benchmark_jmhType -> B3 -> B2 -> B1 -> Benchmark`.

> 之所以不能在同一类中安排这些字段，是因为 Java 虚拟机的字段重排列。而类之间的继承关系，便可以避免不同类所包含的字段之间的重排列。

JMH中比较重要的配置文件

1. `MANIFEST.MF`中指定了该 jar 包的默认入口，即`org.openjdk.jmh.Main`[7]。
2. `BenchmarkList`中存放了测试配置。该配置是根据`MyBenchmark.java`里的注解自动生成的，具体我会在下一篇中详细介绍源代码中如何配置。
3. `CompilerHints`中存放了传递给 Java 虚拟机的`-XX:CompileCommandFile`参数的内容。它规定了无法内联以及必须内联的几个方法，其中便有存放业务逻辑的测试方法`testMethod`。

```
基准测试输出
# Run complete. Total time: 00:04:38

Benchmark                      (n)   Mode  Samples       Score(平均吞吐量)  Score error(误差范围)   Units
o.s.BenchMark.arrayTraverse     10  thrpt       20  335398.448     1312.442  ops/ms
o.s.BenchMark.arrayTraverse     40  thrpt       20  334806.221     2025.001  ops/ms
o.s.BenchMark.arrayTraverse     70  thrpt       20  335762.666     1057.246  ops/ms
```

## JMH部分注解解析

```
# VM invoker: C:\Program Files\Java\jdk1.8.0_152\jre\bin\java.exe
# VM options: -javaagent:D:\idea\IntelliJ IDEA 2021.1\lib\idea_rt.jar=49558:D:\idea\IntelliJ IDEA 2021.1\bin -Dfile.encoding=UTF-8
# Warmup: 4 iterations, 1 s each
# Measurement: 10 iterations, 1 s each
# Threads: 1 thread, will synchronize iterations
# Benchmark mode: Throughput, ops/time
# Benchmark: org.sample.BenchMark.listTraverse
# Parameters: (n = 100)


# Run progress: 93.75% complete, ETA 00:00:17
# Fork: 2 of 2 
# Warmup Iteration   1: 801.285 ops/ms
...
# Warmup Iteration   4: 806.757 ops/ms
Iteration   1: 809.829 ops/ms
Iteration   2: 804.419 ops/ms
...
Iteration  10: 812.165 ops/ms


Result: 807.621 ±(99.9%) 6.638 ops/ms [Average]
  Statistics: (min, avg, max) = (788.763, 807.621, 816.831), stdev = 7.644
  Confidence interval (99.9%): [800.983, 814.259]


# Run complete. Total time: 00:04:38

Benchmark                      (n)   Mode  Samples       Score  Score error   Units
o.s.BenchMark.arrayTraverse     10  thrpt       20  335398.448     1312.442  ops/ms
o.s.BenchMark.arrayTraverse     40  thrpt       20  334806.221     2025.001  ops/ms
o.s.BenchMark.arrayTraverse     70  thrpt       20  335762.666     1057.246  ops/ms
o.s.BenchMark.arrayTraverse    100  thrpt       20  334636.996     2240.907  ops/ms
o.s.BenchMark.listTraverse      10  thrpt       20   50773.890     1722.899  ops/ms
o.s.BenchMark.listTraverse      40  thrpt       20    6819.056       34.278  ops/ms
o.s.BenchMark.listTraverse      70  thrpt       20    1967.642        8.671  ops/ms
o.s.BenchMark.listTraverse     100  thrpt       20     807.621        6.638  ops/ms

Process finished with exit code 0

```

### @Fork 和 @BenchmarkMode

```
# Run progress: 0,00% complete, ETA 00:08:20
# Fork: 2 of 2
# Warmup Iteration   1: 1023500,647 ops/s
```

这里的Fork指的是JMH会Fork出一个新的Java虚拟机,来运行性能基准.使用新的虚拟机,极大程序保证不受类型profile污染导致无法内联,保证性能数据精确.

Java虚拟机的优化会带来不确定,如 TLAB 内存分配（TLAB 的大小会变化），偏向锁、轻量锁算法，并发数据结构等,导致性能测试结果不同.运行更多Fork,获取平均值,增强最终数据的可信度,减少误差.

- `@BenchmarkMode`允许指定性能数据的格式。

### @Warmup 和 @Measurement

```
# Warmup Iteration   1: 801.285 ops/ms  //预热迭代
...
# Warmup Iteration   4: 806.757 ops/ms  //测试迭代
Iteration   1: 809.829 ops/ms
Iteration   2: 804.419 ops/ms
...
Iteration  10: 812.165 ops/ms
```

区分预热迭代和测试迭代，是为了在记录性能数据之前，将 Java 虚拟机带至一个稳定状态。

稳定状态,包括测试方法被编译成机器码,也包括虚拟机中各个自适配优化算法稳定下来.

```
@Warmup(iterations=10, time=100, timeUnit=TimeUnit.MILLISECONDS, batchSize=10)
public class MyBenchmark {
  ...
}
```

`@Warmup`注解有四个参数，分别为预热迭代的次数`iterations`，每次迭代持续的时间`time`和`timeUnit`（前者是数值，后者是单位。例如上面代码代表的是每次迭代持续 100 毫秒），以及每次操作包含多少次对测试方法的调用`batchSize`。

`@Measurement`参数与`@Warmup`一致.

### @State、@Setup 和 @TearDown

`@State`注解，被它标注的类便是程序的状态。

JMH 还将程序状态细分为整个虚拟机的程序状态，线程私有的程序状态，以及线程组私有的程序状态，分别对应`@State`注解的参数`Scope.Benchmark`，`Scope.Thread`和`Scope.Group`。

> 1. Scope.Benchmark：所有测试线程共享一个实例，测试有状态实例在多线程共享下的性能
> 2. Scope.Group：同一个线程在同一个 group 里共享实例
> 3. Scope.Thread：默认的 State，每个测试线程分配一个实例

`@Setup`初始化方法，在全部Benchmark运行之前进行

`@TearDown` 结束方法，在全部Benchmark运行之后进行

上述注解的执行粒度.

可供选择的粒度有在整个性能测试前后调用(Level.Trial)，在每个迭代前后调用(Level.Iteration)，以及在每次调用测试方法前后调用(Level.Invocation)。其中，最后一个粒度将影响测试数据的精度。

### 即时编译相关功能

`@CompilerControl`控制每个方法内联与否

更小粒度的功能则是`Blackhole`类。它里边的`consume`方法可以防止即时编译器将所传入的值给优化掉.

具体的使用方法便是为被`@Benchmark`注解标注了的测试方法增添一个类型为`Blackhole`的参数，并且在测试方法的代码中调用其实例方法`Blackhole.consume`

```java
@Benchmark
public void testMethod(Blackhole bh) {
  bh.consume(new Object()); // prevents escape analysis
}
```



参考:

[lombok介绍](http://notatube.blogspot.com/2010/11/project-lombok-trick-explained.html)

[伪共享介绍](https://www.cnblogs.com/cyfonly/p/5800758.html)

[寄存器](https://baike.baidu.com/item/%E5%AF%84%E5%AD%98%E5%99%A8/187682?fr=aladdin)

[分支预测器](https://en.wikipedia.org/wiki/Branch_predictor)

[超线程技术](https://en.wikipedia.org/wiki/Hyper-threading)

[JMH介绍](http://openjdk.java.net/projects/code-tools/jmh/)

[为什么要用JMH](https://www.zhihu.com/question/276455629/answer/1259967560)

[1] http://hg.openjdk.java.net/code-tools/jmh/file/3769055ad883/jmh-samples/src/main/java/org/openjdk/jmh/samples/JMHSample_12_Forking.java
[2] http://hg.openjdk.java.net/code-tools/jmh/file/3769055ad883/jmh-samples/src/main/java/org/openjdk/jmh/samples/JMHSample_13_RunToRun.java
[3] https://shipilev.net/talks/devoxx-Nov2013-benchmarking.pdf
[4] http://hg.openjdk.java.net/code-tools/jmh/file/3769055ad883/jmh-core/src/main/java/org/openjdk/jmh/annotations/GroupThreads.java
[5] http://hg.openjdk.java.net/code-tools/jmh/file/3769055ad883/jmh-samples/src/main/java/org/openjdk/jmh/samples/JMHSample_15_Asymmetric.java
[6] http://hg.openjdk.java.net/code-tools/jmh/file/3769055ad883/jmh-core/src/main/java/org/openjdk/jmh/annotations/CompilerControl.java
[7] http://hg.openjdk.java.net/code-tools/jmh/file/3769055ad883/jmh-samples/src/main/java/org/openjdk/jmh/samples
[8] https://shipilev.net/talks/devoxx-Nov2013-benchmarking.pdf
[9] https://www.youtube.com/watch?v=VaWgOCDBxYw