# 50 | 装饰器模式：通过剖析Java IO类库源码学习装饰器模式

装饰器模式主要用于解决继承过于复杂,通过组合来代替继承的问题.主要作用是给原始类添加功能,这点区别于代理模式,代理模式是给原始类添加跟原始类无关的功能.

装饰器典型案例可以参考Java的IO实现.

![image-20210820144948299](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20210820144948299.png)

```
InputStream fileIS = new FileInputStream(FILE_NAME);
InputStream bufferIS = new BufferedInputStream(fileIS);
DataInputStream dataInputStream = new DataInputStream(bufferIS);
```

`BufferedInputStream`该类的作用是读带缓存的字节流,提高读取效率,而`DataInputStream`主要用于获取特定数据类型.

上述代码段通过装饰器模式,使我们在使用时候既能缓存又能获取特定数据类型.

装饰器模式相对于简单的组合关系有两个比较特殊的地方.

1. 装饰器类和原始类继承同样的父类,这样我们可以对原始类嵌套多个装饰器类.
2. 装饰器类是对功能的增强,这也是装饰器模式应用场景的一个重要特点.



