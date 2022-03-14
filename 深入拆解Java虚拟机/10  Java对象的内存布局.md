# 10 | Java对象的内存布局

Java实例化对象的五种方式:
使用构造器的:`new`语句,反射
不使用构造器:`Object.clone()`方法,反序列化及`Unsafe.allocateInstance `

而调用构造器过程中,子类的构造器需要调用父类的构造器(使用`super`直接调用,或者`this`关键词间接调用).

### 压缩指针

Java对象都有一个对象头`Object header`,对象头由标记字段(jvm有关该对象的运行数据) 和类型指针(该对象的类)组成.在64位的虚拟机中这两个字段的长度都为64位.

[压缩指针](https://wiki.openjdk.java.net/display/HotSpot/CompressedOops)的作用范围:对象头的类型指针(64位压缩至32位),引用类型的字段以及引用类型数组.

原理:32位内最多可以表示4GB，64位地址分为堆的基地址+偏移量，当堆内存<32GB时候，在压缩过程中，把偏移量/8后保存到32位地址。在解压再把32位地址放大8倍，所以启用CompressedOops的条件是堆内存要在4GB*8=32GB以内。

原理可以参考 [原理](https://blog.csdn.net/liujianyangbj/article/details/108049482).

为了方便内存寻址,Java使用了内存对齐,可以通过`-XX:ObjectAlignmentInBytes`设置,默认为8.

默认情况下,JVM堆中对象需要对齐至8的倍数.如果对象用不到8N个字节,则需要填充(pedding).

字段内存对齐的其中一个原因，是让字段只出现在同一 CPU 的缓存行中。如果字段不是对齐的，那么就有可能出现跨缓存行的字段。也就是说，该字段的读取可能需要替换两个缓存行，而该字段的存储也会同时污染两个缓存行。

### 字段重排列

 Java 虚拟机重新分配字段的先后顺序，以达到内存对齐的目的。Java 虚拟机中有三种排列方法（对应 Java 虚拟机选项 -XX:FieldsAllocationStyle，默认值为 1），但都会遵循如下两个规则。

排列方式:

1. 类型0,引用在原始前面,然后依次是longs/doubles, ints, shorts/chars, bytes, 最后是填充字段, 以满足对其要求.
2. 类型1,引用在原始类型之后
3. 类型2,JVM在布局时会尽量使父类对象和子对象挨在一起, 原因后面解释.

规则:

1. 如果一个字段占据C个字节,那么该字段的偏移量需要对齐至N*C.偏移量指的是字段地址与对象的起始地址差值.
2. 子类所继承的偏移量,需要与父类对应字段的偏移量保持一致.

```java
//Java实例
class A {
  long l;
  int i；
}
 
class B extends A {
  long l;
  int i;
}

# 启用压缩指针时，B 类的字段分布
B object internals:
 OFFSET  SIZE   TYPE DESCRIPTION
      0     4        (object header)
      4     4        (object header)
      8     4        (object header)
     12     4    int A.i                                       0
     16     8   long A.l                                       0
     24     8   long B.l                                       0
     32     4    int B.i                                       0
     36     4        (loss due to the next object alignment)
     
     
# 关闭压缩指针时，B 类的字段分布
B object internals:
 OFFSET  SIZE   TYPE DESCRIPTION
      0     4        (object header)
      4     4        (object header)
      8     4        (object header)
     12     4        (object header)
     16     8   long A.l
     24     4    int A.i
     28     4        (alignment/padding gap)                  
     32     8   long B.l
     40     4    int B.i
     44     4        (loss due to the next object alignment)
     

```

以上内存分布可以通过 JOL 工具打印.



Java 虚拟机会让不同的 @Contended 字段处于独立的缓存行中，因此你会看到大量的空间被浪费掉。