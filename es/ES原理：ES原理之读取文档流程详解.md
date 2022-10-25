# ES原理：ES原理之读取文档流程详解

## 文档查询步骤顺序

### 单文档查询

![img](https://pdai.tech/_images/db/es/es-th-2-21.png)

1. 客户端向 Node 1 发送获取请求。
2. 节点使用文档的 _id 来确定文档属于分片 0 。分片 0 的副本分片存在于所有的三个节点上。 在这种情况下，它将请求转发到 Node 2 。
3. Node 2 将文档返回给 Node 1 ，然后将文档返回给客户端。

协调节点请求时轮询所有副本达到负载均衡.由于ES不是实时的,存在主分片能成功返回,而副分片报告分片不存在的情况.

### 多个文档

![img](https://pdai.tech/_images/db/es/es-th-2-22.png)

使用mget请求,获取多个文档步骤:

1. 客户端向 Node 1 发送 mget 请求。
2. Node 1 为每个分片构建多文档获取请求，然后并行转发这些请求到托管在每个所需的主分片或者副本分片的节点上。一旦收到所有答复， Node 1 构建响应并将其返回给客户端。

## 文档读取过程详解

一般的搜索流程是第一阶段先获取doc_id,第二阶段通过id获取完整文档,在ES中被称为`query_then_fetch`

![img](https://pdai.tech/_images/db/es/es-th-2-32.jpeg)

1. 查询阶段,广播请求到所有分片,然后创建一个优先级队列对返回请求的的全局排序,大小为from+size
2. 每个分片返回各自优先级队列中所有文档ID和排序值给协调节点
3. 协调节点确定要取回的文档,并通过DocID想分片请求获取文档详细信息

### Elasticsearch的读

![img](https://pdai.tech/_images/db/es/es-th-3-7.jpeg)

存在以上三个数据节点,P1是主分片,其他为副本分片.

查询的时候，从三个节点中根据Request中的preference参数选择一个节点查询。preference可以设置_local，_primary，_replica以及其他选项。如果选择了primary，则每次查询都是直接查询Primary，可以保证每次查询都是最新的。如果设置了其他参数，那么可能会查询到R1或者R2，这时候就有可能查询不到最新的数据。



Elasticsearch中的查询主要分为两类，**Get请求**：通过ID查询特定Doc；**Search请求**：通过Query查询匹配Doc。

![img](https://pdai.tech/_images/db/es/es-th-3-9.jpeg)



对于**Search类请求**，查询的时候是一起查询内存和磁盘上的Segment，最后将结果合并后返回。这种查询是近实时（Near Real Time）的，主要是由于内存中的Index数据需要一段时间后才会刷新为Segment。

对于**Get类请求**，查询的时候是先查询内存中的TransLog，如果找到就立即返回，如果没找到再查询磁盘上的TransLog，如果还没有则再去查询磁盘上的Segment。这种查询是实时（Real Time）的。这种查询顺序可以保证查询到的Doc是最新版本的Doc，这个功能也是为了保证NoSQL场景下的实时性要求。

### Elasticsearch查询流程

以最常见的的二阶段查询为例:

![img](https://pdai.tech/_images/db/es/es-th-2-31.jpeg)

#### Client Node

1. **Get Remove Cluster Shard**:获取其他集群要访问的Shard列表。
2. **Get Search Shard Iterator**:获取当前Cluster中要访问的Shard，和上一步中的Remove Cluster Shard合并,构建出最终要访问的完整Shard列表
3. **For Every Shard:Perform**:遍历shard,并执行逻辑
4. **Send Request To Query Shard**:发送请求,等待返回结果
5. **Merge Docs**:异步等待返回结果，然后对结果合并
6. **Send Request To Fetch Shard**:选出Top N个Doc ID后发送给这些Doc ID所在的Shard执行Fetch Phase，最后会返回Top N的Doc的内容



#### Query Phase

1. **Create Search Context**:创建Search Context，之后Search过程中的所有中间状态都会存在Context中
2. **Parse Query**:解析Query的Source，将结果存入Search Context
3. **Get From Cache**:判断请求是否允许被Cache，如果允许，则检查Cache中是否已经有结果，如果有则直接读取Cache
4. **Add Collectors**:Collector主要目标是收集查询结果，实现排序，对自定义结果集过滤和收集等
5. **lucene::search**:调用Lucene中IndexSearch的search接口，执行真正的搜索逻辑
6. **rescore**:根据Request中是否包含rescore配置决定是否进行二阶段排序，如果有则执行二阶段算分逻辑，会考虑更多的算分因子
7. **suggest::execute()**:有推荐请求，则在这里执行推荐请求
8. **aggregation::execute()**:有聚合统计请求，则在这里执行

#### Fetch Phase

Fetch阶段的目的是通过DocID获取到用户需要的完整Doc内容。这些内容包括了DocValues，Store，Source，Script和Highlight等，具体的功能点是在SearchModule中注册的

















参考:[ES详解 - 原理：ES原理之读取文档流程详解](https://pdai.tech/md/db/nosql-es/elasticsearch-y-th-4.html)