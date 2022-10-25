# seata AT模式整体调用流程

## 客户端调用链路

### 客户端初始化逻辑

`GlobalTransactionScanner`类在初始化时会扫描所有类,如果该类上有注解`GlobalTransactional`则会将其进行aop切面增强.增强类由`GlobalTransactionalInterceptor`实现,在该类的invoke方法中,会调用`TransactionalTemplate#execute`方法,该方法使用模版模式,实现重复代码封装,使事务的开启回滚等方式得以复用.

### TM 调用链路

**1. 开启事务时**

调用 `TransactionalTemplate#beginTransaction`方法开启事务,实际是向请求seata开启全局事务,同步获取xid.xid示例 host:port:tranctionid 192.168.20.52:8091:2117408493.

**2. 决定事务提交/回滚**

**事务提交**

调用 `TransactionalTemplate#commitTransaction`方法开启事务,实际是向请求seata提交全局事务,默认重试次数为1次,超过重试次数未成功则抛出`TransactionException`异常.提交方法最后会清除事务信息(xid);

**事务回滚**

调用 `TransactionalTemplate#rollbackTransaction`方法开启事务,实际是向请求seata回滚全局事务,默认重试次数为1次,超过重试次数未成功则抛出`TransactionException`异常.提交方法最后会清除事务信息(xid),并且通过`TransactionalExecutor`抛出异常.执行逻辑与commit相似



### RM 调用链路

**事务提交**

`DataSource`对象被Seata代理,当提交成功后会调用seata的注册分支接口.实际调用`ConnectionProxy#processGlobalTransactionCommit`方法进行

```java
private void processGlobalTransactionCommit() throws SQLException {
    try {
        //向TC注册分支事务
        register();
    } catch (TransactionException e) {
        recognizeLockKeyConflictException(e, context.buildLockKeys());
    }

    try {
        //如果有undolog的话
        if (context.hasUndoLog()) {
           //插入undolog日志记录
            UndoLogManagerFactory.getUndoLogManager(this.getDbType()).flushUndoLogs(this);
        }
        //提交事务
        targetConnection.commit();
    } catch (Throwable ex) {
        report(false);
        throw new SQLException(ex);
    }
    if (IS_REPORT_SUCCESS_ENABLE) {
        //报告分支事务提交成功
        report(true);
    }
    //清除上下文的xid,branchId等相关信息
    context.reset();
}
```

注册时传递的参数是 `branchType(事务模式)`,`resourceId(数据库连接)`,`clientId(seata-order-service:192.168.20.52:62369)`,`xid`和`lockKeys(resourceId^^tableName^^pk)`



**事务回滚**

当事务失败发生回滚时,会首先回滚数据库,如果配置`client.rm.lock.retry.policy.branch-rollback-on-conflict`为true直接回滚不会向TC报告.



**TC事务提交回调**

`RmMessageListener#onMessage`,根据不同的选择进行后续处理.

```java
public void onMessage(RpcMessage request, String serverAddress, ClientMessageSender sender) {
    Object msg = request.getBody();
    //根据不同的msg类型进行处理
    if (msg instanceof BranchCommitRequest) {
        //该方法调用到AbstractRMHandler.doBranchCommit
        handleBranchCommit(request, serverAddress, (BranchCommitRequest)msg, sender);
    } else if (msg instanceof BranchRollbackRequest) {
        handleBranchRollback(request, serverAddress, (BranchRollbackRequest)msg, sender);
    } else if (msg instanceof UndoLogDeleteRequest) {
        handleUndoLogDelete((UndoLogDeleteRequest)msg);
    }
}
```

`AbstractRMHandler.doBranchCommit`方法主要是组装事务信息,xid,branchId等,然后交由`ResourceManager`处理分支事务提交操作.AT中默认的`ResourceManager`实现类是`DataSourceManager`,它会将上阶段组装的实体类加入到 `ASYNC_COMMIT_BUFFER`队列中交由异步线程执行. 该异步线程是`DataSourceManager`初始化时的 `AsyncWorker`线程池 `timerExecutor`.

```java
private void doBranchCommits() {
    if (ASYNC_COMMIT_BUFFER.size() == 0) {
        return;
    }
	//跟进资源id对二阶段提交请求分组
    Map<String, List<Phase2Context>> mappedContexts = new HashMap<>(DEFAULT_RESOURCE_SIZE);
    while (!ASYNC_COMMIT_BUFFER.isEmpty()) {
        Phase2Context commitContext = ASYNC_COMMIT_BUFFER.poll();
        List<Phase2Context> contextsGroupedByResourceId = mappedContexts.get(commitContext.resourceId);
        if (contextsGroupedByResourceId == null) {
            contextsGroupedByResourceId = new ArrayList<>();
            mappedContexts.put(commitContext.resourceId, contextsGroupedByResourceId);
        }
        contextsGroupedByResourceId.add(commitContext);

    }
	//遍历所有请求
    for (Map.Entry<String, List<Phase2Context>> entry : mappedContexts.entrySet()) {
        Connection conn = null;
        DataSourceProxy dataSourceProxy;
        try {
            try {
                //获取数据库链接
                DataSourceManager resourceManager = (DataSourceManager)DefaultResourceManager.get()
                    .getResourceManager(BranchType.AT);
                dataSourceProxy = resourceManager.get(entry.getKey());
                if (dataSourceProxy == null) {
                    throw new ShouldNeverHappenException("Failed to find resource on " + entry.getKey());
                }
                conn = dataSourceProxy.getPlainConnection();
            } catch (SQLException sqle) {
                continue;
            }
            List<Phase2Context> contextsGroupedByResourceId = entry.getValue();
            Set<String> xids = new LinkedHashSet<>(UNDOLOG_DELETE_LIMIT_SIZE);
            Set<Long> branchIds = new LinkedHashSet<>(UNDOLOG_DELETE_LIMIT_SIZE);
            //批量删除undolog
            for (Phase2Context commitContext : contextsGroupedByResourceId) {
                xids.add(commitContext.xid);
                branchIds.add(commitContext.branchId);
                int maxSize = xids.size() > branchIds.size() ? xids.size() : branchIds.size();
                if (maxSize == UNDOLOG_DELETE_LIMIT_SIZE) {
                    try {
                        UndoLogManagerFactory.getUndoLogManager(dataSourceProxy.getDbType()).batchDeleteUndoLog(
                            xids, branchIds, conn);
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
                //批量删除undolog
                UndoLogManagerFactory.getUndoLogManager(dataSourceProxy.getDbType()).batchDeleteUndoLog(xids,
                    branchIds, conn);
            } catch (Exception ex) {
            }

        }
    }
}
```









## 服务端调用链路

### TC针对TM执行逻辑

#### **1. 开启全局事务**

客户端通过netty与服务端通信,开启全局事务时会调用到`DefaultCore#begin`方法,实际流程如下.

1. 创建一个`GlobalSession`,然后将该session添加`SessionLifecycleListener`管理,实际实现类为`DataBaseSessionManager`.

2. 更新session启动信息,并写入到数据库中.写入数据格式

   ```sql
   
   INSERT INTO `global_table` (xid,transaction_id,status,application_id,transaction_service_group,transaction_name
   timeout,begin_time,application_data,gmt_create,gmt_modified) VALUES ('192.168.20.52:8091:2117469235', 2117469235, 1, 'seata-order-service', 'fsp_tx_group', 'fsp-create-order', 6000000, 1663741946862, NULL, '2022-09-21 14:33:38', '2022-09-21 14:33:38');
   ```




3. 返回xid

```java
public String begin(String applicationId, String transactionServiceGroup, String name, int timeout)
    throws TransactionException {
    GlobalSession session = GlobalSession.createGlobalSession(applicationId, transactionServiceGroup, name,
        timeout);
    session.addSessionLifecycleListener(SessionHolder.getRootSessionManager());

    session.begin();
    
    return session.getXid();
}
```

#### **2. 提交全局事务**

Server端全局提交时调用 `DefaultCoordinator#doGlobalCommit`方法,实际时通过调用`DefaultCore#commit`方法修改全局事务状态.

step 1:获取全局session

step 2:添加listener,因为一般在begin时已经添加了,所以这一步并没有实际执行

step 3: 调用`globalSession#lockAndExcute`方法,利用模版模式获取锁和是方法锁(ReentrantLock)

step 4: 修改事务属性,删除`branch_table`表中全局事务对应的`branchId`的记录

step 5: 将事务状态修改为Committing,状态修改失败直接返回

step 6: 同步/异步更是事务状态



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

异步提交的内容处理由`DefaultCoordinator`初始化时的`asyncCommitting`异步线程池处理.实际执行`DefaultCore#doGlobalCommit`方法.

该方法实际执行是,针对通知每个分支提交,并更新全局事务状态,所有步骤顺利执行后关闭事务删除相关数据.

step 1: 遍历分支事务调用,`DefaultCoordinator#branchCommit`,通知rm提交分支事务

step 2: 提交成功的,删除分支表数据,删除lock表数据,移除分支事务;提交失败如果不能重试直接修改全局事务状态为`CommitFailed`,并结束事务;默认清空如果能重试则交由`RetryCommit`处理

step 3:更新 事务状态,批量删除分支锁信息,清除当前全局事务状态

```java
//异步提交事务 retrying=true 同步提交 retrying = false

public void doGlobalCommit(GlobalSession globalSession, boolean retrying) throws TransactionException {
    
        for (BranchSession branchSession : globalSession.getSortedBranches()) {
            BranchStatus currentStatus = branchSession.getStatus();
            if (currentStatus == BranchStatus.PhaseOne_Failed) {
                globalSession.removeBranch(branchSession);
                continue;
            }
            try {
                
                BranchStatus branchStatus = resourceManagerInbound.branchCommit(branchSession.getBranchType(),
                    branchSession.getXid(), branchSession.getBranchId(), branchSession.getResourceId(),
                    branchSession.getApplicationData());

                switch (branchStatus) {
                    case PhaseTwo_Committed:
                        globalSession.removeBranch(branchSession);
                        continue;
                    case PhaseTwo_CommitFailed_Unretryable:
                        if (globalSession.canBeCommittedAsync()) {
                            continue;
                        } else {
                            SessionHelper.endCommitFailed(globalSession);
                            return;
                        }
                    default:
                        if (!retrying) {
                            queueToRetryCommit(globalSession);
                            return;
                        }
                        if (globalSession.canBeCommittedAsync()) {
                            continue;
                        } else {
                            return;
                        }
                }

            } catch (Exception ex) {
                if (!retrying) {
                    queueToRetryCommit(globalSession);
                    throw new TransactionException(ex);
                }
            }
        }
        if (globalSession.hasBranch()) {
            return;
        }
    SessionHelper.endCommitted(globalSession);
}
```

整个流程执行的sql如下:

```sql
for(banch){
-- 删除分支事务
delete from branch_table where xid = ? and branch_id = ?
delete from lock_table where xid = ? and branch_id = ? 
}
-- 删除全局事务
update global_table set status = ?, gmt_modified = now() where xid = ?
delete from global_table where xid = ?

```

#### 3. 回滚全局事务

Server端全局提交时调用 `DefaultCore#rollback`方法,实际时通过调用`DefaultCore#doGlobalRollback`方法修改全局事务状态.

整体方法流程与提交方法相似,不再赘述.



### TC针对RM执行逻辑

#### 分支事务注册

分支注册时主要调用`DefaultCore#branchRegister`方法

该方法流程如下:

step 1: 首先校验全局事务是否存在,属性

step 2:初始化分支事务实体

step 3:获取分支事务锁信息

step 4:在分支事务表中创建记录

step 4:返回分支事务id

```java
public Long branchRegister(BranchType branchType, String resourceId, String clientId, String xid,
                               String applicationData, String lockKeys) throws TransactionException {
        GlobalSession globalSession = assertGlobalSessionNotNull(xid, false);
        return globalSession.lockAndExcute(() -> {
            globalSession.addSessionLifecycleListener(SessionHolder.getRootSessionManager());
            //初始化branchSession实体
            BranchSession branchSession = SessionHelper.newBranchByGlobal(globalSession, branchType, resourceId,
                    applicationData, lockKeys, clientId);
            //首先去lock_table表中查询是否有冲突key,如果不存在则插入
            if (!branchSession.lock()) {
                throw new BranchTransactionException();
            }
            try {
                //将分支事务加入到全局事务中管理
                globalSession.addBranch(branchSession);
            } catch (RuntimeException ex) {
                branchSession.unlock();
                throw new BranchTransactionException();
            }
			//返回分支事务id
            return branchSession.getBranchId();
        });
}
```

#### **分支事务报告事务状态**

分支事务注册成功后会根据事务状态向TC报告当前分支事务状态,true为成功,false为失败

事务报告方法为`DefaultCore#branchReport`服务端根据参数修改事务状态.具体流程为

step 1: 校验事务属性

step 2: 修改分支事务状态

```java
public void branchReport(BranchType branchType, String xid, long branchId, BranchStatus status,
                         String applicationData) throws TransactionException {
    GlobalSession globalSession = assertGlobalSessionNotNull(xid, true);
    BranchSession branchSession = globalSession.getBranch(branchId);
    if (branchSession == null) {
        throw new BranchTransactionException(BranchTransactionNotExist,
                String.format("Could not found branch session xid = %s branchId = %s", xid, branchId));
    }
    globalSession.addSessionLifecycleListener(SessionHolder.getRootSessionManager());
    globalSession.changeBranchStatus(branchSession, status);

}
```

#### 全局事务提交时通知分支事务

事务提交时会调用 `RpcServer#sendSyncRequest` 异步通知分支事务做后续处理. `AbstractRpcRemoting#sendAsyncRequest`在做后续处理时会先把相关信息封装成`sendAsyncRequest`实体, 然后根据`clientId`分组,放入到`basketMap`中,唤醒 `AbstractRpcRemotingClient#MergedSendRunnable`线程.执行发送逻辑.



