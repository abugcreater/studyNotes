# Spring Cloud 组件 OpenFeign	

## 理解远程调用

那远程调用就是不同 Service 之间的方法调用。Service 级的方法调用，就是我们自己构造请求 URL和请求参数，就可以发起远程调用了。一般用到的框架,OKHttp3，Netty, HttpURLConnection 等

![image-20220117152851938](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117152851938.png)

## Feign 和 OpenFeign

**OpenFeign 为 Feign 的增强版,支持spring mvc注解**

- OpenFeign 是声明式的 HTTP 客户端，让远程调用更简单。
- 提供了HTTP请求的模板，编写简单的接口和插入注解，就可以定义好HTTP请求的参数、格式、地址等信息
- 整合了Ribbon（负载均衡组件）和 Hystix（服务熔断组件），不需要显示使用这两个组件
- Spring Cloud Feign 在 Netflix Feign的基础上扩展了对SpringMVC注解的支持

## OpenFeign 如何用

![image-20220117153113244](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117153113244.png)

```java
//代码实现
@FeignClient(name = "${feignConfig.userServiceName}", url = "${feignConfig.userServiceUrl}", configuration = FeignHttpsConfiguration.class,  fallbackFactory = UserFeignFallbackFactory.class)
public interface BaseUserFeignService {


    @GetMapping(value = "/api/v1/schools/0/classes/{classId}/teachers/", consumes = MediaType.APPLICATION_JSON_VALUE)
    String getSchoolClassTeacherInfo(@PathVariable("classId") Integer classId);
    
}
//需要在启动类上添加 @EnableFeignClients 注解

```

## 梳理 OpenFeign 的核心流程

![image-20220117153450882](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117153450882.png)

首先扫包,获取所有` @FeignClient `注解的接口,统一被spring管理,其次MVC解析方法上的路径和参数,生成`MethodMetadata `并缓存,然后spring生成动态代理对象,指向了一个包含对应方法的 MethodHandler 的 HashMap。

当服务需要调用fegin时,获取动态代理对象,然后从hashMap中获取MethodHandler ,包装生成request.经过负债均衡想对应服务发起请求.

## OpeFeign 包扫描原理

从`@EnableFeignClients`注解发现包扫描原理.

![image-20220117154103426](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117154103426.png)



```Java
//核心代码 FeignClientsRegistrar#registerBeanDefinitions
public void registerBeanDefinitions(AnnotationMetadata metadata,
			BeanDefinitionRegistry registry) {
    	//注册配置
		registerDefaultConfiguration(metadata, registry);
    	//遍历扫描包,获取所有@FeignClient注解的接口
		registerFeignClients(metadata, registry);
	}

public void registerFeignClients(AnnotationMetadata metadata,
			BeanDefinitionRegistry registry) {
		ClassPathScanningCandidateComponentProvider scanner = getScanner();
		scanner.setResourceLoader(this.resourceLoader);

		Set<String> basePackages;

		Map<String, Object> attrs = metadata
				.getAnnotationAttributes(EnableFeignClients.class.getName());
		AnnotationTypeFilter annotationTypeFilter = new AnnotationTypeFilter(
				FeignClient.class);
		final Class<?>[] clients = attrs == null ? null
				: (Class<?>[]) attrs.get("clients");
		if (clients == null || clients.length == 0) {
			scanner.addIncludeFilter(annotationTypeFilter);
			basePackages = getBasePackages(metadata);
		}
		else {
			final Set<String> clientClasses = new HashSet<>();
			basePackages = new HashSet<>();
			for (Class<?> clazz : clients) {
				basePackages.add(ClassUtils.getPackageName(clazz));
				clientClasses.add(clazz.getCanonicalName());
			}
			AbstractClassTestingTypeFilter filter = new AbstractClassTestingTypeFilter() {
				@Override
				protected boolean match(ClassMetadata metadata) {
					String cleaned = metadata.getClassName().replaceAll("\\$", ".");
					return clientClasses.contains(cleaned);
				}
			};
			scanner.addIncludeFilter(
					new AllTypeFilter(Arrays.asList(filter, annotationTypeFilter)));
		}

		for (String basePackage : basePackages) {
            //调用 find 方法来查找指定路径 basePackage 的所有带有 @FeignClients 注解的带有 @FeignClient 注解的类、接口。
			Set<BeanDefinition> candidateComponents = scanner
					.findCandidateComponents(basePackage);
			for (BeanDefinition candidateComponent : candidateComponents) {
                //只保留带有 @FeignClient 的接口。
				if (candidateComponent instanceof AnnotatedBeanDefinition) {
					// verify annotated class is an interface
					AnnotatedBeanDefinition beanDefinition = (AnnotatedBeanDefinition) candidateComponent;
					AnnotationMetadata annotationMetadata = beanDefinition.getMetadata();
					Assert.isTrue(annotationMetadata.isInterface(),
							"@FeignClient can only be specified on an interface");

					Map<String, Object> attributes = annotationMetadata
							.getAnnotationAttributes(
									FeignClient.class.getCanonicalName());

					String name = getClientName(attributes);
					registerClientConfiguration(registry, name,
							attributes.get("configuration"));

					registerFeignClient(registry, annotationMetadata, attributes);
				}
			}
		}
	}

```

## 注册 FeignClient 到 Spring 的原理

// 核心代码 FeignClientsRegistrar#registerFeignClient

```java
private void registerFeignClient(BeanDefinitionRegistry registry,
      AnnotationMetadata annotationMetadata, Map<String, Object> attributes) {
   String className = annotationMetadata.getClassName();
   //构造 FeignClientFactoryBean
   BeanDefinitionBuilder definition = BeanDefinitionBuilder
				.genericBeanDefinition(FeignClientFactoryBean.class);
   ...
   //注册bean   
   BeanDefinitionHolder holder = new BeanDefinitionHolder(beanDefinition, className,
         new String[] { alias });
   BeanDefinitionReaderUtils.registerBeanDefinition(holder, registry);
}
```
OpenFeign 框架去创建 FeignClient Bean 的时候，就会使用这些参数去生成 Bean。流程图:

![image-20220117155506903](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117155506903.png)

- 解析 `@FeignClient` 定义的属性。
- 将注解`@FeignClient` 的属性 +  接口 信息构造成一个  beanDefinition。
- 然后将 beanDefinition 转换成一个 holder，这个 holder 就是包含了 beanDefinition, alias, beanName 信息。
- 最后将这个 holder 注册到 Spring 容器中。

## OpenFeign 动态代理原理

在创建 FeignClient Bean 的过程中就会去生成动态代理对象。调用接口时，其实就是调用动态代理对象的方法来发起请求的。

```java
//FeignClientBean#getTarget方法最后执行操作
Targeter targeter = get(context, Targeter.class);
//生成动态代理对象
return (T) targeter.target(this, builder, context,
      new HardCodedTarget<>(this.type, this.name, url));
```

target 会有两种实现类：DefaultTargeter 和 HystrixTargeter

target实现类最终调用Feign.java 的 builder 方法去构造一个 feign client。

```java
public <T> T target(Target<T> target) {
    return this.build().newInstance(target);
}

// 省略部分代码
public class ReflectiveFeign extends Feign {
  // 为 feign client 接口中的每个接口方法创建一个 methodHandler
 public <T> T newInstance(Target<T> target) {
    for(...) {
      methodToHandler.put(method, handler);
    }
    // 基于 JDK 动态代理的机制，创建了一个 passjava-study 接口的动态代理，所有对接口的调用都会被拦截，然后转交给 handler 的方法。
    InvocationHandler handler = factory.create(target, methodToHandler);
    T proxy = (T) Proxy.newProxyInstance(target.type().getClassLoader(),
          new Class<?>[] {target.type()}, handler);
}
```

创建动态代理的原理

![image-20220117160536973](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117160536973.png)



解析方法上的信息,将解析到的数据封装成元数,生成方法级别代理,将这些方法放到hashMap中.然后会生成一个 InvocationHandler 用来管理这个 hashMap，其中 Dispatch 指向这个 HashMap。使用JDK动态代理代理该方法.

## 解析 MVC 注解的原理

![image-20220117161639656](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117161639656.png)



而解析的类就是 `SpringMvcContract` 类，调用 `parseAndValidateMetadata` 进行解析。

```java
//FeignClientsConfiguration#feignContract 配置解析器
@Bean
@ConditionalOnMissingBean
public Contract feignContract(ConversionService feignConversionService) {
   return new SpringMvcContract(this.parameterProcessors, feignConversionService);
}

// ReflectiveFeign#newInstance 方法 第一行调用 apply方法
public Map<String, MethodHandler> apply(Target target) {
      //解析target参数
List<MethodMetadata> metadata = contract.parseAndValidateMetadata(target.type());
}


```

MethodMetadata 中包含

- 方法的定义，如 StudyTimeFeignService 的 getMemberStudyTimeList 方法。
- 方法的参数类型，如 Long。
- 发送 HTTP 请求的地址，如 /study/studytime/member/list/test/{id}。

## OpenFeign 发送请求的原理

![image-20220117163149930](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117163149930.png)

```java
//ReflectiveFeign#invoke 方法
public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
     //Map<Method, InvocationHandlerFactory.MethodHandler> dispatch 通过方法获取对应的动态代理类,调用代理方法
      return dispatch.get(method).invoke(args);
    }
 //SynchronousMethodHandler#invoke 方法
 public Object invoke(Object[] argv) throws Throwable {
     //创建RequestTemplate 调用方法
    RequestTemplate template = buildTemplateFromArgs.create(argv);
    Options options = findOptions(argv);
    Retryer retryer = this.retryer.clone();
    while (true) {
      try {
        return executeAndDecode(template, options);
      } catch (RetryableException e) {
        try {
          retryer.continueOrPropagate(e);
        } catch (RetryableException th) {
          Throwable cause = th.getCause();
          if (propagationPolicy == UNWRAP && cause != null) {
            throw cause;
          } else {
            throw th;
          }
        }
        if (logLevel != Logger.Level.NONE) {
          logger.logRetry(metadata.configKey(), logLevel);
        }
        continue;
      }
    }
  }
 Object executeAndDecode(RequestTemplate template, Options options) throws Throwable{
     ...
     //执行请求    
     response = client.execute(request, options);
     ...
 }

```

## OpenFeign 如何与 Ribbon 整合的原理

在上问中client.execute() 方法其实会调用 LoadBalancerFeignClient 的 exceute 方法。

执行流程如下

![image-20220117165051054](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220117165051054.png)

- 根据服务名从缓存中找 FeignLoadBalancer，如果缓存中没有，则创建一个 FeignLoadBalancer。

- FeignLoadBalancer 会创建出一个 command，这个 command 会执行一个 sumbit 方法。

- submit 方法里面就会用 Ribbon 的负载均衡算法选择一个 server

  ```java
  Observable<T> o = 
          (server == null ? selectServer() : Observable.just(server))
  ```

- 然后将 IP 地址和之前剔除掉服务名称的 URL 进行拼接，生成最后的服务地址。

- 最后 FeignLoadBalancer 执行 execute 方法发送请求(具体执行类是`RibbonLoadBalancingHttpClient`)

## OpenFeign 处理响应的原理

当远程服务 passjava-study 处理完业务逻辑后，就会返回 reponse 给 passjava-member 服务了，这里还会对 reponse 进行一次解码操作。

```java
//SynchronousMethodHandler#executeAndDecode 方法
if (decoder != null)
return decoder.decode(response, metadata.returnType());
```

这个里面做的事情就是调用 ResponseEntityDecoder 的 decode 方法，将 Json 字符串转化为 Bean 对象。

## 总结

**核心思想：**

- OpenFeign 会扫描带有 @FeignClient 注解的接口，然后为其生成一个动态代理。
- 动态代理里面包含有接口方法的 MethodHandler，MethodHandler 里面又包含经过 MVC Contract 解析注解后的元数据。
- 发起请求时，MethodHandler 会生成一个 Request。
- 负载均衡器 Ribbon 会从服务列表中选取一个 Server，拿到对应的 IP 地址后，拼接成最后的 URL，就可以发起远程服务调用了。