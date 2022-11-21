# ES查询：DSL查询之全文搜索详解

## 知识体系构建

- 第一点：**全局观**，即我们现在学习内容在整个体系的哪个位置？

参考下图,了解当前学习内容.

![img](https://pdai.tech/_images/db/es/es-dsl-full-text-3.png)

- 第二点： **分类别**，从上层理解，而不是本身

- 第三点： **知识点还是API**？ API类型的是可以查询的，只需要知道大致有哪些功能就可以了.API型只需要理解意图,而不是所有接口/参数.

## Match类型

- 前期准备

```sh
PUT /test-dsl-match
{ "settings": { "number_of_shards": 1 }} 

POST /test-dsl-match/_bulk
{ "index": { "_id": 1 }}
{ "title": "The quick brown fox" }
{ "index": { "_id": 2 }}
{ "title": "The quick brown fox jumps over the lazy dog" }
{ "index": { "_id": 3 }}
{ "title": "The quick brown fox jumps over the quick dog" }
{ "index": { "_id": 4 }}
{ "title": "Brown fox brown dog" }
```

- 查询语句

```sh
GET /test-dsl-match/_search
{
    "query": {
        "match": {
            "title": "QUICK!"
        }
    }
}
```

ES执行match查询步骤是:

1. **检查字段类型**

   标题title字段是string类型(analyzed)已分析的全文字段,所以查询字符串本身也应该被分析

2. **分析查询字符串**

   将字符串`QUICK!`传入标准分析器中,输出的结果是单个项quick.因为只有一个单词,所以match执行单个底层term查询

3. **查找匹配文档**

   用term查询在倒排索引汇总查找quick然后获取一组包含该项的文档

4. **为每个文档评分**

   用 term 查询计算每个文档相关度评分` _score` ，这是种将词频（term frequency，即词 quick 在相关文档的 title 字段中出现的频率）和反向文档频率（inverse document frequency，即词 quick 在所有文档的 title 字段中出现的频率），以及字段的长度（即字段越短相关度越高）相结合的计算方式。

### match多个词深入

- 查询语句

```sh
GET /test-dsl-match/_search
{
    "query": {
        "match": {
            "title": "BROWN DOG"
        }
    }
}
```

**match多个词的本质**

因为是多个词,match查询必须查找两个词[BROWN,DOG],他会在内部执行两次term查询,然后将两次查询结果合并输出.为了做到这一点,它会将两个term查询包含在一个bool查询中.

等同于如下语句.ps:使用term查询需要转换为小写

```sh
#### 等同于bool查询
GET /test-dsl-match/_search
{
  "query": {
    "bool": {
      "should": [
        {
          "term": {
            "title": {
              "value": "brown"
            }
          }
        },
        {
          "term": {
            "title": {
              "value": "dog"
            }
          }
        }
      ]
    }
  }
}
```

**match多个词的逻辑**

以上match等同于`should`,是因为`match`还有一个`operator`参数,默认是or,所以对应的是`should`

```sh
### operator参数
GET /test-dsl-match/_search
{
  "query": {
    "match": {
      "title": {
        "query": "BROWN DOG",
        "operator": "or"
      }
    }
  }
}
```

如果我们选择`and`,那么等同于bool查询中的`must`

```sh
GET /test-dsl-match/_search
{
  "query": {
    "bool": {
      "must": [
        {
          "term": {
            "title": {
              "value": "brown"
            }
          }
        },
        {
          "term": {
            "title": {
              "value": "dog"
            }
          }
        }
      ]
    }
  }
}
```

### 控制match的匹配精度

如果用户给定3个查询词,想要查找至少包含其中两个的文档,需要通过控制`minimum_should_match` 最小匹配参数,去指定必须匹配的词项数用来表示一个文档是否相关.

该数值可以设位置具体个数,或者将其设置为一个百分数,因为无法控制用户搜索时输入的单词数量.

**示例**

```sh
GET /test-dsl-match/_search
{
  "query": {
    "match": {
      "title": {
        "query": "quick brown dog",
        "minimum_should_match": "75%"
      }
    }
  }
}
```

>  ps:`minimum_should_match `会做合适的事,当包含3词项时,会将75%自动截断成66%

上述语句等同于

```sh
GET /test-dsl-match/_search
{
  "query": {
    "bool": {
      "should": [
        {
          "term": {
            "title": {
              "value": "quick"
            }
          }
        },
        {
          "term": {
            "title": {
              "value": "brown"
            }
          }
        },
        {
          "term": {
            "title": {
              "value": "dog"
            }
          }
        }
      ],
      "minimum_should_match": 2
    }
  }
}
```

### 其它match类型

 **match_pharse**

示例

```sh
### match_pharse
GET /test-dsl-match/_search
{
  "query": {
    "match_phrase": {
      "title": "quick brown"
    }
  }
}
```

因为前文中我们知道了match本质上是对term组合，match_phrase本质是连续的term的查询

**match_pharse_prefix**

`ELasticSearch`在`match_phrase`基础上提供了一种可以查最后一个词项是前缀的方法，这样就可以查询`quick brown f`了

```sh
### **match_pharse_prefix**
GET /test-dsl-match/_search
{
  "query": {
    "match_phrase_prefix": {
      "title": "quick brown f"
    }
  }
}
```

ps: prefix的意思不是整个text的开始匹配，而是最后一个词项满足term的prefix查询而已

 **match_phrase_prefix**

除了`match_phrase_prefix`，ElasticSearch还提供了`match_bool_prefix`查询,但他的本质是无序的.

```sh
### match_bool_prefix
GET /test-dsl-match/_search
{
  "query": {
    "match_bool_prefix": {
      "title": "quick brown f"
    }
  }
}

#### 相同语句

GET /test-dsl-match/_search
{
  "query": {
    "bool": {
      "should": [
        {
          "term": {
            "title": {
              "value": "quick"
            }
          }
        },
        {
          "term": {
            "title": {
              "value": "brown"
            }
          }
        },
        {
          "prefix": {
            "title": {
              "value": "f"
            }
          }
        }
      ]
    }
  }
}
```

- **multi_match**

如果希望对多个字段查询

```sh
### multi_match
GET bank/_search
{
  "query": {
    "multi_match": {
      "query": 18,
      "fields": ["age","*number"]
    }
  }
}
```

`*`表示前缀匹配字段

## query string类型

### query_string

此查询使用语法根据运算符（例如AND或）来解析和拆分提供的查询字符串NOT。然后查询在返回匹配的文档之前独立分析每个拆分的文本。

可以使用该query_string查询创建一个复杂的搜索，其中包括通配符，跨多个字段的搜索等等。尽管用途广泛，但查询是严格的，如果查询字符串包含任何无效语法，则返回错误。

示例

```sh
###query_string
GET test-dsl-match/_search
{
  "query": {
    "query_string": {
      "default_field": "title",
      "query": "(brown dog) OR (brown fox)"
    }
  }
}
```

### query_string_simple

使用简单预发来解析查询字符串,并拆分为基于特殊运算符的术语.然后独立分析每个术语并返回匹配文档.**simple_query_string相对于query_string不会针对无效语法返回错误,而是忽略无效部分** 

示例

```sh
### simple_query_string 
GET test-dsl-match/_search
{
  "query": {
    "simple_query_string": {
      "query": "\"over the\" + {lazy | quick} + dog",
      "fields": []
    }
  }
}
### 返回参数
{
  "took" : 6,
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
    "max_score" : 2.5637567,
    "hits" : [
      {
        "_index" : "test-dsl-match",
        "_type" : "_doc",
        "_id" : "2",
        "_score" : 2.5637567,
        "_source" : {
          "title" : "The quick brown fox jumps over the lazy dog"
        }
      },
      {
        "_index" : "test-dsl-match",
        "_type" : "_doc",
        "_id" : "3",
        "_score" : 0.75073993,
        "_source" : {
          "title" : "The quick brown fox jumps over the quick dog"
        }
      }
    ]
  }
}
```

## Interval类型

intervals query 允许用户精确控制查询词在文档中出现的先后关系，实现了对terms顺序、terms之间的距离以及它们之间的包含关系的灵活控制

Intervals是时间间隔的意思，本质上将多个规则按照顺序匹配。

示例

```sh
##interval
GET test-dsl-match/_search
{
  "query": {
    "intervals": {
      "title": {
        "all_of": {
          "ordered": true,
          "intervals": [
            {
              "match": {
                "query": "quick",
                "max_gaps": 0,
                "ordered": true
              }
            },
            {
              "any_of": {
                "intervals": [
                  {
                    "match": {
                      "query": "jump over"
                    }
                  },
                  {
                    "match": {
                      "query": "quick dog"
                    }
                  }
                ]
              }
            }
          ]
        }
      }
    }
  }
}
```

该语句表示,查询包含字符串quick,且该字符串在jump over或quick dog之前的文本.

**主要参数**

- **match**

| 参数     | 描述                                                         |
| :------- | :----------------------------------------------------------- |
| query    | 用户查询的字符串                                             |
| max_gaps | 字符串中每个词在text field中出现的最大词间距，超过最大间距的将不会被检索到；默认值是-1，即不限制，设置为0的话，query中的字符串必须彼此相连不能拆分 |
| ordered  | query中的字符串是否需要有序显示，默认值是false，即不考虑先后顺序 |
| analyzer | 对query参数中的字符串使用什么分词器，默认使用mapping时该field配置的  search analyzer |
| filter   | 可以为query搭配一个intervals filter，该filter不同于Boolean filter 有自己的语法结构 |

- **all_of**

| 参数      | 描述                                                         |
| :-------- | :----------------------------------------------------------- |
| intervals | 一个interval集合，集合里面的所有match需要同时在一个文档数据上同时满足才行 |
| max_gaps  | 多个interval查询在一个文档中允许的最大间距，超过最大间距的将不会被检索到；默认值是-1，即不限制，设置为0的话，所有的interval query必须彼此相连不能拆分 |
| ordered   | 配置 intervals 出现的先后顺序，默认值false                   |
| filter    | 可以为query搭配一个intervals filter，该filter不同于Boolean filter 有自己的语法结构 |

- **any_of**

| 参数      | 描述                                                         |
| :-------- | :----------------------------------------------------------- |
| intervals | 一个interval集合，集合里面的所有match不需要同时在一个文档数据上同时满足 |
| filter    | 可以为query搭配一个intervals filter                          |





参考:[ES详解 - 查询：DSL查询之全文搜索详解](https://pdai.tech/md/db/nosql-es/elasticsearch-x-dsl-full-text.html)

[elasticsearch 7.0 新特性之Intervals quer](https://www.jianshu.com/p/39146f535bad)