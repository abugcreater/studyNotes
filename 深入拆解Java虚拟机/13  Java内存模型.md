# 13 | Java内存模型

### Java 内存模型与 happens-before 关系
`happens-before`关系用来描述两个操作间的内存可见性.如果操作A`happens-before`操作B,那操作B能见到A的结果,且`happens-before`有传递性.

在程序中,靠后的字节码没有观测(没有使用靠前字节码的结果)靠前的字节码那么就有可能重排序.

JMM中定义了线程中的`happens-before`关系:

1. 解锁在加锁之后
2. volatile写操作在读操作之后
3. 线程Thread.start()处于线程第一个操作
4. 线程最后一个操作在在的终止事件之前
5. 对其他线程的中断操作处于被中断线程接受到的中断事件之前
6. 构造器最后一个操作早于第一个操作

### JMM的底层实现

JMM通过内存屏障(memory barrier)来禁止重排序.

对于日常接触的x86_64架构来说,只有当涉及到写读操作时内存屏障才会被替换成具体指令,其他都是空操作(no-op).

volatile底层实现:编译器在读操作和写操作前都插入一些内存屏障.当涉及到写操作时,编译器会强制将缓存刷新到主存,时寄存器中的缓存失效.该字段对所有线程可见.



### 锁，volatile 字段，final 字段与安全发布

volatile 字段的另一个特性是即时编译器无法将其分配到寄存器里。换句话说，volatile 字段的每次访问均需要直接从内存中读写。

具体来说，解锁操作 happens-before 之后对同一把锁的加锁操作。实际上，在解锁时，Java 虚拟机同样需要强制刷新缓存，使得当前线程所修改的内存对其他线程可见。

即时编译器会在 final 字段的写操作后插入一个写写屏障，以防某些优化将新建对象的发布（即将实例对象写入一个共享引用中）重排序至 final 字段的写操作之前。

一个对象只有在它的构造函数返回后才处于可预测的、一致的状态，所以从它的构造函数中发布一个对象可以发布一个不完整构造的对象。确保构造器不会产生逃逸.













[Java安全对象发布](https://vlkan.com/blog/post/2014/02/14/java-safe-publication/)

[深入理解Java内存模型](https://www.infoq.cn/minibook/java_memory_model)

[编译器中写读内存屏障会被替换成具体指令](http://gee.cs.oswego.edu/dl/jmm/cookbook.html)

[逃逸分析](https://zhuanlan.zhihu.com/p/208366191)

[并发测试工具jcstress](https://www.cnblogs.com/wwjj4811/p/14310930.html)