# Redis分布式锁

## 1. 关于分布式锁

### 1.1 分布式锁是什么?

分布式锁是指在分布式系统中控制同步访问共享资源的一种方式.

### 1.2 分布式锁的特性

1. 一个资源在同一时间内只能被一条机器访问
2. 高可用/高性能的获取锁与释放锁
3. 具备可重入特性
4. 具备锁失效机制,防止死锁
5. 非阻塞,没有获取到锁直接返回获取锁失败

## 2. SetNX

### 2.1 SetNX基本原理

在单节点的Redis中通常使用`SetNX`.该指令通常代指redis对`set`命令加上`nx`参数进行使用,`set`命令的使用方式如下:

>  SET key value [EX seconds|PX milliseconds] [NX|XX] [KEEPTTL]

`SetNX`指令只有在key不存在时才能set成功,否则返回失败.但是`SetNX`不具备可重入性特性,需要手动维护key值信息保存UUID等随机数判断是否是同一进程.且其他进程能释放当前进程的锁.

一般获取锁与删除锁都是通过LUA脚本的方式原子执行.

```lua
-- lua删除锁：
-- KEYS和ARGV分别是以集合方式传入的参数，对应上文的Test和uuid。
-- 如果对应的value等于传入的uuid。
if redis.call('get', KEYS[1]) == ARGV[1] 
    then 
	-- 执行删除操作
        return redis.call('del', KEYS[1]) 
    else 
	-- 不成功，返回0
        return 0 
end
```

### 2.2 SetNX存在问题

当锁遇到故障转移时会导致锁资源失效.比如在主节点获取到锁资源,然而锁还没有同步到从节点,主节点宕机,此时其他服务获取锁资源成功,导致并发问题.

## 3. RedLock

### 3.1 RedLock算法

 在Redis集群中获取锁的操作:

1. 获取当前Unix时间,单位ms
2. 尝试从N(N=集群节点数量),使用相同的key和随机值中获取锁.获取锁过程中,会设置一个网络连接和响应超时时间,超时时间小于锁失效时间.避免无用等待.超时后会切换其它实例.
3. 客户端用当前时间减去开始获取时间获得锁使用时间.当大多数节点(n/2+1)都获取到锁,且使用时间小于锁失效时间时才算成功
4. 如果获取成功,锁真正有效时间等于有效时间减去获取锁使用的时间
5. 如果获取锁失败,则需要进行解锁操作.

### 3.2 存在问题

因为网络和硬件等原因导致redis实例访问超时,此时可能会导致获取redis分布式锁失败.

Redis没有备份,在获取n/2 + 1个节点后其中一个节点宕机,之后再重启,就存在其他客户端能同时获取到同一把锁的可能.锁的互斥性就被破坏了.此时我们需要开始AOF持久化,最好设置fsync=always,才能保证重启后还能有这把锁信息.

RedLock也可能因为时钟漂移问题而导致获取锁失败,因为节点时钟不同步,本地时间与标准时间不一致导致计算时间错误,获取锁失败.

## 4. Redisson

### 4.1 Redisson介绍

Redisson - 具有内存数据网格功能的 Redis Java 客户端。超过 50 个基于 Redis 的 Java 对象和服务：Set、Multimap、SortedSet、Map、List、Queue、Deque、Semaphore、Lock、AtomicLong、Map Reduce、发布/订阅、Bloom filter、Spring Cache、Tomcat、Scheduler、JCache API、Hibernate , MyBatis, RPC, 本地缓存...

[github](https://github.com/redisson/redisson)

[官网](https://redisson.org/)

### 4.2 Redisson锁实现

Redisson普通的锁实现源码主要是**RedissonLock**这个类.

加锁过程是通过一段lua脚本实现,代码如下

```lua
- 其中keys[1]代表加锁key,ARGV[1]表示锁的默认生存时间默认30s,ARGV[2]代表的是加锁的客户端ID285475da-9152-4c83-822a-67ee2f116a79:52,最后那个1数字表示可重入次数
if (redis.call('exists', KEYS[1]) == 0) then " +
   "redis.call('hincrby', KEYS[1], ARGV[2], 1); " +
   "redis.call('pexpire', KEYS[1], ARGV[1]); " +
   "return nil; " +
   "end; " +
"if (redis.call('hexists', KEYS[1], ARGV[2]) == 1) then " +
    "redis.call('hincrby', KEYS[1], ARGV[2], 1); " +
    "redis.call('pexpire', KEYS[1], ARGV[1]); " +
    "return nil; " +
    "end; " +
"return redis.call('pttl', KEYS[1]);"
```

普通锁结构在redis中是一个hash结构.myLockKey:{{ID(UUID):ThreadId}:hincrby}

代码解释:先判断锁是否存在,如果不存在则加锁,使用`hincrby`设置一个hash结构.获取锁后会返回这个锁的生存时间.    基于信息量，当锁被其它资源占用时，当前线程通过 Redis 的 channel 订阅锁的释放事件，一旦锁释放会发消息通知待等待的线程进行竞争.当收到锁释放信号量时,会循环尝试获取锁,获取成功返回true,时间超时返回false.

第二段是可重入判断,当这个锁存在时,如果是当前应用的当前进程则更新重入次数,即更新hash中的value字段.

### 4.3 锁续期机制

Redisson 提供了一个续期机制， 只要客户端 1 一旦加锁成功，就会启动一个 Watch Dog.只有使用默认过期时间时看门狗机制才会生效,否则会使用指定过期时间.

```java
private <T> RFuture<Long> tryAcquireAsync(long leaseTime, TimeUnit unit, long threadId) {
    if (leaseTime != -1) {
        return tryLockInnerAsync(leaseTime, unit, threadId, RedisCommands.EVAL_LONG);
    }
    RFuture<Long> ttlRemainingFuture = tryLockInnerAsync(commandExecutor.getConnectionManager().getCfg().getLockWatchdogTimeout(), TimeUnit.MILLISECONDS, threadId, RedisCommands.EVAL_LONG);
    ttlRemainingFuture.onComplete((ttlRemaining, e) -> {
        if (e != null) {
            return;
        }

        // lock acquired
        if (ttlRemaining == null) {
            scheduleExpirationRenewal(threadId);
        }
    });
    return ttlRemainingFuture;
}
```

Watch Dog 机制其实就是一个后台定时任务线程，获取锁成功之后，会将持有锁的线程放入到一个 `RedissonLock.EXPIRATION_RENEWAL_MAP`里面，然后每隔 10 秒 `（internalLockLeaseTime / 3）` 检查一下，如果客户端 1 还持有锁 key（判断客户端是否还持有 key，其实就是遍历 `EXPIRATION_RENEWAL_MAP` 里面线程 id 然后根据线程 id 去 Redis 中查，如果存在就会延长 key 的时间），那么就会不断的延长锁 key 的生存时间。

### 4.4 **释放锁机制**

释放锁的源码:

```java
@Override
public RFuture<Void> unlockAsync(long threadId) {
    RPromise<Void> result = new RedissonPromise<Void>();
    // 1. 异步释放锁
    RFuture<Boolean> future = unlockInnerAsync(threadId);
    // 取消 Watch Dog 机制
    future.onComplete((opStatus, e) -> {
        cancelExpirationRenewal(threadId);

        if (e != null) {
            result.tryFailure(e);
            return;
        }

        if (opStatus == null) {
            IllegalMonitorStateException cause = new IllegalMonitorStateException("attempt to unlock lock, not locked by current thread by node id: "
                    + id + " thread-id: " + threadId);
            result.tryFailure(cause);
            return;
        }

        result.trySuccess(null);
    });

    return result;
}

protected RFuture<Boolean> unlockInnerAsync(long threadId) {
    return commandExecutor.evalWriteAsync(getName(), LongCodec.INSTANCE, RedisCommands.EVAL_BOOLEAN,
            // 判断锁 key 是否存在
            "if (redis.call('hexists', KEYS[1], ARGV[3]) == 0) then " +
                "return nil;" +
            "end; " +
            // 将该客户端对应的锁的 hash 结构的 value 值递减为 0 后再进行删除
            // 然后再向通道名为 redisson_lock__channel publish 一条 UNLOCK_MESSAGE 信息
            "local counter = redis.call('hincrby', KEYS[1], ARGV[3], -1); " +
            "if (counter > 0) then " +
                "redis.call('pexpire', KEYS[1], ARGV[2]); " +
                "return 0; " +
            "else " +
                "redis.call('del', KEYS[1]); " +
                "redis.call('publish', KEYS[2], ARGV[1]); " +
                "return 1; "+
            "end; " +
            "return nil;",
            Arrays.<Object>asList(getName(), getChannelName()), LockPubSub.UNLOCK_MESSAGE, internalLockLeaseTime, getLockName(threadId));
}
```

主要操作流程

1. 删除锁（这里注意可重入锁，在上面的脚本中有详细分析）。
2. 广播释放锁的消息，通知阻塞等待的进程（向通道名为 `redisson_lock__channel` publish 一条 `UNLOCK_MESSAGE` 信息）。
3. 取消 Watch Dog 机制，即将 `RedissonLock.EXPIRATION_RENEWAL_MAP` 里面的线程 id 删除，并且 cancel 掉 Netty 的那个定时任务线程。


### 4.5 Redisson中Rlock实现

Redisson中的解决方案是使用RedissonRedLock类实现,该类继承自RedissonMultiLock ,重写 `failedLocksLimit` 方法,当获取失败时修改时间.使用时需要设定多个key,但是使用过程中需要自己判断 key 落在哪个节点上，对使用者不是很友好。

![img](https://segmentfault.com/img/remote/1460000040335374)



Redisson RedLock 已经被弃用，直接使用普通的加锁即可，会基于 wait 机制将锁同步到从节点，但是也并不能保证一致性。仅仅是最大限度的保证一致性。



参考:[分布式锁概念及实现方式](https://juejin.cn/post/6844903753288515592)

[Redis分布式锁](http://redis.cn/topics/distlock.html)

[Redisson 实现分布式锁原理分析](https://zhuanlan.zhihu.com/p/135864820)

[Redisson单进程Redis分布式悲观锁的使用与实现](https://www.zhihu.com/question/425541402/answer/1523396048)

[分布式锁 Redisson Redlock](https://zhuanlan.zhihu.com/p/440865954)

[Redisson 分布式锁源码 09：RedLock 红锁的故事](https://segmentfault.com/a/1190000040335370)



