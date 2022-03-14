# 04 | 网络通信：RPC框架在网络通信上更倾向于哪种网络IO模型？

### 常见的网络 IO 模型

常见的网络 IO 模型分为四种：同步阻塞 IO（BIO）、同步非阻塞 IO（NIO）、IO 多路复用和异步非阻塞 IO（AIO）。在这四种 IO 模型中，只有 AIO 为异步 IO，其他都是同步 IO。

#### 阻塞 IO（blocking IO）

应用进程发起 IO 系统调用后，应用进程被阻塞，转到内核空间处理.等到整个IO处理完毕后返回,结束阻塞.

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220107155113537.png" alt="image-20220107155113537" style="zoom:60%;" />



#### IO 多路复用（IO multiplexing）

当用户进程发起了 select 调用，进程会被阻塞，当发现该 select 负责的 socket 有准备好的数据时才返回.用户可以在一个线程内同时处理多个 socket 的 IO 请求。用户可以注册多个 socket，然后不断地调用 select 读取被激活的 socket，即可达到在同一个线程内同时处理多个 IO 请求的目的。

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220107155248572.png" alt="image-20220107155248572" style="zoom:67%;" />

复用IO的基本思路就是通过slect或poll、epoll 来监控多fd ，来达到不必为每个fd创建一个对应的监控线程，从而减少线程资源创建的目的。

#### 为什么说阻塞 IO 和 IO 多路复用最为常用？

在这四种常用的 IO 模型中，应用最多的、系统内核与编程语言支持最为完善的，便是阻塞 IO 和 IO 多路复用。这两种 IO 模型，已经可以满足绝大多数网络 IO 的应用场景。

#### RPC 框架在网络通信上倾向选择哪种网络 IO 模型？

IO 多路复用更适合高并发的场景，可以用较少的进程（线程）处理较多的 socket 的 IO 请求，但使用难度比较高。

RPC 调用在大多数的情况下，是一个高并发调用的场景，考虑到系统内核的支持、编程语言的支持以及 IO 模型本身的特点，在 RPC 框架的实现中，在网络通信的处理上，我们会选择 IO 多路复用的方式。

### 什么是零拷贝？

所谓的零拷贝，就是取消用户空间与内核空间之间的数据拷贝操作，应用进程每一次的读写操作，可以通过一种方式，直接将数据写入内核或从内核中读取数据，再通过 DMA 将内核中的数据拷贝到网卡，或将网卡中的数据 copy 到内核。

零拷贝有两种解决方式，分别是  mmap+write  方式和  sendfile  方式

### Netty 中的零拷贝

netty基于 Reactor 模式实现的框架.零拷贝完全站在了用户空间上(JVM),偏向于数据操作的优化上

Netty 的零拷贝就是为了解决这个问题，在用户空间对数据操作进行优化。

- Netty 提供了 CompositeByteBuf 类，它可以将多个 ByteBuf 合并为一个逻辑上的  ByteBuf，避免了各个 ByteBuf 之间的拷贝。

- ByteBuf 支持 slice 操作，因此可以将 ByteBuf 分解为多个共享同一个存储区域的 ByteBuf，避免了内存的拷贝。

* 通过 wrap 操作，我们可以将 byte[] 数组、ByteBuf、ByteBuffer  等包装成一个 Netty ByteBuf 对象, 进而避免拷贝操作。

Netty  的  ByteBuffer 可以采用 Direct Buffers，使用堆外直接内存进行 Socketd 的读写操作



参考:

[100%弄明白5种IO模型](https://zhuanlan.zhihu.com/p/115912936)

[原来 8 张图，就可以搞懂「零拷贝」了](https://zhuanlan.zhihu.com/p/258513662)