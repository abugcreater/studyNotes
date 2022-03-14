## RocketMQ主从同步(HA)机制

### RocketMQ主从复制原理

RocketMQ主从同步核心类:

![UTOOLS1620808974058.png](https://upload.cc/i1/2021/05/12/yQTr9n.png)

![UTOOLS1623220308920.png](https://upload.cc/i1/2021/06/09/Iy2OT1.png)

#### HAservice整体工作机制

```
HAservice#start
/**
 * HA实现原理
 * @throws Exception
 */
public void start() throws Exception {
    this.acceptSocketService.beginAccept();
    this.acceptSocketService.start();
    this.groupTransferService.start();
    this.haClient.start();
}
```

工作步骤:

1. 主服务器启动监听从节点的链接
2. 从服务器主动连接主节点,主服务器接受客户端的连接,并建立相关TCP连接
3. 从节点主动向主节点发送带拉取消息偏移量,主节点解析请求并返回
4. 从节点保存消息并继续发送新的消息同步请求

#### AcceptSocketService实现原理

![image-20210609153652118](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20210609153652118.png)

```
HAservice$AcceptSocketService#beginAccept
    /**
         * Starts listening to slave connections.
         *
         * @throws Exception If fails.
         */
        public void beginAccept() throws Exception {
            //创建 serverSocketChannel
            this.serverSocketChannel = ServerSocketChannel.open();
            //创建 selector
            this.selector = RemotingUtil.openSelector();
            //设置TCP ReuseAddress(允许绑定到已绑定的端口,复用套接字)
            this.serverSocketChannel.socket().setReuseAddress(true);
            //绑定监听端口
            this.serverSocketChannel.socket().bind(this.socketAddressListen);
            //设置为非阻塞模式
            this.serverSocketChannel.configureBlocking(false);
            //注册连接事件
            this.serverSocketChannel.register(this.selector, SelectionKey.OP_ACCEPT);
        }
```

```
/**
 * {@inheritDoc}
 */
@Override
public void run() {
    log.info(this.getServiceName() + " service started");

    while (!this.isStopped()) {
        try {
            this.selector.select(1000);
            Set<SelectionKey> selected = this.selector.selectedKeys();

            if (selected != null) {
                for (SelectionKey k : selected) {
                    if ((k.readyOps() & SelectionKey.OP_ACCEPT) != 0) {
                        SocketChannel sc = ((ServerSocketChannel) k.channel()).accept();

                        if (sc != null) {
                            HAService.log.info("HAService receive new connection, "
                                + sc.socket().getRemoteSocketAddress());

                            try {
                                HAConnection conn = new HAConnection(HAService.this, sc);
                                conn.start();
                                HAService.this.addConnection(conn);
                            } catch (Exception e) {
                                log.error("new HAConnection exception", e);
                                sc.close();
                            }
                        }
                    } else {
                        log.warn("Unexpected ops in select " + k.readyOps());
                    }
                }

                selected.clear();
            }
        } catch (Exception e) {
            log.error(this.getServiceName() + " service has exception.", e);
        }
    }

    log.info(this.getServiceName() + " service end");
}
```

该方法是标准的基于 NIO 的服务端程式实例，选择器每 1s 处理一次连接就绪事件 。 连接事件就绪后，调用 ServerSocketChannel 的 accept（）方法创建 SocketChannel 。 然后为每一 个连接创建一个 HAConnection 对象 ， 该 HAConnection 将负责 M-S 数据同步逻辑 。

#### GroupTransferService实现原理

GroupTransferService 主从同步阻塞实现，如果是同步主从模式，消息发 送 者将消 息刷写到磁盘后，需要继续等待新数据被传输到从服务器，从服务器数据的复 制 是在另 外 一 个线程 HAConnection 中去拉取，所以消息发送者在这里需要等待数据传输的结果， GroupTransferService 就是实现该功能，该类的 整体结构与同步刷盘实现类（ CommitLog$-GroupCommitService ）类似，本节只关注该类的核心业务逻辑 doWaitTransfer 的实现 。

```
/**
 * 为读请求设置属性()
 */
private void doWaitTransfer() {
    synchronized (this.requestsRead) {
        if (!this.requestsRead.isEmpty()) {
            //遍历读请求
            for (CommitLog.GroupCommitRequest req : this.requestsRead) {
                //判断请求偏移量是否满足推送到从节点的要求
                boolean transferOK = HAService.this.push2SlaveMaxOffset.get() >= req.getNextOffset();
                //获取最大等待时间
                long waitUntilWhen = HAService.this.defaultMessageStore.getSystemClock().now()
                    + HAService.this.defaultMessageStore.getMessageStoreConfig().getSyncFlushTimeout();
                while (!transferOK && HAService.this.defaultMessageStore.getSystemClock().now() < waitUntilWhen) {
                    this.notifyTransferObject.waitForRunning(1000);
                    transferOK = HAService.this.push2SlaveMaxOffset.get() >= req.getNextOffset();
                }

                if (!transferOK) {
                    log.warn("transfer messsage to slave timeout, " + req.getNextOffset());
                }

                req.wakeupCustomer(transferOK ? PutMessageStatus.PUT_OK : PutMessageStatus.FLUSH_SLAVE_TIMEOUT);
            }

            this.requestsRead.clear();
        }
    }
}
```

#### HAClient实现原理

![UTOOLS1623225999413.png](https://upload.cc/i1/2021/06/09/LZGn3s.png)

```
/**
 * 注册到主节点
 * @return
 * @throws ClosedChannelException
 */
private boolean connectMaster() throws ClosedChannelException {
    if (null == socketChannel) {
        //获取主节点地址
        String addr = this.masterAddress.get();
        if (addr != null) {
            //组装成InetSocketAddress
            SocketAddress socketAddress = RemotingUtil.string2SocketAddress(addr);
            //通过连接注册
            if (socketAddress != null) {
                this.socketChannel = RemotingUtil.connect(socketAddress);
                if (this.socketChannel != null) {
                    this.socketChannel.register(this.selector, SelectionKey.OP_READ);
                }
            }
        }
        //初始化偏移量为commitlog最大物理偏移量
                this.currentReportedOffset = HAService.this.defaultMessageStore.getMaxPhyOffset();
                //初始化写入时间戳为当前时间戳
                this.lastWriteTimestamp = System.currentTimeMillis();
    }

    return this.socketChannel != null;
}
如果没有连接到master会使socketChannel==null,返回false,且不进行主从同步
```

```
/**
 * 将系统当前时间和最近的写时间做比较,返回是否大于心跳间隔
 * @return
 */
private boolean isTimeToReportOffset() {
    long interval =
        HAService.this.defaultMessageStore.getSystemClock().now() - this.lastWriteTimestamp;
    boolean needHeart = interval > HAService.this.defaultMessageStore.getMessageStoreConfig()
        .getHaSendHeartbeatInterval();

    return needHeart;
}
```

```
/**
 * 将系统当前时间和最近的写时间做比较,返回是否大于心跳间隔
 * @return
 */
private boolean isTimeToReportOffset() {
    long interval =
        HAService.this.defaultMessageStore.getSystemClock().now() - this.lastWriteTimestamp;
    boolean needHeart = interval > HAService.this.defaultMessageStore.getMessageStoreConfig()
        .getHaSendHeartbeatInterval();

    return needHeart;
}
双重意义:对于slave端,是发送下次待拉取消息偏移量;对于master端,认为slave本次请求拉取的消息偏移量,也可以认为slave消息同步ACK确认
```

```
/**
 * 向主节点报告从节点最大偏移量
 * @param maxOffset
 * @return
 */
private boolean reportSlaveMaxOffset(final long maxOffset) {
    //position设置为0,limit待写入长度putLong待拉取偏移量
    this.reportOffset.position(0);
    this.reportOffset.limit(8);
    this.reportOffset.putLong(maxOffset);
    //写模式切换到读模式
    this.reportOffset.position(0);
    this.reportOffset.limit(8);

    for (int i = 0; i < 3 && this.reportOffset.hasRemaining(); i++) {
        try {
            this.socketChannel.write(this.reportOffset);
        } catch (IOException e) {
            log.error(this.getServiceName()
                + "reportSlaveMaxOffset this.socketChannel.write exception", e);
            return false;
        }
    }
	//更新写时间戳
    lastWriteTimestamp = HAService.this.defaultMessageStore.getSystemClock().now();
    return !this.reportOffset.hasRemaining();
}
```

```
/**
 * 处理读请求事件,处理从master服务器传回来的数据
 * @return
 */
private boolean processReadEvent() {
    int readSizeZeroTimes = 0;
    //如果byteBufferRead还有剩余空间,则将通道中数据读到缓存空间
    while (this.byteBufferRead.hasRemaining()) {
        try {
            int readSize = this.socketChannel.read(this.byteBufferRead);
            if (readSize > 0) {
                //如果读到字节数大于0则重置读到0次数
                readSizeZeroTimes = 0;
                //转发读请求,将消息追加到内存映射文件中
                boolean result = this.dispatchReadRequest();
                if (!result) {
                    log.error("HAClient, dispatchReadRequest error");
                    return false;
                }
            } else if (readSize == 0) {
                //如果连续3次读到字节数等于0则结束本次连接
                if (++readSizeZeroTimes >= 3) {
                    break;
                }
            } else {
                log.info("HAClient, processReadEvent read socket < 0");
                return false;
            }
        } catch (IOException e) {
            log.info("HAClient, processReadEvent read socket exception", e);
            return false;
        }
    }

    return true;
}
```

HAClient线程反复执行上述5个步骤完成主从同步复制功能

#### HAConnection实现原理

Master 服务器在收到从服务器的连接请求后，会将主从服务器的连接 SocketChannel 封装成 HAConnection 对象，实现主服务器与从服务器的读写操作 。 

![UTOOLS1623229276479.png](https://upload.cc/i1/2021/06/09/RcyuwS.png)





![UTOOLS1623303052097.png](https://upload.cc/i1/2021/06/10/5HcZXO.png)

### RocketMQ读写分离机制

MQ根据`MessageQueue.brokerName`获取同一组(M-S)下的服务器,然后跟brokerId区分主从,等于0的是Master,大于0的是从服务器.根据`MQClientInstance.findBrokerAddressInSubscribe`方法

```java
/**
 * 根据brokerName,brokerId查找broker地址
 * @param brokerName
 * @param brokerId
 * @param onlyThisBroker
 * @return
 */
public FindBrokerResult findBrokerAddressInSubscribe(
    final String brokerName,
    final long brokerId,
    final boolean onlyThisBroker
) {
    String brokerAddr = null;
    boolean slave = false;
    boolean found = false;

    HashMap<Long/* brokerId */, String/* address */> map = this.brokerAddrTable.get(brokerName);
    if (map != null && !map.isEmpty()) {
        brokerAddr = map.get(brokerId);
        slave = brokerId != MixAll.MASTER_ID;
        found = brokerAddr != null;

        if (!found && slave) {
            brokerAddr = map.get(brokerId + 1);
            found = brokerAddr != null;
        }

        if (!found && !onlyThisBroker) {
            Entry<Long, String> entry = map.entrySet().iterator().next();
            brokerAddr = entry.getValue();
            slave = entry.getKey() != MixAll.MASTER_ID;
            found = true;
        }
    }

    if (found) {
        return new FindBrokerResult(brokerAddr, slave, 
        //获取broker版本
        findBrokerVersion(brokerName, brokerAddr));
    }

    return null;
}
```

```Java
/**
 * 根据消息队列返回brokerId
 * @param mq
 * @return
 */
public long recalculatePullFromWhichNode(final MessageQueue mq) {
    if (this.isConnectBrokerByUser()) {
        return this.defaultBrokerId;
    }

    AtomicLong suggest = this.pullFromWhichNodeTable.get(mq);
    if (suggest != null) {
        return suggest.get();
    }

    return MixAll.MASTER_ID;
}
```

`pullFromWhichNodeTable`缓存从`PullAPIWrapper.updatePullFromWhichNode`处更新,处理拉取请求时同步更新缓存.

`processPullResult.updatePullFromWhichNode(mq, pullResultExt.getSuggestWhichBrokerId());`

```java
public void updatePullFromWhichNode(final MessageQueue mq, final long brokerId) {
    AtomicLong suggest = this.pullFromWhichNodeTable.get(mq);
    if (null == suggest) {
        this.pullFromWhichNodeTable.put(mq, new AtomicLong(brokerId));
    } else {
        suggest.set(brokerId);
    }
}
```

```java
DefaultMessageStore#getMessage 中更新设置上述brokerId
long diff /* 当前未被消息消费端拉取的消息长度 */ = maxOffsetPy /* 当前主服务器存储消息的最大偏移量 */ - maxPhyOffsetPulling /* 此次消息拉取的偏移量 */;
long memory = (long) (StoreUtil.TOTAL_PHYSICAL_MEMORY_SIZE /* 服务器总内存大小 */
                            * (this.messageStoreConfig.getAccessMessageInMemoryMaxRatio() / 100.0) /*设置的消息内存最大比例*/);
 //如果 diff > memory表示拉取的消息大小已经超过内存空间,此时建议从从服务器总拉取
 getResult.setSuggestPullingFromSlave(diff > memory);
```

```Java
PullMessageProcessor#processRequest
//根据主从同步延迟设置下一次拉取的BrokerId
if (getMessageResult.isSuggestPullingFromSlave()) {
    responseHeader.setSuggestWhichBrokerId(subscriptionGroupConfig.getWhichBrokerWhenConsumeSlowly());
} else {
    responseHeader.setSuggestWhichBrokerId(MixAll.MASTER_ID);
}
```

### 总结

RocketMQ的HA机制是,从服务器向主服务器建立TCP连接,从服务器中拉取commitlog的偏移量,并以此偏移量向主服务器拉取消息,主服务器根据偏移量向从服务发送一批消息,此过程循环执行.不同的是,主服务器可以建议从服务器从Master节点拉取还是从其他slave节点拉取.

