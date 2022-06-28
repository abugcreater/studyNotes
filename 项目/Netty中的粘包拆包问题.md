# Netty中的粘包/拆包问题

## 1. TCP粘包/拆包

TCP是个“流”协议，所谓流，就是没有界限的一串数据。大家可以想想河里的流水，是连成一片的，其间并没有分界线。TCP底层并不了解上层业务数据的具体含义，它会根据TCP缓冲区的实际情况进行包的划分，所以在业务上认为，一个完整的包可能会被TCP拆分成多个包进行发送，也有可能把多个小的包封装成一个大的数据包发送，这就是所谓的TCP粘包和拆包问题。

>  TCP中的粘包/拆包主要指的是应用层处理过程中,而不是TCP本身传输的问题

**为什么UDP没有粘包?**

粘包拆包问题在数据链路层、网络层以及传输层都有可能发生。日常的网络应用开发大都在传输层进行，由于UDP有消息保护边界，不会发生粘包拆包问题，因此粘包拆包问题只发生在TCP协议中。

**粘包拆包发生场景**

如果一次请求发送的数据量比较小，没达到缓冲区大小，TCP则会将多个请求合并为同一个请求进行发送，这就形成了粘包问题。

如果一次请求发送的数据量比较大，超过了缓冲区大小，TCP就会将其拆分为多次发送，这就是拆包

![img](https://ask.qcloudimg.com/http-save/yehe-1161110/2h3b8tn5s3.jpeg?imageView2/2/w/1620)

上图中演示了以下几种情况：

- 正常的理想情况，两个包恰好满足TCP缓冲区的大小或达到TCP等待时长，分别发送两个包；
- 粘包：两个包较小，间隔时间短，发生粘包，合并成一个包发送；
- 拆包：一个包过大，超过缓存区大小，拆分成两个或多个包发送；
- 拆包和粘包：Packet1过大，进行了拆包处理，而拆出去的一部分又与Packet2进行粘包处理。

**粘包、拆包解决办法**

1、发送端给每个数据包添加包首部，首部中应该至少包含数据包的长度，这样接收端在接收到数据后，通过读取包首部的长度字段，便知道每一个数据包的实际长度了。

2、发送端将每个数据包封装为固定长度（不够的可以通过补0填充），这样接收端每次从接收缓冲区中读取固定长度的数据就自然而然的把每个数据包拆分开来。

3、可以在数据包之间设置边界，如添加特殊符号，这样，接收端通过这个边界就可以将不同的数据包拆分开。



## 2. Netty中的解决方案

Netty对解决粘包和拆包的方案做了抽象，提供了一些解码器（Decoder）来解决粘包和拆包的问题。如：

- LineBasedFrameDecoder：以行为单位进行数据包的解码；
- DelimiterBasedFrameDecoder：以特殊的符号作为分隔来进行数据包的解码；
- FixedLengthFrameDecoder：以固定长度进行数据包的解码；
- LenghtFieldBasedFrameDecode：适用于消息头包含消息长度的协议（最常用）；

实际使用:

```java
.handler(new ChannelInitializer() {
                        @Override
                        public void initChannel(Channel ch)
                                throws Exception {
                            //新增解码器
                            ch.pipeline().addLast(new LineBasedFrameDecoder(1024));
                            ch.pipeline().addLast(new StringDecoder());
                            ch.pipeline().addLast(new TimeClientHandler());
                        }
                    }
```

**LineBasedFrameDecoder和StringDecoder的原理分析**

LineBasedFrameDecoder的工作原理是它依次遍历ByteBuf中的可读字节，判断看是否有“\n”或者“\r\n”，如果有，就以此位置为结束位置，从可读索引到结束位置区间的字节就组成了一行。它是以换行符为结束标志的解码器，支持携带结束符或者不携带结束符两种解码方式，同时支持配置单行的最大长度。如果连续读取到最大长度后仍然没有发现换行符，就会抛出异常，同时忽略掉之前读到的异常码流。

StringDecoder的功能非常简单，就是将接收到的对象转换成字符串，然后继续调用后面的handler。LineBasedFrameDecoder + StringDecoder组合就是按行切换的文本解码器，它被设计用来支持TCP的粘包和拆包。





参考:[TCP粘包，拆包及解决方法 ](https://www.cnblogs.com/panchanggui/p/9518735.html)

[TCP粘包/拆包问题 ](https://www.cnblogs.com/wade-luffy/p/6165671.html)

[聊聊TCP的粘包、拆包以及解决方案](https://cloud.tencent.com/developer/article/1804413)

[如何获取一个完整的网络包](https://learn.lianglianglee.com/%E4%B8%93%E6%A0%8F/Netty%20%E6%A0%B8%E5%BF%83%E5%8E%9F%E7%90%86%E5%89%96%E6%9E%90%E4%B8%8E%20RPC%20%E5%AE%9E%E8%B7%B5-%E5%AE%8C/06%20%20%E7%B2%98%E5%8C%85%E6%8B%86%E5%8C%85%E9%97%AE%E9%A2%98%EF%BC%9A%E5%A6%82%E4%BD%95%E8%8E%B7%E5%8F%96%E4%B8%80%E4%B8%AA%E5%AE%8C%E6%95%B4%E7%9A%84%E7%BD%91%E7%BB%9C%E5%8C%85%EF%BC%9F.md)