使用rocketmq的producer发送消息时碰到No route info of this topic错误



查看源码发现,BrokerStartup 在start方法中会先初始化相应的配置

```java
final BrokerController controller = new BrokerController(
                brokerConfig,
                nettyServerConfig,
                nettyClientConfig,
                messageStoreConfig);
```

执行完构造方法后调用`initialize` 此时会将创建一系列的线程并初始化,如果此时broker中配置了namesrvAddr的话,就会更新`brokerOuterAPI` 中的`remotingClient` 的 `namesrvAddrList`属性

```java
this.brokerOuterAPI.updateNameServerAddressList(this.brokerConfig.getNamesrvAddr());
```

初始完配置信息,调用controller.start(); 方法启动broker,将依次启动各个模块并向namesrv进行注册.