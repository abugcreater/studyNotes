# 深度剖析Seata

## 1. Transaction Coordinator

TC是事务管理的核心,如果TC出现问题会导致分布式事务管理错误.TC的特点包括以下几点:

1. 正确的协调:能正确的协调RM和TM接下来应该做什么,做错了应该怎么办,做对了应该怎么办
2. 高可用:事务协调器在分布式事务中很重要,如果不能保证高可用,那么他也没有存在的必要了
3. 高性能:事务协调器的性能一定要高,如果事务协调性能有瓶颈,那么他所管理的RM和TM会经常遇到超时,从而导致频繁回滚
4. 高扩展行:这个特点属于代码层面,如果一个优秀的框架,那么需要适用方很多自定义扩展,比如服务注册/发现,读取配置等等.

### 1.1 Seata-Server 的设计

seata-server对应的正是TC的角色.

![img](https://pic1.zhimg.com/80/v2-69f62ca87bbf567384769ddcf8209b68_720w.jpg)

- Coordinator Core:最下面的模块是事务协调器核心代码,主要用于处理事务协调的逻辑,如是否commit,rollback等
- Store:存储模块,用于将我们数据持久化,防止重启或者宕机数据丢失
- Discover:服务注册/发现模块,用于将Server地址暴露给Client
- Config:用于存储和查找服务端的配置
- Lock:锁模块,用于给Seata提供全局锁功能
- RPC:用于和其他端通信
- HA-Cluster:高可用集群,目前还没开源,提供可靠的高可用功能

### 1.2 Discover

服务注册/发现模块,在Seata-Server启动之后,需要将自己的地址暴露给其他使用者,那么就需要这个模块帮忙.

该模块下核心是`seata-discovery-core`,他包括了两个基础包:loadbalance和registry.那么注册的核心接口是registry包下的`RegistryService`

```java
public interface RegistryService<T> {

    
    void register(InetSocketAddress address) throws Exception;

   
    void unregister(InetSocketAddress address) throws Exception;


    void subscribe(String cluster, T listener) throws Exception;

  
    void unsubscribe(String cluster, T listener) throws Exception;

    
    List<InetSocketAddress> lookup(String key) throws Exception;

    
    void close() throws Exception;

}
```

该类的属性如上所示,主要包括注册,订阅,获取列表等.

- register：服务端使用，进行服务注册。
- unregister：服务端使用，一般在 JVM 关闭钩子，ShutdownHook 中调用。
- subscribe：客户端使用，注册监听事件，用来监听地址的变化。
- unsubscribe：客户端使用，取消注册监听事件。
- lookup：客户端使用，根据 Key 查找服务地址列表。
- close：都可以使用，用于关闭 Register 资源。

如果需要自定义服务发现,只要实现这个接口即可.已Nacos为例.



#### 1.2.1 register 接口

```java
public void register(InetSocketAddress address) throws Exception {
    validAddress(address);
    getNamingInstance().registerInstance(PRO_SERVER_ADDR_KEY, address.getAddress().getHostAddress(), address.getPort(), getClusterName());
}
```

首先校验地址是否合法,获取nacos的name实例,然后将地址注册到当前Cluster名称上.

`unregister`方法实现与之类似,不过是将注册变成注销.

#### 1.2.2 lookup 接口

```java
public List<InetSocketAddress> lookup(String key) throws Exception {
    //组装cluster名称
    String clusterName = getServiceGroup(key);
    if (null == clusterName) {
        return null;
    }
    //判断是否已经有,如果没有则进行获取,如果有就直接从缓存中获取
    if (!LISTENER_SERVICE_MAP.containsKey(clusterName)) {
        synchronized (LOCK_OBJ) {
            if (!LISTENER_SERVICE_MAP.containsKey(clusterName)) {
                List<String> clusters = new ArrayList<>();
                clusters.add(clusterName);
                //获取所有该名称对应的实例
                List<Instance> firstAllInstances = getNamingInstance().getAllInstances(PRO_SERVER_ADDR_KEY, clusters);
                if (null != firstAllInstances) {
                    List<InetSocketAddress> newAddressList = new ArrayList<>();
                    for (Instance instance : firstAllInstances) {
                        if (instance.isEnabled() && instance.isHealthy()) {
                            newAddressList.add(new InetSocketAddress(instance.getIp(), instance.getPort()));
                        }
                    }
                    CLUSTER_ADDRESS_MAP.put(clusterName, newAddressList);
                }
                //注册listener监听地址变动
                subscribe(clusterName, event -> {
                    List<Instance> instances = ((NamingEvent)event).getInstances();
                    //更新最新地址列表
                    if (null == instances && null != CLUSTER_ADDRESS_MAP.get(clusterName)) {
                        CLUSTER_ADDRESS_MAP.remove(clusterName);
                    } else if (!CollectionUtils.isEmpty(instances)) {
                        List<InetSocketAddress> newAddressList = new ArrayList<>();
                        for (Instance instance : instances) {
                            if (instance.isEnabled() && instance.isHealthy()) {
                                newAddressList.add(new InetSocketAddress(instance.getIp(), instance.getPort()));
                            }
                        }
                        CLUSTER_ADDRESS_MAP.put(clusterName, newAddressList);
                    }
                });
            }
        }
    }
    return CLUSTER_ADDRESS_MAP.get(clusterName);
}
```

step1：获取当前 clusterName 名字；

step2：判断当前 Cluster 是否已经获取过了，如果获取过就从 Map 中取；

step3：从 Nacos 拿到地址数据，将其转换成我们所需要的；

step4：将我们事件变动的 Listener 注册到 Nacos。

#### 1.2.3 subscribe 接口

```java
public void subscribe(String cluster, EventListener listener) throws Exception {
    List<String> clusters = new ArrayList<>();
    clusters.add(cluster);
    LISTENER_SERVICE_MAP.putIfAbsent(cluster, new ArrayList<>());
    LISTENER_SERVICE_MAP.get(cluster).add(listener);
    getNamingInstance().subscribe(PRO_SERVER_ADDR_KEY, clusters, listener);
}
```

step1：将 Clstuer 和 Listener 添加进 Map 中；

step2：向 Nacos 注册

`unsubscribe`与之类似,只不过移除map中的listener,并且调用nacos移除监听

### 1.3 Config

配置模块也是一个比较基础，比较简单的模块。我们需要配置一些常用的参数比如：Netty 的 Select 线程数量，Work 线程数量，Session 允许最大为多少等等，当然这些参数在 Seata 中都有自己的默认设置。

同样的在 Seata 中也提供了一个接口 Configuration，用来自定义我们需要的获取配置的地方：

```java
public interface Configuration {

    short getShort(String dataId, int defaultValue, long timeoutMills);

	// 省略 getInt,getLong....
  
    String getConfig(String dataId, String defaultValue, long timeoutMills);

    String getConfig(String dataId);

    boolean putConfig(String dataId, String content, long timeoutMills);

    boolean putConfigIfAbsent(String dataId, String content);

    boolean removeConfig(String dataId, long timeoutMills);

    void addConfigListener(String dataId, ConfigurationChangeListener listener);


    void removeConfigListener(String dataId, ConfigurationChangeListener listener);

    Set<ConfigurationChangeListener> getConfigListeners(String dataId);

    default String getConfigFromSysPro(String dataId) {
        return System.getProperty(dataId);
    }

}
```

- getInt/Long/Boolean/Config()：通过 DataId 来获取对应的值。
- putConfig：用于添加配置。
- removeConfig：删除一个配置。
- add/remove/get ConfigListener：添加/删除/获取 配置监听器，一般用来监听配置的变更。

目前为止有四种方式获取 Config：File（文件获取）、Nacos、Apollo、ZK。在 Seata 中首先需要配置 registry.conf，来配置 conf 的类型。

### 1.4 Store

存储层的实现对于 Seata 是否高性能，是否可靠非常关键。如果没有存储层,当发生宕机时那么正在进行的分布式事务都会丢失,如果存储层性能由问题,那么RM可能会频繁发生回滚.

在 Seata 中默认提供了文件方式的存储，下面定义存储的数据为 Session，而 TM 创造的全局事务数据叫 GloabSession，RM 创造的分支事务叫 BranchSession，一个 GloabSession 可以拥有多个 BranchSession。我们的目的就是要将这么多 Session 存储下来.

seata对于数据的存储管理定义在`TransactionStoreManager`接口中.默认保存在文件中,目前也有数据库实现`DatabaseTransactionStoreManager`



在 FileTransactionStoreManager#writeSession 代码中：

```java
public boolean writeSession(LogOperation logOperation, SessionStorable session) {
        writeSessionLock.lock();
        long curFileTrxNum;
        try {
            //校验并分批写入数据
            if (!writeDataFile(new TransactionWriteStore(session, logOperation).encode())) {
                return false;
            }
            lastModifiedTime = System.currentTimeMillis();
            //更新当前文件中事务数量
            curFileTrxNum = FILE_TRX_NUM.incrementAndGet();
            //超过时间或数量限制保存到历史文件中
            if (curFileTrxNum % PER_FILE_BLOCK_SIZE == 0
                && (System.currentTimeMillis() - trxStartTimeMills) > MAX_TRX_TIMEOUT_MILLS) {
                return saveHistory();
            }
        } finally {
            writeSessionLock.unlock();
        }
        //刷盘
        flushDisk(curFileTrxNum, currFileChannel);
        return true;
    }
```

step 1:首先获取到锁

step 2:将初始化TransactionWriteStore,并编码为byte数组,写入到文件通道中.

step 3:将超过限制的文件保存到历史文件中

step 4:利用writeDataFileRunnable根据类型选择同步或异步刷盘

当需要恢复保存的session时会通过初始化时调用 SessionHolder#init方法,触发FileBasedSessionManager#reload方法,将文件中的session加载到缓存中.

恢复方法比较简单.文件操作部分主要是 FileTransactionStoreManager#parseDataFile 解析文件,然后获取文件中的所有session信息.

### 1.5 Lock

Seata通过锁实现数据库隔离级别,一般写操作是互斥的,而读的级别是未提交,但是提供了达到读已提交的手段.

Lock 模块也就是 Seata 实现隔离级别的核心模块。在 Lock 模块中提供了一个接口用于管理锁：

```java
public interface LockManager {

    //RM 获取锁信息
    boolean acquireLock(BranchSession branchSession) throws TransactionException;

    //RM 释放锁
    boolean releaseLock(BranchSession branchSession) throws TransactionException;

    // TC 释放全局锁
    boolean releaseGlobalSessionLock(GlobalSession globalSession) throws TransactionException;

    //判断锁是否可用
    boolean isLockable(String xid, String resourceId, String lockKey) throws TransactionException;

    //清除所有锁信息
    void cleanAllLocks() throws TransactionException;

}
```

其中有三个方法：

- acquireLock：用于对 BranchSession 加锁，这里虽然是传的分支事务 Session，实际上是对分支事务的资源加锁，成功返回 true。
- isLockable：根据事务 ID，资源 ID，锁住的 Key 来查询是否已经加锁。
- cleanAllLocks：清除所有的锁。

获取锁的实现方式

`AbstractLockManager# acquireLock`

```java
public boolean acquireLock(BranchSession branchSession) throws TransactionException {
    if (branchSession == null) {
        throw new IllegalArgumentException("branchSession can't be null for memory/file locker.");
    }
    String lockKey = branchSession.getLockKey();
    if (StringUtils.isNullOrEmpty(lockKey)) {
        //no lock
        return true;
    }
    //解析当前分支的信息 xid,resourceId等,然后遍历组装RowLock
    List<RowLock> locks = collectRowLocks(branchSession);
    if (CollectionUtils.isEmpty(locks)) {
        //no lock
        return true;
    }
    //具体获取锁信息
    return getLocker(branchSession).acquireLock(locks);
}
```

**存储为database的获取锁方式:**

`LockStoreDataBaseDAO#acquireLock`

```java
public boolean acquireLock(List<LockDO> lockDOs) {
    Connection conn = null;
    PreparedStatement ps = null;
    ResultSet rs = null;
    Set<String> dbExistedRowKeys = new HashSet<>();
    boolean originalAutoCommit = true;
    if (lockDOs.size() > 1) {
        lockDOs = lockDOs.stream().filter(LambdaUtils.distinctByKey(LockDO::getRowKey)).collect(Collectors.toList());
    }
    try {
        conn = logStoreDataSource.getConnection();
        if (originalAutoCommit = conn.getAutoCommit()) {
            conn.setAutoCommit(false);
        }
        //check lock
        StringJoiner sj = new StringJoiner(",");
        for (int i = 0; i < lockDOs.size(); i++) {
            sj.add("?");
        }
        boolean canLock = true;
        //query
        String checkLockSQL = LockStoreSqls.getCheckLockableSql(lockTable, sj.toString(), dbType);
        ps = conn.prepareStatement(checkLockSQL);
        for (int i = 0; i < lockDOs.size(); i++) {
            ps.setString(i + 1, lockDOs.get(i).getRowKey());
        }
        rs = ps.executeQuery();
        String currentXID = lockDOs.get(0).getXid();
        while (rs.next()) {
            String dbXID = rs.getString(ServerTableColumnsName.LOCK_TABLE_XID);
            if (!StringUtils.equals(dbXID, currentXID)) {
                canLock &= false;
                break;
            }
            dbExistedRowKeys.add(rs.getString(ServerTableColumnsName.LOCK_TABLE_ROW_KEY));
        }

        if (!canLock) {
            conn.rollback();
            return false;
        }
        List<LockDO> unrepeatedLockDOs = null;
        if (CollectionUtils.isNotEmpty(dbExistedRowKeys)) {
            unrepeatedLockDOs = lockDOs.stream().filter(lockDO -> !dbExistedRowKeys.contains(lockDO.getRowKey()))
                .collect(Collectors.toList());
        } else {
            unrepeatedLockDOs = lockDOs;
        }
        if (CollectionUtils.isEmpty(unrepeatedLockDOs)) {
            conn.rollback();
            return true;
        }
        //lock
        if (unrepeatedLockDOs.size() == 1) {
            LockDO lockDO = unrepeatedLockDOs.get(0);
            if (!doAcquireLock(conn, lockDO)) {
                conn.rollback();
                return false;
            }
        } else {
            if (!doAcquireLocks(conn, unrepeatedLockDOs)) {
                conn.rollback();
                return false;
            }
        }
        conn.commit();
        return true;
    } catch (SQLException e) {
        throw new StoreException(e);
    } finally {
        IOUtil.close(rs, ps);
        if (conn != null) {
            try {
                if (originalAutoCommit) {
                    conn.setAutoCommit(true);
                }
                conn.close();
            } catch (SQLException e) {
            }
        }
    }
}
```

上述方法流程:

step1 : 对lockDOS去重

step2:组装sql,进行查询

step3: 比对xid如果不一致则回滚,并获取锁失败

step4:数据库还没有的锁则需要保存锁记录

//todo 锁调用流程



### 1.6 RPC

保证 Seata 高性能的关键之一也是使用了 Netty 作为 RPC 框架，采用默认配置的线程模型如下图所示：

![img](https://pic1.zhimg.com/80/v2-5cf60e84f7496a61fab1d081501a3190_720w.jpg)如果采用默认的基本配置那么会有一个 Acceptor 线程用于处理客户端的链接，会有 cpu*2 数量的 NIO-Thread，再这个线程中不会做业务太重的事情，只会做一些速度比较快的事情，比如编解码，心跳事件和TM注册。一些比较费时间的业务操作将会交给业务线程池，默认情况下业务线程池配置为最小线程为 100，最大为 500。

Seata 的心跳机制，这里是使用 Netty 的 IdleStateHandler 完成的，如下：

`AbstractRpcRemotingServer#start`

```java
ch.pipeline().addLast(new IdleStateHandler(nettyServerConfig.getChannelMaxReadIdleSeconds(), 0, 0))
```

在 Sever 端对于写没有设置最大空闲时间，对于读设置了最大空闲时间，默认为 15s，如果超过 15s 则会将链接断开，关闭资源。

`RpcServer#userEventTriggered`

```java
//该方法重写了netty中的ChannelInboundHandlerAdapter 超时时触发
public void userEventTriggered(ChannelHandlerContext ctx, Object evt) {
    if (evt instanceof IdleStateEvent) {
        IdleStateEvent idleStateEvent = (IdleStateEvent)evt;
        if (idleStateEvent.state() == IdleState.READER_IDLE) {
            handleDisconnect(ctx);
            closeChannelHandlerContext(ctx);
        }
    }
}
```

step1：判断是否是读空闲的检测事件；

step2：如果是则断开链接，关闭资源。

### 1.7 HA-Cluster

目前官方没有公布 HA-Cluster，但是通过一些其他中间件和官方的一些透露，可以将 HA-Cluster 用如下方式设计：

![img](https://pic1.zhimg.com/80/v2-7940428c1ca63e37923933e5c2d57820_720w.jpg)



具体的流程如下：

step1：客户端发布信息的时候根据 TranscationId 保证同一个 Transcation 是在同一个 Master 上，通过多个 Master 水平扩展，提供并发处理性能。

step2：在 Server 端中一个 Master 有多个 Slave，Master 中的数据近实时同步到 Slave上，保证当 Master 宕机的时候，还能有其他 Slave 顶上来可以用。

### 1.8 Metrics & Tracing

这个模块也是一个没有具体公布实现的模块，当然有可能会提供插件口，让其他第三方 metric 接入进来。另外最近 Apache SkyWalking 正在和 Seata 小组商讨如何接入进来。

## 2.Coordinator Core

上面我们讲了很多 Server 基础模块，想必大家对 Seata 的实现已经有个大概，接下来我会讲解事务协调器具体逻辑是如何实现的，让大家更加了解 Seata 的实现内幕。

### 2.1 启动流程

启动方法在 Server 类有个 main 方法，定义了我们启动流程：

```java
public static void main(String[] args) throws IOException {
    //解析启动参数
    ParameterParser parameterParser = new ParameterParser(args);

    //统计管理类加载
    MetricsManager.get().init();
    //获取存储模式
    System.setProperty(ConfigurationKeys.STORE_MODE, parameterParser.getStoreMode());
    //初始化rpcServer,配置相关netty参数
    RpcServer rpcServer = new RpcServer(WORKING_THREADS);
    //设置监听端口
    rpcServer.setListenPort(parameterParser.getPort());
    //设置节点id,初始化UUIDGenerator用于生成各种id
    UUIDGenerator.init(parameterParser.getServerNode());
    //根据存储类型,初始化session管理类
    SessionHolder.init(parameterParser.getStoreMode());
    //创建事务管理者(TC)
    DefaultCoordinator coordinator = new DefaultCoordinator(rpcServer);
    //初始化事务管理者,开启异步线程用于处理回滚,超时,提交等操作
    coordinator.init();
    //设置事务处理器
    rpcServer.setHandler(coordinator);
    // 注册钩子函数,处理线程池关闭
    ShutdownHook.getInstance().addDisposable(coordinator);

    //127.0.0.1 and 0.0.0.0 are not valid here.
    //处理ip + port
    if (NetUtil.isValidIp(parameterParser.getHost(), false)) {
        XID.setIpAddress(parameterParser.getHost());
    } else {
        XID.setIpAddress(NetUtil.getLocalIp());
    }
    XID.setPort(rpcServer.getListenPort());

    //初始化rpc功能
    rpcServer.init();

}
```

主要流程

step1：创建一个 RpcServer，再这个里面包含了我们网络的操作，用 Netty 实现了服务端。

step2：解析端口号和文件地址。

step3：初始化 SessionHoler，其中最重要的重要就是重我们 dataDir 这个文件夹中恢复我们的数据，重建我们的Session。

step4：创建一个CoorDinator,这个也是我们事务协调器的逻辑核心代码，然后将其初始化，其内部初始化的逻辑会创建四个定时任务：

- retryRollbacking：重试 rollback 定时任务，用于将那些失败的 rollback 进行重试的，每隔 5ms 执行一次。
- retryCommitting：重试 commit 定时任务，用于将那些失败的commit 进行重试的，每隔 5ms 执行一次。
- asyncCommitting：异步 commit 定时任务，用于执行异步的commit，每隔 10ms 一次。
- timeoutCheck：超时定时任务检测，用于检测超时的任务，然后执行超时的逻辑，每隔 2ms 执行一次。

step5： 初始化 UUIDGenerator 这个也是我们生成各种 ID(transcationId,branchId) 的基本类。

step6：将本地 IP 和监听端口设置到 XID 中，初始化 rpcServer 等待客户端的连接。

启动流程比较简单，下面我会介绍分布式事务框架中的常见的一些业务逻辑 Seata 是如何处理的。

### 2.2 Begin - 开启全局事务

一次分布式事务的起始点一定是开启全局事务，首先我们看看全局事务 Seata 是如何实现的：

```java
public String begin(String applicationId, String transactionServiceGroup, String name, int timeout)
    throws TransactionException {
    //通过appId,事务组名称,
    GlobalSession session = GlobalSession.createGlobalSession(applicationId, transactionServiceGroup, name,
        timeout);
    session.addSessionLifecycleListener(SessionHolder.getRootSessionManager());

    session.begin();

    //transaction start event
    eventBus.post(new GlobalTransactionEvent(session.getTransactionId(), GlobalTransactionEvent.ROLE_TC,
        session.getTransactionName(), session.getBeginTime(), null, session.getStatus()));

    return session.getXid();
}
```

step1： 根据应用 ID，事务分组，名字，超时时间创建一个 GloabSession，这个再前面也提到过他和 branchSession 分别是什么。

step2：对其添加一个 RootSessionManager 用于监听一些事件,目前定义了4种类型的listener,均实现了`SessionLifecycleListener`接口

- ROOT_SESSION_MANAGER:最全，最大的，拥有所有的 Session
- ASYNC_COMMITTING_SESSION_MANAGER:用于管理需要做异步 commit 的 Session
- RETRY_COMMITTING_SESSION_MANAGER: 用于管理重试 commit 的 Session
- RETRY_ROLLBACKING_SESSION_MANAGER: 用于管理重试回滚的 Session

step3：开启 Globalsession

```java
public void begin() throws TransactionException {
    this.status = GlobalStatus.Begin;
    this.beginTime = System.currentTimeMillis();
    this.active = true;
    for (SessionLifecycleListener lifecycleListener : lifecycleListeners) {
        //实际是将session写入到file或db中
        lifecycleListener.onBegin(this);
    }
}

```

step4：最后返回 XID，这个 XID 是由 ip+port+transactionId 组成的，非常重要，当 TM 申请到之后需要将这个 ID 传到 RM 中，RM 通过 XID 来决定到底应该访问哪一台 Server。

### 2.3 BranchRegister - 分支事务注册

当全局事务在 TM 开启之后，RM 的分支事务也需要注册到全局事务之上，这里看看是如何处理的：

```java
public Long branchRegister(BranchType branchType, String resourceId, String clientId, String xid,
                           String applicationData, String lockKeys) throws TransactionException {
    GlobalSession globalSession = assertGlobalSessionNotNull(xid, false);
    return globalSession.lockAndExcute(() -> {
        if (!globalSession.isActive()) {
            throw new GlobalTransactionException(GlobalTransactionNotActive, String
                .format("Could not register branch into global session xid = %s status = %s",
                    globalSession.getXid(), globalSession.getStatus()));
        }
        //添加全局事务监听器
        globalSession.addSessionLifecycleListener(SessionHolder.getRootSessionManager());
        //创建新的分支session
        BranchSession branchSession = SessionHelper.newBranchByGlobal(globalSession, branchType, resourceId,
            applicationData, lockKeys, clientId);
        if (!branchSession.lock()) {
            throw new BranchTransactionException(LockKeyConflict, String
                .format("Global lock acquire failed xid = %s branchId = %s", globalSession.getXid(),
                    branchSession.getBranchId()));
        }
        try {
            //将分支事务添加到全局事务中
            globalSession.addBranch(branchSession);
        } catch (RuntimeException ex) {
            branchSession.unlock();
        }
        return branchSession.getBranchId();
    });
}
```

step1：通过 transactionId 获取并校验全局事务是否是开启状态。

step2：创建一个新的分支事务，也就是 BranchSession。

step3：对分支事务进行加全局锁，这里的逻辑就是使用锁模块的逻辑。

step4：添加 branchSession，主要是将其添加到 globalSession 对象中，并写入到我们的文件中。

step5：返回 branchId，这个 ID 也很重要，我们后续需要用它来回滚我们的事务，或者对我们分支事务状态更新。

### 2.4 GlobalCommit - 全局提交

当分支事务执行完成之后，就轮到 TM-事务管理器来决定是提交还是回滚，如果是提交，那么就会走到下面的逻辑：

```java
public GlobalStatus commit(String xid) throws TransactionException {
    GlobalSession globalSession = SessionHolder.findGlobalSession(xid);
    if (globalSession == null) {
        return GlobalStatus.Finished;
    }
    globalSession.addSessionLifecycleListener(SessionHolder.getRootSessionManager());
    // just lock changeStatus
    boolean shouldCommit = globalSession.lockAndExcute(() -> {
        //the lock should release after branch commit
        //释放全局锁,关闭全局事务后不再有分支注册
        globalSession
            .closeAndClean(); // Highlight: Firstly, close the session, then no more branch can be registered.
        //修改事务状态
        if (globalSession.getStatus() == GlobalStatus.Begin) {
            globalSession.changeStatus(GlobalStatus.Committing);
            return true;
        }
        return false;
    });
    if (!shouldCommit) {
        return globalSession.getStatus();
    }
    //跟进事务提交属性同步/异步更新
    if (globalSession.canBeCommittedAsync()) {
        asyncCommit(globalSession);
        return GlobalStatus.Committed;
    } else {
        doGlobalCommit(globalSession, false);
    }
    return globalSession.getStatus();
}
```

step1：首先找到 globalSession。如果他为 Null 证明已经被 commit 过了，那么直接幂等操作，返回成功。

step2：关闭 GloabSession 防止再次有新的 branch 进来。

step3：如果 status 是等于 Begin，那么久证明还没有提交过，改变其状态为 Committing 也就是正在提交。

step4：判断是否是可以异步提交，目前只有AT模式可以异步提交，因为是通过 Undolog 的方式去做的。 TCC 需要走同步提交的代码。

step5：如果是异步提交，直接将其放进 ASYNC_COMMITTING_SESSION_MANAGER，让其再后台线程异步去做 step6，如果是同步的那么直接执行 step6。

step6：遍历 BranchSession 进行提交，如果某个分支事务失败，根据不同的条件来判断是否进行重试，异步不需要重试，因为其本身都在 manager 中，只要没有成功就不会被删除会一直重试，如果是同步提交的会放进异步重试队列进行重试。

### 3.5 GlobalRollback - 全局回滚

如果我们的 TM 决定全局回滚，那么会走到下面的逻辑：

```java
public GlobalStatus rollback(String xid) throws TransactionException {
    GlobalSession globalSession = SessionHolder.findGlobalSession(xid);
    if (globalSession == null) {
        return GlobalStatus.Finished;
    }
    globalSession.addSessionLifecycleListener(SessionHolder.getRootSessionManager());
    // just lock changeStatus
    boolean shouldRollBack = globalSession.lockAndExcute(() -> {
        globalSession.close(); // Highlight: Firstly, close the session, then no more branch can be registered.
        if (globalSession.getStatus() == GlobalStatus.Begin) {
            globalSession.changeStatus(GlobalStatus.Rollbacking);
            return true;
        }
        return false;
    });
    if (!shouldRollBack) {
        return globalSession.getStatus();
    }

    doGlobalRollback(globalSession, false);
    return globalSession.getStatus();
}
```

整体过程与commit相似.



参考:[深度剖析一站式分布式事务方案 Seata-Server](https://zhuanlan.zhihu.com/p/61981170)