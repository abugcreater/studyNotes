# HashMap

## 基本知识点

键值对存放数据,线程不安全,对应的同步容器为`HashTable`,并发容器为ConcurrentHashMap,利用开链方式解决hash冲突,一般hash冲突解决方式:1.开链2.开放寻址

底层数据结构为数组+链表节点,默认当链表长度大于8时链表转为红黑树.

扩容因子为0.75,默认大小是16,最大为1<<30,当树节点数量小于6的时候退化为链表.当树节点数量大于64时,hashmap会整体扩容.hashmap的大小一般是2的倍数.

扩容因子表示数组装填的数量.

## 基本方法

初始化方法

```java
public HashMap(int initialCapacity, float loadFactor) {
    if (initialCapacity < 0)
        throw new IllegalArgumentException("Illegal initial capacity: " +
                                           initialCapacity);
    if (initialCapacity > MAXIMUM_CAPACITY)
        initialCapacity = MAXIMUM_CAPACITY;
    if (loadFactor <= 0 || Float.isNaN(loadFactor))
        throw new IllegalArgumentException("Illegal load factor: " +
                                           loadFactor);
    //扩容因子必须大于0
    this.loadFactor = loadFactor;
    //初始化map大小,一般是2的整数倍,当initialCapacity输入是1时最终会出现大小为1的特殊情况
    this.threshold = tableSizeFor(initialCapacity);
}
```

计算map大小方法

```java
//实际就是将cap逐步位移总位移30位,所以保证了在cap位移以后每位都是1,而之所以先减一,是为了防止溢出,因为int一共32位,第一位是符号位,存在符号位是1,其他都是0的情况,输入后所有位都变为1,导致数值溢出
static final int tableSizeFor(int cap) {
    int n = cap - 1;
    n |= n >>> 1;
    n |= n >>> 2;
    n |= n >>> 4;
    n |= n >>> 8;
    n |= n >>> 16;
    return (n < 0) ? 1 : (n >= MAXIMUM_CAPACITY) ? MAXIMUM_CAPACITY : n + 1;
}
```

//存放数据方法

```java
public V put(K key, V value) {
 	//首先计算key的hash值
    return putVal(hash(key), key, value, false, true);
}

//如果key为0则直接返回0,否则获取key的hashcode值,并与h >>> 16, 数字(long,int)的hash值始终为该值本身
//扰动函数,结合高低位特征防止hash碰撞
static final int hash(Object key) {
   int h;
    //做^操作时h=hashcode,^操作是为了将低16位的值与高16的值做^操作(^按位异或)
   return (key == null) ? 0 : (h = key.hashCode()) ^ (h >>> 16);
}
```

//实际执行的存放数据方法

```java
// onlyIfAbsent(true)当数据存在是不做替换,evict(false)table在创建模式,hashmap中无实际作用,LinkedHashMap中有做判断,用以逐出头结点
final V putVal(int hash, K key, V value, boolean onlyIfAbsent,
               boolean evict) {
    //p指数组对应的原有节点,tab指底层数组,n指大小,i指hash计算后的值
    Node<K,V>[] tab; Node<K,V> p; int n, i;
    //如果table没有被初始化,则需要先初始化table
    if ((tab = table) == null || (n = tab.length) == 0) 
        //resize 初始化或扩容map
        n = (tab = resize()).length;
    //如果数组对应的位置还没有被初始化,则初始化该节点值,节点类型为链表
    //i = (n - 1) & hash,n = tab.length;等于利用hash值寻址, 使用n-1是为了避免hash碰撞
    if ((p = tab[i = (n - 1) & hash]) == null)
        tab[i] = newNode(hash, key, value, null);
    else {
        //该节点已被初始化过时,e指新插入的节点,k=p.key
        Node<K,V> e; K k;
        if (p.hash == hash &&
            ((k = p.key) == key || (key != null && key.equals(k))))
            //当key完全相等时,e替换原有p节点
            e = p;
        else if (p instanceof TreeNode)
            //如果原先是树节点则调用树节点的put操作
            e = ((TreeNode<K,V>)p).putTreeVal(this, tab, hash, key, value);
        else {
            //hash桶还是链表,且该桶已被初始化
            for (int binCount = 0; ; ++binCount) {
                if ((e = p.next) == null) {
                    //将节点添加到尾部,如果大于8则需要转化为树节点，但是树化必须要满足的条件是,数组大小大于64
                    p.next = newNode(hash, key, value, null);
                    if (binCount >= TREEIFY_THRESHOLD - 1) // -1 for 1st
                        treeifyBin(tab, hash);
                    break;
                }
                //如果有相同hash节点则跳出循环
                if (e.hash == hash &&
                    ((k = e.key) == key || (key != null && key.equals(k))))
                    break;
                //这步操作是用来遍历链表, e = p.next && p = e => p = p.next
                p = e;
            }
        }
        //e=null在新增时才出现
        if (e != null) { // existing mapping for key
            V oldValue = e.value;
            if (!onlyIfAbsent || oldValue == null)
                e.value = value;
            afterNodeAccess(e);
            return oldValue;
        }
    }
    ++modCount;
    //键值对数量大于阈值时进行扩容
    if (++size > threshold)
        resize();
    afterNodeInsertion(evict);
    return null;
}
```

//扩容方法,将表格大小初始化或加倍。如果为空，则根据字段阈值中的初始容量目标进行分配。

```java
final Node<K,V>[] resize() {
    Node<K,V>[] oldTab = table;
    int oldCap = (oldTab == null) ? 0 : oldTab.length;
    int oldThr = threshold;
    //新容量及门槛(cap*loadFactor(扩展因子))
    int newCap, newThr = 0;
    //扩容时
    if (oldCap > 0) {
        //如果原有大小已经是最大值了,则返回最大值,不做扩容处理
        if (oldCap >= MAXIMUM_CAPACITY) {
            threshold = Integer.MAX_VALUE;
            return oldTab;
        }
        //如果原有double后小于最大值且大于默认初始值,直接左移一位
        else if ((newCap = oldCap << 1) < MAXIMUM_CAPACITY &&
                 oldCap >= DEFAULT_INITIAL_CAPACITY)
            newThr = oldThr << 1; // double threshold
    }
    //初始化时,容量修改为之前构造函数中的数值
    else if (oldThr > 0) // initial capacity was placed in threshold
        newCap = oldThr;
    else {               // zero initial threshold signifies using defaults
        //0初始阈表示使用默认值
        newCap = DEFAULT_INITIAL_CAPACITY;
        newThr = (int)(DEFAULT_LOAD_FACTOR * DEFAULT_INITIAL_CAPACITY);
    }
    //获取新门槛值
    if (newThr == 0) {
        float ft = (float)newCap * loadFactor;
        newThr = (newCap < MAXIMUM_CAPACITY && ft < (float)MAXIMUM_CAPACITY ?
                  (int)ft : Integer.MAX_VALUE);
    }
    threshold = newThr;
    @SuppressWarnings({"rawtypes","unchecked"})
    //初始化底层数组
        Node<K,V>[] newTab = (Node<K,V>[])new Node[newCap];
    table = newTab;
    //不为空进入扩容阶段
    if (oldTab != null) {
        //迁移原有数据
        for (int j = 0; j < oldCap; ++j) {
            // e设置成每个hash桶的头结点,然后将原有头结点置空
            Node<K,V> e;
            if ((e = oldTab[j]) != null) {
                oldTab[j] = null;
                if (e.next == null)
                    //如果只有头结点有数据则重新计算hash桶位置后存入e
                    newTab[e.hash & (newCap - 1)] = e;
                else if (e instanceof TreeNode)
                    //如果e是树结构则需要分裂该树
                    ((TreeNode<K,V>)e).split(this, newTab, j, oldCap);
                else { // preserve order
                    //将链表分为 lowHead,lowTail,highHead,highTail
                    Node<K,V> loHead = null, loTail = null;
                    Node<K,V> hiHead = null, hiTail = null;
                    Node<K,V> next;
                    do {
                        next = e.next;
                        //遍历链表重新计算hash值,与原有桶大小逻辑与一直的存放在low链表中
                        //当e.hash对应的oldCap位为1,则进入high链表,否则进入low链表
                        //利用hash值与oldCap判断求模前的数值是否大于oldCap,以此分类
                        if ((e.hash & oldCap) == 0) {
                            if (loTail == null)
                                loHead = e;
                            else
                                loTail.next = e;
                            loTail = e;
                        }
                        else {
                            if (hiTail == null)
                                hiHead = e;
                            else
                                hiTail.next = e;
                            hiTail = e;
                        }
                    } while ((e = next) != null);
                    //最后将对应的high链表及low链表分别放到对应的桶中
                    if (loTail != null) {
                        loTail.next = null;
                        newTab[j] = loHead;
                    }
                    if (hiTail != null) {
                        hiTail.next = null;
                        newTab[j + oldCap] = hiHead;
                    }
                }
            }
        }
    }
    //返回扩容后的数组
    return newTab;
}
```

//获取数据

```java
final Node<K,V> getNode(int hash, Object key) {
    Node<K,V>[] tab; Node<K,V> first, e; int n; K k;
    //当对应的hash桶中有值时
    if ((tab = table) != null && (n = tab.length) > 0 &&
        (first = tab[(n - 1) & hash]) != null) {
        //如果头结点满足要求,则直接返回头结点
        if (first.hash == hash && // always check first node
            ((k = first.key) == key || (key != null && key.equals(k))))
            return first;
        if ((e = first.next) != null) {
            if (first instanceof TreeNode)
                //遍历树结构返回
                return ((TreeNode<K,V>)first).getTreeNode(hash, key);
            do {
                //遍历链表返回
                if (e.hash == hash &&
                    ((k = e.key) == key || (key != null && key.equals(k))))
                    return e;
            } while ((e = e.next) != null);
        }
    }
    return null;
}
```





 ![20170920,从网上转了一张图，据说来自美团，侵删](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/9/20/9839d16b2f8d488f3400dda385f993e1~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)



















参考:[位运算符号](https://blog.csdn.net/zhaoyunfei1/article/details/95312550)

[HashMap源码解析](https://juejin.cn/post/6844903491815604237#heading-4)

[深入源码解析HashMap 1.8](https://www.jianshu.com/p/8324a34577a0)

[HashMap的结构，1.7和1.8有哪些区别](https://zhuanlan.zhihu.com/p/109225949)