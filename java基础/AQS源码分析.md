# AQS源码分析

### 思想 

如果资源空闲,则直接获取并锁定,否则利用CLH队列实现线程的阻塞等待及唤醒,将这些线程入队.队列为FIFO队列.使用了模版模式,一些简单方法由子类实现,父类组装顺序.

```
CLH(Craig,Landin,and Hagersten)队列是一个虚拟的双向队列(虚拟的双向队列即不存在队列实例，仅存在结点之间的关联关系)。AQS是将每条请求共享资源的线程封装成一个CLH锁队列的一个结点(Node)来实现锁的分配。
```

### 数据结构

底层由CLH队列实现,队列封装成节点入队..由一条同步双向链表sync Queue,及多条单线链表condition queue组成,(condition queue非必须)

![image-20220216214223715](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220216214223715.png)



## 源码解读

<img src="C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220216214757602.png" alt="image-20220216214757602" style="zoom:50%;" />

`AbstractQueuedSynchronizer`类结构如上所示.他包含了Node类与ConditionObject类,实现了AbstractOwnableSynchronizer接口和序列化接口.

```Java
public abstract class AbstractOwnableSynchronizer
    implements java.io.Serializable {

    private static final long serialVersionUID = 3737899427754241961L;

   	//子类使用的构造函数
    protected AbstractOwnableSynchronizer() { }
	//独占线程
    private transient Thread exclusiveOwnerThread;
	//设置独占线程
    protected final void setExclusiveOwnerThread(Thread thread) {
        exclusiveOwnerThread = thread;
    }
    //获取独占线程
    protected final Thread getExclusiveOwnerThread() {
        return exclusiveOwnerThread;
    }
}
```

node类文档

> 等待队列节点类。
> 等待队列是“CLH”（Craig、Landin 和 Hagersten）锁定队列的变体。 CLH 锁通常用于自旋锁。相反，我们将它们用于阻塞同步器，但使用相同的基本策略，即在其节点的前身中保存有关线程的一些控制信息。每个节点中的“状态”字段跟踪线程是否应该阻塞。节点在其前身释放时发出信号。否则，队列的每个节点都用作持有单个等待线程的特定通知样式的监视器。状态字段不控制线程是否被授予锁等。线程可能会尝试获取队列中的第一个线程。但成为第一并不能保证成功；它只赋予抗争的权利。所以当前释放的竞争者线程可能需要重新等待。
> 要加入 CLH 锁，您可以将其自动拼接为新的尾部。要出列，您只需设置 head 字段。
>             +------+  prev +-----+       +-----+
>        head |      | <---- |     | <---- |     |  tail
>             +------+       +-----+       +-----+
>        
> 插入 CLH 队列只需要对“尾”进行单个原子操作，因此从未排队到排队有一个简单的原子分界点。同样，出队只涉及更新“头”。然而，节点需要做更多的工作来确定他们的继任者是谁，部分原因是为了处理由于超时和中断而可能导致的取消。
> “prev”链接（未在原始 CLH 锁中使用）主要用于处理取消。如果一个节点被取消，它的后继者（通常）重新链接到一个未取消的前任者。有关自旋锁情况下类似机制的解释，请参阅 Scott 和 Scherer 的论文，网址为 http://www.cs.rochester.edu/u/scott/synchronization/
> 我们还使用“下一个”链接来实现阻塞机制。每个节点的线程 id 保存在它自己的节点中，因此前驱节点通过遍历下一个链接来确定它是哪个线程来通知下一个节点唤醒。继任者的确定必须避免与新排队的节点竞争以设置其前任者的“下一个”字段。当节点的后继节点似乎为空时，通过从原子更新的“尾部”向后检查来解决这个问题。 （或者，换一种说法，下一个链接是一种优化，因此我们通常不需要向后扫描。）
> 取消对基本算法引入了一些保守性。由于我们必须轮询其他节点的取消，我们可能会错过注意到取消的节点是在我们前面还是在我们后面。这是通过在取消时始终取消停放继任者来处理的，从而使他们能够稳定在新的前任上，除非我们能够确定一个未取消的前任来承担这一责任。
> CLH 队列需要一个虚拟头节点才能启动。但是我们不会在构建时创建它们，因为如果从不存在争用，那将是浪费精力。相反，在第一次争用时构造节点并设置头和尾指针。
> 等待条件的线程使用相同的节点，但使用附加链接。条件只需要链接简单（非并发）链接队列中的节点，因为它们仅在独占时才被访问。在等待时，将一个节点插入到条件队列中。收到信号后，节点被转移到主队列。状态字段的特殊值用于标记节点在哪个队列上



```java
 static final class Node {
    //标记节点在共享模式
    static final Node SHARED = new Node();
    /** 标记节点在独占模式 */
    static final Node EXCLUSIVE = null;

    //CANCELLED 1 当前线程被取消
    //SIGNAL -1 当前节点的后继节点包含的线程需要运行 unpark
     //CONDITION -2 当前节点在等待condition,在condition的队列中
     //PROPAGATE -3 表示当前场景下后续的acquireShared能够得以执行
     //0 当前节点在sync队列中,等待获取锁
    static final int CANCELLED =  1;
    static final int SIGNAL    = -1;
    static final int CONDITION = -2;
    static final int PROPAGATE = -3;

   // 节点状态,菲负值代表节点不需要信号,大多数代码不需要检测数值,只需要检测符号即可
    volatile int waitStatus;

    //前置节点
    volatile Node prev;

    //后置节点
    volatile Node next;

    //当前线程
    volatile Thread thread;

    //下一个等待者
    Node nextWaiter;

    //节点是否在等待模式,
    final boolean isShared() {
        return nextWaiter == SHARED;
    }

    //获取上一个节点,如果为空则抛空指针
    final Node predecessor() throws NullPointerException {
        Node p = prev;
        if (p == null)
            throw new NullPointerException();
        else
            return p;
    }
	//构造器,用于建立头部或共享标记
    Node() {    
    }
	//用于addWaiter方法,独占
    Node(Thread thread, Node mode) {     
        this.nextWaiter = mode;
        this.thread = thread;
    }
	//用于condition
    Node(Thread thread, int waitStatus) { 
        this.waitStatus = waitStatus;
        this.thread = thread;
    }
}
```

内部类 ConditionObject 继承自 Condition.接口中定义了await、signal方法，用来等待条件、释放条件。

```Java
public interface Condition{
	//当前线程等待,直到被唤醒或中断
    void await() throws InterruptedException;
	//当前线程等待,直到被唤醒无法被中断
    void awaitUninterruptibly();
	//当前线程等待,直到被唤醒或中断或等待时间到达
    long awaitNanos(long nanosTimeout) throws InterruptedException;
    //当前线程等待,直到被唤醒或中断或等待时间到达
    boolean await(long time, TimeUnit unit) throws InterruptedException;
	////当前线程等待,直到被唤醒或中断或等待时间到达
    boolean awaitUntil(Date deadline) throws InterruptedException;

    //唤醒一个等待线程。如果所有的线程都在等待此条件，则选择其中的一个唤醒。在从 await 返回之前，该线程必须重新获取锁
    void signal();

    //唤醒所有线程,在从 await 返回之前，每个线程都必须重新获取锁。
    void signalAll();
}
```

condition中唤醒方法指从condition队列移动到sync队列

```java
public class ConditionObject implements Condition, java.io.Serializable {
        private static final long serialVersionUID = 1173984872572414699L;
    	//condition队列第一个等待者
        private transient Node firstWaiter;
        //condition队列最后一个等待者
        private transient Node lastWaiter;

        
        public ConditionObject() { }

        // 内部方法

        //添加新的等待者到等待队列   
        private Node addConditionWaiter() {
            //保存尾节点
            Node t = lastWaiter;
            // 如果尾节点已经被取消
            if (t != null && t.waitStatus != Node.CONDITION) {
                //遍历所有节点,移除被取消的节点,并更新尾节点
                unlinkCancelledWaiters();
                //将尾节点复制给t
                t = lastWaiter;
            }
            //构件新的节点,状态为-2,表示再condition队列中
            Node node = new Node(Thread.currentThread(), Node.CONDITION);
            if (t == null)
                //尾节点为空,将头结点更新尾node
                firstWaiter = node;
            else
                t.nextWaiter = node;
            //更新尾节点为node
            lastWaiter = node;
            return node;
        }

     
    	//唤醒头节点如果不成功则遍历队列
        private void doSignal(Node first) {
            do {
                if ( (firstWaiter = first.nextWaiter) == null) //头结点没有后续节点时,更新最后节点为空
                    lastWaiter = null;
                //设置
                first.nextWaiter = null;
            } while (!transferForSignal(first) &&
                     (first = firstWaiter) != null); //将节点从condition队列转移到sycn队列失败,且投节点不为空
        }

        //删除并转义所有节点到sync队列
        private void doSignalAll(Node first) {
            //condition队列头尾节点都设置为空
            lastWaiter = firstWaiter = null;
            do {             
                Node next = first.nextWaiter;
                first.nextWaiter = null;
                //节点转义到sync队列
                transferForSignal(first);
                //设置next节点为头节点
                first = next;
            } while (first != null);
        }

        //遍历所有节点,移除那些不为CONDITION的节点
        private void unlinkCancelledWaiters() {
            //保存头节点
            Node t = firstWaiter;
            Node trail = null;
            while (t != null) {
                //获取t的下一个节点
                Node next = t.nextWaiter;
                if (t.waitStatus != Node.CONDITION) {//不满足条件时
                    //next域设置为空
                    t.nextWaiter = null;
                    if (trail == null) //trail为空可能情况头结点不满足条件
                        //从新设置头节点
                        firstWaiter = next;
                    else
                        //更新trail的next域
                        trail.nextWaiter = next;
                    if (next == null) //next为空
                        //设置尾节点
                        lastWaiter = trail;
                }
                else//满足条件设置trail节点
                    trail = t;
                //移动到下一个节点
                t = next;
            }
        }

        // 公共方法

        
    	//移动等待时间最长的线程到sync队列
        public final void signal() {
            if (!isHeldExclusively()) //不被当前线程独占时抛出异常
                throw new IllegalMonitorStateException();
            //保存头结点
            Node first = firstWaiter;
            if (first != null)
                //唤醒头结点
                doSignal(first);
        }

     
    	//唤醒所有condition中的线程
        public final void signalAll() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            //保存头节点
            Node first = firstWaiter;
            if (first != null)
                doSignalAll(first);
        }

        
    	//实现不间断的条件等待,不响应中断
        public final void awaitUninterruptibly() {
            //添加一个节点到condition队列
            Node node = addConditionWaiter();
            //获取释放状态 fullyRelease会调用relase方法,详细调用看下文
            int savedState = fullyRelease(node);
            boolean interrupted = false;
            while (!isOnSyncQueue(node)) {  //节点不在sync队列上,该方法位于aqs类中
               //阻塞当前线程,unpark时唤醒
                LockSupport.park(this);
                //线程是否中断
                if (Thread.interrupted())
                    //设置interrupted属性
                    interrupted = true;
            }
            if (acquireQueued(node, savedState) || interrupted) //获取资源成功或线程中断
                //中断当前线程
                selfInterrupt();
        }

        //当遇到中断时是否抛出异常

       //退出等待时重新中断
        private static final int REINTERRUPT =  1;
        //退出等待时抛出异常
        private static final int THROW_IE    = -1;

        
    	//线程没有中断返回0,如果线程中断,线程没有发出signal前被取消返回THROW_IE,,否则返回REINTERRUPT
        private int checkInterruptWhileWaiting(Node node) {
            return Thread.interrupted() ?
                (transferAfterCancelledWait(node) ? THROW_IE : REINTERRUPT) :
                0;
        }

        //根据模式选择抛出异常或者中断线程
        private void reportInterruptAfterWait(int interruptMode)
            throws InterruptedException {
            if (interruptMode == THROW_IE)
                throw new InterruptedException();
            else if (interruptMode == REINTERRUPT)
                selfInterrupt();
        }

        // 等待，当前线程在接到信号或被中断之前一直处于等待状态
        public final void await() throws InterruptedException {
            if (Thread.interrupted()) //线程中断抛出异常
                throw new InterruptedException();
            //将节点加入到队列
            Node node = addConditionWaiter();
            //获取释放状态 
            int savedState = fullyRelease(node);
            int interruptMode = 0;
            while (!isOnSyncQueue(node)) { //如果不在sync队列上
                //阻塞该线程
                LockSupport.park(this); 
                //当模式改变(遇到线程中断时)退出循环
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
            }
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE) //获取到资源成功,且模式不为THROW_IE 
                //修改模式为 REINTERRUPT
                interruptMode = REINTERRUPT;
            if (node.nextWaiter != null) //如果有后续节点 
                //清除队列中取消的节点
                unlinkCancelledWaiters();
            if (interruptMode != 0)//遇到线程中断
                //根据模式判断是否需要抛出异常
                reportInterruptAfterWait(interruptMode);
        }

        //// 等待，当前线程在接到信号或被中断或时间到达之前一直处于等待状态
        public final long awaitNanos(long nanosTimeout)
                throws InterruptedException {
            if (Thread.interrupted())
                throw new InterruptedException();
            //新增waiter
            Node node = addConditionWaiter();
            //获取释放状态
            int savedState = fullyRelease(node);
            //最大有效时间
            final long deadline = System.nanoTime() + nanosTimeout;
            int interruptMode = 0;
            while (!isOnSyncQueue(node)) {
                if (nanosTimeout <= 0L) { //等待到期
                    //尝试修改node state
                    transferAfterCancelledWait(node);
                    break;
                }
                if (nanosTimeout >= spinForTimeoutThreshold) //剩余等待时长大于默认配置
                    //阻塞当前线程
                    LockSupport.parkNanos(this, nanosTimeout);
                //当模式改变(遇到线程中断时)退出循环
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
                //更新剩余时长
                nanosTimeout = deadline - System.nanoTime();
            }
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE)//获取到资源成功,且模式不为THROW_IE 
                //修改模式为 REINTERRUPT
                interruptMode = REINTERRUPT;
            if (node.nextWaiter != null)//如果有后续节点
                //清除队列中取消的节点
                unlinkCancelledWaiters();
            if (interruptMode != 0)//遇到线程中断
                //根据模式判断是否需要抛出异常
                reportInterruptAfterWait(interruptMode);
            //获取剩余时长
            return deadline - System.nanoTime();
        }

        /** 实现绝对定时条件等待。
1.如果当前线程被中断，则抛出 InterruptedException。
2.保存getState返回的锁定状态。
3.使用保存状态作为参数调用release ，如果失败则抛出 IllegalMonitorStateException。
4.阻塞直到发出信号、中断或超时。
5.通过以保存状态作为参数调用特定版本的acquire来重新获取。
6.如果在步骤 4 中被阻塞时被中断，则抛出 InterruptedException。
7.如果在步骤 4 中被阻塞时超时，则返回 false，否则返回 true。**/
        public final boolean awaitUntil(Date deadline)
                throws InterruptedException {
            long abstime = deadline.getTime();
            if (Thread.interrupted())
                throw new InterruptedException();
            Node node = addConditionWaiter();
            int savedState = fullyRelease(node);
            boolean timedout = false;
            int interruptMode = 0;
            while (!isOnSyncQueue(node)) {
                if (System.currentTimeMillis() > abstime) {
                    timedout = transferAfterCancelledWait(node);
                    break;
                }
                LockSupport.parkUntil(this, abstime);
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
            }
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE)
                interruptMode = REINTERRUPT;
            if (node.nextWaiter != null)
                unlinkCancelledWaiters();
            if (interruptMode != 0)
                reportInterruptAfterWait(interruptMode);
            return !timedout;
        }

        //等待，当前线程在接到信号或被中断或时间到达之前一直处于等待状态 等同于awaitNanos(long nanosTimeout)
        public final boolean await(long time, TimeUnit unit)
                throws InterruptedException {
            long nanosTimeout = unit.toNanos(time);
            if (Thread.interrupted())
                throw new InterruptedException();
            Node node = addConditionWaiter();
            int savedState = fullyRelease(node);
            final long deadline = System.nanoTime() + nanosTimeout;
            boolean timedout = false;
            int interruptMode = 0;
            while (!isOnSyncQueue(node)) {
                if (nanosTimeout <= 0L) {
                    timedout = transferAfterCancelledWait(node);
                    break;
                }
                if (nanosTimeout >= spinForTimeoutThreshold)
                    LockSupport.parkNanos(this, nanosTimeout);
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
                nanosTimeout = deadline - System.nanoTime();
            }
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE)
                interruptMode = REINTERRUPT;
            if (node.nextWaiter != null)
                unlinkCancelledWaiters();
            if (interruptMode != 0)
                reportInterruptAfterWait(interruptMode);
            return !timedout;
        }

        //如果此条件是由给定的同步对象创建的，则返回 true。
        final boolean isOwnedBy(AbstractQueuedSynchronizer sync) {
            return sync == AbstractQueuedSynchronizer.this;
        }

        //是否存在waiter节点
        protected final boolean hasWaiters() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            for (Node w = firstWaiter; w != null; w = w.nextWaiter) {
                if (w.waitStatus == Node.CONDITION)
                    return true;
            }
            return false;
        }

        //获取等待队列长度(大致数据)
        protected final int getWaitQueueLength() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            int n = 0;
            for (Node w = firstWaiter; w != null; w = w.nextWaiter) {
                if (w.waitStatus == Node.CONDITION)
                    ++n;
            }
            return n;
        }

        //返回包含那些可能正在等待此条件的线程集合
        protected final Collection<Thread> getWaitingThreads() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            ArrayList<Thread> list = new ArrayList<Thread>();
            for (Node w = firstWaiter; w != null; w = w.nextWaiter) {
                if (w.waitStatus == Node.CONDITION) {
                    Thread t = w.thread;
                    if (t != null)
                        list.add(t);
                }
            }
            return list;
        }
    }
```

 AbstractQueuedSynchronizer类属性

```java

public abstract class AbstractQueuedSynchronizer extends AbstractOwnableSynchronizer
    implements java.io.Serializable {    
    // 版本号
    private static final long serialVersionUID = 7373984972572414691L;    
    // 头节点
    private transient volatile Node head;    
    // 尾结点
    private transient volatile Node tail;    
    // 状态
    private volatile int state;    
    // 自旋时间
    static final long spinForTimeoutThreshold = 1000L;
    
    // Unsafe类实例
    private static final Unsafe unsafe = Unsafe.getUnsafe();
    //通过下面的偏移地址信息可以获取属性在内存中的值
    // state内存偏移地址
    private static final long stateOffset;
    // head内存偏移地址
    private static final long headOffset;
    // tail内存偏移地址
    private static final long tailOffset;
    // 内存偏移地址
    private static final long waitStatusOffset;
    // next内存偏移地址
    private static final long nextOffset;
    // 静态初始化块
    static {
        try {
            stateOffset = unsafe.objectFieldOffset
                (AbstractQueuedSynchronizer.class.getDeclaredField("state"));
            headOffset = unsafe.objectFieldOffset
                (AbstractQueuedSynchronizer.class.getDeclaredField("head"));
            tailOffset = unsafe.objectFieldOffset
                (AbstractQueuedSynchronizer.class.getDeclaredField("tail"));
            waitStatusOffset = unsafe.objectFieldOffset
                (Node.class.getDeclaredField("waitStatus"));
            nextOffset = unsafe.objectFieldOffset
                (Node.class.getDeclaredField("next"));

        } catch (Exception ex) { throw new Error(ex); }
    }
}
```

### 核心方法 - acquire

```java
/**
以独占模式获取，忽略中断。通过调用至少一次tryAcquire来实现，成功返回。否则线程排队，可能重复阻塞和解除阻塞，调用tryAcquire直到成功。此方法可用于实现方法Lock.lock **/
public final void acquire(int arg) {
	//tryAcquire由子类实现,默认抛出UnsupportedOperationException
    if (!tryAcquire(arg) &&
        acquireQueued(addWaiter(Node.EXCLUSIVE), arg))
        selfInterrupt();
}
```

该方法调用流程

![image](https://pdai.tech/_images/thread/java-thread-x-juc-aqs-2.png)

1. 首先尝试获取tryAcquire获取锁
2. addWaiter方法如果获取失败将节点加入sync队列
3. acquireQueued方法sync队列中节点以独占模式不间断获取资源,acquireQueued返回的是线程中断状态
4. 都失败线程中断



addWaiter方法

```java
//添加新节点到队列
private Node addWaiter(Node mode) {
    Node node = new Node(Thread.currentThread(), mode);
    // Try the fast path of enq; backup to full enq on failure
    //保存尾节点
    Node pred = tail;
    if (pred != null) {
        //将尾节点设置为当前节点的前度节点
        node.prev = pred;
        if (compareAndSetTail(pred, node)) { //如果尾节点没有发生变化,则将node设置为尾节点
            //设置当前节点属性并返回
            pred.next = node;
            return node;
        }
    }
    //如果队列为空初始化队列,或则尾节点设置
    enq(node);
    return node;
}
```

enq方法将节点插入到队列,必要时初始化
```java
private Node enq(final Node node) {
    for (;;) { //循环
        Node t = tail;
        if (t == null) { // tail为空延迟初始化
            if (compareAndSetHead(new Node())) //将头结点设置初始化为
                tail = head;
        } else {
            node.prev = t;  //当前t表示尾/头节点,将node插入到尾节点之后
            if (compareAndSetTail(t, node)) { //将node替换成尾节点
                //更新node链接
                t.next = node;
                return t;
            }
        }
    }
}
```

acquire方法,以独占不间断模式获取已在队列中的线程

```java
final boolean acquireQueued(final Node node, int arg) {
    //成功标志
    boolean failed = true;
    try {
        //线程中断标志
        boolean interrupted = false;
        for (;;) { //无线循环保证执行成功
            //获取node的前度节点
            final Node p = node.predecessor();
            if (p == head && tryAcquire(arg)) {//如果p是头结点,且尝试获取成功
                //将node节点设置为头节点
                setHead(node);
                p.next = null; // help GC
                failed = false;
                //返回线程中断状态
                return interrupted;
            }
            if (shouldParkAfterFailedAcquire(p, node) && //前置节点为SIGNAL且线程中断成功
                parkAndCheckInterrupt())
                //修改线程中断标志
                interrupted = true;
        }
    } finally {
        if (failed)
            //失败取消线程
            cancelAcquire(node);
    }
}
```



shouldParkAfterFailedAcquire方法,只有当前置节点为signal时才能对封装线程的node进行park操作

```java
private static boolean shouldParkAfterFailedAcquire(Node pred, Node node) {
    int ws = pred.waitStatus;
    if (ws == Node.SIGNAL)
        //前置节点状态为SIGNAL-1,可以被park
        return true;
    if (ws > 0) { //大于0表示前置节点状态为cancelled,需要被移除
        do {
            //遍历移除状态为cancelled的节点
            node.prev = pred = pred.prev;
        } while (pred.waitStatus > 0);
        pred.next = node;
    } else { //为PROPAGATE -3 或者是0 表示无状态,(为CONDITION -2时，表示此节点在condition queue中) 
        //设置前置节点状态为SIGNAL-1
        compareAndSetWaitStatus(pred, ws, Node.SIGNAL);
    }
    //不能被park
    return false;
}
```

parkAndCheckInterrupt方法,park后中断线程,返回是否中断成功

```java
//进行park操作并且返回该线程是否被中断 
private final boolean parkAndCheckInterrupt() {
    	//在许可可用之前禁用当前线程，并且设置了blocker
        LockSupport.park(this);
    	//当前线程是否已被中断，并清除中断标记位
        return Thread.interrupted();
    }
```

cancelAcquire方法用于取消node获取资源,取消当前线程对资源的获取，即设置该结点的状态为CANCELLED

```java
private void cancelAcquire(Node node) {
    // 节点不存在直接返回
    if (node == null)
        return;
	//将节点关联的线程设置为空
    node.thread = null;

    // 异常所有状态为cancelled的前度节点
    Node pred = node.prev;
    while (pred.waitStatus > 0)
        node.prev = pred = pred.prev;

    //pred节点的next应该指向当前节点,应该被取消
    Node predNext = pred.next;

    //将节点设置为CANCELLED其他线程观测到后会忽略该节点
    node.waitStatus = Node.CANCELLED;

    if (node == tail && compareAndSetTail(node, pred)) {//如果我们是尾节点,且将pred替换成尾节点成功
         // 比较并设置pred结点的next节点为null
        compareAndSetNext(pred, predNext, null);
    } else {

        //如果后续者需要获取信号,尝试设置pred的下一个连接
        //我们会获得一个,或者唤醒至propagate
        int ws;
        if (pred != head &&
            ((ws = pred.waitStatus) == Node.SIGNAL ||
             (ws <= 0 && compareAndSetWaitStatus(pred, ws, Node.SIGNAL))) &&
            pred.thread != null) { //pred不为头节点,且前置节点状态为SIGNAL
            Node next = node.next;
            if (next != null && next.waitStatus <= 0) //next不为空且不是取消状态
                //将next与pred连接
                compareAndSetNext(pred, predNext, next);
        } else {
            //释放node的后置节点
            unparkSuccessor(node);
        }

        node.next = node; // help GC
    }
}
```

unparkSuccessor 方法释放node的后续节点

```java
private void unparkSuccessor(Node node) {
    //如果线程状态不为cancelled或无状态,需要等待信号,则需修改成无状态
    int ws = node.waitStatus;
    if (ws < 0)
        compareAndSetWaitStatus(node, ws, 0);

  	//寻找node节点后继节点中需要获取信号的节点
    Node s = node.next;
    if (s == null || s.waitStatus > 0) {
        s = null;
        for (Node t = tail; t != null && t != node; t = t.prev)
            if (t.waitStatus <= 0)
                s = t;
    }
    // 该结点不为为空，释放许可
    if (s != null)
        LockSupport.unpark(s.thread);
}
```

1. 判断结点的前驱是否为head并且是否成功获取(资源)。

2. 若步骤1均满足，则设置结点为head，之后会判断是否finally模块，然后返回。

3. 若步骤2不满足，则判断是否需要park当前线程，是否需要park当前线程的逻辑是判断结点的前驱结点的状态是否为SIGNAL，若是，则park当前结点，否则，不进行park操作。

4. 若park了当前线程，之后某个线程对本线程unpark后，并且本线程也获得机会运行。那么，将会继续进行步骤①的判断

###  类的核心方法 - release方法

以独占模式释放对象

```java
public final boolean release(int arg) {
    if (tryRelease(arg)) { //尝试释放且成功,由子类实现默认抛UnsupportedOperationException
       	//获取当前头节点
        Node h = head;
        if (h != null && h.waitStatus != 0)//头结点不为空且状态不是无状态
            //释放后继节点
            unparkSuccessor(h);
        return true;
    }
    return false;
}
```