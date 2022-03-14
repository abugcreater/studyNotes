## 消息过滤FilterSever

### ClassFilter运行机制

RocketMQ类过滤模式允许消费者将代码上传到FilterSever中,消费者向FilterSever拉取消息,FilterSever将拉取请求转发到Broker,然后对返回消息进行逻辑过滤,最后将消息返回消费者端.

![UTOOLS1620804395461.png](https://upload.cc/i1/2021/05/12/L3wKaS.png)

注册FilterServer方法

`AdminBrokerProcessor#registerFilterServer`

```
private RemotingCommand registerFilterServer(ChannelHandlerContext ctx,
    RemotingCommand request) throws RemotingCommandException {
    final RemotingCommand response = RemotingCommand.createResponseCommand(RegisterFilterServerResponseHeader.class);
    final RegisterFilterServerResponseHeader responseHeader = (RegisterFilterServerResponseHeader) response.readCustomHeader();
    final RegisterFilterServerRequestHeader requestHeader =
        (RegisterFilterServerRequestHeader) request.decodeCommandCustomHeader(RegisterFilterServerRequestHeader.class);

    this.brokerController.getFilterServerManager().registerFilterServer(ctx.channel(), requestHeader.getFilterServerAddr());

    responseHeader.setBrokerId(this.brokerController.getBrokerConfig().getBrokerId());
    responseHeader.setBrokerName(this.brokerController.getBrokerConfig().getBrokerName());

    response.setCode(ResponseCode.SUCCESS);
    response.setRemark(null);
    return response;
}
```

Broker自己扫描并填充因心跳超时而删除的Filter

```java
FilterServerManager#start
{

    this.scheduledExecutorService.scheduleAtFixedRate(new Runnable() {
        @Override
        public void run() {
            try {
                //定时扫描,检测存活filter个数
                FilterServerManager.this.createFilterServer();
            } catch (Exception e) {
                log.error("", e);
            }
        }
    }, 1000 * 5, 1000 * 30, TimeUnit.MILLISECONDS);
}

public void createFilterServer() {
        int more =
            this.brokerController.getBrokerConfig().getFilterServerNums() - this.filterServerTable.size();
        //构件shell名并调用
        String cmd = this.buildStartCommand();
        for (int i = 0; i < more; i++) {
            /**
             * 构件启动命令,并调用
             */
            FilterServerUtil.callShell(cmd, log);
        }
    }
```

### 类过滤模式订阅机制

RocketMQ通过`DefaultMQPushConsumerImpl#subscribe`方法订阅信息.

```
//创建订阅的过滤信息
SubscriptionData subscriptionData = FilterAPI.buildSubscriptionData(this.defaultMQPushConsumer.getConsumerGroup(),
    topic, subExpression);
this.rebalanceImpl.getSubscriptionInner().put(topic, subscriptionData);
```