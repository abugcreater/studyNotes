# Consul

## Consul是什么

consul是一种分布式,高可用的数据中心感知解决方案，用于跨动态分布式基础架构连接和配置应用程序。(摘自[github](https://github.com/hashicorp/consul))

> Consul is a distributed, highly available, and data center aware solution to connect and configure applications across dynamic, distributed infrastructure.

他具有了以下几种关键特性:

- **多数据中心**: consul集群由数据中心构成,他能支持任意数量的区域而不需要复杂的配置.
- **服务网格/服务分段**——Consul Connect 通过自动 TLS 加密和基于身份的授权实现安全的服务到服务通信。应用程序可以在服务网格配置中使用 sidecar 代理来为入站和出站连接建立 TLS 连接，而完全不知道 Connect。
- **服务发现**——Consul 使服务注册自己并通过 DNS 或 HTTP 接口发现其他服务变得简单。也可以注册 SaaS 提供商等外部服务。
- **健康检查**- 健康检查使 Consul 能够快速提醒操作员有关集群中的任何问题。与服务发现的集成可防止将流量路由到不健康的主机并启用服务级别断路器。
- **键/值存储**- 灵活的键/值存储可以存储动态配置、功能标记、协调、领导者选举等。简单的 HTTP API 使其在任何地方都易于使用。

## Consul的服务发现

节点通过Consul提供的api向consul中注册节点,在spring中最终通过调用,`AgentConsulClient.agentServiceRegister`方法注册.该方法实现

```java
public Response<Void> agentServiceRegister(NewService newService, String token) {
   UrlParameters tokenParam = token != null ? new SingleUrlParameters("token", token) : null;

   String json = GsonFactory.getGson().toJson(newService);
   HttpResponse httpResponse = rawClient.makePutRequest("/v1/agent/service/register", json, tokenParam);
	//省略不重要信息
   ....
}
```

实际是通过调用`/v1/agent/service/register`接口实现注册.

节点的注册信息,consul保存在注册的client节点,然后client节点再转发到server节点保存.

**一般服务发现提供了http和DNS两种注册方式.**

## Consul 健康检测如何实现

Consul做服务发现是专业的，健康检查是其中一项必不可少的功能，其提供Script/TCP/HTTP+Interval，以及TTL等多种方式。服务的健康检查由服务注册到的Agent来处理，这个Agent既可以是Client也可以是Server。

Agent可以配置成和Server的同级节点:

![img](https://blog.didispace.com/images/pasted-128.png)

一般我们使用的agent是在服务启动时的agent进程,和服务运行在同一个jvm中,非server同级节点

![img](https://blog.didispace.com/images/pasted-125.png)

**script是consul主动去检查服务的健康状况，ttl是服务主动向consul报告自己的健康状况。**

以下是几种配置方式:

1. **Script check（Script+ Interval）**

   执行外部应用进行健康检查:脚本按照指预置时间间隔(参数interval指定)来调用脚本执行，脚本执行超时时间由timeout参数配置.以下是执行`/usr/local/bin/check_mem.py`脚本的输出

   ```json
   {  
     "check": {  
       "id": "mem-util",  
       "name": "Memory utilization",  
       "script": "/usr/local/bin/check_mem.py",  
       "interval": "10s",  
       "timeout": "1s"  
     }  
   }
   ```

2. **HTTP check（HTTP+ Interval）**

   这种检查将按照预设的时间间隔创建一个HTTP “get”请求。2xx表示成功,429表示警告.下面是consul每个10s发送http get请求[http://localhost:5000/health](https://links.jianshu.com/go?to=http%3A%2F%2Flocalhost%3A5000%2Fhealth)，请求超时时间是1s，来检测服务健康状态。

   ```json
   {  
     "check": {  
       "id": "api",  
       "name": "HTTP API on port 5000",  
       "http": "http://localhost:5000/health",  
       "interval": "10s",  
       "timeout": "1s"  
     }  
   }
   ```

3. **TCP check(TCP + Interval)**

   将按照预设的时间间隔与指定的IP/Hostname和端口创建一个TCP连接。如果连接成功状态success,否则critical.

   ```json
   {  
     "check": {  
       "id": "ssh",  
       "name": "SSH TCP on port 22",  
       "tcp": "localhost:22",  
       "interval": "10s",  
       "timeout": "1s"  
     }  
   }
   ```

4. **TTL check:（Timeto Live生存时间）主动上报**

   这种checks为给定TTL保留了最后一种状态，checks的状态必须通过HTTP接口周期性更新，如果外部接口没有更新状态，那么状态就会被认定为不正常。

   这种机制，服务需要服务周期性汇报健康状态.

**节点注册后状态信息**

默认情况下，当checks注册到Consul agent时，健康状态立即被设置为“Critical”。可以防止服务直接被注册为通过（“passing”）状态，在进入service pool前认为是健康状态。在某些情况下，checks可能需要指定健康检查的初始状态，可以通过设置“status”字段来实现。

**节点健康状态变更**

- [`success_before_passing`](https://www.consul.io/docs/discovery/checks#success_before_passing)- 在检查状态转换为通过之前所需的连续成功结果数。默认为`0`. 在 Consul 1.7.0 中添加。
- [`failures_before_warning`](https://www.consul.io/docs/discovery/checks#failures_before_warning)- 检查状态转换为警告之前所需的连续不成功结果的数量。默认为与 的值相同的值， 以保持不将服务检查状态更改为之前`failures_before_critical`的预期行为，除非配置为这样做。高于的值无效。在 Consul 1.11.0 中添加。`warning``critical``failures_before_critical`
- [`failures_before_critical`](https://www.consul.io/docs/discovery/checks#failures_before_critical)- 在检查状态转换为关键之前所需的连续不成功结果的数量。默认为`0`. 在 Consul 1.7.0 中添加。

## 存储实现

Consul提供了一个易用的键/值存储,这可以用来 **保持动态配置** ,**协助服务协调**,**领袖选举**,做开发者可以想到的任何事情可以通过Http API `v1/kv/?{key}`来获取设置获取KEY/VALUE的存储与设置.
这些信息会被保存到`/consul/data/`文件夹下,在启动时指定`-data-dir`,可以恢复原有数据

## Consul 集群

![img](https://blog.didispace.com/images/pasted-125.png)

正常的consul集群结构如上图,一般由server节点和client节点组成.client节点主要用于健康检测和转发请求到server节点.client节点也会保存一部分注册信息,并将注册信息转发到server节点保存.server节点间会同步注册信息,但是节点的健康状态由注册到的agent节点维护.

**服务注册方式与调用流程**

- **http方式:**服务器Server6中Program D需要访问Service B，这时候Program D首先访问本机Consul Client提供的HTTP API，本机Client会将请求转发到Consul Server，Consul Server查询到Service B当前的信息返回，最终Program D拿到了Service B的所有部署的IP和端口，然后就可以选择Service B的其中一个部署并向其发起请求了。
- **DNS方式:**如果服务发现采用的是DNS方式，则Program D中直接使用Service B的服务发现域名，域名解析请求首先到达本机DNS代理，然后转发到本机Consul Client，本机Client会将请求转发到Consul Server，Consul Server查询到Service B当前的信息返回，最终Program D拿到了Service B的某个部署的IP和端口。

## Consul中的gossip协议

### gossip协议概述
gossip 协议（gossip protocol）又称 epidemic 协议（epidemic protocol），是基于流行病传播方式的节点或者进程之间信息交换的协议，在分布式系统中被广泛使用，比如我们可以使用 gossip 协议来确保网络中所有节点的数据一样。 gossip 协议利用一种随机的方式将信息传播到整个网络中，并在一定时间内使得系统内的所有节点数据一致。Gossip 其实是一种去中心化思路的分布式协议，解决状态在集群中的传播和状态一致性的保证两个问题。

Consul用了两种不同的Gossip池。把这两种池分别叫做LAN池和WAN池。

### LAN池

Consul中的每个数据中心有一个LAN池，它包含了这个数据中心的所有成员，包括clients和servers。LAN池用于以下几个目的:

- 成员关系信息允许client自动发现server, 减少了所需要的配置量。
- 分布式失败检测机制使得由整个集群来做失败检测这件事， 而不是集中到几台机器上。
- gossip池使得类似领导人选举这样的事件变得可靠而且迅速。

### WAN池

WAN池是全局唯一的，因为所有的server都应该加入到WAN池中，无论它位于哪个数据中心。由WAN池提供的成员关系信息允许server做一些跨数据中心的请求。一体化的失败检测机制允许Consul优雅地去处理：整个数据中心失去连接， 或者仅仅是别的数据中心的某一台失去了连接。

consul在gossip上的实现实际上是使用的memberlist库，也是自家公司提供的。 其实现了集群内节点发现、 节点失效探测、节点故障转移、节点状态同步等。 

### Consul中gossip 周期性任务

- 1、故障检测
- 2、状态合并（push/pull消息）
- 3、广播gossip消息





参考:[Consul之：服务健康监测 ](https://www.cnblogs.com/duanxz/p/9662862.html)

[使用Consul做服务发现的若干姿势](https://blog.didispace.com/consul-service-discovery-exp/)

[Consul运维笔记](https://liuhll.github.io/hexo-blog-deploy/2018/09/24/2018-09-consul/)

[Gossip算法](https://blog.csdn.net/chen77716/article/details/6275762)

[Gossip协议及Consul中的实现](https://zhuanlan.zhihu.com/p/369178156)