# Spring AbstractApplicationContext refresh方法解析

## 1. 容器启动方法主体

```java
public void refresh() throws BeansException, IllegalStateException {
   synchronized (this.startupShutdownMonitor) {
      // Prepare this context for refreshing.
       //准备上下文
      prepareRefresh();

      // Tell the subclass to refresh the internal bean factory.
       //告诉子类刷新内部bean工厂返回的类型为DefaultListableBeanFactory,在GenericApplicationContext中设置serializationId后返回beanFactory
      ConfigurableListableBeanFactory beanFactory = obtainFreshBeanFactory();

      // Prepare the bean factory for use in this context.
       //准备上下文使用的bean factory
      prepareBeanFactory(beanFactory);

      try {
         // Allows post-processing of the bean factory in context subclasses.
          //允许在上下文子类中对 bean 工厂进行后处理.AnnotationConfigServletWebServerApplicationContext#postProcessBeanFactory,然后执行super.postProcessBeanFactory逻辑registerWebApplicationScopes
         postProcessBeanFactory(beanFactory);

         // Invoke factory processors registered as beans in the context.
          //将工厂处理器在上下文中注册未bean;调用BeanFactory后处理器 实际调用 PostProcessorRegistrationDelegate.invokeBeanFactoryPostProcessors;这一步触发了bean的实例化
         invokeBeanFactoryPostProcessors(beanFactory);

         // Register bean processors that intercept bean creation.
         //用于处理BeanPostProcessor,主要用于对Bean的扩展主要用在 bean 实例化之后，执行初始化方法前后，允许开发者对 bean 实例进行修改 主要调用 PostProcessorRegistrationDelegate#registerBeanPostProcessors方法
         registerBeanPostProcessors(beanFactory);

         // Initialize message source for this context.
          //初始化消息源,如果没有则初始化DelegatingMessageSource(dms),设置父信息,并将messageSource设置为dms,然后注册该bean  调用AbstractApplicationContext #initMessageSource方法
         initMessageSource();

         // Initialize event multicaster for this context.
         //为上下文初始化 event multicaster,在Spring ApplicationEvent已解析
         initApplicationEventMulticaster();

         // Initialize other special beans in specific context subclasses.
          //初始化特定上下文子类中的其他特殊 bean。 执行 ServletWebServerApplicationContext#onRefresh方法
         onRefresh();

         // Check for listener beans and register them.
         //注册Listener类在Spring ApplicationEvent已解释 
         registerListeners();

         // Instantiate all remaining (non-lazy-init) singletons.
         //该方法会实例化所有剩余的非懒加载单例 bean。除了一些内部的 bean、实现了 BeanFactoryPostProcessor 接口的 bean、实现了 BeanPostProcessor 接口的 bean，其他的非懒加载单例 bean 都会在这个方法中被实例化，并且 BeanPostProcessor 的触发也是在这个方法中。
         finishBeanFactoryInitialization(beanFactory);

         // Last step: publish corresponding event.
         //完成此背景下的刷新，调用LifecycleProcessor的onRefresh（）方法和发布ContextRefreshedEvent 。 
         finishRefresh();
      }

      catch (BeansException ex) {
         if (logger.isWarnEnabled()) {
            logger.warn("Exception encountered during context initialization - " +
                  "cancelling refresh attempt: " + ex);
         }

         // Destroy already created singletons to avoid dangling resources.
         destroyBeans();

         // Reset 'active' flag.
         cancelRefresh(ex);

         // Propagate exception to caller.
         throw ex;
      }

      finally {
         // Reset common introspection caches in Spring's core, since we
         // might not ever need metadata for singleton beans anymore...
         resetCommonCaches();
      }
   }
}
```

## 2. 准备刷新工作

### 2.1 实际实现子类

AnnotationConfigReactiveWebServerApplicationContext

```java
AnnotationConfigReactiveWebServerApplicationContext#prepareRefresh
prepareRefresh方法实际实现
protected void prepareRefresh() {
    //调用 CachingMetadataReaderFactory#clearCache
   this.scanner.clearCache();
   super.prepareRefresh();
}
```

### 2.2 清除缓存

```java
public void setCacheLimit(int cacheLimit) {
   if (cacheLimit <= 0) {
      this.metadataReaderCache = null;
   }
   //metadataReaderCache类型为ConcurrentHashMap
   else if (this.metadataReaderCache instanceof LocalResourceCache) {
      ((LocalResourceCache) this.metadataReaderCache).setCacheLimit(cacheLimit);
   }
   else {
   		// LocalResourceCache extends LinkedHashMap<Resource, MetadataReader>,初始化一个LinkedHashMap
      this.metadataReaderCache = new LocalResourceCache(cacheLimit);
   }
}
```

### 2.3 实际刷新流程

准备此上下文以进行刷新、设置其启动日期和活动标志以及执行任何属性源的初始化

```java
protected void prepareRefresh() {
   // Switch to active. 设置上下文活动状态
   this.startupDate = System.currentTimeMillis();
   this.closed.set(false);
   this.active.set(true);


   // Initialize any placeholder property sources in the context environment.
    //初始化上下文占位属性,实际走 GenericWebApplicationContext#initPropertySources
   initPropertySources();

   // Validate that all properties marked as required are resolvable:
   // see ConfigurablePropertyResolver#setRequiredProperties
    // 校验必要的属性,执行AbstractPropertyResolver#validateRequiredProperties方法
   getEnvironment().validateRequiredProperties();

   // Store pre-refresh ApplicationListeners...
    //存储监听器
   if (this.earlyApplicationListeners == null) {
      this.earlyApplicationListeners = new LinkedHashSet<>(this.applicationListeners);
   }
   else {
      // Reset local application listeners to pre-refresh state.
      this.applicationListeners.clear();
      this.applicationListeners.addAll(this.earlyApplicationListeners);
   }

   // Allow for the collection of early ApplicationEvents,
   // to be published once the multicaster is available...
   this.earlyApplicationEvents = new LinkedHashSet<>();
}
```

#### 2.3.1 初始化上下占位符属性

用给定的servletContext和servletConfig对象填充的实际实例替换基于Servlet的stub property sources 。
该方法是幂等的，因为它可以被调用任意次数，但将执行一次且仅一次将存根属性源替换为其对应的实际属性源

```java
//initPropertySources实际执行的方法
public static void initServletPropertySources(MutablePropertySources sources,
      @Nullable ServletContext servletContext, @Nullable ServletConfig servletConfig) {

   Assert.notNull(sources, "'propertySources' must not be null");
   String name = StandardServletEnvironment.SERVLET_CONTEXT_PROPERTY_SOURCE_NAME;
   if (servletContext != null && sources.get(name) instanceof StubPropertySource) {
       //填充给定的servletContext属性
      sources.replace(name, new ServletContextPropertySource(name, servletContext));
   }
   name = StandardServletEnvironment.SERVLET_CONFIG_PROPERTY_SOURCE_NAME;
   if (servletConfig != null && sources.get(name) instanceof StubPropertySource) {
       //填充给定的servletConfig属性
      sources.replace(name, new ServletConfigPropertySource(name, servletConfig));
   }
}
```

## 3. 告诉子类刷新工厂

```java
protected ConfigurableListableBeanFactory obtainFreshBeanFactory() {
   refreshBeanFactory();
   return getBeanFactory();
}
```

### 3.1 刷新工厂

GenericApplicationContext#refreshBeanFactory

```java
protected final void refreshBeanFactory() throws IllegalStateException {
   //设置beanFactory 序列化id
   this.beanFactory.setSerializationId(getId());
}
```

### 3.2 返回工厂类
```java
public final ConfigurableListableBeanFactory getBeanFactory() {
   return this.beanFactory;
}
```

扫描bean实例:`AbstractApplicationContext#obtainFreshBeanFactory`->`AbstractRefreshableApplicationContext#refreshBeanFactory`->`AnnotationConfigWebApplicationContext#loadBeanDefinitions`->`ClassPathBeanDefinitionScanner#scan`->`ClassPathBeanDefinitionScanner#doScan`->`ClassPathScanningCandidateComponentProvider#findCandidateComponents`->`ClassPathScanningCandidateComponentProvider#scanCandidateComponents`-> 1:getResourcePatternResolver().getResources(packageSearchPath) 加载基础包下所有类; 2 :getMetadataReaderFactory().getMetadataReader(resource) 获取类属性



## 4. 配置BeanFactory

```java
//准备BeanFactory,设置classLoader及后处理器等
protected void prepareBeanFactory(ConfigurableListableBeanFactory beanFactory) {
   // Tell the internal bean factory to use the context's class loader etc.
   beanFactory.setBeanClassLoader(getClassLoader());
   beanFactory.setBeanExpressionResolver(new StandardBeanExpressionResolver(beanFactory.getBeanClassLoader()));
   beanFactory.addPropertyEditorRegistrar(new ResourceEditorRegistrar(this, getEnvironment()));

   // Configure the bean factory with context callbacks.
    //配置 bean 工厂使用上下文回调
    //替换ApplicationContextAwareProcessor
   beanFactory.addBeanPostProcessor(new ApplicationContextAwareProcessor(this));
   beanFactory.ignoreDependencyInterface(EnvironmentAware.class);
   beanFactory.ignoreDependencyInterface(EmbeddedValueResolverAware.class);
   beanFactory.ignoreDependencyInterface(ResourceLoaderAware.class);
   beanFactory.ignoreDependencyInterface(ApplicationEventPublisherAware.class);
   beanFactory.ignoreDependencyInterface(MessageSourceAware.class);
   beanFactory.ignoreDependencyInterface(ApplicationContextAware.class);

   // BeanFactory interface not registered as resolvable type in a plain factory.
   // MessageSource registered (and found for autowiring) as a bean.
    //使用相应的自动装配值注册一个特殊的依赖类型。
   beanFactory.registerResolvableDependency(BeanFactory.class, beanFactory);
   beanFactory.registerResolvableDependency(ResourceLoader.class, this);
   beanFactory.registerResolvableDependency(ApplicationEventPublisher.class, this);
   beanFactory.registerResolvableDependency(ApplicationContext.class, this);

   // Register early post-processor for detecting inner beans as ApplicationListeners.
    //检测实现ApplicationListener接口的BeanPostProcessor 注册为post-processor
   beanFactory.addBeanPostProcessor(new ApplicationListenerDetector(this));

   // Detect a LoadTimeWeaver and prepare for weaving, if found.
   if (beanFactory.containsBean(LOAD_TIME_WEAVER_BEAN_NAME)) {
      beanFactory.addBeanPostProcessor(new LoadTimeWeaverAwareProcessor(beanFactory));
      // Set a temporary ClassLoader for type matching.
      beanFactory.setTempClassLoader(new ContextTypeMatchClassLoader(beanFactory.getBeanClassLoader()));
   }

   // Register default environment beans.
    //注册默认环境bean
   if (!beanFactory.containsLocalBean(ENVIRONMENT_BEAN_NAME)) {
      beanFactory.registerSingleton(ENVIRONMENT_BEAN_NAME, getEnvironment());
   }
   if (!beanFactory.containsLocalBean(SYSTEM_PROPERTIES_BEAN_NAME)) {
      beanFactory.registerSingleton(SYSTEM_PROPERTIES_BEAN_NAME, getEnvironment().getSystemProperties());
   }
   if (!beanFactory.containsLocalBean(SYSTEM_ENVIRONMENT_BEAN_NAME)) {
      beanFactory.registerSingleton(SYSTEM_ENVIRONMENT_BEAN_NAME, getEnvironment().getSystemEnvironment());
   }
}
```

## 5. 前置BeanFactory

AnnotationConfigServletWebServerApplicationContext#postProcessBeanFactory

```java
protected void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) {
   //调用父类ServletWebServerApplicationContext 方法
   super.postProcessBeanFactory(beanFactory);
   //默认情况下以下两个if都不会走   
   if (this.basePackages != null && this.basePackages.length > 0) {
      this.scanner.scan(this.basePackages);
   }
   if (!this.annotatedClasses.isEmpty()) {
      this.reader.register(ClassUtils.toClassArray(this.annotatedClasses));
   }
}
```

### 5.1 web环境处理beanFactory

ServletWebServerApplicationContext # postProcessBeanFactory

```java
protected void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) {
    //将WebApplicationContextServletContextAwareProcessor添加到beanPostProcessors中
		beanFactory.addBeanPostProcessor(new WebApplicationContextServletContextAwareProcessor(this));
    //忽略ServletContextAware接口对象
		beanFactory.ignoreDependencyInterface(ServletContextAware.class);
		registerWebApplicationScopes();
	}
```

#### 5.1.1 注册web应用范围

```java
//postProcessBeanFactory方法中注册web应用范围
//所在类 ServletWebServerApplicationContext
private void registerWebApplicationScopes() {
    //跟新scopes缓存中的scope内容
   ExistingWebApplicationScopes existingScopes = new ExistingWebApplicationScopes(getBeanFactory());
   WebApplicationContextUtils.registerWebApplicationScopes(getBeanFactory());
    //因为Scopes中没有值所以该方法没有实际运行
   existingScopes.restore();
}
```

WebApplicationContextUtils#registerWebApplicationScopes

```java
//registerWebApplicationScopes 方法中实际调用,传参为beanFactory,null;所在类WebApplicationContextUtils
public static void registerWebApplicationScopes(ConfigurableListableBeanFactory beanFactory,
      @Nullable ServletContext sc) {
	//注册beanFactory的范围 request/session,实际将scopename和scope映射放到缓存中
   beanFactory.registerScope(WebApplicationContext.SCOPE_REQUEST, new RequestScope());
   beanFactory.registerScope(WebApplicationContext.SCOPE_SESSION, new SessionScope());
    //调试中不走入
   if (sc != null) {
      ServletContextScope appScope = new ServletContextScope(sc);
      beanFactory.registerScope(WebApplicationContext.SCOPE_APPLICATION, appScope);
      // Register as ServletContext attribute, for ContextCleanupListener to detect it.
      sc.setAttribute(ServletContextScope.class.getName(), appScope);
   }
	//使用相应的自动装配值注册一个特殊的依赖类型
   beanFactory.registerResolvableDependency(ServletRequest.class, new RequestObjectFactory());
   beanFactory.registerResolvableDependency(ServletResponse.class, new ResponseObjectFactory());
   beanFactory.registerResolvableDependency(HttpSession.class, new SessionObjectFactory());
   beanFactory.registerResolvableDependency(WebRequest.class, new WebRequestObjectFactory());
    //  //调试中不走入
   if (jsfPresent) {
      FacesDependencyRegistrar.registerFacesDependencies(beanFactory);
   }
}
```

## 6. 调用BeanFactory前置处理器



```java
protected void invokeBeanFactoryPostProcessors(ConfigurableListableBeanFactory beanFactory) {
   PostProcessorRegistrationDelegate.invokeBeanFactoryPostProcessors(beanFactory, getBeanFactoryPostProcessors());

   // Detect a LoadTimeWeaver and prepare for weaving, if found in the meantime
   // (e.g. through an @Bean method registered by ConfigurationClassPostProcessor)
   if (beanFactory.getTempClassLoader() == null && beanFactory.containsBean(LOAD_TIME_WEAVER_BEAN_NAME)) {
      beanFactory.addBeanPostProcessor(new LoadTimeWeaverAwareProcessor(beanFactory));
      beanFactory.setTempClassLoader(new ContextTypeMatchClassLoader(beanFactory.getBeanClassLoader()));
   }
}
```

### 6.1 按照类型/顺序依次调用前置处理器

根据不同的BeanPostProcessor分类后进行注册 优先PriorityOrdered类型,然后Ordered类型,之后是其他普通的最后是内部类型

```java
public static void invokeBeanFactoryPostProcessors(
      ConfigurableListableBeanFactory beanFactory, List<BeanFactoryPostProcessor> beanFactoryPostProcessors) {

   // Invoke BeanDefinitionRegistryPostProcessors first, if any.
   Set<String> processedBeans = new HashSet<>();

   if (beanFactory instanceof BeanDefinitionRegistry) {
      BeanDefinitionRegistry registry = (BeanDefinitionRegistry) beanFactory;
      List<BeanFactoryPostProcessor> regularPostProcessors = new ArrayList<>();
      List<BeanDefinitionRegistryPostProcessor> registryProcessors = new ArrayList<>();

      for (BeanFactoryPostProcessor postProcessor : beanFactoryPostProcessors) {
         if (postProcessor instanceof BeanDefinitionRegistryPostProcessor) {
            BeanDefinitionRegistryPostProcessor registryProcessor =
                  (BeanDefinitionRegistryPostProcessor) postProcessor;
            registryProcessor.postProcessBeanDefinitionRegistry(registry);
            registryProcessors.add(registryProcessor);
         }
         else {
            regularPostProcessors.add(postProcessor);
         }
      }

      // Do not initialize FactoryBeans here: We need to leave all regular beans
      // uninitialized to let the bean factory post-processors apply to them!
      // Separate between BeanDefinitionRegistryPostProcessors that implement
      // PriorityOrdered, Ordered, and the rest.
      List<BeanDefinitionRegistryPostProcessor> currentRegistryProcessors = new ArrayList<>();

      // First, invoke the BeanDefinitionRegistryPostProcessors that implement PriorityOrdered.
      String[] postProcessorNames =
            beanFactory.getBeanNamesForType(BeanDefinitionRegistryPostProcessor.class, true, false);
      for (String ppName : postProcessorNames) {
         if (beanFactory.isTypeMatch(ppName, PriorityOrdered.class)) {
            currentRegistryProcessors.add(beanFactory.getBean(ppName, BeanDefinitionRegistryPostProcessor.class));
            processedBeans.add(ppName);
         }
      }
      sortPostProcessors(currentRegistryProcessors, beanFactory);
      registryProcessors.addAll(currentRegistryProcessors);
      invokeBeanDefinitionRegistryPostProcessors(currentRegistryProcessors, registry);
      currentRegistryProcessors.clear();

      // Next, invoke the BeanDefinitionRegistryPostProcessors that implement Ordered.
      postProcessorNames = beanFactory.getBeanNamesForType(BeanDefinitionRegistryPostProcessor.class, true, false);
      for (String ppName : postProcessorNames) {
         if (!processedBeans.contains(ppName) && beanFactory.isTypeMatch(ppName, Ordered.class)) {
            currentRegistryProcessors.add(beanFactory.getBean(ppName, BeanDefinitionRegistryPostProcessor.class));
            processedBeans.add(ppName);
         }
      }
      sortPostProcessors(currentRegistryProcessors, beanFactory);
      registryProcessors.addAll(currentRegistryProcessors);
      invokeBeanDefinitionRegistryPostProcessors(currentRegistryProcessors, registry);
      currentRegistryProcessors.clear();

      // Finally, invoke all other BeanDefinitionRegistryPostProcessors until no further ones appear.
      boolean reiterate = true;
      while (reiterate) {
         reiterate = false;
         postProcessorNames = beanFactory.getBeanNamesForType(BeanDefinitionRegistryPostProcessor.class, true, false);
         for (String ppName : postProcessorNames) {
            if (!processedBeans.contains(ppName)) {
               currentRegistryProcessors.add(beanFactory.getBean(ppName, BeanDefinitionRegistryPostProcessor.class));
               processedBeans.add(ppName);
               reiterate = true;
            }
         }
         sortPostProcessors(currentRegistryProcessors, beanFactory);
         registryProcessors.addAll(currentRegistryProcessors);
         invokeBeanDefinitionRegistryPostProcessors(currentRegistryProcessors, registry);
         currentRegistryProcessors.clear();
      }

      // Now, invoke the postProcessBeanFactory callback of all processors handled so far.
      invokeBeanFactoryPostProcessors(registryProcessors, beanFactory);
      invokeBeanFactoryPostProcessors(regularPostProcessors, beanFactory);
   }

   else {
      // Invoke factory processors registered with the context instance.
      invokeBeanFactoryPostProcessors(beanFactoryPostProcessors, beanFactory);
   }

   // Do not initialize FactoryBeans here: We need to leave all regular beans
   // uninitialized to let the bean factory post-processors apply to them!
   String[] postProcessorNames =
         beanFactory.getBeanNamesForType(BeanFactoryPostProcessor.class, true, false);

   // Separate between BeanFactoryPostProcessors that implement PriorityOrdered,
   // Ordered, and the rest.
   List<BeanFactoryPostProcessor> priorityOrderedPostProcessors = new ArrayList<>();
   List<String> orderedPostProcessorNames = new ArrayList<>();
   List<String> nonOrderedPostProcessorNames = new ArrayList<>();
   for (String ppName : postProcessorNames) {
      if (processedBeans.contains(ppName)) {
         // skip - already processed in first phase above
      }
      else if (beanFactory.isTypeMatch(ppName, PriorityOrdered.class)) {
         priorityOrderedPostProcessors.add(beanFactory.getBean(ppName, BeanFactoryPostProcessor.class));
      }
      else if (beanFactory.isTypeMatch(ppName, Ordered.class)) {
         orderedPostProcessorNames.add(ppName);
      }
      else {
         nonOrderedPostProcessorNames.add(ppName);
      }
   }

   // First, invoke the BeanFactoryPostProcessors that implement PriorityOrdered.
   sortPostProcessors(priorityOrderedPostProcessors, beanFactory);
   invokeBeanFactoryPostProcessors(priorityOrderedPostProcessors, beanFactory);

   // Next, invoke the BeanFactoryPostProcessors that implement Ordered.
   List<BeanFactoryPostProcessor> orderedPostProcessors = new ArrayList<>(orderedPostProcessorNames.size());
   for (String postProcessorName : orderedPostProcessorNames) {
      orderedPostProcessors.add(beanFactory.getBean(postProcessorName, BeanFactoryPostProcessor.class));
   }
   sortPostProcessors(orderedPostProcessors, beanFactory);
   invokeBeanFactoryPostProcessors(orderedPostProcessors, beanFactory);

   // Finally, invoke all other BeanFactoryPostProcessors.
   List<BeanFactoryPostProcessor> nonOrderedPostProcessors = new ArrayList<>(nonOrderedPostProcessorNames.size());
   for (String postProcessorName : nonOrderedPostProcessorNames) {
      nonOrderedPostProcessors.add(beanFactory.getBean(postProcessorName, BeanFactoryPostProcessor.class));
   }
   invokeBeanFactoryPostProcessors(nonOrderedPostProcessors, beanFactory);

   // Clear cached merged bean definitions since the post-processors might have
   // modified the original metadata, e.g. replacing placeholders in values...
   beanFactory.clearMetadataCache();
}
```



## 7. 注册BeanPostProcessors

注册bean前置处理器

```java
//注册 BeanPostProcessors 方法整体结构与invokeBeanFactoryPostProcessors类似,根据不同的BeanPostProcessor分类后进行注册 优先PriorityOrdered类型,然后Ordered类型,之后是其他普通的最后是内部类型
public static void registerBeanPostProcessors(
      ConfigurableListableBeanFactory beanFactory, AbstractApplicationContext applicationContext) {

   String[] postProcessorNames = beanFactory.getBeanNamesForType(BeanPostProcessor.class, true, false);

   // Register BeanPostProcessorChecker that logs an info message when
   // a bean is created during BeanPostProcessor instantiation, i.e. when
   // a bean is not eligible for getting processed by all BeanPostProcessors.
   int beanProcessorTargetCount = beanFactory.getBeanPostProcessorCount() + 1 + postProcessorNames.length;
   beanFactory.addBeanPostProcessor(new BeanPostProcessorChecker(beanFactory, beanProcessorTargetCount));

   // Separate between BeanPostProcessors that implement PriorityOrdered,
   // Ordered, and the rest.
   List<BeanPostProcessor> priorityOrderedPostProcessors = new ArrayList<>();
   List<BeanPostProcessor> internalPostProcessors = new ArrayList<>();
   List<String> orderedPostProcessorNames = new ArrayList<>();
   List<String> nonOrderedPostProcessorNames = new ArrayList<>();
   for (String ppName : postProcessorNames) {
      if (beanFactory.isTypeMatch(ppName, PriorityOrdered.class)) {
         BeanPostProcessor pp = beanFactory.getBean(ppName, BeanPostProcessor.class);
         priorityOrderedPostProcessors.add(pp);
         if (pp instanceof MergedBeanDefinitionPostProcessor) {
            internalPostProcessors.add(pp);
         }
      }
      else if (beanFactory.isTypeMatch(ppName, Ordered.class)) {
         orderedPostProcessorNames.add(ppName);
      }
      else {
         nonOrderedPostProcessorNames.add(ppName);
      }
   }

   // First, register the BeanPostProcessors that implement PriorityOrdered.
   sortPostProcessors(priorityOrderedPostProcessors, beanFactory);
   registerBeanPostProcessors(beanFactory, priorityOrderedPostProcessors);

   // Next, register the BeanPostProcessors that implement Ordered.
   List<BeanPostProcessor> orderedPostProcessors = new ArrayList<>(orderedPostProcessorNames.size());
   for (String ppName : orderedPostProcessorNames) {
      BeanPostProcessor pp = beanFactory.getBean(ppName, BeanPostProcessor.class);
      orderedPostProcessors.add(pp);
      if (pp instanceof MergedBeanDefinitionPostProcessor) {
         internalPostProcessors.add(pp);
      }
   }
   sortPostProcessors(orderedPostProcessors, beanFactory);
   registerBeanPostProcessors(beanFactory, orderedPostProcessors);

   // Now, register all regular BeanPostProcessors.
   List<BeanPostProcessor> nonOrderedPostProcessors = new ArrayList<>(nonOrderedPostProcessorNames.size());
   for (String ppName : nonOrderedPostProcessorNames) {
      BeanPostProcessor pp = beanFactory.getBean(ppName, BeanPostProcessor.class);
      nonOrderedPostProcessors.add(pp);
      if (pp instanceof MergedBeanDefinitionPostProcessor) {
         internalPostProcessors.add(pp);
      }
   }
   registerBeanPostProcessors(beanFactory, nonOrderedPostProcessors);

   // Finally, re-register all internal BeanPostProcessors.
   sortPostProcessors(internalPostProcessors, beanFactory);
   registerBeanPostProcessors(beanFactory, internalPostProcessors);

   // Re-register post-processor for detecting inner beans as ApplicationListeners,
   // moving it to the end of the processor chain (for picking up proxies etc).
    //最后重新注册ApplicationListenerDetector
   beanFactory.addBeanPostProcessor(new ApplicationListenerDetector(applicationContext));
}
```

## 8.初始消息源

```java
protected void initMessageSource() {
   ConfigurableListableBeanFactory beanFactory = getBeanFactory();
   if (beanFactory.containsLocalBean(MESSAGE_SOURCE_BEAN_NAME)) {
     ...

   }
   else {
      //使用空 MessageSource 能够接受 getMessage 调用。
      DelegatingMessageSource dms = new DelegatingMessageSource();
      dms.setParentMessageSource(getInternalParentMessageSource());
      this.messageSource = dms;
      beanFactory.registerSingleton(MESSAGE_SOURCE_BEAN_NAME, this.messageSource);

   }
}
```

## 9. 初始化事件传播器

```java
protected void initApplicationEventMulticaster() {
   ConfigurableListableBeanFactory beanFactory = getBeanFactory();
   if (beanFactory.containsLocalBean(APPLICATION_EVENT_MULTICASTER_BEAN_NAME)) {
     .......
   }
   else {
       //创建并注册
      this.applicationEventMulticaster = new SimpleApplicationEventMulticaster(beanFactory);
      beanFactory.registerSingleton(APPLICATION_EVENT_MULTICASTER_BEAN_NAME, this.applicationEventMulticaster);

   }
}
```

## 10. 初始化特殊bean

```java
@Override
protected void onRefresh() {
//初始化themeSource类;主要用于消息参数化和国际化
   super.onRefresh();
   try {
   		//创建webserver
      createWebServer();
   }
   catch (Throwable ex) {
      throw new ApplicationContextException("Unable to start web server", ex);
   }
}
```

### 10.1 创建webserver

```java
private void createWebServer() {
   WebServer webServer = this.webServer;
   ServletContext servletContext = getServletContext();
   if (webServer == null && servletContext == null) {
       //调用getBean方法初始化第一个ServletWebServerFactory bean
      ServletWebServerFactory factory = getWebServerFactory();
       //getSelfInitializer 准备web环境,设置scope为application,并获取ServletContextInitializer,调用onStartup方法 使用初始化所需的任何 servlet、filters、listeners上下文参数和属性配置给定的ServletContext ,创建tomcat并连接
      this.webServer = factory.getWebServer(getSelfInitializer());
      getBeanFactory().registerSingleton("webServerGracefulShutdown",
            new WebServerGracefulShutdownLifecycle(this.webServer));
      getBeanFactory().registerSingleton("webServerStartStop",
            new WebServerStartStopLifecycle(this, this.webServer));
   }
   else if (servletContext != null) {
      try {
          
         getSelfInitializer().onStartup(servletContext);
      }
      catch (ServletException ex) {
         throw new ApplicationContextException("Cannot initialize servlet context", ex);
      }
   }
    //将基于Servlet的stub property sources替换为填充给定servletContext和servletConfig对象的实际实例。
   initPropertySources();
}
```

## 11. 增加listener

```java
protected void registerListeners() {
   // Register statically specified listeners first.
   for (ApplicationListener<?> listener : getApplicationListeners()) {
      getApplicationEventMulticaster().addApplicationListener(listener);
   }

   // Do not initialize FactoryBeans here: We need to leave all regular beans
   // uninitialized to let post-processors apply to them!
   String[] listenerBeanNames = getBeanNamesForType(ApplicationListener.class, true, false);
   for (String listenerBeanName : listenerBeanNames) {
      getApplicationEventMulticaster().addApplicationListenerBean(listenerBeanName);
   }

   // Publish early application events now that we finally have a multicaster...
   Set<ApplicationEvent> earlyEventsToProcess = this.earlyApplicationEvents;
   this.earlyApplicationEvents = null;
   if (!CollectionUtils.isEmpty(earlyEventsToProcess)) {
      for (ApplicationEvent earlyEvent : earlyEventsToProcess) {
         getApplicationEventMulticaster().multicastEvent(earlyEvent);
      }
   }
```

## 12. 加载除了lazy外的所有bean

```java
protected void finishBeanFactoryInitialization(ConfigurableListableBeanFactory beanFactory) {

   // Stop using the temporary ClassLoader for type matching.
   beanFactory.setTempClassLoader(null);

   // Allow for caching all bean definition metadata, not expecting further changes.
    // 允许缓存所有的bean元数据
   beanFactory.freezeConfiguration();

   //加载所有bean
   beanFactory.preInstantiateSingletons();
}
```

### 12.1 实例化所有非惰性初始化单例

```java
public void preInstantiateSingletons() throws BeansException {

   // Iterate over a copy to allow for init methods which in turn register new bean definitions.
   // While this may not be part of the regular factory bootstrap, it does otherwise work fine.
   List<String> beanNames = new ArrayList<>(this.beanDefinitionNames);

   //初始化bean对象
   for (String beanName : beanNames) {
      RootBeanDefinition bd = getMergedLocalBeanDefinition(beanName);
      if (!bd.isAbstract() && bd.isSingleton() && !bd.isLazyInit()) {
         if (isFactoryBean(beanName)) {
            Object bean = getBean(FACTORY_BEAN_PREFIX + beanName);
            if (bean instanceof FactoryBean) {
               FactoryBean<?> factory = (FactoryBean<?>) bean;
               boolean isEagerInit;
               if (System.getSecurityManager() != null && factory instanceof SmartFactoryBean) {
                  isEagerInit = AccessController.doPrivileged(
                        (PrivilegedAction<Boolean>) ((SmartFactoryBean<?>) factory)::isEagerInit,
                        getAccessControlContext());
               }
               else {
                  isEagerInit = (factory instanceof SmartFactoryBean &&
                        ((SmartFactoryBean<?>) factory).isEagerInit());
               }
               if (isEagerInit) {
                  getBean(beanName);
               }
            }
         }
         else {
            getBean(beanName);
         }
      }
   }

   // 为所有适用的 bean 触发初始化后回调...
   for (String beanName : beanNames) {
      Object singletonInstance = getSingleton(beanName);
      if (singletonInstance instanceof SmartInitializingSingleton) {
         SmartInitializingSingleton smartSingleton = (SmartInitializingSingleton) singletonInstance;
         if (System.getSecurityManager() != null) {
            AccessController.doPrivileged((PrivilegedAction<Object>) () -> {
               smartSingleton.afterSingletonsInstantiated();
               return null;
            }, getAccessControlContext());
         }
         else {
            smartSingleton.afterSingletonsInstantiated();
         }
      }
   }
}
```



### 12.2 初始化bean对象

```java
//返回bean实例,该实例可以共享的也可以是独立的
protected <T> T doGetBean(
      String name, @Nullable Class<T> requiredType, @Nullable Object[] args, boolean typeCheckOnly)
      throws BeansException {

   String beanName = transformedBeanName(name);
   Object bean;

   // Eagerly check singleton cache for manually registered singletons.
    //尝试从缓存中加载
   Object sharedInstance = getSingleton(beanName);
   if (sharedInstance != null && args == null) {
       //返回beanName对应的实例对象（主要用于FactoryBean的特殊处理，普通Bean会直接返回sharedInstance本身）
      bean = getObjectForBeanInstance(sharedInstance, name, beanName, null);
   }

   else {
      // Fail if we're already creating this bean instance:
      // We're assumably within a circular reference.
       //在prototype的scope中校验循环引用;如果beanName已经正在创建Bean实例中，而此时我们又要再一次创建beanName的实例，则代表出现了循环依赖，需要抛出异常
      if (isPrototypeCurrentlyInCreation(beanName)) {
         throw new BeanCurrentlyInCreationException(beanName);
      }

      // Check if bean definition exists in this factory.
      //检查这个bean是否在改beanFactory中被定义了
      //获取父 BeanFactory
      BeanFactory parentBeanFactory = getParentBeanFactory();
      if (parentBeanFactory != null && !containsBeanDefinition(beanName)) {
         // Not found -> check parent.
          //如果父BeanFactory不为空,则获取该bean的原始名称,并从父BeanFactory中获取
         String nameToLookup = originalBeanName(name);
         if (parentBeanFactory instanceof AbstractBeanFactory) {
            return ((AbstractBeanFactory) parentBeanFactory).doGetBean(
                  nameToLookup, requiredType, args, typeCheckOnly);
         }
         else if (args != null) {
            // Delegation to parent with explicit args.
            return (T) parentBeanFactory.getBean(nameToLookup, args);
         }
         else if (requiredType != null) {
            // No args -> delegate to standard getBean method.
            return parentBeanFactory.getBean(nameToLookup, requiredType);
         }
         else {
            return (T) parentBeanFactory.getBean(nameToLookup);
         }
      }
	  //如果不为检查类型则设置缓存,允许BeanFactory创建该实例	
      if (!typeCheckOnly) {
          //将beanName放到alreadyCreated缓存
         markBeanAsCreated(beanName);
      }

      try {
          //获取合并后的bean定义
         RootBeanDefinition mbd = getMergedLocalBeanDefinition(beanName);
         checkMergedBeanDefinition(mbd, beanName, args);

         // Guarantee initialization of beans that the current bean depends on.
          // 保证当前bean依赖的bean都已经创建
         String[] dependsOn = mbd.getDependsOn();
         if (dependsOn != null) {
            for (String dep : dependsOn) {
                //判断是否循环引用
               if (isDependent(beanName, dep)) {
                  throw new BeanCreationException(mbd.getResourceDescription(), beanName,
                        "Circular depends-on relationship between '" + beanName + "' and '" + dep + "'");
               }
                //为给定的 bean 注册一个依赖 bean，在给定的 bean 被销毁之前被销毁;将dep和beanName的依赖关系注册到缓存中
               registerDependentBean(dep, beanName);
               try {
                  //获取dep对应的bean实例，如果dep还没有创建bean实例，则创建dep的bean实例
                  getBean(dep);
               }
               catch (NoSuchBeanDefinitionException ex) {
                  throw new BeanCreationException(mbd.getResourceDescription(), beanName,
                        "'" + beanName + "' depends on missing bean '" + dep + "'", ex);
               }
            }
         }

         // Create bean instance.
         //创建bean实例
         if (mbd.isSingleton()) {
             //调用单例情况下的创建Bean操作,利用当前Factory创建
            sharedInstance = getSingleton(beanName, () -> {
               try {
                   //创建bean
                  return createBean(beanName, mbd, args);
               }
               catch (BeansException ex) {
                  // Explicitly remove instance from singleton cache: It might have been put there
                  // eagerly by the creation process, to allow for circular reference resolution.
                  // Also remove any beans that received a temporary reference to the bean.
                  destroySingleton(beanName);
                  throw ex;
               }
            });
             //根据名称获取给定 bean 实例的对象，如果是 FactoryBean，则是 bean 实例本身或其创建的对象。
            bean = getObjectForBeanInstance(sharedInstance, name, beanName, mbd);
         }

         else if (mbd.isPrototype()) {
            // It's a prototype -> create a new instance.
            Object prototypeInstance = null;
            try {
               //创建实例前的操作（将beanName保存到prototypesCurrentlyInCreation缓存中）
               beforePrototypeCreation(beanName);               
               prototypeInstance = createBean(beanName, mbd, args);
            }
            finally {
                //创建实例后的操作（将创建完的beanName从prototypesCurrentlyInCreation缓存中移除）
               afterPrototypeCreation(beanName);
            }
            bean = getObjectForBeanInstance(prototypeInstance, name, beanName, mbd);
         }

         else {
            String scopeName = mbd.getScope();
            if (!StringUtils.hasLength(scopeName)) {
               throw new IllegalStateException("No scope name defined for bean ´" + beanName + "'");
            }
            Scope scope = this.scopes.get(scopeName);
            if (scope == null) {
               throw new IllegalStateException("No Scope registered for scope name '" + scopeName + "'");
            }
            try {
                //其他scope的情况下,同Prototype的创建流程
               Object scopedInstance = scope.get(beanName, () -> {
                  beforePrototypeCreation(beanName);
                  try {
                     return createBean(beanName, mbd, args);
                  }
                  finally {
                     afterPrototypeCreation(beanName);
                  }
               });
               bean = getObjectForBeanInstance(scopedInstance, name, beanName, mbd);
            }
            catch (IllegalStateException ex) {
               throw new BeanCreationException(beanName,
                     "Scope '" + scopeName + "' is not active for the current thread; consider " +
                     "defining a scoped proxy for this bean if you intend to refer to it from a singleton",
                     ex);
            }
         }
      }
      catch (BeansException ex) {
           // 如果创建bean实例过程中出现异常，则将beanName从alreadyCreated缓存中移除
         cleanupAfterBeanCreationFailure(beanName);
         throw ex;
      }
   }

   // Check if required type matches the type of the actual bean instance.
    //检查所需类型是否与实际 bean 实例的类型匹配。
   if (requiredType != null && !requiredType.isInstance(bean)) {
      try {
         T convertedBean = getTypeConverter().convertIfNecessary(bean, requiredType);
         if (convertedBean == null) {
            throw new BeanNotOfRequiredTypeException(name, requiredType, bean.getClass());
         }
         return convertedBean;
      }
      catch (TypeMismatchException ex) {
         if (logger.isTraceEnabled()) {
            logger.trace("Failed to convert bean '" + name + "' to required type '" +
                  ClassUtils.getQualifiedName(requiredType) + "'", ex);
         }
         throw new BeanNotOfRequiredTypeException(name, requiredType, bean.getClass());
      }
   }
   return (T) bean;
}
```





引用:

[invokeBeanFactoryPostProcessors 详解](https://blog.csdn.net/qq_44836294/article/details/107794445)

[registerBeanPostProcessors 详解](https://blog.csdn.net/qq_44836294/article/details/107795388)

[Spring源码分析之扫描注册Bean----ConfigurationClassPostProcessor的processConfigBeanDefinitions方法](https://blog.csdn.net/jy02268879/article/details/88062673)

[finishBeanFactoryInitialization 详解（六）](https://blog.csdn.net/qq_44836294/article/details/107795639)

[getBean 详解（七）](https://blog.csdn.net/qq_44836294/article/details/108057618)