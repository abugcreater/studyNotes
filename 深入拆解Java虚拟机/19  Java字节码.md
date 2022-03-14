# 19 | Java字节码

虚拟机栈描述方法执行的内存模型:

方法执行时创建栈帧(stack Frame)存储局部变量表,操作数栈,动态链接,方法出口等信息.方法从开始调用到结束对应了栈帧在虚拟机栈中入栈到出栈的过程.现在运行中只有一个栈帧处于活跃状态,称为当前栈帧.

动态链接:每个栈帧内部都包含一个指向当前方法所在类型的**运行时常量池**的引用，以便对当前方法的代码实现**动态链接**。

方法出口:如果方法调用正常完成（没有抛出任何异常包括`throw`显式抛出的异常），当前栈帧承担着**恢复调用者状态的责任**，包括**恢复调用者的局部变量表和操作数栈**，以及正确递增程序计数器，以跳过刚才执行的方法调用指令等。被调用方法的返回值会被压入调用者栈帧的操作数栈，然后继续执行;如果调用异常,那么一定不会有方法返回值返回给他的调用者

本地方法栈:如果JVM支持本地方法栈，那么这个栈一般会在线程创建的时候按线程分配。





参考:

[虚拟机栈](https://blog.csdn.net/abc67509227/article/details/101562041)



### 操作数栈

操作数栈用来存放计算的操作数以及返回结果:执行每一条指令之前，Java 虚拟机要求该指令的操作数已被压入操作数栈中。在执行指令时，Java 虚拟机会将该指令所需的操作数弹出，并且将指令的结果重新压入栈中。

```
  public void foo() {
    Object o = new Object();
  }
  // 对应的字节码如下：
  public void foo();
    0  new java.lang.Object [3]
    3  dup	//复制栈顶元素
    4  invokespecial java.lang.Object() [8]
    7  astore_1 [o]
    8  return
```

当执行new操作时,需要开辟一块没有初始化的内存作为引用,压入操作数栈.之后通过该引用调用构造器.因为该指令需要消耗操作数栈上的元素作为调用者和参数.所以我们需要复制一份new指令的结果,用来调用构造器.

pop 指令则常用于舍弃调用指令的返回结果。一般在调用了某个函数但不使用返回结果时.

![常数加载指令表](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20211223104713487.png)

操作数栈的压入弹出都是一条条指令完成的。唯一的例外情况是在抛异常时，Java 虚拟机会清除操作数栈上的所有内容，而后将异常实例压入操作数栈上。

### 局部变量区

局部变量表:用来存储方法中的所有局部变量值(包括传递的参数);

局部变量值:变量的作用范围,变量的名称和数据类型信息

**如果当前字节码程序计数器的值超出了某个变量的作用域，那么这个变量对应的Slot就可以被其他变量使用。
（变量的作用域为 [start, start+length] ，要注意的一点是作用域两边都是包含的）**

![局部变量区访问指令表](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20211223111254236.png)

### Java 字节码简介

Java 相关指令，包括各类具备高层语义的字节码，即 new（后跟目标类，生成该类的未初始化的对象），instanceof（后跟目标类，判断栈顶元素是否为目标类 / 接口的实例。是则压入 1，否则压入 0），checkcast（后跟目标类，判断栈顶元素是否为目标类 / 接口的实例。如果不是便抛出异常），athrow（将栈顶异常抛出），以及 monitorenter（为栈顶对象加锁）和 monitorexit（为栈顶对象解锁）。

```
 public int neg(int i) {
    return -i;
  }
 
  public int foo(int i) {
    return neg(neg(i));
  }
  // foo 方法对应的字节码如下：foo 方法对应的字节码如下：
  public int foo(int i);
    0  aload_0 [this]
    1  aload_0 [this]
    2  iload_1 [i]
    3  invokevirtual FooTest.neg(int) : int [25]
    6  invokevirtual FooTest.neg(int) : int [25]
    9  ireturn
```

![image-20211223131052832](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20211223131052832.png)









[Oracle字节码文档](https://docs.oracle.com/javase/specs/jvms/se10/html/jvms-6.html#jvms-6.5)