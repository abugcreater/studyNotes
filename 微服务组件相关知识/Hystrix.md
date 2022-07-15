# Hystrix

## 1. 背景

在微服务调用链中,不能保证所有的系统都能无故障持续运行的,调用链中下游故障会导致上游资源耗尽,进而导致整个系统崩溃.

假设某个应用依赖30个微服务,每个微服务的可用性为99.99%,那么这30个微服务的整体可用性为99.99%^30约为99.7%,那么每个月可能存在2小时的不可用时间.

为了这几小时的不可用时间,我们必须考虑相关的容错即**弹力 (Resiliency)** 设计.

经典的容错设计主要有以下四种:

- **隔离(Bulkhead):**可以根据用户,用途对重要资源隔离,防止错误蔓延
- **断路器(circuit breaker):**检测故障,当达到阈值时打开断路器,之后一段时间内返回都是错误.之后校验是否调用成功,成功则恢复正常.遵循Fail Fast原则.
- **重试(Retry):**对于偶发错误可以考虑重试,但是需要考虑次数和间隔
- **降级(degradtion):**下游出现错误或无法提供服务时,返回默认值或缓存值等,保障服务的总体可用性.

## 2. Hystrix

Hystrix 是Netflix在2012年作为他们对分布式系统弹性设计的成果开源.在2019彻底停止维护.

Hystrix 具备如下特性:

- 在出现延迟和失败调用的时候保护服务，并提供控制
- 防止在复杂分布式系统中出现的连锁失败
- 快速失败，快速恢复
- 优雅降级
- 实时的监控、警报、配置

### 2.1 执行流程

Hystrix 的执行流程在 [Hystrix: How it Works](https://github.com/Netflix/Hystrix/wiki/How-it-Works) 上有很详细的说明.

主要流程为:

![img](https://blog-static.fintopia.tech/4d42370cec648cf4657a4fdcf0ddfa92.png)

**1. 构造`HystrixCommand`或`HystrixObservableCommand`对象**

Hystrix 中采用 [Command Pattern](https://en.wikipedia.org/wiki/Command_pattern)(命令模式) 来包装对下游依赖的请求。在 Command 的实现中包含对下游进行调用的逻辑，而每一次调用则会构建一个新的 Command 实例。

根据调用产生的结果是单个还是多个，用户可以选择是继承 `HystrixCommand` 还是 `HystrixObservableCommand`

**2. 执行命令**

Command有以下四种方式执行命令:

- `execute()`: 仅适用于 `HystrixCommand`，同步阻塞执行，并返回下游返回的响应或抛出异常，是 `queue().get()` 的 shortcut。
- `queue()`: 仅适用于 `HystrixCommand`，异步执行，返回 `Future` 对象，是 `toObservable().toBlocking().toFuture()` 的 shortcut。
- `observe()`: 订阅发送响应的 `Observable` 对象，并返回该 `Observable` 的镜像。(即时执行)
- `toObservable()`: 返回发送响应的 `Observable` 对象，仅当用户主动订阅后才开始执行具体逻辑并返回响应。(延时执行)

可以发现，其实四种执行方法都是基于 `Observable` 的实现。

**3. 判断是否有响应缓存**

如果有可用缓存则直接返回缓存中的值

**4. 判断断路器是否打开**

如果断路器打开,则会先尝试获取Fallback,如果是关闭状态则会继续执行

**5. 判断线程池 / 排队队列 / 信号量 是否已经被占满**

根据隔离策略选择其中之一,判断是否已满,获取Fallback放弃执行

**6. 进行实际调用**

触发具体的调用实现：`HystrixCommand.run()` 或 `HystrixObservableCommand.construct()`。如果调用超过了配置的超时时间，会抛出一个 `TimeoutException`，随后和抛出其他除了 HystrixBadRequestException 的异常一样进入获取 Fallback 的流程。

对于具体执行调用并由于超时等原因阻塞住的线程，Hystrix 只能够尝试进行打断。但是由于大多数的 Java HTTP client 并不会响应 InterruptedException，可能线程还是会继续阻塞直到连接超时，导致线程池占满。因此，用户最好在代码中对 InterruptedException 进行处理，以及为 http 客户端配置合理的超时时间.

如果调用正常执行 (没有出现超时或异常)，Hystrix 则在写日志、记录监控信息后返回。

**7. 计算线路健康程度**

根据新取得的监控信息，判断是否要打开或关闭断路器

**8. 获取Fallback**

当断路器打开或调用失败时,Hystrix均会尝试获取Fallbakc响应,通过`HystrixCommand.getFallback()` 或 `HystrixObservableCommand.resumeWithFallback()`.这本身是一种降级措施.

如果没有实现Fallback,或者在Fallback中抛出异常,则则 Hystrix 会返回直接发送 `onError` 通知的 `Observable` 实例.以下为`onError`时的行为:

- `execute()` - 抛出异常
- `queue()` - 返回 `Future`，但是在调用 `get()` 方法时会抛出异常
- `observe()` - 返回 `Observable` 实例，当被订阅时会马上结束并调用订阅者的 `onError` 方法
- `toObservable()` - 同上

**9. 通过 `Observable` 的方式，返回调用成功的响应。根据不同的调用方式，`Observable` 可能会被转换**

![img](https://blog-static.fintopia.tech/3d31c5c64235b89b9d41dc772d806a3e.png)

- `execute()` - 通过和 `queue()` 相同的方式获取 `Future` 对象，并通过调用 `get()` 来获取最底层的 `Observable` 发送的单个响应

- `queue()` - 将 `Observable` 转化为 `BlockingObservable` 以转化为 `Future` 并返回

- `observe()` - 订阅 `Observable` 使得执行马上开始，并返回一个在用户订阅后可以回放 `Observable` 的镜像 `Observable`

- `toObservable()` - 返回原样的 `Observable`，仅在用户进行 `subscribe` 之后才开始执行

### 2.2 主要组件

Hystrix 按照功能,可以分为以下几个模块,caching和collapser相对来说不那么重要.

![img](https://blog-static.fintopia.tech/2ed7728edb3adcd0c49e530dbf4b2e0c.jpeg)



#### 2.2.1 动态配置 DynamicProperties

Hystrix 中许多参数是基于配置的,比如超时时间,线程数量等.为了方便实时调整,Hystrix 支持动态修改.用户可以切换自己的动态配置源实现.一般有几个配置类,例如 `HystrixCommandProperties`, `HystrixThreadPoolProperties` 等.

以 `HystrixCommandProperties` 为例，其结构如图:

![img](https://blog-static.fintopia.tech/ae202c979288ccc9280428b17ae7fc98.jpeg)

在获取配置时，会返回 `HystrixProperty` 接口的实例，具体的实现类为 `ChainHystrixProperty`，底层为 `ChainProperty`，其中以类似链表的形式连接几个 `HystrixDynamicProperty`，对应同一个配置不同优先级的 key

#### 2.2.2 断路器 CircuitBreaker

在一段时间内,一定数量的请求失败和超时达到某个比例时会触发短路,之后在一段时间里的请求会直接失败.过后Hystrix会放行一个请求,检测是否能正常访问.如果还是失败则继续打开断路器.如果成功则会放行所有请求.

在代码中，断路器会实现 `HystrixCircuitBreaker` 接口.

主要接口代码如下 :

```java
public interface HystrixCircuitBreaker {

    // 是否允许执行
    boolean allowRequest();

    // 断路器是否处在打开状态
    boolean isOpen();

    // 在半打开状态，表示测试请求调用成功
    void markSuccess();

    // 在半打开状态，表示测试请求调用失败
    void markNonSuccess();

    // 在 Command 开始执行时调用
    boolean attemptExecution();
}
```

##### 2.2.2.1 默认实现 Default Implementation

断路器接口的默认实现类是`HystrixCircuitBreakerImpl`.如果用户有特殊需求可以自己定义相关类.Command 只通过外部暴露的 `allowRequest` 和 `isOpen` 等部分方法和断路器实例交互，内部可以实现多种多样的逻辑.

在默认断路器实现下有三种断路器状态: `CLOSED`, `OPEN` 和 `HALF_OPEN`

- `CLOSED`:表示断路器关闭,请求能正常访问
- `OPEN` :表示断路器打开,一定时间内请求默认失败
- `HALF_OPEN`:断路器半关闭,会放行一个请求,根据请求返回状态,修改断路器状态

 Command 有的统计类 `HystrixCommandMetrics`，它们都是 `HystrixMetrics` 的实现类，机制非常相似.它会这个统计类,进行动态的状态调整.

```java
public boolean isOpen() {
    if (circuitOpen.get()) {
        return true;
    }
    HealthCounts health = metrics.getHealthCounts();
    if (health.getTotalRequests() < properties.circuitBreakerRequestVolumeThreshold().get()) {
        return false;
    }

    if (health.getErrorPercentage() < properties.circuitBreakerErrorThresholdPercentage().get()) {
        return false;
    } else {
        if (circuitOpen.compareAndSet(false, true)) {
            circuitOpenedOrLastTestedTime.set(System.currentTimeMillis());
            return true;
        } else {
            return true;
        }
    }
}
```

#### 2.2.3 隔离机制 Isolation

##### 2.2.3.1 隔离级别

Hystrix 使用隔离提供容错.是把系统依赖但是却互相不相关的服务调用所使用到的资源隔离开，这样在一个下游服务故障的时候，不会导致整个服务的资源都被占据.

Hystrix 为不同的应用场景提供两种隔离级别：`Thread` 和 `Semaphore`

**线程隔离**

`Thread` 隔离级别很好理解，就是让调用在另外的线程中执行，并且相关的调用都使用一个线程池中的线程.

![img](https://blog-static.fintopia.tech/783b76eb1a15681f00aff65e5fa2f0fe.png)

这样做的好处在于:

- 对委托线程来说，能够随时在出现超时调用时 walk away，执行 fallback 的逻辑，不会阻塞到连接超时从而拖累服务的响应时间。
- 对隔离效果来说，当下游服务出现超时故障时，仅仅该线程池会爆满，对使用其它线程池的不相关服务以及服务本身没有任何影响。当下游恢复健康后，线程池会再次变得可用，恢复往常状态。
- 对监控来说，由于线程池有多种监控数据，例如占用线程数、排队请求数、执行任务数等，当我们错误地配置了客户端或是下游服务出现性能变化我们都能够第一时间感知到并做出应对。
- 对项目来说，相当于引入了一个小型并发模块，可以在使用同步客户端的情况下方便构建异步系统 (Netflix API 就是这么做的)

线程池大小设置公式:

> **requests per second at peak when healthy × 99th percentile latency in seconds + some breathing room** 
>  **峰值 qps \* P99 响应时间 + 适当数量的额外缓冲线程**

**信号量隔离**

如果你的接口响应时间非常小，无法接受线程隔离带来的开销，且信任该接口能够很快返回的话，则可以使用 `Semaphore` 隔离级别.信号量隔离无法像线程隔离一样在超时时直接返回,而是需要等到客户端阻塞结束.

只需要将`execution.isolation.strategy`配置为 `SEMAPHORE` 即可,就能变更隔离级别.

##### 2.2.3.2 具体实现

`AbstractCommand`在初始化时,会调用`initThreadPool(threadPool, this.threadPoolKey, threadPoolPropertiesDefaults)`方法初始化线程池.具体方法是调用`HystrixThreadPool.Factory` 工厂类传入自身的 ThreadPoolKey,创建对于的线程池.

```java
static HystrixThreadPool getInstance(HystrixThreadPoolKey threadPoolKey, HystrixThreadPoolProperties.Setter propertiesBuilder) {
    
    String key = threadPoolKey.name();

    // 若已创建过，则返回缓存的线程池
    HystrixThreadPool previouslyCached = threadPools.get(key);
    if (previouslyCached != null) {
        return previouslyCached;
    }

    // 尚未创建，进入同步块进行创建。这里的同步块主要是防止重复创建线程池
    synchronized (HystrixThreadPool.class) {
        if (!threadPools.containsKey(key)) {
            threadPools.put(key, new HystrixThreadPoolDefault(threadPoolKey, propertiesBuilder));
        }
    }
    return threadPools.get(key);
}
```

`HystrixThreadPool` 是一个简单的接口，有个获取 `ExecutorService` 的方法、rxJava 用到的 Scheduler 的方法另外就是一些记录 metrics 的方法。具体的实现类是 `HystrixThreadPoolDefault`。

```java
  public HystrixThreadPoolDefault(HystrixThreadPoolKey threadPoolKey, HystrixThreadPoolProperties.Setter propertiesDefaults) {
        this.properties = HystrixPropertiesFactory.getThreadPoolProperties(threadPoolKey, propertiesDefaults);
        HystrixConcurrencyStrategy concurrencyStrategy = HystrixPlugins.getInstance().getConcurrencyStrategy();
        this.queueSize = properties.maxQueueSize().get();

        this.metrics = HystrixThreadPoolMetrics.getInstance(threadPoolKey,
                // 通过 concurrencyStrategy 创建线程池，这是用户可扩展的插件
                concurrencyStrategy.getThreadPool(threadPoolKey, properties),
                properties);
        this.threadPool = this.metrics.getThreadPool();
        this.queue = this.threadPool.getQueue();

        /* strategy: HystrixMetricsPublisherThreadPool */
        HystrixMetricsPublisherFactory.createOrRetrievePublisherForThreadPool(threadPoolKey, this.metrics, this.properties);
    }
```

这里简单说一下 `HystrixConcurrencyStrategy`。它是负责控制并发相关策略的插件，可以在 `HystrixPlugins` 中进行配置成自主实现的子类，默认系统实现为 `HystrixConcurrencyStrategyDefault`.该类中可重写的方法如下:

```java
// 控制如何根据配置创建线程池
ThreadPoolExecutor getThreadPool(final HystrixThreadPoolKey threadPoolKey, HystrixProperty<Integer> corePoolSize, HystrixProperty<Integer> maximumPoolSize, HystrixProperty<Integer> keepAliveTime, TimeUnit unit, BlockingQueue<Runnable> workQueue);
ThreadPoolExecutor getThreadPool(final HystrixThreadPoolKey threadPoolKey, HystrixThreadPoolProperties threadPoolProperties);
// 控制如何创建阻塞队列
BlockingQueue<Runnable> getBlockingQueue(int maxQueueSize);
// 控制是否对 Callable 进行处理，例如存储上下文信息
<T> Callable<T> wrapCallable(Callable<T> callable);
// 控制获取请求上下文
<T> HystrixRequestVariable<T> getRequestVariable(final HystrixRequestVariableLifecycle<T> rv);
```

`HystrixThreadPoolMetrics` 则会以基于配置的滑动窗口 + 桶的形式统计线程池的相关监控信息，并输出到用户配置的 HystrixMetricsPublisher 中。这里先略过，因为线程池的 metrics 只是一部分，后面还有其他的 metrics，它们的统计和输出方式都是相似的，可以后面一起说。

信号量的对应接口为 `TryableSemaphore`，接口定义也很简单:

```java
interface TryableSemaphore {
    // 尝试获取信号量，若返回 true 为成功获取
    public abstract boolean tryAcquire();
    // 释放已经获得的信号量
    public abstract void release();
    // 获取已经占用的数量
    public abstract int getNumberOfPermitsUsed();
}
```

它有两个实现类，一个是在 Command 使用信号量作为隔离级别是使用的 `TryableSemaphoreActual`，还有一个是在 Command 使用线程作为隔离级别时使用的不作任何限制的 `TryableSemaphoreNoOp`

#### 2.2.4 运行数据 Metrics

在 `HystrixCommand` 和 `HystrixObservableCommand` 运行期间，会产生许多运行数据如延迟、结果、排队时间等.这些产生的 Metrics 在内存中存储一定时间，便于进行查询和导出。

![img](https://blog-static.fintopia.tech/3061c0e119b026250388dcb9447a2a88.png)

Metrics 在顶层根据类别分为了三个类，用于更新以及获取监控数据。包括 `HystrixCommandMetrics`, `Hystrix ThreadPoolMetrics` 以及 `HystrixCollapserMetrics` 

它们底层实现相似，使用桶结构存放单位时间内的数据，并保存一定时间窗口内的桶，以此提供滑动时间窗口内的 metrics 统计。每过一个单位时间，会删除最老的桶，添加最新完成统计的桶，并新开一个桶统计下一个单位时间的数据.

![img](https://blog-static.fintopia.tech/0e79de4891c1ee11706d10d3fee7e418.jpeg)

只需要实现 Hystrix 提供的 `HystrixMetricsPublisher` 即可按照自己的需求将运行数据导出

```java
public abstract class HystrixMetricsPublisher {
  // 返回用于上报 CommandMetrics 的 HystrixMetricsPublisherCommand
  HystrixMetricsPublisherCommand getMetricsPublisherForCommand(HystrixCommandKey commandKey, HystrixCommandGroupKey commandGroupKey, HystrixCommandMetrics metrics, HystrixCircuitBreaker circuitBreaker, HystrixCommandProperties properties);
  // 返回用于上报 ThreadPoolMetrics 的 HystrixMetricsPublisherThreadPool
  HystrixMetricsPublisherThreadPool getMetricsPublisherForThreadPool(HystrixThreadPoolKey threadPoolKey, HystrixThreadPoolMetrics metrics, HystrixThreadPoolProperties properties);
  // 返回用于上报 CollapserMetrics 的 HystrixMetricsPublisherCollapser
  HystrixMetricsPublisherCollapser getMetricsPublisherForCollapser(HystrixCollapserKey collapserKey, HystrixCollapserMetrics metrics, HystrixCollapserProperties properties);
}
```

### 2.3 使用方法

#### 2.3.1 直接使用

用户可以通过实现 `HystrixCommand` 或者 `HystrixObservableCommand` 并进行配置使用，最原始不过也很灵活。

```java
public class CommandHelloWorld extends HystrixCommand<String> {

    private final String name;

    public CommandHelloWorld(String name) {
        super(HystrixCommandGroupKey.Factory.asKey("ExampleGroup"));
        this.name = name;
    }

    @Override
    protected String run() {
        return "Hello " + name + "!";
    }
}

String s = new CommandHelloWorld("Bob").execute();
Future<String> s = new CommandHelloWorld("Bob").queue();
Observable<String> s = new CommandHelloWorld("Bob").observe();
```

#### 2.3.2 和 Feign 结合使用

feign 运行时生成对应的 http 调用代码，底层可以指定不同的客户端以及 load balancer 等

通过 [HystrixFeign](https://github.com/OpenFeign/feign/blob/master/hystrix) 模块，能够将具体的调用嵌套在 Hystrix command 中。以下是使用 HystrixFeign 构建具备 Hystrix 保护的客户端，并配置 command key、fallback 的使用样例:

```java
GitHub github = HystrixFeign.builder()
        .setterFactory(commandKeyIsRequestLine)
        .target(GitHub.class, "https://api.github.com", fallback);
```

#### 2.3.3 在 Spring Boot 中集成使用

通过在项目中引入依赖 `spring-cloud-starter-hystrix`，就能够方便地在 Bean 中使用 `@HystrixCommand` 注解令方法在 Hystrix 下执行

```java
@Service
public class GreetingService {
    @HystrixCommand(fallbackMethod = "defaultGreeting")
    public String getGreeting(String username) {
        return new RestTemplate()
          .getForObject("http://localhost:9090/greeting/{username}", 
          String.class, username);
    }
 
    private String defaultGreeting(String username) {
        return "Hello User!";
    }
}
```





参考:[Hystrix 源码分析及实践](https://blog.fintopia.tech/60868c70ce7094706059f126/)

[How-it-Works](https://github.com/Netflix/Hystrix/wiki/How-it-Works)