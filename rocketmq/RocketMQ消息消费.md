## RocketMQ消息消费

### RocketMQ消费概述

消息消费以组的形式进行,一个消费组中有多个消费者,每个消费者可订阅多个主题,消息组之间有集群模式和广播模式.

RocketMQ支持局部顺序消息消费,保证同一个消息队列上的消息顺序消费,不支持全局顺序消费.

RocketMQ支持过滤模式为:表达式(TAG,SQL92)和类模式.

### 消费者启动流程

```java
DefaultMQPushConsumer#start
public void start() throws MQClientException {
    setConsumerGroup(NamespaceUtil.wrapNamespace(this.getNamespace(), this.consumerGroup));
    this.defaultMQPushConsumerImpl.start();
    if (null != traceDispatcher) {
        try {
            traceDispatcher.start(this.getNamesrvAddr(), this.getAccessChannel());
        } catch (MQClientException e) {
            log.warn("trace dispatcher start failed ", e);
        }
    }
}
```
### 消息拉取

消费者启动时,初始化并启动了`MQClientInstance`,`MQClientInstance`启动时启动了`pullMessageService`线程负责消息的拉取.`pullMessageService`继承了`ServiceThread`服务线程.
#### PullMessageService实现机制



```
@Override
public void run() {
    log.info(this.getServiceName() + " service started");

    while (!this.isStopped()) {
        try {
            PullRequest pullRequest = this.pullRequestQueue.take();
            this.pullMessage(pullRequest);
        } catch (InterruptedException ignored) {
        } catch (Exception e) {
            log.error("Pull Message Service Run Method exception", e);
        }
    }

    log.info(this.getServiceName() + " service end");
}
```

![UTOOLS1620364801335.png](https://upload.cc/i1/2021/05/07/QynOjF.png)



RocketMQ没有真正实现推模式,而是消费者主动向消息服务器拉取消息,RocketMQ的推模式是循环想消息服务器发送消息拉取请求.



```

PullMessageProcessor#processRequest
case ResponseCode.PULL_NOT_FOUND:
    //消息未找到且支持挂起
    if (brokerAllowSuspend && hasSuspendFlag) {
        long pollingTimeMills = suspendTimeoutMillisLong;
        if (!this.brokerController.getBrokerConfig().isLongPollingEnable()) {
            pollingTimeMills = this.brokerController.getBrokerConfig().getShortPollingTimeMills();
        }

        String topic = requestHeader.getTopic();
        long offset = requestHeader.getQueueOffset();
        int queueId = requestHeader.getQueueId();
        PullRequest pullRequest = new PullRequest(request, channel, pollingTimeMills,
            this.brokerController.getMessageStore().now(), offset, subscriptionData, messageFilter);
        this.brokerController.getPullRequestHoldService().suspendPullRequest(topic, queueId, pullRequest);
        response = null;
        break;
    }
```



### 消息队列负载与重新分布机制

RocketMQ实现消息队列重新分布是由RebalanceService来实现的.MQClientInstance持有rebalanceService,随着MQClientInstance的启动而启动.

```
RebalanceService#run
@Override
public void run() {
    log.info(this.getServiceName() + " service started");

    while (!this.isStopped()) {
        this.waitForRunning(waitInterval);
        this.mqClientFactory.doRebalance();
    }

    log.info(this.getServiceName() + " service end");
}
```

实际实现方法

```
RebalanceImpl#doRebalance

public void doRebalance(final boolean isOrder) {
    Map<String, SubscriptionData> subTable = this.getSubscriptionInner();
    if (subTable != null) {
        for (final Map.Entry<String, SubscriptionData> entry : subTable.entrySet()) {
            final String topic = entry.getKey();
            try {
                this.rebalanceByTopic(topic, isOrder);
            } catch (Throwable e) {
                if (!topic.startsWith(MixAll.RETRY_GROUP_TOPIC_PREFIX)) {
                    log.warn("rebalanceByTopic Exception", e);
                }
            }
        }
    }

    this.truncateMessageQueueNotMyTopic();
}
```

![UTOOLS1620631099504.png](https://upload.cc/i1/2021/05/10/2Mv3LU.png)


### 消息消费过程

ConsumeMessageService实现消息消费的处理逻辑.
![UTOOLS1620631335473.png](https://upload.cc/i1/2021/05/10/nrzihZ.png)

```
ConsumeMessageConcurrentlyService#submitConsumeRequest#run 并发消息消费(广播模式)
```
#### 消息确认(ACK)

如果ACK消息发送失败,则会延迟5s重新消费.网络客户端Ack消息发送的入口:`MQClientAPIImpl#consumerSendMessageBack`,命令编码`RequestCode.CONSUMER_SEND_MSG_BACK`.

服务器应答为 `SendMessageProcessor#asyncConsumerSendMsgBack`

#### 消费进度管理
广播模式: 同一个消费组中的所有消费者消费所有的消息,所以需要独立保存消费进度
集群模式:同一个消费组中的消费者,共享所有消息,同一时间,同一条消息只能被消费一次,所以消费进度要保存在同一个地方
消费进度接口为`OffsetStore`

![UTOOLS1620711206269.png](https://upload.cc/i1/2021/05/11/WoLEYR.png)

##### 集群模式消费进度存储

实现类`org.apache.rocketmq.client.consumer.store.RemoteBrokerOffsetStore`,实现原理

![image-20210511135526969](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20210511135526969.png)

### 定时消息机制

定时消息是指,消息发送到broker后不立刻被消费,而是延迟一定时间后再被消费.RocketMQ不支持任意精度的时间格式,而是支持特定级别的延迟级别.

![UTOOLS1620713579483.png](https://upload.cc/i1/2021/05/11/aKhr1x.png)

RocketMQ的定时消息实现类是`org.apache.rocketmq.store.schedule.ScheduleMessageService`,该类是在`DefaultMessageStore`被实例化,`DefaultMessageStore`启动时会调用`ScheduleMessageService#start`

`ScheduleMessageService`方法加载顺序 构造方法 -> load() -> start()

延迟队列实现原理:

![UTOOLS1620718084426.png](https://upload.cc/i1/2021/05/11/bMkYut.png)

### 消息过滤机制

RocketMQ支持表达式过滤(TAG和SQL92)与类过滤.
类过滤消费者提交一个类到FilterServer,经过FilterServer处理后拉取.
SQL92表达式通过消息上下文过滤消息.
TAG表达式消息通过标签过滤消息.
实现类`org.apache.rocketmq.store.MessageFilter`

```java
public interface MessageFilter {
    /**
     * 根据ConsumeQueue判断消息是否匹配
     * match by tags code or filter bit map which is calculated when message received
     * and stored in consume queue ext.
     *
     * @param tagsCode tagsCode  主要通过tagsCode
     * @param cqExtUnit extend unit of consume queue
     */
    boolean isMatchedByConsumeQueue(final Long tagsCode,
        final ConsumeQueueExt.CqExtUnit cqExtUnit);

    /**
     * 根据存储在CommitLog中的内容判断消息是否匹配
     * match by message content which are stored in commit log.
     * <br>{@code msgBuffer} and {@code properties} are not all null.If invoked in store,
     * {@code properties} is null;If invoked in {@code PullRequestHoldService}, {@code msgBuffer} is null.
     *
     * @param msgBuffer message buffer in commit log, may be null if not invoked in store.
     * @param properties message properties, should decode from buffer if null by yourself. 主要用于SQL92过滤模式
     */
    boolean isMatchedByCommitLog(final ByteBuffer msgBuffer,
        final Map<String, String> properties);
}
```



### 顺序消息

根据并发消息消费流程,消息消费包括:消息队列负载,消息拉取,消息消费,消息消费进度存储

顺序消费实现类`org.apache.rocketmq.client.impl.consumer.ConsumeMessageOrderlyService`

构件`ConsumeRequest`请求体,调用其`run()`方法执行.

服务端消费队列`MessageQueue`加锁处理类`RebalanceLockManager`.

![UTOOLS1620725148010.png](https://upload.cc/i1/2021/05/11/z27OAR.png)



![UTOOLS1620725765243.png](https://upload.cc/i1/2021/05/11/yxmque.png)









