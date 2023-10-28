# ES聚合：聚合查询之Bucket聚合详解

## 聚合的引入

ES中**桶**的概念类似于SQL中的分组`GROUP BY`,而**指标**则类似于 `COUNT()` 、 `SUM()` 、 `MAX()` 等统计方法.

1. **桶(Buckets)**满足特定条件的文档的集合

2. **指标(Metrics)**对桶内的文档进行统计计算

ES包含**桶聚合（Bucket Aggregation)**,**指标聚合（Metric Aggregation)**和**管道聚合（Pipline Aggregation)**三种聚合方式.

## 理解Bucket聚合

桶聚合的种类有十几种,我们需要站在设计者的角度思考,将其按设计分类.大致可以分为三类.

![img](https://pdai.tech/images/db/es/es-agg-bucket-1.png)

## 按知识点学习聚合

**准备数据**

```sh
POST /test-agg-cars/_bulk
{ "index": {}}
{ "price" : 10000, "color" : "red", "make" : "honda", "sold" : "2014-10-28" }
{ "index": {}}
{ "price" : 20000, "color" : "red", "make" : "honda", "sold" : "2014-11-05" }
{ "index": {}}
{ "price" : 30000, "color" : "green", "make" : "ford", "sold" : "2014-05-18" }
{ "index": {}}
{ "price" : 15000, "color" : "blue", "make" : "toyota", "sold" : "2014-07-02" }
{ "index": {}}
{ "price" : 12000, "color" : "green", "make" : "toyota", "sold" : "2014-08-19" }
{ "index": {}}
{ "price" : 20000, "color" : "red", "make" : "honda", "sold" : "2014-11-05" }
{ "index": {}}
{ "price" : 80000, "color" : "red", "make" : "bmw", "sold" : "2014-01-01" }
{ "index": {}}
{ "price" : 25000, "color" : "blue", "make" : "ford", "sold" : "2014-02-12" }
```

### 标准聚合

利用terms桶操作,下列语句示意,对color字段进行group操作

```sh
GET /test-agg-cars/_search
{
    "size" : 0,
    "aggs" : { 
        "popular_colors" : { 
            "terms" : { 
              "field" : "color.keyword"
            }
        }
    }
}
# ps: es5之后移除了string类型,分为text和keyword,当需要对源字段进行操作时,需要精准匹配需要用keyword字段
```

上述语句特征:

1. 聚合操作包含在顶层参数 `aggs`/`aggregations`
2. 需要制定分组名称
3. 最后制定当个桶类型terms

返回结果如下

```json
{
  "took" : 2,
  "timed_out" : false,
  "_shards" : {
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 8,
      "relation" : "eq"
    },
    "max_score" : null,
    "hits" : [ ]
  },
  "aggregations" : {
    "popular_colors" : {
      "doc_count_error_upper_bound" : 0,
      "sum_other_doc_count" : 0,
      "buckets" : [
        {
          "key" : "red",
          "doc_count" : 4
        },
        {
          "key" : "blue",
          "doc_count" : 2
        },
        {
          "key" : "green",
          "doc_count" : 2
        }
      ]
    }
  }
}
```

1. 因为指定size为0所以没有hit返回
2. popular_colors在aggregations返回体重
3. key和color一对一,且每个返回一定有doc_count代表文档数量

### 多个聚合

同时对多个属性聚合

```sh
GET /test-agg-cars/_search
{
  "size": 0,
  "aggs": {
    "popular_colors": {
      "terms": {
        "field": "color.keyword"
      }
    },
    "make_by":{
      "terms": {
        "field": "make.keyword"
      }
    }
  }
}
```

返回结果如下

```json
{
  "took" : 2,
  "timed_out" : false,
  "_shards" : {
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 8,
      "relation" : "eq"
    },
    "max_score" : null,
    "hits" : [ ]
  },
  "aggregations" : {
    "popular_colors" : {
      "doc_count_error_upper_bound" : 0,
      "sum_other_doc_count" : 0,
      "buckets" : [
        {
          "key" : "red",
          "doc_count" : 4
        },
        {
          "key" : "blue",
          "doc_count" : 2
        },
        {
          "key" : "green",
          "doc_count" : 2
        }
      ]
    },
    "make_by" : {
      "doc_count_error_upper_bound" : 0,
      "sum_other_doc_count" : 0,
      "buckets" : [
        {
          "key" : "honda",
          "doc_count" : 3
        },
        {
          "key" : "ford",
          "doc_count" : 2
        },
        {
          "key" : "toyota",
          "doc_count" : 2
        },
        {
          "key" : "bmw",
          "doc_count" : 1
        }
      ]
    }
  }
}
```

可以看到,两个返回结果,分别按指定的分组条件返回.

### 聚合的嵌套

新的聚合层可以将下层度量嵌套在上层桶内

```sh
GET /test-agg-cars/_search
{
   "size" : 0,
   "aggs": {
      "colors": {
         "terms": {
            "field": "color.keyword"
         },
         "aggs": { 
            "avg_price": { 
               "avg": {
                  "field": "price" 
               }
            }
         }
      }
   }
}

```

上述语句得到的结果时计算每个颜色的平均售价,结果如下

```json
{
  "took" : 2,
  "timed_out" : false,
  "_shards" : {
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 8,
      "relation" : "eq"
    },
    "max_score" : null,
    "hits" : [ ]
  },
  "aggregations" : {
    "colors" : {
      "doc_count_error_upper_bound" : 0,
      "sum_other_doc_count" : 0,
      "buckets" : [
        {
          "key" : "red",
          "doc_count" : 4,
          "prices" : {
            "value" : 32500.0
          }
        },
        {
          "key" : "blue",
          "doc_count" : 2,
          "prices" : {
            "value" : 20000.0
          }
        },
        {
          "key" : "green",
          "doc_count" : 2,
          "prices" : {
            "value" : 21000.0
          }
        }
      ]
    }
  }
}
```

可以看到聚合嵌套的结果是将文档分组后,然后对每个分组的文档在进行聚合操作

### 动态脚本的聚合

ES还支持一些基于脚本(运行时生成的字段)的复杂动态聚合

```sh
GET /test-agg-cars/_search
{
  "runtime_mappings": {
    "make.length": {
      "type": "long",
      "script": "emit(doc['make.keyword'].value.length())"
    }
  },
  "size" : 0,
  "aggs": {
    "make_length": {
      "histogram": {
        "interval": 1,
        "field": "make.length"
      }
    }
  }
}
```

上述语句时对运行生成的key进行修改.在histogram中定义field名称未上述定义的`make.length`

直方图 `histogram`含义,每间隔 `interval`个对区间进行取值

## 按分类学习Bucket聚合

### 前置条件的过滤：filter

聚合嵌套,只不过首先过滤出匹配的所有文档,再对这些文档进行聚合

```sh
GET /test-agg-cars/_search
{
  "size": 0,
  "aggs": {
    "make_by": {
      "filter": {
        "term": {
          "make": "honda"
        }
      },
      "aggs": {
        "avg_prices": {
          "avg": {
            "field": "price"
          }
        }
      }
    }
  }
}
```

结果如下

```json
{
  "took" : 0,
  "timed_out" : false,
  "_shards" : {
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 8,
      "relation" : "eq"
    },
    "max_score" : null,
    "hits" : [ ]
  },
  "aggregations" : {
    "make_by" : {
      "doc_count" : 3,
      "avg_prices" : {
        "value" : 16666.666666666668
      }
    }
  }
}
```

计算出的avg_prices就是制造商为honda的平均价格

### 对filter进行分组聚合：filters

数据准备

```sh
PUT /test-agg-logs/_bulk?refresh
{ "index" : { "_id" : 1 } }
{ "body" : "warning: page could not be rendered" }
{ "index" : { "_id" : 2 } }
{ "body" : "authentication error" }
{ "index" : { "_id" : 3 } }
{ "body" : "warning: connection timed out" }
{ "index" : { "_id" : 4 } }
{ "body" : "info: hello pdai" }
```

对不同类型日志进行分组查询

```sh
GET /test-agg-logs/_search
{ 
  "size": 0, 
  "aggs": {
    "messages": {
      "filters": {
        "other_bucket_key": "other_message", 
        "filters": {
          "infos": {"match":{"body":"info"}},
          "warnings":{"match":{"body":"warning"}}
        }
      }
    }
  }
}
```

返回结果

```json
{
  "took" : 1,
  "timed_out" : false,
  "_shards" : {
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 4,
      "relation" : "eq"
    },
    "max_score" : null,
    "hits" : [ ]
  },
  "aggregations" : {
    "messages" : {
      "buckets" : {
        "infos" : {
          "doc_count" : 1
        },
        "warnings" : {
          "doc_count" : 2
        },
        "other_message" : {
          "doc_count" : 1
        }
      }
    }
  }
}
```

### 对number类型聚合：Range

用户定义每个范围代表一个桶,对多桶源进行聚合.聚合过程中,从每个存储区范围中检查从每个文档中提取的值，并“存储”相关/匹配的文档。请注意，此聚合包括from值，但不包括to每个范围的值(闭开区间)。

```sh
GET /test-agg-cars/_search
{
  "size": 0,
  "aggs": {
    "price_ranges": {
      "range": {
        "field": "price",
        "ranges": [
          { "to": 20000 },
          { "from": 20000, "to": 40000 },
          { "from": 40000 }
        ]
      }
    }
  }
}
```

返回结果

```json
GET /test-agg-cars/_search
{
  "size": 0,
  "aggs": {
    "avg_price": {
      "range": {
        "field": "price",
        "ranges": [
          {
            "to": 2000
          },
          {
            "from": 2000,
            "to": 40000
          },
          {
            "from": 40000
          }
        ]
      }
    }
  }
}
```

### 对IP类型聚合：IP Range

专用于对IP值的范围聚合

```sh
GET /ip_addresses/_search
{
  "size": 10,
  "aggs": {
    "ip_ranges": {
      "ip_range": {
        "field": "ip",
        "ranges": [
          { "to": "10.0.0.5" },
          { "from": "10.0.0.5" }
        ]
      }
    }
  }
}
```

返回结果

```json
{
  ...

  "aggregations": {
    "ip_ranges": {
      "buckets": [
        {
          "key": "*-10.0.0.5",
          "to": "10.0.0.5",
          "doc_count": 10
        },
        {
          "key": "10.0.0.5-*",
          "from": "10.0.0.5",
          "doc_count": 260
        }
      ]
    }
  }
}
```

- **CIDR Mask分组**

```sh
GET /ip_addresses/_search
{
  "size": 0,
  "aggs": {
    "ip_ranges": {
      "ip_range": {
        "field": "ip",
        "ranges": [
          { "mask": "10.0.0.0/25" },
          { "mask": "10.0.0.127/25" }
        ]
      }
    }
  }
}
```

返回

```json
{
  ...

  "aggregations": {
    "ip_ranges": {
      "buckets": [
        {
          "key": "10.0.0.0/25",
          "from": "10.0.0.0",
          "to": "10.0.0.128",
          "doc_count": 128
        },
        {
          "key": "10.0.0.127/25",
          "from": "10.0.0.0",
          "to": "10.0.0.128",
          "doc_count": 128
        }
      ]
    }
  }
}
```

- **增加key显示**

```sh
GET /ip_addresses/_search
{
  "size": 0,
  "aggs": {
    "ip_ranges": {
      "ip_range": {
        "field": "ip",
        "ranges": [
          { "to": "10.0.0.5" },
          { "from": "10.0.0.5" }
        ],
        "keyed": true // here
      }
    }
  }
}
```

返回

```json
{
  ...

  "aggregations": {
    "ip_ranges": {
      "buckets": {
        "*-10.0.0.5": {
          "to": "10.0.0.5",
          "doc_count": 10
        },
        "10.0.0.5-*": {
          "from": "10.0.0.5",
          "doc_count": 260
        }
      }
    }
  }
}

```

- **自定义key显示**

```sh
GET /ip_addresses/_search
{
  "size": 0,
  "aggs": {
    "ip_ranges": {
      "ip_range": {
        "field": "ip",
        "ranges": [
          { "key": "infinity", "to": "10.0.0.5" },
          { "key": "and-beyond", "from": "10.0.0.5" }
        ],
        "keyed": true
      }
    }
  }
}
```

### 对日期类型聚合：Date Range

专用于日期值的范围聚合

```sh
GET /test-agg-cars/_search
{
  "size": 0,
  "aggs": {
    "range": {
      "date_range": {
        "field": "sold",
        "format": "yyyy-MM-dd",
        "ranges": [
          { "from": "2014-01-01" },  
          { "to": "2014-12-31" } 
        ]
      }
    }
  }
}
```

ps:`format`的格式必须与`ranges`中的格式一致否则会报错

此聚合与Range聚合之间的主要区别在于 from和to值可以在[Date Math表达式  (opens new window)](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/search-aggregations-bucket-daterange-aggregation.html#date-format-pattern)中表示，并且还可以指定日期格式，通过该日期格式将返回from and to响应字段。请注意，此聚合包括from值，但**不包括to每个范围的值**

### 对柱状图功能：Histrogram

直方图 histogram 本质上是就是为柱状图功能设计的。创建柱状图需要设定区间.根据区间再聚合.

```sh
GET /test-agg-cars/_search
{
  "size": 0,
  "aggs": {
    "price": {
      "histogram": {
        "field": "price",
        "interval": 20000
      },
      "aggs": {
        "sum_num": {
          "sum": {
            "field": "price"
          }
        }
      }
    }
  }
}
```

1. histogram 桶要求两个参数：一个数值字段以及一个定义桶大小间隔。
2. sum 度量嵌套在每个售价区间内，用来显示每个区间内的总收入。
3. 区间返回[0~19999],[20000~39999]

返回结果

```json
{
  "took" : 1,
  "timed_out" : false,
  "_shards" : {
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 8,
      "relation" : "eq"
    },
    "max_score" : null,
    "hits" : [ ]
  },
  "aggregations" : {
    "price" : {
      "buckets" : [
        {
          "key" : 0.0,
          "doc_count" : 3,
          "sum_num" : {
            "value" : 37000.0
          }
        },
        {
          "key" : 20000.0,
          "doc_count" : 4,
          "sum_num" : {
            "value" : 95000.0
          }
        },
        {
          "key" : 40000.0,
          "doc_count" : 0,
          "sum_num" : {
            "value" : 0.0
          }
        },
        {
          "key" : 60000.0,
          "doc_count" : 0,
          "sum_num" : {
            "value" : 0.0
          }
        },
        {
          "key" : 80000.0,
          "doc_count" : 1,
          "sum_num" : {
            "value" : 80000.0
          }
        }
      ]
    }
  }
}
```

如果使用extended_stats聚合则会显示标准差,最大最小值等额外信息

```
GET /test-agg-cars/_search
{
  "size": 0,
  "aggs": {
    "makers": {
      "terms": {
        "field": "make.keyword",
        "size": 10
      },
      "aggs": {
        "stats": {
          "extended_stats": {
            "field": "price"
          }
        }
      }
    }
  }
}
```

返回

```json
{
  .....
  "aggregations" : {
    "makers" : {
      "doc_count_error_upper_bound" : 0,
      "sum_other_doc_count" : 0,
      "buckets" : [
        {
          "key" : "honda",
          "doc_count" : 3,
          "stats" : {
            "count" : 3,
            "min" : 10000.0,
            "max" : 20000.0,
            "avg" : 16666.666666666668,
            "sum" : 50000.0,
            "sum_of_squares" : 9.0E8,
            "variance" : 2.222222222222221E7,
            "variance_population" : 2.222222222222221E7,
            "variance_sampling" : 3.3333333333333313E7,
            "std_deviation" : 4714.045207910315,
            "std_deviation_population" : 4714.045207910315,
            "std_deviation_sampling" : 5773.502691896256,
            "std_deviation_bounds" : {
              "upper" : 26094.757082487296,
              "lower" : 7238.5762508460375,
              "upper_population" : 26094.757082487296,
              "lower_population" : 7238.5762508460375,
              "upper_sampling" : 28213.67205045918,
              "lower_sampling" : 5119.661282874156
            }
          }
        }
      ]
    }
  }
}
```























参考:[ES详解 - 聚合：聚合查询之Bucket聚合详解](https://pdai.tech/md/db/nosql-es/elasticsearch-x-agg-bucket.html)

[Elasticsearch中text与keyword的区别](https://www.cnblogs.com/hahaha111122222/p/12177377.html)