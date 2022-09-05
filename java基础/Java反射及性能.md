# Java反射及性能

## 代码分析

```java
public class TestRef {
    public static void main(String[] args) {
        try {
            Class<?> clazz = Class.forName("com.example.reflect.CommonTestEntity");
            Object refTest = clazz.newInstance();
            Method method = clazz.getMethod("defaultMethod");
            //Method method1 = clazz.getDeclaredMethod("defaultMethod");
            method.invoke(refTest);
        } catch (ClassNotFoundException | InstantiationException | IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            e.printStackTrace();
        }
    }
}
-------------------------------------------------------------------
public class CommonTestEntity {
    static {
        System.out.println("CommonTestEntity执行类加载...");
    }
    public CommonTestEntity() {
        System.out.println(this.getClass() + " ｜ CommonTestEntity实例初始化 | " + this.getClass().getClassLoader());
    }
    public void defaultMethod() {
        System.out.println("执行实例方法：defaultMethod");
    }
}
```

上述代码反射机制执行的流程如下图所示:

![img](https://pdai.tech/_images/java/java-basic-reflection-1.png)

### 反射获取类实例

首先调用class.forName方法获取类对象

```java
public static Class<?> forName(String className)
            throws ClassNotFoundException {
    //返回调用此方法的方法的调用者的类，进而获取对应的类加载器
    Class<?> caller = Reflection.getCallerClass();
    //调用native获取class信息
    return forName0(className, true, ClassLoader.getClassLoader(caller), caller);
}
```

主要是先获取 ClassLoader, 然后调用 native 方法，获取信息，加载类则是回调 `java.lang.ClassLoader`.然后通过类加载器进行类加载.

```java
著作权归https://pdai.tech所有。
链接：https://pdai.tech/md/java/basic/java-basic-x-reflection.html

protected Class<?> loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        // 先获取锁
        synchronized (getClassLoadingLock(name)) {
            // First, check if the class has already been loaded
            // 如果已经加载了的话，就不用再加载了
            Class<?> c = findLoadedClass(name);
            if (c == null) {
                long t0 = System.nanoTime();
                try {
                    // 双亲委托加载
                    if (parent != null) {
                        c = parent.loadClass(name, false);
                    } else {
                        c = findBootstrapClassOrNull(name);
                    }
                } catch (ClassNotFoundException e) {
                    // ClassNotFoundException thrown if class not found
                    // from the non-null parent class loader
                }

                // 父类没有加载到时，再自己加载
                if (c == null) {
                    // If still not found, then invoke findClass in order
                    // to find the class.
                    long t1 = System.nanoTime();
                    c = findClass(name);

                    // this is the defining class loader; record the stats
                    sun.misc.PerfCounter.getParentDelegationTime().addTime(t1 - t0);
                    sun.misc.PerfCounter.getFindClassTime().addElapsedTimeFrom(t1);
                    sun.misc.PerfCounter.getFindClasses().increment();
                }
            }
            if (resolve) {
                resolveClass(c);
            }
            return c;
        }
    }
    
    protected Object getClassLoadingLock(String className) {
        Object lock = this;
        if (parallelLockMap != null) {
            // 使用 ConcurrentHashMap来保存锁
            Object newLock = new Object();
            lock = parallelLockMap.putIfAbsent(className, newLock);
            if (lock == null) {
                lock = newLock;
            }
        }
        return lock;
    }
    
    protected final Class<?> findLoadedClass(String name) {
        if (!checkName(name))
            return null;
        return findLoadedClass0(name);
    }
```

然后在看**newInstance()** 的实现方式

```java
public T newInstance()
    throws InstantiationException, IllegalAccessException
{
    //校验权限
    if (System.getSecurityManager() != null) {
        checkMemberAccess(Member.PUBLIC, Reflection.getCallerClass(), false);
    }
    // 找构造器
    if (cachedConstructor == null) {
        if (this == Class.class) {
            //不允许对Class类使用该方法
            throw new IllegalAccessException(
                "Can not call newInstance() on the Class for java.lang.Class"
            );
        }
        try {
            //获取无参构造器
            Class<?>[] empty = {};
            final Constructor<T> c = getConstructor0(empty, Member.DECLARED);
            //安全校验
            java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<Void>() {
                    public Void run() {
                            c.setAccessible(true);
                            return null;
                        }
                    });
            cachedConstructor = c;
        } catch (NoSuchMethodException e) {
            throw (InstantiationException)
                new InstantiationException(getName()).initCause(e);
        }
    }
    Constructor<T> tmpConstructor = cachedConstructor;
    // Security check (same as in java.lang.reflect.Constructor)
    int modifiers = tmpConstructor.getModifiers();
    if (!Reflection.quickCheckMemberAccess(this, modifiers)) {
        Class<?> caller = Reflection.getCallerClass();
        if (newInstanceCallerCache != caller) {
            Reflection.ensureMemberAccess(caller, this, null, modifiers);
            newInstanceCallerCache = caller;
        }
    }
    // Run constructor
    try {
        //调用无参构造器
        return tmpConstructor.newInstance((Object[])null);
    } catch (InvocationTargetException e) {
        Unsafe.getUnsafe().throwException(e.getTargetException());
        // Not reached
        return null;
    }
}
```

Class类中的newInstance() 主要做了三件事：

1. 权限检测，如果不通过直接抛出异常；

2. 查找无参构造器，并将其缓存起来；
3. 调用具体方法的无参构造方法，生成实例并返回；

下面是获取构造器的过程：

```java
private Constructor<T> getConstructor0(Class<?>[] parameterTypes,
                                    int which) throws NoSuchMethodException
{
    //通过privateGetDeclaredConstructors获取所有构造器
    Constructor<T>[] constructors = privateGetDeclaredConstructors((which == Member.PUBLIC));
    //遍历构造器寻找合适的并拷贝
    for (Constructor<T> constructor : constructors) {
        if (arrayContentsEq(parameterTypes,
                            constructor.getParameterTypes())) {
            //实际实现内容为new一个元素一致的构造器
            return getReflectionFactory().copyConstructor(constructor);
        }
    }
    throw new NoSuchMethodException(getName() + ".<init>" + argumentTypesToString(parameterTypes));
}
```

privateGetDeclaredConstructors方法实现:

```java
private Constructor<T>[] privateGetDeclaredConstructors(boolean publicOnly) {
    checkInitted();
    Constructor<T>[] res;
    ReflectionData<T> rd = reflectionData();
    //缓存不为空直接返回缓存
    if (rd != null) {
        res = publicOnly ? rd.publicConstructors : rd.declaredConstructors;
        if (res != null) return res;
    }
    // 缓存为空需要从jvm中捞一遍数据
    if (isInterface()) {
        @SuppressWarnings("unchecked")
        Constructor<T>[] temporaryRes = (Constructor<T>[]) new Constructor<?>[0];
        res = temporaryRes;
    } else {
        //使用native方法从jvm获取构造器
        res = getDeclaredConstructors0(publicOnly);
    }
    //更新缓存
    if (rd != null) {
        if (publicOnly) {
            rd.publicConstructors = res;
        } else {
            rd.declaredConstructors = res;
        }
    }
    return res;
}



// Lazily create and cache ReflectionData
    private ReflectionData<T> reflectionData() {
        SoftReference<ReflectionData<T>> reflectionData = this.reflectionData;
        int classRedefinedCount = this.classRedefinedCount;
        ReflectionData<T> rd;
        if (useCaches &&
            reflectionData != null &&
            (rd = reflectionData.get()) != null &&
            rd.redefinedCount == classRedefinedCount) {
            return rd;
        }
        // else no SoftReference or cleared SoftReference or stale ReflectionData
        // -> create and replace new instance
        return newReflectionData(reflectionData, classRedefinedCount);
    }
    
    // 新创建缓存，保存反射信息
    private ReflectionData<T> newReflectionData(SoftReference<ReflectionData<T>> oldReflectionData,
                                                int classRedefinedCount) {
        if (!useCaches) return null;

        // 使用cas保证更新的线程安全性，所以反射是保证线程安全的
        while (true) {
            ReflectionData<T> rd = new ReflectionData<>(classRedefinedCount);
            // try to CAS it...
            if (Atomic.casReflectionData(this, oldReflectionData, new SoftReference<>(rd))) {
                return rd;
            }
            // 先使用CAS更新，如果更新成功，则立即返回，否则测查当前已被其他线程更新的情况，如果和自己想要更新的状态一致，则也算是成功了
            oldReflectionData = this.reflectionData;
            classRedefinedCount = this.classRedefinedCount;
            if (oldReflectionData != null &&
                (rd = oldReflectionData.get()) != null &&
                rd.redefinedCount == classRedefinedCount) {
                return rd;
            }
        }
    }
```

privateGetDeclaredConstructors(), 获取所有的构造器主要步骤；

1. 先尝试从缓存中获取；
2. 如果缓存没有，则从jvm中重新获取，并存入缓存，缓存使用软引用进行保存，保证内存可用；

另外，使用 relactionData() 进行缓存保存；ReflectionData 的数据结构如下

```java
// reflection data that might get invalidated when JVM TI RedefineClasses() is called
private static class ReflectionData<T> {
    volatile Field[] declaredFields;
    volatile Field[] publicFields;
    volatile Method[] declaredMethods;
    volatile Method[] publicMethods;
    volatile Constructor<T>[] declaredConstructors;
    volatile Constructor<T>[] publicConstructors;
    // Intermediate results for getFields and getMethods
    volatile Field[] declaredPublicFields;
    volatile Method[] declaredPublicMethods;
    volatile Class<?>[] interfaces;

    // Value of classRedefinedCount when we created this ReflectionData instance
    final int redefinedCount;

    ReflectionData(int redefinedCount) {
        this.redefinedCount = redefinedCount;
    }
}
```
`ReflectionData `这个属性主要是SoftReference的，也就是在某些内存比较苛刻的情况下是可能被回收的，不过正常情况下可以通过`-XX:SoftRefLRUPolicyMSPerMB`这个参数来控制回收的时机，一旦时机到了，只要GC发生就会将其回收，那回收之后意味着再有需求的时候要重新创建一个这样的对象，同时也需要从JVM里重新拿一份数据，那这个数据结构关联的Method，Field字段等都是重新生成的对象。

接下来就只需调用其相应构造器的 newInstance()，即返回实例了

```java
public T newInstance(Object ... initargs)
        throws InstantiationException, IllegalAccessException,
               IllegalArgumentException, InvocationTargetException
    {
        if (!override) {
            //校验权限
            if (!Reflection.quickCheckMemberAccess(clazz, modifiers)) {
                Class<?> caller = Reflection.getCallerClass();
                checkAccess(caller, clazz, null, modifiers);
            }
        }
        if ((clazz.getModifiers() & Modifier.ENUM) != 0)
            throw new IllegalArgumentException("Cannot reflectively create enum objects");
        ConstructorAccessor ca = constructorAccessor;   // read volatile
        if (ca == null) {
            ca = acquireConstructorAccessor();
        }
        @SuppressWarnings("unchecked")
        //最后调用native方法创建构造器           
        T inst = (T) ca.newInstance(initargs);
        return inst;
    }
```

### 反射获取方法

 这里有两种获取方法的方式:

```java
public Method getMethod(String name, Class<?>... parameterTypes)
        throws NoSuchMethodException, SecurityException {
        Objects.requireNonNull(name);
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            // 1. 检查方法权限
            checkMemberAccess(sm, Member.PUBLIC, Reflection.getCallerClass(), true);
        }
        // 2. 获取方法
        Method method = getMethod0(name, parameterTypes);
        if (method == null) {
            throw new NoSuchMethodException(methodToString(name, parameterTypes));
        }
        // 3. 返回方法
        return method;
    }
---------------------------------------------------------------------------------------
public Method getDeclaredMethod(String name, Class<?>... parameterTypes)
        throws NoSuchMethodException, SecurityException {
        Objects.requireNonNull(name);
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            // 1. 检查方法是权限
            checkMemberAccess(sm, Member.DECLARED, Reflection.getCallerClass(), true);
        }
        // 2. 获取方法
        Method method = searchMethods(privateGetDeclaredMethods(false), name, parameterTypes);
        if (method == null) {
            throw new NoSuchMethodException(methodToString(name, parameterTypes));
        }
        // 3. 返回方法
        return method;
}
```

这两个方式的主要区别是`getDeclaredMethod`会查询本类中所有方法(包括private,default不包括父类等),而`getMethod`会获取包括父类中的所有public方法.

**getMethod 及 getDeclaredMethod方法流程**

![img](https://a.perfma.net/img/3020409)



```java
//根据修饰符获取方法列表
private Method[] privateGetDeclaredMethods(boolean publicOnly) {
    checkInitted();
    Method[] res;
    ReflectionData<T> rd = reflectionData();
    if (rd != null) {
        //缓存不为空直接返回缓存
        res = publicOnly ? rd.declaredPublicMethods : rd.declaredMethods;
        if (res != null) return res;
    }
    // 没有缓存则从JVM中请求信息
    res = Reflection.filterMethods(this, getDeclaredMethods0(publicOnly));
    if (rd != null) {
        if (publicOnly) {
            rd.declaredPublicMethods = res;
        } else {
            rd.declaredMethods = res;
        }
    }
    return res;
}
```

方法列表需要进行过滤 Reflection.filterMethods;当然后面看来，这个方法我们一般不会派上用场。

```java
// sun.misc.Reflection
public static Method[] filterMethods(Class<?> containingClass, Method[] methods) {
    if (methodFilterMap == null) {
        // Bootstrapping
        return methods;
    }
    return (Method[])filter(methods, methodFilterMap.get(containingClass));
}
// 可以过滤指定的方法，一般为空，如果要指定过滤，可以调用 registerMethodsToFilter(), 或者...
private static Member[] filter(Member[] members, String[] filteredNames) {
    if ((filteredNames == null) || (members.length == 0)) {
        return members;
    }
    int numNewMembers = 0;
    for (Member member : members) {
        boolean shouldSkip = false;
        for (String filteredName : filteredNames) {
            if (member.getName() == filteredName) {
                shouldSkip = true;
                break;
            }
        }
        if (!shouldSkip) {
            ++numNewMembers;
        }
    }
    Member[] newMembers =
        (Member[])Array.newInstance(members[0].getClass(), numNewMembers);
    int destIdx = 0;
    for (Member member : members) {
        boolean shouldSkip = false;
        for (String filteredName : filteredNames) {
            if (member.getName() == filteredName) {
                shouldSkip = true;
                break;
            }
        }
        if (!shouldSkip) {
            newMembers[destIdx++] = member;
        }
    }
    return newMembers;
}
```
**根据方法名和参数类型过滤指定方法返回**

```java
private static Method searchMethods(Method[] methods,
                                    String name,
                                    Class<?>[] parameterTypes)
{
    Method res = null;
    // 使用常量池，避免重复创建String
    String internedName = name.intern();
    for (int i = 0; i < methods.length; i++) {
        Method m = methods[i];
        if (m.getName() == internedName
            && arrayContentsEq(parameterTypes, m.getParameterTypes())
            && (res == null
                || res.getReturnType().isAssignableFrom(m.getReturnType())))
            res = m;
    }

    return (res == null ? res : getReflectionFactory().copyMethod(res));
}
```
- 这里是匹配最精确的子类进行返回（最优匹配）
- 最后，还是通过 ReflectionFactory, copy 方法后返回

### 调用 method.invoke() 方法

```java
@CallerSensitive
public Object invoke(Object obj, Object... args)
    throws IllegalAccessException, IllegalArgumentException,
       InvocationTargetException
{
    if (!override) {
        //校验权限
        if (!Reflection.quickCheckMemberAccess(clazz, modifiers)) {
            Class<?> caller = Reflection.getCallerClass();
            checkAccess(caller, clazz, obj, modifiers);
        }
    }
           //获取MethodAccessor
    MethodAccessor ma = methodAccessor;             // read volatile
    if (ma == null) {
        ma = acquireMethodAccessor();
    }
           //调用MethodAccessor.invoke
    return ma.invoke(obj, args);
}
```
invoke时，是通过 MethodAccessor 进行调用的，而 MethodAccessor 是个接口，在第一次时调用 acquireMethodAccessor() 进行新创建。

```java
private MethodAccessor acquireMethodAccessor() {
        // First check to see if one has been created yet, and take it
        // if so
        MethodAccessor tmp = null;
        if (root != null) tmp = root.getMethodAccessor();
        if (tmp != null) {
            methodAccessor = tmp;
        } else {
            // Otherwise fabricate one and propagate it up to the root
            tmp = reflectionFactory.newMethodAccessor(this);
            setMethodAccessor(tmp);
        }

        return tmp;
    }
```

每个实际的Java方法只有一个对应的Method对象作为root（实质上就是Method类的一个成员变量）。每次在通过反射获取Method对象时新创建Method对象把root封装起来。在第一次调用一个实际Java方法对应得Method对象的invoke()方法之前，实现调用逻辑的MethodAccessor对象是第一次调用时才会新建并更新给root，然后调用MethodAccessor.invoke()真正完成反射调用.

MethodAccessor本身就是一个接口

```java
public interface MethodAccessor {
    /** Matches specification in {@link java.lang.reflect.Method} */
    public Object invoke(Object obj, Object[] args)
        throws IllegalArgumentException, InvocationTargetException;
}
```

其主要有三种实现

- DelegatingMethodAccessorImpl
- NativeMethodAccessorImpl
- GeneratedMethodAccessorXXX

其中DelegatingMethodAccessorImpl是最终注入给Method的methodAccessor的，也就是某个Method的所有的invoke方法都会调用到这个DelegatingMethodAccessorImpl.invoke，正如其名一样的，是做代理的，也就是真正的实现可以是下面的两种:

```java
class DelegatingMethodAccessorImpl extends MethodAccessorImpl {
    private MethodAccessorImpl delegate;

    DelegatingMethodAccessorImpl(MethodAccessorImpl delegate) {
        setDelegate(delegate);
    }

    public Object invoke(Object obj, Object[] args)
        throws IllegalArgumentException, InvocationTargetException
    {
        return delegate.invoke(obj, args);
    }

    void setDelegate(MethodAccessorImpl delegate) {
        this.delegate = delegate;
    }
}
```

NativeMethodAccessorImpl主要通过jvm实现,而GeneratedMethodAccessorXXX是为每个需要反射调用的Method动态生成的类，后的XXX是一个数字，不断递增.优先使用native,默认15次后才生成GeneratedMethodAccessorXXX.

```java
public Object invoke(Object obj, Object[] args)
    throws IllegalArgumentException, InvocationTargetException
{
    // We can't inflate methods belonging to vm-anonymous classes because
    // that kind of class can't be referred to by name, hence can't be
    // found from the generated bytecode.
    if (++numInvocations > ReflectionFactory.inflationThreshold()
            && !ReflectUtil.isVMAnonymousClass(method.getDeclaringClass())) {
        MethodAccessorImpl acc = (MethodAccessorImpl)
            new MethodAccessorGenerator().
                generateMethod(method.getDeclaringClass(),
                               method.getName(),
                               method.getParameterTypes(),
                               method.getReturnType(),
                               method.getExceptionTypes(),
                               method.getModifiers());
        parent.setDelegate(acc);
    }
    return invoke0(method, obj, args);
}
```

ReflectionFactory.inflationThreshold()默认为15,可以通过-Dsun.reflect.inflationThreshold=xxx来指定,还可以通过-Dsun.reflect.noInflation=true来直接绕过上面的15次NativeMethodAccessorImpl调用.

**生成的GeneratedMethodAccessorXXX大致结构**

![从一起GC血案谈到反射原理数据图表-heapdump性能社区](http://img.mp.itc.cn/upload/20170112/b383e1aa05af49e7b3e3a679ef1d0a24_th.jpeg)

**GeneratedMethodAccessorXXX的类加载器**

![从一起GC血案谈到反射原理数据图表-heapdump性能社区](http://img.mp.itc.cn/upload/20170112/4fc44e4335ee498d8dd27d437adb9b04_th.jpeg)

所以GeneratedMethodAccessorXXX的类加载器其实是一个DelegatingClassLoader类加载器

之所以搞一个新的类加载器，是为了性能考虑，在某些情况下可以卸载这些生成的类，因为类的卸载是只有在类加载器可以被回收的情况下才会被回收的，如果用了原来的类加载器，那可能导致这些新创建的类一直无法被卸载，从其设计来看本身就不希望他们一直存在内存里的，在需要的时候有就行了，在内存紧俏的时候可以释放掉内存

**并发缺点**

如果并发很高的时候，是不是意味着可能同时有很多线程进入到创建GeneratedMethodAccessorXXX类的逻辑里，虽然说最终使用的其实只会有一个，但是这些开销是不是已然存在了，假如有1000个线程都进入到创建GeneratedMethodAccessorXXX的逻辑里，那意味着多创建了999个无用的类，这些类会一直占着内存，直到能回收Perm的GC发生才会回收



### 反射调用流程小结

最后，用几句话总结反射的实现原理：

1. 反射类及反射方法的获取，都是通过从列表中搜寻查找匹配的方法，所以查找性能会随类的大小方法多少而变化；
2. 每个类都会有一个与之对应的Class实例，从而每个类都可以获取method反射方法，并作用到其他实例身上；
3. 反射也是考虑了线程安全的，放心使用；
4. 反射使用软引用relectionData缓存class信息，避免每次重新从jvm获取带来的开销；
5. 反射调用多次生成新代理Accessor, 而通过字节码生存的则考虑了卸载功能，所以会使用独立的类加载器；
6. 当找到需要的方法，都会copy一份出来，而不是使用原来的实例，从而保证数据隔离；
7. 调度反射方法，最终是由jvm执行invoke0()执行；







参考: [Java 基础 - 反射机制详解](https://pdai.tech/md/java/basic/java-basic-x-reflection.html#%E5%8F%8D%E5%B0%84%E5%9F%BA%E7%A1%80)

[Java反射及性能](https://heapdump.cn/article/2753741)

[从一起GC血案谈到反射原理](https://heapdump.cn/article/54786?from=pc)