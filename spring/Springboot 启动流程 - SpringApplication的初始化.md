# Springboot 启动流程 - SpringApplication的初始化

springboot启动类代码

```java
@SpringBootApplication
public class DemoApplication {
    public static void main(String[] args) {
        SpringApplication.run(DemoApplication.class, args);
    }
}
```

main方法启动主线程调用 `SpringApplication.run(DemoApplication.class, args)`方法启动springboot.

```java
//run方法实际调用
public static ConfigurableApplicationContext run(Class<?>[] primarySources, String[] args) {
    //第一步初始化 SpringApplication,第二步调用 run(String... args)方法
   return new SpringApplication(primarySources).run(args);
}
```

首先执行初始化SpringApplication操作

```java
//默认resourceLoader传空,primarySources为启动类的类型 
public SpringApplication(ResourceLoader resourceLoader, Class<?>... primarySources) {
   this.resourceLoader = resourceLoader;
   Assert.notNull(primarySources, "PrimarySources must not be null");
   this.primarySources = new LinkedHashSet<>(Arrays.asList(primarySources));
   //判断容器启动类型是 
   this.webApplicationType = WebApplicationType.deduceFromClasspath();
    //初始化initializers属性内容为,所有类型为ApplicationContextInitializer且在配置文件中指定的实例
   setInitializers((Collection) getSpringFactoriesInstances(ApplicationContextInitializer.class));
    //同上一步,类型为ApplicationListener
   setListeners((Collection) getSpringFactoriesInstances(ApplicationListener.class));
    //根据栈信息,获取主类名称,然后反射获取Class
   this.mainApplicationClass = deduceMainApplicationClass();
}
```
通过读取`spring.factories`,然后根据type,classLoader和文件中的指定名称集合,利用反射初始化相关实例,排序后返回.

```
private <T> Collection<T> getSpringFactoriesInstances(Class<T> type, Class<?>[] parameterTypes, Object... args) {
   ClassLoader classLoader = getClassLoader();
   // Use names and ensure unique to protect against duplicates
   Set<String> names = new LinkedHashSet<>(SpringFactoriesLoader.loadFactoryNames(type, classLoader));
   List<T> instances = createSpringFactoriesInstances(type, parameterTypes, classLoader, args, names);
   AnnotationAwareOrderComparator.sort(instances);
   return instances;
}
```

`SpringApplication#getSpringFactoriesInstances`中会调用`SpringFactoriesLoader#loadSpringFactories`方法读取`spring.factories`文件初始化类.

```java
private static Map<String, List<String>> loadSpringFactories(@Nullable ClassLoader classLoader) {
		MultiValueMap<String, String> result = cache.get(classLoader);
		if (result != null) {
			return result;
		}

		try {
			Enumeration<URL> urls = (classLoader != null ?
					classLoader.getResources(FACTORIES_RESOURCE_LOCATION) :
					ClassLoader.getSystemResources(FACTORIES_RESOURCE_LOCATION));
			result = new LinkedMultiValueMap<>();
			while (urls.hasMoreElements()) {
				URL url = urls.nextElement();
				UrlResource resource = new UrlResource(url);
                //最终读取 file:/E:/Repository/org/springframework/boot/spring-boot/2.3.7.RELEASE/spring-boot-2.3.7.RELEASE.jar!/META-INF/spring.factories文件
				Properties properties = PropertiesLoaderUtils.loadProperties(resource);
				for (Map.Entry<?, ?> entry : properties.entrySet()) {
					String factoryTypeName = ((String) entry.getKey()).trim();
					for (String factoryImplementationName : StringUtils.commaDelimitedListToStringArray((String) entry.getValue())) {
						result.add(factoryTypeName, factoryImplementationName.trim());
					}
				}
			}
			cache.put(classLoader, result);
			return result;
		}
		catch (IOException ex) {
			throw new IllegalArgumentException("Unable to load factories from location [" +
					FACTORIES_RESOURCE_LOCATION + "]", ex);
		}
	}
```