# xxl源码分析

## 1 xxl-job 简介

XXL-JOB是一个分布式任务调度平台，其核心设计目标是开发迅速、学习简单、轻量级、易扩展。

其源码分为三部分：

- **xxl-job-admin** 调度中心
- **xxl-job-core** 核心依赖
- xxl-job-executor-samples 示例

## 2 xxl-job-core

xxl-job-core包下的内容,主要包括xxlJobExecutor,EmbedServer,ExecutorBizImpl,JobThread.

### 2.1 xxlJobExecutor

core中最核心的部分是xxl-job的执行器XxlJobExecutor，它的启动方式是在其子类中实现的.主要查看XxlJobSpringExecutor类,该类与spring继承,通过spring启动后自动获取任务,启动执行器.

**启动方法**

```java
public void afterSingletonsInstantiated() {

    // init JobHandler Repository
    /*initJobHandlerRepository(applicationContext);*/

    // init JobHandler Repository (for method)
    //该方法会扫描获取所有注解了XxlJob注解的方法,然后调用registJobHandler方法,将方法提取相关类信息和方法信息后放入jobHandlerRepository map中
    initJobHandlerMethodRepository(applicationContext);

    // new SpringGlueFactory();
    GlueFactory.refreshInstance(1);

    try {
        //调用父类start方法
        super.start();
    } catch (Exception e) {
        throw new RuntimeException(e);
    }
}
```

**父类启动方法**

```java
public void start() throws Exception {

    // init logpath 创建logpath
    XxlJobFileAppender.initLogPath(logPath);

    // 获取admin信息初始化客户端
    initAdminBizList(adminAddresses, accessToken);


    // 启动日志线程
    JobLogFileCleanThread.getInstance().start(logRetentionDays);

    //启动回调线程
    TriggerCallbackThread.getInstance().start();

    // 初始化netty客户端,与服务端通信
    initEmbedServer(address, ip, port, appname, accessToken);
}
```

我们再来看执行器XxlJobExecutor中的功能

- 启动 start()

  - 初始化日志

  - 初始化rpc调用方列表，根据admin地址和token初始化一个调用列表（admin地址和token通过配置文件获取）

  - 初始化任务日志文件清理

    这里会调用JobLogFileCleanThread线程-   -   ，每天调用一次文件清理

  - 初始化任务触发器回调TriggerCallbackThread（反馈调度结果）

  - 初始化rpc提供方，将address, ip, port, appname, accessToken设置到执行服务中

    - 启动rpc嵌入服务embedServer.start(address, port, appname, accessToken);

      这里通过Nettyn内嵌http服务，启动服务后，会启动额外线程（ExecutorRegistryThread）注册appname, address

      以下为服务注册线程，服务注册线程会调用之前创建好的rpc调用方列表AdminBizList，executor执行器通过远程调用的方式向admin调度中注册的心跳线程registryThread

```java
while (!toStop) {
for (AdminBiz adminBiz: XxlJobExecutor.getAdminBizList()) {
 ReturnT<String> registryResult = adminBiz.registry(registryParam);
if (registryResult!=null && ReturnT.SUCCESS_CODE == registryResult.getCode()) {
      registryResult = ReturnT.SUCCESS;
      break;
} 
}
 if (!toStop) {
                            TimeUnit.SECONDS.sleep(RegistryConfig.BEAT_TIMEOUT);
 }


}
```

销毁 destroy

rpc的调用方 initAdminBizList（在启动时调用）

rpc的提供方 initEmbedServer（在启动时调用）

注册销毁任务处理器（Spring容器初始化之后，扫描到标记注解的方法后，-   调用注册任务处理器，在执行器初始化之前调用）

注册销毁任务线程（执行器被调用时会启动执行任务）

### 2.2 EmbedServer

通过Netty构建Http内嵌服务，这里也是admin和执行器进行rpc通信的核心，也是Netty的经典应用场景.核心逻辑是对admin请求的处理

```java
private Object process(HttpMethod httpMethod, String uri, String requestData, String accessTokenReq) {

    // services mapping
    try {
        switch (uri) {
            case "/beat":
                return executorBiz.beat();
            case "/idleBeat":
                IdleBeatParam idleBeatParam = GsonTool.fromJson(requestData, IdleBeatParam.class);
                return executorBiz.idleBeat(idleBeatParam);
            case "/run":
                TriggerParam triggerParam = GsonTool.fromJson(requestData, TriggerParam.class);
                return executorBiz.run(triggerParam);
            case "/kill":
                KillParam killParam = GsonTool.fromJson(requestData, KillParam.class);
                return executorBiz.kill(killParam);
            case "/log":
                LogParam logParam = GsonTool.fromJson(requestData, LogParam.class);
                return executorBiz.log(logParam);
            default:
                return new ReturnT<String>(ReturnT.FAIL_CODE, "invalid request, uri-mapping(" + uri + ") not found.");
        }
    } catch (Exception e) {
        logger.error(e.getMessage(), e);
        return new ReturnT<String>(ReturnT.FAIL_CODE, "request error:" + ThrowableUtil.toString(e));
    }
}
```

### 2.3 ExecutorBizImpl

用于执行admin调度中心发送过来的执行请求，EmbedServer 收到来自调度中心的run请求时会启动该类中的run方法。然后加载任务线程，通过对应的处理器，注册任务

```java
public ReturnT<String> run(TriggerParam triggerParam) {
    // load old：jobHandler + jobThread
    JobThread jobThread = XxlJobExecutor.loadJobThread(triggerParam.getJobId());
    IJobHandler jobHandler = jobThread!=null?jobThread.getHandler():null;
    String removeOldReason = null;

    // valid：jobHandler + jobThread
    GlueTypeEnum glueTypeEnum = GlueTypeEnum.match(triggerParam.getGlueType());
    if (GlueTypeEnum.GLUE_GROOVY == glueTypeEnum) {

        // valid old jobThread
        if (jobThread != null &&
                !(jobThread.getHandler() instanceof GlueJobHandler
                    && ((GlueJobHandler) jobThread.getHandler()).getGlueUpdatetime()==triggerParam.getGlueUpdatetime() )) {
            // change handler or gluesource updated, need kill old thread
            removeOldReason = "change job source or glue type, and terminate the old job thread.";

            jobThread = null;
            jobHandler = null;
        }

        // valid handler
        if (jobHandler == null) {
            try {
                IJobHandler originJobHandler = GlueFactory.getInstance().loadNewInstance(triggerParam.getGlueSource());
                jobHandler = new GlueJobHandler(originJobHandler, triggerParam.getGlueUpdatetime());
            } catch (Exception e) {
                logger.error(e.getMessage(), e);
                return new ReturnT<String>(ReturnT.FAIL_CODE, e.getMessage());
            }
        }
    } 

    // executor block strategy
    if (jobThread != null) {
        ExecutorBlockStrategyEnum blockStrategy = ExecutorBlockStrategyEnum.match(triggerParam.getExecutorBlockStrategy(), null);
        if (ExecutorBlockStrategyEnum.DISCARD_LATER == blockStrategy) {
            // discard when running
            if (jobThread.isRunningOrHasQueue()) {
                return new ReturnT<String>(ReturnT.FAIL_CODE, "block strategy effect："+ExecutorBlockStrategyEnum.DISCARD_LATER.getTitle());
            }
        } else if (ExecutorBlockStrategyEnum.COVER_EARLY == blockStrategy) {
            // kill running jobThread
            if (jobThread.isRunningOrHasQueue()) {
                removeOldReason = "block strategy effect：" + ExecutorBlockStrategyEnum.COVER_EARLY.getTitle();

                jobThread = null;
            }
        } else {
            // just queue trigger
        }
    }

    // replace thread (new or exists invalid)
    if (jobThread == null) {
        jobThread = XxlJobExecutor.registJobThread(triggerParam.getJobId(), jobHandler, removeOldReason);
    }

    // push data to queue
    ReturnT<String> pushResult = jobThread.pushTriggerQueue(triggerParam);
    return pushResult;
}
```

### 2.4 JobThread

任务线程，通过任务线程执行对应的任务处理

步骤：

- 初始化处理器
- 执行
  - 取出队列triggerQueue
  - 移除triggerLogIdSet记录
  - 设置XxlJobContext内容
  - 执行处理器
  - 获取执行结果
  - 删除任务线程
- 调用TriggerCallbackThread反馈调度结果
- 销毁处理器

核心代码如下:

```java
public void run() {

       // init
       try {
      handler.init();
   } catch (Throwable e) {
          logger.error(e.getMessage(), e);
   }

   // execute
   while(!toStop){
      running = false;
      idleTimes++;

           TriggerParam triggerParam = null;
           try {
         // to check toStop signal, we need cycle, so wo cannot use queue.take(), instand of poll(timeout)
         triggerParam = triggerQueue.poll(3L, TimeUnit.SECONDS);
         if (triggerParam!=null) {
            running = true;
            idleTimes = 0;
            triggerLogIdSet.remove(triggerParam.getLogId());

            // log filename, like "logPath/yyyy-MM-dd/9999.log"
            String logFileName = XxlJobFileAppender.makeLogFileName(new Date(triggerParam.getLogDateTime()), triggerParam.getLogId());
            XxlJobContext xxlJobContext = new XxlJobContext(
                  triggerParam.getJobId(),
                  triggerParam.getExecutorParams(),
                  logFileName,
                  triggerParam.getBroadcastIndex(),
                  triggerParam.getBroadcastTotal());

            // init job context
            XxlJobContext.setXxlJobContext(xxlJobContext);

            // execute
            XxlJobHelper.log("<br>----------- xxl-job job execute start -----------<br>----------- Param:" + xxlJobContext.getJobParam());

            if (triggerParam.getExecutorTimeout() > 0) {
               // limit timeout
               Thread futureThread = null;
               try {
                  FutureTask<Boolean> futureTask = new FutureTask<Boolean>(new Callable<Boolean>() {
                     @Override
                     public Boolean call() throws Exception {

                        // init job context
                        XxlJobContext.setXxlJobContext(xxlJobContext);

                        handler.execute();
                        return true;
                     }
                  });
                  futureThread = new Thread(futureTask);
                  futureThread.start();

                  Boolean tempResult = futureTask.get(triggerParam.getExecutorTimeout(), TimeUnit.SECONDS);
               } catch (TimeoutException e) {

                  XxlJobHelper.log("<br>----------- xxl-job job execute timeout");
                  XxlJobHelper.log(e);

                  // handle result
                  XxlJobHelper.handleTimeout("job execute timeout ");
               } finally {
                  futureThread.interrupt();
               }
            } else {
               // just execute
               handler.execute();
            }

            // valid execute handle data
            if (XxlJobContext.getXxlJobContext().getHandleCode() <= 0) {
               XxlJobHelper.handleFail("job handle result lost.");
            } else {
               String tempHandleMsg = XxlJobContext.getXxlJobContext().getHandleMsg();
               tempHandleMsg = (tempHandleMsg!=null&&tempHandleMsg.length()>50000)
                     ?tempHandleMsg.substring(0, 50000).concat("...")
                     :tempHandleMsg;
               XxlJobContext.getXxlJobContext().setHandleMsg(tempHandleMsg);
            }
            XxlJobHelper.log("<br>----------- xxl-job job execute end(finish) -----------<br>----------- Result: handleCode="
                  + XxlJobContext.getXxlJobContext().getHandleCode()
                  + ", handleMsg = "
                  + XxlJobContext.getXxlJobContext().getHandleMsg()
            );

         } else {
            if (idleTimes > 30) {
               if(triggerQueue.size() == 0) { // avoid concurrent trigger causes jobId-lost
                  XxlJobExecutor.removeJobThread(jobId, "excutor idel times over limit.");
               }
            }
         }
      } catch (Throwable e) {
         if (toStop) {
            XxlJobHelper.log("<br>----------- JobThread toStop, stopReason:" + stopReason);
         }

         // handle result
         StringWriter stringWriter = new StringWriter();
         e.printStackTrace(new PrintWriter(stringWriter));
         String errorMsg = stringWriter.toString();

         XxlJobHelper.handleFail(errorMsg);

         XxlJobHelper.log("<br>----------- JobThread Exception:" + errorMsg + "<br>----------- xxl-job job execute end(error) -----------");
      } finally {
               if(triggerParam != null) {
                   // callback handler info
                   if (!toStop) {
                       // commonm
                       TriggerCallbackThread.pushCallBack(new HandleCallbackParam(
                              triggerParam.getLogId(),
                     triggerParam.getLogDateTime(),
                     XxlJobContext.getXxlJobContext().getHandleCode(),
                     XxlJobContext.getXxlJobContext().getHandleMsg() )
               );
                   } else {
                       // is killed
                       TriggerCallbackThread.pushCallBack(new HandleCallbackParam(
                              triggerParam.getLogId(),
                     triggerParam.getLogDateTime(),
                     XxlJobContext.HANDLE_CODE_FAIL,
                     stopReason + " [job running, killed]" )
               );
                   }
               }
           }
       }

   // callback trigger request in queue
   while(triggerQueue !=null && triggerQueue.size()>0){
      TriggerParam triggerParam = triggerQueue.poll();
      if (triggerParam!=null) {
         // is killed
         TriggerCallbackThread.pushCallBack(new HandleCallbackParam(
               triggerParam.getLogId(),
               triggerParam.getLogDateTime(),
               XxlJobContext.HANDLE_CODE_FAIL,
               stopReason + " [job not executed, in the job queue, killed.]")
         );
      }
   }

   // destroy
   try {
      handler.destroy();
   } catch (Throwable e) {
      logger.error(e.getMessage(), e);
   }

   logger.info(">>>>>>>>>>> xxl-job JobThread stoped, hashCode:{}", Thread.currentThread());
}
```

## 3 客户端代码总结

xxl-job-core 核心就是执行器，我们再来看一下xxl-job-core的执行原理：

Spring集成环境下：

1. Spring容器初始化
2. 容器化初始完成后
3. 注册处理器，通过扫描Spring容器中标有注解的任务处理器
4. 启动执行器
   - 初始化日志
   - 初始化rpc调用方（调度中心）列表
   - 初始化任务日志文件清理
   - 这里会调用JobLogFileCleanThread线程，每天调用一次文件清理
   - 初始化任务触发器回调TriggerCallbackThread（反馈调度结果）
   - 初始化rpc提供方，将address, ip, port, appname, accessToken设置到执行服务中
     - 创建执行服务器（Netty HTTP服务）
     - 创建向调度中心发生心跳的线程
5. 任务调度中心admin调度服务
6. 启动任务执行线程
7. 通过任务线程，调用对应处理器执行任务
8. 向任务调度中心反馈调度结果
9. 销毁任务执行线程

## 4 xxl-job-admin（调度中心）核心源码分析

xxl-job-admin是一个独立的项目，我们需要将其单独部署在服务器上，支持集群模式。xxl-job-admin负责管理调度信息，按照调度配置发出调度请求，自身不承担业务代码。调度系统与任务解耦，提高了系统可用性和稳定性，同时调度系统性能不再受限于任务模块；支持可视化、简单且动态的管理调度信息，包括任务新建，更新，删除，GLUE开发和任务报警等，所有上述操作都会实时生效，同时支持监控调度结果以及执行日志，支持执行器故障转移（Failover）。

**调度中心就是根据配置的任务信息，在合适的时间调用执行器执行任务**，目的就是将**任务**和**调度**解耦。

### 4.1 调度器启动

它的启动方式也是基于Spring，是在属性填充后执行，首先是获取配置，然后就是初始化调度器。

```java
public class XxlJobAdminConfig implements InitializingBean, DisposableBean {

    private static XxlJobAdminConfig adminConfig = null;
    public static XxlJobAdminConfig getAdminConfig() {
        return adminConfig;
    }


    // ---------------------- XxlJobScheduler ----------------------

    private XxlJobScheduler xxlJobScheduler;

    @Override
    public void afterPropertiesSet() throws Exception {
        adminConfig = this;

        xxlJobScheduler = new XxlJobScheduler();
        xxlJobScheduler.init();
    }
    /**省略其他属性**/
}    
```

### 4.2 xxlJobScheduler 初始化

这里我们只看最为核心的代码

初始化步骤：

- 国际化
- **触发器线程池**
- **注册监视器（任务服务心跳监控）**
- 文件监控器
- **任务完成（丢失任务监控器）**
- 日志报告监控
- **任务调度**

```java
public void init() throws Exception {
    // init i18n
    initI18n();

    // admin trigger pool start
    JobTriggerPoolHelper.toStart();

    // admin registry monitor run
    JobRegistryHelper.getInstance().start();

    // admin fail-monitor run
    JobFailMonitorHelper.getInstance().start();

    // admin lose-monitor run ( depend on JobTriggerPoolHelper )
    JobCompleteHelper.getInstance().start();

    // admin log report start
    JobLogReportHelper.getInstance().start();

    // start-schedule  ( depend on JobTriggerPoolHelper )
    JobScheduleHelper.getInstance().start();

    logger.info(">>>>>>>>> init xxl-job admin success.");
}
```
### 4.3 JobTriggerPoolHelper触发器线程池

JobTriggerPoolHelper是其中最为核心的一个类，它通过线程池的方式能够实现多任务并行触发执行，我们调度中心向执行器发号施令都是通过此处，这里一快一慢构建了两个线程池。当任务的超时时间设置超过10秒时，我们认为其为慢任务，使用慢触发器线程池去处理，没有设置超时间和超时时间小于10s的我们认为其运行很快交由快触发器线程池处理。

这里addTrigger是核心方法，使我们触发任务时调用的。

```java
//初始化快慢线程池
public void start(){
    //快线程池处理任务超时时间小于10s的
    fastTriggerPool = new ThreadPoolExecutor(
            10,
            XxlJobAdminConfig.getAdminConfig().getTriggerPoolFastMax(),
            60L,
            TimeUnit.SECONDS,
            new LinkedBlockingQueue<Runnable>(1000),
            new ThreadFactory() {
                @Override
                public Thread newThread(Runnable r) {
                    return new Thread(r, "xxl-job, admin JobTriggerPoolHelper-fastTriggerPool-" + r.hashCode());
                }
            });
	//慢线程池处理任务超时时间大于10s的
    slowTriggerPool = new ThreadPoolExecutor(
            10,
            XxlJobAdminConfig.getAdminConfig().getTriggerPoolSlowMax(),
            60L,
            TimeUnit.SECONDS,
            new LinkedBlockingQueue<Runnable>(2000),
            new ThreadFactory() {
                @Override
                public Thread newThread(Runnable r) {
                    return new Thread(r, "xxl-job, admin JobTriggerPoolHelper-slowTriggerPool-" + r.hashCode());
                }
            });
}
```
**触发任务时调用**
```java
public void addTrigger(final int jobId,
                       final TriggerTypeEnum triggerType,
                       final int failRetryCount,
                       final String executorShardingParam,
                       final String executorParam,
                       final String addressList) {

    // choose thread pool
    ThreadPoolExecutor triggerPool_ = fastTriggerPool;
    AtomicInteger jobTimeoutCount = jobTimeoutCountMap.get(jobId);
    if (jobTimeoutCount!=null && jobTimeoutCount.get() > 10) {      // job-timeout 10 times in 1 min
        triggerPool_ = slowTriggerPool;
    }

    // trigger
    triggerPool_.execute(new Runnable() {
        @Override
        public void run() {

            long start = System.currentTimeMillis();

            try {
                // do trigger,实际执行触发操作
                XxlJobTrigger.trigger(jobId, triggerType, failRetryCount, executorShardingParam, executorParam, addressList);
            } catch (Exception e) {
                logger.error(e.getMessage(), e);
            } finally {

                // check timeout-count-map
                long minTim_now = System.currentTimeMillis()/60000;
                if (minTim != minTim_now) {
                    minTim = minTim_now;
                    jobTimeoutCountMap.clear();
                }

                // incr timeout-count-map
                long cost = System.currentTimeMillis()-start;
                if (cost > 500) {       // ob-timeout threshold 500ms
                    AtomicInteger timeoutCount = jobTimeoutCountMap.putIfAbsent(jobId, new AtomicInteger(1));
                    if (timeoutCount != null) {
                        timeoutCount.incrementAndGet();
                    }
                }

            }

        }
    });
}
```

### 4.4 XxlJobTrigger 任务触发器

该类是任务触发的详细流程

- trigger
  - 获取任务详细信息
  - 设置任务参数
  - 获取失败重试次数
  - 获取任务组（执行机器）
  - 判断是否需要分片广播（此处可将分片参数传入，每台机器执行对应的分片任务）
  - processTrigger
    - 获取阻塞策略
    - 获取路由策略
    - 获取分片参数
    - 记录日志log-id
    - 初始化触发器参数
    - 初始化调用地址
    - runExecutor
      - rpc远程调用executorBiz.run(triggerParam)（触发执行器执行任务）
    - 获取调用结果
    - 记录日志trigger-info

```java
private static void processTrigger(XxlJobGroup group, XxlJobInfo jobInfo, int finalFailRetryCount, TriggerTypeEnum triggerType, int index, int total){

    // param
    ExecutorBlockStrategyEnum blockStrategy = ExecutorBlockStrategyEnum.match(jobInfo.getExecutorBlockStrategy(), ExecutorBlockStrategyEnum.SERIAL_EXECUTION);  // block strategy
    ExecutorRouteStrategyEnum executorRouteStrategyEnum = ExecutorRouteStrategyEnum.match(jobInfo.getExecutorRouteStrategy(), null);    // route strategy
    String shardingParam = (ExecutorRouteStrategyEnum.SHARDING_BROADCAST==executorRouteStrategyEnum)?String.valueOf(index).concat("/").concat(String.valueOf(total)):null;

    // 1、save log-id
    XxlJobLog jobLog = new XxlJobLog();
    jobLog.setJobGroup(jobInfo.getJobGroup());
    jobLog.setJobId(jobInfo.getId());
    jobLog.setTriggerTime(new Date());
    XxlJobAdminConfig.getAdminConfig().getXxlJobLogDao().save(jobLog);
    logger.debug(">>>>>>>>>>> xxl-job trigger start, jobId:{}", jobLog.getId());

    // 2、init trigger-param
    TriggerParam triggerParam = new TriggerParam();
    triggerParam.setJobId(jobInfo.getId());
    triggerParam.setExecutorHandler(jobInfo.getExecutorHandler());
    triggerParam.setExecutorParams(jobInfo.getExecutorParam());
    triggerParam.setExecutorBlockStrategy(jobInfo.getExecutorBlockStrategy());
    triggerParam.setExecutorTimeout(jobInfo.getExecutorTimeout());
    triggerParam.setLogId(jobLog.getId());
    triggerParam.setLogDateTime(jobLog.getTriggerTime().getTime());
    triggerParam.setGlueType(jobInfo.getGlueType());
    triggerParam.setGlueSource(jobInfo.getGlueSource());
    triggerParam.setGlueUpdatetime(jobInfo.getGlueUpdatetime().getTime());
    triggerParam.setBroadcastIndex(index);
    triggerParam.setBroadcastTotal(total);

    // 3、init address
    String address = null;
    ReturnT<String> routeAddressResult = null;
    if (group.getRegistryList()!=null && !group.getRegistryList().isEmpty()) {
        if (ExecutorRouteStrategyEnum.SHARDING_BROADCAST == executorRouteStrategyEnum) {
            if (index < group.getRegistryList().size()) {
                address = group.getRegistryList().get(index);
            } else {
                address = group.getRegistryList().get(0);
            }
        } else {
            routeAddressResult = executorRouteStrategyEnum.getRouter().route(triggerParam, group.getRegistryList());
            if (routeAddressResult.getCode() == ReturnT.SUCCESS_CODE) {
                address = routeAddressResult.getContent();
            }
        }
    } else {
        routeAddressResult = new ReturnT<String>(ReturnT.FAIL_CODE, I18nUtil.getString("jobconf_trigger_address_empty"));
    }

    // 4、trigger remote executor
    ReturnT<String> triggerResult = null;
    if (address != null) {
        triggerResult = runExecutor(triggerParam, address);
    } else {
        triggerResult = new ReturnT<String>(ReturnT.FAIL_CODE, null);
    }

    // 5、collection trigger info
    StringBuffer triggerMsgSb = new StringBuffer();
    triggerMsgSb.append(I18nUtil.getString("jobconf_trigger_type")).append("：").append(triggerType.getTitle());
    triggerMsgSb.append("<br>").append(I18nUtil.getString("jobconf_trigger_admin_adress")).append("：").append(IpUtil.getIp());
    triggerMsgSb.append("<br>").append(I18nUtil.getString("jobconf_trigger_exe_regtype")).append("：")
            .append( (group.getAddressType() == 0)?I18nUtil.getString("jobgroup_field_addressType_0"):I18nUtil.getString("jobgroup_field_addressType_1") );
    triggerMsgSb.append("<br>").append(I18nUtil.getString("jobconf_trigger_exe_regaddress")).append("：").append(group.getRegistryList());
    triggerMsgSb.append("<br>").append(I18nUtil.getString("jobinfo_field_executorRouteStrategy")).append("：").append(executorRouteStrategyEnum.getTitle());
    if (shardingParam != null) {
        triggerMsgSb.append("("+shardingParam+")");
    }
    triggerMsgSb.append("<br>").append(I18nUtil.getString("jobinfo_field_executorBlockStrategy")).append("：").append(blockStrategy.getTitle());
    triggerMsgSb.append("<br>").append(I18nUtil.getString("jobinfo_field_timeout")).append("：").append(jobInfo.getExecutorTimeout());
    triggerMsgSb.append("<br>").append(I18nUtil.getString("jobinfo_field_executorFailRetryCount")).append("：").append(finalFailRetryCount);

    triggerMsgSb.append("<br><br><span style=\"color:#00c0ef;\" > >>>>>>>>>>>"+ I18nUtil.getString("jobconf_trigger_run") +"<<<<<<<<<<< </span><br>")
            .append((routeAddressResult!=null&&routeAddressResult.getMsg()!=null)?routeAddressResult.getMsg()+"<br><br>":"").append(triggerResult.getMsg()!=null?triggerResult.getMsg():"");

    // 6、save log trigger-info
    jobLog.setExecutorAddress(address);
    jobLog.setExecutorHandler(jobInfo.getExecutorHandler());
    jobLog.setExecutorParam(jobInfo.getExecutorParam());
    jobLog.setExecutorShardingParam(shardingParam);
    jobLog.setExecutorFailRetryCount(finalFailRetryCount);
    //jobLog.setTriggerTime();
    jobLog.setTriggerCode(triggerResult.getCode());
    jobLog.setTriggerMsg(triggerMsgSb.toString());
    XxlJobAdminConfig.getAdminConfig().getXxlJobLogDao().updateTriggerInfo(jobLog);

}
```

### 4.5 JobRegistryHelper 心跳检测线程池

这里会有一个线程实时检测每个任务组的注册情况，每当有服务注册会启动单独的线程进行注册。



```java
public void start(){

   // for registry or remove 注册线程池
   registryOrRemoveThreadPool = new ThreadPoolExecutor(
         2,
         10,
         30L,
         TimeUnit.SECONDS,
         new LinkedBlockingQueue<Runnable>(2000),
         new ThreadFactory() {
            @Override
            public Thread newThread(Runnable r) {
               return new Thread(r, "xxl-job, admin JobRegistryMonitorHelper-registryOrRemoveThreadPool-" + r.hashCode());
            }
         },
         new RejectedExecutionHandler() {
            @Override
            public void rejectedExecution(Runnable r, ThreadPoolExecutor executor) {
               r.run();
               logger.warn(">>>>>>>>>>> xxl-job, registry or remove too fast, match threadpool rejected handler(run now).");
            }
         });

   // for monitor 监控客户端的健康状态
   registryMonitorThread = new Thread();
   registryMonitorThread.setDaemon(true);
   registryMonitorThread.setName("xxl-job, admin JobRegistryMonitorHelper-registryMonitorThread");
   registryMonitorThread.start();
}
```

注册

```java
public ReturnT<String> registry(RegistryParam registryParam) {

   // async execute
   registryOrRemoveThreadPool.execute(new Runnable() {
      @Override
      public void run() {
          //upsert数据库中的记录
         int ret = XxlJobAdminConfig.getAdminConfig().getXxlJobRegistryDao().registryUpdate(registryParam.getRegistryGroup(), registryParam.getRegistryKey(), registryParam.getRegistryValue(), new Date());
         if (ret < 1) {
            XxlJobAdminConfig.getAdminConfig().getXxlJobRegistryDao().registrySave(registryParam.getRegistryGroup(), registryParam.getRegistryKey(), registryParam.getRegistryValue(), new Date());

            // fresh
            freshGroupRegistryInfo(registryParam);
         }
      }
   });

   return ReturnT.SUCCESS;
}
```

移除注册

```java
public ReturnT<String> registryRemove(RegistryParam registryParam) {

   // async execute
   registryOrRemoveThreadPool.execute(new Runnable() {
      @Override
      public void run() {
          //移除注册信息
         int ret = XxlJobAdminConfig.getAdminConfig().getXxlJobRegistryDao().registryDelete(registryParam.getRegistryGroup(), registryParam.getRegistryKey(), registryParam.getRegistryValue());
         if (ret > 0) {
            // fresh
            freshGroupRegistryInfo(registryParam);
         }
      }
   });

   return ReturnT.SUCCESS;
}
```

### 2.6 JobScheduleHelper 任务调度线程

调度线程scheduleThread

- 开启事务
- 根据时刻获取任务列表
- 当任务满足条件会调用触发器触发任务执行
  - 当前时间> 触发时间 + 预读时间
    - 调度过期策略判断是否需要再次执行
  - 当前时间> 触发时间
    - 触发任务
    - 刷新下一次触发时间
  - 当前时间小于等于触发时间
    - 加入时间环中，等待执行
    - 刷新下一次触发时间
- 修改保存调度结果
- 提交事务

ringThread 将上述线程推入待触发的任务，根据时刻触发执行

```java
public void start(){

    // schedule thread
    scheduleThread = new Thread(new Runnable() {
        @Override
        public void run() {

            try {
                TimeUnit.MILLISECONDS.sleep(5000 - System.currentTimeMillis()%1000 );
            } catch (InterruptedException e) {
                if (!scheduleThreadToStop) {
                    logger.error(e.getMessage(), e);
                }
            }
            logger.info(">>>>>>>>> init xxl-job admin scheduler success.");

            // pre-read count: treadpool-size * trigger-qps (each trigger cost 50ms, qps = 1000/50 = 20)
            int preReadCount = (XxlJobAdminConfig.getAdminConfig().getTriggerPoolFastMax() + XxlJobAdminConfig.getAdminConfig().getTriggerPoolSlowMax()) * 20;

            while (!scheduleThreadToStop) {

                // Scan Job
                long start = System.currentTimeMillis();

                Connection conn = null;
                Boolean connAutoCommit = null;
                PreparedStatement preparedStatement = null;

                boolean preReadSuc = true;
                try {

                    conn = XxlJobAdminConfig.getAdminConfig().getDataSource().getConnection();
                    connAutoCommit = conn.getAutoCommit();
                    conn.setAutoCommit(false);

                    preparedStatement = conn.prepareStatement(  "select * from xxl_job_lock where lock_name = 'schedule_lock' for update" );
                    preparedStatement.execute();

                    // tx start

                    // 1、pre read
                    long nowTime = System.currentTimeMillis();
                    List<XxlJobInfo> scheduleList = XxlJobAdminConfig.getAdminConfig().getXxlJobInfoDao().scheduleJobQuery(nowTime + PRE_READ_MS, preReadCount);
                    if (scheduleList!=null && scheduleList.size()>0) {
                        // 2、push time-ring
                        for (XxlJobInfo jobInfo: scheduleList) {

                            // time-ring jump
                            if (nowTime > jobInfo.getTriggerNextTime() + PRE_READ_MS) {
                                // 2.1、trigger-expire > 5s：pass && make next-trigger-time
                                logger.warn(">>>>>>>>>>> xxl-job, schedule misfire, jobId = " + jobInfo.getId());

                                // 1、misfire match
                                MisfireStrategyEnum misfireStrategyEnum = MisfireStrategyEnum.match(jobInfo.getMisfireStrategy(), MisfireStrategyEnum.DO_NOTHING);
                                if (MisfireStrategyEnum.FIRE_ONCE_NOW == misfireStrategyEnum) {
                                    // FIRE_ONCE_NOW 》 trigger
                                    JobTriggerPoolHelper.trigger(jobInfo.getId(), TriggerTypeEnum.MISFIRE, -1, null, null, null);
                                    logger.debug(">>>>>>>>>>> xxl-job, schedule push trigger : jobId = " + jobInfo.getId() );
                                }

                                // 2、fresh next
                                refreshNextValidTime(jobInfo, new Date());

                            } else if (nowTime > jobInfo.getTriggerNextTime()) {
                                // 2.2、trigger-expire < 5s：direct-trigger && make next-trigger-time

                                // 1、trigger
                                JobTriggerPoolHelper.trigger(jobInfo.getId(), TriggerTypeEnum.CRON, -1, null, null, null);
                                logger.debug(">>>>>>>>>>> xxl-job, schedule push trigger : jobId = " + jobInfo.getId() );

                                // 2、fresh next
                                refreshNextValidTime(jobInfo, new Date());

                                // next-trigger-time in 5s, pre-read again
                                if (jobInfo.getTriggerStatus()==1 && nowTime + PRE_READ_MS > jobInfo.getTriggerNextTime()) {

                                    // 1、make ring second
                                    int ringSecond = (int)((jobInfo.getTriggerNextTime()/1000)%60);

                                    // 2、push time ring
                                    pushTimeRing(ringSecond, jobInfo.getId());

                                    // 3、fresh next
                                    refreshNextValidTime(jobInfo, new Date(jobInfo.getTriggerNextTime()));

                                }

                            } else {
                                // 2.3、trigger-pre-read：time-ring trigger && make next-trigger-time

                                // 1、make ring second
                                int ringSecond = (int)((jobInfo.getTriggerNextTime()/1000)%60);

                                // 2、push time ring
                                pushTimeRing(ringSecond, jobInfo.getId());

                                // 3、fresh next
                                refreshNextValidTime(jobInfo, new Date(jobInfo.getTriggerNextTime()));

                            }

                        }

                        // 3、update trigger info
                        for (XxlJobInfo jobInfo: scheduleList) {
                            XxlJobAdminConfig.getAdminConfig().getXxlJobInfoDao().scheduleUpdate(jobInfo);
                        }

                    } else {
                        preReadSuc = false;
                    }

                    // tx stop


                } catch (Exception e) {
                    if (!scheduleThreadToStop) {
                        logger.error(">>>>>>>>>>> xxl-job, JobScheduleHelper#scheduleThread error:{}", e);
                    }
                } finally {

                    // commit
                    if (conn != null) {
                        try {
                            conn.commit();
                        } catch (SQLException e) {
                            if (!scheduleThreadToStop) {
                                logger.error(e.getMessage(), e);
                            }
                        }
                        try {
                            conn.setAutoCommit(connAutoCommit);
                        } catch (SQLException e) {
                            if (!scheduleThreadToStop) {
                                logger.error(e.getMessage(), e);
                            }
                        }
                        try {
                            conn.close();
                        } catch (SQLException e) {
                            if (!scheduleThreadToStop) {
                                logger.error(e.getMessage(), e);
                            }
                        }
                    }

                    // close PreparedStatement
                    if (null != preparedStatement) {
                        try {
                            preparedStatement.close();
                        } catch (SQLException e) {
                            if (!scheduleThreadToStop) {
                                logger.error(e.getMessage(), e);
                            }
                        }
                    }
                }
                long cost = System.currentTimeMillis()-start;


                // Wait seconds, align second
                if (cost < 1000) {  // scan-overtime, not wait
                    try {
                        // pre-read period: success > scan each second; fail > skip this period;
                        TimeUnit.MILLISECONDS.sleep((preReadSuc?1000:PRE_READ_MS) - System.currentTimeMillis()%1000);
                    } catch (InterruptedException e) {
                        if (!scheduleThreadToStop) {
                            logger.error(e.getMessage(), e);
                        }
                    }
                }

            }

            logger.info(">>>>>>>>>>> xxl-job, JobScheduleHelper#scheduleThread stop");
        }
    });
    scheduleThread.setDaemon(true);
    scheduleThread.setName("xxl-job, admin JobScheduleHelper#scheduleThread");
    scheduleThread.start();


    // ring thread
    ringThread = new Thread(new Runnable() {
        @Override
        public void run() {

            while (!ringThreadToStop) {

                // align second
                try {
                    TimeUnit.MILLISECONDS.sleep(1000 - System.currentTimeMillis() % 1000);
                } catch (InterruptedException e) {
                    if (!ringThreadToStop) {
                        logger.error(e.getMessage(), e);
                    }
                }

                try {
                    // second data
                    List<Integer> ringItemData = new ArrayList<>();
                    int nowSecond = Calendar.getInstance().get(Calendar.SECOND);   // 避免处理耗时太长，跨过刻度，向前校验一个刻度；
                    for (int i = 0; i < 2; i++) {
                        List<Integer> tmpData = ringData.remove( (nowSecond+60-i)%60 );
                        if (tmpData != null) {
                            ringItemData.addAll(tmpData);
                        }
                    }

                    // ring trigger
                    logger.debug(">>>>>>>>>>> xxl-job, time-ring beat : " + nowSecond + " = " + Arrays.asList(ringItemData) );
                    if (ringItemData.size() > 0) {
                        // do trigger
                        for (int jobId: ringItemData) {
                            // do trigger
                            JobTriggerPoolHelper.trigger(jobId, TriggerTypeEnum.CRON, -1, null, null, null);
                        }
                        // clear
                        ringItemData.clear();
                    }
                } catch (Exception e) {
                    if (!ringThreadToStop) {
                        logger.error(">>>>>>>>>>> xxl-job, JobScheduleHelper#ringThread error:{}", e);
                    }
                }
            }
            logger.info(">>>>>>>>>>> xxl-job, JobScheduleHelper#ringThread stop");
        }
    });
    ringThread.setDaemon(true);
    ringThread.setName("xxl-job, admin JobScheduleHelper#ringThread");
    ringThread.start();
}
```

## 3 服务端总结

调度中心是分布式任务调度的重中之中，我们下面再来过一下其执行流程

- Spring初始化
- 调度器初始化
  - 国际化
  - **触发器线程池**
  - **注册监视器（任务服务心跳监控）**
  - 文件监控器
  - **任务完成（丢失任务监控器）**
  - 日志报告监控
  - **任务调度**
- 任务调度线程，监控到待执行任务
- 调度通过触发器线程池执行调度
- 调度判断其是不是分片任务
- 根据路由策略，阻塞处理策略触发rpc远程调用执行器执行任务
- 执行器触发执行，任务结束后写回响应结果



参考:[xxl-job-core(执行器) 源码分析](https://juejin.cn/post/7068635930705264647)

[xxl-job-admin(调度中心) 源码分析](https://juejin.cn/post/7069222244785848356)

[XXL-JOB分布式调度框架全面详解，一篇就够啦！](https://juejin.cn/post/6948397386926391333#heading-15)

[xxl-job通信流程设计](https://juejin.cn/post/7053699649084850213)