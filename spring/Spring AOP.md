# Spring AOP

XML配置中aop部分的解析器由AopNamespaceHandler注册,核心方法.

```java
@Override
public void init() {
   // In 2.0 XSD as well as in 2.1 XSD.
    //2.0 XSD及2.1XSD中都有使用
   registerBeanDefinitionParser("config", new ConfigBeanDefinitionParser());
   registerBeanDefinitionParser("aspectj-autoproxy", new AspectJAutoProxyBeanDefinitionParser());
   registerBeanDefinitionDecorator("scoped-proxy", new ScopedProxyBeanDefinitionDecorator());

   // Only in 2.0 XSD: moved to context namespace as of 2.1
    //2.1后移动到context namespace
   registerBeanDefinitionParser("spring-configured", new SpringConfiguredBeanDefinitionParser());
}
```

## XML配置

xml中配置了切面,切点,advisor

```xml
<aop:config>
    <aop:pointcut expression="execution(* exam.service..*.*(..))" id="transaction"/>
    <aop:advisor advice-ref="txAdvice" pointcut-ref="transaction"/>
    <aop:aspect ref="" />
</aop:config>
```

调用`ConfigBeanDefinitionParser.parse`解析

```java
public BeanDefinition parse(Element element, ParserContext parserContext) {
   CompositeComponentDefinition compositeDef =
         new CompositeComponentDefinition(element.getTagName(), parserContext.extractSource(element));
   parserContext.pushContainingComponent(compositeDef);
  //生成代理类，调用AopConfigUtils.registerOrEscalateApcAsRequired 创建代理类的beanDefinition,并设置类代理属性和暴露属性	
   configureAutoProxyCreator(parserContext, element);
	//获取aop:config的子标签信息
   List<Element> childElts = DomUtils.getChildElements(element);
   for (Element elt: childElts) {
      String localName = parserContext.getDelegate().getLocalName(elt);
      if (POINTCUT.equals(localName)) {
         parsePointcut(elt, parserContext);
      }
      else if (ADVISOR.equals(localName)) {
         parseAdvisor(elt, parserContext);
      }
      else if (ASPECT.equals(localName)) {
         parseAspect(elt, parserContext);
      }
   }

   parserContext.popAndRegisterContainingComponent();
   return null;
}
```

## 解析

**parsePointcut 解析 aop:pointcut 配置**

```java
private AbstractBeanDefinition parsePointcut(Element pointcutElement, ParserContext parserContext) {
    //解析id及表达式
   String id = pointcutElement.getAttribute(ID);
   String expression = pointcutElement.getAttribute(EXPRESSION);
	//解析pointcut相信bean信息
   AbstractBeanDefinition pointcutDefinition = null;
   try {
       //parseState基于Stack的结构，用于在解析过程中跟踪逻辑位置。在解析阶段的每个点以特定于阅读器的方式将entries添加到堆栈中。
      this.parseState.push(new PointcutEntry(id));
       //pointcutDefinition 的scope为prototype
      pointcutDefinition = createPointcutDefinition(expression);
     //BeanDefinition的class是AspectJExpressionPointcut。 
      pointcutDefinition.setSource(parserContext.extractSource(pointcutElement));

      String pointcutBeanName = id;
      if (StringUtils.hasText(pointcutBeanName)) {
         parserContext.getRegistry().registerBeanDefinition(pointcutBeanName, pointcutDefinition);
      }
      else {
         pointcutBeanName = parserContext.getReaderContext().registerWithGeneratedName(pointcutDefinition);
      }

      parserContext.registerComponent(
            new PointcutComponentDefinition(pointcutBeanName, pointcutDefinition, expression));
   }
   finally {
      this.parseState.pop();
   }

   return pointcutDefinition;
}
```

AspectJExpressionPointcut类图如下

![AspectJExpressionPointcut类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/AspectJExpressionPointcut.jpg)

**parseAdvisor解析 aop:advisor 配置**

解析方法与pointcut类似,不再赘述

```xml
<!--一般情况下advisor配置-->
<aop:advisor id="" order="" advice-ref="aopAdvice" pointcut="" pointcut-ref="" />
```

[advisor介绍](https://docs.spring.io/spring-framework/docs/current/reference/html/core.html#aop-schema-advisors):概念来自 Spring 中定义的 AOP 支持，在 AspectJ 中没有直接的等价物。advisor就像一个独立的小方面，只有一条建议。通知本身由 bean 表示(所以不需要spring生成代理类)，并且必须实现 [Spring 中的 Advice Types 中](https://docs.spring.io/spring-framework/docs/current/reference/html/core.html#aop-api-advice-types)描述的通知接口之一

advice-ref是必须的属性，**并且这里的advice必须实现org.aopalliance.aop.Advice的子接口**。常用与和事务结合

```xml
<tx:advice id="txAdvice" transaction-manager="transactionManager">
    <tx:attributes>
        <tx:method name="get*" read-only="true" propagation="NOT_SUPPORTED"/>
        <tx:method name="find*" read-only="true" propagation="NOT_SUPPORTED"/>
        <tx:method name="*" propagation="REQUIRED"/>
    </tx:attributes>
</tx:advice>

<aop:config>
    <aop:pointcut expression="execution(* exam.service..*.*(..))" id="transaction"/>
    <aop:advisor advice-ref="txAdvice" pointcut-ref="transaction"/>
</aop:config>
```

![DefaultBeanFactoryPointcutAdvisor类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/DefaultBeanFactoryPointcutAdvisor.jpg)

对于pointcut属性，Spring会同样创建一个AspectJExpressionPointcut类型的BeanDefinition，对于pointcut-ref会生成一个RuntimeBeanReference对象指向原pointcut的引用

**parseAspect解析aop:aspect**

常用XML配置方式

```java
<bean id="aopAdvice" class="base.aop.AopDemoAdvice" />
<!-- 必须配置，因为被代理的对象必须在Spring容器中 -->
<bean id="aopDemo" class="base.aop.AopDemo" />
<aop:config>
    <aop:pointcut id="pointcut" expression="execution(* base.aop.AopDemo.send())" />
    <aop:aspect ref="aopAdvice">
        <aop:before method="beforeSend" pointcut-ref="pointcut" />
        <aop:after method="afterSend" pointcut-ref="pointcut" />
    </aop:aspect>
</aop:config>
```

经过parseAspect方法解析结构如下

```yml
AspectComponentDefinition
    beanRefArray
        RuntimeBeanReference(aop:aspect的ref属性)
    beanDefArray
        // 被注册
        RootBeanDefinition(aop:declare-parents)
            beanClass: DeclareParentsAdvisor
            ConstructorArg
                implement-interface
                types-matching
                default-impl
                delegate-ref
        // 被注册
        RootBeanDefinition(aop:before,aop:after...)
            beanClass: AspectJPointcutAdvisor
            ConstructorArg
                RootBeanDefinition
                    beanClass: 由子标签决定
                    ConstructorArg
                        RootBeanDefinition
                            beanClass: MethodLocatingFactoryBean
                            properties
                                targetBeanName: aspectName
                                methodName: method属性
                        RootBeanDefinition
                            beanClass: SimpleBeanFactoryAwareAspectInstanceFactory
                            properties
                                aspectBeanName: aspectName
                        //还有pointcut定义和引用...
```

在解析aspect时会调用方法,getAdviceClass,解析advice返回类型.

```java
private Class<?> getAdviceClass(Element adviceElement, ParserContext parserContext) {
   String elementName = parserContext.getDelegate().getLocalName(adviceElement);
   if (BEFORE.equals(elementName)) {
      return AspectJMethodBeforeAdvice.class;
   }
   else if (AFTER.equals(elementName)) {
      return AspectJAfterAdvice.class;
   }
   else if (AFTER_RETURNING_ELEMENT.equals(elementName)) {
      return AspectJAfterReturningAdvice.class;
   }
   else if (AFTER_THROWING_ELEMENT.equals(elementName)) {
      return AspectJAfterThrowingAdvice.class;
   }
   else if (AROUND.equals(elementName)) {
      return AspectJAroundAdvice.class;
   }
}
```

## 代理子类生成

关键在于AspectJAwareAdvisorAutoProxyCreator，此对象在ConfigBeanDefinitionParser的configureAutoProxyCreator方法中注册

![AspectJAwareAdvisorAutoProxyCreator类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/AspectJAwareAdvisorAutoProxyCreator.jpg)

### 入口

`AspectJAwareAdvisorAutoProxyCreator`实现了`SmartInstantiationAwareBeanPostProcessor`接口,在创建类之前会调用`postProcessBeforeInstantiation`方法.

#### 调用时机

AbstractAutowireCapableBeanFactory.createBean部分源码:

```java
// Give BeanPostProcessors a chance to return a proxy instead of the target bean instance.在改方法中会调用`postProcessBeforeInstantiation`方法
Object bean = resolveBeforeInstantiation(beanName, mbdToUse);
if (bean != null) {
    return bean;
}
Object beanInstance = doCreateBean(beanName, mbdToUse, args);
```

调用`postProcessBeforeInstantiation`,实际会调用`AspectJAwareAdvisorAutoProxyCreator`类中的方法.

```java
public Object postProcessBeforeInstantiation(Class<?> beanClass, String beanName) throws BeansException {
   Object cacheKey = getCacheKey(beanClass, beanName);

   if (beanName == null || !this.targetSourcedBeans.contains(beanName)) {
       //检测缓存结果,因为会在每个bean实例化前调用,使用advisedBeans作为缓存
      if (this.advisedBeans.containsKey(cacheKey)) {
         return null;
      }
       //判断是否应该被代理,不应该被代理的分为两种情况:1.用于实现spring AOP的基础类 2.应该跳过的类,默认返回false,即都不跳过
      if (isInfrastructureClass(beanClass) || shouldSkip(beanClass, beanName)) {
         this.advisedBeans.put(cacheKey, Boolean.FALSE);
         return null;
      }
   }

   // Create proxy here if we have a custom TargetSource.
   // Suppresses unnecessary default instantiation of the target bean:
   // The TargetSource will handle target instances in a custom fashion.
   //如果我们有自定义 TargetSource，请在此处创建代理。抑制目标 bean 的不必要的默认实例化：TargetSource 将以自定义方式处理目标实例 
   if (beanName != null) {
      TargetSource targetSource = getCustomTargetSource(beanClass, beanName);
      if (targetSource != null) {
         this.targetSourcedBeans.add(beanName);
         Object[] specificInterceptors = getAdvicesAndAdvisorsForBean(beanClass, beanName, targetSource);
         Object proxy = createProxy(beanClass, beanName, specificInterceptors, targetSource);
         this.proxyTypes.put(cacheKey, proxy.getClass());
         return proxy;
      }
   }

   return null;
}
```

##### 基础类检测

```java
//返回retVal,所有列举到类及其子类都被当成基础类
boolean retVal = Advice.class.isAssignableFrom(beanClass) ||
      Pointcut.class.isAssignableFrom(beanClass) ||
      Advisor.class.isAssignableFrom(beanClass) ||
      AopInfrastructureBean.class.isAssignableFrom(beanClass);
```

##### 跳过类检测

shouldSkip方法在子类`AspectJAwareAdvisorAutoProxyCreator`的实现

```java
protected boolean shouldSkip(Class<?> beanClass, String beanName) {
   // TODO: Consider optimization by caching the list of the aspect names
   List<Advisor> candidateAdvisors = findCandidateAdvisors();
   for (Advisor advisor : candidateAdvisors) {
      if (advisor instanceof AspectJPointcutAdvisor) {
         if (((AbstractAspectJAdvice) advisor.getAdvice()).getAspectName().equals(beanName)) {
            return true;
         }
      }
   }
   return super.shouldSkip(beanClass, beanName);
}
```

实际跳过的是`<aop:aspect ref="aopAdvice">`配置的ref类.Spring对于aop:config的解析其实是把aop:before/after等标签解析成为了AspectJPointcutAdvisor类型的BeanDefinition，而aopAdvice以AbstractAspectJAdvice的类型保存在其中。

所以可以得出结论: **Spring跳过的是适用于当前bean的Advisor的Advice/Aspect对象**。

###### ![AOP逻辑图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/aop_logic.jpg)

获取`Advisor`对象,方法调用 `AspectJAwareAdvisorAutoProxyCreator#findCandidateAdvisors`->`BeanFactoryAdvisorRetrievalHelper#findAdvisorBeans`

```java
public List<Advisor> findAdvisorBeans() {
   // Determine list of advisor bean names, if not cached already.
   String[] advisorNames = null;
   synchronized (this) {
      advisorNames = this.cachedAdvisorBeanNames;
      if (advisorNames == null) {
         // Do not initialize FactoryBeans here: We need to leave all regular beans
         // uninitialized to let the auto-proxy creator apply to them!
          //获取当前beanFactory中的所有advisor类
         advisorNames = BeanFactoryUtils.beanNamesForTypeIncludingAncestors(
               this.beanFactory, Advisor.class, true, false);
         this.cachedAdvisorBeanNames = advisorNames;
      }
   }
   if (advisorNames.length == 0) {
      return new LinkedList<Advisor>();
   }

   List<Advisor> advisors = new LinkedList<Advisor>();
   for (String name : advisorNames) {
       //判断是否有资格,默认为true,所以容器会对所有的Advisor的Advice进行跳过
      if (isEligibleBean(name)) {
         if (this.beanFactory.isCurrentlyInCreation(name)) {
         }
         else {         
           advisors.add(this.beanFactory.getBean(name, Advisor.class));        
         }
      }
   }
   return advisors;
}
```

回到`postProcessBeforeInstantiation`方法,对于检测通过的bean,如果targetBean是自定义对象则立即创建.

```java
//为 bean 实例创建目标源。如果设置，则使用任何 TargetSourceCreators。如果不应使用自定义 TargetSource，则返回null 
protected TargetSource getCustomTargetSource(Class<?> beanClass, String beanName) {
   // We can't create fancy target sources for directly registered singletons.
   if (this.customTargetSourceCreators != null &&
         this.beanFactory != null && this.beanFactory.containsBean(beanName)) {
      for (TargetSourceCreator tsc : this.customTargetSourceCreators) {
         TargetSource ts = tsc.getTargetSource(beanClass, beanName);
          if (ts != null) {
           return ts;
          }
         }
      }
   }

   // No custom TargetSource found.
//没有找到自定义对象
   return null;
}
```

TargetSource类图

![TargetSource.jpg](https://github.com/seaswalker/spring-analysis/blob/master/note/images/TargetSource.jpg?raw=true)

##### 为指定bean创建代理

方法createProxy

```java
protected Object createProxy(
      Class<?> beanClass, String beanName, Object[] specificInterceptors, TargetSource targetSource) {

   if (this.beanFactory instanceof ConfigurableListableBeanFactory) {
      AutoProxyUtils.exposeTargetClass((ConfigurableListableBeanFactory) this.beanFactory, beanName, beanClass);
   }

   ProxyFactory proxyFactory = new ProxyFactory();
   proxyFactory.copyFrom(this);

   if (!proxyFactory.isProxyTargetClass()) {
      if (shouldProxyTargetClass(beanClass, beanName)) {
         proxyFactory.setProxyTargetClass(true);
      }
      else {
         evaluateProxyInterfaces(beanClass, proxyFactory);
      }
   }
  //创建advisor	
   Advisor[] advisors = buildAdvisors(beanName, specificInterceptors);
   //设置基础属性
   proxyFactory.addAdvisors(advisors);
   proxyFactory.setTargetSource(targetSource);
   //空实现
   customizeProxyFactory(proxyFactory);

   proxyFactory.setFrozen(this.freezeProxy);
   if (advisorsPreFiltered()) {
      proxyFactory.setPreFiltered(true);
   }
   //此步骤调用DefaultAopProxyFactory.createAopProxy 然后利用生成类调用getProxy方法	
   return proxyFactory.getProxy(getProxyClassLoader());
}
```

生成对应的AopProxy代理类

```java
public AopProxy createAopProxy(AdvisedSupport config) throws AopConfigException {
    //如果指定了(proxy-target-classs设为true)使用Cglib，那么就会使用Cglib的方式，如果没有指定(或为false)，那么先回检测被代理类是否实现了自己的接口，如果实现了，那么就采用JDK动态代理的方式
   if (config.isOptimize() || config.isProxyTargetClass() || hasNoUserSuppliedProxyInterfaces(config)) {
      Class<?> targetClass = config.getTargetClass();
      if (targetClass.isInterface() || Proxy.isProxyClass(targetClass)) {
         return new JdkDynamicAopProxy(config);
      }
      return new ObjenesisCglibAopProxy(config);
   }
   else {
      return new JdkDynamicAopProxy(config);
   }
}
```



### postProcessAfterInitialization

AbstractAutoProxyCreator.postProcessAfterInitialization:

```java
public Object postProcessAfterInitialization(Object bean, String beanName) throws BeansException {
   if (bean != null) {
       //获取缓存键
      Object cacheKey = getCacheKey(bean.getClass(), beanName);
       //早起代理引用是否包含了改缓存键如果没有则包装
      if (!this.earlyProxyReferences.contains(cacheKey)) {
         return wrapIfNecessary(bean, beanName, cacheKey);
      }
   }
   return bean;
}
```

 `wrapIfNecessary`如有必要，包装给定的bean，即如果它有资格被代理。

```java
protected Object wrapIfNecessary(Object bean, String beanName, Object cacheKey) {
    //检测是否应该被包装
   if (beanName != null && this.targetSourcedBeans.contains(beanName)) {
      return bean;
   }
   if (Boolean.FALSE.equals(this.advisedBeans.get(cacheKey))) {
      return bean;
   }
   if (isInfrastructureClass(bean.getClass()) || shouldSkip(bean.getClass(), beanName)) {
      this.advisedBeans.put(cacheKey, Boolean.FALSE);
      return bean;
   }

   
   Object[] specificInterceptors = getAdvicesAndAdvisorsForBean(bean.getClass(), beanName, null);
   if (specificInterceptors != DO_NOT_PROXY) {
      this.advisedBeans.put(cacheKey, Boolean.TRUE);
       //创建代理
      Object proxy = createProxy(
            bean.getClass(), beanName, specificInterceptors, new SingletonTargetSource(bean));
      this.proxyTypes.put(cacheKey, proxy.getClass());
      return proxy;
   }

   this.advisedBeans.put(cacheKey, Boolean.FALSE);
   return bean;
}
```

`getAdvicesAndAdvisorsForBean`最终调用的是`AbstractAdvisorAutoProxyCreator.findEligibleAdvisors`

```java
//寻找Advisor
protected List<Advisor> findEligibleAdvisors(Class<?> beanClass, String beanName) {
    //findCandidateAdvisors在上文有做解析
   List<Advisor> candidateAdvisors = findCandidateAdvisors();
    //最终调用AopUtils.findAdvisorsThatCanApply
   List<Advisor> eligibleAdvisors = findAdvisorsThatCanApply(candidateAdvisors, beanClass, beanName);
   extendAdvisors(eligibleAdvisors);
   if (!eligibleAdvisors.isEmpty()) {
      eligibleAdvisors = sortAdvisors(eligibleAdvisors);
   }
   return eligibleAdvisors;
}
```

`AopUtils.findAdvisorsThatCanApply`方法

```java
//确定适用于给定类的candidateAdvisors列表的子列表。
public static List<Advisor> findAdvisorsThatCanApply(List<Advisor> candidateAdvisors, Class<?> clazz) {
   if (candidateAdvisors.isEmpty()) {
      return candidateAdvisors;
   }
   List<Advisor> eligibleAdvisors = new LinkedList<Advisor>();
   //优先处理 IntroductionAdvisor,导致IntroductionAdvisor在Advisor链中总是位于非IntroductionAdvisor前面。
   for (Advisor candidate : candidateAdvisors) {
       //canApply方法判断是否匹配条件
      if (candidate instanceof IntroductionAdvisor && canApply(candidate, clazz)) {
         eligibleAdvisors.add(candidate);
      }
   }
   boolean hasIntroductions = !eligibleAdvisors.isEmpty();
   for (Advisor candidate : candidateAdvisors) {
      if (candidate instanceof IntroductionAdvisor) {
         // already processed
         continue;
      }
      if (canApply(candidate, clazz, hasIntroductions)) {
         eligibleAdvisors.add(candidate);
      }
   }
   return eligibleAdvisors;
}
```

上述方法最终调用`canApply#hasIntroductions`默认传false

```java
public static boolean canApply(Advisor advisor, Class<?> targetClass, boolean hasIntroductions) {
    //引入的advisor如其他的advisor使用不同的逻辑
    if (advisor instanceof IntroductionAdvisor) {
        return ((IntroductionAdvisor) advisor).getClassFilter().matches(targetClass);
    }
    else if (advisor instanceof PointcutAdvisor) {
        PointcutAdvisor pca = (PointcutAdvisor) advisor;
        return canApply(pca.getPointcut(), targetClass, hasIntroductions);
    }
    else {
        // It doesn't have a pointcut so we assume it applies.
        return true;
    }
}
```

Spring的Pointcut由ClassFilter和MethodMatcher两部分组成，其中前者用以判断给定的类是否在Pointcut的匹配范围内，后者用以在ClassFilter匹配满足的情况下判断给定的方法是否在Pointcut匹配的范围内。

从源码中可以看出，如果ClassFilter匹配得到满足并且Pointcut并不能匹配此类的任意方法，便会**用反射的方法获取targetClass(被检测类)的全部方法逐一交由Pointcut的MethodMatcher进行检测**。

继续回到findEligibleAdvisors方法,之后调用extendAdvisors,扩展Advisor.

最终调用`AspectJProxyUtils.makeAdvisorChainAspectJCapableIfNecessary`方法.将会把一个ExposeInvocationInterceptor添加到链表的表头，目的在于将MethodInvocation以ThreadLocal的方式暴露给后面所有的Advisor.最后对sortAdvisors根据order接口进行排序.



#### 创建

##### JDK动态代理

JdkDynamicAopProxy.getProxy

```java
@Override
public Object getProxy(ClassLoader classLoader) {
    //获取代理接口类型
   Class<?>[] proxiedInterfaces = AopProxyUtils.completeProxiedInterfaces(this.advised, true);
   //判断是否重写了equals和hashCode方法 
   findDefinedEqualsAndHashCodeMethods(proxiedInterfaces);
    //创建代理
   return Proxy.newProxyInstance(classLoader, proxiedInterfaces, this);
}
```

invoke方法解析

```java
public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
   MethodInvocation invocation;
   Object oldProxy = null;
   boolean setProxyContext = false;

   TargetSource targetSource = this.advised.targetSource;
   Class<?> targetClass = null;
   Object target = null;

   try {
       //如果被代理类实现了equals或者是hashCode方法，那么生成的代理子类的equals、hashCode方法实际上执行的是JdkDynamicAopProxy相应方法的逻辑。
      if (!this.equalsDefined && AopUtils.isEqualsMethod(method)) {
         // The target does not implement the equals(Object) method itself.
         return equals(args[0]);
      }
      else if (!this.hashCodeDefined && AopUtils.isHashCodeMethod(method)) {
         // The target does not implement the hashCode() method itself.
         return hashCode();
      }
      else if (method.getDeclaringClass() == DecoratingProxy.class) {
         // There is only getDecoratedClass() declared -> dispatch to proxy config.
          //只有声明了 getDecoratedClass() -> 分派到代理配置。
         return AopProxyUtils.ultimateTargetClass(this.advised);
      }
      else if (!this.advised.opaque && method.getDeclaringClass().isInterface() &&
            method.getDeclaringClass().isAssignableFrom(Advised.class)) {
         // Service invocations on ProxyConfig with the proxy config...
          //使用代理配置对 ProxyConfig 进行服务调用
         return AopUtils.invokeJoinpointUsingReflection(this.advised, method, args);
      }

      Object retVal;
	  //如果advised被暴露
      if (this.advised.exposeProxy) {
         // 确保invocation能够被使用,保存当前代理类到threadLocal对象中
         oldProxy = AopContext.setCurrentProxy(proxy);
         setProxyContext = true;
      }

      // May be null. Get as late as possible to minimize the time we "own" the target,
      // in case it comes from a pool.
      target = targetSource.getTarget();
      if (target != null) {
         targetClass = target.getClass();
      }

      // Get the interception chain for this method.
      //链式调用
      List<Object> chain = this.advised.getInterceptorsAndDynamicInterceptionAdvice(method, targetClass);

      // Check whether we have any advice. If we don't, we can fallback on direct
      // reflective invocation of the target, and avoid creating a MethodInvocation.
      if (chain.isEmpty()) {
         // We can skip creating a MethodInvocation: just invoke the target directly
         // Note that the final invoker must be an InvokerInterceptor so we know it does
         // nothing but a reflective operation on the target, and no hot swapping or fancy proxying.
         //调用链为空直接调用invocation 
         Object[] argsToUse = AopProxyUtils.adaptArgumentsIfNecessary(method, args);
          //实际调用method.invoke(target, args) 方法
         retVal = AopUtils.invokeJoinpointUsingReflection(target, method, argsToUse);
      }
      else {
         // We need to create a method invocation...
         invocation = new ReflectiveMethodInvocation(proxy, target, method, args, targetClass, chain);
         // Proceed to the joinpoint through the interceptor chain.
         retVal = invocation.proceed();
      }
		
      // Massage return value if necessary
      //返回方法调用后的返回值
      Class<?> returnType = method.getReturnType();
      if (retVal != null && retVal == target &&
            returnType != Object.class && returnType.isInstance(proxy) &&
            !RawTargetAccess.class.isAssignableFrom(method.getDeclaringClass())) {
         // Special case: it returned "this" and the return type of the method
         // is type-compatible. Note that we can't help if the target sets
         // a reference to itself in another returned object.
         retVal = proxy;
      }
      else if (retVal == null && returnType != Void.TYPE && returnType.isPrimitive()) {
         throw new AopInvocationException(
               "Null return value from advice does not match primitive return type for: " + method);
      }
      return retVal;
   }
   finally {
      if (target != null && !targetSource.isStatic()) {
         // Must have come from TargetSource.
         targetSource.releaseTarget(target);
      }
      if (setProxyContext) {
         // Restore old proxy.
         AopContext.setCurrentProxy(oldProxy);
      }
   }
}
```

CGlib调用方式与JDK方式相似不再赘述.

# aop:scoped-proxy

示例

```xml
<bean id="userPreferences" class="com.foo.UserPreferences" scope="session">
    <aop:scoped-proxy/>
</bean>
<bean id="userManager" class="com.foo.UserManager">
    <property name="userPreferences" ref="userPreferences"/>
</bean>
```

对于ref属性，**只会在userManager初始化时注入一次**。这会造成什么问题呢?以session的Scope为例，因为只会注入一次，所以，**userManager引用的始终是同一个userPreferences对象，即使现在可能已经过时了**。此配置便可以使userManager引用的其实是一个对代理的引用，所以可以始终获取到最新的userPreferences。

等效注解 `@ComponentScan`可以设置`ScopedProxyMode`属性为`TARGET_CLASS`或者`INTERFACES`.

解析由ScopedProxyBeanDefinitionDecorator类完成.

![ScopedProxyBeanDefinitionDecorator类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/ScopedProxyBeanDefinitionDecorator.jpg)

## 解析

### 入口

#### 装饰

```java
public BeanDefinitionHolder decorate(Node node, BeanDefinitionHolder definition, ParserContext parserContext) {
    boolean proxyTargetClass = true;
    if (node instanceof Element) {
        Element ele = (Element) node;
        //解析proxy-target-class 属性
        if (ele.hasAttribute(PROXY_TARGET_CLASS)) {
            proxyTargetClass = Boolean.valueOf(ele.getAttribute(PROXY_TARGET_CLASS));
        }
    }
    BeanDefinitionHolder holder =
            ScopedProxyUtils.
            createScopedProxy(definition, parserContext.getRegistry(), proxyTargetClass);
    String targetBeanName = ScopedProxyUtils.getTargetBeanName(definition.getBeanName());
    // 空实现
    parserContext.getReaderContext().fireComponentRegistered(
            new BeanComponentDefinition(definition.getBeanDefinition(), targetBeanName));
    return holder;
}
```

核心便是createScopedProxy方法，其源码较长，但是这个套路之前见识过了，就是一个偷天换日: 创建一个新的BeanDefinition对象，beanName为被代理的bean的名字，被代理的bean名字为scopedTarget.原名字。被代理的bean扔将被注册到容器中。

## 代理生成

新的BeanDefintion的beanClass为ScopedProxyFactoryBean.调用setBeanFactory

```java
public void setBeanFactory(BeanFactory beanFactory) {

   ConfigurableBeanFactory cbf = (ConfigurableBeanFactory) beanFactory;

   this.scopedTargetSource.setBeanFactory(beanFactory);

   ProxyFactory pf = new ProxyFactory();
   pf.copyFrom(this);
   pf.setTargetSource(this.scopedTargetSource);

   Class<?> beanType = beanFactory.getType(this.targetBeanName);

   if (!isProxyTargetClass() || beanType.isInterface() || Modifier.isPrivate(beanType.getModifiers())) {
       // JDK动态代理可用的接口
      pf.setInterfaces(ClassUtils.getAllInterfacesForClass(beanType, cbf.getBeanClassLoader()));
   }

   // Add an introduction that implements only the methods on ScopedObject.
   ScopedObject scopedObject = new DefaultScopedObject(cbf, this.scopedTargetSource.getTargetBeanName());
    //调用AdvisedSupport.addAdvice
   pf.addAdvice(new DelegatingIntroductionInterceptor(scopedObject));

   // Add the AopInfrastructureBean marker to indicate that the scoped proxy
   // itself is not subject to auto-proxying! Only its target bean is.
   pf.addInterface(AopInfrastructureBean.class);

   this.proxy = pf.getProxy(cbf.getBeanClassLoader());
}
```

套路与createScopedProxy方法类似,上文详解了.

核心的拦截逻辑是通过DelegatingIntroductionInterceptor来完成的

![DelegatingIntroductionInterceptor类图](https://github.com/seaswalker/spring-analysis/raw/master/note/images/DelegatingIntroductionInterceptor.jpg)

`AdvisedSupport.addAdvice`方法实现

```java
public void addAdvice(int pos, Advice advice) throws AopConfigException {
   Assert.notNull(advice, "Advice must not be null");
   if (advice instanceof IntroductionInfo) {
      // We don't need an IntroductionAdvisor for this kind of introduction:
      // It's fully self-describing.
      //setBeanFactory调用会默认进入该分支 
      addAdvisor(pos, new DefaultIntroductionAdvisor(advice, (IntroductionInfo) advice));
   }
   else {
      addAdvisor(pos, new DefaultPointcutAdvisor(advice));
   }
}
```

Introduction:指的是在不修改类源码的情况下，**直接为一个类添加新的功能**

#### 原理

从根本上来说在于AbstractBeanFactory.doGetBean，部分源码:

```java
//scope非prototype和Singleton
else {
    String scopeName = mbd.getScope();
    final Scope scope = this.scopes.get(scopeName);
    Object scopedInstance = scope.get(beanName, new ObjectFactory<Object>() {
        @Override
        public Object getObject() throws BeansException {
            beforePrototypeCreation(beanName);
            try {
                return createBean(beanName, mbd, args);
            }
            finally {
                afterPrototypeCreation(beanName);
            }
        }
    });
    bean = getObjectForBeanInstance(scopedInstance, name, beanName, mbd);
}
```

scopes是BeanFactory内部的一个 LinkedHashMap<String, Scope>类型的对象。scope.get实际上调用的就是我们的OneSocpe的get方法，没有用到ObjectFactory。

所以，**每调用一次getBean，就会导致一个新的Sudent被创建并返回**。

**Callback是Cglib所有自定义逻辑(增强)的共同接口**。

[CGLIB介绍与原理](http://blog.csdn.net/zghwaicsdn/article/details/50957474)



# aop:aspectj-autoproxy

此标签用以开启对于`@AspectJ`注解风格AOP的支持。

**即使标注了@Aspect注解，仍然需要将切面自己配置到Spring容器中。**

## 解析

`AspectJAutoProxyBeanDefinitionParser.parse`

```java
public BeanDefinition parse(Element element, ParserContext parserContext) {

 //注册AspectJ注解的对象 ,初始化一个beanDefinition并注册
  AopNamespaceUtils.registerAspectJAnnotationAutoProxyCreatorIfNecessary(parserContext, element);
    //如果有子节点遍历并解析
   extendBeanDefinition(element, parserContext);
   return null;
}
```

利用AspectJ注解生成的代理子类方法与XML配置的类似,不同点在于体现注解特性.

`AnnotationAwareAspectJAutoProxyCreator#findCandidateAdvisors`方法寻找Advisor对象

```java
protected List<Advisor> findCandidateAdvisors() {
   // Add all the Spring advisors found according to superclass rules.
   List<Advisor> advisors = super.findCandidateAdvisors();
   // Build Advisors for all AspectJ aspects in the bean factory.
   //从容器中得到所有的bean，逐一判断是不是一个Aspect
   advisors.addAll(this.aspectJAdvisorsBuilder.buildAspectJAdvisors());
   return advisors;
}
```

## AOP切面的坑

1. 定义在private方法上的切面不会被执行，这个很容易理解，毕竟子类不能覆盖父类的私有方法。
2. 同一个代理子类内部的方法相互调用不会再次执行切面。



MethodProxy是Cglib对方法代理的抽象，这里的关键是**方法调用的对象(目标)是我们的原生类对象，而不是Cglib代理子类的对象，这就从根本上决定了对同类方法的调用不会再次经过切面**。