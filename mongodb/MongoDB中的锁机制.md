# MongoDB中的锁机制

## 1. MongoDB中的锁粒度

MongoDB 有 4 个粒度级别的锁

1. Global (MongoD 实例) – 所有的数据库上加锁
2. Database – 锁定某个数据库
3. Collection – 锁定某个集合
4. Document – 锁定某个文档

其中Document 粒度由WiredTiger引擎提供.

 ## 2.MongoDB中的锁类型

MongoDB 使用读写锁，并且允许并发读共享对资源 (database 或者 collection) 的访问.

MongoDB 中的锁分为四个类型:
1. 读锁（共享锁）R
2. 写锁（排它锁）W
3. 意向读锁（意向共享锁）r
4. 意向写锁（意向排它锁）w

对一个数据行或者 document 加意向锁的含义是：当前事务想要在这个节点或者这个节点的子节点加某种锁 (共享或者排它)，有了意向锁以后，**对任何数据加共享锁或者排它锁都要先加意向锁再加具体锁**.

加锁的限制如下图所示:

![img](https://bbs-img.huaweicloud.com/data/forums/attachment/forum/201805/06/185306rlor8esh2qtyecem.png)

WiredTiger 对大多数操作一般默认是使用乐观锁.WiredTiger 仅在全局，数据库和集合级别使用意向锁。当存储引擎检测到两个操作之间的冲突时，会发生写入冲突，导致 MongoDB 透明地重试该操作.

我们可以使用命令:`db.serverStatus () `或者 `db.currentOp ()`查看锁信息.

下面的表格列出了一些操作以及它们用于文档级锁定存储引擎的锁类型：

| 操作                   | Database                   | Collection                         |
| ---------------------- | -------------------------- | ---------------------------------- |
| query                  | r (意向共享锁)             | r (意向共享锁)                     |
| insert                 | w (意向排它锁)             | w (意向排它锁)                     |
| remove                 | w (意向排它锁)             | w (意向排它锁)                     |
| update                 | w (意向排它锁)             | w (意向排它锁)                     |
| aggregation (聚合)     | r (意向共享锁)             | r (意向共享锁)                     |
| create an index (前台) | W (排它锁)                 |                                    |
| create an index (后台) | w (意向排它锁)             | w (意向排它锁)                     |
| list collections       | r (意向共享锁)             |                                    |
| map-reduce             | W (排它锁) 以及 R (共享锁) | w (意向排它锁) 以及 r (意向共享锁) |

## 3. MongoDB中的意向锁

而意向锁协议，是一种对树形（层级）资源进行并发控制的协议。它由"操作约定"和"冲突矩阵"两部分组成.

### 3.1 MongoDB中的意向锁的定义

MongoDb使用了简化版的意向锁协议，抛却了SIX状态，保留了 IS/IX/S/X四种锁状态

>  IS (意向共享)  IX (意向排它)  S (共享)  X (排它) 

意向锁使用方式:

1. 对一个节点加IX/X锁时，必须先（递归）获取其父节点的IX锁
2. 对一个节点加IS/S锁时，必须先（递归）获取其父节点的IS锁

MongoDB中的资源层级结构如下：

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/G2NnvykaFvU1yQnQXJmrFxUVCsf0xkdQLZfBnbPploeHiccMrfa18iaWjoIKNapUHsuasQEAGrQGHtnyKwDaiaZgQ/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

要想获取Collection的意向锁,必须同时获取Clobal,Db2的意向锁.

意向锁的设计较为简洁，仅仅通过一个矩阵（冲突矩阵），两条原则（递归加锁）就可以满足数据库系统中对资源的并发控制的需求。

### 3.2 Mongo中意向锁的实现

在设计意向锁时我们必须考虑以下两点:

1. 一个高并发读写的db中，IS/IX锁源源不断的加上来，且相互不冲突，在这种条件下，如何避免X锁的饿死。
2. 如何避免死锁。

 整体结构 mongoDB中的意向锁实现主要在 lock*manager.cpp/lock*state.cpp两部分。一个简化的意向锁的原语可以用如下两条语句来表达。

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/G2NnvykaFvU1yQnQXJmrFxUVCsf0xkdQLL48vhCQyrNY9RqEV4JzbcIqeibh4GoNM6yR85Fic7QMwxDia1VONcgHA/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

其整体结构如下:

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/G2NnvykaFvU1yQnQXJmrFxUVCsf0xkdQv3AdFKgLSbkfUxgldA04yYmiaNTmpMibKF1N5vT8VefGc6RqAQdfo0bg/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)



**BucketArray**

上图中，意向锁划分为128个元素的BucketsArray, BucketsArray可以无锁访问，一个lock(ResourceId, LockMode)操作，首先通过Hash(ResourceId)%128 找到对于的bucket，这一步无锁操作非常重要，充分利用了不同ResourceId的无关性，使得意向锁模块具备水平扩展性。

**Bucket**

每个Bucket是ResourceId->LockHead的哈希表。该哈希表被Bucket对象中的mutex保护。

**LockHead**

LockHead是对应于某个ResourceId的锁对象。LockHead维护着所有对该ResourceId的锁请求。LockHead由ConflictList和GrantList组成。ConflictList是该锁的等待队列， GrantList是持有锁的对象链表。

**引用计数数组**

为了解决每次获取锁时都要遍历GrantList列表导致的性能浪费,**MongoDB为GrantList和ConflictList增加了引用计数数组**.

在将一个对象增加到GrantList中时，顺带对grantedCounts[mode] 累加，如果grantedCounts[mode]是从0到1的变化， 则将grantedModes对应的bitMask设置为1。 从GrantList中删除对象时，是一个逆向的对称操作。这样，在判断某个模式是否与GrantList中已有对象冲突时，可以通过对grantedModes和待加节点的mode进行比较，将时间复杂度从O(n)降到O(1)

**避免饿死**

如果ConflictList中有X锁在等待，而GrantedList中的IS/IX锁源源不断的进来，那么X锁就一直得不到调度.为了解决这个问题,MongoDB中为加锁操作增加了**compatibleFirst**参数.

MongoDB中源码如下:

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/G2NnvykaFvU1yQnQXJmrFxUVCsf0xkdQKuPqee8gFG6ick4s4Aj8Kt9oIISYqMzJ40ib2fJTDjouU1dc5e9FITkg/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)



1. 如果锁请求与该锁目前的grantModes冲突，则进入等待，这一点毫无疑问。

2. 207行可以看到如果请求与grantModes不冲突，也未必能加锁成功，还要检验锁资源上的compatibleFirstCount, 该变量可以解释为：锁资源的GrantList中compatibleFirst=true的属性的锁请求的元素的个数。如果GrantList中无compatibleFirst的锁请求，且conflictList非空（有等待的锁请求），则将请求加入到conflictList中。

3. 如果获锁成功，则将锁请求加入到GrantList中，并累加锁资源的compatibleFirstCount计数器。



### 3.3 **死锁检测**

MongoDB意向锁的死锁检测基于广度优先遍历(BFS)算法。某个锁请求是否会产生死锁，等价于 “从有向图中的一点出发，是否可以找到一个环”.

**死锁检测的构图**

MongoDB中，一个锁依赖图 G=（V, E）， Vi = (Request, Resource, Mode)， 即图中的一个点的含义为对某个资源的某种模式的锁请求，一个点Vi对另一个点Vj有依赖 当且仅当 Vj 持有 Vi.Resource的锁，且锁模式与Vi.Mode冲突，且Vj 本身也在等待其他资源。概念有点绕，举个例子。

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/G2NnvykaFvU1yQnQXJmrFxUVCsf0xkdQHibm39CJ9zyIBu0A4NdNOMPpW74YKRSczZ6vYkk8aqGAKcicticxWmRzQ/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)



上图中，有三个Lock，分别为Lock1， Lock2， Lock3，Lock1当前持有Res1，在等待Res2, Lock2当前持有Res2，在等待Res3，Lock3 当前持有Res3，在等待Res1。很明显死锁了，但是如何将其转化为有向图，使得计算机能帮我们检测死锁呢。



我们从Lock1 Acquire Res2来看， 由于Res2被Lock2持有，所以Lock1 Acquire Res2 依赖 Lock2 Release Res2。 而Lock2 Release Res2 依赖 Lock2 Acquire Res3， Lock2 Acquire Res3 依赖 Lock3 Release Res3。 如下图所示：

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/G2NnvykaFvU1yQnQXJmrFxUVCsf0xkdQ6Bt2GEvp2zBfHBEdIicNk8lYnrONalIcvKZuO1IIIbOHDXCszibT0oIQ/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

图中Release动作的依赖并不是必须的，可以简化成：

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/G2NnvykaFvU1yQnQXJmrFxUVCsf0xkdQP7nMSBecSBr8BUeD00B2iaLfl6iaUKuhKyRErtBpZe5Nn218QibO13XZQ/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)








参考:[MongoDB 的锁事](https://generalthink.github.io/2019/07/05/mongodb-locks/)

[浅析MongoDB中的意向锁](https://mp.weixin.qq.com/s/aD6AySeHX8uqMlg9NgvppA)

[MongoDB面试指南之并发](https://juejin.cn/post/6844904081731878925)