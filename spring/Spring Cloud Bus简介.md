# Spring Cloud Bus简介

> Spring Cloud Bus links the nodes of a distributed system with a lightweight message broker. This broker can then be used to broadcast state changes (such as configuration changes) or other management instructions. A key idea is that the bus is like a distributed actuator for a Spring Boot application that is scaled out. However, it can also be used as a communication channel between apps. This project provides starters for either an AMQP broker or Kafka as the transport.

简单来说:`spring cloud bus`提供了分布式节点间广播通信的能力,一般基于AMQP或者kafka传输.一般用来广播配置更新事件,节点收到该事件时,会刷新配置.



## 使用方法

引入依赖

```
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-bus-amqp</artifactId>
    <version>2.2.4.RELEASE</version>
</dependency>
```

配置rabbitmq 的相关属性

```
spring:
  rabbitmq:
    host: mybroker.com
    port: 5672
    username: user
    password: secret
```

`Spring Cloud Bus`中的事件基于`RemoteApplicationEvent`,所以我们自定义的事件需要继承`RemoteApplicationEvent`.

```
//自定义类
public class xxEvent extends RemoteApplicationEvent {

    public xxEvent(Object source, String originService, String destinationService) {
        super(source, originService, destinationService);

    }
}
```

Listener类可以通过实现 `ApplicationListener`接口,也可以通过注解 `@EventListener`,与正常的`Spring`事件驱动一致.

```
@Component
public class xxEventListener  {

    @EventListener(classes = xxEvent.class)
    public void listener(xxEvent event){
    	System.out.print("hello world");
    }
}
```

如果想要`Spring Cloud Bus`识别事件,需要将该类放在`org.springframework.cloud.bus.event`子包下,或者添加`@RemoteApplicationEventScan`注解,将事件的包路径添加到扫描范围.

```
@Configuration
@RemoteApplicationEventScan(basePackages = "com.xx.event")
public class BusConfiguration {
}
```

项目启动后可以看到有绑定`bus`的队列被创建.

![image-20211011134115976](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20211011134115976.png)

## 基本原理

### Spring事件驱动基本概念

`Spring Cloud Bus`自定义事件,基于`Spring`中的事件驱动,启动时注册观察者,当事件发布后,会遍历通知观察者,如果没有配置线程池会顺序执行各自的逻辑.

```java
SimpleApplicationEventMulticaster#multicastEvent

//遍历获取观察者    
public void multicastEvent(final ApplicationEvent event, @Nullable ResolvableType eventType) {
   ResolvableType type = (eventType != null ? eventType : resolveDefaultEventType(event));
   for (final ApplicationListener<?> listener : getApplicationListeners(event, type)) {
      Executor executor = getTaskExecutor();
      if (executor != null) {
         executor.execute(() -> invokeListener(listener, event));
      }
      else {
         invokeListener(listener, event);
      }
   }
}
```

```Java
SimpleApplicationEventMulticaster#doInvokeListener
//调用onApplicationEvent方法执行
private void doInvokeListener(ApplicationListener listener, ApplicationEvent event) {
		try {
			listener.onApplicationEvent(event);
		}
		catch (ClassCastException ex) {
			String msg = ex.getMessage();
			if (msg == null || matchesClassCastMessage(msg, event.getClass().getName())) {
				// Possibly a lambda-defined listener which we could not resolve the generic event type for
				// -> let's suppress the exception and just log a debug message.
				Log logger = LogFactory.getLog(getClass());
				if (logger.isDebugEnabled()) {
					logger.debug("Non-matching event type for listener: " + listener, ex);
				}
			}
			else {
				throw ex;
			}
		}
	}
```

### Bus基本概念

`Spring Cloud Bus`为每个实例生成唯一的`service ID`,也可以通过`spring.cloud.bus.id`配置.默认的值是通过`spring.application.name`和`server.port`或`spring.application.index`(不缺省)组合,组合为 `app:index:id`,`id`是通过`vcap.application.instance_id`设置,如果没有设置则随机生成一个值.

`service ID`值必须唯一,bus 会处理两类事件,一类是本地的`ApplicationEvent`,另一类是来自队列的事件.这时候,bus会根据`service ID`来判断事件来源,如果有多个实例有相同的`service ID`,那该事件将不会被处理.为了确保`service ID`唯一可以设置`spring.application.index`.

`Spring Cloud Bus`中`RemoteApplicationEvent`的基本属性.

```

//源服务的serviceId
private final String originService;
//目标服务,放空则设置为通配符"**"
private final String destinationService;
//事件的id,在构造函数中生成
private final String id;
```

`serviceId`在`RemoteApplicationEvent`中用于表示消息生产者.

`ServiceMatcher`中根据`serviceId`判断是否来自本机及对象是否包含本机.

```java
public boolean isFromSelf(RemoteApplicationEvent event) {
   String originService = event.getOriginService();
   String serviceId = getServiceId();
   return this.matcher.match(originService, serviceId);
}

public boolean isForSelf(RemoteApplicationEvent event) {
   String destinationService = event.getDestinationService();
   return (destinationService == null || destinationService.trim().isEmpty()
         || this.matcher.match(destinationService, getServiceId()));
}
```



### Bus 具体实现逻辑实现

 `BusAutoConfiguration` 处理了本地和从队列中推送的事件.

`acceptLocal()`方法监听本地`RemoteApplicationEvent`事件,当前服务发布了非`AckRemoteApplicationEvent`类型的`RemoteApplicationEvent`事件时,该方法会将事件组装成`Message`格式,并投递到队列中.

```Java
@EventListener(classes = RemoteApplicationEvent.class)
public void acceptLocal(RemoteApplicationEvent event) {
   if (this.serviceMatcher.isFromSelf(event)
         && !(event instanceof AckRemoteApplicationEvent)) {
      this.cloudBusOutboundChannel.send(MessageBuilder.withPayload(event).build());
   }
}
```

收到队列消息后判断消息类型,调用 `acceptRemote`方法.

如果是ACK类型且开启了追踪,消息发送者不为当前服务,则发布事件,由对应观察者处理.

```java

if (event instanceof AckRemoteApplicationEvent) {
      if (this.bus.getTrace().isEnabled() && !this.serviceMatcher.isFromSelf(event)
            && this.applicationEventPublisher != null) {
         this.applicationEventPublisher.publishEvent(event);
      }
      // If it's an ACK we are finished processing at this point
      return;
   }

```

默认的ACK处理是在`TraceListener`中完成,处理方式是日志已debug模式打印.

```Java

@EventListener
public void onAck(AckRemoteApplicationEvent event) {
   Map<String, Object> trace = getReceivedTrace(event);
   // FIXME boot 2 this.repository.add(trace);
}

protected Map<String, Object> getSentTrace(SentApplicationEvent event) {
		Map<String, Object> map = new LinkedHashMap<String, Object>();
		map.put("signal", "spring.cloud.bus.sent");
		map.put("type", event.getType().getSimpleName());
		map.put("id", event.getId());
		map.put("origin", event.getOriginService());
		map.put("destination", event.getDestinationService());
		if (log.isDebugEnabled()) {
			log.debug(map);
		}
		return map;
}

```

如果是非ACK类型的事件,目标对象包括自身时,如果是其他服务发送的则发布该事件.如果ACK模式开启则,投递ack消息.

```java
if (this.serviceMatcher.isForSelf(event)
      && this.applicationEventPublisher != null) {
   if (!this.serviceMatcher.isFromSelf(event)) {
      this.applicationEventPublisher.publishEvent(event);
   }
   if (this.bus.getAck().isEnabled()) {
      AckRemoteApplicationEvent ack = new AckRemoteApplicationEvent(this,
            this.serviceMatcher.getServiceId(),
            this.bus.getAck().getDestinationService(),
            event.getDestinationService(), event.getId(), event.getClass());
      this.cloudBusOutboundChannel
            .send(MessageBuilder.withPayload(ack).build());
      this.applicationEventPublisher.publishEvent(ack);
   }
}
```

如果追踪模式被开启,则发布`SentApplicationEvent`,该事件的默认处理和`AckRemoteApplicationEvent`事件相同.

```java
if (this.bus.getTrace().isEnabled() && this.applicationEventPublisher != null) {
   // We are set to register sent events so publish it for local consumption,
   // irrespective of the origin
   this.applicationEventPublisher.publishEvent(new SentApplicationEvent(this,
         event.getOriginService(), event.getDestinationService(),
         event.getId(), event.getClass()));
}
```





引用:

[官方文档](https://docs.spring.io/spring-cloud-bus/docs/current/reference/html/#configuration-properties)

[细聊Spring Cloud Bus](https://www.cnblogs.com/bluersw/p/11643166.html)