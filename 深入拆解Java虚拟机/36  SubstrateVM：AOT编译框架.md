# 36 | SubstrateVM：AOT编译框架

AOT 编译指的则是，在**程序运行之前**，便将字节码转换为机器码的过程。它的成果可以是需要链接至托管环境中的动态共享库，也可以是独立运行的可执行文件。

 AOT 编译针对的目标代码:狭义,原本可以被即使编译的代码;广义将其理解为类似于GCC的静态编译器.

有点:无须在运行过程中耗费 CPU 资源来进行即时编译，而程序也能够在启动伊始就达到理想的性能。

Java 9 引入了实验性 AOT 编译工具[jaotc](http://openjdk.java.net/jeps/295)。它借助了 Graal 编译器，将所输入的 Java 类文件转换为机器码，并存放至生成的动态共享库之中。

在启动过程中，Java 虚拟机将加载参数`-XX:AOTLibrary`所指定的动态共享库，并部署其中的机器码。这些机器码的作用机理和即时编译生成的机器码作用机理一样，都是在方法调用时切入，并能够去优化至解释执行。

jaotc 使用的机制便是类指纹（class fingerprinting）。它会在动态共享库中保存被 AOT 编译的 Java 类的摘要信息。在运行过程中，Java 虚拟机负责将该摘要信息与已加载的 Java 类相比较，一旦不匹配，则直接舍弃这份 AOT 编译的机器码。

### SubstrateVM 的设计与实现

SubstrateVM 的设计初衷是提供一个高启动性能、低内存开销，并且能够无缝衔接 C 代码的 Java 运行时。与jaotc的区别:脱离了 HotSpot 虚拟机; 要求目标程序是封闭的,没有其他类库需要加载.探索整个编译空间,将所有可能执行到的方法纳入编译范围中.

从执行时间上来划分，SubstrateVM 可分为两部分：`native image generator `以及 SubstrateVM 运行时。后者 SubstrateVM 运行时便是前面提到的精简运行时，经过 AOT 编译的目标程序将跑在该运行时之上。

`native image generator `则包含了真正的 AOT 编译逻辑。将Java类文件编译为可执行文件或者动态链接库。

`native image generator` 将采用指针分析（points-to analysis），从用户提供的程序入口出发，探索并初始化所有可达的代码,生成可执行文件,将已初始化堆保存到对快照中。

### SubstrateVM 的启动时间与内存开销

SubstrateVM 的启动时间和内存开销非常少。

由于 SubstrateVM 的轻量特性，它十分适合于嵌入至其他系统之中。Oracle Labs 的另一个团队便是将 Truffle 语言实现嵌入至 Oracle 数据库之中，这样就可以在数据库中运行任意语言的预储程序（stored procedure）。如果你感兴趣的话，可以搜索 Oracle Database Multilingual Engine（MLE），或者参阅这个[网址](https://www.oracle.com/technetwork/database/multilingual-engine/overview/index.html)。我们团队也在与 MySQL 合作，开发 MySQL MLE，详情可留意我们在今年 Oracle Code One 的[讲座](https://oracle.rainfocus.com/widget/oracle/oow18/catalogcodeone18?search=MySQL JavaScript)。