# ES原理：ES原理知识点补充和整体结构

## ElasticSearch整体结构

![img](https://pdai.tech/images/db/es/es-th-2-3.png)

上图所示的系统整体结构为:

- 有多个NODE(即ES实例),一个主节点两个从节点的一主两从架构
- 每个节点会有多个分片,这些分片分为主分片(P0,P1),副分片(R1,R0)
- 每个分片上对应着就是一个Lucene Index(底层索引文件)
- Lucene Index是一个统称
  - 一个Index由多个Segment(段文件,倒排索引)组成.每个Segment存储着就是Doc(文档)
  - commit point记录了所有segments的信息

## Lucene索引结构

Lucene的索引结构中文件组成

![img](https://pdai.tech/images/db/es/es-th-2-2.png)



从上图中可以看到 `.tip`,`.tim`文件存储了词典信息, `.doc`保存了倒排表信息即`Postings`

文件的关系如下：

![img](https://pdai.tech/images/db/es/es-th-3-2.jpeg)



## Lucene处理流程

![img](https://pdai.tech/images/db/es/es-th-3-21.jpeg)

**创建索引**过程:

1. 准备待索引文档,包括数据库,磁盘文件或网络等来源
2. 对文档内容进行分词组件处理,形成一系列的term(单词)
3. 索引组件对文档和Term处理,形成字典和倒排表

**搜索索引**过程:

1. 对查询语句进行词法,语法分析,形成一系列的Trem(单词)
2. 根据倒排索引表查找出包含Term的文档,并进行合并形成符合结果的文档集
3. 对比各文档得分,按照得分高低返回

## ElasticSearch分析器

分析包括:

1. 将文本拆分成适合倒排索引的独立词条
2. 将这些词条格式统一,提高他的可搜索性



分析器通过三个功能模块执行上述工作:

1. **字符过滤器**,字符串顺序通过字符过滤器.字符过滤器将整理这些字符串,去掉HTML,或者将&转化成and等
2. **分词器**,分词器将这些字符串分为单个词条.
3. **Token 过滤器**, token过滤器将这些词条转为标准格式,比如转为小写,删除无用词条如(a,and,the等),或者增加词条

Elasticsearch提供了开箱即用的字符过滤器、分词器和token 过滤器。 这些可以组合起来形成自定义的分析器以用于不同的目的。

###  内置分析器

ES提供几种不同的分析器,通过下述字符串比较区别:

```
"Set the shape to semi-transparent by calling set_trans(5)"
```

- **简单分析器**

简单分析器会在任何不是字母的地方分隔文本,并将词条小写.它会生产

```
set, the, shape, to, semi, transparent, by, calling, set, trans
```

- **标准分析器**

ES默认分析器.它根据Unicode联盟定义的**单词边界**划分文本.删除绝大部分标点,并将词条小写

```
set, the, shape, to, semi, transparent, by, calling, set_trans, 5
```

- **空格分析器**

跟进空格划分文本

```
Set, the, shape, to, semi-transparent, by, calling, set_trans(5)
```

- **语言分析器**

特定语言分析器可用于 很多语言。它们可以考虑指定语言的特点.英文分析器会将无关英语单词删除如(and,the等)

```
set, shape, semi, transpar, call, set_tran, 5
```

### 什么时候使用分析器

在查询时需要将查询字符串通过相同分析过程保证搜索的词条格式与索引中的词条格式一致.

全文查询,分为**全文域**和**精确值域**:

- **全文域**会对查询字符串应用相同的分析器,用来产生正确的搜索词条列表
- **精确值域**不会分析查询字符串,而是搜索你指定的精确值



示例

```sh
GET /_search?q=2014              # 12 results
GET /_search?q=2014-09-15        # 12 results !
GET /_search?q=date:2014-09-15   # 1  result
GET /_search?q=date:2014         # 0  results !
```

前两条语句使用类全文域:_all域,分词结果为 2014,09,15

后两条语句为精确域 date域,不对文本分词

所以导致前后两条匹配到的结果不同.





参考:[ES详解 - 原理：ES原理知识点补充和整体结构](https://pdai.tech/md/db/nosql-es/elasticsearch-y-th-2.html)

[Lucene文件信息](http://lucene.apache.org/core/7_2_1/core/org/apache/lucene/codecs/lucene70/package-summary.html#package.description)