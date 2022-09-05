# netty 中 ChannelFuture.sync () 的作用

一般实现nettyServer最后都有两行代码

```java
ChannelFuture future = bootstrap.bind(port).sync();
future.channel().closeFuture().sync();
```

## bootstrap.bind (port).sync () 分析

```java
ChannelFuture future = bootstrap.bind(port).sync();
```

这行代码等同于

```
ChannelFuture future1 = bootstrap.bind(port);
ChannelFuture future2 = future1.sync();
```

那么看看这两个future有什么区别.

通过log日志输出tostring结果发现两个future都是同一个对象:

> future1:AbstractBootstrap$PendingRegistrationPromise@1f19d423(incomplete)
> future2:AbstractBootstrap$PendingRegistrationPromise@1f19d423(success)

区别在于future1包含incomplete信息,而future2已经被修改为success.

**sync()方法**

debuge进入`DefaultPromise#sync`方法

```java
public Promise<V> sync() throws InterruptedException {
    await();
    rethrowIfFailed();
    return this;
}
```

sync中主要的操作是调用await方法,简单是来说就是死循环检测是否完成了,没有则wait等待其他线程唤醒.

```java
public Promise<V> await() throws InterruptedException {
    if (isDone()) {
        return this;
    }
   
    checkDeadLock();

    synchronized (this) {
        while (!isDone()) {
            incWaiters();
            try {
                wait();
            } finally {
                decWaiters();
            }
        }
    }
    return this;
}
```

**future1获取逻辑**

future1在 AbstractBootstrap#doBind方法中初始化,初始化时注册了listener,主要用于promise的注册与绑定.这两件事中`promise.registered()`并没有初始化result,那么基本确定由doBind0触发了result的初始化.

```java
final PendingRegistrationPromise promise = new PendingRegistrationPromise(channel);
regFuture.addListener(new ChannelFutureListener() {
    @Override
    public void operationComplete(ChannelFuture future) throws Exception {
        Throwable cause = future.cause();
        if (cause != null) {

            promise.setFailure(cause);
        } else {

            promise.registered();

            doBind0(regFuture, channel, localAddress, promise);
        }
    }
});
return promise;
```

doBind0的调用链如下:

```
doBind0 
  -> ChannelFuture.bind 
  -> AbstractChannel.bind 
  -> pipeline.bind 
  -> tail.bind 
  -> next.invokeBind 
  -> ((ChannelOutboundHandler) handler()).bind 
  -> unsafe.bind 
  -> AbstractChannel$AbstractUnsafe.bind
```

经过层层地跟踪，最终会执行到 `AbstractUnsafe.bind` 方法

```java
public final void bind(final SocketAddress localAddress, final ChannelPromise promise) {
    ....

    safeSetSuccess(promise);
}
```

进入safeSetSuccess方法发现会调用promise.trySuccess

```java
protected final void safeSetSuccess(ChannelPromise promise) {
    if (!(promise instanceof VoidChannelPromise) && !promise.trySuccess()) {
    }
}
```

进入DefaultPromise类查看trySuccess方法

```java
public boolean trySuccess(V result) {
    //setSuccess0 将 result设置为SUCCESS
    if (setSuccess0(result)) {
        //该方法通知listener
        notifyListeners();
        return true;
    }
    return false;
}
```

唤醒线程在setSuccess方法中被调用

```java
private boolean setValue0(Object objResult) {
    if (RESULT_UPDATER.compareAndSet(this, null, objResult) ||
        RESULT_UPDATER.compareAndSet(this, UNCANCELLABLE, objResult)) {
        checkNotifyWaiters();
        return true;
    }
    return false;
}

private synchronized void checkNotifyWaiters() {
        if (waiters > 0) { //如果有线程等待那么会唤醒所有等待的线程
            notifyAll();
        }
    }
```

## 总结

`bootstrap.bind()` 返回一个 `AbstractBootstrap$PendingRegistrationPromise` 对象，它本质上是一个 `DefaultPromise` 对象，实现了 `Future` 接口

`future.sync()` 最终会使 `DefaultPromise` 的属性 waiters 值加一，然后调用 `Object.wait` 方法阻塞等待

`bootstrap.bind()` 会提交绑定事件到 `EventLoop` 中执行，待 socket 绑定地址成功后会调用 `DefaultPromise` 的 `trySuccess` 方法更改其状态并通知所有 waiter

此处 `future.sync()` 目的是等待异步的 socket 绑定事件完成

![image-20210128020412505](https://d33wubrfki0l68.cloudfront.net/26810de6eda114690848961c7cf93d51be0e58de/98205/images/uploads/image-20210128020412505.png)











参考:[netty 中 ChannelFuture.sync () 的作用是什么？](https://gorden5566.com/post/1066.html)