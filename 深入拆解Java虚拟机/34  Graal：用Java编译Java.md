# 34 | Graal：用Java编译Java

GraalVM 是一个高性能的、支持多种编程语言的执行环境。它既可以在传统的 OpenJDK 上运行，也可以通过 AOT（Ahead-Of-Time）编译成可执行文件单独运行，甚至可以集成至数据库中运行。

Graal 编译器可以通过 Java 虚拟机参数`-XX:+UnlockExperimentalVMOptions -XX:+UseJVMCICompiler`启用。



### Graal 和 Java 虚拟机的交互

即时编译器与 Java 虚拟机的交互

1. 响应编译请求；
2. 获取编译所需的元数据（如类、方法、字段）和反映程序执行状态的 profile；
3. 将生成的二进制码部署至代码缓存（code cache）里。

Graal引入了[Java 虚拟机编译器接口](http://openjdk.java.net/jeps/243)（JVM Compiler Interface，JVMCI），将上述三个功能抽象成一个 Java 层面的接口。可以通过替换 Graal编译器相关的 jar 包,完成Graal升级.

### Graal 和 C2 的区别

Graal 和 C2 最为明显的一个区别是：Graal 是用 Java 写的，而 C2 是用 C++ 写的。相对来说，Graal 更加模块化，也更容易开发与维护

如果 C2 和 Graal 采用相同的优化手段，那么它们的编译结果是一样的。所以，程序达到稳定状态（即不再触发新的即时编译）的性能，也就是峰值性能，将也是一样的。

### Graal 的实现

Graal 编译器将编译过程分为前端和后端两大部分。前端用于实现平台无关的优化（如方法内联），以及小部分平台相关的优化；而后端则负责大部分的平台相关优化（如寄存器分配），以及机器码的生成。

>  感兴趣的同学可以阅读 Graal repo 里配置这些编译优化阶段的源文件
> [HighTier.java](https://github.com/oracle/graal/blob/master/compiler/src/org.graalvm.compiler.core/src/org/graalvm/compiler/core/phases/HighTier.java)，[MidTier.java](https://github.com/oracle/graal/blob/master/compiler/src/org.graalvm.compiler.core/src/org/graalvm/compiler/core/phases/MidTier.java)，以及[LowTier.java](https://github.com/oracle/graal/blob/master/compiler/src/org.graalvm.compiler.core/src/org/graalvm/compiler/core/phases/LowTier.java)。

Graal 与 C2 相比会更加激进。它从设计上便十分青睐这种基于假设的优化手段。在编译过程中，Graal 支持自定义假设，并且直接与去优化节点相关联。

当对应的去优化被触发时，Java 虚拟机将负责记录对应的自定义假设。而 Graal 在第二次编译同一方法时，便会知道该自定义假设有误，从而不再对该方法使用相同的激进优化。