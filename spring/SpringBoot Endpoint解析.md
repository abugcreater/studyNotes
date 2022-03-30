# SpringBoot Endpoint解析

##  Endpoint解析入口

入口是在`WebMvcEndpointManagementContextConfiguration`类中.

```java
@Bean
@ConditionalOnMissingBean
public WebMvcEndpointHandlerMapping webEndpointServletHandlerMapping(
      WebEndpointsSupplier webEndpointsSupplier,
      ServletEndpointsSupplier servletEndpointsSupplier,
      ControllerEndpointsSupplier controllerEndpointsSupplier,
      EndpointMediaTypes endpointMediaTypes, CorsEndpointProperties corsProperties,
      WebEndpointProperties webEndpointProperties) {
    //加载所有 Endpoint 合并到allEndpoints集合中
   List<ExposableEndpoint<?>> allEndpoints = new ArrayList<>();
   Collection<ExposableWebEndpoint> webEndpoints = webEndpointsSupplier
         .getEndpoints();
   allEndpoints.addAll(webEndpoints);
   allEndpoints.addAll(servletEndpointsSupplier.getEndpoints());
   allEndpoints.addAll(controllerEndpointsSupplier.getEndpoints());
    //获取配置中的前缀路径
   EndpointMapping endpointMapping = new EndpointMapping(
         webEndpointProperties.getBasePath());
    //初始化WebMvcEndpointHandlerMapping
   return new WebMvcEndpointHandlerMapping(endpointMapping, webEndpoints,
         endpointMediaTypes, corsProperties.toCorsConfiguration(),
         new EndpointLinksResolver(allEndpoints,
               webEndpointProperties.getBasePath()));
}
```

WebMvcEndpointHandlerMapping 类图

![WebMvcEndpointHandlerMapping类图](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220323155735935.png)

可以看到WebMvcEndpointHandlerMapping 继承了 InitializingBean类,在bean初始化时会调用afterPropertiesSet方法.根据方法的调用链会执行, AbstractWebMvcEndpointHandlerMapping#initHandlerMethods方法.该方法的核心源码:

```java
protected void initHandlerMethods() {
    //遍历所有端点
   for (ExposableWebEndpoint endpoint : this.endpoints) {
      for (WebOperation operation : endpoint.getOperations()) {
         registerMappingForOperation(endpoint, operation);
      }
   }
    //获取端点配置的basePackage
   if (StringUtils.hasText(this.endpointMapping.getPath())) {
      registerLinksMapping();
   }
}
```

为端点动作注册到mvc中

```java
private void registerMappingForOperation(ExposableWebEndpoint endpoint,
      WebOperation operation) {
   OperationInvoker invoker = operation::invoke;
   ServletWebOperation servletWebOperation = wrapServletWebOperation(endpoint,
         operation, new ServletWebOperationAdapter(invoker));
    //创建一个 RequestMappingInfo注册到路径中
   registerMapping(createRequestMappingInfo(operation),
         new OperationHandler(servletWebOperation), this.handleMethod);
}
```

将base package已get请求的方式入住到mvc中

```java
private void registerLinksMapping() {
    //获取路径匹配逻辑, base package
   PatternsRequestCondition patterns = patternsRequestConditionForPattern("");
    RequestMethodsRequestCondition methods = new RequestMethodsRequestCondition(
         RequestMethod.GET);
   ProducesRequestCondition produces = new ProducesRequestCondition(
         this.endpointMediaTypes.getProduced().toArray(StringUtils
               .toStringArray(this.endpointMediaTypes.getProduced())));
   RequestMappingInfo mapping = new RequestMappingInfo(patterns, methods, null, null,
         null, produces, null);
   registerMapping(mapping, this, this.linksMethod);
}
```

处理逻辑与mvc相同,经过DispatcherServlet转发到对应实现,完成处理.

## 端点的定义

以spring boot自带的health端点为例.

首先定义一个配置类,当满足条件时初始化 health bean

```java
@Configuration
class HealthEndpointConfiguration {

    //上下文容器中没有该bean且开启注解时自动注册
   @Bean
   @ConditionalOnMissingBean
   @ConditionalOnEnabledEndpoint
   public HealthEndpoint healthEndpoint(ApplicationContext applicationContext) {
      return new HealthEndpoint(HealthIndicatorBeansComposite.get(applicationContext));
   }

}
```

然后定义端点实体

```java
@Endpoint(id = "health")
public class HealthEndpoint {

   private final HealthIndicator healthIndicator;

   /**
    * Create a new {@link HealthEndpoint} instance.
    * @param healthIndicator the health indicator
    */
   public HealthEndpoint(HealthIndicator healthIndicator) {
      Assert.notNull(healthIndicator, "HealthIndicator must not be null");
      this.healthIndicator = healthIndicator;
   }

    //ReadOperation 为get请求,被注册到mvc中,方法为实际实现
   @ReadOperation 
   public Health health() {
      return this.healthIndicator.health();
   }

}
```

