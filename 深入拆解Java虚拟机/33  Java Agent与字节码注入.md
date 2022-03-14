# 33 | Java Agent与字节码注入

以Java agent方法运行permain方法能在main方法之前运行.

```
执行语句
# 注意第一条命令会向 manifest.txt 文件写入两行数据，其中包括一行空行
$ echo 'Premain-Class: org.example.MyAgent
' > manifest.txt
$ jar cvmf manifest.txt myagent.jar org/
$ java -javaagent:myagent.jar HelloWorld
premain
Hello, World
```

可以使用Attach API远程加载执行agentmain方法.

```
import java.io.IOException;
 
import com.sun.tools.attach.*;
 
public class AttachTest {
  public static void main(String[] args)
      throws AttachNotSupportedException, IOException, AgentLoadException, AgentInitializationException {
    if (args.length <= 1) {
      System.out.println("Usage: java AttachTest <PID> /PATH/TO/AGENT.jar");
      return;
    }
    VirtualMachine vm = VirtualMachine.attach(args[0]);
    vm.loadAgent(args[1]);
  }
}

运行方式
$ echo 'Agent-Class: org.example.MyAgent
' > manifest.txt
$ jar cvmf manifest.txt myagent.jar org/
$ java HelloWorld
Hello, World
$ jps
$ java AttachTest <pid> myagent.jar
agentmain
// 最后一句输出来自于运行 HelloWorld 的 Java 进程

```

### 字节码注入

```java
public class MyAgent {
  public static void premain(String args, Instrumentation instrumentation) {
   //通过instrumentation注册类加载事件拦截器
   instrumentation.addTransformer(new MyTransformer());
  }
 
  static class MyTransformer implements ClassFileTransformer {
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
        ProtectionDomain protectionDomain, byte[] classfileBuffer) throws IllegalClassFormatException {
        //打印被加载类字节码的前4位
      System.out.printf("Loaded %s: 0x%X%X%X%X\n", className, classfileBuffer[0], classfileBuffer[1],
          classfileBuffer[2], classfileBuffer[3]);
          //返回更新过后的字节码
      return null;
    }
  }
}
```

Java agent提供redefine和retransform方法针对已加载的类实例.

`redefine`指的是舍弃原本的字节码，并替换成由用户提供的 byte 数组。该功能比较危险，一般用于修复出错了的字节码。

`retransform`则将针对所传入的类，重新调用所有已注册的`ClassFileTransformer`的`transform`方法.执行场景

1. 在执行`premain`或者`agentmain`方法前，Java 虚拟机早已加载了不少类，而这些类的加载事件并没有被拦截，因此也没有被注入。使用`retransform`功能可以注入这些已加载但未注入的类。
2. 在定义了多个 Java agent，多个注入的情况下，我们可能需要移除其中的部分注入。当调用`Instrumentation.removeTransformer`去除某个注入类后，我们可以调用`retransform`功能，重新从原始 byte 数组开始进行注入。

### 基于字节码注入的 profiler

利用字节码注入来实现代码覆盖工具（例如[JaCoCo](https://www.jacoco.org/jacoco/)），或者各式各样的 profiler。

当使用字节码注入开发 profiler 时，需要辩证地看待所收集的数据。它仅能表示在被注入的情况下程序的执行状态，而非没有注入情况下的程序执行状态

### 面向切面编程

面向方面编程的核心理念是定义切入点（pointcut）以及通知（advice）。程序控制流中所有匹配该切入点的连接点（joinpoint）都将执行这段通知代码。



参考:

深入Java调试体系之JPDA体系概览

- [第 1 部分: JPDA 体系概览](https://my.oschina.net/itblog/blog/1358659)
- [第 2 部分: JVMTI 和 Agent 实现](http://www.uml.org.cn/j2ee/200910101.asp)
- [第 3 部分: JDWP 协议及实现](https://my.oschina.net/itblog/blog/1421889)
- [第 4 部分: Java 调试接口（JDI）](https://web.archive.org/web/20150422000007/http://www.ibm.com/developerworks/cn/java/j-lo-jpda4/)

[attach机制实现](http://lovestblog.cn/blog/2014/06/18/jvm-attach/)

