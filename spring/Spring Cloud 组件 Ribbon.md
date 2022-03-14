# Spring Cloud 组件 Ribbon

> 简介:ribbon 负责客户端的负债均衡

## Ribbon 核心组件

Ribbon 主要有五大功能组件：ServerList、Rule、Ping、ServerListFilter、ServerListUpdater。

#### 1 负载均衡器 LoadBalancer

用于管理负载均衡的组件。初始化的时候通过加载 YMAL 配置文件创建出来的。

#### 2 服务列表 ServerList

ServerList 主要用来获取所有服务的地址信息，并存到本地。

静态存储：从配置文件中获取服务节点列表并存储到本地。

动态存储：从注册中心获取服务节点列表并存储到本地

#### 3 服务列表过滤 ServerListFilter

将获取到的服务列表按照过滤规则过滤。

- 通过 Eureka 的分区规则对服务实例进行过滤。
- 比较服务实例的通信失败数和并发连接数来剔除不够健康的实例。
- 根据所属区域过滤出同区域的服务实例。

#### 4 服务列表更新 ServerListUpdater

服务列表更新就是 Ribbon 会从注册中心获取最新的注册表信息。是由这个接口 ServerListUpdater 定义的更新操作。而它有两个实现类，也就是有两种更新方式：

- 通过定时任务进行更新。由这个实现类 PollingServerListUpdater 做到的。
- 利用 Eureka 的事件监听器来更新。由这个实现类 EurekaNotificationServerListUpdater 做到的

#### 5 心跳检测 Ping

IPing 接口类用来检测哪些服务可用。如果不可用了，就剔除这些服务。

实现类主要有这几个：PingUrl、PingConstant、NoOpPing、DummyPing、NIWSDiscoveryPing。

心跳检测策略对象 IPingStrategy，默认实现是轮询检测。

#### 6 负载均衡策略 Rule

![image-20220117142259381](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117142259381.png)

类图:

![image-20220117142313308](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117142313308.png)

主要的负载均衡策略

- **线性轮询均衡** （RoundRobinRule）：轮流依次请求不同的服务器。优点是无需记录当前所有连接的状态，无状态调度。
- **可用服务过滤负载均衡**（AvailabilityFilteringRule）：过滤多次访问故障而处于断路器状态的服务，还有过滤并发连接数量超过阈值的服务，然后对剩余的服务列表按照轮询策略进行访问。默认情况下，如果最近三次连接均失败，则认为该服务实例断路。然后保持 30s 后进入回路关闭状态，如果此时仍然连接失败，那么等待进入关闭状态的时间会随着失败次数的增加呈指数级增长。
- **加权响应时间负载均衡**（WeightedResponseTimeRule）：为每个服务按响应时长自动分配权重，响应时间越长，权重越低，被选中的概率越低。
- **区域感知负载均衡**（ZoneAvoidanceRule）：更倾向于选择发出调用的服务所在的托管区域内的服务，降低延迟，节省成本。Spring Cloud Ribbon 中默认的策略。
- **重试负载均衡**（RetryRule)：通过轮询均衡策略选择一个服务器，如果请求失败或响应超时，可以选择重试当前服务节点，也可以选择其他节点。
- **高可用均衡**（BestAvailableRule)：忽略请求失败的服务器，尽量找并发比较低的服务器。注意：这种会给服务器集群带来成倍的压力。
- **随机负载均衡**（RandomRule）：随机选择服务器。适合并发比较大的场景。

##  Ribbon 拦截请求的原理

我们可以画一张原理图来梳理下 Ribbon 拦截请求的原理：

![image-20220117143604892](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117143604892.png)

拦截RestTemplate,将拦截器添加到RestTemplate 执行逻辑中,创建ILoadBalancer实例,包括负载均衡逻辑,服务列表等,Ribbon选择服务并转发.

## Ribbon 初始化的原理

已`@LoadBalanced `注解为突破口,剖析Ribbon源码

![image-20220117144935269](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117144935269.png)

## Ribbon 同步服务列表原理

Ribbon 首次从 Eureka 获取全量注册表后，就会隔一定时间获取注册表

![image-20220117145857774](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117145857774.png)

PollingServerListUpdater ，专门用来做定时同步的。

## Eureka 心跳检测的原理

- **Eureka 服务摘除机制**：不是一次性将服务实例摘除，每次最多随机摘除 15%。如果摘除不完，1 分钟之后再摘除。
- **Eureka 心跳机制**：每个服务每隔 30s 自动向 Eureka Server 发送一次心跳，Eureka Server 更新这个服务的最后心跳时间。如果 180 s 内（版本1.7.2）未收到心跳，则任务服务故障了。
- **Eureka 自我保护机制**：如果上一分钟实际的心跳次数，比我们期望的心跳次数要小，就会触发自我保护机制，不会摘除任何实例。期望的心跳次数：服务实例数量 * 2 * 0.85。

## Ribbon 心跳检测的原理

**Ribbon 不是通过每个服务向 Ribbon 发送心跳或者 Ribbon 给每个服务发送心跳来检测服务是否存活的**。

![image-20220117150223989](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117150223989.png)

核心代码

```java
public class ConsulPing implements IPing {
    public ConsulPing() {
    }

    public boolean isAlive(Server server) {
        boolean isAlive = true;
        if (server != null && server instanceof ConsulServer) {
            ConsulServer consulServer = (ConsulServer)server;
            return consulServer.isPassingChecks();
        } else {
            return isAlive;
        }
    }
}
```