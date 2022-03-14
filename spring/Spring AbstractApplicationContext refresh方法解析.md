# Spring AbstractApplicationContext refresh方法解析

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

```java
AnnotationConfigReactiveWebServerApplicationContext#prepareRefresh
prepareRefresh方法实际实现
protected void prepareRefresh() {
    //调用 CachingMetadataReaderFactory#clearCache
   this.scanner.clearCache();
   super.prepareRefresh();
}
```

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

```java
protected void prepareRefresh() {
   // Switch to active. 设置上下文活动状态
   this.startupDate = System.currentTimeMillis();
   this.closed.set(false);
   this.active.set(true);

   if (logger.isDebugEnabled()) {
      if (logger.isTraceEnabled()) {
         logger.trace("Refreshing " + this);
      }
      else {
         logger.debug("Refreshing " + getDisplayName());
      }
   }

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

```java
//registerWebApplicationScopes 方法中实际调用,传参为beanFactory,null;所在类WebApplicationContextUtils
public static void registerWebApplicationScopes(ConfigurableListableBeanFactory beanFactory,
      @Nullable ServletContext sc) {
	//注册beanFactory的范围 request/session
   beanFactory.registerScope(WebApplicationContext.SCOPE_REQUEST, new RequestScope());
   beanFactory.registerScope(WebApplicationContext.SCOPE_SESSION, new SessionScope());
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
   if (jsfPresent) {
      FacesDependencyRegistrar.registerFacesDependencies(beanFactory);
   }
}
```

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

```java
@Override
protected void onRefresh() {
//初始化themeSource类;主要用于消息参数化和国际化
   super.onRefresh();
   try {
   		//
      createWebServer();
   }
   catch (Throwable ex) {
      throw new ApplicationContextException("Unable to start web server", ex);
   }
}
```

```java
private void createWebServer() {
   WebServer webServer = this.webServer;
   ServletContext servletContext = getServletContext();
   if (webServer == null && servletContext == null) {
       //调用getBean方法初始化第一个ServletWebServerFactory bean
      ServletWebServerFactory factory = getWebServerFactory();
       //getSelfInitializer 准备web环境,设置scope为application,并获取ServletContextInitializer,调用onStartup方法 使用初始化所需的任何 servlet、filters、listeners上下文参数和属性配置给定的ServletContext 
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
      if (logger.isTraceEnabled()) {
         if (isSingletonCurrentlyInCreation(beanName)) {
            logger.trace("Returning eagerly cached instance of singleton bean '" + beanName +
                  "' that is not fully initialized yet - a consequence of a circular reference");
         }
         else {
            logger.trace("Returning cached instance of singleton bean '" + beanName + "'");
         }
      }
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