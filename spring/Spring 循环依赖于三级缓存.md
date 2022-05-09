# Spring 循环依赖于三级缓存

## 主要流程

spring 主要的生命周期定义在refresh方法中,refresh方法会启动容器.

refresh中会调用finishBeanFactoryInitialization方法初始化除了lazy bean以外的bean,主要是调用`DefaultListableBeanFactory#preInstantiateSingletons`方法,会对所有扫描获取到的bean初始话.

## 三级缓存基本方法解析

AbstractBeanFactory#doGetBean方法是初始化bean的基本方法.

```java
//只显示关键的创建bean及从缓存获取方法,其他省略
protected <T> T doGetBean(final String name, @Nullable final Class<T> requiredType,
      @Nullable final Object[] args, boolean typeCheckOnly) throws BeansException {
      	//进入方法先去缓存汇中找对应的bean
      	Object sharedInstance = getSingleton(beanName);
		if (sharedInstance != null && args == null) {
			bean = getObjectForBeanInstance(sharedInstance, name, beanName, null);
		}else{
            if (mbd.isSingleton()) {
                //创建对象
					sharedInstance = getSingleton(beanName, () -> {
							return createBean(beanName, mbd, args);
					});
					bean = getObjectForBeanInstance(sharedInstance, name, beanName, mbd);
				}
        } 
      
}
```

重头戏,三级缓存,解决循环依赖的方法 `DefaultSingletonBeanRegistry#getSingleton`

参数:

beanName – 要查找的 bean 的名称
allowEarlyReference – 是否应该创建早期引用(用于解决循环引用)

```java
protected Object getSingleton(String beanName, boolean allowEarlyReference) {
   Object singletonObject = this.singletonObjects.get(beanName);
   if (singletonObject == null && isSingletonCurrentlyInCreation(beanName)) {
      synchronized (this.singletonObjects) {
         singletonObject = this.earlySingletonObjects.get(beanName);
         if (singletonObject == null && allowEarlyReference) {
            ObjectFactory<?> singletonFactory = this.singletonFactories.get(beanName);
            if (singletonFactory != null) {
               singletonObject = singletonFactory.getObject();
               this.earlySingletonObjects.put(beanName, singletonObject);
               this.singletonFactories.remove(beanName);
            }
         }
      }
   }
   return singletonObject;
}
```

第一级缓存

```java
//存储关系 beanName:bean bean名称与bean实例映射存储
private final Map<String, Object> singletonObjects = new ConcurrentHashMap<>(256);
```

创建单例bean时调用的getSingleton方法时会将新建的bean维护进该缓存

```java
public Object getSingleton(String beanName, ObjectFactory<?> singletonFactory){
    singletonObject = singletonFactory.getObject();
	newSingleton = true;
    if (newSingleton) {
		addSingleton(beanName, singletonObject);
	}
}
```

当单例创建完成后会维护缓存结构

```java
protected void addSingleton(String beanName, Object singletonObject) {
   synchronized (this.singletonObjects) {
       //新增一级缓存
      this.singletonObjects.put(beanName, singletonObject);
       //删除二级缓存
      this.singletonFactories.remove(beanName);
       //删除三级缓存
      this.earlySingletonObjects.remove(beanName);
      this.registeredSingletons.add(beanName);
   }
}
```



如果第一级缓存没有获取到创建完成的bean实例则从第二级缓存中获取

```java
//存储关系 beanName:bean bean名称与bean实例映射存储,存储早期引用的beanmap 
//早期（半成品）对象
private final Map<String, Object> earlySingletonObjects = new HashMap<>(16);
```

该缓存维护在同方法中

```java
if (singletonFactory != null) {
   //如果第三级缓存不为空,那么调用 ObjectFactory.getObject方法,通常会获取一个半成品
    //ObjectFactory是一个函数接口,接口中只定义了一个方法,通常作为一个匿名内部类传参,调用getObject时是回调该方法,此处执行的是getEarlyBeanReference方法
   singletonObject = singletonFactory.getObject();
   this.earlySingletonObjects.put(beanName, singletonObject);
   this.singletonFactories.remove(beanName);
}
```

如果第二级缓存中为获取到实例,且该实例支持提前引用,那么从第三级缓存中获取

```java
//存储关系 beanName:bean bean名称与ObjectFactory实例映射存储,存储提前暴露的beanmap
private final Map<String, ObjectFactory<?>> singletonFactories = new HashMap<>(16);
```

该缓存维护在

```java
protected void addSingletonFactory(String beanName, ObjectFactory<?> singletonFactory) {
   synchronized (this.singletonObjects) {
       //当bean没有创建成功时
      if (!this.singletonObjects.containsKey(beanName)) {
          //将ObjectFactory,设置到三级缓存中
         this.singletonFactories.put(beanName, singletonFactory);
         this.earlySingletonObjects.remove(beanName);
         this.registeredSingletons.add(beanName);
      }
   }
}
```

该方法是createBean时如果有循环引用且尝试解时会调用

```java
boolean earlySingletonExposure = (mbd.isSingleton() && this.allowCircularReferences &&
      isSingletonCurrentlyInCreation(beanName));
if (earlySingletonExposure) {

   addSingletonFactory(beanName, () -> getEarlyBeanReference(beanName, mbd, bean));
}
```

此处ObjectFactory执行的方法为,

```java
protected Object getEarlyBeanReference(String beanName, RootBeanDefinition mbd, Object bean) {
   Object exposedObject = bean;
    //该bean是否应用与单例
   if (!mbd.isSynthetic() && hasInstantiationAwareBeanPostProcessors()) {
      for (BeanPostProcessor bp : getBeanPostProcessors()) {
         if (bp instanceof SmartInstantiationAwareBeanPostProcessor) {
            SmartInstantiationAwareBeanPostProcessor ibp = (SmartInstantiationAwareBeanPostProcessor) bp;
             //获取对指定 bean 的早期访问的引用，通常用于解析循环引用
            exposedObject = ibp.getEarlyBeanReference(exposedObject, beanName);
         }
      }
   }
   return exposedObject;
}
```

## Bean初始化过程

bean的创建基本方法是 (当bean没有任何依赖于被依赖)
`AbstractBeanFactory#doGetBean`,主要流程是

1. 先调用`getSingleton(string name)`从缓存中获取,此时获取失败返回空值,此时进入创建流程.
2. 创建之前标记该bean为被创建`Set<String> alreadyCreated`,表示已经至少倍创建一次
3. 在创建前将该bean放入`Set<String> singletonsCurrentlyInCreation`缓存,该缓存存放的是正在创建的bean名称
4. 进入`doCreateBean`方法将该bean加入到三级缓存中
5. 创建完bean会维护缓存,删除二三级缓存,加入一级缓存,并将bean从`singletonsCurrentlyInCreation`缓存中移除

如果没有引用的话,该bean的初始化从头到尾都没有涉及到二三级缓存的获取.

非循环引用时,A在初始化时,如果需要注入B,那么就会调用`doGetBean(b)`方法,此时与正常初始化B一致.

循环引用回导致doGetBean->A->B->A,那么此时会调用三级缓存中的`getEarlyBeanReference`方法初始化一个半成品.提前结束初始化流程.

### 为什么需要三级缓存

#### 三级循环流程示意图

![循环依赖.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/271af8606b794d5f9bcd04a227459925~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp)



#### 二级缓存解决循环依赖问题

二级缓存能解决正常的循环依赖问题,但是当存在代理类时会出现问题.

```java
protected Object getEarlyBeanReference(String beanName, RootBeanDefinition mbd, Object bean) {
   Object exposedObject = bean;
   if (!mbd.isSynthetic() && hasInstantiationAwareBeanPostProcessors()) {
      for (BeanPostProcessor bp : getBeanPostProcessors()) {
         if (bp instanceof SmartInstantiationAwareBeanPostProcessor) {
            SmartInstantiationAwareBeanPostProcessor ibp = (SmartInstantiationAwareBeanPostProcessor) bp;
             //如果需要代理则进入代理方法中AbstractAutoProxyCreator#getEarlyBeanReference
            exposedObject = ibp.getEarlyBeanReference(exposedObject, beanName);
         }
      }
   }
   return exposedObject;
}
```

代理类中的获取引用

```java
public Object getEarlyBeanReference(Object bean, String beanName) {
   Object cacheKey = getCacheKey(bean.getClass(), beanName);
    //放入早暴露的代理对象缓存中
   this.earlyProxyReferences.put(cacheKey, bean);
    //如果需要代理则更新proxyTypes和advisedBeans缓存,并返回代理对象
   return wrapIfNecessary(bean, beanName, cacheKey);
}
```

当 A,B循环引用,且B为代理对象时.

如果正常使用二级缓存,那么注入的类是非代理类.那么即使代理类生成后,也无法在调用.在A调用B的时候,只是调用正常的B功能方法,该方法无法被增强.







参考:[【spring源码】spring中bean的创建流程，三级缓存以及循环依赖问题](https://juejin.cn/post/6969034239199150087)

[曹工说Spring Boot源码（29）-- Spring 解决循环依赖为什么使用三级缓存，而不是二级缓存 ](https://www.cnblogs.com/grey-wolf/p/13034371.html)

[一文告诉你Spring是如何利用“三级缓存“巧妙解决Bean的循环依赖问题的【享学Spring】](https://blog.csdn.net/f641385712/article/details/92801300)