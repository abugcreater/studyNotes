Spring ApplicationEvent

> 基于Java 的 EventObject, spring 事件,使用观察者模式进行事件调用.

容器启动时向spring注册时间监听`AbstractApplicationContext#refresh()`方法中执行.

```java
@Override
public void refresh() throws BeansException, IllegalStateException {
   synchronized (this.startupShutdownMonitor) {

      try {
         // Initialize event multicaster for this context.
         //为此上下文初始化事件多播器
         initApplicationEventMulticaster();
     
         // Check for listener beans and register them.
         //检查侦听器 bean 并注册它们
         registerListeners();
      }
   }
}
省略部分代码.
```

初始化`AbstractApplicationContext`中的`ApplicationEventMulticaster applicationEventMulticaster`属性.


```java
/**
	 * Initialize the ApplicationEventMulticaster.
	 * Uses SimpleApplicationEventMulticaster if none defined in the context.
	 * @see org.springframework.context.event.SimpleApplicationEventMulticaster
	 */
	protected void initApplicationEventMulticaster() {
		ConfigurableListableBeanFactory beanFactory = getBeanFactory();
		if (beanFactory.containsLocalBean(APPLICATION_EVENT_MULTICASTER_BEAN_NAME)) {
			this.applicationEventMulticaster =
					beanFactory.getBean(APPLICATION_EVENT_MULTICASTER_BEAN_NAME, ApplicationEventMulticaster.class);
			if (logger.isDebugEnabled()) {
				logger.debug("Using ApplicationEventMulticaster [" + this.applicationEventMulticaster + "]");
			}
		}
		else {
			this.applicationEventMulticaster = new SimpleApplicationEventMulticaster(beanFactory);
			beanFactory.registerSingleton(APPLICATION_EVENT_MULTICASTER_BEAN_NAME, this.applicationEventMulticaster);
			if (logger.isDebugEnabled()) {
				logger.debug("Unable to locate ApplicationEventMulticaster with name '" +
						APPLICATION_EVENT_MULTICASTER_BEAN_NAME +
						"': using default [" + this.applicationEventMulticaster + "]");
			}
		}
	}
```

判断`BeanFactory`是否已经初始化了`APPLICATION_EVENT_MULTICASTER_BEAN_NAME`名称的`Bean`,如果已经初始化了则直接获取并赋值.如果没有,则初始化并注册这个`Bean`.

注册监听器.

```java 
protected void registerListeners() {
   // Register statically specified listeners first.
   for (ApplicationListener<?> listener : getApplicationListeners()) {
      getApplicationEventMulticaster().addApplicationListener(listener);
   }

   // Do not initialize FactoryBeans here: We need to leave all regular beans
   // uninitialized to let post-processors apply to them!
   
   ▲ String[] listenerBeanNames = getBeanNamesForType(ApplicationListener.class, true, false);
   for (String listenerBeanName : listenerBeanNames) {
      getApplicationEventMulticaster().addApplicationListenerBean(listenerBeanName);
   }

   // Publish early application events now that we finally have a multicaster...
   Set<ApplicationEvent> earlyEventsToProcess = this.earlyApplicationEvents;
   this.earlyApplicationEvents = null;
   if (earlyEventsToProcess != null) {
      for (ApplicationEvent earlyEvent : earlyEventsToProcess) {
         getApplicationEventMulticaster().multicastEvent(earlyEvent);
      }
   }
}
```

一般自定义的listener在▲这一步处理,`getBeanNamesForType`获取定义bean中的所有  `ApplicationListener`类型的bean,循环将listener名称添加到 `defaultRetriever.applicationListenerBeans`中.

默认listener为

>result = {LinkedHashSet@4453}  size = 15
> 0 = {RSocketPortInfoApplicationContextInitializer$Listener@5784} 
> 1 = {ServerPortInfoApplicationContextInitializer@5789} 
> 2 = {ConditionEvaluationReportLoggingListener$ConditionEvaluationReportListener@5790} 
> 3 = {CloudFoundryVcapEnvironmentPostProcessor@5791} 
> 4 = {ConfigFileApplicationListener@5792} 
> 5 = {AnsiOutputApplicationListener@5793} 
> 6 = {LoggingApplicationListener@5794} 
> 7 = {ClasspathLoggingApplicationListener@5795} 
> 8 = {BackgroundPreinitializer@5796} 
> 9 = {DelegatingApplicationListener@5797} 
> 10 = {ParentContextCloserApplicationListener@5798} 
> 11 = {ClearCachesApplicationListener@5799} 
> 12 = {FileEncodingApplicationListener@5800} 
> 13 = {LiquibaseServiceLocatorApplicationListener@5801} 
> 14 = {SharedMetadataReaderFactoryContextInitializer$SharedMetadataReaderFactoryBean@5802} 



根据类型获取beanNames得到结果

>0 = "&org.springframework.boot.autoconfigure.internalCachingMetadataReaderFactory"
>1 = "firstListener"
>2 = "mvcResourceUrlProvider"
>3 = "springApplicationAdminRegistrar"
>4 = "applicationAvailability"





 `AbstractApplicationEventMulticaster#getApplicationListeners(event, type)`获取`ApplicationListener`集合时会有限从缓存`retrieverCache`中获取,当缓存中没有时会更新.

```java
protected Collection<ApplicationListener<?>> getApplicationListeners(
      ApplicationEvent event, ResolvableType eventType) {

   Object source = event.getSource();
   Class<?> sourceType = (source != null ? source.getClass() : null);
   ListenerCacheKey cacheKey = new ListenerCacheKey(eventType, sourceType);

   // Potential new retriever to populate
   CachedListenerRetriever newRetriever = null;

   // Quick check for existing entry on ConcurrentHashMap
   CachedListenerRetriever existingRetriever = this.retrieverCache.get(cacheKey);
   if (existingRetriever == null) {
      // Caching a new ListenerRetriever if possible
      if (this.beanClassLoader == null ||
            (ClassUtils.isCacheSafe(event.getClass(), this.beanClassLoader) &&
                  (sourceType == null || ClassUtils.isCacheSafe(sourceType, this.beanClassLoader)))) {
         newRetriever = new CachedListenerRetriever();
         existingRetriever = this.retrieverCache.putIfAbsent(cacheKey, newRetriever);
         if (existingRetriever != null) {
            newRetriever = null;  // no need to populate it in retrieveApplicationListeners
         }
      }
   }

   if (existingRetriever != null) {
      Collection<ApplicationListener<?>> result = existingRetriever.getApplicationListeners();
      if (result != null) {
         return result;
      }
      // If result is null, the existing retriever is not fully populated yet by another thread.
      // Proceed like caching wasn't possible for this current local attempt.
   }

   return retrieveApplicationListeners(eventType, sourceType, newRetriever);
}
```

如果缓存中有则直接返回,如果没有则会通过使用`retrieveApplicationListeners`方法更新缓存.他是通过遍历所有的监听器,找出匹配的名称,然后更新`retriever`,达到更新缓存的目的.

```java
private Collection<ApplicationListener<?>> retrieveApplicationListeners(
      ResolvableType eventType, @Nullable Class<?> sourceType, @Nullable CachedListenerRetriever retriever) {

   List<ApplicationListener<?>> allListeners = new ArrayList<>();
   Set<ApplicationListener<?>> filteredListeners = (retriever != null ? new LinkedHashSet<>() : null);
   Set<String> filteredListenerBeans = (retriever != null ? new LinkedHashSet<>() : null);

   Set<ApplicationListener<?>> listeners;
   Set<String> listenerBeans;
   synchronized (this.defaultRetriever) {
      listeners = new LinkedHashSet<>(this.defaultRetriever.applicationListeners);
      listenerBeans = new LinkedHashSet<>(this.defaultRetriever.applicationListenerBeans);
   }

   // Add programmatically registered listeners, including ones coming
   // from ApplicationListenerDetector (singleton beans and inner beans).
   for (ApplicationListener<?> listener : listeners) {
      if (supportsEvent(listener, eventType, sourceType)) {
         if (retriever != null) {
            filteredListeners.add(listener);
         }
         allListeners.add(listener);
      }
   }

   // Add listeners by bean name, potentially overlapping with programmatically
   // registered listeners above - but here potentially with additional metadata.
   if (!listenerBeans.isEmpty()) {
      ConfigurableBeanFactory beanFactory = getBeanFactory();
      for (String listenerBeanName : listenerBeans) {
         try {
            if (supportsEvent(beanFactory, listenerBeanName, eventType)) {
               ApplicationListener<?> listener =
                     beanFactory.getBean(listenerBeanName, ApplicationListener.class);
               if (!allListeners.contains(listener) && supportsEvent(listener, eventType, sourceType)) {
                  if (retriever != null) {
                     if (beanFactory.isSingleton(listenerBeanName)) {
                        filteredListeners.add(listener);
                     }
                     else {
                        filteredListenerBeans.add(listenerBeanName);
                     }
                  }
                  allListeners.add(listener);
               }
            }
            else {
               // Remove non-matching listeners that originally came from
               // ApplicationListenerDetector, possibly ruled out by additional
               // BeanDefinition metadata (e.g. factory method generics) above.
               Object listener = beanFactory.getSingleton(listenerBeanName);
               if (retriever != null) {
                  filteredListeners.remove(listener);
               }
               allListeners.remove(listener);
            }
         }
         catch (NoSuchBeanDefinitionException ex) {
            // Singleton listener instance (without backing bean definition) disappeared -
            // probably in the middle of the destruction phase
         }
      }
   }

   AnnotationAwareOrderComparator.sort(allListeners);
   if (retriever != null) {
      if (filteredListenerBeans.isEmpty()) {
         retriever.applicationListeners = new LinkedHashSet<>(allListeners);
         retriever.applicationListenerBeans = filteredListenerBeans;
      }
      else {
         retriever.applicationListeners = filteredListeners;
         retriever.applicationListenerBeans = filteredListenerBeans;
      }
   }
   return allListeners;
}
```

spring中的事件执行.

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

异步线程配置可以通过

```xml
<task:executor id="multicasterExecutor" pool-size="3"/>
<bean class="org.springframework.context.event.SimpleApplicationEventMulticaster">
    <property name="taskExecutor" ref="multicasterExecutor"></property>
</bean>
```

参考 [Spring定时任务的几种实现](http://gong1208.iteye.com/blog/1773177)

