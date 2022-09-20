# 深度剖析Seata TCC模式

## 1 什么是TCC

TCC(Try-Confirm-Cancel)是分布式事务中的二阶段提交协议,即资源预留(try),确认操作(confirm),取消操作(cancel),具体含义如下:

1. Try:对业务资源的检查并预留
2. confirm:对业务处理进行提交,只要try成功,那么该步骤一定成功
3. cancel:对业务处理进行取消,即回滚,对try预留的资源进行释放

TCC是一种侵入式的分布式事务解决方案,以上三个操作都需要业务系统自行实现,对业务系统有着非常大的侵入性,设计相对复杂,但是TCC不依赖数据库,能够实现跨数据库,跨应用资源管理,对这些不同数据访问通过侵入式编码方式实现一个原子操作,更好解决了在各种复杂业务场景下的分布式事务问题.

![图片](https://mmbiz.qpic.cn/mmbiz_png/4MfwdbrwE6d4mBXw77t7OtKMBVRlRBbl9iatc1Cuh6qWgTib3aicMGFkfHFULpKNYeRjLJeqWIUiaCUv6MIge8TQXQ/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

## 2 Seata TCC 模式

Seata TCC 模式跟通用型 TCC 模式原理一致，我们先来使用 Seata TCC 模式实现一个分布式事务：

主要是有一个服务实现了TCC的接口:

```java
public interface TccActionOne {

    @TwoPhaseBusinessAction(name = "DubboTccActionOne", commitMethod = "commit", rollbackMethod = "rollback")
    public boolean prepare(BusinessActionContext actionContext, @BusinessActionContextParameter(paramName = "a") int a);
    public boolean commit(BusinessActionContext actionContext);
    public boolean rollback(BusinessActionContext actionContext);
}
```

同样服务B实现了TCC接口:

```java
public interface TccActionTwo {


    @TwoPhaseBusinessAction(name = "DubboTccActionTwo", commitMethod = "commit", rollbackMethod = "rollback")
    public boolean prepare(BusinessActionContext actionContext,
                           @BusinessActionContextParameter(paramName = "b") String b,
                           @BusinessActionContextParameter(paramName = "c", index = 1) List list);
    public boolean commit(BusinessActionContext actionContext);
    public boolean rollback(BusinessActionContext actionContext);

}
```

业务系统中去调用这两个服务的prepare方法:

```java
//开启全局事务
@GlobalTransactional
public String doTransactionCommit() {
    //第一个TCC 事务参与者
    boolean result = tccActionOne.prepare(null, 1);
    if (!result) {
        throw new RuntimeException("TccActionOne failed.");
    }
    List list = new ArrayList();
    list.add("c1");
    list.add("c2");
    result = tccActionTwo.prepare(null, "two", list);
    if (!result) {
        throw new RuntimeException("TccActionTwo failed.");
    }
    return RootContext.getXID();
}
```

以上就是使用 Seata TCC 模式实现一个全局事务的例子，可以看出，TCC 模式同样使用 `@GlobalTransactional` 注解开启全局事务，而服务 A 和服务 B 的 TCC 接口为事务参与者，Seata 会把一个 TCC 接口当成一个 Resource，也叫 TCC Resource。

TCC 接口可以是 RPC，也可以是 JVM 内部调用，意味着一个 TCC 接口，会有发起方和调用方两个身份，以上例子，TCC 接口在服务 A 和服务 B 中是发起方，在业务所在系统中是调用方。如果该 TCC 接口为 Dubbo RPC，那么调用方就是一个 dubbo:reference，发起方则是一个 dubbo:service。

![图片](https://mmbiz.qpic.cn/mmbiz_png/4MfwdbrwE6d4mBXw77t7OtKMBVRlRBblib91IzAYbpYyyUcn2rMdPxmxAkvYicApr9jIiaAicCPTHNw4r2pw8VKcnQ/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

Seata 启动时会对 TCC 接口进行扫描并解析，如果 TCC 接口是一个发布方，则在 Seata 启动时会向 TC 注册 TCC Resource，每个 TCC Resource 都有一个资源 ID；如果 TCC 接口时一个调用方，Seata 代理调用方，与 AT 模式一样，代理会拦截 TCC 接口的调用，即每次调用 Try 方法，会向 TC 注册一个分支事务，接着才执行原来的 RPC 调用。

当全局事务决议提交/回滚时，TC 会通过分支注册的的资源 ID 回调到对应参与者服务中执行 TCC Resource 的 Confirm/Cancel 方法。

## 2 Seata 如何实现 TCC 模式

Seata主要通过资源解析,资源管理,事务处理等步骤,实现TCC模式.

主要涉及到的源码集中于:

| module       | class                                                       | 功能                                            |
| :----------- | :---------------------------------------------------------- | :---------------------------------------------- |
| seata-spring | GlobalTransactionalInterceptor.class                        | 全局事务切面逻辑，包括注册全局事务，拿到 xid    |
| seata-spring | TccActionInterceptor.class                                  | TCC 参与方切面逻辑                              |
| seata-tcc    | TCCResourceManager.class                                    | 解析 TCC Bean，保存 TCC Resources，便于后续回调 |
| seata-tcc    | ActionInterceptorHandler.class                              | TCC 分支事务注册实现                            |
| seata-server | DefaultCoordinator.class、FileTransactionStoreManager.class | 主要是 TC 的实现、事务存储等实现                |



### 资源解析

资源解析是把TCC接口进行解析并注册,为此Seata TCC模块中专门有一个remoting模块,该模块专门用于解析具有`TwoPhaseBusinessAction` 注解的 TCC 接口资源:

![图片](https://mmbiz.qpic.cn/mmbiz_png/4MfwdbrwE6d4mBXw77t7OtKMBVRlRBblxIvRqlftiaiacrEPl5pEpnqIokjUia87qlp43aZm2TXupVBPXvibdV38Jg/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

RemotingParser接口如下,他的默认实现为DefaultRemotingParser,DefaultRemotingParser

```java
public interface RemotingParser {
	//遍历所有协议类判断是否是远程调用bean
    boolean isRemoting(Object bean, String beanName) throws FrameworkException;

    boolean isReference(Object bean, String beanName) throws FrameworkException;

    boolean isService(Object bean, String beanName) throws FrameworkException;

    RemotingDesc getServiceDesc(Object bean, String beanName) throws FrameworkException;

    short getProtocol();

}
```

Seata客户端启动时`GlobalTransactionScanner`扫描`GlobalTransactional`,`GlobalLock`注解,同事会执行`TCCBeanParserUtils#isTccAutoProxy`方法,判断bean是否被TCC代理,这个过程中先判断bean是否是一个 Remoting bean，如果是则调用 `getServiceDesc` 方法对 remoting bean 进行解析，同时判断如果是一个发起方，则对其进行资源注册：

```java
public RemotingDesc parserRemotingServiceInfo(Object bean, String beanName) {
    //判断是否是remoting bean
    RemotingDesc remotingBeanDesc = getServiceDesc(bean, beanName);
    if (remotingBeanDesc == null) {
        return null;
    }
    remotingServiceMap.put(beanName, remotingBeanDesc);

    Class<?> interfaceClass = remotingBeanDesc.getInterfaceClass();
    Method[] methods = interfaceClass.getMethods();
    //发起方则注册
    if (isService(bean, beanName)) {
        try {
            //service bean, registry resource
            Object targetBean = remotingBeanDesc.getTargetBean();
            for (Method m : methods) {
                TwoPhaseBusinessAction twoPhaseBusinessAction = m.getAnnotation(TwoPhaseBusinessAction.class);
                if (twoPhaseBusinessAction != null) {
                    //解析Tcc资源信息并将该信息注册到服务端,以便后续回调
                    TCCResource tccResource = new TCCResource();
                    tccResource.setActionName(twoPhaseBusinessAction.name());
                    tccResource.setTargetBean(targetBean);
                    tccResource.setPrepareMethod(m);
                    tccResource.setCommitMethodName(twoPhaseBusinessAction.commitMethod());
                    tccResource.setCommitMethod(ReflectionUtil
                        .getMethod(interfaceClass, twoPhaseBusinessAction.commitMethod(),
                            new Class[] {BusinessActionContext.class}));
                    tccResource.setRollbackMethodName(twoPhaseBusinessAction.rollbackMethod());
                    tccResource.setRollbackMethod(ReflectionUtil
                        .getMethod(interfaceClass, twoPhaseBusinessAction.rollbackMethod(),
                            new Class[] {BusinessActionContext.class}));
                    //registry tcc resource,将资源放到本地缓存中tccResourceCache
                    DefaultResourceManager.get().registerResource(tccResource);
                }
            }
        } catch (Throwable t) {
            throw new FrameworkException(t, "parser remoting service error");
        }
    }
    if (isReference(bean, beanName)) {
        //reference bean, TCC proxy
        remotingBeanDesc.setReference(true);
    }
    return remotingBeanDesc;
}
```

以上方法，先调用解析类 `getServiceDesc` 方法对 remoting bean 进行解析，并将解析后的 `remotingBeanDesc` 放入 本地缓存 `remotingServiceMap` 中，同时调用解析类 `isService` 方法判断是否为发起方，如果是发起方，则解析 `TwoPhaseBusinessAction` 注解内容生成一个 `TCCResource`，并对其进行资源注册。

### 资源管理

#### **1、资源注册**

Seata TCC 模式的资源叫 `TCCResource`，其资源管理器叫 `TCCResourceManager`，前面讲过，当解析完 TCC 接口 RPC 资源后，如果是发起方，则会对其进行资源注册:

io.seata.rm.tcc.TCCResourceManager#registerResource

```java
// 内存中保存的 resourceId 和 TCCResource 的映射关系
private Map<String, Resource> tccResourceCache = new ConcurrentHashMap<String, Resource>();

public void registerResource(Resource resource) {
    TCCResource tccResource = (TCCResource)resource;
    tccResourceCache.put(tccResource.getResourceId(), tccResource);
    //rm注册
    super.registerResource(tccResource);
}
```

`TCCResource` 包含了 TCC 接口的相关信息，同时会在本地进行缓存。继续调用父类 `registerResource` 方法（封装了通信方法）向 TC 注册，TCC 资源的 resourceId 是 actionName，actionName 就是 `@TwoParseBusinessAction` 注解中的 name。

#### **2、资源提交/回滚**

io.seata.rm.tcc.TCCResourceManager#branchCommit

```java
public BranchStatus branchCommit(BranchType branchType, String xid, long branchId, String resourceId,
                                 String applicationData) throws TransactionException {
    TCCResource tccResource = (TCCResource)tccResourceCache.get(resourceId);
    if (tccResource == null) {
        throw new ShouldNeverHappenException("TCC resource is not exist, resourceId:" + resourceId);
    }
    Object targetTCCBean = tccResource.getTargetBean();
    Method commitMethod = tccResource.getCommitMethod();
    if (targetTCCBean == null || commitMethod == null) {
        throw new ShouldNeverHappenException("TCC resource is not available, resourceId:" + resourceId);
    }
    try {
        boolean result = false;
        //BusinessActionContext
        BusinessActionContext businessActionContext = getBusinessActionContext(xid, branchId, resourceId,
            applicationData);
        //发射调用 commit方法
        Object ret = commitMethod.invoke(targetTCCBean, businessActionContext);
        if (ret != null) {
            if (ret instanceof TwoPhaseResult) {
                result = ((TwoPhaseResult)ret).isSuccess();
            } else {
                result = (boolean)ret;
            }
        }
        return result ? BranchStatus.PhaseTwo_Committed : BranchStatus.PhaseTwo_CommitFailed_Retryable;
    } catch (Throwable t) {
        throw new FrameworkException(t, msg);
    }
}
```

当 TM 决议二阶段提交，TC 会通过分支注册的的资源 ID 回调到对应参与者（即 TCC 接口发起方）服务中执行 TCC Resource 的 Confirm/Cancel 方法。

资源管理器中会根据 resourceId 在本地缓存找到对应的 `TCCResource`，同时根据 xid、branchId、resourceId、applicationData 找到对应的 `BusinessActionContext` 上下文，执行的参数就在上下文中。最后，执行 `TCCResource` 中获取 `commit` 的方法进行二阶段提交。

二阶段回滚同理类似。

### 事务处理

前面讲过，如果 TCC 接口时一个调用方，则会使用 Seata TCC 代理对调用方进行拦截处理，并在处理调用真正的 RPC 方法前对分支进行注册。

执行方法`io.seata.spring.util.TCCBeanParserUtils#isTccAutoProxy`除了对 TCC 接口资源进行解析，还会判断 TCC 接口是否为调用方，如果是调用方则返回 true：

io.seata.spring.annotation.GlobalTransactionScanner#wrapIfNecessary

```java
if (TCCBeanParserUtils.isTccAutoProxy(bean, beanName, applicationContext)) {
    //TCC interceptor, proxy bean of sofa:reference/dubbo:reference, and LocalTCC
    interceptor = new TccActionInterceptor(TCCBeanParserUtils.getRemotingDesc(beanName));
}
```

如图，当 `GlobalTransactionalScanner` 扫描到 TCC 接口调用方（Reference）时，会使 `TccActionInterceptor` 对其进行代理拦截处理，`TccActionInterceptor` 实现 `MethodInterceptor`。

在 `TccActionInterceptor` 中还会调用 `ActionInterceptorHandler` 类型执行拦截处理逻辑，事务相关处理就在 `ActionInterceptorHandler#proceed` 方法中：

```java
public Object invoke(final MethodInvocation invocation) throws Throwable {
    if (!RootContext.inGlobalTransaction() || disable || RootContext.inSagaBranch()) {
        //not in transaction
        return invocation.proceed();
    }
    Method method = getActionInterfaceMethod(invocation);
    TwoPhaseBusinessAction businessAction = method.getAnnotation(TwoPhaseBusinessAction.class);
    //try method
    if (businessAction != null) {
        //save the xid
        String xid = RootContext.getXID();
        //save the previous branchType
        BranchType previousBranchType = RootContext.getBranchType();
        //if not TCC, bind TCC branchType
        if (BranchType.TCC != previousBranchType) {
            RootContext.bindBranchType(BranchType.TCC);
        }
        try {
            Object[] methodArgs = invocation.getArguments();
            //Handler the TCC Aspect
            Map<String, Object> ret = actionInterceptorHandler.proceed(method, methodArgs, xid, businessAction,
                    invocation::proceed);
            //return the final result
            return ret.get(Constants.TCC_METHOD_RESULT);
        }
        finally {
            //if not TCC, unbind branchType
            if (BranchType.TCC != previousBranchType) {
                RootContext.unbindBranchType();
            }
            //MDC remove branchId
            MDC.remove(RootContext.MDC_KEY_BRANCH_ID);
        }
    }
    return invocation.proceed();
}
```

实际TCC相关逻辑处理

```java
public Object proceed(Method method, Object[] arguments, String xid, TwoPhaseBusinessAction businessAction,
                      Callback<Object> targetCallback) throws Throwable {
    //Get action context from arguments, or create a new one and then reset to arguments
    BusinessActionContext actionContext = getOrCreateActionContextAndResetToArguments(method.getParameterTypes(), arguments);
    //Creating Branch Record
    String branchId = doTccActionLogStore(method, arguments, businessAction, actionContext);
    // ... ... 
    try { 
        // ... ...
        return targetCallback.execute();
    } finally {
        try {
            //to report business action context finally if the actionContext.getUpdated() is true
            BusinessActionContextUtil.reportContext(actionContext);
        } finally {
            // ... ... 
        }
    }
}
```

以上，在执行 TCC 接口一阶段之前，会调用 `doTccActionLogStore` 方法分支注册，同时还会将 TCC 相关信息比如参数放置在上下文，上面讲的资源提交/回滚就会用到这个上下文。

## 如何控制异常

### 如何处理空回滚

![图片](https://mmbiz.qpic.cn/mmbiz_png/4MfwdbrwE6d4mBXw77t7OtKMBVRlRBblb5C8gBNTaD94MDQZanBsvUPvicnibaQ7jb9cic8ibYzsnBWeUiciaFib21HzA/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

**空翻滚:**在没有调用参与方的 Try 方法的情况下，TM 驱动二阶段回滚调用了参与方的 Cancel 方法,可能因为网络抖动,服务异常导致Try方法未执行.但是此时全局事务已经开启，Seata 必须要推进到终态，在全局事务回滚时会调用参与者 A 的 Cancel 方法，从而造成空回滚。

**处理方式:**Seata 的做法是新增一个 TCC 事务控制表，包含事务的 XID 和 BranchID 信息，在 Try 方法执行时插入一条记录，表示一阶段执行了，执行 Cancel 方法时读取这条记录，如果记录不存在，说明 Try 方法没有执行。

### 如何处理幂等

![图片](https://mmbiz.qpic.cn/mmbiz_png/4MfwdbrwE6d4mBXw77t7OtKMBVRlRBbl7JzkibThAicvzkwwShb0DkaCGBKXY4hvEF7UlwOAhVjAfkrWwvOl4xBQ/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

参与者 A 执行完二阶段之后，由于网络抖动或者宕机问题，会造成 TC 收不到参与者 A 执行二阶段的返回结果，TC 会重复发起调用，直到二阶段执行结果成功

**处理方式**

同样的也是在 TCC 事务控制表中增加一个记录状态的字段 status，该字段有有 3 个值，分别为：

1. tried：1
2. committed：2
3. rollbacked：3

二阶段 Confirm/Cancel 方法执行后，将状态改为 committed 或 rollbacked 状态。当重复调用二阶段 Confirm/Cancel 方法时，判断事务状态即可解决幂等问题

### 如何处理悬挂

悬挂指的是二阶段 Cancel 方法比 一阶段 Try 方法优先执行，由于允许空回滚的原因，在执行完二阶段 Cancel 方法之后直接空回滚返回成功，此时全局事务已结束，但是由于 Try 方法随后执行，这就会造成一阶段 Try 方法预留的资源永远无法提交和释放了。

![图片](https://mmbiz.qpic.cn/mmbiz_png/4MfwdbrwE6d4mBXw77t7OtKMBVRlRBblFPZGKcAX6icRxb2NNicibataztZkSxDuaKywrgkWFNb8aOL6DdKEGXKQg/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

如上图所示，在执行参与者 A 的一阶段 Try 方法时，出现网路拥堵，由于 Seata 全局事务有超时限制，执行 Try 方法超时后，TM 决议全局回滚，回滚完成后如果此时 RPC 请求才到达参与者 A，执行 Try 方法进行资源预留，从而造成悬挂。

**处理方式**

在 TCC 事务控制表记录状态的字段 status 中增加一个状态：

1. suspended：4

当执行二阶段 Cancel 方法时，如果发现 TCC 事务控制表有相关记录，说明二阶段 Cancel 方法优先一阶段 Try 方法执行，因此插入一条 status=4 状态的记录，当一阶段 Try 方法后面执行时，判断 status=4 ，则说明有二阶段 Cancel 已执行，并返回 false 以阻止一阶段 Try 方法执行成功。









参考:[深度剖析 Seata TCC 模式【图解 + 源码分析】](https://mp.weixin.qq.com/s?__biz=MzU3MjQ1ODcwNQ==&mid=2247490856&idx=1&sn=612b9a06c68718321387993d34fbbb51&chksm=fcd1ce96cba64780a9a6445a16fe13e61514718edad1c10b41ac063c42ee3e4802fa01f92b82&cur_album_id=1337925915665399808&scene=190#rd)

[Seata TCC 分布式事务源码分析](https://chenjiayang.me/2019/05/02/seata-tcc/)