

# Springboot 启动流程 - run方法解析

springboot启动是当初始化完成`SpringApplication`后调用`run`方法进行容器的初始化和启动操作.

## Run方法解析

```java
public ConfigurableApplicationContext run(String... args) {
    //StopWatch 性能监控,监控启动时长
   StopWatch stopWatch = new StopWatch();
   stopWatch.start();
   //spring配置上下文 !
   ConfigurableApplicationContext context = null;
   //用于支持自定义上报SpringApplication启动错误的回调接口
   Collection<SpringBootExceptionReporter> exceptionReporters = new ArrayList<>();
   //设置关于图像处理,设置java.awt.headless属性为true 
   configureHeadlessProperty();
    //初始化SpringApplicationRunListeners,监听器为所有类型为SpringApplicationRunListener且在配置文件中指定的实例
   SpringApplicationRunListeners listeners = getRunListeners(args);
    //实际调用为EventPublishingRunListener#starting,推送ApplicationStartingEvent事件,触发的监听器分别为LoggingApplicationListener/BackgroundPreinitializer/DelegatingApplicationListener/LiquibaseServiceLocatorApplicationListener, 实际只执行了LoggingApplicationListener的logsystem 初始化
   listeners.starting();
   try {
       //初始化applicationArguments类,初始化过程中会触发参数解析,new SimpleCommandLineArgsParser().parse(args)
      ApplicationArguments applicationArguments = new DefaultApplicationArguments(args);
       //创建和配置环境
      ConfigurableEnvironment environment = prepareEnvironment(listeners, applicationArguments);
       //配置忽略Bean的信息,默认为spring.beaninfo.ignore包
      configureIgnoreBeanInfo(environment);
       //打印banner信息
      Banner printedBanner = printBanner(environment);
       //根据环境创建application上下文,此处为AnnotationConfigServletWebServerApplicationContext类
      context = createApplicationContext();
       //实例化配置的SpringBootExceptionReporter类集合
      exceptionReporters = 
          //获取springfactories中配置的SpringBootExceptionReporter信息并初始化
          getSpringFactoriesInstances(SpringBootExceptionReporter.class,
            new Class[] { ConfigurableApplicationContext.class }, context);
      	//准备上下文
       prepareContext(context, environment, listeners, applicationArguments, printedBanner);
       //启动容器
      refreshContext(context);
       //springboot空实现
      afterRefresh(context, applicationArguments);
       //计算性能
      stopWatch.stop();
       //打印容器启动时长等相关信息
      if (this.logStartupInfo) {
         new StartupInfoLogger(this.mainApplicationClass).logStarted(getApplicationLog(), stopWatch);
      }
       //发布ApplicationStartedEvent,AvailabilityChangeEvent事件
      listeners.started(context);
      callRunners(context, applicationArguments);
   }
   catch (Throwable ex) {
       //处理失败情况
      handleRunFailure(context, ex, exceptionReporters, listeners);
      throw new IllegalStateException(ex);
   }

   try {
       //发布启动成功事件
      listeners.running(context);
   }
   catch (Throwable ex) {
      handleRunFailure(context, ex, exceptionReporters, null);
      throw new IllegalStateException(ex);
   }
   return context;
}
```



### 1.2  初始化Environment 

```java
private ConfigurableEnvironment prepareEnvironment(SpringApplicationRunListeners listeners,
      ApplicationArguments applicationArguments) {
   // Create and configure the environment
   //根据webApplicationType类型初始化不同的 ConfigurableEnvironment 类型,此处为StandardServletEnvironment
   ConfigurableEnvironment environment = getOrCreateEnvironment();
    //配置Environment的属性,active文件,ConversionService,配置各种解析器
   configureEnvironment(environment, applicationArguments.getSourceArgs());
    //将PropertySources放到最前面,类型为SpringConfigurationPropertySources,包含source中的所有配置,确保ConfigurationPropertySource支持各个环境
   ConfigurationPropertySources.attach(environment);
   //出发ApplicationEnvironmentPreparedEvent事件 
   listeners.environmentPrepared(environment);
   //初始化binder类, 将application与environment绑定
   bindToSpringApplication(environment);
   if (!this.isCustomEnvironment) {
       //根据webApplicationType类型将environment转化为不同的StandardEnvironment类
      environment = new EnvironmentConverter(getClassLoader()).convertEnvironmentIfNecessary(environment,
            deduceEnvironmentClass());
   }
   //将PropertySources放到最前面,类型为SpringConfigurationPropertySources,包含source中的所有配置,确保ConfigurationPropertySource支持各个环境
   ConfigurationPropertySources.attach(environment);
   return environment;
}
```

#### 1.2.1 配置Environment

填充Environment相关属性,如类型转换器等,以及默认的属性和配置文件

```java
protected void configureEnvironment(ConfigurableEnvironment environment, String[] args) {
   if (this.addConversionService) {
   // 初始化conversionService 单例模式
      ConversionService conversionService = ApplicationConversionService.getSharedInstance();
       //设置对属性执行类型转换时需要用的ConfigurableConversionService
      environment.setConversionService((ConfigurableConversionService) conversionService);
   }
   //配置属性(包括默认,及CommandLine)更新environment.getPropertySources()字段 
   configurePropertySources(environment, args);
   //设置active配置文件,如果设置了spring.profiles.active属性的话,修改environment.setActiveProfiles()字段 
   configureProfiles(environment, args);
}
```
#### 1.2.2  设置active profile属性


```java
//2. 看注释的意思，大概就是找到配置中的 active profile的值
protected void configureProfiles(ConfigurableEnvironment environment, String[] args) {
    // 1 让ServletStandardEnvironment中的 PropertySourcesPropertyResolver去 已经加载了的 MutablePropertySources中，遍历每一个PropertySources, 获取里面的spring.profiles.active对应的值，，
    // 2 并执行 this.activeProfiles.add(profile);，添加到 AbstractEnvironment的实例变量private final Set<String> activeProfiles 中
    // 3 一般情况下，是不存在的，我们一般指定的地方都是在 application.properties文件中指定的，因为此时还没有读到，所以 此时获取到的是空
  	// 4 那什么时候能获取到呢，PropertiesPropertySource 或者 SystemEnvironmentPropertySource 中指定了，或者 SimpleCommandLinePropertySource中，
  	// SimpleCommandLinePropertySource中 可以在 idea -> Edit Configurations -> Environment -> Program argumemts中指定
    // PropertiesPropertySource或者SystemEnvironmentPropertySource中一个是在 idea -> Edit Configurations -> Environment -> VM Option 中指定的，忘记哪个是了。
  	//具体代码不贴了，太多。 
	environment.getActiveProfiles(); 
	// But these ones should go first (last wins in a property key clash)
	Set<String> profiles = new LinkedHashSet<>(this.additionalProfiles);
	profiles.addAll(Arrays.asList(environment.getActiveProfiles()));
	environment.setActiveProfiles(StringUtils.toStringArray(profiles));
  	// 一般情况下setActiveProfiles设置的是null,因为我们都是在application.properties文件指定的，而不会再运行变量中指定profile.
}
```

#### 1.2.3 绑定容器与propertySources

```java
protected void bindToSpringApplication(ConfigurableEnvironment environment) {
   try {
   //初始化Binder类,利用environment获取propertySources,填充占位符解析器等信息
     //将ConfigurationPropertySources与容器对象springApplication绑定
      //Bindable.ofInstance(this) 创建一个类型为SpringApplication的Bindable,并将现有值复制给该实例
      Binder.get(environment).bind("spring.main", Bindable.ofInstance(this));
   }
   catch (Exception ex) {
      throw new IllegalStateException("Cannot bind to SpringApplication", ex);
   }
}
```
### 1.3 跳过对BeanInfo类的搜索


```java
private void configureIgnoreBeanInfo(ConfigurableEnvironment environment) {
   if 
	// 系统缺省设置时设置为true
  (System.getProperty(CachedIntrospectionResults.IGNORE_BEANINFO_PROPERTY_NAME) == null) {
       //值为“true”时跳过对BeanInfo类的搜索
      Boolean ignore = environment.getProperty("spring.beaninfo.ignore", Boolean.class, Boolean.TRUE);
      System.setProperty(CachedIntrospectionResults.IGNORE_BEANINFO_PROPERTY_NAME, ignore.toString());
   }
}
```
### 1.4 创建上下文

```java
protected ConfigurableApplicationContext createApplicationContext() {
//根据应用启动类型创建对应的上下文
   Class<?> contextClass = this.applicationContextClass;
   if (contextClass == null) {
      try {
         switch (this.webApplicationType) {
         case SERVLET:
            contextClass = Class.forName(DEFAULT_SERVLET_WEB_CONTEXT_CLASS);
            break;
         case REACTIVE:
            contextClass = Class.forName(DEFAULT_REACTIVE_WEB_CONTEXT_CLASS);
            break;
         default:
            contextClass = Class.forName(DEFAULT_CONTEXT_CLASS);
         }
      }
      catch (ClassNotFoundException ex) {
         throw new IllegalStateException(
               "Unable create a default ApplicationContext, please specify an ApplicationContextClass", ex);
      }
   }
   return (ConfigurableApplicationContext) BeanUtils.instantiateClass(contextClass);
}
```

### 1.5 设置容器上下文

```java
//准备上下文
private void prepareContext(ConfigurableApplicationContext context, ConfigurableEnvironment environment,
      SpringApplicationRunListeners listeners, ApplicationArguments applicationArguments, Banner printedBanner) {
    //设置上下文的环境
   context.setEnvironment(environment);
    //对应用上下文进行post处理,基本知识设置BeanFactory的ConversionService
   postProcessApplicationContext(context);
    //应用Initializers,每个Initializer负责初始化一部分容器,设置BeanFactoryPostProcessor等
   applyInitializers(context);
    //发布 ApplicationContextInitializedEvent 事件,触发BackgroundPreinitializer,DelegatingApplicationListener监听器
   listeners.contextPrepared(context);
    //打印启动信息 包括active文件名称等
   if (this.logStartupInfo) {
      logStartupInfo(context.getParent() == null);
      logStartupProfileInfo(context);
   }
   // Add boot specific singleton beans 添加启动特定的单例 bean,springApplicationArguments bean
   ConfigurableListableBeanFactory beanFactory = context.getBeanFactory();
   beanFactory.registerSingleton("springApplicationArguments", applicationArguments);
   if (printedBanner != null) {
       //注册单例springBootBanner
      beanFactory.registerSingleton("springBootBanner", printedBanner);
   }
    //beanFactory类型为DefaultListableBeanFactory
   if (beanFactory instanceof DefaultListableBeanFactory) {
      ((DefaultListableBeanFactory) beanFactory)
   //设置是否允许通过注册具有相同名称/别名的不同定义来覆盖 bean 定义，自动替换前者,false情况下会报错    
       .setAllowBeanDefinitionOverriding(this.allowBeanDefinitionOverriding);
   }
   if (this.lazyInitialization) {
      context.addBeanFactoryPostProcessor(new LazyInitializationBeanFactoryPostProcessor());
   }
   // Load the sources,一般是主类
   Set<Object> sources = getAllSources();
   Assert.notEmpty(sources, "Sources must not be empty");
    //注册resources 为bean 并count+1
   load(context, sources.toArray(new Object[0]));
    //新增监听器 CloudFoundryVcapEnvironmentPostProcessor,并发布ApplicationPreparedEvent事件
   listeners.contextLoaded(context);
}
```
#### 1.5.1 添加容器后置处理 

```java
protected void postProcessApplicationContext(ConfigurableApplicationContext context) {
   if (this.beanNameGenerator != null) {
      context.getBeanFactory().registerSingleton(AnnotationConfigUtils.CONFIGURATION_BEAN_NAME_GENERATOR,
            this.beanNameGenerator);
   }
   if (this.resourceLoader != null) {
      if (context instanceof GenericApplicationContext) {
         ((GenericApplicationContext) context).setResourceLoader(this.resourceLoader);
      }
      if (context instanceof DefaultResourceLoader) {
         ((DefaultResourceLoader) context).setClassLoader(this.resourceLoader.getClassLoader());
      }
   }
    //添加转化服务
   if (this.addConversionService) {
     //返回默认实例用于延迟构建getSharedInstance 
       context.getBeanFactory().setConversionService(ApplicationConversionService.getSharedInstance());
   }
}
```

```java
protected void applyInitializers(ConfigurableApplicationContext context) {
 //获取配置的ApplicationContextInitializer类,针对上下文进行各种初始化操作
   for (ApplicationContextInitializer initializer : getInitializers()) {
      Class<?> requiredType = GenericTypeResolver.resolveTypeArgument(initializer.getClass(),
            ApplicationContextInitializer.class);
      Assert.isInstanceOf(requiredType, context, "Unable to call initializer.");
      initializer.initialize(context);
   }
}
```

```java
private int load(Class<?> source) {
   if (isGroovyPresent() && GroovyBeanDefinitionSource.class.isAssignableFrom(source)) {
      // Any GroovyLoaders added in beans{} DSL can contribute beans here
      GroovyBeanDefinitionSource loader = BeanUtils.instantiateClass(source, GroovyBeanDefinitionSource.class);
      load(loader);
   }
    //主要逻辑注册source的bean
   if (isEligible(source)) {
      this.annotatedReader.register(source);
      return 1;
   }
   return 0;
}
```

### 1.6 初始化容器

```java
//刷新上下文
private void refreshContext(ConfigurableApplicationContext context) {
   if (this.registerShutdownHook) {
      try {
          //注册钩子函数,实际实现/**Runtime.getRuntime().addShutdownHook(this.shutdownHook);**/
         context.registerShutdownHook();
          
          
      }
      catch (AccessControlException ex) {
         // Not allowed in some environments.
      }
   }
    //启动spring容器
   refresh((ApplicationContext) context);
}
```

### 1.7 触发ApplicationStartedEvent事件

EventPublishingRunListener#started

```java
public void started(ConfigurableApplicationContext context) {
    //触发ApplicationStartedEvent事件,初始化时对应的listener没有其他操作
   context.publishEvent(new ApplicationStartedEvent(this.application, this.args, context));
   AvailabilityChangeEvent.publish(context, LivenessState.CORRECT);
}
```

### 1.8 执行Runner调用

​     调用ApplicationRunner和CommandLineRunner的run方法，实现spring容器启动后需要做的一些东西

```java
private void callRunners(ApplicationContext context, ApplicationArguments args) {
   List<Object> runners = new ArrayList<>();
   runners.addAll(context.getBeansOfType(ApplicationRunner.class).values());
   runners.addAll(context.getBeansOfType(CommandLineRunner.class).values());
   AnnotationAwareOrderComparator.sort(runners);
   for (Object runner : new LinkedHashSet<>(runners)) {
      if (runner instanceof ApplicationRunner) {
         callRunner((ApplicationRunner) runner, args);
      }
      if (runner instanceof CommandLineRunner) {
         callRunner((CommandLineRunner) runner, args);
      }
   }
}
```

 



参考:

[什么是Java Headless模式？](http://jimolonely.github.io/2019/03/26/java/039-java-headless/)

[spring启动流程](https://juejin.cn/post/6844904099897409550)

[spring内置ApplicationListener](https://blog.csdn.net/andy_zhang2007/article/details/84105284)

[prepareEnvironment()环境配置 方法解析](https://juejin.cn/post/6844904087574544398)

[Spring Boot启动源码分析【一万字】](https://juejin.cn/post/7114303555724869668)

[启动流程图](https://www.processon.com/view/60605358f346fb6d9ef1bd9c?fromnew=1)

[SpringBoot 启动原理](https://mp.weixin.qq.com/s/KgMpU_CdG87PUMgVF7_f6g)