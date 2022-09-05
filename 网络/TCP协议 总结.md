# TCP协议 总结

相关知识点

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/2/23/1707236e03d22cbc~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)



## TCP 和 UDP 的区别？

**主要区别**

- **TCP是一个面向连接的、可靠的、基于字节流的传输层协议。**

- **UDP是一个面向无连接的传输层协议。**

TCP的三大核心特点

1. 面向连接:TCP 需要三次握手建立连接
2. 可靠的:TCP有状态且可控制.
   1. TCP 会精准记录哪些数据发送了，哪些数据被对方接收了，哪些没有被接收到，而且保证数据包按序到达，不允许半点差错。这是**有状态**。
   2. 当意识到丢包了或者网络环境不佳，TCP 会根据具体情况调整自己的行为，控制自己的发送速度或者重发。这是**可控制**。
3. 面向字节流

## TCP 三次握手

 TCP 的三次握手，需要确认双方的两样能力: `发送的能力`和`接收的能力`.

开始时双方都是`CLOSED`状态,然后服务端进入`LISTEN`监听状态,此时客户端向服务端发送SYN,序列号为x,自己变成SYN-SENT状态.

服务端接受到客户端的请求后,返回SYN,和ACK,序列号为y,ack=x+1保证安全性.自己变成SYN-RCVD状态

之后客户端再发送ACK,seq=x+1,ack=y+1,状态置为ESTABLISHED,服务端收到后也变更状态为ESTABLISHED.

连接建立完成后开始传输数据.

> 凡是需要对端确认的，一定消耗TCP报文的序列号。

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/2/23/170723de9b8aa08b~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

### 为什么不是两次？

根本原因: 无法确认客户端的接收能力。当客户端发送完,发现包未送达,再次重发,第一次发的包一直在网络中,当断开连接时到达服务端,服务端建立连接,但客户端已经下线,导致连接资源浪费.

只要能保证双发都有接受和发送的能力,那么无论几次握手都可以,3此握手时次数最少能解决该问题的方法.

### 三次握手过程中可以携带数据么？

第三次握手的时候，可以携带。前两次握手不能携带数据。

前两次握手如果能携带数据,容易被有心人攻击.

第三次握手的时候，客户端已经处于`ESTABLISHED`状态，并且已经能够确认服务器的接收、发送能力正常，这个时候相对安全了，可以携带数据。

### 同时打开会怎样？

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/2/23/170723e219f0a415~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

双方同时发送完SYN,同时变成SYN-SENT,接受到SYN后又同时变为SYN-REVD,之后发送完SYN+ACK后同事变为ESTABLISHED.

## TCP 四次挥手