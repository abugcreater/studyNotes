# Seata AT 模式启动源码分析

## 客户端启动逻辑

TM 是负责整个全局事务的管理器，因此一个全局事务是由 TM 开启的，TM 有个全局管理类 GlobalTransaction，结构如下：

```java
public interface GlobalTransaction {

    /**
     * Begin a new global transaction with default timeout and name.
     *
     * @throws TransactionException Any exception that fails this will be wrapped with TransactionException and thrown
     * out.
     */
    void begin() throws TransactionException;

    /**
     * Begin a new global transaction with given timeout and default name.
     *
     */
    void begin(int timeout) throws TransactionException;

    /**
     * Begin a new global transaction with given timeout and given name.
     *
     */
    void begin(int timeout, String name) throws TransactionException;

    /**
     * 提交全局事务
     *
     */
    void commit() throws TransactionException;

    /**
     * 回滚全局事务
     *
     */
    void rollback() throws TransactionException;

    /**
     *向 TC 询问相应全局事务的当前状态。
     *
     */
    GlobalStatus getStatus() throws TransactionException;

    /**
     * Get XID.
     */
    String getXid();

    /**
     * 报告全局事务状态.
     */
    void globalReport(GlobalStatus globalStatus) throws TransactionException;

}
```

可以通过GlobalTransactionContext创建GlobalTransaction,然后通过GlobalTransaction进行全局事务的开启,提交,回滚等操作.类型JDBC中的操作.

```java
//init seata;
TMClient.init(applicationId, txServiceGroup);
RMClient.init(applicationId, txServiceGroup);
//trx
GlobalTransaction tx = GlobalTransactionContext.getCurrentOrCreate();
try {
  tx.begin(60000, "testBiz");
  // 事务处理
  // ...
  tx.commit();
} catch (Exception exx) {
  tx.rollback();
  throw exx;
}
```

或者利用spring AOP的特性,用模块模式把这些冗余代码封装模版里.只需要在开启全局事务的方法上加上`@GlobalTransactional`注解.

Seata 的 Spring 模块中，有个 GlobalTransactionScanner，它的继承关系如下：

```java
public class GlobalTransactionScanner extends AbstractAutoProxyCreator implements InitializingBean, ApplicationContextAware, DisposableBean {
  // ...
}
```

下图标识了初始化bean时对注解了GlobalTransactional或GlobalLock的bean对象进行包装

![图片](https://mmbiz.qpic.cn/mmbiz_png/4MfwdbrwE6dOHthqqIf9UEqgYZpOerdFb0iaPTYswVSeNz8XEV9ib8RicCqlQrbKFzfZKdHt3Su0ZPKOaLSEQS0bw/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)



InitializingBean 的 afterPropertiesSet() 方法调用了 initClient() 方法：

`AbstractAutowireCapableBeanFactory#initializeBean-> AbstractAutowireCapableBeanFactory#invokeInitMethods -> ((InitializingBean) bean).afterPropertiesSet() -> 
io.seata.spring.annotation.GlobalTransactionScanner#initClient`

```java
//对 TM 和 RM 做了初始化操作
private void initClient() {
	
    //init TM
    TMClient.init(applicationId, txServiceGroup);
 
    //init RM
    RMClient.init(applicationId, txServiceGroup);

    registerSpringShutdownHook();

}
```

### **TM 初始化**

io.seata.tm.TMClient#init

```java
public static void init(String applicationId, String transactionServiceGroup) {
    	//获取tmRpcClient实例
        TmRpcClient tmRpcClient = TmRpcClient.getInstance(applicationId, transactionServiceGroup);
    	//初始化tmRpcClient实例
        tmRpcClient.init();
    }
```

调用 TmRpcClient.getInstance() 方法会获取一个 TM 客户端实例

```java
//使用单例模式 double check,初始化单例
public static TmRpcClient getInstance() {
    if (null == instance) {
        synchronized (TmRpcClient.class) {
            if (null == instance) {
                //获取Netty客户端配置
                NettyClientConfig nettyClientConfig = new NettyClientConfig();
                //创建messageExecutor线程池,用于处理各种与服务端消息的交互,在创建 TmRpcClient 实例时，创建 ClientBootstrap，用于管理 Netty 服务的启停，以及 ClientChannelManager
                final ThreadPoolExecutor messageExecutor = new ThreadPoolExecutor(
                    nettyClientConfig.getClientWorkerThreads(), nettyClientConfig.getClientWorkerThreads(),
                    KEEP_ALIVE_TIME, TimeUnit.SECONDS,
                    new LinkedBlockingQueue<>(MAX_QUEUE_SIZE),
                    new NamedThreadFactory(nettyClientConfig.getTmDispatchThreadPrefix(),
                        nettyClientConfig.getClientWorkerThreads()),
                    RejectedPolicies.runsOldestTaskPolicy());
                instance = new TmRpcClient(nettyClientConfig, null, messageExecutor);
            }
        }
    }
    return instance;
}
```

TMClient 继承了AbstractRpcRemotingClient,初始化时触发了 `io.seata.core.rpc.netty.AbstractRpcRemotingClient#init`方法

```java
public void init() {
    clientBootstrap.start();
    // 定时尝试连接服务端
    timerExecutor.scheduleAtFixedRate(new Runnable() {
        @Override
        public void run() {
            clientChannelManager.reconnect(getTransactionServiceGroup());
        }
    }, SCHEDULE_INTERVAL_MILLS, SCHEDULE_INTERVAL_MILLS, TimeUnit.SECONDS);
    mergeSendExecutorService = new ThreadPoolExecutor(MAX_MERGE_SEND_THREAD,
        MAX_MERGE_SEND_THREAD,
        KEEP_ALIVE_TIME, TimeUnit.MILLISECONDS,
        new LinkedBlockingQueue<>(),
        new NamedThreadFactory(getThreadPrefix(), MAX_MERGE_SEND_THREAD));
    mergeSendExecutorService.submit(new MergedSendRunnable());
    super.init();
}
```

调用 TM 客户端 init() 方法，最终会启动 netty 客户端(在对象池被调用时真正启动);开启一个定时任务,定时重新发送RegisterTMRequest请求尝试连接服务端

具体逻辑是NettyClientChannelManager#reconnect方法,NettyClientChannelManager缓存了客户端channel,如果此时channels不存在获取已过期,那么就会尝试连接服务端以重新获取channel并将其缓存到channels中;

AbstractRpcRemotingClient继承了AbstractRpcRemoting,init方法会触发`io.seata.core.rpc.netty.AbstractRpcRemoting#init`

```java
public void init() {
  timerExecutor.scheduleAtFixedRate(new Runnable() {
    @Override
    public void run() {
      for (Map.Entry<Integer, MessageFuture> entry : futures.entrySet()) {
        if (entry.getValue().isTimeout()) {
          futures.remove(entry.getKey());
          entry.getValue().setResultMessage(null);
        }
      }
      nowMills = System.currentTimeMillis();
    }
  }, TIMEOUT_CHECK_INTERNAL, TIMEOUT_CHECK_INTERNAL, TimeUnit.MILLISECONDS);
}
```

在AbstractRpcRemoting的init方法中,又开启了一个定时任务,主要用于定时清除已过期的futrue,futures是保存发送请求需要返回结果的future对象,该对象有个超时时间,超时后会自动抛异常.

### RM 初始化

io.seata.rm.RMClient#init

```java
public static void init(String applicationId, String transactionServiceGroup) {
  RmRpcClient rmRpcClient = RmRpcClient.getInstance(applicationId, transactionServiceGroup);
  rmRpcClient.setResourceManager(DefaultResourceManager.get());
  rmRpcClient.setClientMessageListener(new RmMessageListener(DefaultRMHandler.get()));
  rmRpcClient.init();
}
```

- RmRpcClient.getInstance处理逻辑与TM大致相同;

- ResourceManager是RM资源管理器,负载分支事务的注册,提交,上报以及回滚操作,以及全局锁的查询操作.DefaultResourceManager会持有当前所有的RM资源管理器,进行统一调用处理,而get()方法主要是加载当前的资源管理器,主要用了类似SPI的机制,进行灵活加载.Seata会扫描 META-INF/services/ 目录下的配置类并进行动态加载
- ClientMessageListener是RM消息处理监听器,用于负责处理从TC发送过来的指令,并对分支进行分支提交、分支回滚，以及 undo log 文件删除操作；最后init方法跟TM逻辑也答题一致;DefaultRMHandler封装了RM分支事务的一些具体操作逻辑.

### wrapIfNecessary逻辑处理

GlobalTransactionScanner继承了AbstractAutoProxyCreator,重写了**wrapIfNecessary方法**.

io.seata.spring.annotation.GlobalTransactionScanner#wrapIfNecessary

```java
protected Object wrapIfNecessary(Object bean, String beanName, Object cacheKey) {
    //判断是否开启了全局事务
    if (disableGlobalTransaction) {
        return bean;
    }
  
        synchronized (PROXYED_SET) {
            //如果该bean已经被代理了,则直接返回
            if (PROXYED_SET.contains(beanName)) {
                return bean;
            }
            interceptor = null;
            //check TCC proxy
            if (TCCBeanParserUtils.isTccAutoProxy(bean, beanName, applicationContext)) {
                //TCC interceptor， proxy bean of sofa:reference/dubbo:reference, and LocalTCC
                interceptor = new TccActionInterceptor(TCCBeanParserUtils.getRemotingDesc(beanName));
            } else {
                Class<?> serviceInterface = SpringProxyUtils.findTargetClass(bean);
                Class<?>[] interfacesIfJdk = SpringProxyUtils.findInterfaces(bean);
				//判断 bean 中是否有 GlobalTransactional 和 GlobalLock 注解
                if (!existsAnnotation(new Class[]{serviceInterface})
                    && !existsAnnotation(interfacesIfJdk)) {
                    return bean;
                }

                if (interceptor == null) {
                    interceptor = new GlobalTransactionalInterceptor(failureHandlerHook);
                }
            }


            if (!AopUtils.isAopProxy(bean)) {
                bean = super.wrapIfNecessary(bean, beanName, cacheKey);
            } else {
                AdvisedSupport advised = SpringProxyUtils.getAdvisedSupport(bean);
                //包装目标对象到代理对象
                Advisor[] advisor = buildAdvisors(beanName, getAdvicesAndAdvisorsForBean(null, null, null));
                for (Advisor avr : advisor) {
                    advised.addAdvisor(0, avr);
                }
            }
            PROXYED_SET.add(beanName);
            return bean;
        }
}
```

GlobalTransactionalInterceptor 实现了 MethodInterceptor：

io.seata.spring.annotation.GlobalTransactionalInterceptor#invoke

```java
public Object invoke(final MethodInvocation methodInvocation) throws Throwable {
    Class<?> targetClass = (methodInvocation.getThis() != null ? AopUtils.getTargetClass(methodInvocation.getThis()) : null);
    Method specificMethod = ClassUtils.getMostSpecificMethod(methodInvocation.getMethod(), targetClass);
    final Method method = BridgeMethodResolver.findBridgedMethod(specificMethod);

    final GlobalTransactional globalTransactionalAnnotation = getAnnotation(method, GlobalTransactional.class);
    final GlobalLock globalLockAnnotation = getAnnotation(method, GlobalLock.class);
    //如果有@GlobalTransactional注解
    if (globalTransactionalAnnotation != null) {
        return handleGlobalTransaction(methodInvocation, globalTransactionalAnnotation);
        //如果有@GlobalLock注解
    } else if (globalLockAnnotation != null) {
        return handleGlobalLock(methodInvocation);
    } else {
        return methodInvocation.proceed();
    }
}
```

其中 handleGlobalTransaction() 方法里面调用了 TransactionalTemplate 模版：

io.seata.spring.annotation.GlobalTransactionalInterceptor#handleGlobalTransaction

```java
private Object handleGlobalTransaction(final MethodInvocation methodInvocation,
                                       final GlobalTransactional globalTrxAnno) throws Throwable {
  try {
    return transactionalTemplate.execute(new TransactionalExecutor() {
      @Override
      public Object execute() throws Throwable {
        return methodInvocation.proceed();
      }
      @Override
      public TransactionInfo getTransactionInfo() {
        // ...
      }
    });
  } catch (TransactionalExecutor.ExecutionException e) {
    // ...
  }
}
```
handleGlobalTransaction() 方法执行了就是 TransactionalTemplate 模版类的 execute 方法：

io.seata.tm.api.TransactionalTemplate#execute
```java
public Object execute(TransactionalExecutor business) throws Throwable {
    // 1. get or create a transaction
    //如果上游有xid传过来说明自己是下游,直接参与这个全局事务,角色是 Participant
    //如果上游没有xid传递过来,说明自己是发起方,新开启一个全局事务,角色是Launcher
    GlobalTransaction tx = GlobalTransactionContext.getCurrentOrCreate();

    // 1.1 get transactionInfo
    TransactionInfo txInfo = business.getTransactionInfo();
    if (txInfo == null) {
        throw new ShouldNeverHappenException("transactionInfo does not exist");
    }
    try {

        // 2. begin transaction
        beginTransaction(txInfo, tx);

        Object rs = null;
        try {

            // Do Your Business
            rs = business.execute();

        } catch (Throwable ex) {

            // 3.the needed business exception to rollback.
            completeTransactionAfterThrowing(txInfo,tx,ex);
            throw ex;
        }

        // 4. everything is fine, commit.
        commitTransaction(tx);

        return rs;
    } finally {
        //5. clear
        triggerAfterCompletion();
        cleanUp();
    }
}
```

以上就是我们使用 API 时经常写的冗余代码，现在 Spring 通过代理模式，把这些冗余代码都封装带模版里面了，它将那些冗余代码统统封装起来统一流程处理，并不需要你显示写出来了.

## 服务端处理逻辑

服务端收到客户端的连接,是将其channel 也缓存起来,前面也说到客户端会发送 RegisterRMRequest/RegisterTMRequest 请求给服务端，服务端收到后会调用 ServerMessageListener 监听器处理：

io.seata.core.rpc.ServerMessageListener

```java
publicinterface ServerMessageListener {
  // 处理各种事务，如分支注册、分支提交、分支上报、分支回滚等等
  void onTrxMessage(RpcMessage request, ChannelHandlerContext ctx, ServerMessageSender sender);
	// 处理 RM 客户端的注册连接
  void onRegRmMessage(RpcMessage request, ChannelHandlerContext ctx,
                      ServerMessageSender sender, RegisterCheckAuthHandler checkAuthHandler);
  // 处理 TM 客户端的注册连接
  void onRegTmMessage(RpcMessage request, ChannelHandlerContext ctx,
                      ServerMessageSender sender, RegisterCheckAuthHandler checkAuthHandler);
  // 服务端与客户端保持心跳
  void onCheckMessage(RpcMessage request, ChannelHandlerContext ctx, ServerMessageSender sender)
}
```

ChannelManager 是服务端 channel 的管理器，服务端每次和客户端通信，都需要从 ChannelManager 中获取客户端对应的 channel，它用于保存 TM 和 RM 客户端 channel 的缓存结构如下：

```java
/**
 * resourceId -> applicationId -> ip -> port -> RpcContext
 */
privatestaticfinal ConcurrentMap<String, ConcurrentMap<String, ConcurrentMap<String, ConcurrentMap<Integer,
RpcContext>>>>
  RM_CHANNELS = new ConcurrentHashMap<String, ConcurrentMap<String, ConcurrentMap<String, ConcurrentMap<Integer,
RpcContext>>>>();

/**
 * ip+appname,port
 */
privatestaticfinal ConcurrentMap<String, ConcurrentMap<Integer, RpcContext>> TM_CHANNELS
  = new ConcurrentHashMap<String, ConcurrentMap<Integer, RpcContext>>();
```

**RM_CHANNELS**：

1. resourceId 指的是 RM client 的数据库地址；
2. applicationId 指的是 RM client 的服务 Id，比如 springboot 的配置 spring.application.name=account-service 中的 account-service 即是  applicationId；
3. ip 指的是 RM client 服务地址；
4. port 指的是 RM client 服务地址；
5. RpcContext 保存了本次注册请求的信息。

**TM_CHANNELS**：

1. ip+appname：这里的注释应该是写错了，应该是 appname+ip，即 TM_CHANNELS 的 Map 结构第一个 key 为 appname+ip；
2. port：客户端的端口号。

以下是 RM Client 注册逻辑：

io.seata.core.rpc.ChannelManager#registerRMChannel

```java
public static void registerRMChannel(RegisterRMRequest resourceManagerRequest, Channel channel)
    throws IncompatibleVersionException {
    Version.checkVersion(resourceManagerRequest.getVersion());
    //  将 ResourceIds 数据库连接连接信息放入一个set中
    Set<String> dbkeySet = dbKeytoSet(resourceManagerRequest.getResourceIds());
    RpcContext rpcContext;
    //判断缓存中是否有改channel信息
    if (!IDENTIFIED_CHANNELS.containsKey(channel)) {
        //根据请求信息,构建rpcContext
        rpcContext = buildChannelHolder(NettyPoolKey.TransactionRole.RMROLE, resourceManagerRequest.getVersion(),
            resourceManagerRequest.getApplicationId(), resourceManagerRequest.getTransactionServiceGroup(),
            resourceManagerRequest.getResourceIds(), channel);
        //将rpcContext放入缓存
        rpcContext.holdInIdentifiedChannels(IDENTIFIED_CHANNELS);
    } else {
        rpcContext = IDENTIFIED_CHANNELS.get(channel);
        //添加数据库连接信息
        rpcContext.addResources(dbkeySet);
    }
    if (dbkeySet == null || dbkeySet.isEmpty()) { return; }
    for (String resourceId : dbkeySet) {
        String clientIp;
            // 将请求信息存入 RM_CHANNELS 中，这里用了 java8 的 computeIfAbsent 方法操作
        ConcurrentMap<Integer, RpcContext> portMap = CollectionUtils.computeIfAbsent(RM_CHANNELS, resourceId, key -> new ConcurrentHashMap<>())
                .computeIfAbsent(resourceManagerRequest.getApplicationId(), key -> new ConcurrentHashMap<>())
                .computeIfAbsent(clientIp = ChannelUtil.getClientIpFromChannel(channel), key -> new ConcurrentHashMap<>());
		// 将当前 rpcContext 放入 portMap 中
        rpcContext.holdInResourceManagerChannels(resourceId, portMap);
        updateChannelsResource(resourceId, clientIp, resourceManagerRequest.getApplicationId());
    }
}
```

注册 RM client 主要是将注册请求信息，放入 RM_CHANNELS 缓存中，同时还会从 IDENTIFIED_CHANNELS 中判断本次请求的 channel 是否已验证过，IDENTIFIED_CHANNELS 的结构如下：

```java
private static final ConcurrentMap<Channel, RpcContext> IDENTIFIED_CHANNELS = new ConcurrentHashMap<>();
```

以下是 TM 注册逻辑：

io.seata.core.rpc.ChannelManager#registerTMChannel

```java
public static void registerTMChannel(RegisterTMRequest request, Channel channel)
    throws IncompatibleVersionException {
    Version.checkVersion(request.getVersion());
    //构建RpcContext
    RpcContext rpcContext = buildChannelHolder(NettyPoolKey.TransactionRole.TMROLE, request.getVersion(),
        request.getApplicationId(),
        request.getTransactionServiceGroup(),
        null, channel);
    // 将 RpcContext 放入 IDENTIFIED_CHANNELS 缓存中
    rpcContext.holdInIdentifiedChannels(IDENTIFIED_CHANNELS);
    //字符串形式如下: account-service:127.0.0.1:63353
    String clientIdentified = rpcContext.getApplicationId() + Constants.CLIENT_ID_SPLIT_CHAR
        + ChannelUtil.getClientIpFromChannel(channel);
    //将请求信息存入 TM_CHANNELS 缓存中,将上一步创建好的get出来，之后再将rpcContext放入这个map的value中
    ConcurrentMap<Integer, RpcContext> clientIdentifiedMap = CollectionUtils.computeIfAbsent(TM_CHANNELS,
        clientIdentified, key -> new ConcurrentHashMap<>());
    rpcContext.holdInClientChannels(clientIdentifiedMap);
}
```

TM client 的注册大体类似，把本次注册的信息放入对应的缓存中保存，但比 RM client 的注册逻辑简单一些，主要是 RM client 会涉及分支事务资源的信息，需要注册的信息也会比 TM client 多。















参考:[Seata AT 模式启动源码分析](https://mp.weixin.qq.com/s?__biz=MzU3MjQ1ODcwNQ==&mid=2247484327&idx=1&sn=0fffba772a6124a92feb85913eb0f8a4&chksm=fcd1d019cba6590f42b731f60206f8b7f0ad244be1c33e7bc7cced821ec595207b0e2c1bf2fa&scene=178&cur_album_id=1337925915665399808#rd)

[Seata AT模式源码大剖析](https://juejin.cn/post/7083769837126123527#heading-9)

[源码系列(八)-Seata1.5.0源码解析](https://blog.csdn.net/zwq56693/article/details/123643074)