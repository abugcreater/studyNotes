# ES原理：ES原理之索引文档流程详解

## 文档索引步骤顺序

线上ES集群情况下文档索引步骤

### 单个文档

新建单个文档所需要的步骤顺序：

![img](https://pdai.tech/images/db/es/es-th-2-4.png)

1. 客户端想Node1发送,索引或者删除的请求
2. 节点使用文档的_id确定文档属于分片0.请求会被转发到Node3,因为分片0的主分片在Node3上
3. Node3请求执行成功后会同步到其他副本分片上,当所有的副本分片都成功时,Node3会向协调节点报告成功,协调节点向客户端报告成功



### 多个文档

使用bulk修改多个文档步骤顺序:

![img](https://pdai.tech/images/db/es/es-th-2-23.png)

1. 客户端想Node1发送bulk请求
2. Node1为每个节点创建一个批量请求,然后将这些请求转发到对应的主分片的节点上
3. 这些节点依次执行,并转发请求到对应的分片副本节点上.当所有的分片都执行成功会将成功消息转发到协调节点,协调节点将这些响应收集整理并返回给客户端.

## 文档索引过程详解

###  整体的索引流程

![img](https://pdai.tech/images/db/es/es-th-2-5.jpeg)

1. 协调节点(coordinating node)默认使用文档ID参与计算(也支持通过routing),以便路由到合适的节点
2. 当分片所在节点接受到请求后,会将这些请求先写到memory buffer中,然后定时写入到文件系统缓存(filesystem cache),这个写入的过程就叫做refresh
3. 会存在memory buffer和filesystem cache都丢失的情况,ES通过translog来保证数据的可靠性.其实现机制是接受到请求后,同事会写入到translog中然后每5s持久化磁盘,每隔30min或文件体积达到某个阈值会清空文件.这个过程叫做flush
4. flush过程中,内存中的缓存会被清除,内容写入新段(segment),段的fsync将创建一个新的提交点,并将内容刷盘,旧的translog将被删除并开始一个新的translog



### 分步骤看数据持久化过程

数据持久化的过程先后顺序可以分为: write,refresh,flush,merge

**write 过程**

![img](https://pdai.tech/images/db/es/es-th-2-6.png)

有新的文档写入时,先会写入到memory buffer和translog(事务日志)中,每一次ES操作都会写入到translog.

此时数据还没有写入到段(segment)中,还不可以被索引到.数据只有被refresh后才会被索引到.

- **refresh 过程**

![img](https://pdai.tech/images/db/es/es-th-2-7.png)

1. memory buffer中的文档写入到新的segment中,segment存储在filesystem cache中,此时文档可以被检索到
2. 清空memory buffer,此时translog中的文件没有被清空,需要等待到segment中的数据刷盘
3. refresh后segment写到文件系统缓存,避免了性能IO操作.refresh默认1s一次,性能损耗太大,可以通过设置 index.refresh_interval 设置 refresh （冲刷）间隔时间.

> 每个 segment 是一个包含正排（空间占比90~95%）+ 倒排（空间占比5~10%）的完整索引文件，每次搜索请求会将所有 segment 中的倒排索引部分加载到内存，进行查询和打分，然后将命中的文档号拿到正排中召回完整数据记录。如果不对segment做配置，就会导致查询性能下降
> 

- **flush 过程**

当translog越来越大,需要通过flush刷盘创建新的translog.flush也保证了文档不会丢失.

![img](https://pdai.tech/images/db/es/es-th-2-9.png)

1. 所有的内存缓冲区的文档都被刷入到新的段中
2. 缓冲区被清空
3. 一个commit point被写入到硬盘
4. 文件系统通过fsync被刷新
5. 老的translog被删除

- **merge 过程**

每次refresh都会创建新段,而每次搜索都必须轮流检索所有段,太多的段会导致性能消耗大,查询速度变慢.ES通过后台将这些小段合并成大段来解决这个问题.

![img](https://pdai.tech/images/db/es/es-th-2-10.png)

合并进程会选择大小相似的段进行合并,合并结束后老的段会被删除.

1. 合并完成
2. 将新的 segment 文件 flush 写入磁盘
3. 创建新的commit point文件,标识新的segment 文件,排除旧的和已合并的段
4. 打开新的端用于搜索
5. 删除旧的段文件



合并大的 segment 需要消耗大量的 I/O 和 CPU 资源，同时也会对搜索性能造成影响。所以 Elasticsearch 在默认情况下会对合并线程进行资源限制，确保它不会对搜索性能造成太大影响。默认归并线程的限速配置 `indices.store.throttle.max_bytes_per_sec `是 20MB.



### 段合并

#### 相关参数:

**mergeFactor**:当同一组的段数量达到该值时开始合并,默认为10

**SegmentSize**:段的实际大小,单位为字节

**minMergeSize**:小于这个值的会分到一组,加速小片段合并

**maxMergeSize**:段的值大于该值时就不参与合并,减少大段合并导致的资源消耗

#### 合并流程

**总体流程**

- 对段分组,将大小相近段分组,由LogMergePolicy1 类处理
- 将同一分组的段合并成一个大段

**分组流程**

1. 根据生产时间对segment排序**,计算段标准化值**并赋值给段

2. 在分组中找到最大的段,将该段标准化值作为上线,减去LEVEL_LOG_SPAN(默认值为0.75)后的值作为下线,这个区间中的段可以作为同一分组

3. 查找属于该分组的段,使用双指针(start,end)搜索

   1. start指向第一个段,end指向` start+MergeFactor`,end指针向前移动

   2. 找到标准值在区间中的端则停止,并将当前段到start指针的段同一分到一个组

      ps: 分组时不会过滤不满足条件的段
   
4. 分组后,在将不满足条件的超大段排除,如果数量不满足**mergeFactor**,则会去下一个分组继续寻找

5. 到段数量满足要求时,开始合并



**段合并的操作**分为两部分：

- **一个是正向信息合并，**例如存储域、词向量、标准化因子等。
- **一个是反向信息的合并，**例如词典、倒排表等。





## ES中更新文档流程

完整的更新请求如图所示.因为Lucene不支持部分字段更新,所以更新由ES完成

![img](https://pdai.tech/images/db/es/es-th-3-5.jpeg)

更新流程:

1. ES收到更新请求时,首先会获取同id的完整doc,记为V1
2. 然后会将V1的全量DOC和请求中的部分字段合并称为V2版本,然后记录到versionMap中,请求变更为index请求,加锁
3. 再次从versionMap中读取版本号最大的V2doc,如果没有则需要从segment或translog中读取
4. 比较版本冲突(v1==v2),如果存在版本冲突,则需要重新走update流程,如果不冲突则add
5. 在Index Doc阶段，首先将Version + 1得到V3，再将Doc加入到Lucene中去，Lucene中会先删同id下的已存在doc id，然后再增加新Doc。写入Lucene成功后，将当前V3更新到versionMap中。
6. 释放锁，部分更新的流程就结束了

![img](https://pdai.tech/images/db/es/es-th-3-6.jpeg)





## 关于ES分布式中的六大特性



**可靠性：**由于Lucene的设计中不考虑可靠性，在Elasticsearch中通过Replica和TransLog两套机制保证数据的可靠性。

**一致性：**Lucene中的Flush锁只保证Update接口里面Delete和Add中间不会Flush，但是Add完成后仍然有可能立即发生Flush，导致Segment可读。这样就没法保证Primary和所有其他Replica可以同一时间Flush，就会出现查询不稳定的情况，这里只能实现最终一致性。

**原子性：**Add和Delete都是直接调用Lucene的接口，是原子的。当部分更新时，使用Version和锁保证更新是原子的。

**隔离性：**仍然采用Version和局部锁来保证更新的是特定版本的数据。

**实时性：**使用定期Refresh Segment到内存，并且Reopen Segment方式保证搜索可以在较短时间（比如1秒）内被搜索到。通过将未刷新到磁盘数据记入TransLog，保证对未提交数据可以通过ID实时访问到。

**性能：**性能是一个系统性工程，所有环节都要考虑对性能的影响，在Elasticsearch中，在很多地方的设计都考虑到了性能，

- 一是不需要所有Replica都返回后才能返回给用户，只需要返回特定数目的就行；
- 二是生成的Segment现在内存中提供服务，等一段时间后才刷新到磁盘，Segment在内存这段时间的可靠性由TransLog保证；
- 三是TransLog可以配置为周期性的Flush，但这个会给可靠性带来伤害；
- 四是每个线程持有一个Segment，多线程时相互不影响，相互独立，性能更好；
- 五是系统的写入流程对版本依赖较重，读取频率较高，因此采用了versionMap，减少热点数据的多次磁盘IO开销。Lucene中针对性能做了大量的优化











参考[ES详解 - 原理：ES原理之索引文档流程详解](https://pdai.tech/md/db/nosql-es/elasticsearch-y-th-3.html)

[Elasticsearch搜索引擎：ES的segment段合并原理](https://blog.csdn.net/a745233700/article/details/117953198)

[掌握它才说明你真正懂 Elasticsearch - Lucene （二）](https://learnku.com/articles/40399#replies)

