# ConcurrentHashMap

## 基本知识点

HashMap 的并发容器,基本属性与hashmap相同,默认并发级别是16(用于兼容JDK7中的版本,无实际作用).默认大小为16,如果指定了initialCapacity,则容量大小是1.5的initialCapacity向上取2的整数次幂.

### 基本属性

```java
//最大容量
private static final int MAXIMUM_CAPACITY = 1 << 30;
//默认容量
private static final int DEFAULT_CAPACITY = 16;
//最大的数组长度，toArray方法需要
static final int MAX_ARRAY_SIZE = Integer.MAX_VALUE - 8;
//负载因子
private static final float LOAD_FACTOR = 0.75f;
//链表树化的阈值
static final int TREEIFY_THRESHOLD = 8;
//红黑树变成链表的阈值
static final int UNTREEIFY_THRESHOLD = 6;
//链表需要树化的最小容量要求
static final int MIN_TREEIFY_CAPACITY = 64;
//在进行扩容时单个线程处理的最小步长。
private static final int MIN_TRANSFER_STRIDE = 16;
//sizeCtl 中用于生成标记的位数。对于 32 位数组，必须至少为 6。
private static int RESIZE_STAMP_BITS = 16;
//可以帮助调整大小的最大线程数。必须适合 32 - RESIZE_STAMP_BITS 位。
private static final int MAX_RESIZERS = (1 << (32 - RESIZE_STAMP_BITS)) - 1;
//在 sizeCtl 中记录大小标记的位移。
private static final int RESIZE_STAMP_SHIFT = 32 - RESIZE_STAMP_BITS;
//哈希表中的节点状态，会在节点的hash值中体现
static final int MOVED     = -1; // hash for forwarding nodes
static final int TREEBIN   = -2; // hash for roots of trees
static final int RESERVED  = -3; // hash for transient reservations
static final int HASH_BITS = 0x7fffffff; // usable bits of normal node hash

//基本计数器值(拿来统计哈希表中元素个数的)，主要在没有争用时使用，但也可作为表初始化竞赛期间的后备。通过 CAS 更新。
private transient volatile long baseCount;
//表初始化和调整大小控制。如果为负数，则表正在初始化或调整大小：-1 表示初始化，否则 -(1 + 活动调整大小线程的数量)。否则，当 table 为 null 时，保存要在创建时使用的初始表大小，或者默认为 0。初始化后，保存下一个元素计数值，根据该值调整表的大小。
private transient volatile int sizeCtl;
//调整大小时要拆分的下一个表索引（加一个）。
private transient volatile int transferIndex;
//调整大小和/或创建 CounterCell 时使用自旋锁（通过 CAS 锁定）。
private transient volatile int cellsBusy;
//计数单元表。当非空时，大小是 2 的幂。   与baseCount一起记录哈希表中的元素个数。
private transient volatile CounterCell[] counterCells;

```

## 核心方法

//构造函数

```java
public ConcurrentHashMap(int initialCapacity,
                         float loadFactor, int concurrencyLevel) {
    if (!(loadFactor > 0.0f) || initialCapacity < 0 || concurrencyLevel <= 0)
        throw new IllegalArgumentException();
    //当并发度大于初始值时,将初始值赋值为并发度
    if (initialCapacity < concurrencyLevel)   // Use at least as many bins
        initialCapacity = concurrencyLevel;   // as estimated threads
    //size = 初始值/负载因子+1,cap对size向上取2的整数次幂
    long size = (long)(1.0 + (long)initialCapacity / loadFactor);
    int cap = (size >= (long)MAXIMUM_CAPACITY) ?
        MAXIMUM_CAPACITY : tableSizeFor((int)size);
    this.sizeCtl = cap;
}
```

//存入数据

```java
//map中的基本方法,核心通过putVal方法实现
public V put(K key, V value) {
    return putVal(key, value, false);
}
```

//扰动函数

```java
//将高位中的hash值扩展到低位,将最高位强制置为0
static final int spread(int h) {
    return (h ^ (h >>> 16)) & HASH_BITS;
}
```

//核心方法

```java
final V putVal(K key, V value, boolean onlyIfAbsent) {
    //concurrentHashMap中键值对均不能为空
    if (key == null || value == null) throw new NullPointerException();
    //使用扰动函数处理hash值
    int hash = spread(key.hashCode());
    int binCount = 0;
    //无限循环处理put操作
    for (Node<K,V>[] tab = table;;) {
      	//f是对应hash桶的头结点,n表示当前桶容量,i表示对应的hash值代表的桶下标,fh用来帮助判断是否正在迁移
        Node<K,V> f; int n, i, fh;
        //如果tab没有初始化则初始化tab
        if (tab == null || (n = tab.length) == 0)
            tab = initTable();
        //如果对应的hash桶为空只要将node节点放入桶中即可,次数tabat函数是直接通过获取指针地址,后获取节点,线程安全
        else if ((f = tabAt(tab, i = (n - 1) & hash)) == null) {
            //利用cas替换node节点
            if (casTabAt(tab, i, null,
                         new Node<K,V>(hash, key, value, null)))
                break;                   // no lock when adding to empty bin
        }
        //如果对应的hash桶正在迁移,则帮助一起迁移,fwd节点hash值为-1
        else if ((fh = f.hash) == MOVED)
            tab = helpTransfer(tab, f);
        else {
            //寻找到了对应的hash桶,并开始插入操作
            V oldVal = null;
            //对头节点加锁,保证该桶的线程安全
            synchronized (f) {
				//当前头结点没有被改变时做一下操作
                if (tabAt(tab, i) == f) {
                    //如果是链表
                    if (fh >= 0) {
                        binCount = 1;
                        //遍历链表
                        for (Node<K,V> e = f;; ++binCount) {
                            K ek;
                            //从头结点开始寻找是否有相同的key节点存在,如果有则
                            if (e.hash == hash &&
                                ((ek = e.key) == key ||
                                 (ek != null && key.equals(ek)))) {
                                oldVal = e.val;
                                if (!onlyIfAbsent)
                                    e.val = value;
                                break;
                            }
                            Node<K,V> pred = e;
                            //知道找到最后都没有相同key存在则加入新的节点
                            if ((e = e.next) == null) {
                                pred.next = new Node<K,V>(hash, key,
                                                          value, null);
                                break;
                            }
                        }
                    }
                    //如果是树节点
                    else if (f instanceof TreeBin) {
                        Node<K,V> p;
                        binCount = 2;
                        //如果存在相同key(返回原有节点)
                        if ((p = ((TreeBin<K,V>)f).putTreeVal(hash, key,
                                                       value)) != null) {
                            oldVal = p.val;
                            if (!onlyIfAbsent)
                                p.val = value;
                        }
                    }
                }
            }
            if (binCount != 0) {
                //当链表数量大于8时需要转变为红黑树
                if (binCount >= TREEIFY_THRESHOLD)
                    treeifyBin(tab, i);
                if (oldVal != null)
                    return oldVal;
                break;
            }
        }
    }
    //计算并帮助map扩容
    addCount(1L, binCount);
    return null;
}
```

//初始化tab

```java
private final Node<K,V>[] initTable() {
    Node<K,V>[] tab; int sc;
    //sizeCtl默认为16,实际为map容量大小
    while ((tab = table) == null || tab.length == 0) {
        //正在初始化,则线程放弃时间片
        if ((sc = sizeCtl) < 0)
            Thread.yield(); // lost initialization race; just spin
        //比较并设置sizeCtl=-1
        else if (U.compareAndSwapInt(this, SIZECTL, sc, -1)) {
            try {
                //初始化数组
                if ((tab = table) == null || tab.length == 0) {
                    int n = (sc > 0) ? sc : DEFAULT_CAPACITY;
                    @SuppressWarnings("unchecked")
                    Node<K,V>[] nt = (Node<K,V>[])new Node<?,?>[n];
                    table = tab = nt;
                    //sizeCtl = cap * 0.75(负载因子)
                    sc = n - (n >>> 2);
                }
            } finally {
                sizeCtl = sc;
            }
            break;
        }
    }
    return tab;
}

/**
* 用于将读取传入对象o在内存中偏移量为offset位置的值与期望值expected作比较。,如果相等则将x值赋值给offset位置的值
* Atomically update Java variable to <tt>x</tt> if it is currently
* holding <tt>expected</tt>.
* @return <tt>true</tt> if successful
*/
public final native boolean compareAndSwapInt(Object o, long offset,
                                              int expected,
                                              int x);
```

//帮助扩容

```java
final Node<K,V>[] helpTransfer(Node<K,V>[] tab, Node<K,V> f) {
    //nextTab 表示迁移后的tab,sc=sizeCtl获取当前tab状态,此时应该是MOVED状态=-1
    Node<K,V>[] nextTab; int sc;
    //如果tab已经初始化了,且还没有迁移完成
    if (tab != null && (f instanceof ForwardingNode) &&
        (nextTab = ((ForwardingNode<K,V>)f).nextTable) != null) {
        //获取 扩容戳
        int rs = resizeStamp(tab.length);
        while (nextTab == nextTable && table == tab &&
               (sc = sizeCtl) < 0) {
            // sc右移16位, 当第一个线程扩容时已经将扩容信息保存在高16位了,右移16位是将扩容信息进行比较判断是否一致
            if ((sc >>> RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 ||
                sc == rs + MAX_RESIZERS || transferIndex <= 0)
                break;
            //修改并发线程数后进行扩容
            if (U.compareAndSwapInt(this, SIZECTL, sc, sc + 1)) {
                transfer(tab, nextTab);
                break;
            }
        }
        return nextTab;
    }
    return table;
}

//获取扩容戳 加入n=32 则 26 | 1<< 15 
 static final int resizeStamp(int n) {
        return Integer.numberOfLeadingZeros(n) | (1 << (RESIZE_STAMP_BITS - 1));
  }
```

//实际扩容方法

```java
private final void transfer(Node<K,V>[] tab, Node<K,V>[] nextTab) {
    int n = tab.length, stride;
    //获取范围,小于16时取默认值,否则取 (n >>> 3) / NCPU,如果4核CPU,容量为16,则stride=16
    if ((stride = (NCPU > 1) ? (n >>> 3) / NCPU : n) < MIN_TRANSFER_STRIDE)
        stride = MIN_TRANSFER_STRIDE; // subdivide range
    //初始化扩容的容器,大小为原有大小*2,当大于最大值时,取Integer.MAX_VALUE
    if (nextTab == null) {            // initiating
        try {
            @SuppressWarnings("unchecked")
            Node<K,V>[] nt = (Node<K,V>[])new Node<?,?>[n << 1];
            nextTab = nt;
        } catch (Throwable ex) {      // try to cope with OOME
            sizeCtl = Integer.MAX_VALUE;
            return;
        }
		//将调整的数据记录到属性上
        nextTable = nextTab;
        transferIndex = n;
    }
    //定义相关属性,ForwardingNode扩容节点
    int nextn = nextTab.length;
    ForwardingNode<K,V> fwd = new ForwardingNode<K,V>(nextTab);
    boolean advance = true;
    //确保下一个任务提交前进行扫描
    boolean finishing = false; // to ensure sweep before committing nextTab
    //无限循环进行操作,i代表下一个处理的槽号,bound代表下一个线程或循环跳跃边界
    for (int i = 0, bound = 0;;) {
        //f表示对应桶的头结点,fh代表f的hash值,如果为MOVE则表示有其他现在正在帮助迁移
        Node<K,V> f; int fh;
        while (advance) {
            int nextIndex, nextBound;
            //扩容结束跳出循环
            if (--i >= bound || finishing)
                advance = false;
            //待扩容index<=0跳出循环
            else if ((nextIndex = transferIndex) <= 0) {
                i = -1;
                advance = false;
            }
            //修改下一个index,和bound的值,触发对应index的扩容操作,每次index最少减16,因为stride最小值是16
            else if (U.compareAndSwapInt
                     (this, TRANSFERINDEX, nextIndex,
                      nextBound = (nextIndex > stride ?
                                   nextIndex - stride : 0))) {
                bound = nextBound;
                i = nextIndex - 1;
                advance = false;
            }
        }
        //i<0表示所有桶都已经被处理完成
        if (i < 0 || i >= n || i + n >= nextn) {
            int sc;
            //如果处理已经结束,将新老tab替换,修改sizeCtl为当前的1.5倍并退出
            if (finishing) {
                nextTable = null;
                table = nextTab;
                sizeCtl = (n << 1) - (n >>> 1);
                return;
            }
            //替换sizeCtl = sizeCtl -1, 每个线程加入扩容sizeCtl -1,退出则减一
            if (U.compareAndSwapInt(this, SIZECTL, sc = sizeCtl, sc - 1)) {
                //如果扩容戳的高位不相等表示非最后退出的线程已经扩容结束
                if ((sc - 2) != resizeStamp(n) << RESIZE_STAMP_SHIFT)
                    return;
                //如果相等表示所有线程扩容结束
                finishing = advance = true;
                //再次检查循环的表
                i = n; // recheck before commit
            }
        }
        //如果桶是空的则替换原来的节点为ForwardingNode
        else if ((f = tabAt(tab, i)) == null)
            advance = casTabAt(tab, i, null, fwd);
        //如果原先的桶正在迁移则将advance设置为true,跳到下一个桶中执行迁移操作
        else if ((fh = f.hash) == MOVED)
            advance = true; // already processed
        else {
            //锁定该槽位的头结点开始迁移
            synchronized (f) {
                //此处tabAt比较是防止线程并发操作时第二个获取锁的线程重复执行
                if (tabAt(tab, i) == f) {
                    //将节点分别分为low与high low还是放到原来的桶里,high则需要迁移到新桶
                    Node<K,V> ln, hn;
                    if (fh >= 0) {
                        //判断n二进制位位置处f hash值是否为1
                        int runBit = fh & n;
                        Node<K,V> lastRun = f;
                        for (Node<K,V> p = f.next; p != null; p = p.next) {
                            int b = p.hash & n;
                            //获取到最后一个runbit与之前不同的节点
                            if (b != runBit) {
                                runBit = b;
                                lastRun = p;
                            }
                        }
                        //如果最后为0则最后的头结点是lownode否则是highnode
                        if (runBit == 0) {
                            ln = lastRun;
                            hn = null;
                        }
                        else {
                            hn = lastRun;
                            ln = null;
                        }
                        //lastRun为最前面的节点,然后将节点分为两部分
                        for (Node<K,V> p = f; p != lastRun; p = p.next) {
                            int ph = p.hash; K pk = p.key; V pv = p.val;
                            if ((ph & n) == 0)
                                ln = new Node<K,V>(ph, pk, pv, ln);
                            else
                                hn = new Node<K,V>(ph, pk, pv, hn);
                        }
                        //设置新的容器位置为lownode和highnode
                        setTabAt(nextTab, i, ln);
                        setTabAt(nextTab, i + n, hn);
                        //原有节点变为fwd
                        setTabAt(tab, i, fwd);
                        //进行下一步操作
                        advance = true;
                    }
                    else if (f instanceof TreeBin) {
                        TreeBin<K,V> t = (TreeBin<K,V>)f;
                        TreeNode<K,V> lo = null, loTail = null;
                        TreeNode<K,V> hi = null, hiTail = null;
                        int lc = 0, hc = 0;
                        for (Node<K,V> e = t.first; e != null; e = e.next) {
                            int h = e.hash;
                            TreeNode<K,V> p = new TreeNode<K,V>
                                (h, e.key, e.val, null, null);
                            if ((h & n) == 0) {
                                if ((p.prev = loTail) == null)
                                    lo = p;
                                else
                                    loTail.next = p;
                                loTail = p;
                                ++lc;
                            }
                            else {
                                if ((p.prev = hiTail) == null)
                                    hi = p;
                                else
                                    hiTail.next = p;
                                hiTail = p;
                                ++hc;
                            }
                        }
                        ln = (lc <= UNTREEIFY_THRESHOLD) ? untreeify(lo) :
                            (hc != 0) ? new TreeBin<K,V>(lo) : t;
                        hn = (hc <= UNTREEIFY_THRESHOLD) ? untreeify(hi) :
                            (lc != 0) ? new TreeBin<K,V>(hi) : t;
                        setTabAt(nextTab, i, ln);
                        setTabAt(nextTab, i + n, hn);
                        setTabAt(tab, i, fwd);
                        advance = true;
                    }
                }
            }
        }
    }
}
```

//转换为树节点的方法

```java
private final void treeifyBin(Node<K,V>[] tab, int index) {
    Node<K,V> b; int n, sc;
    if (tab != null) {
        //如果容器数量小于64
        if ((n = tab.length) < MIN_TREEIFY_CAPACITY)
            //尝试预先调整表的大小以容纳给定数量的元素。
            tryPresize(n << 1);
        else if ((b = tabAt(tab, index)) != null && b.hash >= 0) {
            //synchronized+tabAt确保后一个获取锁的线程不重复执行
            synchronized (b) {
                if (tabAt(tab, index) == b) {
                    //构造树节点
                    TreeNode<K,V> hd = null, tl = null;
                    for (Node<K,V> e = b; e != null; e = e.next) {
                        TreeNode<K,V> p =
                            new TreeNode<K,V>(e.hash, e.key, e.val,
                                              null, null);
                        if ((p.prev = tl) == null)
                            hd = p;
                        else
                            tl.next = p;
                        tl = p;
                    }
                    setTabAt(tab, index, new TreeBin<K,V>(hd));
                }
            }
        }
    }
}
```

//尝试预测大小

```java
private final void tryPresize(int size) {
    //计算下一次扩容的容器大小
    int c = (size >= (MAXIMUM_CAPACITY >>> 1)) ? MAXIMUM_CAPACITY :
        tableSizeFor(size + (size >>> 1) + 1);
    int sc;
    while ((sc = sizeCtl) >= 0) {
        Node<K,V>[] tab = table; int n;
        //初始化tab
        if (tab == null || (n = tab.length) == 0) {
            n = (sc > c) ? sc : c;
            if (U.compareAndSwapInt(this, SIZECTL, sc, -1)) {
                try {
                    if (table == tab) {
                        @SuppressWarnings("unchecked")
                        Node<K,V>[] nt = (Node<K,V>[])new Node<?,?>[n];
                        table = nt;
                        sc = n - (n >>> 2);
                    }
                } finally {
                    sizeCtl = sc;
                }
            }
        }
        //表示已经到最大值
        else if (c <= sc || n >= MAXIMUM_CAPACITY)
            break;
        else if (tab == table) {
            int rs = resizeStamp(n);
            if (sc < 0) {
                Node<K,V>[] nt;
                if ((sc >>> RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 ||
                    sc == rs + MAX_RESIZERS || (nt = nextTable) == null ||
                    transferIndex <= 0)
                    break;
                //新线程帮助扩容
                if (U.compareAndSwapInt(this, SIZECTL, sc, sc + 1))
                    transfer(tab, nt);
            }
            //首次开始扩容
            else if (U.compareAndSwapInt(this, SIZECTL, sc,
                                         (rs << RESIZE_STAMP_SHIFT) + 2))
                transfer(tab, null);
        }
    }
}
```

//计算大小

```java
private final void addCount(long x, int check) {
    CounterCell[] as; long b, s;
	//有多个线程竞争计数器,如果cas成功说明,已经维护的basecount的数量值
    if ((as = counterCells) != null ||
        !U.compareAndSwapLong(this, BASECOUNT, b = baseCount, s = b + x)) {
        CounterCell a; long v; int m;
        //uncontended(没有竞争):竞争标记位
        boolean uncontended = true;
        if (as == null || (m = as.length - 1) < 0 ||
            //a代表获取当前线程的探测值与数组长度(为2的幂次)减一&操作,类似于hashmap,如果为空或cas失败则需要全量计算
            (a = as[ThreadLocalRandom.getProbe() & m]) == null ||
            !(uncontended =
              U.compareAndSwapLong(a, CELLVALUE, v = a.value, v + x))) {
            fullAddCount(x, uncontended);
            return;
        }
        //check小于1 表示只有在没有竞争的情况下统计
        if (check <= 1)
            return;
        //统计counterCells中总和加上baseCount
        s = sumCount();
    }
    if (check >= 0) {
        Node<K,V>[] tab, nt; int n, sc;
        //当 s(baseCount+x)大于sc时表示已经有超过总容量*负载因子的数据已被存储需要扩容 ,且容器已经初始化,且容器小于最大长度
        while (s >= (long)(sc = sizeCtl) && (tab = table) != null &&
               (n = tab.length) < MAXIMUM_CAPACITY) {
            //获取扩容戳
            int rs = resizeStamp(n);
            if (sc < 0) {
                if ((sc >>> RESIZE_STAMP_SHIFT) != rs || sc == rs + 1 ||
                    sc == rs + MAX_RESIZERS || (nt = nextTable) == null ||
                    transferIndex <= 0)
                    break;
                //协助扩容
                if (U.compareAndSwapInt(this, SIZECTL, sc, sc + 1))
                    transfer(tab, nt);
            }
            //开始扩容
            else if (U.compareAndSwapInt(this, SIZECTL, sc,
                                         (rs << RESIZE_STAMP_SHIFT) + 2))
                transfer(tab, null);
            //计算总和
            s = sumCount();
        }
    }
}

//只提供记录长度功能
@sun.misc.Contended static final class CounterCell {
        volatile long value;
        CounterCell(long x) { value = x; }
 }

//统计所有桶中的数据数量总和
final long sumCount() {
        CounterCell[] as = counterCells; CounterCell a;
        long sum = baseCount;
        if (as != null) {
            for (int i = 0; i < as.length; ++i) {
                if ((a = as[i]) != null)
                    sum += a.value;
            }
        }
        return sum;
}
```

//当遇到竞争时全量统计方法

```java
private final void fullAddCount(long x, boolean wasUncontended) {
    int h;
    //获取当前线程的probe的值，如果值为0，则初始化当前线程的probe的值,probe就是随机数
    if ((h = ThreadLocalRandom.getProbe()) == 0) {
        ThreadLocalRandom.localInit();      // force initialization
        h = ThreadLocalRandom.getProbe();
        //更新竞争标志
        wasUncontended = true;
    }
    boolean collide = false;                // True if last slot nonempty
    //自循环更新
    for (;;) {
        CounterCell[] as; CounterCell a; int n; long v;
        //如果之前已经初始化了
        if ((as = counterCells) != null && (n = as.length) > 0) {
            // 获取当前线程对应的CounterCell桶
            if ((a = as[(n - 1) & h]) == null) {
                if (cellsBusy == 0) {            // Try to attach new Cell
                    //乐观创建
                    CounterCell r = new CounterCell(x); // Optimistic create
                    //修改cellsBusy=1
                    if (cellsBusy == 0 &&
                        U.compareAndSwapInt(this, CELLSBUSY, 0, 1)) {
                        boolean created = false;
                        try {               // Recheck under lock
                            CounterCell[] rs; int m, j;
                            //双重校验该桶位置还没有创建则填充之前创建的值
                            if ((rs = counterCells) != null &&
                                (m = rs.length) > 0 &&
                                rs[j = (m - 1) & h] == null) {
                                rs[j] = r;
                                created = true;
                            }
                        } finally {
                            cellsBusy = 0;
                        }
                        if (created)
                            break;
                        continue;           // Slot is now non-empty
                    }
                }
                collide = false;
            }
            //说明指定cells下标位置的数据不为空，则进行下一次循环
            else if (!wasUncontended)       // CAS already known to fail
                wasUncontended = true;      // Continue after rehash
            //修改当前槽位的值,成功则退出
            else if (U.compareAndSwapLong(a, CELLVALUE, v = a.value, v + x))
                break;
            //多个线程并发修改时上一步cas修改失败,循环重试
            else if (counterCells != as || n >= NCPU)
                collide = false;            // At max size or stale
            //恢复collide状态，标识下次循环会进行扩容
            else if (!collide)
                collide = true;
            //进入这一步,说明CounterCell数组容量不够，线程竞争较大，所以先设置一个标识表示为正在扩容
            else if (cellsBusy == 0 &&
                     U.compareAndSwapInt(this, CELLSBUSY, 0, 1)) {
                try {
                    //扩容counterCells值
                    if (counterCells == as) {// Expand table unless stale
                        CounterCell[] rs = new CounterCell[n << 1];
                        for (int i = 0; i < n; ++i)
                            rs[i] = as[i];
                        counterCells = rs;
                    }
                } finally {
                    cellsBusy = 0;
                }
                collide = false;
                continue;                   // Retry with expanded table
            }
            //继续下一次自旋
            h = ThreadLocalRandom.advanceProbe(h);
        }
        //没有初始化则进行初始化,将cellsBusy更新为1
        else if (cellsBusy == 0 && counterCells == as &&
                 U.compareAndSwapInt(this, CELLSBUSY, 0, 1)) {
            boolean init = false;
            try {                           // Initialize table
                if (counterCells == as) {
                    //初始化数组,随机将rs的其中一个填充当前x计数值,更新初始化标志和counterCells
                    CounterCell[] rs = new CounterCell[2];
                    rs[h & 1] = new CounterCell(x);
                    counterCells = rs;
                    init = true;
                }
            } finally {
                cellsBusy = 0;
            }
            if (init)
                break;
        }
        //前两步更新失败则直接修改basecount的值
        else if (U.compareAndSwapLong(this, BASECOUNT, v = baseCount, v + x))
            break;                          // Fall back on using base
    }
}
```





参考:[ConcurrentHashMap如何实现扩容机制之学习笔记](https://blog.csdn.net/weixin_42022924/article/details/102865519)

[ConcurrentHashMap面试灵魂拷问](https://zhuanlan.zhihu.com/p/355565143)

[JAVA源码之ConcurrentHashMap](https://juejin.cn/post/7084931212871434271)

[ConcurrentHashmap的fullAddCount](https://blog.csdn.net/luzhensmart/article/details/105958364)