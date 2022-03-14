# 01 | 核心原理：能否画张图解释下RPC的通信流程？

### 什么是 RPC？

RPC 的全称是 Remote Procedure Call，即远程过程调用.RPC是帮助我们屏蔽网络编程细节,实现调用远程方法跟本地一样的体验,而无需编写业务无关代码.

**RPC 的作用就是体现在这样两个方面：**

- 屏蔽远程调用跟本地调用的区别，让我们感觉就是调用项目内的方法；

- 隐藏底层网络通信的复杂性，让我们更专注于业务逻辑。

### RPC 通信流程

RPC默认采用TCP,需要传输的数据被序列化成二进制,序列化过程可逆.被传输的数据需要遵循协议格式,分为数据头和请求体,据头一般用于身份识别，包括协议标识、数据大小、请求类型、序列化类型等信息；消息体主要是请求的业务参数信息和扩展属性等。服务提供方,通过反序列化将消息体还原成对象.序列化和编解码通过动态代理实现.

![image-20220105150234303](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220105150234303.png)

### RPC 在架构中的位置

应用之间会通过 RPC 进行通信，可以说 RPC 对应的是整个分布式应用系统，就像是“经络”一样的存在。

**RPC 框架能够帮助我们解决系统拆分后的通信问题，并且能让我们像调用本地一样去调用远程方法。**

![RPC应用场景](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220105150442441.png)

参考:

[restful与RPC的区别](https://blog.csdn.net/dongcheng_2015/article/details/115798084)