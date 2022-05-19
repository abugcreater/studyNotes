# Spring boot AOP

## 简介

在Spring boot中不需要像之前配置xml才能启用aop功能.

一般aop的使用方法如下:

```java
/**
定义一个切面类,将他纳入到spring生命周期中,
在这个类中定义一个切点Pointcut,里面描述了切面的范围
然后定义一个增强方法,在方法处理中使用
*/
@Service
@Aspect
public class LogAspect {

    @Pointcut("execution(* com.example.demo.service.PersonService.personName())")
    private void startName() {
    }

    @AfterReturning("startName()")
    public void doAfter(JoinPoint joinPoint){
        System.out.println("after name");
    }

}
```

## 具体处理方式

在springboot中如果使用AOP需要开启EnableAspectJAutoProxy注解,通过import中的认识,通常在`@Import`注解中引入需要注册的类.

```java
class AspectJAutoProxyRegistrar implements ImportBeanDefinitionRegistrar {

   @Override
   public void registerBeanDefinitions(
         AnnotationMetadata importingClassMetadata, BeanDefinitionRegistry registry) {

  //该方法注册bean 类型是AnnotationAwareAspectJAutoProxyCreator,名称是internalAutoProxyCreator
       AopConfigUtils.registerAspectJAnnotationAutoProxyCreatorIfNecessary(registry);

      AnnotationAttributes enableAspectJAutoProxy =
            AnnotationConfigUtils.attributesFor(importingClassMetadata, EnableAspectJAutoProxy.class);
      if (enableAspectJAutoProxy != null) {
          //是否为被代理这生成CGLIB子类，默认false
         if (enableAspectJAutoProxy.getBoolean("proxyTargetClass")) {
            AopConfigUtils.forceAutoProxyCreatorToUseClassProxying(registry);
         }
          //代表是否将代理bean暴露给用户，如果暴露，可以通过Spring AopContext类获得，默认不暴露。
         if (enableAspectJAutoProxy.getBoolean("exposeProxy")) {
            AopConfigUtils.forceAutoProxyCreatorToExposeProxy(registry);
         }
      }
   }

}
```

由AnnotationAwareAspectJAutoProxyCreator在创建bean的时候创建代理类

类图如下

![在这里插入图片描述](https://www.freesion.com/images/546/ba132389dc6044fd39346e025fa95642.png)

### 解析AnnotationAwareAspectJAutoProxyCreator

从上面的类图可见`AnnotationAwareAspectJAutoProxyCreator`类继承了`AbstractAutoProxyCreator`,而该类实现了BeanPostProcessor接口,在bean的初始化前后对bean进行了处理.BeanPostProcessor相关实现类在容器refresh时注册到了容器中.

```java
// Register bean processors that intercept bean creation.
registerBeanPostProcessors(beanFactory);
```

## 切面解析

那么切面是在什么时候被解析的呢?









参考:[【SPRING - AOP】--- AOP核心后置处理器INTERNALAUTOPROXYCREATOR的创建过程](https://www.freesion.com/article/2395397775/)

[【bean的生命周期】--- 对象创建+初始化流程分析 --- 【重点@Autowired的作用时机】](https://blog.csdn.net/nrsc272420199/article/details/103192081)

[Spring Aop中四个重要概念](https://blog.csdn.net/sinolover/article/details/104490634)

[解析@Aspect获取增强器](https://blog.csdn.net/xiaojie_570/article/details/104819921)