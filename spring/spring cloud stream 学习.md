spring cloud stream 学习 

简称scs

`spring cloud stream`基于`spring message`(todo 待学习)及`spring Integration`(todo 待学习)

> Spring Cloud Stream is a framework for building message-driven microservice applications. Spring Cloud Stream builds upon Spring Boot to create standalone, production-grade Spring applications and uses Spring Integration to provide connectivity to message brokers. It provides opinionated configuration of middleware from several vendors, introducing the concepts of persistent publish-subscribe semantics, consumer groups, and partitions.

scs是用于构建高度可扩展的基于事件驱动的微服务,目的是简化消息在spring cloud应用程序中的开发.

scs发送消息:基于`MessageChannel`进行消息发送

```java
@FunctionalInterface
public interface MessageChannel {

	/**
	 * Constant for sending a message without a prescribed timeout.
	 */
	long INDEFINITE_TIMEOUT = -1;

	default boolean send(Message<?> message) {
		return send(message, INDEFINITE_TIMEOUT);
	}

	boolean send(Message<?> message, long timeout);

}

```

进过调试获取具体的实现类是`AbstractSubscribableChannel`

调用链路 `AbstractSubscribableChannel#doSend`->`UnicastingDispatcher#dispatch`->

`UnicastingDispatcher#doDispatch`->`UnicastingDispatcher#tryOptimizedDispatch`->

`AbstractMessageHandler#handleMessage`

然后它会委托给 `AbstractMessageChannelBinder#createProducerMessageHandler` 创建的 MessageHandler 处理(该方法由不同的消息中间件实现).



关于注解 `@StreamListener`的使用,在配置类`BindingServiceConfiguration`中会初始化`StreamListenerAnnotationBeanPostProcessor`这个类.`StreamListenerAnnotationBeanPostProcessor`继承了`SmartInitializingSingleton`类,重写了方法`afterSingletonsInstantiated()`,该方法会在单例预实例化结束时调用,保证所有常规单例bean都已经被创建.该方法会将被注解的方法初始化为`handler`,在`handleMessageInternal`方法中调用.

```java
public final void afterSingletonsInstantiated() {
	//运行注册在streamListenerCallbacks中的线程,初始化map中的值	
   this.injectAndPostProcessDependencies();
   EvaluationContext evaluationContext = IntegrationContextUtils.getEvaluationContext(this.applicationContext.getBeanFactory());
   for (Map.Entry<String, List<StreamListenerHandlerMethodMapping>> mappedBindingEntry : mappedListenerMethods
         .entrySet()) {
      ArrayList<DispatchingStreamListenerMessageHandler.ConditionalStreamListenerMessageHandlerWrapper> handlers = new ArrayList<>();
      for (StreamListenerHandlerMethodMapping mapping : mappedBindingEntry.getValue()) {
         final InvocableHandlerMethod invocableHandlerMethod = this.messageHandlerMethodFactory
               .createInvocableHandlerMethod(mapping.getTargetBean(),
                     checkProxy(mapping.getMethod(), mapping.getTargetBean()));
         StreamListenerMessageHandler streamListenerMessageHandler = new StreamListenerMessageHandler(
               invocableHandlerMethod, resolveExpressionAsBoolean(mapping.getCopyHeaders(), "copyHeaders"),
               springIntegrationProperties.getMessageHandlerNotPropagatedHeaders());
         streamListenerMessageHandler.setApplicationContext(this.applicationContext);
         streamListenerMessageHandler.setBeanFactory(this.applicationContext.getBeanFactory());
         if (StringUtils.hasText(mapping.getDefaultOutputChannel())) {
            streamListenerMessageHandler.setOutputChannelName(mapping.getDefaultOutputChannel());
         }
         streamListenerMessageHandler.afterPropertiesSet();
         if (StringUtils.hasText(mapping.getCondition())) {
            String conditionAsString = resolveExpressionAsString(mapping.getCondition(), "condition");
            Expression condition = SPEL_EXPRESSION_PARSER.parseExpression(conditionAsString);
            handlers.add(
                  new DispatchingStreamListenerMessageHandler.ConditionalStreamListenerMessageHandlerWrapper(
                        condition, streamListenerMessageHandler));
         }
         else {
            handlers.add(
                  new DispatchingStreamListenerMessageHandler.ConditionalStreamListenerMessageHandlerWrapper(
                        null, streamListenerMessageHandler));
         }
      }
      if (handlers.size() > 1) {
         for (DispatchingStreamListenerMessageHandler.ConditionalStreamListenerMessageHandlerWrapper handler : handlers) {
            Assert.isTrue(handler.isVoid(), StreamListenerErrorMessages.MULTIPLE_VALUE_RETURNING_METHODS);
         }
      }
      AbstractReplyProducingMessageHandler handler;

      if (handlers.size() > 1 || handlers.get(0).getCondition() != null) {
         handler = new DispatchingStreamListenerMessageHandler(handlers, evaluationContext);
      }
      else {
         handler = handlers.get(0).getStreamListenerMessageHandler();
      }
      handler.setApplicationContext(this.applicationContext);
      handler.setChannelResolver(this.binderAwareChannelResolver);
      handler.afterPropertiesSet();
//经过以上一系列的处理,将初始化的messageHandler注册成bean对象
this.applicationContext.getBeanFactory().registerSingleton(handler.getClass().getSimpleName() + handler.hashCode(), handler);
      //SubscribableChannel订阅该messageHandler
      applicationContext.getBean(mappedBindingEntry.getKey(), SubscribableChannel.class).subscribe(handler);
   }
   this.mappedListenerMethods.clear();
}
```

而`StreamListenerAnnotationBeanPostProcessor#postProcessAfterInitialization`方法会将所有被注解方法,解析后 在`orchestrateStreamListenerSetupMethod`中调用`registerHandlerMethodOnListenedChannel`注册将这些方法注册到`mappedListenerMethods`中.



进过上述步骤的处理,当订阅的channel有新的消息需要处理时,`getDispatcher()`会获取到`MessageDispatcher`,而`MessageDispatcher`中包含了所有的`MessageHandler`最后当调用,`MessageDispatcher#doDispatch`处理消息时会遍历所有`MessageHandler`对消息进行处理.

```
private boolean doDispatch(Message<?> message) {
   if (tryOptimizedDispatch(message)) {
      return true;
   }
   boolean success = false;
   Iterator<MessageHandler> handlerIterator = this.getHandlerIterator(message);
   if (!handlerIterator.hasNext()) {
      throw new MessageDispatchingException(message, "Dispatcher has no subscribers");
   }
   List<RuntimeException> exceptions = new ArrayList<RuntimeException>();
   while (!success && handlerIterator.hasNext()) {
      MessageHandler handler = handlerIterator.next();
      try {
         handler.handleMessage(message);
         success = true; // we have a winner.
      }
      catch (Exception e) {
         @SuppressWarnings("deprecation")
         RuntimeException runtimeException = wrapExceptionIfNecessary(message, e);
         exceptions.add(runtimeException);
         this.handleExceptions(exceptions, message, !handlerIterator.hasNext());
      }
   }
   return success;
}
```

而注解`@EnableBinding`充当`@Configuration`的作用

```java
@Target({ ElementType.TYPE, ElementType.ANNOTATION_TYPE })
@Retention(RetentionPolicy.RUNTIME)
@Documented
@Inherited
@Configuration
@Import({ BindingServiceConfiguration.class, BindingBeansRegistrar.class, BinderFactoryConfiguration.class,
      SpelExpressionConverterConfiguration.class })
@EnableIntegration
public @interface EnableBinding {

   /**
    * A list of interfaces having methods annotated with {@link Input} and/or
    * {@link Output} to indicate binding targets.
    */
   Class<?>[] value() default {};

}
```







[官方文档](https://docs.spring.io/spring-cloud-stream/docs/3.1.4/reference/html/spring-cloud-stream.html#spring-cloud-stream-overview-introducing)

[干货｜Spring Cloud Stream 体系及原理介绍](https://mp.weixin.qq.com/s/e_pDTFmFcSqHH-uSIzNmMg)











