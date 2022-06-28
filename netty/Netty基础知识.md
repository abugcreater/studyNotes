# Netty基础知识

## 1. 简介

Netty是异步的基于事件驱动的网络应用框架,以高性能,高并发著称.主要用于开发基于TCP协议的网络IO程序(如IM系统等).基于Java NIO构建.层次结构 `TCP/IP->JDK原生IO->Java NIO->Netty`.

## 2.Java中的IO模型

Java中一般有三种IO模型:BIO,NIO,AIO

### 2.1 BIO

BIO:是同步阻塞IO.当客户端发起请求后服务端开启一个线程处理链接.在处理过程中客户端会一直阻塞.这种模型对线程量耗费极大,线程利用率低.可以通过线程池优化,但是无法解决线程利用率低的问题.

<img src="https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ac7eae92fade4ff8b078962d58fc4b1f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp" alt="BIO模型" style="zoom:33%;" />

### 2.2 NIO

NIO:同步,非阻塞IO.服务端一个线程能处理多个链接,客户端请求被注册到多路复用器(selector)上.多路复用器轮询并处理.它提高了线程的利用率.

<img src="https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f78988ad208942ae8dd80e1b731159ac~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp" alt="NIO模型" style="zoom:33%;" />

NIO与BIO的区别:BIO面向流,NIO面向缓冲;BIO是阻塞IO,NIO是非阻塞IO;BIO没有选择器,NIO有

### 2.3 AIO

AIO:异步非阻塞IO.在这种模型中，由操作系统完成与客户端之间的 read/write，之后再由操作系统主动通知服务器线程去处理后面的工作，在这个过程中服务器线程不必同步等待 read/write 完成。



**名词解释**

- 阻塞: 调用read/write过程中如果没有准备就绪,线程一直等待,那就阻塞读写
- 非阻塞:调用read/write过程中如果没有准备就绪,线程会先处理其他工作,等就绪后在回来处理
- 异步:read/write完全托管给系统完成,完成后系统回调IO程序进行后续处理.
- 同步:read/write有IO程序完成

## 3.NIO详解

NIO面向缓冲区(buffer)处理数据,基于channel和buffer操作,数据从通道读到缓冲区,或从缓冲区写到通道.

selector对应一个处理线程,该处理线程上能注册很多的channel通道,每个通道对应一个buffer缓冲区.selector根据事件类型,分发到各个channel.buffer和channel都是双向的,既能写又能读.

### 3.1 缓冲区(Buffer)

缓冲区（Buffer）本质上是一个可读可写的内存块,channel读写文件都要经过buffer.NIO中Buffer是一个抽象类,针对各个类型都有子类实现.其中 ByteBuffer 支持类型化的数据存取,不仅支持byte,也支持char,long,int等.

### 3.2 通道(Channel)

通道（Channel）是双向的，可读可写.顶级接口Channel.常用子类如下

- FileChannel：用于文件读写
- DatagramChannel：用于 UDP 数据包收发
- ServerSocketChannel：用于服务端 TCP 数据包收发
- SocketChannel：用于客户端 TCP 数据包收发

### 3.3 选择器(Selector)

选择器（Selector）是实现 IO 多路复用的关键，多个 Channel 注册到某个 Selector 上，当 Channel 上有事件发生时，Selector 就会取得事件然后调用线程去处理事件。

## 4.Netty架构

<img src="https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c90f34f2270c45b698e9b92f86892401~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp" alt="Netty 架构如下" style="zoom:50%;" />

### 4.1 Netty与NIO比较

NIO存在的缺点:API和类库复杂,开发量大;程序员需要非常熟悉网络编程;存在bug如Epoll会导致selector空轮训,消耗CPU资源.

Netty优点:1.设计优雅,按需拓展事件模型;2.具备高性能和更大吞吐量,减少资源消耗;3.提供安全传输特性;4.支持多种主流协议,支持开发私有协议

### 4.2 Netty线程模型

**初级版本**

Netty基于主从Reactor多线程模型.

<img src="https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/581873c3e86546eca3ee37a8099be1f9~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp" alt="netty简单模型" style="zoom:50%;" />



1. 链接建立时注册到BossGroup,BossGroup当作主要reactor
2. 收到请求将请求封装到socketChannel,注册到WorkerGroup
3. WorkerGroup接受到事件后调用handler处理

**中级版本**

<img src="https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f249300efb1049b9b55b967db64335a2~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp" alt="netty中级模型" style="zoom:50%;" />

1. BossGroup负责与客户端建立链接,WorkerGroup负责处理每个连接的读写等具体操作
2. BossGroup 和 WorkerGroup都包含很多个线程在循环不断的处理事件
3. BossGroup 工作线程池中的线程,轮训serverSocketChannel中的OP_ACCEPT事件,建立连接后,生成NioSocketChannel,分派到WorkerGroup的selector上,然后处理下一个事件
4. WorkerGroup工作线程池中的线程,轮训注册在其上的 NioSocketChannel 的 read/write 事件,然后处理,之后获取下一个事件.

**终极版本**

<img src="https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b96c3059616c449c92a0f48880b3904e~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp" alt="netty终级模型" style="zoom:50%;" />



1. BossGroup 和 WorkerGroup,都是NioEventLoopGroup线程池中都有, NioEventLoop 线程.BossGroup 线程池用来处理客户端连接,WorkerGroup线程池处理连接上的读写请求.

2. NioEventLoopGroup 相当于事件循环组,每个事件循环就是NioEventLoop

3. NioEventLoop表示循环处理时间的线程,每个NioEventLoop都包含一个selector,用于处理注册的socket连接

4. NioEventLoop循环执行:**select**->**processSelectedKeys**->**runAllTasks**

5. 每个 BossNioEventLoop 中循环执行以下三个步骤：

   1. **select**：轮训注册在其上的 ServerSocketChannel 的 accept 事件（OP_ACCEPT 事件）

   2. **processSelectedKeys**：处理 accept 事件，与客户端建立连接，生成一个 NioSocketChannel，并将其注册到某个 WorkerNioEventLoop 上的 Selector 上

   3. **runAllTasks**：再去以此循环处理任务队列中的其他任务
   
6. 每个 WorkerNioEventLoop 中循环执行以下三个步骤：

	1. **select**：轮训注册在其上的 NioSocketChannel 的 read/write 事件（OP_READ/OP_WRITE 事件）

   2. **processSelectedKeys**：在对应的 NioSocketChannel 上处理 read/write 事件

   3. **runAllTasks**：再去以此循环处理任务队列中的其他任务

7. 在以上两个**processSelectedKeys**步骤中，会使用 Pipeline（管道），Pipeline 中引用了 Channel，即通过 Pipeline 可以获取到对应的 Channel，Pipeline 中维护了很多的处理器

### 4.3 Netty中的组件

#### 4.3.1 Netty中的Handler组件

Netty中Handler顶级接口是`ChannelHandler`,服务端中的`NettyServerHandler`基础类`SimpleChannelInboundHandler`类,`SimpleChannelInboundHandler`继承自`ChannelInboundHandlerAdapter`,再寻找它的父类是`ChannelHandlerAdapter`.

多个`ChannelHandler`经过`ChannelPipeline `串联,形成一个责任链,各个`ChannelHandler`依次处理.数据处理流程:读取数据-->解码数据-->处理数据-->编码数据-->发送数据

![ChannelHandler责任链](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b0c1cbc27e394da4bb8ffb6f5a27db3a~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp)

Netty中的`ChannelHandler`体系如下

<img src="https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f30c59d9236d401397a1d5d1cf361980~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp" alt="ChannelHandler体系" style="zoom:50%;" />

![netty-channelHandler-system](..\img\netty-channelHandler-system.png)

- ChannelInboundHandler 用于处理入站IO事件
- ChannelOutboundHandler 用于处理出站 IO 事件
- ChannelInboundHandlerAdapter 用于处理入站 IO 事件
- ChannelOutboundHandlerAdapter 用于处理出站 IO 事件

ChannelPipeline 提供了 ChannelHandler 链的容器.当事件到达服务端后,对于服务端来讲是入站事件,那么事件会经过一系列ChannelInboundHandler 处理.当事件从服务端发送给客户端时,对服务端来讲是出栈事件,会经过一系列的ChannelOutboundHandler 处理.

自定义的`NettyServerHandler `通常继承自`ChannelInboundHandlerAdapter `.通过覆盖器方法可以在指定时刻被触发调用.

#### 4.3.2 Netty中的Pipeline组件

ChannelPipeline 实现了一种高级形式的拦截过滤器模式，使用户可以完全控制事件的处理方式，以及 Channel 中各个 ChannelHandler 如何相互交互。

每个`Channel`包含了一个`ChannelPipeline`,`DefaultChannelPipeline`是默认实现类,他维护了`AbstractChannelHandlerContext`类型的双向链表.而`AbstractChannelHandlerContext`也持有`DefaultChannelPipeline`.

```java
class DefaultChannelPipeline
//头和尾都是固定的,addLast方法修改 tail.pre     
final AbstractChannelHandlerContext head;
final AbstractChannelHandlerContext tail;

class AbstractChannelHandlerContext
//指针指向县后两个handler    
volatile AbstractChannelHandlerContext next;
volatile AbstractChannelHandlerContext prev;
private final DefaultChannelPipeline pipeline;
```

![netty中Pipline组件](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/78ea3ddc18f14a7dad573fc5fdfde0a6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp)

ChannelHandlerContext 除了包含 ChannelHandler 之外，还关联了对应的 Channel 和 Pipeline。可以这么来讲：ChannelHandlerContext、ChannelHandler、Channel、ChannelPipeline 这几个组件之间互相引用，互为各自的属性，你中有我、我中有你。

入站事件会从同流向尾,并依次在每个`ChannelInboundHandler`中处理,而出站事件则是从尾流向头,并依次在每个`ChannelOutboundHandler` 处理.

![netty中pipline出入站执行顺序](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c6b34810a3e24e6c80024a8f2359ba0a~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp)



#### 4.3.3 Netty中的EventLoopGroup 组件

在基于 Netty 的 TCP Server 代码中，包含了两个 EventLoopGroup——bossGroup 和 workerGroup，EventLoopGroup 是一组 EventLoop 的抽象。EventLoopGroup 继承了EventExcutorGroup接口,往上溯源最终继承自Java中的线程池接口Executor.本质就是一个线程池.

![netty-eventloop-system](D:\笔记\img\netty-eventloop-system.png)

Netty 为了更好地利用多核 CPU 的性能，一般会有多个 EventLoop 同时工作，每个 EventLoop 维护着一个 Selector 实例，Selector 实例监听注册其上的 Channel 的 IO 事件。

EventLoopGroup 通过next方法,按照一定规则从group中选取一个EventLoop处理IO事件.

通常服务端Boss EventLoopGroup只包含一个EventLoop(单线程),该EventLoop维护一个注册了 ServerSocketChannel 的 Selector 实例.EventLoop 通过不断轮询Selector得到客户端连接事件.然后将Channel交个worker EventLoop,worker通过next方法获取下一个EventLoop,并将这个channel注册到其中的Selector上.之后由这个EventLoop执行后续的IO事件.

![3万字加50张图，带你深度解析 Netty 架构与原理（下）](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e7ed749cbed5495783518fddca53e2bf~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp)

#### 4.3.4 Netty中的TaskQueue

Netty中每个NioEventLoopGroup的父类SingleThreadEventExecutor,都维护了一个taskQueue属性.设计他的目的是在任务提交速度大于线程处理速度的时候起缓冲作用.或者异步地处理Selector监听到的IO事件.

![Netty中的TaskQueue](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7700db0c6eb04a349b7256e5036d8515~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp)

TaskQueue使用场景:

1. 处理用户程序的自定义普通任务的时候
2. 处理用户程序的自定义定时任务的时候
3. 非当前 Reactor 线程调用当前 Channel 的各种方法的时候

第一种场景,实际开发过程中当遇到某个事件处理非常耗时时可以通过加入任务队列异步处理.

```java
// 通过 ctx.channel().eventLoop().execute()将耗时
// 操作放入任务队列异步执行
ctx.channel().eventLoop().execute(() ->{
    // 需要执行的业务操作
});
```

第二种场景,如果碰到某些任务需要延期执行,则可以通过schedule方法.

```java
// 通过 ctx.channel().eventLoop().schedule()将操作
// 放入任务队列定时执行（5min 之后才进行处理）
ctx.channel().eventLoop().schedule(()->{
    // 需要执行的业务操作
}, 5 , TimeUnit.SECONDS);
```

对于第三种场景,利用本地缓存包括用户信息及其对应的channel信息,当其他reactor线程调用时,可以从本地缓存中获取,并执行write操作

#### 4.3.5 Netty中的Future和Promise

Netty多数IO接口都是异步的,通过返回Future,而IO都是异步进行.调用者可以通过Futre获取执行结果.也可以通过新增监听获取执行结果.

```java
//对通道关闭进行监听
ChannelFuture future = channel.closeFuture().sync();
future.addListener(new ChannelFutureListener() {
    /**
     * 回调方法，上面的 bootstrap.connect()操作执行完之后触发
     */
    @Override
    public void operationComplete(ChannelFuture future)
            throws Exception {
        if (future.isSuccess()) {
            System.out.println("client has closed!");
            // TODO 其他处理
        } else {
            System.out.println("closed fail!");
            // TODO 其他处理
        }
    }
});
```

Promise 继承了Future是可写的 Future，Future 自身并没有写操作相关的接口，Netty 通过 Promise 对 Future 进行扩展，用于设置 IO 操作的结果。相对于Future方法多类一些`setxx`方法.

Netty 发起 IO 写操作的时候，会创建一个新的 Promise 对象，例如调用 ChannelHandlerContext 的 write(Object object)方法时，会创建一个新的 ChannelPromise.源码如下

```java
AbstractChannelHandlerContext类
@Override
public ChannelFuture write(Object msg) {
    return write(msg, newPromise());
}

@Override
public ChannelFuture write(final Object msg, final ChannelPromise promise) {
    if (msg == null) {
        throw new NullPointerException("msg");
    }

    try {
        if (isNotValidPromise(promise, true)) {
            ReferenceCountUtil.release(msg);
            // cancelled
            return promise;
        }
    } catch (RuntimeException e) {
        ReferenceCountUtil.release(msg);
        throw e;
    }
    write(msg, false, promise);

    return promise;
}
```

当操作完成可以设置是否成功,通知listener.



参考:[3万字加50张图，带你深度解析 Netty 架构与原理（上）](https://juejin.cn/post/6919486248599945224#heading-6)

[3万字加50张图，带你深度解析 Netty 架构与原理（下）](https://juejin.cn/post/6919486634710794254#heading-0)

