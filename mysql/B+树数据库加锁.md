# B+树数据库加锁

## 1. B+Tree

**B+树**: 所有的数据信息都只存在于叶子节点，中间的非叶子节点只用来存储索引信息，这种选择进一步的增加了中间节点的索引效率，使得内存可以缓存尽可能多的索引信息，减少磁盘访问；除根节点以外，每个节点的键值对个数需要介于M/2和M之间，超过或者不足需要做分裂或者合并.

![B+Tree](http://catkang.github.io/assets/img/btree_lock/btree.jpg)

每个节点中包含一组有序的key，介于两个连续的有序key，key+1中间的是指向一个子树的指针，而这个子树中包含了所有取值在[key, key+1)的记录.

![B+Tree Split](http://catkang.github.io/assets/img/btree_lock/btree_split.jpg)

当节点中的记录插入超过M时，需要触发Split.分裂的过程可能继续向更上层传到。与之对应的是从树中删除数据时，可能触发节点合并，同样需要修改其父节点，同样可能向更上层传导。

## 2. Lock

并发控制中的Lock跟我们在多线程编程中保护临界区的Lock不是一个东西.Lock通常有不同的Mode，如常见的读锁SL，写锁WL，不同Mode的锁相互直接的兼容性可以用兼容性表来表示：

![Lock Mode](http://catkang.github.io/assets/img/btree_lock/lock_mode.jpg)

# 问题

加锁方案主要关注B+Tree加锁策略的两个指标：

- **并发度（Concurrency）**：在满足正确性的前提下，能够支持多少种相同或不同的数据库操作的并发。
- **加锁开销（Locking Overhead）**：通过调度模块请求加锁，其本身的开销大小。

# 传统的加锁策略

### 2PL

该加锁策略**认为树节点是最小的加锁单位**.因为B+Tree需要从根乡下搜索,所以事务需要持有从根节点到叶子节点上路径上的所有锁.而两阶段锁(2PL)又要求所有这些锁都持有到事务Commit,而任何插入或删除操作都可能导致节点**分裂或合并（Structure Modification Operations, SMO）**,而分裂或合并都可能向上传导到根节点.所以需要对根节点加写锁WL,也就是说任何时刻只允许一个包含Insert或Delete操作的事务进行.严重影响并发度.

### Tree Protocol

Tree Protocol放松了2PL限制,允许部分提前放锁,任然能够保证Serializable:

- 先对root加锁
- 之后对下一层节点加锁必须持有其父节点的锁
- 任何时候可以放锁
- 对任何元素在放锁后不能再次加锁

这种加锁方式被称为Lock Coupling.虽然有提前放锁，但自root开始的访问顺序保证了对任何节点，事务的加锁顺序是一致的，因此仍然保证Seralizable.

**存在的问题:**如果使用乐观方式,因为乐观方式任务SMO并不会经常发生,所以所有节点都是加读锁,当叶子节点需要分裂时,要按照搜索路径把所有读锁升级成写锁（**Upgrade Lock**）。当两个持有同一个节点读锁的事务同时想要升级写锁时，就会发生死锁，为了避免这种情况，引入了**Update Lock Mode**，只有Update Lock允许升级，并且Update Lock之间不兼容，这其实是一种权衡。

### Blink Tree

2PL和Tree Protocol中面临的最大问题其实在于：**节点的SMO时需要持有其父节点的写锁**。Blink Tree巧妙的提出对所有节点增加右向指针，指向其Sibling(兄弟)节点，这是个非常漂亮的解决方案，因为他其实是提供了一种新的节点访问路径，让上述这种SMO的中间状态，变成一种合法的状态，从而避免了SMO过程持有父节点的写锁。

![Blink Tree](http://catkang.github.io/assets/img/btree_lock/blink_tree.jpg)

## 对Record加Lock而不是Page

### ARIES/KVL 峰回路转

ARIES/KVL首先**明确的区分了B+Tree的物理内容和逻辑内容**，逻辑内容就是存储在B+Tree上的那些数据记录，而B+Tree自己本身的结构属于物理内容，物理内容其实事务是不关心的，那么节点分裂、合并这些操作其实只是改变了B+Tree的物理内容而不是逻辑内容.

ARIES/KVL就将这些从Lock的保护中抽离出来，也就是Lock在Record上加锁，对物理结构则通过Latch来保护其在多线程下的安全.Latch只需要在临界区代码前后持有,而不需要整个事务生命周期持有.更多Lock和Latch的区别见下表：

![Lock VS Latch](http://catkang.github.io/assets/img/btree_lock/lock_latch.jpg)

#### Latch保护物理结构

Latch才是我们在多线程编程中熟悉的，保护临界区访问的锁。传统的B+Tree加锁方式的优化，也可以直接无缝转化成Latch,作用对象从事务变成线程.

#### Lock保护逻辑结构

事务的并发控制不再需要考虑树本身的结构变化.假设事务T1要查询、修改、删除或插入一个Record X，而Record X所在的Page为q，那么加Lock过程就变成这样：

```
Search（X）and Hold Latch(q);

SLock(X) or WLock(X);

// Fetch, Insert or Delete
Unlatch(q);

....

T Commit and Unlock(X)
```

首先对Btree的查找过程,最终持有Record所在叶子结点的Latch.

然后向调度器申请Record Lock，如果能马上满足，那么这个Page Latch就可以释放了.如果Lock等待则先对Record加**Conditional Lock**，这种Lock如果不能满足会立即返回而不是阻塞等待，如果失败则先释放Page Latch，再对Record 加**Unconditional Lock**来阻塞等待.

等这个Lock可以满足并返回的时候,判断叶子节点或祖先节点有没有发生变化,如果有则需要从未变化的节点重新搜索.

### Key Range Locking

传统的解决幻读的方案是**谓词锁(Predicate Lock)**，也就是直接对查询条件加锁，每次插入删除需要判断是否与现有的判断条件冲突，但通用数据库中，条件千变万化，判断条件冲突这件事情开销极大。在B+Tree上，由于其Key有序的特点，通常会采用**Key Range Locking**，也就是对Key加锁，来保护其旁边的区间Range，有很多中选择，如下图所示：

![Key Range Lock](http://catkang.github.io/assets/img/btree_lock/key_range_lock.jpg)

最常见的一种实现是**Next Key Locking**,也就是上图中最上面的一条线，通过对1174加锁，来保护1174这个节点以及其前面和1171之间的Gap.

假设当前Record X的下一条记录是Y，且都在Page q，X和Y都满足条件查询事务T1的查询条件，T1会重复上面的加锁过程并持有X和Y上的Lock。这时事务T2尝试在X和Y之间插入Record M，我们看看它的加锁过程：

```java
Search（M，Y）and Hold Latch(q);

XLock(Y);

XLock(M);

Insert M into q

Unlatch(q);

....

T2 Commit and Unlock(M), Unlock(Y)
```

跟之前相比，这里多了对Next Key也就是Y加锁，正是由于这个锁，可以发现持有Y上SLock，并且还没有提交的查询事务T1，等待直到T1 完成Commit并放锁。为了追求更高的并发度，会有一些手段来改进Key Range Locking.

#### Instant Locking

在上述加锁过程中如果已经是最大则需要对正无穷加锁,并持有整个事务声明周期.在高频率顺序插入场景会成为瓶颈.Instant Locking只在瞬间持有这把Next Key Locking，其加锁是为了判断有没有这个Range冲突的读操作，但**获得锁后并不持有，而是直接放锁**。

如下是引入Instant Locking后的Insert加Next Key Lock的流程：

```
Search（M，Y）and Hold Latch(q);

XLock(Y);
Unlock(Y)

XLock(M);

Insert M into q

Unlatch(q);
....

T Commit and Unlock(M)
```

Y上Lock的获取和释放，和插入新的Record两件事情是在Page q的Latch保护下进行的，因此这个中间过程是不会有其他事务进来的，等Latch释放的时候，新的Record其实已经插入，这个X到Y的区间已经被划分成了，X到M以及M到Y，新的事务只需要对自己关心的Range加锁即可.

可以提前放锁的根本原因是：Insert的New Record在其他事务对这个Range加锁的时候已经可见。Delete就没有这么幸运了，因为Delete之后这个Key就不可见了，因此Delete的持有的Next Key Locking似乎不能采用Instant Locking，这个时候Ghost Record就派上用场了。

#### Ghost Records

Ghost Record给每个记录增加了一位Ghost Bit位，正常记录为0，当需要删除某个Record的时候，直接将其Ghost Bit修改为1即可，正常的查询会检查Ghost Bit，如果发现是1则跳过.Ghost Record可以跟正常的Record一样作为Key Range Lock的加锁对象的。相当于把删除操作变为更新操作.

由于Ghost Record对用户请求是不可见的，因此这个删除过程只是对物理结构的改变，也不需要加Lock。真正的删除操作会放到后台异步进行,该过程可能涉及节点合并,因此需要获取Latch,**在Lock的Latch保护下对Lock Range合并**.

#### ARIES/IM

单个表常常会有大量的二级索引，也就是大量的B+Tree，显然对每个B+Tree分别加锁开销是很大的。将加锁对象由B+Tree上的Key-Value变成了其最终指向的数据，这样无论有多少二级索引最终都只需要加一把锁。显而易见的，这种做法在降低锁开销的同时也降低了并发度。另外，从这个例子中可以清楚地看到：**对Locking Resource的选择是在Concurrency 和 Locking Overhead之间的权衡**。

#### KRL

首先是Range Lock的加锁范围，KVL和IM中的Range都是对Next Key以及中间的Gap同时加锁，一定程度上限制了对Key和对Gap访问的并发，KRL提出将二者拆分，分别加锁。这种选择在提高并发度的同时，由于需要加更多的锁而增加了加锁开销。第二个改进提出了更精确的锁Mode，包括Intention Update，Intention Insert以及Intention Delete，其基本思路就是**用更精确的锁mode区分操作类型从而更大程度的挖掘他们之间的并发可能**。

#### Hierarchical Locking

初衷是为了让大事务拿大锁，小事务拿小锁，来在事务并发度及加锁开销做权衡，常见的加锁层级包括对表加锁，对索引加锁，以及对Record加锁。Hierarchical Locking对高层级的加锁对象通常采用Intention Lock来增加并发， 比如Intention X Lock自己是相互兼容的。

Graefe G.在《Hierarchical locking in B-tree indexes》[14]中提出了可以在Table和Key之间增加更多的加锁粒度，其中探索了两种思路，一种是利用B+Tree的层级关系，在中间节点的Key上加Range Lock；另一种是对Key的前缀加锁，这其实是更接近传统谓词锁的一种方式

#### Early Lock Release

但如果拉长事务Commit的过程看，其包括进入Commit状态后内存状态的改变，以及等待Commit Log落盘的时间，其中等待落盘的时间通常又大大超过了修改内存状态的时间.

提出了可以在等待Commit Log落盘之前就释放锁的方案，以此来提高并发度.

## 总结

我们看到，B+Tree加锁的发展历史其实都是围绕着我们前面提到的两个主要问题进行的，即**提高并发度**和**降低锁开销**，而采用的手段通常包括：

1. **对Lock对象或粒度的选择**，比如从Page Lock到Key Lock，以及Hierarchical Locking。
2. **引入新的Lock Mode**，比如KRL以及没有提到的Increment Lock。
3. **缩短Lock持有时间**，比如Early Lock Release或Controlled Lock Violation

回顾整个的发展过程：

- 传统阶段将Btree Page当成最小的加锁单位。围绕如何减少事务需要持有的锁的Page数量而发展：提出了Tree Protocol的来实现提前释放祖先节点的Lock；又通过Blink Tree避免对父节点的加锁；
- Multilevel Transaction尝试改变加锁的最小单位，从对Page加锁变成对Record加锁；
- ARIES/KVL将逻辑内容和物理内容分离，由Lock和Latch分别保护，并提供了一套相对完善的对Record加锁的实现算法，Key Value Locking，也基本确定了后续的发展方向。
- ARIES/KVL，ARIES/IM以及后续的很多算法都采用Key Range Locking来解决幻读的问题，并采用Instant Locking及Ghost Locking来进一步提高并发度。
- 后续的研究多围绕权衡Lock粒度、新的Lock Mode及缩短Lock持有时间等方向继续前进。

如果用横坐标表示算法的并发度，纵坐标表示加锁开销，可以看到本文提到的算法之间的关系，如下图所示，注意这里只是定性的分析。通常认为，在可接受的锁开销范围内，更倾向于获得更高的并发度。因此图中红框的部分是笔者认为现代数据库应该做到的区域。

![Coordinator](http://catkang.github.io/assets/img/btree_lock/coordinator.png)





参考:[B+树数据库加锁历史](http://catkang.github.io/2022/01/27/btree-lock.html)