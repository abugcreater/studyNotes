# 02 | Java的基本类型

## Java 虚拟机的 boolean 类型

在 Java 虚拟机规范中，boolean 类型则被映射成 int 类型。具体来说，“true”被映射为整数 1，而“false”被映射为整数 0。这个编码规则约束了 Java 字节码的具体实现。

## Java 的基本类型

除了上面提到的 boolean 类型外，Java 的基本类型还包括整数类型 byte、short、char、int 和 long，以及浮点类型 float 和 double。

![image-20211201170736652](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20211201170736652.png)

在这些基本类型中，boolean 和 char 是唯二的无符号类型。

Java 的浮点类型采用 IEEE 754 浮点数格式。

float中存在两个0,即 +0.0F 和 -0.0F 这两个定义后，我们便可以定义浮点数中的正无穷及负无穷。正无穷就是任意正浮点数（不包括 +0.0F）除以 +0.0F 得到的值，而负无穷是任意正浮点数除以 -0.0F 得到的值。



## Java 基本类型的大小

Java 虚拟机每调用一个 Java 方法，便会创建一个栈帧。为了方便理解，这里我只讨论供解释器使用的解释栈帧（interpreted frame）。

这种栈帧有两个主要的组成部分，分别是局部变量区，以及字节码的操作数栈。这里的局部变量是广义的，除了普遍意义下的局部变量之外，它还包含实例方法的“this 指针”以及方法所接收的参数。

在 Java 虚拟机规范中，局部变量区等价于一个数组，并且可以用正整数来索引。除了 long、double 值需要用两个数组单元来存储之外，其他基本类型以及引用类型的值均占用一个数组单元。