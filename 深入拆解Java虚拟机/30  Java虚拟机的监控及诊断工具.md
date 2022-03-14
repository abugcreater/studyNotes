# 30 | Java虚拟机的监控及诊断工具

## 命令相关

1. `jps`将打印所有正在运行的 Java 进程。

2. `jstat`允许用户查看目标 Java 进程的类加载、即时编译以及垃圾回收相关的信息。它常用于检测垃圾回收问题以及内存泄漏问题。

   引申;G1 GC,Java 虚拟机不再设置 Eden 区、Survivor 区，老年代区的内存边界，而是将堆划分为若干个等长内存区域.逻辑上我们只有一个 Survivor 区。当需要迁移 Survivor 区中的数据时（即 Copying GC），我们只需另外申请一个或多个内存区域，作为新的 Survivor 区。

3. `jmap`允许用户统计目标 Java 进程的堆中存放的 Java 对象，并将它们导出成二进制文件。

   引申;[attach机制](http://lovestblog.cn/blog/2014/06/18/jvm-attach/):jvm提供一种jvm进程间通信的能力，能让一个进程传命令给另外一个进程，并让它执行内部的一些操作

4. `jinfo`将打印目标 Java 进程的配置参数，并能够改动其中 manageabe 的参数。

5. `jstack`将打印目标 Java 进程中各个线程的栈轨迹、线程状态、锁状况等信息。它还将自动检测死锁。

6. `jcmd`则是一把瑞士军刀，可以用来实现前面除了`jstat`之外所有命令的功能。

## GUI相关

### eclipse MAT

eclipse 的[MAT 工具](https://www.eclipse.org/mat/)便是其中一个能够解析这类二进制快照的工具。

 MAT 计算对象占据内存的[两种方式](https://help.eclipse.org/mars/topic/org.eclipse.mat.ui.help/concepts/shallowretainedheap.html?cp=46_2_1)。第一种是 Shallow heap，指的是对象自身所占据的内存。第二种是 Retained heap，指的是当对象不再被引用时，垃圾回收器所能回收的总内存，包括对象自身所占据的内存，以及仅能够通过该对象引用到的其他对象所占据的内存。

支配树的概念源自图论。在一则流图（flow diagram）中，如果从入口节点到 b 节点的所有路径都要经过 a 节点，那么 a 支配（dominate）b。

在 a 支配 b，且 a 不同于 b 的情况下（即 a 严格支配 b），如果从 a 节点到 b 节点的所有路径中不存在支配 b 的其他节点，那么 a 直接支配（immediate dominate）b。这里的支配树指的便是由节点的直接支配节点所组成的树状结构。

### Java Mission Control

[Java Mission Control](http://jdk.java.net/jmc/)（JMC）是 Java 虚拟机平台上的性能监控工具。它包含一个 GUI 客户端，以及众多用来收集 Java 虚拟机性能数据的插件，如 JMX Console（能够访问用来存放虚拟机各个子系统运行数据的[MXBeans](https://en.wikipedia.org/wiki/Java_Management_Extensions#Managed_beans)），以及虚拟机内置的高效 profiling 工具 Java Flight Recorder（JFR）。

JFR 将记录运行过程中发生的一系列事件,且性能开销小。其中包括 Java 层面的事件，如线程事件、锁事件，以及 Java 虚拟机内部的事件，如新建对象、垃圾回收和即时编译事件。

JFR 的事件共有四种类型，它们分别为以下四种。

1. 瞬时事件（Instant Event），用户关心的是它们发生与否，例如异常、线程启动事件。
2. 持续事件（Duration Event），用户关心的是它们的持续时间，例如垃圾回收事件。
3. 计时事件（Timed Event），是时长超出指定阈值的持续事件。
4. 取样事件（Sample Event），是周期性取样的事件。
   取样事件的其中一个常见例子便是方法抽样（Method Sampling），即每隔一段时间统计各个线程的栈轨迹。如果在这些抽样取得的栈轨迹中存在一个反复出现的方法，那么我们可以推测该方法是热点方法。

JFR 的启用方式有三种，分别为在命令行中使用`-XX:StartFlightRecording=`参数，使用`jcmd`的`JFR.*`子命令，以及 JMC 的 JFR 插件。JMC 能够加载 JFR 的输出结果，并且生成各种信息丰富的图表。  