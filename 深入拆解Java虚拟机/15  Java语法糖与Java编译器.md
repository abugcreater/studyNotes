# 15 | Java语法糖与Java编译器

### 自动装箱与自动拆箱

自动装箱与自动拆箱通过字节码中加入 `[Wrapper].valueOf`（如` Integer.valueOf`）以及 `[Wrapper].[primitive]Value`（如` Integer.intValue`）方法调用来实现的

```
如 list.add(0); 对应的字节码 
9: iconst_0
10: invokestatic java/lang/Integer.valueOf:(I)Ljava/lang/Integer;
13: invokevirtual java/util/ArrayList.add:(Ljava/lang/Object;)Z
```

### 泛型与类型擦除

简单地说，那便是 Java 程序里的泛型信息，在 Java 虚拟机里全部都丢失了。这么做主要是为了兼容引入泛型之前的代码。一般时将类型转化成了`Object`但如果指定了对应的继承类的话还是会转化成相应类.例如<T extend Number>那么类型擦除后会转化为`Number`.

类型擦除通过`checkcast java/lang/Integer` 字节码实现.

字节码中仍存在泛型参数的信息，如方法声明里的 T foo(T)，以及方法签名（Signature）中的“(TT;)TT;”。用于编译时使用

```
T foo(T);
  descriptor: (Ljava/lang/Number;)Ljava/lang/Number;
  flags: (0x0000)
  Code:
    stack=1, locals=2, args_size=2
       0: aload_1
       1: areturn
  Signature: (TT;)TT;
```

### 桥接方法

由于 Java 语义与 Java 字节码中关于重写的定义并不一致，因此 Java 编译器会生成桥接方法作为适配器。例如

```java
class Merchant<T extends Customer> {
  public double actionPrice(T customer) {
    return 0.0d;
  }
}
 
class VIPOnlyMerchant extends Merchant<VIP> {
  @Override
  public double actionPrice(VIP customer) {
    return 0.0d;
  }
}
//会在子类中生成桥接方法

class VIPOnlyMerchant extends Merchant<VIP>
...
  public double actionPrice(VIP);
    descriptor: (LVIP;)D
    flags: (0x0001) ACC_PUBLIC
    Code:
         0: dconst_0
         1: dreturn
 
  public double actionPrice(Customer);
    descriptor: (LCustomer;)D
    flags: (0x1041) ACC_PUBLIC, ACC_BRIDGE, ACC_SYNTHETIC
    Code:
         0: aload_0
         1: aload_1
         2: checkcast class VIP
         5: invokevirtual actionPrice:(LVIP;)D
         8: dreturn
 
// 这个桥接方法等同于
public double actionPrice(Customer customer) {
  return actionPrice((VIP) customer);
}

```

同理如果父类和子类中有同名方法,但是子类的返回与父类返回不一致时也会生成桥接方法.

字符串 switch 编译而成的字节码看起来非常复杂，但实际上就是一个哈希桶。由于每个 case 所截获的字符串都是常量值，因此，Java 编译器会将原来的字符串 switch 转换为 int 值 switch，比较所输入的字符串的哈希值。

由于字符串哈希值很容易发生碰撞，因此，我们还需要用 String.equals 逐个比较相同哈希值的字符串。