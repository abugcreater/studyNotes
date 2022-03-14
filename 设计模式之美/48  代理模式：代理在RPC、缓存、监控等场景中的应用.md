# 48 | 代理模式：代理在RPC、缓存、监控等场景中的应用

## 代理模式的原理与实现

代理模式(Proxy Design Pattern):在不改变原始类（或叫被代理类）的情况下，通过引入代理类来给原始类附加功能。

一般情况下，我们让代理类和原始类实现同样的接口。但是，如果原始类并没有定义接口，并且原始类代码并不是我们开发维护的。在这种情况下，我们可以通过让代理类继承原始类的方法来实现代理模式。

## 动态代理的原理与实现

静态代理需要针对每个类都创建一个代理类，并且每个代理类中的代码都有点像模板式的“重复”代码，增加了维护成本和开发成本。对于静态代理存在的问题，我们可以通过动态代理来解决。我们不事先为每个原始类编写代理类，而是在运行的时候动态地创建原始类对应的代理类，然后在系统中用代理类替换掉原始类。

## 代理模式的应用场景

1. 业务系统的非功能性需求开发

   比如：监控、统计、鉴权、限流、事务、幂等、日志。

2. 代理模式还可以用在 RPC、缓存等应用场景中

## 代码示例

```
public interface IUserController {
  UserVo login(String telephone, String password);
  UserVo register(String telephone, String password);
}
public class UserController implements IUserController {
  //...省略其他属性和方法...
  @Override
  public UserVo login(String telephone, String password) {
    //...省略login逻辑...
    //...返回UserVo数据...
  }
  @Override
  public UserVo register(String telephone, String password) {
    //...省略register逻辑...
    //...返回UserVo数据...
  }
}
public class UserControllerProxy implements IUserController {
  private MetricsCollector metricsCollector;
  private UserController userController;
  public UserControllerProxy(UserController userController) {
    this.userController = userController;
    this.metricsCollector = new MetricsCollector();
  }
  @Override
  public UserVo login(String telephone, String password) {
    long startTimestamp = System.currentTimeMillis();
    // 委托
    UserVo userVo = userController.login(telephone, password);
    long endTimeStamp = System.currentTimeMillis();
    long responseTime = endTimeStamp - startTimestamp;
    RequestInfo requestInfo = new RequestInfo("login", responseTime, startTimestamp);
    metricsCollector.recordRequest(requestInfo);
    return userVo;
  }
  @Override
  public UserVo register(String telephone, String password) {
    long startTimestamp = System.currentTimeMillis();
    UserVo userVo = userController.register(telephone, password);
    long endTimeStamp = System.currentTimeMillis();
    long responseTime = endTimeStamp - startTimestamp;
    RequestInfo requestInfo = new RequestInfo("register", responseTime, startTimestamp);
    metricsCollector.recordRequest(requestInfo);
    return userVo;
  }
}
//UserControllerProxy使用举例
//因为原始类和代理类实现相同的接口，是基于接口而非实现编程
//将UserController类对象替换为UserControllerProxy类对象，不需要改动太多代码
IUserController userController = new UserControllerProxy(new UserController());
```



```
public class MetricsCollectorProxy {
  private MetricsCollector metricsCollector;
  public MetricsCollectorProxy() {
    this.metricsCollector = new MetricsCollector();
  }
  public Object createProxy(Object proxiedObject) {
    Class<?>[] interfaces = proxiedObject.getClass().getInterfaces();
    DynamicProxyHandler handler = new DynamicProxyHandler(proxiedObject);
    return Proxy.newProxyInstance(proxiedObject.getClass().getClassLoader(), interfaces, handler);
  }
  private class DynamicProxyHandler implements InvocationHandler {
    private Object proxiedObject;
    public DynamicProxyHandler(Object proxiedObject) {
      this.proxiedObject = proxiedObject;
    }
    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
      long startTimestamp = System.currentTimeMillis();
      Object result = method.invoke(proxiedObject, args);
      long endTimeStamp = System.currentTimeMillis();
      long responseTime = endTimeStamp - startTimestamp;
      String apiName = proxiedObject.getClass().getName() + ":" + method.getName();
      RequestInfo requestInfo = new RequestInfo(apiName, responseTime, startTimestamp);
      metricsCollector.recordRequest(requestInfo);
      return result;
    }
  }
}
//MetricsCollectorProxy使用举例
MetricsCollectorProxy proxy = new MetricsCollectorProxy();
IUserController userController = (IUserController) proxy.createProxy(new UserControlle
```









https://www.jianshu.com/p/9bcac608c714

https://www.cnblogs.com/flyoung2008/p/3251148.html

http://www.noobyard.com/article/p-tnmulczu-my.html