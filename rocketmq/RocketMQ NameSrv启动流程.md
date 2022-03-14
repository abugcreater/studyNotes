## RocketMQ NameSrv启动流程

启动类`NamesrvStartup` 
启动流程 第一步调用`createNamesrvController` 方法解析配置文件,填充`namesrvConfig`和`nettyServerConfig`属性

```
if (commandLine.hasOption('c')) {
    String file = commandLine.getOptionValue('c');
    if (file != null) {
        namesrvConfig.setConfigStorePath(file);
		....
        System.out.printf("load config properties file OK, %s%n", file);
        in.close();
    }
}

if (commandLine.hasOption('p')) {
			 ...
            MixAll.printObjectProperties(console, namesrvConfig);
            MixAll.printObjectProperties(console, nettyServerConfig);
            System.exit(0);
        }

```

通过以上代发可以发现,通过`c`,`p`来接受指令

第二步根据配置属性, 创建`NamesrvController`实例,并初始化,`NamesrvController` 是`NameSrv`的核心控制器

```
final NamesrvController controller = new NamesrvController(namesrvConfig, nettyServerConfig);

// remember all configs to prevent discard
controller.getConfiguration().registerConfig(properties);
```

```java
public static NamesrvController start(final NamesrvController controller) throws Exception {

    if (null == controller) {
        throw new IllegalArgumentException("NamesrvController is null");
    }

    boolean initResult = controller.initialize();
    if (!initResult) {
        controller.shutdown();
        System.exit(-3);
    }
	....
    controller.start();
    return controller;
}
```

最后一步 注册JVM钩子函数,当应用退出时,注销相关线程池服务

```java
Runtime.getRuntime().addShutdownHook(new ShutdownHookThread(log, new Callable<Void>() {
    @Override
    public Void call() throws Exception {
        controller.shutdown();
        return null;
    }
}));

  public void shutdown() {
        this.remotingServer.shutdown();
        this.remotingExecutor.shutdown();
        this.scheduledExecutorService.shutdown();

        if (this.fileWatchService != null) {
            this.fileWatchService.shutdown();
        }
    }
```



Broker注册/心跳流程

在`BrokerController` 的  `start`方法中,在定时线程池中每10s运行一次`registerBrokerAll`方法

```
this.scheduledExecutorService.scheduleAtFixedRate(new Runnable() {

    @Override
    public void run() {
        try {
            //每10s中发送心跳
            BrokerController.this.registerBrokerAll(true, false, brokerConfig.isForceRegister());
        } catch (Throwable e) {
            log.error("registerBrokerAll Exception", e);
        }
    }
}, 1000 * 10, Math.max(10000, Math.min(brokerConfig.getRegisterNameServerPeriod(), 60000)), TimeUnit.MILLISECONDS);
```

`registerBrokerAll` 调用链为 `registerBrokerAll` -> `doRegisterBrokerAll` -> `registerBrokerAll` 实际内容由 `registerBrokerAll` 完成

```
final List<RegisterBrokerResult> registerBrokerResultList = new CopyOnWriteArrayList<>();
List<String> nameServerAddressList = this.remotingClient.getNameServerAddressList();
if (nameServerAddressList != null && nameServerAddressList.size() > 0) {

    final RegisterBrokerRequestHeader requestHeader = new RegisterBrokerRequestHeader();
    requestHeader.setBrokerAddr(brokerAddr);
    requestHeader.setBrokerId(brokerId);
    requestHeader.setBrokerName(brokerName);
    requestHeader.setClusterName(clusterName);
    requestHeader.setHaServerAddr(haServerAddr);
    requestHeader.setCompressed(compressed);

    RegisterBrokerBody requestBody = new RegisterBrokerBody();
    requestBody.setTopicConfigSerializeWrapper(topicConfigWrapper);
    requestBody.setFilterServerList(filterServerList);
    final byte[] body = requestBody.encode(compressed);
    final int bodyCrc32 = UtilAll.crc32(body);
    requestHeader.setBodyCrc32(bodyCrc32);
    final CountDownLatch countDownLatch = new CountDownLatch(nameServerAddressList.size());
    for (final String namesrvAddr : nameServerAddressList) {
        brokerOuterExecutor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    RegisterBrokerResult result = registerBroker(namesrvAddr,oneway, timeoutMills,requestHeader,body);
                    if (result != null) {
                        registerBrokerResultList.add(result);
                    }

                    log.info("register broker[{}]to name server {} OK", brokerId, namesrvAddr);
                } catch (Exception e) {
                    log.warn("registerBroker Exception, {}", namesrvAddr, e);
                } finally {
                    countDownLatch.countDown();
                }
            }
        });
    }
```

该方法就是遍历nameserver列表,依次发送心跳包











