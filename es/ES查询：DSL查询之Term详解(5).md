# ES查询：DSL查询之Term详解

## Term查询引入

查询分为基于词项的查询和基于文本的查询:

![img](https://pdai.tech/images/db/es/es-dsl-full-text-3.png)

term是基于词项的查询.

主要分为以下几种类型:

exists 查询,fuzzy 查询,ids 查询,prefix 查询,range 查询,regexp 查询,term 查询,terms 查询,terms_set 查询,type 查询,wildcard 查询

## Term查询

准备数据

```sh
#创建索引模版
PUT /test-dsl-term-level
{
  "mappings": {
    "properties": {
      "name": {
        "type": "keyword"
      },
      "programming_languages": {
        "type": "keyword"
      },
      "required_matches": {
        "type": "long"
      }
    }
  }
}
#插入数据
POST /test-dsl-term-level/_bulk
{ "index": { "_id": 1 }}
{"name": "Jane Smith", "programming_languages": [ "c++", "java" ], "required_matches": 2}
{ "index": { "_id": 2 }}
{"name": "Jason Response", "programming_languages": [ "java", "php" ], "required_matches": 2}
{ "index": { "_id": 3 }}
{"name": "Dave Pdai", "programming_languages": [ "java", "c++", "php" ], "required_matches": 3, "remarks": "hello world"}
```

### 字段是否存在:exist

由于多种原因，文档字段的索引值可能不存在:

- 源JSON中的字段是null或[]

- 该字段已"index" : false在映射中设置
- 字段值的长度超出ignore_above了映射中的设置
- 字段值格式错误，并且ignore_malformed已在映射中定义

示例

```sh
GET /test-dsl-term-level/_search
{
  "query": {
    "exists": {
      "field": "remarks"
    }
  }
}
```

### id查询:ids

根据id查找

示例

```sh
GET /test-dsl-term-level/_search
{
  "query": {
    "ids": {
      "values": [1,3]
    }
  }
}
```

### 前缀:prefix

通过前缀查找某个字段

示例

```sh
##prefix
GET /test-dsl-term-level/_search
{
  "query": {
    "prefix": {
      "name": {
        "value": "Jan"
      }
    }
  }
}
```

### 分词匹配:term

最常见的根据分词查询

示例

```sh
GET /test-dsl-term-level/_search
{
  "query": {
    "term": {
      "programming_languages": {
        "value": "php"
      }
    }
  }
}
```

### 多个分词匹配:terms

按照多个分词term匹配，它们是or的关系

示例

```sh
## terms
GET /test-dsl-term-level/_search
{
  "query": {
    "terms": {
      "programming_languages": [
        "php",
        "c++"
      ]
    }
  }
}
```

### 按某个数字字段分词匹配:term set

设计这种方式查询的初衷是用文档中的数字字段动态匹配查询满足term的个数

示例

```sh
GET test-dsl-term-level/_search
{
  "query": {
    "terms_set": {
      "programming_languages": {
        "terms": [
          "php",
          "java"
        ],
        "minimum_should_match_field": "required_matches"
      }
    }
  }
}
```

### 通配符:wildcard

通配符匹配，比如`*`

示例

```sh
## wildcard 通配符
GET test-dsl-term-level/_search
{
  "query": {
    "wildcard": {
      "name": {
        "value": "J*",
        "boost": 1,
        "rewrite": "constant_score"
      }
    }
  }
}
```

### 范围:range

常用于数字或者日期范围的查询

示例

```sh
GET test-dsl-term-level/_search
{
  "query": {
    "range": {
      "required_matches": {
        "gte": 1,
        "lte": 2
      }
    }
  }
}
```

### 正则:regexp

通过[正则表达式](https://pdai.tech/md/develop/regex/dev-regex-all.html)查询

示例:以"Ja"开头的name字段

```sh
##regexp 正则匹配
GET test-dsl-term-level/_search
{
  "query": {
    "regexp": {
      "name": {
        "value": "ja.*",
        "case_insensitive":true
      }
    }
  }
}
```

`case_insensitive`:属性设置忽略大小写

### 模糊匹配:fuzzy

一般情况下有一个单词错误的情况下，`fuzzy` 查询可以找到另一个近似的词来代替，主要有以下场景：

- 修改一个单词，如：`box`--->`fox`。
- 移除一个单词，如：`black`-->`lack`。
- 插入一个单词，如：`sic`-->`sick`。
- 转换两个单词顺序，如：`act`-->`cat`。

为了可以查询到这种近似的单词，`fuzzy` 查询需要创建一个所有近似词的集合，这样搜索的时候就可以采用精确查询找到近似的词来代替查询。

示例

```sh
#fuzzy 模糊匹配
GET test-dsl-term-level/_search
{
  "query": {
    "fuzzy": {
      "remarks": {
        "value": "hello1"
      }
    }
  }
}
```





参考:[ES详解 - 查询：DSL查询之Term详解](https://pdai.tech/md/db/nosql-es/elasticsearch-x-dsl-term.html)

[官方文档](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/term-level-queries.html)

[Elasticsearch中的Term查询和全文查询](https://www.cnblogs.com/lonely-wolf/p/14975414.html)