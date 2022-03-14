# 35 | Truffle：语言实现框架

编译器分为前端和后端：前端负责词法分析、语法分析、类型检查和中间代码生成，后端负责编译优化和目标代码生成。

### Truffle 项目简介

Truffle 是一个用 Java 写就的语言实现框架。基于 Truffle 的语言实现仅需用 Java 实现词法分析、语法分析以及针对语法分析所生成的抽象语法树（Abstract Syntax Tree，AST）的解释执行器，便可以享用由 Truffle 提供的各项运行时优化。可以运行在任何Java虚拟机上.

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220104155901060.png" alt="image-20220104155901060" style="zoom:50%;" />

### Partial Evaluation

假设有一段程序`P`，它将一系列输入`I`转换成输出`O`（即`P: I -> O`）。而这些输入又可以进一步划分为编译时已知的常量`IS`，和编译时未知的`ID`。

那么，我们可以将程序`P: I -> O`转换为等价的另一段程序`P': ID -> O`。这个新程序`P'`便是`P`的特化（Specialization），而从`P`转换到`P'`的这个过程便是所谓的 Partial Evaluation。

我们可以将 Truffle 语言的解释执行器当成`P`，将某段用 Truffle 语言写就的程序当作`IS`，并通过 Partial Evaluation 特化为`P'`。由于 Truffle 语言的解释执行器是用 Java 写的，因此我们可以利用 Graal 编译器将`P'`编译为二进制码。

Truffle 的 Partial Evaluator 会不断进行方法内联（直至遇到被``@TruffleBoundary`注解的方法)。

### 节点重写

Truffle 语言解释器会收集每个 AST 节点所代表的操作的类型，并且在即时编译时，作出针对所收集得到的类型 profile 的特化（specialization）。

还是以加法操作为例，如果所收集的类型 profile 显示这是一个整数加法操作，那么在即时编译时我们会将对应的 AST 节点当成整数加法；如果是一个字符串加法操作，那么我们会将对应的 AST 节点当成字符串加法。

在即时编译过后，如果运行过程中发现 AST 节点的实际类型和所假设的类型不同，Truffle 会主动调用 Graal 编译器提供的去优化 API，返回至解释执行 AST 节点的状态，并且重新收集 AST 节点的类型信息。之后，Truffle 会再次利用 Graal 编译器进行新一轮的即时编译。