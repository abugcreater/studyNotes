# spring context

## annotation-config

AnnotationConfigBeanDefinitionParser.parse:

```java
@Override
public BeanDefinition parse(Element element, ParserContext parserContext) {
    //返回null
    Object source = parserContext.extractSource(element);
    // Obtain bean definitions for all relevant BeanPostProcessors.
    Set<BeanDefinitionHolder> processorDefinitions =
            AnnotationConfigUtils.
                registerAnnotationConfigProcessors(parserContext.getRegistry(), source);
    // Register component for the surrounding <context:annotation-config> element.
    CompositeComponentDefinition compDefinition = 
        new CompositeComponentDefinition(element.getTagName(), source);
    parserContext.pushContainingComponent(compDefinition);
    // Nest the concrete beans in the surrounding component.
    for (BeanDefinitionHolder processorDefinition : processorDefinitions) {
        parserContext.registerComponent(new BeanComponentDefinition(processorDefinition));
    }
    // Finally register the composite component.
    // 空实现
    parserContext.popAndRegisterContainingComponent();
    return null;
}
```

### BeanPostProcessor注册

AnnotationConfigUtils.registerAnnotationConfigProcessors源码:

BeanDefinitionHolder:具有名称和别名的 BeanDefinition 的持有者。可以注册为内部 bean 的占位符

```java
//第一个参数其实就是DefaultListableBeanFactory,第二个参数为null
public static Set<BeanDefinitionHolder> registerAnnotationConfigProcessors(
        BeanDefinitionRegistry registry, Object source) {
    //将registery强转为DefaultListableBeanFactory类型
    DefaultListableBeanFactory beanFactory = unwrapDefaultListableBeanFactory(registry);
    if (beanFactory != null) {
        if (!(beanFactory.getDependencyComparator() instanceof AnnotationAwareOrderComparator)) {
   //AnnotationAwareOrderComparator 其作用是比较标注了@Order或@Priority注解的元素的优先级,设置加载优先级
            beanFactory.setDependencyComparator(AnnotationAwareOrderComparator.INSTANCE);
        }
        if (!(beanFactory.getAutowireCandidateResolver() instanceof 
            ContextAnnotationAutowireCandidateResolver)) {
            //ContextAnnotationAutowireCandidateResolver用以决定一个bean是否可以当作一个依赖的候选者
            beanFactory.setAutowireCandidateResolver(new ContextAnnotationAutowireCandidateResolver());
        }
    }

    Set<BeanDefinitionHolder> beanDefs = new LinkedHashSet<BeanDefinitionHolder>(4);

    if (!registry.containsBeanDefinition(CONFIGURATION_ANNOTATION_PROCESSOR_BEAN_NAME)) {
        //ConfigurationClassPostProcessor 用于处理标注了@Configuration注解的类
        RootBeanDefinition def = new RootBeanDefinition(ConfigurationClassPostProcessor.class);
        def.setSource(source);
        beanDefs.add(registerPostProcessor(registry, def,CONFIGURATION_ANNOTATION_PROCESSOR_BEAN_NAME));
    }

    if (!registry.containsBeanDefinition(AUTOWIRED_ANNOTATION_PROCESSOR_BEAN_NAME)) {
        //AutowiredAnnotationBeanPostProcessor 处理@Autowire等注解的bean或是方法进行注入
        RootBeanDefinition def = new RootBeanDefinition(AutowiredAnnotationBeanPostProcessor.class);
        def.setSource(source);
        beanDefs.add(registerPostProcessor(registry, def, AUTOWIRED_ANNOTATION_PROCESSOR_BEAN_NAME));
    }

    if (!registry.containsBeanDefinition(REQUIRED_ANNOTATION_PROCESSOR_BEAN_NAME)) {
        //处理@Require注解,该注解表示setter方法对应的属性必须被Spring所注入
        RootBeanDefinition def = new RootBeanDefinition(RequiredAnnotationBeanPostProcessor.class);
        def.setSource(source);
        beanDefs.add(registerPostProcessor(registry, def, REQUIRED_ANNOTATION_PROCESSOR_BEAN_NAME));
    }

    // Check for JSR-250 support, and if present add the CommonAnnotationBeanPostProcessor.
    if (jsr250Present && !registry.containsBeanDefinition(COMMON_ANNOTATION_PROCESSOR_BEAN_NAME)) {
        //用于开启对JSR-250的支持，开启的先决条件是当前classpath中有其类,@Resource类处理
        RootBeanDefinition def = new RootBeanDefinition(CommonAnnotationBeanPostProcessor.class);
        def.setSource(source);
        beanDefs.add(registerPostProcessor(registry, def, COMMON_ANNOTATION_PROCESSOR_BEAN_NAME));
    }

    // Check for JPA support, and if present add the PersistenceAnnotationBeanPostProcessor.
    if (jpaPresent && !registry.containsBeanDefinition(PERSISTENCE_ANNOTATION_PROCESSOR_BEAN_NAME)) {
        RootBeanDefinition def = new RootBeanDefinition();
       //用于提供JPA支持
        def.setBeanClass(ClassUtils.forName(PERSISTENCE_ANNOTATION_PROCESSOR_CLASS_NAME,
                AnnotationConfigUtils.class.getClassLoader()));
        def.setSource(source);
        beanDefs.add(registerPostProcessor(registry, def, PERSISTENCE_ANNOTATION_PROCESSOR_BEAN_NAME));
    }

    if (!registry.containsBeanDefinition(EVENT_LISTENER_PROCESSOR_BEAN_NAME)) {
        //处理@EventListener注解
        RootBeanDefinition def = new RootBeanDefinition(EventListenerMethodProcessor.class);
        def.setSource(source);
        beanDefs.add(registerPostProcessor(registry, def, EVENT_LISTENER_PROCESSOR_BEAN_NAME));
    }
    if (!registry.containsBeanDefinition(EVENT_LISTENER_FACTORY_BEAN_NAME)) {
        //和上面配合使用用以产生EventListener对象
        RootBeanDefinition def = new RootBeanDefinition(DefaultEventListenerFactory.class);
        def.setSource(source);
        beanDefs.add(registerPostProcessor(registry, def, EVENT_LISTENER_FACTORY_BEAN_NAME));
    }

    return beanDefs;
}
```

context元素配合起来一起完成某种功能的组件,Spring抽象成为ComponentDefinition接口，组件的集合表示成为CompositeComponentDefinition，类图:

[![CompositeComponentDefinition类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/CompositeComponentDefinition.jpg)](https://github.com/seaswalker/spring-analysis/blob/master/note/images/CompositeComponentDefinition.jpg)

### 运行

执行入口`AbstractApplicationContext.refresh` -> `invokeBeanFactoryPostProcessors(beanFactory/*BeanFactoryPostProcessor类型*/);`

#### postProcessBeanDefinitionRegistry

此方法大体由两部分组成。

###### BeanPostProcessor注册
ImportAware接口的作用就是**使被引用的配置类可以获得引用类的相关信息**

##### 类解析

这里便是对标注了@Configuration注解的类及进行解析，通过调用ConfigurationClassPostProcessor的processConfigBeanDefinitions方法来实现

###### bean名字生成策略

对于配置类，Spring也会将其当作一个bean放到容器中，这就关系到bean的起名了，其实这部分对于@Component, @Controller等注解都是一样的。

```java
//检测通过封闭应用程序上下文提供的任何自定义 bean 名称生成策略
SingletonBeanRegistry sbr = null;
if (registry instanceof SingletonBeanRegistry) {
   sbr = (SingletonBeanRegistry) registry;
   if (!this.localBeanNameGeneratorSet && sbr.containsSingleton(CONFIGURATION_BEAN_NAME_GENERATOR)) {
       //org.springframework.context.annotation.internalConfigurationBeanNameGenerator
      BeanNameGenerator generator = (BeanNameGenerator) sbr.getSingleton(CONFIGURATION_BEAN_NAME_GENERATOR);
      this.componentScanBeanNameGenerator = generator;
      this.importBeanNameGenerator = generator;
   }
}
```

![BeanNameGenerator类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/BeanNameGenerator.jpg)

BeanFactoryPostProcessor的触发时机: **配置解析、BeanDefinition加载之后，Singleton初始化之前**，所以即使配置了此接口的实现，但是此时此bean尚未初始化，所以根本看不到此实例

真正执行解析操作在processConfigBeanDefinitions循环中

```java
do {
	//这一步真正解析BeanDefinitionHolder信息,具体操作在 ConfigurationClassParser.parse中执行
   parser.parse(candidates);
   parser.validate();
  candidates.clear();
}
while (!candidates.isEmpty());
```

#### postProcessBeanFactory

此方法调用了enhanceConfigurationClasses，其实就是将@Configuration的beanClass转换为CGLIB代理子类.主要代码

```java
ConfigurationClassEnhancer enhancer = new ConfigurationClassEnhancer();
// configBeanDefs<beanName, AbstractBeanDefinition>
for (Map.Entry<String, AbstractBeanDefinition> entry : configBeanDefs.entrySet()) {
   AbstractBeanDefinition beanDef = entry.getValue();
   // If a @Configuration class gets proxied, always proxy the target class
    //如果 @Configuration 类被代理，总是代理目标类
   beanDef.setAttribute(AutoProxyUtils.PRESERVE_TARGET_CLASS_ATTRIBUTE, Boolean.TRUE);
   
      // Set enhanced subclass of the user-specified bean class
     //设置用户指定bean类的增强子类
      Class<?> configClass = beanDef.resolveBeanClass(this.beanClassLoader);
      Class<?> enhancedClass = enhancer.enhance(configClass, this.beanClassLoader);
      if (configClass != enhancedClass) {
         beanDef.setBeanClass(enhancedClass);
      }
}

//enhancer.enhance 关键步骤
//初始化增强器,利用增强器生成超类的子类，确保为新子类注册回调
Class<?> enhancedClass = createClass(newEnhancer(configClass, classLoader));
//初始化增强器的关键.ConfigurationClassEnhancer.ConditionalCallbackFilter
enhancer.setCallbackFilter(CALLBACK_FILTER);
enhancer.setCallbackTypes(CALLBACK_FILTER.getCallbackTypes());

//CALLBACK_FILTER 实现
private static final Callback[] CALLBACKS = new Callback[] {
			new BeanMethodInterceptor(),
			new BeanFactoryAwareMethodInterceptor(),
			NoOp.INSTANCE
	};

private static final ConditionalCallbackFilter CALLBACK_FILTER = new ConditionalCallbackFilter(CALLBACKS);

//使用CALLBACK_FILTER原因:提供Scope支持,实现EnhancedConfiguration接口

```

### AutowiredAnnotationBeanPostProcessor

真正实现的是postProcessMergedBeanDefinition和postProcessPropertyValues两个方法。

#### postProcessMergedBeanDefinition

方法入口在`AbstractAutowireCapableBeanFactory.doCreateBean`

```java
synchronized (mbd.postProcessingLock) {
   if (!mbd.postProcessed) {
      try {
         applyMergedBeanDefinitionPostProcessors(mbd, beanType, beanName);
      }
	  postProcessed = true;
   }
}
```

`AbstractAutowireCapableBeanFactory#applyMergedBeanDefinitionPostProcessors`

```java
//将 MergedBeanDefinitionPostProcessors 应用于指定的 bean 定义，调用它们的postProcessMergedBeanDefinition方法
protected void applyMergedBeanDefinitionPostProcessors(RootBeanDefinition mbd, Class<?> beanType, String beanName) {
   for (BeanPostProcessor bp : getBeanPostProcessors()) {
      if (bp instanceof MergedBeanDefinitionPostProcessor) {
         MergedBeanDefinitionPostProcessor bdp = (MergedBeanDefinitionPostProcessor) bp;
         bdp.postProcessMergedBeanDefinition(mbd, beanType, beanName);
      }
   }
}
```

`AutowiredAnnotationBeanPostProcessor#postProcessMergedBeanDefinition`

```java
public void postProcessMergedBeanDefinition(RootBeanDefinition beanDefinition, Class<?> beanType, String beanName) {
   if (beanType != null) {
       //查找自动装配元数据
      InjectionMetadata metadata = findAutowiringMetadata(beanName, beanType, null);
      metadata.checkConfigMembers(beanDefinition);
   }
}
```

findAutowiringMetadata:首先会获取缓存,缓存injectionMetadataCache是一个ConcurrentHashMap对象,如果获取缓存双检后失败会重建.

缓存设置原因:

- 假设有多线程同时调用针对某一个bean的getBean方法，那么这样可以保证只有一个线程执行一次@Autowire注解的扫描工作。
- 对于非singleton(比如prototype)类型的bean，这样同样可以保证只解析一次，防止做无用功。

Spring使用了一个do while循环来一直检测其父类，直到Object，这就说明，**Spring注入注解可以配置在此bean的父类上**。其实，最开始的时候网站的Service层和Dao层一直都是这么做的。

##### 变量扫描

之后便是逐一扫描当前类的成员变量，检测是否有@Autowire注解。ReflectionUtils的实现其实就是[访问者模式](https://www.jianshu.com/p/1f1049d0a0f4).

```java
//对给定类的所有匹配方法执行给定的回调操作
public static void doWithLocalFields(Class<?> clazz, FieldCallback fc) {
		for (Field field : getDeclaredFields(clazz)) {
				fc.doWith(field);
		}
	}
```

##### 方法扫描

###### bridge方法

```java
//寻找bridge方法
Method bridgedMethod = BridgeMethodResolver.findBridgedMethod(method);
```

bridge和static之类一样，在java内部也是有一个修饰符的，只不过只在jvm内部可见。理解:指子类重写了的父类方法.两个方法的签名是一模一样的，只有返回类型不同.

可以参考: [Java那些不为人知的特殊方法](http://ifeve.com/syntethic-and-bridge-methods/)

###### PropertyDescriptor

用于描述java bean，如果被标注@Autowire的方法是一个getter或setter方法，那么Spring会保存下来其PropertyDescriptor对象，如果不是，那么就是空。

#### postProcessPropertyValues

##### 入口

AbstractAutowireCapableBeanFactory.populateBean方法，执行时机是在bean的属性都已经计算(根据xml配置进行完autowire)完毕，设置到bean实例之前。

```java
public PropertyValues postProcessPropertyValues(
      PropertyValues pvs, PropertyDescriptor[] pds, Object bean, String beanName) throws BeanCreationException {
   InjectionMetadata metadata = findAutowiringMetadata(beanName, bean.getClass(), pvs);
		//方法和属性注入,从容器中查找相关的依赖，用反射的方法调用Field的set方法
      metadata.inject(bean, beanName, pvs);

   return pvs;
}
```

### RequiredAnnotationBeanPostProcessor

与AutowiredAnnotationBeanPostProcessor类似,无论类结构与调用时机顺序都是一样的

#### postProcessMergedBeanDefinition

该方法空实现

#### postProcessPropertyValues

核心代码

```java
for (PropertyDescriptor pd : pds) {
   if (isRequiredProperty(pd) && !pvs.contains(pd.getName())) {
      invalidProperties.add(pd.getName());
   }
    //validatedBeanNames 缓存经过验证的 bean 名称，跳过对同一 bean 的重新验证
    this.validatedBeanNames.add(beanName);
}
```

populateBean 中通过获取BeanWrapper中的PropertyDescriptor数组,遍历数组执行postProcessPropertyValues方法.BeanWrapper中通过getPropertyDescriptors方法,变量所有方法获取,@Reqired注解只对setter方法有效.

### CommonAnnotationBeanPostProcessor

此类主要是整合了MergedBeanDefinitionPostProcessor和DestructionAwareBeanPostProcessor的功能

#### postProcessMergedBeanDefinition

结合父类`InitDestroyAnnotationBeanPostProcessor`与自己的方法

```java
public void postProcessMergedBeanDefinition(RootBeanDefinition beanDefinition, Class<?> beanType, String beanName) {
   if (beanType != null) {
       //获取Lifecycle元素,即init和destory方法
		LifecycleMetadata metadata = findLifecycleMetadata(beanType);
		metadata.checkConfigMembers(beanDefinition);
	}
   if (beanType != null) {
       //查找自动装配元数据,Resource 解析,大体过程与AutowiredAnnotationBeanPostProcessor中的相似
      InjectionMetadata metadata = findResourceMetadata(beanName, beanType, null);
      metadata.checkConfigMembers(beanDefinition);
   }
}
```

#### postProcessPropertyValues

与`AutowiredAnnotationBeanPostProcessor`中的相似

#### postProcessBeforeInitialization

调用父类中的方法

```java
public Object postProcessBeforeInitialization(Object bean, String beanName) throws BeansException {
    
   LifecycleMetadata metadata = findLifecycleMetadata(bean.getClass());
    //spring支持一个类中有多个init方法
   metadata.invokeInitMethods(bean, beanName);
   return bean;
}

public void invokeInitMethods(Object target, String beanName) throws Throwable {
    Collection<LifecycleElement> initMethodsToIterate =
            (this.checkedInitMethods != null ? this.checkedInitMethods : this.initMethods);
    if (!initMethodsToIterate.isEmpty()) {
        for (LifecycleElement element : initMethodsToIterate) {
             // 反射调用
            element.invoke(target);
        }
    }
}
```

#### postProcessBeforeDestruction

实现逻辑与postProcessBeforeInitialization相似.

# component-scan

`ComponentScanBeanDefinitionParser.parse`源码

```java
public BeanDefinition parse(Element element, ParserContext parserContext) {
   //获取 basePackage 路径
   String basePackage = element.getAttribute(BASE_PACKAGE_ATTRIBUTE);
   //解析basePackage路径 
   basePackage = parserContext.getReaderContext().getEnvironment().resolvePlaceholders(basePackage);
    //通过 `,;` 分隔多个路径
   String[] basePackages = StringUtils.tokenizeToStringArray(basePackage,
         ConfigurableApplicationContext.CONFIG_LOCATION_DELIMITERS);

   // Actually scan for bean definitions and register them.
   //实际扫描 bean 定义并注册它们。 
    //初始化扫描器
   ClassPathBeanDefinitionScanner scanner = configureScanner(parserContext, element);
    //利用扫描器扫描基础包获取bean定义
   Set<BeanDefinitionHolder> beanDefinitions = scanner.doScan(basePackages);
   registerComponents(parserContext.getReaderContext(), beanDefinitions, element);
   return null;
}
```

## 初始化

此部分负责初始化包扫描用到的扫描器，是一个ClassPathBeanDefinitionScanner对象，configureScanner方法源码

```java
//初始化扫描器
protected ClassPathBeanDefinitionScanner configureScanner(ParserContext parserContext, Element element) {
   boolean useDefaultFilters = true;
    //use-default-filters 默认扫描 @Component、@Repository、@Service和@Controller,如果不设置则不会扫描这几个属性
   if (element.hasAttribute(USE_DEFAULT_FILTERS_ATTRIBUTE)) {
      useDefaultFilters = Boolean.valueOf(element.getAttribute(USE_DEFAULT_FILTERS_ATTRIBUTE));
   }

   // Delegate bean definition registration to scanner class.
    //初始化ClassPathBeanDefinitionScanner类
   ClassPathBeanDefinitionScanner scanner = createScanner(parserContext.getReaderContext(), useDefaultFilters);
   scanner.setBeanDefinitionDefaults(parserContext.getDelegate().getBeanDefinitionDefaults());
   scanner.setAutowireCandidatePatterns(parserContext.getDelegate().getAutowireCandidatePatterns());

   if (element.hasAttribute(RESOURCE_PATTERN_ATTRIBUTE)) {
		//resource-pattern 用以配置扫描器扫描的路径，默认**/*.class
       scanner.setResourcePattern(element.getAttribute(RESOURCE_PATTERN_ATTRIBUTE));
   }

	//指定命名策略,parseBeanNameGenerator方法会直接使用反射的方法生成其对象。
   parseBeanNameGenerator(element, scanner);

	//使用的ScopeMetadataResolver,用于解析bean的scope定义;检测到的bean生成代理子类,默认no
   parseScope(element, scanner);

   //parseTypeFilters方法负责此部分(exclude-filter/include-filter)的解析
   parseTypeFilters(element, scanner, parserContext);

   return scanner;
}
```

## 扫描

入口方法便是ClassPathBeanDefinitionScanner.doScan

```java
//在指定的基本包中执行扫描，返回注册的 bean 定义。此方法不注册注释配置处理器，而是将其留给调用者
protected Set<BeanDefinitionHolder> doScan(String... basePackages) {
   Set<BeanDefinitionHolder> beanDefinitions = new LinkedHashSet<BeanDefinitionHolder>();
   for (String basePackage : basePackages) {
       //扫描候选组件的类路径
      Set<BeanDefinition> candidates = findCandidateComponents(basePackage);
      for (BeanDefinition candidate : candidates) {
         ScopeMetadata scopeMetadata = this.scopeMetadataResolver.resolveScopeMetadata(candidate);
         candidate.setScope(scopeMetadata.getScopeName());
         //根据名称解析策略生成 beanName
         String beanName = this.beanNameGenerator.generateBeanName(candidate, this.registry);
         if (candidate instanceof AbstractBeanDefinition) {
             //  为BeanDefinition设置默认的属性 beanDefinition.applyDefaults
            postProcessBeanDefinition((AbstractBeanDefinition) candidate, beanName);
         }
         if (candidate instanceof AnnotatedBeanDefinition) {
             //根据注解设置bean的不同属性
    	        AnnotationConfigUtils.processCommonDefinitionAnnotations((AnnotatedBeanDefinition) candidate);
         }
          //检测是否存在同名的BeanDefinition
         if (checkCandidate(beanName, candidate)) {
            BeanDefinitionHolder definitionHolder = new BeanDefinitionHolder(candidate, beanName);
            definitionHolder =
                  AnnotationConfigUtils.applyScopedProxyMode(scopeMetadata, definitionHolder, this.registry);
            beanDefinitions.add(definitionHolder);
            registerBeanDefinition(definitionHolder, this.registry);
         }
      }
   }
   return beanDefinitions;
}
```

### 逐包扫描/BeanDefinition解析

扫描其实就是在classpath下直接读取class文件。读取到的class文件被Spring用Resource接口表示。之后交由MetadataReader进行解析.

对class文件的读取、分析是通过ASM完成的，入口在SimpleMetadataReader的构造器:

```java
AnnotationMetadataReadingVisitor visitor = new AnnotationMetadataReadingVisitor(classLoader);
```

核心在于其visitAnnotation方法:

```java
@Override
public AnnotationVisitor visitAnnotation(final String desc, boolean visible) {
    //type 类在ASM包中 (org.springframework.asm)
    String className = Type.getType(desc).getClassName();
    this.annotationSet.add(className);
    return new AnnotationAttributesReadingVisitor(
        className, this.attributesMap, this.metaAnnotationMap, this.classLoader);
}
```

### @Scope解析

`AnnotationScopeMetadataResolver.resolveScopeMetadata`

在doScan方法中对BeanDefinition进行解析,获取定义的scope注解中的value属性.

### bean名字生成

`AnnotationBeanNameGenerator.generateBeanName`

```java
@Override
public String generateBeanName(BeanDefinition definition, BeanDefinitionRegistry registry) {
    if (definition instanceof AnnotatedBeanDefinition) {
        //尝试根据@Component、@Service、@Controller、@Repository、@ManagedBean、@Named的value属性生成beanName
        String beanName = determineBeanNameFromAnnotation((AnnotatedBeanDefinition) definition);
        if (StringUtils.hasText(beanName)) {
            // Explicit bean name found.
            return beanName;
        }
    }
    // Fallback: generate a unique default bean name.
    //上述都不符合则使用默认策略 base.SimpleBean -> SimpleBean SimpleBean -> simpleBean
    return buildDefaultBeanName(definition, registry);
}
```

### 冲突检测

Spring会检测容器中是否已经存在同名的BeanDefinition。`ClassPathBeanDefinitionScanner.checkCandidate`

```java
protected boolean checkCandidate(String beanName, BeanDefinition beanDefinition) {
    // 没有同名的，直接返回
    if (!this.registry.containsBeanDefinition(beanName)) {
        return true;
    }
    BeanDefinition existingDef = this.registry.getBeanDefinition(beanName);
    BeanDefinition originatingDef = existingDef.getOriginatingBeanDefinition();
    if (originatingDef != null) {
        existingDef = originatingDef;
    }
    //isCompatible用于判断和之前的BeanDefinition是否兼容
    if (isCompatible(beanDefinition, existingDef)) {
        return false;
    }
    throw new ConflictingBeanDefinitionException("冲突啦!");
}
```

**如果已经存在的BeanDefinition不是扫描来的，如果是由同一个class文件解析来的，如果两者equals，Spring都认为是兼容的，即Spring会用新的替换之前的。**

### 代理生成

入口: `ClassPathBeanDefinitionScanner.doScan`

```java
BeanDefinitionHolder definitionHolder = new BeanDefinitionHolder(candidate, beanName);
//如果ScopedProxyMode.NO则直接返回传入的参数,否则执行ScopedProxyCreator.createScopedProxy
definitionHolder = AnnotationConfigUtils.applyScopedProxyMode(scopeMetadata, definitionHolder, this.registry);
```

最终调用的是ScopedProxyUtils.createScopedProxy，源码很多，这里说下重点:

- 这里所做的是生成了一个新的BeanDefinition对象，作为代理者，其属性拷贝自被代理者，代理者的beanClass为ScopedProxyFactoryBean，代理者的名字设置为被代理者的名字，而被代理者的名字改为scopedTarget.原名字，代理者内部有一个targetBeanName属性，就是被代理者的名字。

- 被代理者的autowireCandidate和primary属性被设为false，不能再当作其它bean的注入候选者。

- 将被代理者以scopedTarget.原名字注册到容器，返回代理者。

- 代理者和被代理者同时存在于容器中。

## property-override

### 作用

允许我们使用属性文件(.properties)的形式对bean的属性进行替换.

示例

```properties
student.name=dog
```

```xml
<bean id="student" class="base.Student">
    <property name="name" value="skywalker" />
    <property name="age" value="30" />
</bean>
```

### 类图

具体的实现类是PropertyOverrideBeanDefinitionParser

![PropertyOverrideBeanDefinitionParser类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/PropertyOverrideBeanDefinitionParser.jpg)

### 解析

解析的原理是将此配置相关的信息保存到BeanDefinition中，更准确的说是一个GenericBeanDefinition.

实际调用父类`AbstractPropertyLoadingBeanDefinitionParser`中`doParse`方法

```java
protected void doParse(Element element, ParserContext parserContext, BeanDefinitionBuilder builder) {
   String location = element.getAttribute("location");
   if (StringUtils.hasLength(location)) {
      location = parserContext.getReaderContext().getEnvironment().resolvePlaceholders(location);
       //将逗号分隔的loaction列表,解析为数组
      String[] locations = StringUtils.commaDelimitedListToStringArray(location);
      builder.addPropertyValue("locations", locations);
   }
	//此属性允许我们直接引用一个java.util.Properties类型的bean作为数据源
   String propertiesRef = element.getAttribute("properties-ref");
   if (StringUtils.hasLength(propertiesRef)) {
      builder.addPropertyReference("properties", propertiesRef);
   }

   String fileEncoding = element.getAttribute("file-encoding");
   if (StringUtils.hasLength(fileEncoding)) {
      builder.addPropertyValue("fileEncoding", fileEncoding);
   }
	//指定其优先级
   String order = element.getAttribute("order");
   if (StringUtils.hasLength(order)) {
      builder.addPropertyValue("order", Integer.valueOf(order));
   }
	//如果设为true，那么对于没有找到的属性文件将会忽略，否则会抛出异常，默认为false，抛异常
   builder.addPropertyValue("ignoreResourceNotFound",
         Boolean.valueOf(element.getAttribute("ignore-resource-not-found")));
   //
   builder.addPropertyValue("localOverride",
         Boolean.valueOf(element.getAttribute("local-override")));

   builder.setRole(BeanDefinition.ROLE_INFRASTRUCTURE);
}
```

#### BeanDefinition

保存的BeanDefinition的beanClass为PropertyOverrideConfigurer，其类图

![PropertyOverrideConfigurer类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/PropertyOverrideConfigurer.jpg)

### 运行

入口当然是BeanFactoryPostProcessor.postProcessBeanFactory

```java
@Override
public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) throws BeansException {
   	  //属性加载:对local-override的支持是通过改变local和文件两者的加载顺序来实现的
      Properties mergedProps = mergeProperties();
      // Convert the merged properties, if necessary.
      //合并属性
      convertProperties(mergedProps);
      // Let the subclass process the properties.
      //设置属性:逐个属性调用PropertyOverrideConfigurer.applyPropertyValue
      processProperties(beanFactory, mergedProps);

}
```

## property-placeholder

 `<context:property-placeholder location="classpath:foo.properties" />` spring会自动引入配置文件

### 解析

解析的实现类是PropertyPlaceholderBeanDefinitionParser，此类的父类继承体系和property-override的PropertyOverrideBeanDefinitionParser完全一样，所以整体的处理套路也是基本一致

```java
protected void doParse(Element element, ParserContext parserContext, BeanDefinitionBuilder builder) {
   super.doParse(element, parserContext, builder);

   builder.addPropertyValue("ignoreUnresolvablePlaceholders",
         Boolean.valueOf(element.getAttribute("ignore-unresolvable")));

    //Spring会将java的System.getProperties也当做属性的来源，此配置用于设置系统的和本地文件的同名属性的覆盖方式,系统会覆盖本地的同名属性
   String systemPropertiesModeName = element.getAttribute(SYSTEM_PROPERTIES_MODE_ATTRIBUTE);
   if (StringUtils.hasLength(systemPropertiesModeName) &&
         !systemPropertiesModeName.equals(SYSTEM_PROPERTIES_MODE_DEFAULT)) {
      builder.addPropertyValue("systemPropertiesModeName", "SYSTEM_PROPERTIES_MODE_" + systemPropertiesModeName);
   }

    //用于配置默认的值的分隔符:
   if (element.hasAttribute("value-separator")) {
      builder.addPropertyValue("valueSeparator", element.getAttribute("value-separator"));
   }
    //是否移除开头和结尾的空格,无论设置什么值都会生效
   if (element.hasAttribute("trim-values")) {
      builder.addPropertyValue("trimValues", element.getAttribute("trim-values"));
   }
    //遇到哪些值应该当做空处理
   if (element.hasAttribute("null-value")) {
      builder.addPropertyValue("nullValue", element.getAttribute("null-value"));
   }
}
```

### BeanDefinition

这次是PropertySourcesPlaceholderConfigurer，其类图:

[![PropertySourcesPlaceholderConfigurer类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/PropertySourcesPlaceholderConfigurer.jpg)](https://github.com/seaswalker/spring-analysis/blob/master/note/images/PropertySourcesPlaceholderConfigurer.jpg)

### 运行

`PropertySourcesPlaceholderConfigurer.postProcessBeanFactory`

如果其内部的propertySources属性不为空(当然默认是空)，那么属性文件和系统属性都会被忽略。它的使用场景应该是这样:

不使用property-placeholder标签，以显式的bean定义代替。

#### 处理

处理的过程就是遍历全部BeanDefinition，替换${}

## load-time-weaver & spring-configured

load-time-weaver用以开启aspectj类加载期织入，实际上是利用jdk1.6提供的instrument API实现的，原理就是jvm会在类加载之前将class暴露给我们制定的类，允许我们在此时对类进行修改。aspectj便利用此机会根据我们的配置生成对应的满足需求的子类。

### javaagent

要想使用此功能需要配置jvm参数javaagent指定代理类的jar包，示例:
```
-javaagent:D:\Software\maven-repos\org\springframework\spring-agent\2.5.6.SEC03\spring-agent-2.5.6.SEC03.jar
```

此jar包的META-INF/MANIFEST.MF文件需要配置如下一行:

`Premain-Class: org.springframework.instrument.InstrumentationSavingAgent`

## 解析

解析的实现类是`LoadTimeWeaverBeanDefinitionParser`

#### LoadTimeWeaver

此接口用于向ClassLoader添加ClassFileTransformer对象，其继承体系:

![LoadTimeWeaver继承体系](https://github.com/seaswalker/spring-analysis/raw/master/note/images/LoadTimeWeaver.jpg)



`getBeanClassName`如果有设置`weaver-class`则使用设置的值,如果没有默认为`DefaultContextLoadTimeWeaver`.解析出来的BeanDefinition id是`loadTimeWeaver`.

DefaultContextLoadTimeWeaver其实是个包装类,它会自动根据外部环境确定内部LoadTimeWeaver的实现.

#### LoadTimeWeaverBeanDefinitionParser

`LoadTimeWeaverBeanDefinitionParser#doParse`核心方法.

```java
protected void doParse(Element element, ParserContext parserContext, BeanDefinitionBuilder builder) {
   builder.setRole(BeanDefinition.ROLE_INFRASTRUCTURE);
	//aspectj-weaving根据该设置,确定是否开启,autodect会判断是否存在AOP.xml
   if (isAspectJWeavingEnabled(element.getAttribute(ASPECTJ_WEAVING_ATTRIBUTE), parserContext)) {
      if (!parserContext.getRegistry().containsBeanDefinition(ASPECTJ_WEAVING_ENABLER_BEAN_NAME)) {
          //如果没有就初始化AspectJWeavingEnabler bean
         RootBeanDefinition def = new RootBeanDefinition(ASPECTJ_WEAVING_ENABLER_CLASS_NAME);
         parserContext.registerBeanComponent(
               new BeanComponentDefinition(def, ASPECTJ_WEAVING_ENABLER_BEAN_NAME));
      }

      if (isBeanConfigurerAspectEnabled(parserContext.getReaderContext().getBeanClassLoader())) {
          //如果AspectJWeavingEnabler 类存在则,使用SpringConfiguredBeanDefinitionParser 解析
         new SpringConfiguredBeanDefinitionParser().parse(element, parserContext);
      }
   }
}
```



#### AspectJWeavingEnabler

AspectJWeavingEnabler 用来向LoadTimeWeaver设置aspectj的ClassPreProcessorAgentAdapter对象.

![AspectJWeavingEnabler类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/AspectJWeavingEnabler.jpg)

#### SpringConfiguredBeanDefinitionParser

ContextNamespaceHandler的init方法中spring-configured对应的解析器其实就是它.

```java
registerBeanDefinitionParser("spring-configured", new SpringConfiguredBeanDefinitionParser());
```

其parse方法

```java

public BeanDefinition parse(Element element, ParserContext parserContext) {
   if (!parserContext.getRegistry().containsBeanDefinition(BEAN_CONFIGURER_ASPECT_BEAN_NAME)) {
      //不包含internalBeanConfigurerAspect bean则创建一个,实际类为AnnotationBeanConfigurerAspect 
      RootBeanDefinition def = new RootBeanDefinition();
      def.setBeanClassName(BEAN_CONFIGURER_ASPECT_CLASS_NAME);
      def.setFactoryMethodName("aspectOf");
      def.setRole(BeanDefinition.ROLE_INFRASTRUCTURE);
      def.setSource(parserContext.extractSource(element));
      parserContext.registerBeanComponent(new BeanComponentDefinition(def, BEAN_CONFIGURER_ASPECT_BEAN_NAME));
   }
   return null;
}
```

AnnotationBeanConfigurerAspect及其父类其实是由aspectj源文件(.aj)编译而来，所以在spring-aspectj的源码包中看到的是.aj文件而不是.java。

## 运行

`AspectJWeavingEnabler.postProcessBeanFactory`

```java
public void postProcessBeanFactory(ConfigurableListableBeanFactory beanFactory) throws BeansException {
   enableAspectJWeaving(this.loadTimeWeaver, this.beanClassLoader);
}

public static void enableAspectJWeaving(LoadTimeWeaver weaverToUse, ClassLoader beanClassLoader) {
	if (weaverToUse == null) {
		if (InstrumentationLoadTimeWeaver.isInstrumentationAvailable()) {
			weaverToUse = new InstrumentationLoadTimeWeaver(beanClassLoader);
			}
		else {
			throw new IllegalStateException("No LoadTimeWeaver available");
			}
		}
	weaverToUse.addTransformer(
				new AspectJClassBypassingClassFileTransformer(new ClassPreProcessorAgentAdapter()));
	}
```

当Context初始化时，AbstractApplicationContext.prepareBeanFactory中注入了`LoadTimeWeaverAwareProcessor`类

```java
// Detect a LoadTimeWeaver and prepare for weaving, if found.
if (beanFactory.containsBean(LOAD_TIME_WEAVER_BEAN_NAME)) {
   beanFactory.addBeanPostProcessor(new LoadTimeWeaverAwareProcessor(beanFactory));
   // Set a temporary ClassLoader for type matching.
   beanFactory.setTempClassLoader(new ContextTypeMatchClassLoader(beanFactory.getBeanClassLoader()));
}
```

而`LoadTimeWeaverAwareProcessor#postProcessBeforeInitialization`方法中注入了`AspectJWeavingEnabler`类.

BeanFactoryPostProcessor也是一个bean，所以它的初始化也会BeanPostProcessor的处理。不过注意一点:

BeanPostProcessor的注册是在BeanFactoryPostProcessor的调用之后进行的:

AbstractApplicationContext.refresh:

```
// Invoke factory processors registered as beans in the context.
invokeBeanFactoryPostProcessors(beanFactory);
// Register bean processors that intercept bean creation.
registerBeanPostProcessors(beanFactory);
```

**我们自定义的BeanPostProcessor不会对BeanFactoryPostProcessor的初始化造成影响，除非使用调用BeanFactory.addBeanPostProcessor的方式进行添加**。

#### BeanClassLoaderAware

##### 入口

AbstractAutowireCapableBeanFactory.initializeBean调用了invokeAwareMethods方法，源码:

```java
private void invokeAwareMethods(final String beanName, final Object bean) {
    if (bean instanceof Aware) {
        if (bean instanceof BeanNameAware) {
            ((BeanNameAware) bean).setBeanName(beanName);
        }
        if (bean instanceof BeanClassLoaderAware) {
            ((BeanClassLoaderAware) bean).setBeanClassLoader(getBeanClassLoader());
        }
        if (bean instanceof BeanFactoryAware) {
            ((BeanFactoryAware) bean).setBeanFactory(AbstractAutowireCapableBeanFactory.this);
        }
    }
}
```

##### setBeanClassLoader

这个方法很关键，对instrument的获取就是在这里。源码:

```java
@Override
public void setBeanClassLoader(ClassLoader classLoader) {
//获取服务器agent,通过检测当前类加载器判断服务器
    LoadTimeWeaver serverSpecificLoadTimeWeaver = createServerSpecificLoadTimeWeaver(classLoader);
    if (serverSpecificLoadTimeWeaver != null) {
        this.loadTimeWeaver = serverSpecificLoadTimeWeaver;
    } else if (InstrumentationLoadTimeWeaver.isInstrumentationAvailable()) {
        //使用spring agent
        this.loadTimeWeaver = new InstrumentationLoadTimeWeaver(classLoader);
    } else {
        //反射
        this.loadTimeWeaver = new ReflectiveLoadTimeWeaver(classLoader);
    }
}
```

其实可以不用Spring，只使用aspectj自己便可以实现LTW，只需要把代理jar包设为aspect-weaver.jar，并自己编写aop.xml文件以及相应的aspect类即可。可以参考官方文档:[Chapter 5. Load-Time Weaving](http://www.eclipse.org/aspectj/doc/released/devguide/ltw-configuration.html#enabling-load-time-weaving)

#### ClassFileTransformer

根据java instrument API的定义，每当一个Class被加载的时候都会去调用挂在Instrumentation上的ClassFileTransformer的transform方法。所以LTW的核心便在这里了。

`AspectJClassBypassingClassFileTransformer.transform`

```java
public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
        ProtectionDomain protectionDomain, byte[] classfileBuffer) {
    // aspectj自身的类无需检测(织入)，直接跳过
    if (className.startsWith("org.aspectj") || className.startsWith("org/aspectj")) {
        return classfileBuffer;
    }
    return this.delegate.transform(loader, className, classBeingRedefined, 
        protectionDomain, classfileBuffer);
}
```

Spring将切面以编译过的Aspectj语言形式定义，不过也可以用原生java类。spring-aspectj包定义的是供各个模块进行LTW的切面。Aspectj部分不再继续向下深入探究。