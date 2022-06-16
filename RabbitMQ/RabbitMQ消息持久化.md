# RabbitMQ消息持久化

RabbitMQ在三个维度上进行持久化,分别是队列(queue),消息持久化(message)和交换机(exchange).

## 队列持久化

队列持久化通过在声明队列时,设置参数为true实现.

完整的队列定义如下:

```java
/**
 * Declare a queue
 * @see com.rabbitmq.client.AMQP.Queue.Declare
 * @see com.rabbitmq.client.AMQP.Queue.DeclareOk
 * @param queue the name of the queue
 * @param durable true if we are declaring a durable queue (the queue will survive a server restart)
 * @param exclusive true if we are declaring an exclusive queue (restricted to this connection) 是否是排他队列
 * @param autoDelete true if we are declaring an autodelete queue (server will delete it when no longer in use) 是否自动删除队列
 * @param arguments other properties (construction arguments) for the queue
 * @return a declaration-confirm method to indicate the queue was successfully declared
 * @throws java.io.IOException if an error is encountered
 */
Queue.DeclareOk queueDeclare(String queue, boolean durable, boolean exclusive, boolean autoDelete,
                             Map<String, Object> arguments) throws IOException;
```
> 排他队列:仅对首次声明他的连接可见,在连接断开时自动删除.

案例如下:
```java
Connection connection = connectionFactory.newConnection();
Channel channel = connection.createChannel();
//第二个参数是持久化标识
channel.queueDeclare("queue.persistent.name", true, false, false, null);
```
## 消息持久化

队列持久化仅仅是保存队列相关信息,当服务重启时也可以能看到之前被持久化的队列.而队列中的消息是否存在取决于消息是否持久化.消息持久化取决于,发送消息是是否设置了持久化标识.

消息发送通过`basicPublish`方法实现,关键是参数中的`MessageProperties.PERSISTENT_TEXT_PLAIN`

```java
//交换机名称,路由键,基本策略和消息体
void basicPublish(String exchange, String routingKey, BasicProperties props, byte[] body) throws IOException;
//mandatory 标识没有对应的队列会将消息回退到生产者
void basicPublish(String exchange, String routingKey, boolean mandatory, BasicProperties props, byte[] body)
        throws IOException;
//immediate标识没有消费者,则将消息回退
void basicPublish(String exchange, String routingKey, boolean mandatory, boolean immediate, BasicProperties props, byte[] body)
        throws IOException;

```

这里关键的是BasicProperties props这个参数了，这里看下BasicProperties的定义：

```java
public BasicProperties(
            String contentType,//消息类型如：text/plain
            String contentEncoding,//编码
            Map<String,Object> headers,
            Integer deliveryMode,//1:nonpersistent 不持久化 2:persistent 持久化
            Integer priority,//优先级
            String correlationId,
            String replyTo,//反馈队列
            String expiration,//expiration到期时间
            String messageId,
            Date timestamp,
            String type,
            String userId,
            String appId,
            String clusterId)
```

实现案例

```java
channel.basicPublish("exchange.persistent", "persistent", MessageProperties.PERSISTENT_TEXT_PLAIN, "persistent_test_message".getBytes());
```



设置了队列和消息的持久化之后，当broker服务重启的之后，消息依旧存在。单只设置队列持久化，重启之后消息会丢失；单只设置消息的持久化，重启之后队列消失，既而消息也丢失。单单设置消息持久化而不设置队列的持久化显得毫无意义。

## 交换器持久化

exchange不持久化对消息的可靠性没什么影响,但是由于重启后交换器不存在,会导致无法发送消息.

声明交换器相关方法

```java
Exchange.DeclareOk exchangeDeclare(String exchange, String type, boolean durable) throws IOException;
Exchange.DeclareOk exchangeDeclare(String exchange, String type, boolean durable, boolean autoDelete,
                                   Map<String, Object> arguments) throws IOException;
Exchange.DeclareOk exchangeDeclare(String exchange, String type) throws IOException;
Exchange.DeclareOk exchangeDeclare(String exchange,
                                          String type,
                                          boolean durable,
                                          boolean autoDelete,
                                          boolean internal,
                                          Map<String, Object> arguments) throws IOException;
void exchangeDeclareNoWait(String exchange,
                           String type,
                           boolean durable,
                           boolean autoDelete,
                           boolean internal,
                           Map<String, Object> arguments) throws IOException;
Exchange.DeclareOk exchangeDeclarePassive(String name) throws IOException;
```

## 其他问题

1. 当三者都被设置持久化后,消息是否还会丢失

   会的,当消息消费开启自动ack,消费者还没处理这条消息,但是进程crash,会导致消息丢失.这种情况也好处理，只需将autoAck设置为false(方法定义如下)，然后在正确处理完消息之后进行手动ack（channel.basicAck）.

2. 消息落盘机制

   写入文件前会有一个Buffer,大小为1M,数据在写入文件时，首先会写入到这个Buffer，如果Buffer已满，则会将Buffer写入到文件（未必刷到磁盘）。
   有个固定的刷盘时间：25ms,也就是不管Buffer满不满，每个25ms，Buffer里的数据及未刷新到磁盘的文件内容必定会刷到磁盘。
   每次消息写入后，如果没有后续写入请求，则会直接将已写入的消息刷到磁盘：使用Erlang的receive x after 0实现，只要进程的信箱里没有消息，则产生一个timeout消息，而timeout会触发刷盘操作。
   
   消费者在订阅队列时，可以指定autoAck参数，当autoAck等于false时，RabbitMQ会等待消费者显式地回复确认信号后才从内存（或者磁盘）中移去消息（实质上是先打上删除标记，之后再删除）

当满足以下条件时mq会删除消息:

1. 存放超过72个小时的消息，过期. 通过fileReservedTime 参数设置.
2. 每天凌晨4天清除过期的consume queue和log文件.通过deleteWhen 参数控制
3. 磁盘使用空间超过了75%，开始删除过期文件；超过85%，会开始批量清理文件，不管有没有过期；超过90%，会拒绝消息写入

   





参考:[RabbitMQ之消息持久化](https://blog.csdn.net/u013256816/article/details/60875666)