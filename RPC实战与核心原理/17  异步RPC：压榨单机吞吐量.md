# 17 | 异步RPC：压榨单机吞吐量

### 如何提升单机吞吐量？

使用异步线程并行调用,减少串行调用消耗的时间.

**那是什么影响到了 RPC 调用的吞吐量呢？**

由于处理 RPC 请求比较耗时,造成CPU长时间等待,导致CPU利用率不够.而RPC请求处理本身不够毫秒级,主要还是服务提供方的业务处理逻辑慢.

要提升吞吐量,需要将RPC实现完全异步.

### 调用端如何异步？

发起异步请求,收到一个Future,之后调用Future获取结果.

对于 RPC 框架，无论是同步调用还是异步调用，调用端的内部实现都是异步的。调用端发送消息前创建一个Future,而带有消息id,框架会将消息id与Future映射存储.收到服务提供方响应后获取future并将结果注入future.最后动态代理从future获取正确返回值.

同步调用是框架去获取返回值,异步调用由用户获取future后决定何时去获取返回值.

![image-20220113133757219](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220113133757219.png)

### 如何做到 RPC 调用全异步？

多个服务使用一个业务线程池?  指的是服务调用方共享一个业务线程池.

**那服务端如何支持业务逻辑异步呢？**

让 RPC 框架支持 `CompletableFuture`，实现 RPC 调用在调用端与服务端之间完全异步。

- 服务调用方发起RPC调用,直接拿到返回值`CompletableFuture`对象
- 服务端创建完`CompletableFuture`对象后将业务逻辑放到线程池中异步操作,完成后调用`complete`方法,异步通知
- 调用端收到通知后,自动调用`CompletableFuture`对象的complete方法.

参考:

[Netty线程模型](https://zhuanlan.zhihu.com/p/87630368)

[Fork/Join框架](https://pdai.tech/md/java/thread/java-thread-x-juc-executor-ForkJoinPool.html)

[FutureTask](https://pdai.tech/md/java/thread/java-thread-x-juc-executor-FutureTask.html)

