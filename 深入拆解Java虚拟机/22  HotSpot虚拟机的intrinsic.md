# 22 | HotSpot虚拟机的intrinsic

在 Java 9 之前，字符串是用 char 数组来存储的，主要为了支持非英文字符。然而，大多数 Java 程序中的字符串都是由 Latin1 字符组成的。也就是说每个字符仅需占据一个字节，而使用 char 数组的存储方式将极大地浪费内存空间。

Java 9 引入了 [Compact Strings](http://openjdk.java.net/jeps/254) 的概念，当字符串仅包含 Latin1 字符时，使用一个字节代表一个字符的编码格式，使得内存使用效率大大提高。

在 HotSpot 虚拟机中，所有被该注解标注的方法都是 HotSpot intrinsic。对这些方法的调用，会被 HotSpot 虚拟机替换成高效的指令序列。而原本的方法实现则会被忽略掉。HotSpot 虚拟机将为标注了`@HotSpotIntrinsicCandidate`注解的方法额外维护一套高效实现。如果 Java 核心类库的开发者更改了原本的实现，那么虚拟机中的高效实现也需要进行相应的修改，以保证程序语义一致

### intrinsic 与 CPU 指令

86_64 体系架构的 SSE4.2 指令集就包含一条指令 PCMPESTRI，让它能够在 16 字节以下的字符串中，查找另一个 16 字节以下的字符串，并且返回命中时的索引值。HotSpot 虚拟机便围绕着这一指令，开发出 X86_64 体系架构上的高效实现，并替换原本对`StringLatin1.indexOf`方法的调用。

同理`Math.addExact`方法 在CPU中就有表示指令结果是否溢出的溢出标识位（overflow flag）,我们只需在加法指令之后比较溢出标志位，便可以知道 int 值之和是否溢出了。

还有`Integer.bitCount`计算二进制中有多少个1,可以直接用`popcnt`代替.

### intrinsic 与方法内联

intrinsic 的实现方式分为两种

1. 一种是独立的桩程序。它既可以被解释执行器利用，直接替换对原方法的调用；也可以被即时编译器所利用，它把代表对原方法的调用的 IR 节点，替换为对这些桩程序的调用的 IR 节点。以这种形式实现的 intrinsic 比较少，主要包括`Math`类中的一些方法。
2. 另一种则是特殊的编译器 IR 节点。显然，这种实现方式仅能够被即时编译器所利用。在编译过程中，即时编译器会将对原方法的调用的 IR 节点，替换成特殊的 IR 节点，并参与接下来的优化过程。最终，即时编译器的后端将根据这些特殊的 IR 节点，生成指定的 CPU 指令。大部分的 intrinsic 都是通过这种方式实现的。即时编译器将在方法内联过程中，将对 intrinsic 的调用替换为这些特殊的 IR 节点，并最终生成指定的 CPU 指令。

大部分被`intrinsic`标记的都是native方法,减少JNI的性能开销.

### 已有 intrinsic 简介

主要的`intrinsic`类型

1. `Unsafe`类中的方法
2. `StringBuilder`和`StringBuffer`类的方法。HotSpot 虚拟机将优化利用这些方法构造字符串的方式，以尽量减少需要复制内存的情况。
3. `String`类、`StringLatin1`类、`StringUTF16`类和`Arrays`类的方法。HotSpot 虚拟机将使用 SIMD 指令（single instruction multiple data，即用一条指令处理多个数据）对这些方法进行优化。
4. 基本类型的包装类、`Object`类、`Math`类、`System`类中各个功能性方法，反射 API、`MethodHandle`类中与调用机制相关的方法，压缩、加密相关方法。

[JDK12中的intrinsic]( http://hg.openjdk.java.net/jdk/hs/file/46dc568d6804/src/hotspot/share/classfile/vmSymbols.hpp#l727)
[JDK8中的intrinsic]( http://hg.openjdk.java.net/jdk8u/jdk8u/hotspot/file/2af8917ffbee/src/share/vm/classfile/vmSymbols.hpp#l647)



### 引申:JNI性能消耗

1. JVM不会内联本地方法.因为它们已经编译好了所以不存在即时编译
2. 将一个对象传递给native方法时,native方法访问Java字段需要类似反射的东西.签名在字符串中指定并从JVM查询,既缓慢有容易出错.
3. 为了调用DLL或库,JVM可能必须执行额外的工作,例如重新排列堆栈上的项目.



参考:

[stackoverflow](https://stackoverflow.com/questions/7699020/what-makes-jni-calls-slow)

[The overhead of native calls in Java](https://www.javamex.com/tutorials/jni/overhead.shtml)

[检查JNI成本](http://www.mastercorp.free.fr/Ing1/Cours/Java/java_lesson1/doc/Tutorial/performance/JPNativeCode_fm.htm)