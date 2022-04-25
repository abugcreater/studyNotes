# Spring MVC

## 初始化

spring-mvc的核心便是DispatcherServlet

![DispatcherServlet类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/DispatcherServlet.jpg)

Servlet标准定义了init方法是其生命周期的初始化方法。

`HttpServletBean.init`

```java
//将配置参数映射到此 servlet 的 bean 属性，并调用子类初始化
public final void init() throws ServletException {
   // Set bean properties from init parameters.
    //从初始化参数设置 bean 属性。
   PropertyValues pvs = new ServletConfigPropertyValues(getServletConfig(), this.requiredProperties);
	//包装DispatcherServlet，准备放入容器
   BeanWrapper bw = PropertyAccessorFactory.forBeanPropertyAccess(this);
   //初始化资源加载器,用以加载 spring-mvc 
   ResourceLoader resourceLoader = new ServletContextResourceLoader(getServletContext());
   //为给定类型的所有属性注册给定的自定义属性编辑器。 
   bw.registerCustomEditor(Resource.class, new ResourceEditor(resourceLoader, getEnvironment()));
    //空实现
   initBeanWrapper(bw);
   bw.setPropertyValues(pvs, true);


   // Let subclasses do whatever initialization they like.
   initServletBean();

}
```

**setPropertyValues方法会导致对DispatcherServlet相关setter方法的调用，所以当进行容器初始化时从init-param中读取的参数已被设置到DispatcherServlet的相关字段(Field)中**。

### 容器初始化

`FrameworkServlet.initServletBean`简略版源码:

```java
//设置任何 bean 属性后调用。创建此 servlet 的 WebApplicationContext
protected final void initServletBean() throws ServletException {
   //初始化web上下文环境 
   this.webApplicationContext = initWebApplicationContext();
   //空实现,没有子类实现 
   initFrameworkServlet();
}
```

`FrameworkServlet#initWebApplicationContext`

```java
//创建此 servlet 的 WebApplicationContext
protected WebApplicationContext initWebApplicationContext() {
    //根据容器查找
   WebApplicationContext rootContext =
         WebApplicationContextUtils.getWebApplicationContext(getServletContext());
   WebApplicationContext wac = null;

   if (this.webApplicationContext != null) {//当前servlet webApplicationContext不为空
      // 在构建时注入上下文实例
      wac = this.webApplicationContext;
      if (wac instanceof ConfigurableWebApplicationContext) {
          //强转为ConfigurableWebApplicationContext
         ConfigurableWebApplicationContext cwac = (ConfigurableWebApplicationContext) wac;
         if (!cwac.isActive()) {
            //该状态下代表上下文未被刷新,需要设置父上下文,上下文id等 
            if (cwac.getParent() == null) {
               cwac.setParent(rootContext);
            }
             //设置id,namespce等基础属性
            configureAndRefreshWebApplicationContext(cwac);
         }
      }
   }
   if (wac == null) {
	  //在servlet中寻找context	
      wac = findWebApplicationContext();
   }
   if (wac == null) {
      //如果没有则创建一个 
      wac = createWebApplicationContext(rootContext);
   }

   if (!this.refreshEventReceived) {
      // 调用 DispatcherServlet 中方法 
      onRefresh(wac);
   }

   if (this.publishContext) {
      // Publish the context as a servlet context attribute.
      String attrName = getServletContextAttributeName();
      getServletContext().setAttribute(attrName, wac);
   }

   return wac;
}
```

#### 根容器查找

spring-mvc支持Spring容器与MVC容器共存，此时，Spring容器即根容器，mvc容器将根容器视为父容器。

根据Servlet规范，各组件的加载 顺序如下:

listener -> filter -> servlet

`WebApplicationContextUtils#getWebApplicationContext`根据容器查找

```java
public static WebApplicationContext getWebApplicationContext(ServletContext sc, String attrName) {
    //attrName = "org.springframework.web.context.WebApplicationContext.ROOT"
    Object attr = sc.getAttribute(attrName);
    if (attr == null) {
        return null;
    }
    //强转为WebApplicationContext
    return (WebApplicationContext) attr;
}
```

**如果Spring根容器存在，那么它被保存在ServletContext中，其key为`WebApplicationContext.class.getName() + ".ROOT"`。**

#### 容器创建

`FrameworkServlet.createWebApplicationContext`

```java
//实例化servlet中的WebApplicationContext
protected WebApplicationContext createWebApplicationContext(ApplicationContext parent) {
    //允许我们自定义context类型
   Class<?> contextClass = getContextClass();
   if (!ConfigurableWebApplicationContext.class.isAssignableFrom(contextClass)) {
        throw new ApplicationContextException();
    }
  //使用其无参数构造函数实例化ConfigurableWebApplicationContext
   ConfigurableWebApplicationContext wac =
         (ConfigurableWebApplicationContext) BeanUtils.instantiateClass(contextClass);
	//设置环境,配置等相关信息
   wac.setEnvironment(getEnvironment());
   wac.setParent(parent);
   wac.setConfigLocation(getContextConfigLocation());
	//刷新上下文
   configureAndRefreshWebApplicationContext(wac);
   return wac;
}
```

`FrameworkServlet#configureAndRefreshWebApplicationContext`

```java
protected void configureAndRefreshWebApplicationContext(ConfigurableWebApplicationContext wac) {
   if (ObjectUtils.identityToString(wac).equals(wac.getId())) {
	  //如果默认contextId不为空则设置原始的contextId
      if (this.contextId != null) {
         wac.setId(this.contextId);
      }
      else {
  		//生成默认contextId,规则"WebApplicationContext" + ContextPath + "/" + ServletName()  
          wac.setId(ConfigurableWebApplicationContext.APPLICATION_CONTEXT_ID_PREFIX +
               ObjectUtils.getDisplayString(getServletContext().getContextPath()) + '/' + getServletName());
      }
   }
	//设置WebApplicationContext基本属性
   wac.setServletContext(getServletContext());
   wac.setServletConfig(getServletConfig());
   wac.setNamespace(getNamespace());
   wac.addApplicationListener(new SourceFilteringListener(wac, new ContextRefreshListener()));

   //在刷新之前发生的任何后处理或初始化中使用
   ConfigurableEnvironment env = wac.getEnvironment();
   if (env instanceof ConfigurableWebEnvironment) {
       //用给定的servletContext和servletConfig对象填充的实际实例替换基于Servlet的stub property sources 
      ((ConfigurableWebEnvironment) env).initPropertySources(getServletContext(), getServletConfig());
   }
	//空实现
   postProcessWebApplicationContext(wac);
    //核心代码
   applyInitializers(wac);
   wac.refresh();
}
```

ApplicationContextInitializer允许我们在容器初始化前自定义动作.

```java
//ApplicationContextInitializer 列表
private final List<ApplicationContextInitializer<ConfigurableApplicationContext>> contextInitializers =
			new ArrayList<ApplicationContextInitializer<ConfigurableApplicationContext>>();

//引用ApplicationContextInitializer类
protected void applyInitializers(ConfigurableApplicationContext wac) {
    //获取初始化 参数 globalInitializerClasses
   String globalClassNames = getServletContext().getInitParameter(ContextLoader.GLOBAL_INITIALIZER_CLASSES_PARAM);
   if (globalClassNames != null) {
      for (String className : StringUtils.tokenizeToStringArray(globalClassNames, INIT_PARAM_DELIMITERS)) {
         this.contextInitializers.add(loadInitializer(className, wac));
      }
   }

   if (this.contextInitializerClasses != null) {
      for (String className : StringUtils.tokenizeToStringArray(this.contextInitializerClasses, INIT_PARAM_DELIMITERS)) {
         this.contextInitializers.add(loadInitializer(className, wac));
      }
   }
 //根据order排序	
   AnnotationAwareOrderComparator.sort(this.contextInitializers);
   for (ApplicationContextInitializer<ConfigurableApplicationContext> initializer : this.contextInitializers) {
		//遍历执行初始化方法
      initializer.initialize(wac);
   }
}
```

##### 配置解析

"配置"指的便是spring-servlet.xml

```xml
<context:component-scan base-package="controller"/>
<mvc:annotation-driven/>
<!-- 启用对静态资源使用默认servlet处理，非REST方式不需要 -->
<mvc:default-servlet-handler/>
<!-- 配置视图 -->
<bean class="org.springframework.web.servlet.view.UrlBasedViewResolver">
    <!-- viewClass属性必不可少 -->
    <property name="viewClass" value="org.springframework.web.servlet.view.JstlView"></property>
    <property name="prefix" value="/WEB-INF/"></property>
    <property name="suffix" value=".jsp"></property>
</bean>
```

xml中解析类为`XmlWebApplicationContext`  注解解析类为`AnnotationConfigServletWebServerApplicationContext`而解析的入口便在于对refresh方法的调用，此方法位于AbstractApplicationContext.

XmlWebApplicationContext类图

![XmlWebApplicationContext类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/XmlWebApplicationContext.jpg)

XmlWebApplicationContext通过重写loadBeanDefinitions方法改变了bean加载行为，使其指向spring-servlet.xml。

mvc命名空间的解析器为MvcNamespaceHandler，部分源码:

```java
@Override
public void init() {
    registerBeanDefinitionParser("annotation-driven", new AnnotationDrivenBeanDefinitionParser());
    registerBeanDefinitionParser("default-servlet-handler", 
                                 new DefaultServletHandlerBeanDefinitionParser());
    registerBeanDefinitionParser("interceptors", new IanterceptorsBeanDefinitionParser());
    registerBeanDefinitionParser("view-resolvers", new ViewResolversBeanDefinitionParser());
}
```

###### 注解驱动


 其parse方法负责向Sprng容器注册一些必要的组件，整理如下图:

![mvc-annotation](https://github.com/seaswalker/spring-analysis/raw/master/note/images/mvc-annotation.png)

###### 静态资源处理

即:

```
<mvc:default-servlet-handler/>
```

DefaultServletHandlerBeanDefinitionParser.parse负责向容器注册以下三个组件:

- DefaultServletHttpRequestHandler
- SimpleUrlHandlerMapping
- HttpRequestHandlerAdapter

###### 拦截器

InterceptorsBeanDefinitionParser.parse方法负责**将每一项`mvc:interceptor`配置解析为一个MappedInterceptor bean并注册到容器中**。

###### 视图

有两种方式向Spring容器注册视图:

- 以前采用较土的方式:

  ```xml
  <bean class="org.springframework.web.servlet.view.UrlBasedViewResolver">
    <!-- viewClass属性必不可少 -->
      <property name="viewClass" value="org.springframework.web.servlet.view.JstlView"></property>
      <property name="prefix" value="/WEB-INF/"></property>
      <property name="suffix" value=".jsp"></property>
  </bean>
  ```

- 通过特定的标签:

  ```xml
  <mvc:view-resolvers>
    <mvc:jsp view-class="" />
  </mvc:view-resolvers>
  ```

从这里可以推测出: 拦截器同样支持第一种方式，Spring在查找时应该会查询某一接口的子类。

ViewResolversBeanDefinitionParser.parse方法的作用便是将每一个视图解析为ViewResolver并注册到容器

##### Scope/处理器注册

`AbstractRefreshableWebApplicationContext.postProcessBeanFactory` 该方法在refresh中被调用

```java
//注册请求/会话范围、 ServletContextAwareProcessor等。
protected void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) {
    //用以向实现了ServletContextAware的bean注册ServletContext
   beanFactory.addBeanPostProcessor(new ServletContextAwareProcessor(this.servletContext, this.servletConfig));
    //忽略依赖的接口已完成自动装配
   beanFactory.ignoreDependencyInterface(ServletContextAware.class);
   beanFactory.ignoreDependencyInterface(ServletConfigAware.class);
  //给定BeanFactory注册范围,用以注册"request", "session", "globalSession", "application"四种scope
   WebApplicationContextUtils.registerWebApplicationScopes(beanFactory, this.servletContext);
    //注册环境,将servletContext、servletConfig以及各种启动参数注册到Spring容器中
   WebApplicationContextUtils.registerEnvironmentBeans(beanFactory, this.servletContext, this.servletConfig);
}
```

### MVC初始化

入口位于DispatcherServlet的initStrategies方法(经由onRefresh调用):

```java
protected void initStrategies(ApplicationContext context) {
   //初始化一系列类 `
    //文件上传支持
   initMultipartResolver(context);
   initLocaleResolver(context);
   initThemeResolver(context);
   initHandlerMappings(context);
   initHandlerAdapters(context);
   initHandlerExceptionResolvers(context);
    //此接口用以完成从HttpServletRequest到视图名的解析,404时使用
   initRequestToViewNameTranslator(context);
    //按名称解析视图的对象实现的接口
   initViewResolvers(context);
    //会向容器注册SessionFlashMapManager对象.用于检索和保存 FlashMap 实例的策略接口
   initFlashMapManager(context);
}
```

#### 文件上传支持

initMultipartResolver方法核心代码

```java
//初始化multipartResolver bean 
this.multipartResolver = context.getBean(MULTIPART_RESOLVER_BEAN_NAME, MultipartResolver.class);
```

MultipartResolver用于开启Spring MVC文件上传功能

![MultipartResolver类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/MultipartResolver.jpg)

#### 地区解析器

LocaleResolver接口定义了Spring MVC如何获取客户端(浏览器)的地区，initLocaleResolver方法在容器中寻找此bean，如果没有，注册AcceptHeaderLocaleResolver，即根据request的请求头**Accept-Language**获取地区。

spring-mvc采用了属性文件的方式配置默认策略(即bean)，此文件位于spring-mvc的jar包的org.springframework.web.servlet下。

DispatcherServlet.properties

```properties
org.springframework.web.servlet.LocaleResolver=org.springframework.web.servlet.i18n.AcceptHeaderLocaleResolver

org.springframework.web.servlet.ThemeResolver=org.springframework.web.servlet.theme.FixedThemeResolver

org.springframework.web.servlet.HandlerMapping=org.springframework.web.servlet.handler.BeanNameUrlHandlerMapping,\
   org.springframework.web.servlet.mvc.annotation.DefaultAnnotationHandlerMapping

org.springframework.web.servlet.HandlerAdapter=org.springframework.web.servlet.mvc.HttpRequestHandlerAdapter,\
   org.springframework.web.servlet.mvc.SimpleControllerHandlerAdapter,\
   org.springframework.web.servlet.mvc.annotation.AnnotationMethodHandlerAdapter

org.springframework.web.servlet.HandlerExceptionResolver=org.springframework.web.servlet.mvc.annotation.AnnotationMethodHandlerExceptionResolver,\
   org.springframework.web.servlet.mvc.annotation.ResponseStatusExceptionResolver,\
   org.springframework.web.servlet.mvc.support.DefaultHandlerExceptionResolver

org.springframework.web.servlet.RequestToViewNameTranslator=org.springframework.web.servlet.view.DefaultRequestToViewNameTranslator

org.springframework.web.servlet.ViewResolver=org.springframework.web.servlet.view.InternalResourceViewResolver

org.springframework.web.servlet.FlashMapManager=org.springframework.web.servlet.support.SessionFlashMapManager
```

#### 主题解析器

ThemeResolver接口配合Spring标签库使用可以通过动态决定使用的css以及图片的方式达到换肤的效果,默认使用FixedThemeResolver

![ThemeResolver类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/ThemeResolver.jpg)

#### HandlerMapping检查

确保容器至少有一个`HandlerMapping`.注解驱动导致已经注册了两个此对象。

默认的策略由DispatcherServlet.properties决定，**目前是BeanNameUrlHandlerMapping和DefaultAnnotationHandlerMapping**。

#### HandlerAdapter检查

套路和上面完全一样，默认使用HttpRequestHandlerAdapter、SimpleControllerHandlerAdapter和AnnotationMethodHandlerAdapter。

#### HandlerExceptionResolver检查

套路和上面完全一样，默认使用AnnotationMethodHandlerExceptionResolver、ResponseStatusExceptionResolver、DefaultHandlerExceptionResolver。

#### RequestToViewNameTranslator

initRequestToViewNameTranslator方法回向容器中注册一个DefaultRequestToViewNameTranslator对象，此接口用以完成从HttpServletRequest到视图名的解析，其使用场景是**给定的URL无法匹配任何控制器时**。

DefaultRequestToViewNameTranslator的转换例子:

http://localhost:8080/gamecast/display.html -> display(视图)

![RequestToViewNameTranslator类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/RequestToViewNameTranslator.jpg)

#### ViewResolver检查

熟悉的套路，默认使用InternalResourceViewResolver。可以按名称解析视图的对象实现的接口

#### FlashMapManager

initFlashMapManager方法会向容器注册SessionFlashMapManager对象.用于检索和保存 FlashMap 实例的策略接口.

![FlashMapManager类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/FlashMapManager.jpg)

此接口和FlashMap搭配使用，用于在**请求重定向时保存/传递参数**。

### HandlerMapping初始化

#### RequestMappingHandlerMapping

此实现根据@Controller和@RequestMapping注解完成解析

![RequestMappingHandlerMapping类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/RequestMappingHandlerMapping.jpg)

初始化的入口位于AbstractHandlerMethodMapping的afterPropertiesSet方法和AbstractHandlerMapping的initApplicationContext方法，afterPropertiesSet调用了initHandlerMethods.

  ApplicationContextAwareProcessor.postProcessBeforeInitialization方法调用invokeAwareInterfaces方法,当bean类型为ApplicationContextAware时调用 ApplicationObjectSupport的ApplicationContextAware方法,进一步调用AbstractHandlerMapping的initApplicationContext方法.

```java
//AbstractHandlerMethodMapping 作为一个继承了InitializingBean的bean在initializeBean初始化时会调用afterPropertiesSet方法
protected void initHandlerMethods() {
    //获取容器中所有的bean
    String[] beanNames = (this.detectHandlerMethodsInAncestorContexts ?
            BeanFactoryUtils.beanNamesForTypeIncludingAncestors(getApplicationContext(), Object.class)             :getApplicationContext().getBeanNamesForType(Object.class));
    for (String beanName : beanNames) {
        if (!beanName.startsWith(SCOPED_TARGET_NAME_PREFIX)) {
            Class<?> beanType = null;
            beanType = getApplicationContext().getType(beanName);
             //isHandler方法的原理:
             //判断类上有没有@Controller注解或者是@RequestMapping注解
            if (beanType != null && isHandler(beanType)) {
                //将方法注册到MappingRegistry中
                detectHandlerMethods(beanName);
            }
        }
    }
    //空实现
    handlerMethodsInitialized(getHandlerMethods());
}
```

detectHandlerMethods方法将反射遍历类中所有的public方法，如果方法上含有@RequestMapping注解，那么将方法上的路径与类上的基础路径(如果有)进行合并，之后将映射(匹配关系)注册到MappingRegistry中。

**类上的@RequestMapping注解只能作为基路径存在，也就是说，如果类里面没有任何的方法级@RequestMapping注解，那么类上的注解是没有意义的**。

内部类AbstractHandlerMethodMapping.MappingRegistry是映射的载体

![MappingRegistry类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/MappingRegistry.jpg)

其register方法简略版源码:

```java
//mapping 示例 POST /api/v1/ancient/save是一个RequestMappingInfo对象
public void register(T mapping, Object handler, Method method) {
    //包装bean和方法
    HandlerMethod handlerMethod = createHandlerMethod(handler, method);
    this.mappingLookup.put(mapping, handlerMethod);
    List<String> directUrls = getDirectUrls(mapping);
    for (String url : directUrls) {
        //urlLookup其实就是将paths属性中的每个path都与处理器做映射。
        this.urlLookup.add(url, mapping);
    }
    String name = null;
    if (getNamingStrategy() != null) {
        name = getNamingStrategy().getName(handlerMethod, mapping);
        addMappingName(name, handlerMethod);
    }
    //@CrossOrigin 处理跨域请求,所有注解中的属性都会被封装在CorsConfiguration中
    CorsConfiguration corsConfig = initCorsConfiguration(handler, method, mapping);
    if (corsConfig != null) {
        this.corsLookup.put(handlerMethod, corsConfig);
    }
    this.registry.put(mapping, new MappingRegistration<T>(mapping, handlerMethod, directUrls, name));
}
```

getNamingStrategy得到一个mapping的简单名称比如对于我们的控制器,SimpleController.echo方法，最终得到的名字将是SC#echo。

#### 拦截器初始化

`AbstractHandlerMapping.initApplicationContext`

```java
@Override
protected void initApplicationContext() throws BeansException {
	//默认空实现
   extendInterceptors(this.interceptors);
    //检测MappedInterceptor类型的 bean 并将它们添加到映射拦截器列表中。
   detectMappedInterceptors(this.adaptedInterceptors);
   initInterceptors();
}
```

//初始化拦截器列表

```java
private final List<HandlerInterceptor> adaptedInterceptors = new ArrayList<HandlerInterceptor>();

protected void initInterceptors() {
   if (!this.interceptors.isEmpty()) {
      for (int i = 0; i < this.interceptors.size(); i++) {
         Object interceptor = this.interceptors.get(i);
          //类型转化后添加到缓存中
         this.adaptedInterceptors.add(adaptInterceptor(interceptor));
      }
   }
}
```

### HandlerAdapter初始化

以RequestMappingHandlerAdapter为例

![RequestMappingHandlerAdapter类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/RequestMappingHandlerAdapter.jpg)



口在afterPropertiesSet方法:

```java
//入口
public void afterPropertiesSet() {
    // Do this first, it may add ResponseBody advice beans
    //初始化@ControllerAdvice注解的类
    initControllerAdviceCache();
    if (this.argumentResolvers == null) {
        //参数解析,HandlerMethodArgumentResolver为参数解析器负责从request中解析、得到Controller方法所需的参数
        List<HandlerMethodArgumentResolver> resolvers = getDefaultArgumentResolvers();
        this.argumentResolvers = new HandlerMethodArgumentResolverComposite()
            .addResolvers(resolvers);
    }
    if (this.initBinderArgumentResolvers == null) {
        //@InitBinder支持,自定义类型转换器,initBinder 注解方法首先执行，然后执行requestMapping注解方法,只对当前controller有效,也可以配置扫描路径
        List<HandlerMethodArgumentResolver> resolvers = getDefaultInitBinderArgumentResolvers();
        this.initBinderArgumentResolvers = new HandlerMethodArgumentResolverComposite()
            .addResolvers(resolvers);
    }
    if (this.returnValueHandlers == null) {
        //返回结果解析,用以处理方法调用(Controller方法)的返回值
        List<HandlerMethodReturnValueHandler> handlers = getDefaultReturnValueHandlers();
        this.returnValueHandlers = new HandlerMethodReturnValueHandlerComposite()
            .addHandlers(handlers);
    }
}
```

## 请求响应

FrameworkServlet覆盖了service方法:

```java
protected void service(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException {

   HttpMethod httpMethod = HttpMethod.resolve(request.getMethod());
   if (httpMethod == HttpMethod.PATCH || httpMethod == null) {
      processRequest(request, response);
   }
   else {
      super.service(request, response);
   }
}
```

[PATCH和PUT方法的区别？](https://segmentfault.com/q/1010000005685904)

FrameworkServlet同样也覆盖了doGet和doPost方法，两者只是调用processRequest方法。

### 请求上下文

Spring MVC会在请求分发之前进行上下文的准备工作，含两部分:

1. 将地区(Locale)和请求属性以ThreadLocal的方法与当前线程进行关联，分别可以通过LocaleContextHolder和RequestContextHolder进行获取。
2. 将WebApplicationContext、FlashMap等组件放入到Request属性中。

### 请求分发

DispatcherServlet.doDispatch简略版源码:

```java
//DispatcherServlet 核心代码
protected void doDispatch(HttpServletRequest request, HttpServletResponse response) {
    //获取处理器,确定当前的controller
    HandlerExecutionChain mappedHandler = getHandler(processedRequest);
    //适配器查找,第一个适配器是RequestMappingHandlerAdapter，而其support方法直接返回true，这就导致了使用的适配器总是这一个。
    HandlerAdapter ha = getHandlerAdapter(mappedHandler.getHandler());
    mv = ha.handle(processedRequest, response, mappedHandler.getHandler());
    applyDefaultViewName(processedRequest, mv);
    mappedHandler.applyPostHandle(processedRequest, response, mv);
    processDispatchResult(processedRequest, response, mappedHandler, mv, dispatchException);
}
```

#### 处理器查找

即为请求寻找合适的Controller的过程。DispatcherServlet.getHandler:

```java
protected HandlerExecutionChain getHandler(HttpServletRequest request) {
    //对handlerMappings遍历,找不到则返回空,HandlerMapping有优先级概念,RequestMappingHandlerMapping默认为最高优先级
    for (HandlerMapping hm : this.handlerMappings) {
        HandlerExecutionChain handler = hm.getHandler(request);
        if (handler != null) {
            return handler;
        }
    }
    return null;
}
```

AbstractHandlerMapping.getHandler

```java
public final HandlerExecutionChain getHandler(HttpServletRequest request) throws Exception {
    //从mappingRegistry中获取url匹配的处理器
   Object handler = getHandlerInternal(request);
   if (handler == null) {
      handler = getDefaultHandler();
   }

   // 如果未beanName则需要获取具体bean实例
   if (handler instanceof String) {
      String handlerName = (String) handler;
      handler = getApplicationContext().getBean(handlerName);
   }
	//adaptedInterceptors中获得所有可以适配当前请求URL的MappedInterceptor并将其添加到HandlerExecutionChain的拦截器列表中
   HandlerExecutionChain executionChain = getHandlerExecutionChain(handler, request);
    //跨域处理
   if (CorsUtils.isCorsRequest(request)) {
      CorsConfiguration globalConfig = this.globalCorsConfigSource.getCorsConfiguration(request);
      CorsConfiguration handlerConfig = getCorsConfiguration(handler, request);
      CorsConfiguration config = (globalConfig != null ? globalConfig.combine(handlerConfig) : handlerConfig);
      executionChain = getCorsHandlerExecutionChain(request, executionChain, config);
   }
   return executionChain;
}
```

### 请求处理

`RequestMappingHandlerAdapter.handleInternal`

```java
protected ModelAndView handleInternal(HttpServletRequest request,
      HttpServletResponse response, HandlerMethod handlerMethod) throws Exception {

   ModelAndView mav;
   checkRequest(request);

   // 如果开启了synchronizeOnSession，那么同一个session的请求将会串行执行,默认关闭
   if (this.synchronizeOnSession) {
      HttpSession session = request.getSession(false);
      if (session != null) {
         Object mutex = WebUtils.getSessionMutex(session);
         synchronized (mutex) {
            mav = invokeHandlerMethod(request, response, handlerMethod);
         }
      }
      else {
         // 没有HttpSession 则不需要互斥锁
         mav = invokeHandlerMethod(request, response, handlerMethod);
      }
   }
   else {
      //不需要同步session
      mav = invokeHandlerMethod(request, response, handlerMethod);
   }

   if (!response.containsHeader(HEADER_CACHE_CONTROL)) {
      if (getSessionAttributesHandler(handlerMethod).hasSessionAttributes()) {
          //缓存http头
         applyCacheSeconds(response, this.cacheSecondsForSessionAttributeHandlers);
      }
      else {
          //根据此生成器的设置准备给定的响应。应用为此生成器指定的缓存秒数。
         prepareResponse(response);
      }
   }

   return mav;
}
```

核心调用方法 `InvocableHandlerMethod#invokeForRequest`

```java
public Object invokeForRequest(NativeWebRequest request, ModelAndViewContainer mavContainer,
      Object... providedArgs) throws Exception {
	//参数解析
   Object[] args = getMethodArgumentValues(request, mavContainer, providedArgs);
   //执行invoke方法,getBridgedMethod().invoke(getBean(), args);	
   Object returnValue = doInvoke(args);

   return returnValue;
}
```

#### 参数解析

##### 策略模式

HandlerAdapter内部含有一组解析器负责对各类型的参数进行解析

##### 自定义参数

解析由RequestParamMethodArgumentResolver完成。supportsParameter方法决定了一个解析器可以解析的参数类型，该解析器支持@RequestParam标准的参数或是**简单类型**的参数

忽略复杂的调用关系，最核心的实现位于resolveName方法

```java
//就是根据参数名去request查找对应属性的过程
protected Object resolveName(String name, MethodParameter parameter, NativeWebRequest request) {
    if (arg == null) {
        String[] paramValues = request.getParameterValues(name);
        if (paramValues != null) {
            arg = (paramValues.length == 1 ? paramValues[0] : paramValues);
        }
    }
    return arg;
}
```

###### 参数名是从哪里来的

RequestParamMethodArgumentResolver的resolveArgument方法,中参数名的获取由接口ParameterNameDiscoverer完成.

![ParameterNameDiscoverer](https://github.com/seaswalker/spring-analysis/raw/master/note/images/ParameterNameDiscoverer.jpg)

默认采用DefaultParameterNameDiscoverer，但此类其实相当于StandardReflectionParameterNameDiscoverer和LocalVariableTableParameterNameDiscoverer的组合，且前者先于后者进行解析

StandardReflectionParameterNameDiscoverer类被注解UsesJava8标注，其原理就是利用的jdk8的-parameters编译参数，只有在加上此选项的情况下才能用反射的方法获得真实的参数名，所以一般情况下StandardReflectionParameterNameDiscoverer是无法成功获取参数名的。

LocalVariableTableParameterNameDiscoverer利用了ASM直接访问class文件中的本地变量表来得到变量名.

##### Model

解析由ModelMethodProcessor完成。

supportsParameter方法很简单:

```java
public boolean supportsParameter(MethodParameter parameter) {
    return Model.class.isAssignableFrom(parameter.getParameterType());
}
```

resolveArgument：

```java
public Object resolveArgument(MethodParameter parameter, ModelAndViewContainer mavContainer,
    NativeWebRequest webRequest, WebDataBinderFactory binderFactory) throws Exception {
    return mavContainer.getModel();
}
```

**Model其实是一个BindingAwareModelMap对象，且每次请求(需要注入Model的前提下)都有一个新的该对象生成**

![BindingAwareModelMap类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/BindingAwareModelMap.jpg)

##### 总结

- 我们可以通过实现HandlerMethodArgumentResolver接口并将其注册容器的方式实现自定义参数类型的解析。
- 为了防止出现参数名获取不到的问题，应优先使用@RequestParam注解直接声明需要的参数名称。

#### 返回值解析

套路和上面是一样的，通常情况，我们返回的其实是view名，负责处理的是ViewNameMethodReturnValueHandler，

supportsReturnType方法:

```java
@Override
public boolean supportsReturnType(MethodParameter returnType) {
    Class<?> paramType = returnType.getParameterType();
    return (void.class == paramType || CharSequence.class.isAssignableFrom(paramType));
}
```

handleReturnValue 并没有进行实际的处理，只是解析得到了最终的视图名称。

#### 视图渲染

由DispatcherServlet的processDispatchResult方法完成

```java
private void processDispatchResult(HttpServletRequest request, HttpServletResponse response,
      HandlerExecutionChain mappedHandler, ModelAndView mv, Exception exception) throws Exception {

   boolean errorView = false;

   if (exception != null) {
       //有异常会走到这个分支
      if (exception instanceof ModelAndViewDefiningException) {
         mv = ((ModelAndViewDefiningException) exception).getModelAndView();
      }
      else {
         Object handler = (mappedHandler != null ? mappedHandler.getHandler() : null);	
         //遍历所有的HandlerExceptionResolver实例,调用resolveException方法处理,当第一个返回不为空时中断,没有默认的会直接跑到tomcat容器
         mv = processHandlerException(request, response, handler, exception);
         errorView = (mv != null);
      }
   }

   // Did the handler return a view to render?
   if (mv != null && !mv.wasCleared()) {
       //渲染给定的 ModelAndView。
      render(mv, request, response);
      if (errorView) {
         WebUtils.clearErrorRequestAttributes(request);
      }
   }
 
}
```

##### ModelAndView

![ModelAndView类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/ModelAndView.jpg)

调用`RequestMappingHandlerAdapter.getModelAndView`方法生成

```java
ModelMap model = mavContainer.getModel();
ModelAndView mav = new ModelAndView(mavContainer.getViewName(), model, mavContainer.getStatus());
```

##### 渲染

DispatcherServlet.render简略版源码

```java
protected void render(ModelAndView mv, HttpServletRequest request, HttpServletResponse response) {
    Locale locale = this.localeResolver.resolveLocale(request);
    response.setLocale(locale);
    View view;
    //判断依据: 是否是String类型
    if (mv.isReference()) {
        // We need to resolve the view name.
        //遍历所有的ViewResolver bean，只要有一个解析的结果(View)不为空，即停止遍历
        view = resolveViewName(mv.getViewName(), mv.getModelInternal(), locale, request);
    } else {
        // No need to lookup: the ModelAndView object contains the actual View object.
        view = mv.getView();
    }
    if (mv.getStatus() != null) {
        response.setStatus(mv.getStatus().value());
    }
    view.render(mv.getModelInternal(), request, response);
}
```

渲染的核心逻辑位于`InternalResourceView.renderMergedOutputModel`

```java
protected void renderMergedOutputModel(
        Map<String, Object> model, HttpServletRequest request, HttpServletResponse response) {
    // 将Model中的属性设置的request中
    exposeModelAsRequestAttributes(model, request);
    // 获取资源(jsp)路径
    String dispatcherPath = prepareForRendering(request, response);
    // Obtain a RequestDispatcher for the target resource (typically a JSP).
    RequestDispatcher rd = getRequestDispatcher(request, dispatcherPath);
    // If already included or response already committed, perform include, else forward.
    if (useInclude(request, response)) {
        response.setContentType(getContentType());
        rd.include(request, response);
    } else {
        // Note: The forwarded resource is supposed to determine the content type itself.
        rd.forward(request, response);
    }
}
```

可以看出，对jsp来说，所谓的渲染其实就是**将Model中的属性设置到Request，再利用原生Servlet RequestDispatcher API进行转发的过程**。

### @ResponseBody

@ResponseBody注解表示需要将对象转为JSON并返回给前端.Spring MVC采用`AnnotationDrivenBeanDefinitionParser`进行配置的解析，核心的parse方法中完成了对HttpMessageConverter的初始化。

### HttpMessageConverter

Spring的HttpMessageConverter接口负责HTTP请求-Java对象与Java对象-响应之间的转换.springmvc默认使用jackson转化器.

![HttpMessageConverter](https://github.com/seaswalker/spring-analysis/raw/master/note/images/HttpMessageConverter.jpg)

#### 转换

入口位于ServletInvocableHandlerMethod的invokeAndHandle方法对于响应的处理:调用`handleReturnValue`方法时.会使用`RequestResponseBodyMethodProcessor#writeWithMessageConverters`方法利用messageConverters处理返回消息. 而`RequestResponseBodyMethodProcessor`的初始化是在`RequestMappingHandlerAdapter#afterPropertiesSet`方法中.

```java
handlers.add(new RequestResponseBodyMethodProcessor(getMessageConverters(),
				this.contentNegotiationManager, this.requestResponseBodyAdvice));
this.returnValueHandlers = new HandlerMethodReturnValueHandlerComposite().addHandlers(handlers);
```

RequestResponseBodyMethodProcessor通过supportsReturnType方法判断是否能够转换

```java
public boolean supportsReturnType(MethodParameter returnType) {
   return (AnnotatedElementUtils.hasAnnotation(returnType.getContainingClass(), ResponseBody.class) ||
         returnType.hasMethodAnnotation(ResponseBody.class));
}
```

实际在jackson中的实现为

```java
public boolean canWrite(Class<?> clazz, MediaType mediaType) {
   if (!canWrite(mediaType)) {
      return false;
   }
   AtomicReference<Throwable> causeRef = new AtomicReference<Throwable>();
   // 调用objectMapper序列化
   if (this.objectMapper.canSerialize(clazz, causeRef)) {
      return true;
   }
   logWarningIfNecessary(clazz, causeRef.get());
   return false;
}
```

MappingJacksonHttpMessageConverter中的objectMapper被所有的线程所共享，因为其是线程安全的

### 参数解析 & 结果转换

Spring MVC中参数到各种类型的转换由HandlerMethodArgumentResolver接口完成，而Controller返回值到真正的响应的转换由HandlerMethodReturnValueHandler接口完成.

HandlerMethodArgumentResolver主要实现类

![HandlerMethodArgumentResolver](https://github.com/seaswalker/spring-analysis/raw/master/note/images/HandlerMethodArgumentResolver_all.jpg)

HandlerMethodReturnValueHandler接口主要实现类

![HandlerMethodReturnValueHandler](https://github.com/seaswalker/spring-analysis/raw/master/note/images/HandlerMethodReturnValueHandler_all.jpg)

对于HandlerMethodArgumentResolver和HandlerMethodReturnValueHandler来说，HttpMessageConverter像是两者手中用来实现功能的武器。

### "纯"对象参数接收

示例

```java
@RequestMapping("/echoAgain")
public String echo(SimpleModel simpleModel, Model model) {
    model.addAttribute("echo", "hello " + simpleModel.getName() + ", your age is " + simpleModel.getAge() + ".");
    return "echo";
}
```

get,post请求都能被ServletModelAttributeMethodProcessor解析后获取

![ServletModelAttributeMethodProcessor](https://github.com/seaswalker/spring-analysis/raw/master/note/images/ServletModelAttributeMethodProcessor.jpg)

核心的supportsParameter方法由父类ModelAttributeMethodProcessor实现:

```java
public boolean supportsParameter(MethodParameter parameter) {
    //支持带有ModelAttribute注解或者是非基本类型的参数解析，同时annotationNotRequired必须设为false，即ModelAttribute注解不必存在，这里是在ServletModelAttributeMethodProcessor的构造器中进行控制的
    return (parameter.hasParameterAnnotation(ModelAttribute.class) ||
        (this.annotationNotRequired && !BeanUtils.isSimpleProperty(parameter.getParameterType())));
}
```

改写示例后解析过程

```java
public String echo(@ModelAttribute SimpleModel simpleModel, Model model) {
    model.addAttribute("echo", "hello " + simpleModel.getName() + ", your age is " + simpleModel.getAge() + ".");
    System.out.println(model.asMap().get("simpleModel"));
    return "echo";
}
```

#### 参数对象构造

对象创建入口在ModelAttributeMethodProcessor类中的resolveArgument方法

```java
//name为示例中的simpleModel
String name = ModelFactory.getNameForParameter(parameter);
ModelAttribute ann = parameter.getParameterAnnotation(ModelAttribute.class);
if (ann != null) {
   mavContainer.setBinding(name, ann.binding());
}

//ModelAndViewContainer是model与view的组合体
Object attribute = (mavContainer.containsAttribute(name) ? mavContainer.getModel().get(name) :
      createAttribute(name, parameter, binderFactory, webRequest));
```

#### 参数绑定

利用DataBinder接口,向执行对象中设置属性值.

![DataBinder](https://github.com/seaswalker/spring-analysis/raw/master/note/images/DataBinder.jpg)

WebDataBinderFactory接口用以创建WebDataBinder对象，其继承体系如下图:

![WebDataBinderFactory](https://github.com/seaswalker/spring-analysis/raw/master/note/images/WebDataBinderFactory.jpg)

默认使用的是ServletRequestDataBinderFactory

```java
protected ServletRequestDataBinder createBinderInstance(Object target, String objectName, NativeWebRequest request) {
   return new ExtendedServletRequestDataBinder(target, objectName);
}
```

参数绑定的核心代码在ServletRequestDataBinder的bind方法完成.

```java
public void bind(PropertyValues pvs) {
   MutablePropertyValues mpvs = (pvs instanceof MutablePropertyValues) ?
         (MutablePropertyValues) pvs : new MutablePropertyValues(pvs);
   //实际操作为检查后遍历属性并赋值      
   doBind(mpvs);
}
```

#### 参数校验

如果在参数前使用注解@Validated或者@Valid,当参数校验绑定后，Spring MVC会尝试对参数进行校验.

ModelAttributeMethodProcessor.resolveArgument方法相关源码:

```java
if (binder.getTarget() != null) {
   if (!mavContainer.isBindingDisabled(name)) {
      bindRequestParameters(binder, webRequest);
   }
    //校验参数
   validateIfApplicable(binder, parameter);
   if (binder.getBindingResult().hasErrors() && isBindExceptionRequired(binder, parameter)) {
      throw new BindException(binder.getBindingResult());
   }
}


protected void validateIfApplicable(WebDataBinder binder, MethodParameter parameter) {
		Annotation[] annotations = parameter.getParameterAnnotations();
		for (Annotation ann : annotations) {
			Validated validatedAnn = AnnotationUtils.getAnnotation(ann, Validated.class);
			if (validatedAnn != null || ann.annotationType().getSimpleName().startsWith("Valid")) {
				Object hints = (validatedAnn != null ? validatedAnn.value() : AnnotationUtils.getValue(ann));
				Object[] validationHints = (hints instanceof Object[] ? (Object[]) hints : new Object[] {hints});
				binder.validate(validationHints);
				break;
			}
		}
	}
```

DataBinder.validate:

```java
public void validate(Object... validationHints) {
    for (Validator validator : getValidators()) {
        if (!ObjectUtils.isEmpty(validationHints) && validator instanceof SmartValidator) {
            ((SmartValidator) validator).validate(getTarget(), getBindingResult(), validationHints);
        } else if (validator != null) {
            validator.validate(getTarget(), getBindingResult());
        }
    }
}
```

具体校验交给了Validator执行,相关类图

![Validator](https://github.com/seaswalker/spring-analysis/raw/master/note/images/Validator.png)

根据这里的校验器的来源可以分为**JSR校验**以及**自定义**

- JSR校验

需要引入hibernate-validator到classpath中,利用AnnotationDrivenBeanDefinitionParser进行解析初始化及校验支持.

-  自定义校验器

通过继承Validator接口实现.