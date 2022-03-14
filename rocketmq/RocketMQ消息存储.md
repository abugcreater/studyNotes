## RocketMQ消息存储

### 存储概要设计

RocketMQ主要存储的文件包括Comitlog文件,ConsumeQueue文件,IndexFile文件.提高消费者效率引入ConsumeQueue,IndexFile索引文件,加速消息的检索性能,从Commitlog文件中检索.
![RocketMQ message store.png](https://i.loli.net/2021/03/29/JPObWMLw1An46cY.png)

主要概念解析:

1. Comitlog:消息存储文件,所有消息主题的消息都存储在Comitlog文件中
2. ConsumeQueue:消息消费队列,消息到达Comitlog文件后,将异步转发到消息消费队列,供消费者消费
3. IndexFile:消息索引文件,主要存储消息key与Offset的对应关系
4. 事务状态服务:存储每条消息的事务状态
5. 定时消息服务:每一个延迟级别对应一个消息消费队列,存储延迟队列的消息拉取进度

消息

### 存储流程

RocketMQ的消息存储主要实现类是`org.apache.rocketmq.store.DefaultMessageStore`,包含了很多存储文件的API.

消息存储的入口是`DefaultMessageStore#putMessage`方法.

```java
1. 检查,如果当前broker停止工作,或者当前broker的角色是slave,或者不可写入,则拒绝消息的写入;当消息主题大于256字节或者消息体长度大于65536字节时拒绝消息写入

PutMessageStatus checkStoreStatus = this.checkStoreStatus();

if (this.shutdown) {
            log.warn("message store has shutdown, so putMessage is forbidden");
            return PutMessageStatus.SERVICE_NOT_AVAILABLE;
        }

        if (BrokerRole.SLAVE == this.messageStoreConfig.getBrokerRole()) {
            long value = this.printTimes.getAndIncrement();
            if ((value % 50000) == 0) {
                log.warn("broke role is slave, so putMessage is forbidden");
            }
            return PutMessageStatus.SERVICE_NOT_AVAILABLE;
        }

        if (!this.runningFlags.isWriteable()) {
            long value = this.printTimes.getAndIncrement();
            if ((value % 50000) == 0) {
                log.warn("the message store is not writable. It may be caused by one of the following reasons: " +
                    "the broker's disk is full, write to logic queue error, write to index file error, etc");
            }
            return PutMessageStatus.SERVICE_NOT_AVAILABLE;
        } else {
            this.printTimes.set(0);
        }

PutMessageStatus msgCheckStatus = this.checkMessage(msg);


```

```
2. 存储消息
PutMessageResult result = this.commitLog.putMessage(msg);
详细解析改方法了解过程

```

#### 存储文件组织与内存映射

RocketMQ通过使用内存映射文件来提高IO访问性能,无论是CommitLog,ConsumeQueue还是IndexFile,单个文件都被设计为固定长度,如果一个文件写满以后再创建一个新文件,文件名就为该文件第一条消息对应的全局物理偏移量.

![mappedqueue](https://upload.cc/i1/2021/04/06/rx2y47.png)


MappedFileQueue是MappedFile的管理容器,MappedFileQueue是对存储目录的封装.
MappedFile是RocketMQ内存映射文件的具体实现.

相关内容详见代码

#### RocketMQ存储文件

RocketMQ存储路径为${ROCKET_HOME}/store,store下文件结构

1. commitlog:消息存储目录
2. config:运行期间一些配置信息,包括
 - consumerFilter.json:主题消息过滤信息
 - consumerOffset.json:集群消费模式消息消费进度
 - delayOffset.json:延时消息队列拉取进度
 - subscriptionGroup.json:消息消费组配置信息
 - topics.json:topics配置属性
3. consumequeue:消息消费队列存储目录
4. index:消息索引文件存储目录
5. abort:如果存在abort文件说明Broker非正常关闭,该文件默认启动时创建,正常退出之前删除
6. checkpoint:文件检测点,存储commitlog,consumequeue,index最后一次刷盘时间戳,

##### 1.commitlog文件

commitLog消息组织方式

![](https://upload.cc/i1/2021/04/06/UvAub3.png)

##### 2.consumequeue文件

该文件可以看成是commitLog关于消息消费的"索引"文件,consumequeue的第一级目录为消息主题,第二级目录为主题的消息队列.

consumequeue中条目,一般一个文件存储30w条约为5.72M,单各consumequeue文件可以看出是一个consumequeue条目的数组,其下标为consumequeue的逻辑偏移量,消息消费进度存储的偏移量即逻辑偏移量.consumequeue即为Commitlog文件的索引文件,其构建机制是当消息到达Commitlog文件后,由专门的线程产生消息转发任务,从而构建消息消费队列与下文提出的索引文件.

消息的存储格式如下所示:

![](https://upload.cc/i1/2021/04/06/hMnWOB.png)

##### 2.Index索引文件

RocketMQ引入了Hash索引机制为消息建立索引,HashMap的设计好汉两个基本点:Hash槽与Hash冲突的链表结构.RocketMQ索引文件布局:

![](https://upload.cc/i1/2021/04/13/EznpNd.png)

hash槽的数据结构和index数据结构体现了hash冲突链式解决方案的关键实现,

hash槽存储了当前hashCode对应的index条目索引,当有相同的hashCode存放到hash槽时会覆盖之前的条目索引值,而index条目的最后4位存放了上一个条目的位置,可以通过这个字段遍历链表获取所有的hashCode相同的index条目.类似于hashMap的存储方式.

##### 3.checkpoint文件

checkpoint的作用是记录Commitlog,ConsumeQueue,Index文件的刷盘时间点,文件固定长度为4K,其中只用该文件的前面24字节.存储格式如下图所示:

![UTOOLS1618301664721.png](https://upload.cc/i1/2021/04/13/Uh2NOe.png)

#### 实时更新消息消费队列与索引文件
RocketMQ通过开启一个线程reputMessageService来准时转发CommitLog文件更新事件,相应的任务处理器根据转发的消息及时更新ConsumeQueue,IndexFile文件.

```
DefaultMessageStore#ReputMessageService
@Override
public void run() {
    DefaultMessageStore.log.info(this.getServiceName() + " service started");
    //ReputMessageService线程每执行一次任务推送休息1ms就继续尝试推送消息到消息消费队列和索引文件
    //消息消费转发的核心在doReput方法中实现
    while (!this.isStopped()) {
        try {
            Thread.sleep(1);
            this.doReput();
        } catch (Exception e) {
            DefaultMessageStore.log.warn(this.getServiceName() + " service has exception. ", e);
        }
    }

    DefaultMessageStore.log.info(this.getServiceName() + " service end");
}
```



```
ReputMessageService#doReput
if (dispatchRequest.isSuccess()) {
    if (size > 0) {
        //如果消息长度大于0,调用doDispatch方法,构件消息消费队列,索引文件
        DefaultMessageStore.this.doDispatch(dispatchRequest);
        .....
      }
    }  
    实际由 CommitLogDispatcherBuildConsumeQueue和 CommitLogDispatcherBuildIndex完成
        
```

#### 消息队列与索引文件恢复

因为RocketMQ所有消息全量存储在commitLog中,如果转发任务没有成功执行,Broker宕机了,如果不人工修复的话,那么一部分消息就会永久留在commitLog中.且由于消息没有转发到Consumequeue中,那么这部分消息永远不会被消费者消费.此时需要通过查看RocketMQ的存储文件加载机制来看最终一致性的实现.

```
DefaultMessageStore#load
boolean lastExitOK = !this.isTempFileExist();
log.info("last shutdown {}", lastExitOK ? "normally" : "abnormally");
```

>文件恢复主要完成flushPosition,committedWhere指针的设置,消息消费队列最大偏移量加载到内存,并删除flushPosition之后的文件.如果Broker异常启动,在文件恢复过程中,RocketMQ会将最后一个有效文件中的所有消息重新转发到消息消费队列和索引文件,确保不会丢失消息,但会造成消息的重复消费.

#### 文件刷盘机制

消息基于内存映射机制(MappedByteBuffer),首先追加到内存,根据刷盘策略异步/同步刷盘.如果是同步刷盘,则是追加完调用MappedByteBuffer.force方法;如果是异步刷盘,在消息追加到内存后立刻返回各消息发送端.commitLog.handleDiskFlush刷盘,其他两个与其类似.

```java
public void handleDiskFlush(AppendMessageResult result, PutMessageResult putMessageResult, MessageExt messageExt) {
        // Synchronization flush
        if (FlushDiskType.SYNC_FLUSH == this.defaultMessageStore.getMessageStoreConfig().getFlushDiskType()) {
            final GroupCommitService service = (GroupCommitService) this.flushCommitLogService;
            if (messageExt.isWaitStoreMsgOK()) {
                //构件GroupCommitRequest,并将同步任务提交到GroupCommitRequest
                GroupCommitRequest request = new GroupCommitRequest(result.getWroteOffset() + result.getWroteBytes());
                service.putRequest(request);
                CompletableFuture<PutMessageStatus> flushOkFuture = request.future();
                PutMessageStatus flushStatus = null;
                try {
                    //同步等待刷盘任务完成,超时为空
                    flushStatus = flushOkFuture.get(this.defaultMessageStore.getMessageStoreConfig().getSyncFlushTimeout(),
                            TimeUnit.MILLISECONDS);
                } catch (InterruptedException | ExecutionException | TimeoutException e) {
                    //flushOK=false;
                }
                if (flushStatus != PutMessageStatus.PUT_OK) {
                    log.error("do groupcommit, wait for flush failed, topic: " + messageExt.getTopic() + " tags: " + messageExt.getTags()
                        + " client address: " + messageExt.getBornHostString());
                    //刷盘失败返回任务超时
                    putMessageResult.setPutMessageStatus(PutMessageStatus.FLUSH_DISK_TIMEOUT);
                }
            } else {
                service.wakeup();
            }
        }
        // Asynchronous flush
        else {
            if (!this.defaultMessageStore.getMessageStoreConfig().isTransientStorePoolEnable()) {
                //flushCommitLogService = FlushRealTimeService
                flushCommitLogService.wakeup();
            } else {
                //commitLogService = CommitRealTimeService 如果transientStorePoolEnable为true,broker不为slave
                commitLogService.wakeup();
            }
        }
    }
```
#### 过期文件删除机制

RocketMQ所有的写操作都落在最后一个文件上,之前的文件不会被更新.所以如果非当前写文件在一定时间间隔内没有再次被更新,则认为是过期文件,可以被删除,不管他的消息有没有被全部消费.一般默认的过期时间为72小时.
```
执行顺序:DefaultMessageStore#addScheduleTask(DefaultMessageStore.this.cleanFilesPeriodically())  -> DefaultMessageStore#cleanFilesPeriodically(执行删除commitLog,consumeQueue文件) -> CleanCommitLogService#deleteExpiredFiles/CleanConsumeQueueService#deleteExpiredFiles

```



























