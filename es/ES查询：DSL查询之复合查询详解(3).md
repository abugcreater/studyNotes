# ES查询：DSL查询之复合查询详解

ES的复合查询是指,在查询中会有多种条件组合的查询.他提供了5中复合查询方式:**bool query(布尔查询)**、**boosting query(提高查询)**、**constant_score（固定分数查询）**、**dis_max(最佳匹配查询）**、**function_score(函数查询）**

## 复合查询引入

复合查询案例

```sh
#复合查询例子(bool)
GET bank/_search
{
  "query": {
    "bool": {
      "must": [
        {
          "match": {
            "age": "40"
          }
        }
      ],
      "must_not": [
        {"match": {
          "state": "ID"
        }}
      ]
    }
  }
}
```

这种查询就是本文要介绍的**复合查询**，并且bool查询只是复合查询一种。

## bool query(布尔查询)

布尔查询:通过布尔逻辑将较小的查询组合成较大的查询

### 概念

Bool查询语法的特点

- 子查询可以任意顺序出现
- 可以嵌套多个查询,包括bool查询
- 如果bool查询中没有must条件,should中必须至少满足一条才会返回结果

bool查询包含四种操作符,他们均是数组,数组里面是对应的判断条件

- `must`:必须匹配,贡献算分
- `must_not`:过滤子句,必须不能匹配,但不贡献算分
- `should`:过滤性匹配,至少满足一条.贡献算分
- `filter`:过滤子句,必须匹配,但不贡献算分

### 一些例子

- 例子1

```sh
#注意如果没有指定索引,那么该search语句会所有es中的所有索引
POST _search
{
  "query": {
    "bool" : {
      "must" : {
        "term" : { "user.id" : "kimchy" } 
      },
      "filter": {
        "term" : { "tags" : "production" }
      },
      "must_not" : {
        "range" : {
          "age" : { "gte" : 10, "lte" : 20 }
        }
      },
      "should" : [
        { "term" : { "tags" : "env1" } },
        { "term" : { "tags" : "deployed" } }
      ],
      "minimum_should_match" : 1,
      "boost" : 1.0
    }
  }
}

```

在filter元素下指定的查询对评分没有影响 , 评分返回为0。分数仅受已指定查询的影响。

term表示完全匹配,而match会分词匹配

- 例子2

```sh
GET _search
{
  "query": {
    "bool": {
      "filter": {
        "term": {
          "status": "active"
        }
      }
    }
  }
}
```

filter元素只有过滤和不过滤,因此没有指定评分查询

- 例子3

```sh
GET _search
{
  "query": {
    "bool": {
      "must": {
        "match_all": {}
      },
      "filter": {
        "term": {
          "status": "active"
        }
      }
    }
  }
}
```

此bool查询具有match_all查询，该查询为所有文档指定1.0分。

- 例子4

```sh
GET /_search
{
  "query": {
    "bool": {
      "should": [
        { "match": { "name.first": { "query": "shay", "_name": "first" } } },
        { "match": { "name.last": { "query": "banon", "_name": "last" } } }
      ],
      "filter": {
        "terms": {
          "name.last": [ "banon", "kimchy" ],
          "_name": "test"
        }
      }
    }
  }
}
```

根据索引bank改造后如下

```sh
GET bank/_search
{
  "query": {
    "bool": {
      "should": [
        {
          "match": {
            "firstname": {
              "query": "Elinor",
              "_name": "first"
            }
          }
        },
        {
          "match": {
            "lastname": {
              "query": "Ratliff",
              "_name": "last"
            }
          }
        }
      ],
      "filter": [
        {
          "terms": {
            "account_number": [20,21],
            "_name": "check"
          }
        }
      ]
    }
  }
}
```

每个query条件都可以有一个`_name`属性，用来追踪搜索出的数据到底match了哪个条件。

## boosting query(提高查询)

> 不同于bool查询，bool查询中只要一个子查询条件不匹配那么搜索的数据就不会出现。而boosting query则是降低显示的权重/优先级（即score)。

### 概念

对于只满足部分条件的数据，不是不显示，而是降低显示的优先级（即score)

返回与`positive`匹配的文档，同时减少与`negative`查询匹配的文档的相关性得分。

可以使用`boosting` 查询降级某些文档，而不将它们从搜索结果中排除。

### 例子

```sh
### 首先插入数据
POST test-dsl-boosting/_bulk
{ "index": { "_id": 1 }}
{ "content":"Apple Mac" }
{ "index": { "_id": 2 }}
{ "content":"Apple Fruit" }
{ "index": { "_id": 3 }}
{ "content":"Apple employee like Apple Pie and Apple Juice" }

### 查询案例
GET test-dsl-boosting/_search
{
  "query": {
    "boosting": {
      "positive": {
        "term": {
          "content": {
            "value": "apple"
          }
        }
      },
      "negative": {
        "term": {
          "content": {
            "value": "pie"
          }
        }
      },
      "negative_boost": 0.2
    }
  }
}
```

该查询语句表示优先匹配positive中的查询条件,降低匹配negative的权重,

### boosting 的 Top-level 参数
- `positive`:(必须，查询对象)您希望运行的查询。任何返回的文档都必须匹配此查询。

- `nagative`:(必须, 查询对象)用来降低匹配文档的相关性得分的查询。

如果返回的文档匹配positive查询和此查询，boosting查询将计算该文档的最终相关性得分，如下所示:

1. 从positive查询中取原始的相关性分数。

2. 将分数乘以negative_boost值。

- `negative_boost`:(Required, float) 0到1.0之间的浮点数，用于降低与negative查询匹配的文档的相关性得分。


## constant_score（固定分数查询）

> 查询某个条件时，固定的返回指定的score；显然当不需要计算score时，只需要filter条件即可，因为filter context忽略score。

常用于只需要执行一个filter而没有其他查询(例如评分查询)的情况下.term查询被放置在constant_score中，转换成不评分的filter。这种方式可以用来只有filter的bool查询中。

### 案例

```sh
## 固定分数查询 constant_score
### 插入数据
POST /test-dsl-constant/_bulk
{ "index": { "_id": 1 }}
{ "content":"Apple Mac" }
{ "index": { "_id": 2 }}
{ "content":"Apple Fruit" }

### 固定分数查询
GET /test-dsl-constant/_search
{
  "query": {
    "constant_score": {
      "filter": {
        "term": {
          "content": "mac"
        }
      },
      "boost": 1.2
    }
  }
}
```

查询结果

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
      "value" : 1,
      "relation" : "eq"
    },
    "max_score" : 1.2,
    "hits" : [
      {
        "_index" : "test-dsl-constant",
        "_type" : "_doc",
        "_id" : "1",
        "_score" : 1.2,
        "_source" : {
          "content" : "Apple Mac"
        }
      }
    ]
  }
}
```

### 相关参数

**`filter`**

（必需，查询对象）[过滤](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/query-dsl-bool-query.html)您希望运行的查询。任何返回的文档都必须与此查询匹配。

过滤器查询不计算[相关性分数](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/query-filter-context.html#relevance-scores)。为了提高性能，Elasticsearch 会自动缓存常用的过滤器查询。

**`boost`**

（可选，float）浮点数，用作 与查询匹配的每个文档 的**恒定**[相关性分数。](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/query-filter-context.html#relevance-scores)`filter`默认为`1.0`.

## dis_max(最佳匹配查询）

> 分离最大化查询（Disjunction Max Query）指的是： 将任何与任一查询匹配的文档作为结果返回，但只将最佳匹配的评分作为查询的评分结果返回 。

###  例子

```sh
## 最佳匹配 dis_max
POST /test-dsl-dis-max/_bulk
{ "index": { "_id": 1 }}
{"title": "Quick brown rabbits","body":  "Brown rabbits are commonly seen."}
{ "index": { "_id": 2 }}
{"title": "Keeping pets healthy","body":  "My quick brown fox eats rabbits on a regular basis."}
```

首先使用bool查询,查看结果

```sh
###bool查询
GET /test-dsl-dis-max/_search
{
    "query": {
        "bool": {
            "should": [
                { "match": { "title": "Brown fox" }},
                { "match": { "body":  "Brown fox" }}
            ]
        }
    }
}
```



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
      "value" : 2,
      "relation" : "eq"
    },
    "max_score" : 0.90425634,
    "hits" : [
      {
        "_index" : "test-dsl-dis-max",
        "_type" : "_doc",
        "_id" : "1",
        "_score" : 0.90425634,
        "_source" : {
          "title" : "Quick brown rabbits",
          "body" : "Brown rabbits are commonly seen."
        }
      },
      {
        "_index" : "test-dsl-dis-max",
        "_type" : "_doc",
        "_id" : "2",
        "_score" : 0.77041256,
        "_source" : {
          "title" : "Keeping pets healthy",
          "body" : "My quick brown fox eats rabbits on a regular basis."
        }
      }
    ]
  }
}
```

#### 了解should的分数计算规则

doc分数 = title:(match brown + match fox + match brown fox) + body:(match brown + match fox + match brown fox)

###  引入了dis_max

不使用 bool 查询，可以使用 dis_max 即分离 最大化查询（Disjunction Max Query） 。分离（Disjunction）的意思是 或（or） ，这与可以把结合（conjunction）理解成 与（and） 相对应。分离最大化查询（Disjunction Max Query）指的是： 将任何与任一查询匹配的文档作为结果返回，但只将最佳匹配的评分作为查询的评分结果返回 ：

```sh
## 分离查询
GET /test-dsl-dis-max/_search
{
  "query": {
    "dis_max": {
      "tie_breaker": 0.7,
      "boost": 1.2,
      "queries": [
        {
          "match": {
            "title": "Brown fox"
          }
        },
        {
          "match": {
            "body": "Brown fox"
          }
        }
      ]
    }
  }
}
```

返回结果

```json
{
  "took" : 8,
  "timed_out" : false,
  "_shards" : {
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 2,
      "relation" : "eq"
    },
    "max_score" : 1.0091083,
    "hits" : [
      {
        "_index" : "test-dsl-dis-max",
        "_type" : "_doc",
        "_id" : "1",
        "_score" : 1.0091083,
        "_source" : {
          "title" : "Quick brown rabbits",
          "body" : "Brown rabbits are commonly seen."
        }
      },
      {
        "_index" : "test-dsl-dis-max",
        "_type" : "_doc",
        "_id" : "2",
        "_score" : 0.9244951,
        "_source" : {
          "title" : "Keeping pets healthy",
          "body" : "My quick brown fox eats rabbits on a regular basis."
        }
      }
    ]
  }
}
```

- **dis_max 条件的计算分数**

分数 = 第一个匹配条件分数 + tie_breaker * 第二个匹配的条件的分数 ...

## function_score(函数查询）

>  简而言之就是用自定义function的方式来计算_score。

主要用到的自定义function类型:

- `script_score` 使用自定义的脚本来完全控制分值计算逻辑。如果你需要以上预定义函数之外的功能，可以根据需要通过脚本进行实现。
- `weight` 对每份文档适用一个简单的提升，且该提升不会被归约：当weight为2时，结果为2 * _score。
- `random_score` 使用一致性随机分值计算来对每个用户采用不同的结果排序方式，对相同用户仍然使用相同的排序方式。
- `field_value_factor` 使用文档中某个字段的值来改变_score，比如将受欢迎程度或者投票数量考虑在内。
- `衰减函数(Decay Function)` - `linear`，`exp`，`gauss

### 例子

最简单的random_score为例

```sh
GET bank/_search
{
  "query": {
    "function_score": {
      "query": {"match_all": {}},
      "boost": 3,
      "random_score": {},
      "boost_mode": "multiply"
    }
  }
}
```

进一步的，它还可以使用上述function的组合(functions)

```sh
GET /_search
{
  "query": {
    "function_score": {
      "query": { "match_all": {} },
      "boost": "5", 
      "functions": [
        {
          "filter": { "match": { "test": "bar" } },
          "random_score": {}, 
          "weight": 23
        },
        {
          "filter": { "match": { "test": "cat" } },
          "weight": 42
        }
      ],
      "max_boost": 42,
      "score_mode": "max",
      "boost_mode": "multiply",
      "min_score": 42
    }
  }
}

```

script_score 可以使用如下方式

```sh
GET /_search
{
  "query": {
    "function_score": {
      "query": {
        "match": { "message": "elasticsearch" }
      },
      "script_score": {
        "script": {
          "source": "Math.log(2 + doc['my-int'].value)"
        }
      }
    }
  }
}
```














参考:[ES详解 - 查询：DSL查询之复合查询详解](https://pdai.tech/md/db/nosql-es/elasticsearch-x-dsl-com.html)

[Lucene 的评分机制](https://github.com/felayman/elasticsearch-full/blob/master/%E6%94%B6%E8%97%8F%E5%A5%BD%E6%96%87/Lucene%20%E7%9A%84%E8%AF%84%E5%88%86%E6%9C%BA%E5%88%B6.md)

[es中的term和match的区别](https://www.jianshu.com/p/d5583dff4157)

[elasticsearch 查询（match和term）](https://www.cnblogs.com/yjf512/p/4897294.html)

[组合查询之Boosting Query](https://blog.csdn.net/qq_40179479/article/details/107700846)

[es中批量操作之bulk](https://blog.csdn.net/weixin_39723544/article/details/109237175)

[官方文档](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/query-dsl.html)