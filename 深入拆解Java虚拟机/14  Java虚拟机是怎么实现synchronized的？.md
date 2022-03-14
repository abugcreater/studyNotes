# 14 | Java虚拟机是怎么实现synchronized的？

jvm 通过对编译的字节码新增`monitorenter`和`monitorexit`指令实现`synchronized`关键字.

```
public void foo(Object lock) {
    synchronized (lock) {
      lock.hashCode();
    }
  }
  // 上面的 Java 代码将编译为下面的字节码
  public void foo(java.lang.Object);
    Code:
       0: aload_1
       1: dup
       2: astore_2
       3: monitorenter
       4: aload_1
       5: invokevirtual java/lang/Object.hashCode:()I
       8: pop
       9: aload_2
      10: monitorexit
      11: goto          19
      14: astore_3
      15: aload_2
      16: monitorexit
      17: aload_3
      18: athrow
      19: return
    Exception table:
       from    to  target type
           4    11    14   any
          14    17    14   any
```

上市实例中包含一个`monitorenter`和两个`monitorexit`指令,确保无论方法是正常退出还是异常退出都能进行锁释放.
如果用`synchronized`修饰方法,可以看到方法的访问权限中有`ACC_SYNCHRONIZED`,静态方法的锁是CLASS文件,实例方法是this关键字.

### 重量级锁

JVM会阻塞加锁失败的线程,并在锁释放时唤醒他们.线程阻塞,唤醒涉及到操作系统的用户态切换至内核态.所以一般通过线程自旋替代.CAS自旋,线程在没有竞争到锁的情况下通过运行空指令,当锁释放时竞争锁.自旋会造成CPU资源浪费,以及不公平锁.

JVM通过自适应机制选择是阻塞还是自旋.

### 轻量级锁和偏向锁

轻量级和偏向锁可以通过[下图](https://wiki.openjdk.java.net/display/HotSpot/Synchronization)查看.

![image-20211210160658573](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20211210160658573.png)

多个线程在不同时间请求同一把锁时,使用轻量级锁.

单个线程请求同一把锁时,使用偏向锁.

对象头中记录了该对象的锁状态. 00:轻量级锁  01:无锁/偏向锁 10:重量级锁 11:跟垃圾回收算法的标记有关

轻量级锁加锁时,如果是非重量锁,则在当前栈帧中记录锁记录,将锁对象的标记字段记录到锁记录中.JVM通过CAS替换,如果替换成功,则成功获取到锁.如果没有,则有可能是该线程在重复获取锁,此时会删除锁记录.或者有其他线程已经获取了锁,此时锁膨胀为重量级锁.



偏向锁加锁时,JVM通过CAS将当前线程地址加到标记字段中并将最后3位成101.

在接下来的运行过程中，每当有线程请求这把锁，Java 虚拟机只需判断锁对象标记字段中：最后三位是否为 101，是否包含当前线程的地址，以及 epoch 值是否和锁对象的类的 epoch 值相同。如果都满足，那么当前线程持有该偏向锁，可以直接返回。

加锁失败是,需要撤销偏向锁并替换成轻量级锁.当大于`XX:BiasedLockingBulkRebiasThreshold`中的值时偏向锁会失效.而epoch值则可以理解为竞争失败次数.



[偏向锁百科](https://openjdk.java.net/jeps/374)