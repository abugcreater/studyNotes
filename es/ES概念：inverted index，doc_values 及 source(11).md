# ES概念：inverted index，doc_values 及 source

## Inverted index

ES中最基本的数据存储单位是shard.每个分片对应的都是一个Lucene中的索引(index),每个索引都包含几个Luncene segments.一个segment 包含映射到文档里的所有分词(terms)和倒排索引(inverted index).

![img](https://img-blog.csdnimg.cn/2021081716495985.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L1VidW50dVRvdWNo,size_16,color_FFFFFF,t_70)

倒排索引是ES的核心数据结构.当字符串导入到ES中,需要被结果词法,预发分析,然后创建倒排索引最后保存到磁盘.解析过程如下图:

![img](https://img-blog.csdnimg.cn/20200826094936129.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L1VidW50dVRvdWNo,size_16,color_FFFFFF,t_70)

倒排索引会根据terms指向所在的文档.terms在索引中是按照字母的先后顺序进行排序的.

**创建示例**

输入一下文档:

![img](https://img-blog.csdnimg.cn/2019101920455794.png)

那么得到的索引结果为:

| Term     | Frequency(频率) | Document (postings) |
| :------- | :-------------- | :------------------ |
| choice   | 1               | 3                   |
| day      | 1               | 2                   |
| is       | 3               | 1,2,3               |
| it       | 1               | 1                   |
| last     | 1               | 2                   |
| of       | 1               | 2                   |
| of       | 1               | 2                   |
| sunday   | 2               | 1,2                 |
| the      | 3               | 2,3                 |
| tomorrow | 1               | 1                   |
| week     | 1               | 2                   |
| yours    | 1               | 3                   |

Lucene使用BlockTree数据局结构,用来通过前缀快速查找terms.

![BlockTree](https://img-blog.csdnimg.cn/img_convert/ce5b544306dde183158828bed9c46f1f.png)



每个字符占据一个节点，树的叶子是术语。

通过该树搜索terms非常快,他提供了时间复杂度为O(logn)的实现.

1. terms按照字母顺序排列
2. Frequency列捕获了terms在整个文档集中出现的次数
3. 



ES在倒排索引的基础上做了很多创新,兼顾了搜索和分析.默认情况下ES文档中的每个字段都会创建一个倒排索引.

## Source

ES中每个文档的每一个字段都会被存储在shard里存放source的地方. source 字段用于存储 post 到 ES 的原始 json 文档.,以便在搜到这个文档时能够把原文返回给查询者。

source的存储可以通过查询反馈看到:

```json
{
  "_index" : "twitter",
  "_type" : "_doc",
  "_id" : "2",
  "_version" : 1,
  "_seq_no" : 1,
  "_primary_term" : 1,
  "found" : true,
  "_source" : {
    "user" : "双榆树-张三",
    "message" : "今儿天气不错啊，出去转转去",
    "uid" : 2,
    "age" : 20,
    "city" : "北京",
    "province" : "北京",
    "country" : "中国",
    "name" : {
      "firstname" : "三",
      "surname" : "张"
    },
    "address" : [
      "中国北京市海淀区",
      "中关村29号"
    ],
    "location" : {
      "lat" : "39.970718",
      "lon" : "116.325747"
    }
  }
}
```

可以看到source存储了整个文档信息.

source中的 Stored Fields数据结构 是一个简单的键值对key-value,提供了查找某个特定标题内容文件的解决方案.

![img](https://pdai.tech/_images/db/es/es-th-1-15.png)


## Doc_values

Doc values是ES为了提供sort，aggregtion 和访问脚本中的字段值需要不同的数据访问模式而使用的磁盘数据结构.几乎所有字段都支持doc值,但对字符串字段除外.存储与 _source 相同的值，但以面向列（column）的方式存储,Doc values 告诉你对于给定的文档 ID，字段的值是什么.

 doc values 保存在操作系统的磁盘中，当 doc values 大于节点的可用内存，ES 可以从操作系统页缓存中加载或弹出，从而避免发生 JVM 内存溢出的异常，docValues 远小于节点的可用内存，操作系统自然将所有Doc Values存于内存中（堆外内存），有助于快速访问


**示例**

插入以下文档

```sh
PUT cities
{
  "mappings": {
    "properties": {
      "city": {
        "type": "keyword"
      }
    }
  }
}
 
PUT cities/_doc/1
{
  "city": "Wuhan"
}
 
PUT cities/_doc/2
{
  "city": "Beijing"
}
 
PUT cities/_doc/3
{
  "city": "Shanghai"
}
```

得到的 doc_values 的一个列存储（Columnar store）表格:

| doc id | city     |
| :----- | :------- |
| 1      | Wuhan    |
| 2      | Beijing  |
| 3      | Shanghai |



### doc_values 与 source 的区别？使用 docvalue_fields 检索指定的字段？

post 提交到 ES 的原始 Json 文档都存储在 source 字段中，默认情况下，每次搜索的命中结果都包含文档 source，即使仅请求少量字段，也必须加载并解析整个 source 对象，而 source 每次使用时都必须加载和解析，所以使用 source 非常慢。为避免该问题，当我们只需要返回相当少的支持 doc_values 的字段时，可以使用 docvalue_fields 参数获取选定字段的值。

doc values 存储与 _source 相同的值，但在磁盘上基于列的结构中进行了优化，以进行排序和汇总。由于每个字段都是单独存储的，因此 Elasticsearch 仅读取请求的字段值，并且可以避免加载整个文档 _source。通过 docvalue_fields 可以从建好的列式存储结果中直接返回字段值，毕竟 source 是从一大片物理磁盘去，理论上从 doc values 处拿这个字段值会比 source 要快一点，页面抖动少一点。








参考:[Elasticsearch：inverted index，doc_values 及 source](https://elasticstack.blog.csdn.net/article/details/102642703)

[列式存储wiki](https://en.wikipedia.org/wiki/Column-oriented_DBMS)

[列存储相关概念和常见列式存储数据库](https://blog.csdn.net/zpf_940810653842/article/details/102824419)

[lasticSearch搜索引擎：常用的存储mapping配置项 与 doc_values详细介绍](https://blog.csdn.net/a745233700/article/details/117915118)