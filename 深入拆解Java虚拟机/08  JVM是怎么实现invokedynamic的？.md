# 08 | JVM是怎么实现invokedynamic的？

Java 7 引入了一条新的指令 invokedynamic。该指令的调用机制抽象出调用点这一个概念，并允许应用程序将调用点链接至任意符合条件的方法上。作为 invokedynamic 的准备工作，Java 7 引入了更加底层、更加灵活的方法抽象 ：方法句柄(MethodHandle)。

### 方法句柄的概念

方法句柄是一个强类型的，能够被直接执行的引用 。该引用可以指向常规的静态方法或者实例方法，也可以指向构造器或者字段。当指向字段时，方法句柄实则指向包含字段访问字节码的虚构方法，语义上**等价**于目标字段的 `getter` 或者 `setter` 方法。(此处getter,setter均为虚指)

方法句柄通过方法的参数类型及返回类型组成确定是否适配.方法句柄通过`MethodHandles.Lookup `创建.

当方法句柄通过`MethodType `查找时,需要用户区分具体调用类型.

```java
MethodType methodType = MethodType.methodType(void.class, Object.class);
MethodHandle handle = lookup.findStatic(TestException.class, "bar", methodType);
```

### 方法句柄的操作

`MethodHandle`可以通过`invokeExact`或`invoke`调用方法,区别在于`invokeExact`严格校验参数类型.

`invokeExact` 会确认该 `invokevirtual` 指令对应的方法描述符，和该方法句柄的类型是否严格匹配。在不匹配的情况下，便会在运行时抛出异常。

碰到注解` @PolymorphicSignature`时,Java编译器会根据所传入的参数的声明类型来生成方法描述符.

方法句柄还支持增删改参数的操作，这些操作都是通过生成另一个方法句柄来实现的.

### 方法句柄的实现

Java 虚拟机会对 invokeExact 调用做特殊处理，调用至一个共享的、与方法句柄类型相关的特殊适配器.这个适配器是一个 `LambdaForm`,通过导出class文件分析.

```
final class java.lang.invoke.LambdaForm$MH000 {  static void invokeExact_MT000_LLLLV(jeava.lang.bject, jjava.lang.bject, jjava.lang.bject);
    Code:
        : aload_0
      1 : checkcast      #14                 //Mclass java/lang/invoke/ethodHandle
        : dup
      5 : astore_0
        : aload_32        : checkcast      #16                 //Mclass java/lang/invoke/ethodType
      10: invokestatic  I#22                 // Method java/lang/invoke/nvokers.checkExactType:   (MLjava/lang/invoke/ethodHandle,;Ljava/lang/invoke/ethodType);V  校验参数类型
      13: aload_0
      14: invokestatic   #26     I           // Method java/lang/invoke/nvokers.checkCustomized:(MLjava/lang/invoke/ethodHandle);V 对句柄的执行次数超过阈值时进行优化
      17: aload_0
      18: aload_1
      19: ainvakevirtudl #30             2   // Methodijava/lang/nvokev/ethodHandle.invokeBasic:(LLeava/lang/bject;;V
       23 return
```

虚拟机对invokeBasic的优化,同样是`LambdaForm`,这个试配器获取句柄中MemberName,之后以他未参数调用`linkToStatic`.

```
// 该方法句柄持有的 LambdaForm 实例的 toString() 结果
DMH.invokeStatic_L_V=Lambda(a0:L,a1:L)=>{
  t2:L=DirectMethodHandle.internalMemberName(a0:L);
  t3:V=MethodHandle.linkToStatic(a1:L,t2:L);void}
```

同样虚拟机对`linkToStatic`也有优化,他根据`MemberName`中存储的方法地址或者方法表直接找到目标方法.

```
final class java.lang.invoke.LambdaForm$DMH000 {
  static void invokeStatic000_LL_V(java.lang.Object, java.lang.Object);
    Code:
       0: ldc           #14                 // String CONSTANT_PLACEHOLDER_1 <<Foo.bar(Object)void/invokeStatic>>
       2: checkcast     #16                 // class java/lang/invoke/MethodHandle
       5: astore_0     // 上面的优化代码覆盖了传入的方法句柄
       6: aload_0      // 从这里开始跟初始版本一致
       7: invokestatic  #22                 // Method java/lang/invoke/DirectMethodHandle.internalMemberName:(Ljava/lang/Object;)Ljava/lang/Object;
      10: astore_2
      11: aload_1
      12: aload_2
      13: checkcast     #24                 // class java/lang/invoke/MemberName
      16: invokestatic  #28                 // Method java/lang/invoke/MethodHandle.linkToStatic:(Ljava/lang/Object;Ljava/lang/invoke/MemberName;)V
      19: return
```

但是由于方法句柄是通过间接调用方法的,所以他无法使用内联,性能相对来说回收影响.

### invokedynamic 指令

invokedynamic 将调用点（CallSite）抽象成一个 Java 类，并且将原本由 Java 虚拟机控制的方法调用以及方法链接暴露给了应用程序。在运行过程中，每一条 invokedynamic 指令将捆绑一个调用点，并且会调用该调用点所链接的方法句柄。

在第一次执行 invokedynamic 指令时，Java 虚拟机会调用该指令所对应的启动方法（BootStrap Method），来生成前面提到的调用点，并且将之绑定至该 invokedynamic 指令中。在之后的运行过程中，Java 虚拟机则会直接调用绑定的调用点所链接的方法句柄。

```java
 //bootstrap方法生成callsite
 public static CallSite bootstrap(MethodHandles.Lookup l, String name, MethodType callSiteType) throws Throwable {
    MethodHandle mh = l.findVirtual(Horse.class, name, MethodType.methodType(void.class));
    return new ConstantCallSite(mh.asType(callSiteType));
  }
```

### Java 8 的 Lambda 表达式

该小节可以结合[ Lambda 表达式实现方式](https://blog.csdn.net/zxhoo/article/details/38495085)查看.

Lambda 表达式到函数式接口的转换是通过 invokedynamic 指令来实现的。该 invokedynamic 指令对应的启动方法将通过 ASM 生成一个适配器类。

如果该 Lambda 表达式捕获了其他变量，那么每次执行该 invokedynamic 指令，我们都要更新这些捕获了的变量，以防止它们发生了变化.

在每次执行 invokedynamic 指令时，所调用的方法句柄都需要新建一个适配器类实例。

```java
//实际代码
int x = ..
IntStream.of(1, 2, 3).map(i -> i * 2).map(i -> i * x);

//lambda表达式的适配器类
// i->i*2 对应的适配器类
final class LambdaTest$$Lambda$1 implements IntUnaryOperator {
 private LambdaTest$$Lambda$1();
  Code:
    0: aload_0
    1: invokespecial java/lang/Object."<init>":()V
    4: return
 
 public int applyAsInt(int);
  Code:
    0: iload_1
    1: invokestatic LambdaTest.lambda$0:(I)I
    4: ireturn
}
 
// i->i*x 对应的适配器类
final class LambdaTest$$Lambda$2 implements IntUnaryOperator {
 private final int arg$1;
 
 private LambdaTest$$Lambda$2(int);
  Code:
    0: aload_0
    1: invokespecial java/lang/Object."<init>":()V
    4: aload_0
    5: iload_1
    6: putfield arg$1:I
    9: return
 
 private static java.util.function.IntUnaryOperator get$Lambda(int);
  Code:
    0: new LambdaTest$$Lambda$2
    3: dup
    4: iload_0
    5: invokespecial "<init>":(I)V
    8: areturn
 
 public int applyAsInt(int);
  Code:
    0: aload_0
    1: getfield arg$1:I
    4: iload_1
    5: invokestatic LambdaTest.lambda$1:(II)I
    8: ireturn
}

```

捕获了局部变量的 Lambda 表达式多出了一个 get$Lambda 的方法。启动方法便会所返回的调用点链接至指向该方法的方法句柄。

### Lambda 以及方法句柄的性能分析

不管是捕获型的还是未捕获型的 Lambda 表达式，它们的性能上限皆可以达到直接调用的性能。其中，捕获型 Lambda 表达式借助了即时编译器中的逃逸分析，来避免实际的新建适配器类实例的操作。如果没有逃逸分析,则有可能每次执行都会创建新适配器类.

