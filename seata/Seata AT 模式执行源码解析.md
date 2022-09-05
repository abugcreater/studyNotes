# Seata AT 模式执行源码解析

## Seata AT的工作流程

### 工作流程总览

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQjcrbDZMg2J2kCSIjcBcw8pcWdibq5iaMicj0AALib1yibt72ZIxr94abS3gA/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

AT模式的工作流程分为**两阶段**.以极端进行业务SQL执行,并通过SQL拦截,SQL改写等过程生成修改数据前后的快照,并作为UndoLog和业务修改在**同一个本地事务汇中提交**.

如果一阶段成功那么二阶段仅仅异步删除刚刚插入的UndoLog;如果二阶段失败则通过UndoLog生成反向SQL语句回滚一阶段的数据修改.**其中关键的 SQL 解析和拼接工作借助了 Druid Parser 中的代码，这部分本文并不涉及，感兴趣的小伙伴可以去翻看源码，并不是很复杂**。

### **图解 AT 模式一阶段流程**

一阶段分支事务的具体工作有:

1. 跟进需要执行的SQL类型生成对应的`SqlRecognizer`
2. 进而生成相应的`SqlExecutor`
3. 然后进入核心逻辑查询数据的前后快照,拿到修改数据前后的快照之后,将两者整合生成UndoLog,并尝试将其和业务修改在同一个事务中提交

整个流程图如下:

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQjDUA5PP5QibhiaOUqeg3ajZia6AGl0JE724o6XV3jGWXLC9SNcTYSv2VZw/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

**本地事务提交前必须想服务端注册分支,**分支注册信息中包含由表名和行主键组成的全局锁,如果分支注册过程中发现全局锁正在被其他全局事务锁定则抛出全局锁冲突异常,客户端需要循环等待,知道其他全局事务释放锁之后该本地事务才能提交.**Seata 以这样的机制保证全局事务间的写隔离。**

### **图解二阶段 Commit 流程**

一阶段全部提交成功后,TM会向TC请求提交这个全局事务,服务端根据`xid`查询该全局事务加锁并关闭这个全局事务,**目的时防止该事务后续还有分支继续注册上来**.同时将其状态从`begin`修改为`committing`

紧接着,判断全局事务的分支类型是否均为`AT`类型,符合要求则服务端**异步提交**,因为`AT`模式下一阶段完成数据已经落地.服务端仅仅修改全局事务状态为 `AsyncCommitting`，然后会有一个定时线程池存储介质中查询出待提交的全局事务日志进行提交，如果全局事务提交成功则会释放全局锁并删除事务日志。整个流程如下图所示：

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQj2rRUDYiaKRLbEyN8xz8W41ssic6C6SKauXcXFacdQ1xZrLnmx5aMiaCJA/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

对客户端来说，先是接收到服务端发送的 `branch commit` 请求，然后客户端会根据 `resourceId` 找到相应的 `ResourceManager`，接着将分支提交请求封装成 `Phase2Context` 插入内存队列 `ASYNC_COMMIT_BUFFER`，客户端会有一个定时线程池去查询该队列进行 `UndoLog` 的异步删除。

一旦客户端提交失败或者RPC超时,则服务端会将该全局事务状态置为 `CommitRetrying`，之后由另一个定时线程池去一直重试.

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQjfupuxxicuSNXVpvYRibAsVwp4NAJ0gZTTpTSYRrkzFic3aibkLcnXnp1iaw/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

### **图解二阶段 Rollback 流程**

TM一阶段异常会向TC请求回滚全局事务,服务端会根据xid查询出这个全局事务,加锁关闭事务使得后续不会再有分支注册上来,并同时更改状态`Begin` 为 `Rollbacking`，接着进行**同步回滚**以保证数据一致性。除了同步回滚这个点外，其他流程同提交时相似，如果同步回滚成功则释放全局锁并删除事务日志，如果失败则会进行异步重试。整个流程如下图所示:

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQjfKWibbOScqm9Dk7IS6WiczYbVUfLicXMj9vv4bRib1fcF4f7GEkia1E6dlA/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

客户端接收到服务端的 `branch rollback` 请求，先根据 `resourceId` 拿到对应的数据源代理，然后根据 `xid` 和 `branchId` 查询出 `UndoLog` 记录，反序列化其中的 `rollback` 字段拿到数据的前后快照，我们称该全局事务为 `A`。

根据具体 `SQL` 类型生成对应的 `UndoExecutor`，校验一下数据 `UndoLog` 中的前后快照是否一致或者前置快照和当前数据（这里需要 `SELECT` 一次）是否一致，如果一致说明**不需要做回滚操作**，如果不一致则生成反向 `SQL` 进行补偿，在提交本地事务前会检测获取数据库本地锁是否成功，如果失败则说明存在其他全局事务（假设称之为 `B`）的一阶段正在修改相同的行，但是由于这些行的主键在服务端已经被当前正在执行二阶段回滚的全局事务 `A` 锁定，因此事务 B 的一阶段在本地提交前尝试获取全局锁一定是失败的，等到获取全局锁超时后全局事务 `B` 会释放本地锁，这样全局事务 `A` 就可以继续进行本地事务的提交，成功之后删除本地 `UndoLog` 记录。整个流程如下图所示：

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQjR1ic45sZrYyLJL1Rk17Cibacq2gibx8dGHic351JicTnsXibCmJo6G7Wia6dg/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)





## Seata AT 模式客户端部分

`Seata` 中主要针对 `java.sql` 包下的 `DataSource`、`Connection`、`Statement`、`PreparedStatement` 四个接口进行了再包装，包装类分别为 `DataSourceProxy`、`ConnectionProxy`、`StatementProxy`、`PreparedStatementProxy`，很好一一对印，其功能是在 `SQL` 语句执行前后、事务 `commit` 或者 `rollbakc` 前后进行一些与 `Seata` 分布式事务相关的操作，例如**分支注册、状态回报、全局锁查询、快照存储、反向 SQL 生成**等.

AT模式需要在spring汇总注入`DataSourceProxy bean`对象,然后由该对象生成`ConnectionProxy`,然后由`ConnectionProxy`生成`Statement`代理对象.

```java
@Bean
public DataSourceProxy dataSourceProxy(DataSource dataSource) {
    //数据源代理换成DataSourceProxyXA 就是XA模式
    return new DataSourceProxy(dataSource);
}
```

### ExecuteTemplate 类的 execute 方法

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQjfjqRCekZeTl7eJG2XU1g0IbBgicNYksYL55DuCj1AZBM1C0SmMzdiaWg/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

​	AT模式下,真正的分支事务是在`StatementProxy`、`PreparedStatementProxy`的`execute`、`executeQuery`、`executeUpdate` 等**具体执行方法**中，这些方法均实现自 `Statement` 和 `PreparedStatement` 的标准接口，而方法体内调用了 `ExecuteTemplate.execute` 做**方法拦截**

调用链 `StatementProxy#executeUpdate` -> `ExecuteTemplate#execute`

ExecuteTemplate.execute源码如下:

```java
public static <T, S extends Statement> T execute(SQLRecognizer sqlRecognizer,
                                                 StatementProxy<S> statementProxy,
                                                 StatementCallback<T, S> statementCallback,
                                                 Object... args) throws SQLException {
	
    if (!RootContext.inGlobalTransaction() && !RootContext.requireGlobalLock()) {
        // Just work as original statement
        return statementCallback.execute(statementProxy.getTargetStatement(), args);
    }
	//一般sqlRecognizer默认为空,需要通过sql解析
    if (sqlRecognizer == null) {
        sqlRecognizer = SQLVisitorFactory.get(
                statementProxy.getTargetSQL(),
                statementProxy.getConnectionProxy().getDbType());
    }
    Executor<T> executor = null;
    if (sqlRecognizer == null) {
        executor = new PlainExecutor<T, S>(statementProxy, statementCallback);
    } else {
        //根据不同的sql类型选择不同的执行器
        switch (sqlRecognizer.getSQLType()) {
            case INSERT:
                executor = new InsertExecutor<T, S>(statementProxy, statementCallback, sqlRecognizer);
                break;
            case UPDATE:
                executor = new UpdateExecutor<T, S>(statementProxy, statementCallback, sqlRecognizer);
                break;
            case DELETE:
                executor = new DeleteExecutor<T, S>(statementProxy, statementCallback, sqlRecognizer);
                break;
            case SELECT_FOR_UPDATE:
                executor = new SelectForUpdateExecutor<T, S>(statementProxy, statementCallback, sqlRecognizer);
                break;
            default:
                executor = new PlainExecutor<T, S>(statementProxy, statementCallback);
                break;
        }
    }
    T rs = null;
    try {
        //调用执行器的excute方法,使用模版模式,通用代码在BaseTransactionalExecutor类中实现,定制代码(before image/after image)在各个执行器中自定义
        rs = executor.execute(args);
    } catch (Throwable ex) {
        if (!(ex instanceof SQLException)) {
            // Turn other exception into SQLException
            ex = new SQLException(ex);
        }
        throw (SQLException)ex;
    }
    return rs;
}
```

#### **执行器接口 execute 的实现**

`execute` 方法的实现位于 `BaseTransactionalExecutor` 类中,主要用于全局事务属性设置：

```java
public Object execute(Object... args) throws Throwable {
    if (RootContext.inGlobalTransaction()) {//如果处于全局事务中
        //将xid与当前连接绑定
        String xid = RootContext.getXID();
        statementProxy.getConnectionProxy().bind(xid);
    }
	//设置全局锁标志位,获取全局锁,全局锁在第二阶段完成后被释放
    if (RootContext.requireGlobalLock()) {
        statementProxy.getConnectionProxy().setGlobalLockRequire(true);
    } else {
        statementProxy.getConnectionProxy().setGlobalLockRequire(false);
    }
    return doExecute(args);
}
```

#### **抽象方法 doExecute 的实现**

`AbstractDMLBaseExecutor`中实现了`doExecute `方法,根据是否自动提交,调用相关方法:

```java
public T doExecute(Object... args) throws Throwable {
    AbstractConnectionProxy connectionProxy = statementProxy.getConnectionProxy();
    if (connectionProxy.getAutoCommit()) {
        return executeAutoCommitTrue(args);
    } else {
        return executeAutoCommitFalse(args);
    }
}
```

首先查看executeAutoCommitFalse方法,该方法执行逻辑是,先保存前置镜像,然后执行sql语句,再保存后置镜像,最后将两个镜像合并存入undoLog中:

```java
protected T executeAutoCommitFalse(Object[] args) throws Exception {
    //获取beforeImage,有执行器自定义
    TableRecords beforeImage = beforeImage();
    //执行sql
    T result = statementCallback.execute(statementProxy.getTargetStatement(), args);
    //获取afterImage,同beforeImage
    TableRecords afterImage = afterImage(beforeImage);
    //保存到undoLog,此时会根据sql类型和数据获取局部锁
    prepareUndoLog(beforeImage, afterImage);
    return result;
}
```

`executeAutoCommitFalse`,`ConnectionProxy`会commit信息.

之后查看`executeAutoCommitTrue`,该方法实际实现依赖于`executeAutoCommitFalse`

```java
protected T executeAutoCommitTrue(Object[] args) throws Throwable {
    AbstractConnectionProxy connectionProxy = statementProxy.getConnectionProxy();
    try {
        //首先设置为非自动提交
        connectionProxy.setAutoCommit(false);
        return new LockRetryPolicy(connectionProxy.getTargetConnection()).execute(() -> {
            //实际执行代码,执行executeAutoCommitFalse方法,然后提交
            T result = executeAutoCommitFalse(args);
            connectionProxy.commit();
            return result;
        });
    } catch (Exception e) {
        // when exception occur in finally,this exception will lost, so just print it here
        LOGGER.error("execute executeAutoCommitTrue error:{}", e.getMessage(), e);
        if (!LockRetryPolicy.isLockRetryPolicyBranchRollbackOnConflict()) {
            connectionProxy.getTargetConnection().rollback();
        }
        throw e;
    } finally {
        ((ConnectionProxy) connectionProxy).getContext().reset();
        connectionProxy.setAutoCommit(true);
    }
}
```

#### **ConnectionProxy 复写的 commit 方法**

该 `commit` 方法实现自 `Connection` 接口的 `commit` 方法：

```java
private void doCommit() throws SQLException {
    if (context.inGlobalTransaction()) {
        //针对分支事务
        processGlobalTransactionCommit();
    } else if (context.isGlobalLockRequire()) {
        //针对全局锁
        processLocalCommitWithGlobalLocks();
    } else {
        //没有全局事务或者全局锁直接提交
        targetConnection.commit();
    }
}
```

#### **执行一阶段本地事务提交**

如果是分支事务，调用 `processGlobalTransactionCommit` 方法进行提交：

```java
private void processGlobalTransactionCommit() throws SQLException {
    try {
        //向TC注册分支事务
        register();
    } catch (TransactionException e) {
        //发生异常根据类型区分并向上抛出异常
        recognizeLockKeyConflictException(e, context.buildLockKeys());
    }

    try {
        if (context.hasUndoLog()) {//如果注册正常,且有undolog信息
           //将undolog插入数据库中
            UndoLogManagerFactory.getUndoLogManager(this.getDbType()).flushUndoLogs(this);
        }
        //提交分支事务
        targetConnection.commit();
    } catch (Throwable ex) {
        LOGGER.error("process connectionProxy commit error: {}", ex.getMessage(), ex);
        //向TC报告事务信息
        report(false);
        throw new SQLException(ex);
    }
    if (IS_REPORT_SUCCESS_ENABLE) {
        report(true);
    }
    //重置连接信息
    context.reset();
}
```

#### **GlobalLock 的具体作用**

如果是用 `GlobalLock` 修饰的本地业务方法，虽然该方法并非某个全局事务下的分支事务，但是它对数据资源的操作也需要先查询全局锁，如果存在其他 `Seata` 全局事务正在修改，则该方法也需等待。所以，如果想要 `Seata` 全局事务执行期间，数据库不会被其他事务修改，则该方法需要强制添加 `GlobalLock` 注解，来将其纳入 `Seata` 分布式事务的管理范围.

功能有点类似于 `Spring` 的 `@Transactional` 注解，如果你希望开启事务，那么必须添加该注解，如果你没有添加那么事务功能自然不生效，业务可能出 `BUG`；`Seata` 也一样，如果你希望某个不在全局事务下的 `SQL` 操作不影响 `AT` 分布式事务，那么必须添加 `GlobalLock` 注解。

```java
private void processLocalCommitWithGlobalLocks() throws SQLException {
	//向TC请求全局锁信息,校验是否有锁冲突
    checkLock(context.buildLockKeys());
    try {
        //提交
        targetConnection.commit();
    } catch (Throwable ex) {
        throw new SQLException(ex);
    }
    //清空全局事务信息
    context.reset();
}    
```

#### xid上下游传递

seata调用下游服务时,会在header中设置xid值.下游通过拦截器,重写preHandle方法,将头部的xid解析后存放到ContextCore中.

```java
public class SeataHandlerInterceptor implements HandlerInterceptor{

//重写preHandle方法,实现xid传递
@Override
public boolean preHandle(HttpServletRequest request, HttpServletResponse response,
      Object handler) {

   String xid = RootContext.getXID();
   String rpcXid = request.getHeader(RootContext.KEY_XID);
   if (xid == null && rpcXid != null) {
      RootContext.bind(rpcXid);
      if (log.isDebugEnabled()) {
         log.debug("bind {} to RootContext", rpcXid);
      }
   }
   return true;
}
}
```



#### **二阶段异步删除分支 UndoLog**

如果一阶段成功，则 `TC` 会通知客户端 `RM` 进行第二阶段的提交工作，这部分代码最终实现位于 `AsyncWorker` 类中的 `branchCommit` 方法:

```java
public BranchStatus branchCommit(BranchType branchType, String xid, long branchId, String resourceId, String applicationData) throws TransactionException {
    //将分支提交信息包装成Phase2Context加入异步提交队列ASYNC_COMMIT_BUFFER
    if (!ASYNC_COMMIT_BUFFER.offer(new Phase2Context(branchType, xid, branchId, resourceId, applicationData))) {
		//队列已满,打印log	
    }
    return BranchStatus.PhaseTwo_Committed;
}
```

插入 `ASYNC_COMMIT_BUFFER` 之后，`AsyncWorker` 类中会有一个定时任务，从队列中取出分支提交信息 `Phase2Context`，将其中的 `xid` 和 `branchId` 提取出来生成 `DELETE SQL` 语句，删除本地数据库中存储的相应的 `UndoLog`。下面是该定时任务的关键方法 `doBranchCommits` 的实现：

```java
public synchronized void init() {
    timerExecutor = new ScheduledThreadPoolExecutor(1,
            new NamedThreadFactory("AsyncWorker", 1, true));
    timerExecutor.scheduleAtFixedRate(new Runnable() {
        @Override
        public void run() {
            //初始化时创建定时任务每秒执行doBranchCommits方法
            doBranchCommits();
        }
    }, 10, 1000 * 1, TimeUnit.MILLISECONDS);
}

private void doBranchCommits() {
    if (ASYNC_COMMIT_BUFFER.size() == 0) {
        return;
    }
	//保存resourceId和commitContext列表的对应关系
    Map<String, List<Phase2Context>> mappedContexts = new HashMap<>(DEFAULT_RESOURCE_SIZE);
    while (!ASYNC_COMMIT_BUFFER.isEmpty()) {
        Phase2Context commitContext = ASYNC_COMMIT_BUFFER.poll();
        //取出分支提交信息
        List<Phase2Context> contextsGroupedByResourceId = mappedContexts.get(commitContext.resourceId);
        if (contextsGroupedByResourceId == null) {
            contextsGroupedByResourceId = new ArrayList<>();
            mappedContexts.put(commitContext.resourceId, contextsGroupedByResourceId);
        }
        //将分支提交信息保存到contextsGroupedByResourceId list中
        contextsGroupedByResourceId.add(commitContext);

    }
	//遍历关系map
    for (Map.Entry<String, List<Phase2Context>> entry : mappedContexts.entrySet()) {
        Connection conn = null;
        DataSourceProxy dataSourceProxy;
        try {
            try {
                //获取资源管理器
                DataSourceManager resourceManager = (DataSourceManager) DefaultResourceManager.get().getResourceManager(BranchType.AT);
                //通过资源id获取代理数据源
                dataSourceProxy = resourceManager.get(entry.getKey());
                //获取连接
                conn = dataSourceProxy.getPlainConnection();
            } catch (SQLException sqle) {

                continue;
            }
            List<Phase2Context> contextsGroupedByResourceId = entry.getValue();
            Set<String> xids = new LinkedHashSet<>(UNDOLOG_DELETE_LIMIT_SIZE);
            Set<Long> branchIds = new LinkedHashSet<>(UNDOLOG_DELETE_LIMIT_SIZE);
            for (Phase2Context commitContext : contextsGroupedByResourceId) {
                xids.add(commitContext.xid);
                branchIds.add(commitContext.branchId);
                int maxSize = xids.size() > branchIds.size() ? xids.size() : branchIds.size();
                //满1000条批量删除
                if(maxSize == UNDOLOG_DELETE_LIMIT_SIZE){
                    try {
                        UndoLogManagerFactory.getUndoLogManager(dataSourceProxy.getDbType()).batchDeleteUndoLog(xids, branchIds, conn);
                    } catch (Exception ex) {
                    }
                    xids.clear();
                    branchIds.clear();
                }
            }

            if (CollectionUtils.isEmpty(xids) || CollectionUtils.isEmpty(branchIds)) {
                return;
            }

            try {
 //没有到1000条的批量删除              
                UndoLogManagerFactory.getUndoLogManager(dataSourceProxy.getDbType()).batchDeleteUndoLog(xids, branchIds, conn);
            }catch (Exception ex) {
            }

        } finally {
               conn.close();
            
        }
    }
}
```

#### **二阶段生成反向 SQL 回滚**

如果一阶段失败，则二阶段需要回滚一阶段的数据库更新操作，此时涉及到根据 `UndoLog`构造逆向 `SQL` 进行补偿。这部分逻辑的入口位于 `DataSourceManager` 类中的 `branchRollback` 方法：

```java
public BranchStatus branchRollback(BranchType branchType, String xid, long branchId, String resourceId,
                                   String applicationData) throws TransactionException {
    //根据资源id获取dataSourceProxy
    DataSourceProxy dataSourceProxy = get(resourceId);
    if (dataSourceProxy == null) {
        throw new ShouldNeverHappenException();
    }
    try {
       //调用UndoLogManager的undo方法进行补偿,核心逻辑
        UndoLogManagerFactory.getUndoLogManager(dataSourceProxy.getDbType()).undo(dataSourceProxy, xid, branchId);
    } catch (TransactionException te) {
        if (te.getCode() == TransactionExceptionCode.BranchRollbackFailed_Unretriable) {
            return BranchStatus.PhaseTwo_RollbackFailed_Unretryable;
        } else {
            return BranchStatus.PhaseTwo_RollbackFailed_Retryable;
        }
    }
    return BranchStatus.PhaseTwo_Rollbacked;

}
```

`UndoLogManager` 负责` UndoLog `的插入、删除、补偿等操作，其中核心方法即为 `undo`,一旦当前事务进行二阶段回滚时获取本地锁失败，则进入循环等待逻辑，等待本地锁被释放之后自己再提交本地事务：

```java
public void undo(DataSourceProxy dataSourceProxy, String xid, long branchId) throws TransactionException {
    Connection conn = null;
    ResultSet rs = null;
    PreparedStatement selectPST = null;
    boolean originalAutoCommit = true;

    for (; ; ) {
        try {
            conn = dataSourceProxy.getPlainConnection();

            // The entire undo process should run in a local transaction.
            if (originalAutoCommit = conn.getAutoCommit()) {
                conn.setAutoCommit(false);
            }

            // Find UNDO LOG ,根据branchId,xid获取undolog日志
            selectPST = conn.prepareStatement(SELECT_UNDO_LOG_SQL);
            selectPST.setLong(1, branchId);
            selectPST.setString(2, xid);
            rs = selectPST.executeQuery();

            boolean exists = false;
            while (rs.next()) {
                exists = true;
                //状态不支持回滚则直接返回,导致原因可能是服务端重复发送
                int state = rs.getInt(ClientTableColumnsName.UNDO_LOG_LOG_STATUS);
                if (!canUndo(state)) {
                    return;
                }

                String contextString = rs.getString(ClientTableColumnsName.UNDO_LOG_CONTEXT);
                Map<String, String> context = parseContext(contextString);
                Blob b = rs.getBlob(ClientTableColumnsName.UNDO_LOG_ROLLBACK_INFO);
                byte[] rollbackInfo = BlobUtils.blob2Bytes(b);
				//获取对应的序列化方式,为空就是默认的Jackson
                String serializer = context == null ? null : context.get(UndoLogConstants.SERIALIZER_KEY);
                UndoLogParser parser = serializer == null ? UndoLogParserFactory.getInstance() :
                        UndoLogParserFactory.getInstance(serializer);
                BranchUndoLog branchUndoLog = parser.decode(rollbackInfo);

                try {
                    // put serializer name to local
                    setCurrentSerializer(parser.getName());
                    //解析前后镜像
                    List<SQLUndoLog> sqlUndoLogs = branchUndoLog.getSqlUndoLogs();
					//多条undo需要从后往前执行
                    if (sqlUndoLogs.size() > 1) {
                        Collections.reverse(sqlUndoLogs);
                    }
                    for (SQLUndoLog sqlUndoLog : sqlUndoLogs) {
                        //获取表信息
                        TableMeta tableMeta = TableMetaCacheFactory.getTableMetaCache(dataSourceProxy).getTableMeta(dataSourceProxy, sqlUndoLog.getTableName());
                        sqlUndoLog.setTableMeta(tableMeta);
                        //构造反向SQL,根据不同的数据库和sql类型构造
                        AbstractUndoExecutor undoExecutor = UndoExecutorFactory.getUndoExecutor(
                                dataSourceProxy.getDbType(),
                                sqlUndoLog);
                        //执行
                        undoExecutor.executeOn(conn);
                    }
                } finally {
                    // remove serializer name
                    removeCurrentSerializer();
                }
            }


			//如果undolog存在,说明一阶段提交成功,可以直接清除
            if (exists) {
                deleteUndoLog(xid, branchId, conn);
                conn.commit();
            } else {
                //如果不存在,说明一阶段异常,可以插入一条事务终止日志,防止超时补偿或者悬挂
                insertUndoLogWithGlobalFinished(xid, branchId, UndoLogParserFactory.getInstance(), conn);
                conn.commit();
            }

            return;
        } catch (SQLIntegrityConstraintViolationException e) {
            // Possible undo_log has been inserted into the database by other processes, retrying rollback undo_log

        } catch (Throwable e) {
            if (conn != null) {
                try {
                    conn.rollback();
                } catch (SQLException rollbackEx) {
                    LOGGER.warn("Failed to close JDBC resource while undo ... ", rollbackEx);
                }
            }
            throw new BranchTransactionException(BranchRollbackFailed_Retriable,
                    String.format("Branch session rollback failed and try again later xid = %s branchId = %s %s", xid, branchId, e.getMessage()), e);

        } finally {
            try {
                if (rs != null) {
                    rs.close();
                }
                if (selectPST != null) {
                    selectPST.close();
                }
                if (conn != null) {
                    if (originalAutoCommit) {
                        conn.setAutoCommit(true);
                    }
                    conn.close();
                }
            } catch (SQLException closeEx) {
                
            }
        }
    }
}
```

`UndoExecutorFactory` 类的 `getUndoExecutor` 方法会根据 `UndoLog` 中记录的 `SQLType` 生成不同的 `UndoExecutor` 返回：

```java
public static AbstractUndoExecutor getUndoExecutor(String dbType, SQLUndoLog sqlUndoLog) {
    if (!dbType.equalsIgnoreCase(JdbcConstants.MYSQL)&&!dbType.equalsIgnoreCase(JdbcConstants.ORACLE)) {
        throw new NotSupportYetException(dbType);
    }
    //根据数据库类型和sql类型创建不同的执行器
      if(dbType.equalsIgnoreCase(JdbcConstants.MYSQL)) {
        switch (sqlUndoLog.getSqlType()) {
              case INSERT:
                  return new MySQLUndoInsertExecutor(sqlUndoLog);
              case UPDATE:
                  return new MySQLUndoUpdateExecutor(sqlUndoLog);
              case DELETE:
                  return new MySQLUndoDeleteExecutor(sqlUndoLog);
              default:
                  throw new ShouldNeverHappenException();         
        }
    } 
}
```

`AbstractUndoExecutor#executeOn`使用模版模式,其中`buildUndoSQL`和`getUndoRows`方法由子类扩展

```java
public void executeOn(Connection conn) throws SQLException {

    if (IS_UNDO_DATA_VALIDATION_ENABLE && !dataValidationAndGoOn(conn)) {
        return;
    }
		//生成sql
        String undoSQL = buildUndoSQL();

        PreparedStatement undoPST = conn.prepareStatement(undoSQL);
		//根据语句获取对应的前/后镜像
        TableRecords undoRows = getUndoRows();

        for (Row undoRow : undoRows.getRows()) {
            ArrayList<Field> undoValues = new ArrayList<>();
            Field pkValue = null;
            for (Field field : undoRow.getFields()) {
                if (field.getKeyType() == KeyType.PrimaryKey) {
                    pkValue = field;
                } else {
                    undoValues.add(field);
                }
            }

            undoPrepare(undoPST, undoValues, pkValue);

            undoPST.executeUpdate();
        }


}
```

以`MySQLUndoDeleteExecutor`为例,如果删除失败则必须要insert一条记录.

```java
protected String buildUndoSQL() {
    KeywordChecker keywordChecker = KeywordCheckerFactory.getKeywordChecker(JdbcConstants.MYSQL);
    //根据beforeImage解析数据,生成insert语句
    TableRecords beforeImage = sqlUndoLog.getBeforeImage();
    List<Row> beforeImageRows = beforeImage.getRows();
    if (beforeImageRows == null || beforeImageRows.size() == 0) {
        throw new ShouldNeverHappenException("Invalid UNDO LOG");
    }
    Row row = beforeImageRows.get(0);
    List<Field> fields = new ArrayList<>(row.nonPrimaryKeys());
    Field pkField = row.primaryKeys().get(0);
    // PK is at last one.
    fields.add(pkField);

    String insertColumns = fields.stream()
        .map(field -> keywordChecker.checkAndReplace(field.getName()))
        .collect(Collectors.joining(", "));
    String insertValues = fields.stream().map(field -> "?")
        .collect(Collectors.joining(", "));

    return String.format(INSERT_SQL_TEMPLATE, keywordChecker.checkAndReplace(sqlUndoLog.getTableName()),
                         insertColumns, insertValues);
}
```

### **Seata AT 模式服务端部分**

`AT` 模式下，全局事务注册、提交、回滚均和 `TCC` 模式一模一样，均是根据一阶段调用抛不抛异常决定。

区别在于两点：

1. 分支事务的注册，`TCC` 模式下分支事务是在进入参与方 `Try` 方法之前的切面中注册的，而且分支实现完毕不需要再次汇报分支状态；但 `AT` 模式不一样，分支事务是在代理数据源提交本地事务之前注册的，注册成功才能提交一阶段本地事务，如果注册失败报锁冲突则一直阻塞等待直到该全局锁被释放，且本地提交之后不论是否成功还需要再次向 `TC` 汇报一次分支状态。
2. `AT` 模式由于一阶段已经完成数据修改，因此二阶段可以异步提交，但回滚是同步的，回滚失败才会异步重试；但是 `Seata` 中 `TCC` 模式二阶段 `Confirm` 是同步提交的，可以最大程度保证 `TCC` 模式的数据一致性，但是笔者认为在要求性能的场景下，`TCC`的二阶段也可以改为异步提交

#### **服务端提交全局事务**

核心方法是 `DefaultCore` 类中的 `commit` 方法：

```java
public GlobalStatus commit(String xid) throws TransactionException {
   	//获取全局事务
    GlobalSession globalSession = SessionHolder.findGlobalSession(xid);
    if (globalSession == null) {
        return GlobalStatus.Finished;
    }
    globalSession.addSessionLifecycleListener(SessionHolder.getRootSessionManager());
    // 锁住全局事务并关闭,不让后续分支提交
    boolean shouldCommit = globalSession.lockAndExcute(() -> {
        globalSession
            .closeAndClean();
        //修改全局事务状态
        if (globalSession.getStatus() == GlobalStatus.Begin) {
            globalSession.changeStatus(GlobalStatus.Committing);
            return true;
        }
        return false;
    });
    if (!shouldCommit) {
        return globalSession.getStatus();
    }
    //根据设置调用同步/异步提价方法
    if (globalSession.canBeCommittedAsync()) {
        //添加到DefaultSessionManager的sessionMap中
        asyncCommit(globalSession);
        return GlobalStatus.Committed;
    } else {
        doGlobalCommit(globalSession, false);
    }
    return globalSession.getStatus();
}
```

#### **服务端异步提交分支事务**

`DefaultCoordinator` 类中有一个 `asyncCommitting` 定时线程池，会定时调用 `handleAsyncCommitting` 方法从存储介质（文件或者数据库）中分批查询出状态为 `AsyncCommitting` 的全局事务列表，针对每个全局事务调用 `doGlobalCommit` 方法提交其下所有未提交的分支事务。

```java
public void init() {
    //每秒执行handleAsyncCommitting
        asyncCommitting.scheduleAtFixedRate(() -> {
            try {
                handleAsyncCommitting();
            } catch (Exception e) {
                LOGGER.info("Exception async committing ... ", e);
            }
        }, 0, ASYN_COMMITTING_RETRY_PERIOD, TimeUnit.MILLISECONDS);
}
```

```java
protected void handleAsyncCommitting() {
    //获取所有注册的全局事务
    Collection<GlobalSession> asyncCommittingSessions = SessionHolder.getAsyncCommittingSessionManager()
        .allSessions();
    
    if (CollectionUtils.isEmpty(asyncCommittingSessions)) {
        return;
    }
    for (GlobalSession asyncCommittingSession : asyncCommittingSessions) {
        try {
           
            if (GlobalStatus.AsyncCommitting != asyncCommittingSession.getStatus()) {
                continue;
            }
            asyncCommittingSession.addSessionLifecycleListener(SessionHolder.getRootSessionManager());
            //提交所有分支事务
            core.doGlobalCommit(asyncCommittingSession, true);
        } catch (TransactionException ex) {
                ex.getMessage(), ex);
        }
    }
}
```

#### **服务端同步回滚分支事务**

一旦一阶段失败，全局事务发起方通知 `TC` 回滚全局事务的话，那么二阶段的回滚调用是同步进行的，一旦同步回滚失败才会进入异步重试阶段。核心方法为 `DefaultCore` 类中的 `doGlobalRollback` 方法：

```java
public void doGlobalRollback(GlobalSession globalSession, boolean retrying) throws TransactionException {
    //start rollback event,主要用于统计
    eventBus.post(new GlobalTransactionEvent(globalSession.getTransactionId(), GlobalTransactionEvent.ROLE_TC,
        globalSession.getTransactionName(), globalSession.getBeginTime(), null, globalSession.getStatus()));
		//遍历分支事务
        for (BranchSession branchSession : globalSession.getReverseSortedBranches()) {
            BranchStatus currentBranchStatus = branchSession.getStatus();
            if (currentBranchStatus == BranchStatus.PhaseOne_Failed) {
                globalSession.removeBranch(branchSession);
                continue;
            }
            try {
                //通知客户端RM进行分支回滚
                BranchStatus branchStatus = resourceManagerInbound.branchRollback(branchSession.getBranchType(),
                    branchSession.getXid(), branchSession.getBranchId(), branchSession.getResourceId(),
                    branchSession.getApplicationData());

                switch (branchStatus) {
                    case PhaseTwo_Rollbacked:
                        globalSession.removeBranch(branchSession);
                        LOGGER.info("Successfully rollback branch xid={} branchId={}", globalSession.getXid(),
                            branchSession.getBranchId());
                        continue;
                    case PhaseTwo_RollbackFailed_Unretryable:
                        SessionHelper.endRollbackFailed(globalSession);
                        LOGGER.info("Failed to rollback branch and stop retry xid={} branchId={}",
                            globalSession.getXid(), branchSession.getBranchId());
                        return;
                    default:
                        LOGGER.info("Failed to rollback branch xid={} branchId={}", globalSession.getXid(),
                            branchSession.getBranchId());
                        if (!retrying) {
                            queueToRetryRollback(globalSession);
                        }
                        return;

                }
            } catch (Exception ex) {
                ...
        }

        //In db mode, there is a problem of inconsistent data in multiple copies, resulting in new branch
        // transaction registration when rolling back.
        //1. New branch transaction and rollback branch transaction have no data association
        //2. New branch transaction has data association with rollback branch transaction
        //The second query can solve the first problem, and if it is the second problem, it may cause a rollback
        // failure due to data changes.
        GlobalSession globalSessionTwice = SessionHolder.findGlobalSession(globalSession.getXid());
        if (globalSessionTwice != null && globalSessionTwice.hasBranch()) {
            LOGGER.info("Global[{}] rollbacking is NOT done.", globalSession.getXid());
            return;
        }
    
	//回调成功,释放全局锁并删除日志
    SessionHelper.endRollbacked(globalSession);

    //rollbacked event
    eventBus.post(new GlobalTransactionEvent(globalSession.getTransactionId(), GlobalTransactionEvent.ROLE_TC,
        globalSession.getTransactionName(), globalSession.getBeginTime(), System.currentTimeMillis(),
        globalSession.getStatus()));


}
```

回滚的异步重试与异步提交相同，都是一个定时线程池去扫描存储介质中尚未完成回滚的全局事务。

### **Seata AT 模式的全局锁**

#### **全局锁的组成和作用**

全局锁主要由表名加操作行的主键两个部分组成，`Seata AT` 模式使用服务端保存全局锁的方法保证：

1. 全局事务之前的写隔离
2. 全局事务与被 `GlobalLock` 修饰方法间的写隔离性

#### **全局锁的注册**

当客户端在进行一阶段本地事务提交前，会先向服务端注册分支事务，此时会将修改行的表名、主键信息封装成全局锁一并发送到服务端进行保存，如果服务端保存时发现已经存在其他全局事务锁定了这些行主键，则抛出全局锁冲突异常，客户端循环等待并重试。

#### **全局锁的查询**

被 @`GlobalLock` 修饰的方法虽然不在某个全局事务下，但是其在提交事务前也会进行全局锁查询，如果发现全局锁正在被其他全局事务持有，则自身也会循环等待。

#### **全局锁的释放**

由于二阶段提交是异步进行的，当服务端向客户端发送 `branch commit` 请求后，客户端仅仅是将分支提交信息插入内存队列即返回，服务端只要判断这个流程没有异常就会释放全局锁。因此，可以说如果一阶段成功则在二阶段一开始就会释放全局锁，不会锁定到二阶段提交流程结束。

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/RpldnLMp99hLzDENlozs9ib657nEscLQjjrpfDadYUanokiblqjW9IOcQM5fEzqtYq5BT6MXDJBGibeobVbKZaacA/640?wx_fmt=jpeg&wxfrom=5&wx_lazy=1&wx_co=1)

但是如果一阶段失败二阶段进行回滚，则由于回滚是同步进行的，全局锁直到二阶段回滚完成才会被释放。



参考:[Seata AT 模式分布式事务源码分析](https://mp.weixin.qq.com/s/Nb3qeOXXW1xa-qFr-uNi9g)