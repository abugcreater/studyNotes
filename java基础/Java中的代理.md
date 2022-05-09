# Java中的代理

## 代理模式

**代理模式(proxy design pattern)**:属于结构型模式,通过引入代理类来给原始类附加功能.

结构型模式主要总结了一些类或对象组合在一起的经典结构，这些经典的结构可以解决特定应用场景的问题。

代理模式分为静态代理和动态代理.

### 静态代理

静态代理,多个类通过实现同一个接口,并组合类之间的调用关系,完成给原始类附加功能.

```java
//需要实现的接口
public interface IUserController {
  UserVo login(String telephone, String password);
}
//实际实现接口的类
public class UserController implements IUserController {
   @Override
  public UserVo login(String telephone, String password) {
    //...省略login逻辑...
    //...返回UserVo数据...
  } 
}
//添加附加功能,如示例中完成的是添加计时的操作
public class UserControllerProxy implements IUserController {
    private UserController userController;
  
  public UserControllerProxy(UserController userController) {
    this.userController = userController;
  }
    
  @Override
  public UserVo login(String telephone, String password) {
    long startTimestamp = System.currentTimeMillis();
    // 委托
    UserVo userVo = userController.login(telephone, password);
    long endTimeStamp = System.currentTimeMillis();
    return userVo;
  } 
}
```

除了实现接口的方式,我们也可以使用继承的方式实现代理模式,在遇到无法对原始类抽象接口时.利用super调用父类方法,此外在调用后添加自己需要实现的功能方法.

这种模式虽然好理解，但是缺点也很明显：

- 会存在大量的冗余的代理类，这里演示了2个接口，如果有10个接口，就必须定义10个代理类。
- 不易维护，一旦接口更改，代理类和目标类都需要更改。

#### 动态代理

**动态代理(Dynamic Proxy)**:不需要事先为每个原始类编写代理类,运行时动态创建对应的代理类,然后在系统中用代理类替换原始类.

```java
//统一代理类
public class JdkDynamic implements InvocationHandler {

    private Object target;

    public JdkDynamic(Object target) {
        this.target = target;
    }
	//核心方法,添加附加功能
    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        System.out.println("方法开始.....");
        Object restult = method.invoke(target, args);
        System.out.println("方法结束...");
        return restult;
    }
	//实际调用流程
    public static void main(String[] args) {

        JdkDynamic dynamic = new JdkDynamic(new Student());
        //参数 ClassLoader loader,Class<?>[] interfaces,InvocationHandler(将方法调用分派到的调用处理程序)
        Person student = (Person) Proxy.newProxyInstance(dynamic.getClass().getClassLoader(), new Class[]{Person.class}, dynamic);
        student.sayHello();
    }
}
//接口类
interface Person {
    void sayHello();
}
//实际实现类
class Student implements Person {
    @Override
    public void sayHello() {
        System.out.println("say hello ......");
    }
}
```

##### Proxy.newProxyInstance()

该方法用于生成一个实例对象的代理对象.

```java
/**
loder，选用的类加载器。因为代理的是Student，所以一般都会用加载Student的类加载器。
interfaces，被代理的类所实现的接口，这个接口可以是多个。
h，绑定代理类的一个方法。
*/
public static Object newProxyInstance(ClassLoader loader,
                                          Class<?>[] interfaces,
                                          InvocationHandler h)
        throws IllegalArgumentException
    {
        Objects.requireNonNull(h);
		//克隆接口
        final Class<?>[] intfs = interfaces.clone();
        final SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            checkProxyAccess(Reflection.getCallerClass(), loader, intfs);
        }

        /*
         * Look up or generate the designated proxy class.
          从缓存中获取到对应的class类
         */
        Class<?> cl = getProxyClass0(loader, intfs);

        /*
         * Invoke its constructor with the designated invocation handler.
         使用指定的调用处理程序调用其构造函数。
         */
        try {
            if (sm != null) {
                checkNewProxyPermission(Reflection.getCallerClass(), cl);
            }
			//获取构造方法
            final Constructor<?> cons = cl.getConstructor(constructorParams);
            final InvocationHandler ih = h;
            //如果不公开则修改修饰符为可见
            if (!Modifier.isPublic(cl.getModifiers())) {
                AccessController.doPrivileged(new PrivilegedAction<Void>() {
                    public Void run() {
                        cons.setAccessible(true);
                        return null;
                    }
                });
            }
            //构件代理类
            return cons.newInstance(new Object[]{h});
        } catch (IllegalAccessException|InstantiationException e) {
        	.....   
        }
    }
```

##### InvocationHandler

```java
/**
 * {@code InvocationHandler} is the interface implemented by
 * the <i>invocation handler</i> of a proxy instance.
 *
 * <p>Each proxy instance has an associated invocation handler.
 * When a method is invoked on a proxy instance, the method
 * invocation is encoded and dispatched to the {@code invoke}
 * method of its invocation handler.
 *
 * @author      Peter Jones
 * @see         Proxy
 * @since       1.3
 */
public interface InvocationHandler {

    //InvocationHandler作用就是，当代理对象的原本方法被调用的时候，会绑定执行一个方法，这个方法就是InvocationHandler里面定义的内容，同时会替代原本方法的结果返回。
	public Object invoke(Object proxy, Method method, Object[] args)
        throws Throwable;

}
```

InvocationHandler接收三个参数

- proxy，代理后的实例对象。
- method，对象被调用方法。
- args，调用时的参数。

如果在`invoke`方法中调用改成 

```java
Object restult = method.invoke(proxy, args);
```

会导致无线循环,为proxy是代理类的对象，当该对象方法被调用的时候，会触发InvocationHandler，而InvocationHandler里面又调用一次proxy里面的对象，所以会不停地循环调用。并且，proxy对应的方法是没有实现的。所以是会循环的不停报错

##### 动态代理的原理

就是把接口复制出来，通过这些接口和类加载器，拿到这个代理类cl。然后通过反射的技术复制拿到代理类的构造函数（这部分代码在Class类中的getConstructor0方法），最后通过这个构造函数new个一对象出来，同时用InvocationHandler绑定这个对象。





参考:[你真的完全了解Java动态代理吗？看这篇就够了](https://www.jianshu.com/p/95970b089360)

