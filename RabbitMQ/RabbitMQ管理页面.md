# RabbitMQ管理页面

## 概述 

一般后台管理页面端口号是,15672,然后默认用户名称和密码都是guest.

![RabbitMQ-Management-index](..\img\RabbitMQ-Management-index.png)

一般能看到六个标签栏:

- overview:看到MQ整体的情况,生产,消费,应答,ack数量,各个节点情况,包括mq的端口映射信息等
- Connections:这个选项卡显示MQ生产者和消费者的情况
- channels:展示通道信息
- Exchange:展示所有交换机信息
- Queue:展示所有队列信息
- Admin:展示用户信息

## Overview

整个overview细分了如下页面:

![img](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fb95396ef3df46ccb1188d977b7d1558~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

分别是：

**Totals：**

Totals 里面有 准备消费的消息数、待确认的消息数、消息总数以及消息的各种处理速率（发送速率、确认速率、写入硬盘速率等等）

![RabbitMQ-Management-overview-total](D:\笔记\img\RabbitMQ-Management-overview-total.png)

**Nodes：**

启动一个broker都会产生一个node。

![RabbitMQ-Management-overview-node](..\img\RabbitMQ-Management-overview-node.png)

**Churn statistics**

里边展示的是 Connection、Channel 以及 Queue 的创建/关闭速率

![img](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1c034eddc10443a286a45e2dfa8e5678~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

**Ports and contexts：**

这个里边展示了端口的映射信息以及 Web 的上下文信息。

 ![img](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0f30ee182e2046c8b9f049be1a8899c5~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

- 5672 是 RabbitMQ 通信端口
- 15672 是 Web 管理页面端口
- 25672 是集群通信端口

**Export definitions**&& **Import definitions**

最后面这两个可以导入导出当前实例的一些配置信息.

定义由用户，虚拟主机，权限，参数，交换，队列和绑定组成。 它们不包括队列的内容或集群名称。 独占队列不会被导出。

## Connections

这里主要展示的是当前连接上 RabbitMQ 的信息，无论是消息生产者还是消息消费者，只要连接上来了这里都会显示出来。

![RabbitMQ-Management-connections](..\img\RabbitMQ-Management-connections.png)

- User name：当前连接使用的用户名。
- State：当前连接的状态，running 表示运行中；idle 表示空闲。
- SSL/TLS：表示是否使用 ssl 进行连接。
- Channels：当前连接创建的通道总数。
- From client：每秒发出的数据包。
- To client：每秒收到的数据包。

点击连接名称可以查看每一个连接的详情。

在详情中可以查看每一个连接的通道数以及其他详细信息，也可以强制关闭一个连接。

## Channels

这个地方展示的是通道的信息

一个连接（IP）可以有多个通道,这个多个通道通过多线程实现，一般情况下，我们在通道中创建队列、交换机等。生产者的通道一般会立马关闭；消费者是一直监听的，通道几乎是会一直存在。

![RabbitMQ-Management-channel](D:\笔记\img\RabbitMQ-Management-channel.png)

- Channel：通道名称。
- User name：该通道登录使用的用户名。
- Model：通道确认模式，C 表示 confirm；T 表示事务。
- State：通道当前的状态，running 表示运行中；idle 表示空闲。
- Unconfirmed：待确认的消息总数。
- Prefetch：Prefetch 表示每个消费者最大的能承受的未确认消息数目，简单来说就是用来指定一个消费者一次可以从 RabbitMQ 中获取多少条消息并缓存在消费者中，一旦消费者的缓冲区满了，RabbitMQ 将会停止投递新的消息到该消费者中直到它发出有消息被 ack 了。总的来说，消费者负责不断处理消息，不断 ack，然后只要 unAcked 数少于 prefetch * consumer 数目，RabbitMQ 就不断将消息投递过去。
- Unacker：待 ack 的消息总数。
- publish：消息生产者发送消息的速率。
- confirm：消息生产者确认消息的速率。
- unroutable (drop)：表示未被接收，且已经删除了的消息。
- deliver/get：消息消费者获取消息的速率。
- ack：消息消费者 ack 消息的速率。

## Exchange

这个地方展示交换机信息：

![img](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a6d5da072ca84b96bd1c30c5b95f0ed1~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

Virtual host：所属的虚拟主机。
Name：名称。
Type：exchange type，具体的type可以查看[RabbitMq系列之一：基础概念](https://www.jianshu.com/p/5319b06f2e80)。
Features：功能。 可以是以下之一，或者不是：D: 持久化。T：Internal.I 表示这个交换机不可以被消息生产者用来推送消息，仅用来进行交换机和交换机之间的绑定。

Message rate in：消息进入的速率。
Message rate out：消息出去的速率。

## Queue

这个选项卡就是用来展示消息队列的：

![img](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/cb19366ff998479680b2846a4ba146b6~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

- Name：表示消息队列名称。
- Type：表示消息队列的类型，除了上图的 classic，另外还有一种消息类型是 Quorum。两个区别如下图：![img](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2ab6e641e85a4855b5f0cb794d87c861~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

- Features：表示消息队列的特性，
  - D 表示消息队列持久化。
  - DLX、DLK：表示设置给队列设置了死信交换机和死信消息的routing key
  - TTL：设置了消息过期时间
  - ad:自动删除,最后一个消费者断开时，该队列会被自动删除
  - excl:独占队列,只有创建它的连接有权访问，连接断开后，排他队列将自动删除
- State：表示当前队列的状态，running 表示运行中；idle 表示空闲。
- Ready：表示待消费的消息总数。
- Unacked：表示待应答的消息总数。
- Total：表示消息总数 Ready+Unacked。
- incoming：表示消息进入的速率。
- deliver/get：表示获取消息的速率。
- ack：表示消息应答的速率

## Admin

这里是做一些用户管理操作，如下图：

![img](https://img-blog.csdnimg.cn/img_convert/59e7595cb7be86197e1507a9e5edeb09.png)

Tags：角色标签，只能选取一个。
Can access virtual hosts：允许进入的vhost。
Has password：设置了密码

tags(原链接:[https://www.cnblogs.com/java-zhao/p/5670476.html](https://link.jianshu.com/?t=https%3A%2F%2Fwww.cnblogs.com%2Fjava-zhao%2Fp%2F5670476.html))

- administrator (超级管理员)
  可登陆管理控制台(启用management plugin的情况下)，可查看所有的信息，并且可以对用户，策略(policy)进行操作。
- monitoring(监控者)
  可登陆管理控制台(启用management plugin的情况下)，同时可以查看rabbitmq节点的相关信息(进程数，内存使用情况，磁盘使用情况等)
- policymaker(策略制定者)
  可登陆管理控制台(启用management plugin的情况下), 同时可以对policy进行管理。
- management(普通管理者)
  仅可登陆管理控制台(启用management plugin的情况下)，无法看到节点信息，也无法对策略进行管理。
- none(其他)
  无法登陆管理控制台，通常就是普通的生产者和消费者



参考:[RabbitMQ 管理页面该如何使用](https://juejin.cn/post/7033655802531414053)

[RabbitMQ——控制台介绍](https://blog.csdn.net/b15735105314/article/details/114650119)

