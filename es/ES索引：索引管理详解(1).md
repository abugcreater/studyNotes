# ES索引：索引管理详解

## 索引管理的引入

当我们向ES中增加文档时,ES会自动创建他里面的字段类型.

示例如下:

我们向es中插入一个文档

```sh
PUT /customer/_doc/1
{
  "name": "John Doe"
}
```

es会自动创建一个customer的index

```json
{
  "customer" : {
    "mappings" : {
      "properties" : {
        "name" : {
          "type" : "text",
          "fields" : {
            "keyword" : {
              "type" : "keyword",
              "ignore_above" : 256
            }
          }
        }
      }
    }
  }
}
```

如果想对索引有更多的控制,比如确保索引有数量始终的主分片,在索引数据前,分析器和映射已经被建立好.那么就需要:1.**禁止自动创建索引**;2.**手动创建索引**

禁止自动创建索引需要通过修改es的配置,需要在 config/elasticsearch.yml 添加以下配置

```yaml
action.auto_create_index: false
```

通常情况下,该配置因为限制性太强.es会在启动时报错.

```
the [action.auto_create_index] setting value [false] is too restrictive.
```

这时候我们需要修改配置信息 

```yml
action.auto_create_index: .monitoring-kibana*,.monitoring-data*,.watches,.kibana,.watcher-history*,.monitoring-es*,.security,.triggered_watches,-test*
```

`-test*`代表针对`test`开头的文档不自动创建索引

`+test*`代表针对`test`开头的文档会自动创建索引



## 索引的格式

在请求体里面传入设置或类型映射，如下所示：

````sh
PUT /my_index
{
    "settings": { ... any settings ... }, #设置分片,副本信息
    "mappings": { #字段映射,类型等
        "properties": { ... any properties ... } #由于type在后续版本过期,所以暂不关心
    }
}
````

## 索引管理操作

### 创建索引

如下语句表示创建一个包括三个字段name,age,remarks.存储在一个分片一个副本上

```sh
#手动创建索引
PUT /test-index-users
{
  "settings": {
    "number_of_shards": 1, #存储分片数量
    "number_of_replicas": 1  #存储副本数量
  },
  "mappings": {
    "properties": {
      "name":{
        "type": "text",
        "fields": {
          "keyword":{
            "type": "keyword",
            "ignore_above":256 
          }
        }
      },
      "age":{
        "type": "long"
      },
      "remarks":{
        "type": "text"
      }
    }
  }  
}
```

ignore_above:表示设置超过指定字符后，超出的部分不能被索引或者存储

**Text与 keyword的区别**

> Text：会分词，然后进行索引
> 支持模糊、精确查询
> 不支持聚合
> keyword：不进行分词，直接索引
> 支持模糊、精确查询
> 支持聚合

- 插入测试数据

```sh
POST /test-index-users/_doc
{
  "name":"xh test name",
  "age":18,
  "remarks":"no remarks"
}
```

获取到的返回结果:

```json
{
  "_index" : "test-index-users",
  "_type" : "_doc",
  "_id" : "EOdDg4MBQTajtxCSOSku",
  "_version" : 1,
  "result" : "created", //创建成功
  "_shards" : {
    "total" : 2,
    "successful" : 1,
    "failed" : 0
  },
  "_seq_no" : 0,
  "_primary_term" : 1
}
```

- 插入不匹配的数据时

```sh
POST /test-index-users/_doc
{
  "name":"xh test name",
  "age":"error age type",
  "remarks":"no remarks"
}
```

返回结果如下: 会报类型不匹配错误`mapper_parsing_exception`

```json
{
  "error" : {
    "root_cause" : [
      {
        "type" : "mapper_parsing_exception",
        "reason" : "failed to parse field [age] of type [long] in document with id 'EedGg4MBQTajtxCSuykz'. Preview of field's value: 'error age type'"
      }
    ],
    "type" : "mapper_parsing_exception",
    "reason" : "failed to parse field [age] of type [long] in document with id 'EedGg4MBQTajtxCSuykz'. Preview of field's value: 'error age type'",
    "caused_by" : {
      "type" : "illegal_argument_exception",
      "reason" : "For input string: \"error age type\""
    }
  },
  "status" : 400
}
```

### 修改索引

通过`GET /_cat/indices?v&index=test-index-users 获取到所有的索引.可以看到我们刚创建的`test-index-users`索引信息如下:

```
health status index                           uuid                   pri rep docs.count docs.deleted store.size pri.store.size
yellow open   test-index-users                lI_1uKDnQt-Q16Zlbj-dSw   1   1          1            0      4.3kb          4.3kb
```

上面的列含义如下:

- pri primary缩写，主分片数量
- rep 副分片数量
- docs.count Lucene级别文档数量
- docs.deleted 删除文档
- store.size 全部分片大小（包含副本）
- pri.store.size 主分片大小

为什么我们这个索引的健康状态是yellow?

因为我们是单点环境,无法创建副本,但是在上述`number_of_replicas`配置中设置了副本数是1;所以我们需要修改索引的配置.

**修改副本数量为0**

```sh
#修改索引
PUT /test-index-users/_settings
{
  "settings":{
    "number_of_replicas":0
  }
}
```

修改完再查询会发现健康状态为green

### 打开/关闭索引

- **关闭索引**

  一旦索引被关闭,那么就**不能对该索引进行读写操作**,该索引只能显示元数据信息

  ![img](https://pdai.tech/_images/db/es/es-index-manage-7.png)

当我们插入数据时会显示`index_closed_exception`异常

```json
{
  "error" : {
    "root_cause" : [
      {
        "type" : "index_closed_exception",
        "reason" : "closed",
        "index_uuid" : "lI_1uKDnQt-Q16Zlbj-dSw",
        "index" : "test-index-users"
      }
    ],
    "type" : "index_closed_exception",
    "reason" : "closed",
    "index_uuid" : "lI_1uKDnQt-Q16Zlbj-dSw",
    "index" : "test-index-users"
  },
  "status" : 400
}
```

- **打开索引**

打开索引后就又可以进行读写操作了

```sh
# 打开索引
POST /test-index-users/_open
```

### 删除索引

最后我们将创建的test-index-users删除

```sh
#删除索引
DELETE /test-index-users
```

### 查看索引

- **mapping**

```sh
GET /bank/_mapping
```

- **settings**

```sh
GET /bank/_settings
```

##  Kibana管理索引

在Kibana如下路径，我们可以查看和管理索引

![img](https://pdai.tech/_images/db/es/es-index-manage-6.png)







参考:[ES详解 - 索引：索引管理详解](https://pdai.tech/md/db/nosql-es/elasticsearch-x-index-mapping.html)

[es /_cat/indices API](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/cat-indices.html#cat-indices-api-desc)

[《Elasticsearch:权威指南》cat APIs -- cat indices(查看索引信息)](https://blog.csdn.net/m0_45406092/article/details/107719165)