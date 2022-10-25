# ES基本查询语法

```elastic
PUT /customer/_doc/1
{
  "name":"over watch"
}

GET /customer/_doc/1

# 查询
GET /bank/_search
{
  "query":{"match_all": {}},
  "sort":[{"account_num":"asc"}]
}

# 分页查询

GET /bank/_search
{
  "from": 0,
  "size": 1,
  "query": { "match_all": {} },
  "sort": [
    { "account_number": "asc" }
  ]

}

#查询 注意body与url中不要有空行,否则body不生效
GET /bank/_search
{
  "query":{"match": {
     "address": "Columbus Place"
  }}
}

#段落查询 包含指定的值
GET /bank/_search
{
  "query": {"match_phrase": {
    "address": "Columbus Place"
  }}
}

#多条件查询
#搜索40岁客户的帐户，但不包括居住在爱达荷州（ID）的任何人
GET /bank/_search
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

#同时使用query和filter查询
#query对数据进行打分,匹配越高,_score越好;filter只有两种结果匹配/不匹配,后者全部过滤
GET /bank/_search
{
  "query": {
    "bool": {
      "must": [
        {
          "match": {
            "state": "ND"
          }
        }
      ],
      "filter": [
        {
          "term": {
            "age": "40"
          }
        },
        {
          "range": {
            "balance": {
              "gte": 20000,
              "lte": 30000
            }
          }
        }
      ]
    }
  }
}

# 只包含filter的查询语句 查询到的结果所有score值都为0
GET /bank/_search
{
  "query": {
    "bool": {
      "filter": [
        {
          "term": {
            "age": "40"
          }
        },
        {
          "range": {
            "balance": {
              "gte": 20000,
              "lte": 30000
            }
          }
        }
      ]
    }
  }
}

#聚合查询
##简单聚合 
##希望根据州统计梳理 使用aggs关键字对state字段聚合 被聚合的字段无需对分词统计，所以使用state.keyword对整个字段统计
GET /bank/_search
{
  "size": 0, //无需返回具体数据所有size=0
  "aggs": {
    "group_by_state": {
      "terms": {
        "field": "state.keyword"
      }
    }
  }
}

##嵌套聚合
##在上述基础上在进行avg操作
GET /bank/_search
{
  "size": 0,
  "aggs": {
    "group_by_state": {
      "terms": {
        "field": "state.keyword"
      },
      "aggs": {
        "average_balance": {
          "avg": {
            "field": "balance"
          }
        }
      }
    }
  }
}

##对查询结果排序
GET /bank/_search
{
  "size": 0,
  "aggs": {
    "group_by_state": {
      "terms": {
        "field": "state.keyword",
        "order": {
          "average_balance": "desc"
        }
      },
      "aggs": {
        "average_balance": {
          "avg": {
            "field": "balance"
          }
        }
      }
    }
  }
}
```

## 基本结果解释

```json
{
  "took" : 4,  //es运行查询所花费的时间 单位 ms
  "timed_out" : false, //请求是否超时
  "_shards" : { //搜索了多少碎片,以及成功,失败或跳过了多少个碎片的细目分类
    "total" : 1,
    "successful" : 1,
    "skipped" : 0,
    "failed" : 0
  },
  "hits" : {
    "total" : {
      "value" : 1000, //找到多少个匹配的文档
      "relation" : "eq"
    },
    "max_score" : null, //找到最相关文档的分数
    "hits" : [
      {
        "_index" : "bank", 
        "_type" : "_doc",
        "_id" : "0",
        "_score" : null,
        "_source" : {  //文档的相关性得分(使用match_all时不适用)
          "account_number" : 0,
          "balance" : 16623,
          "firstname" : "Bradshaw",
          "lastname" : "Mckenzie",
          "age" : 29,
          "gender" : "F",
          "address" : "244 Columbus Place",
          "employer" : "Euron",
          "email" : "bradshawmckenzie@euron.com",
          "city" : "Hobucken",
          "state" : "CO"
        },
        "sort" : [  //文档的排序位置（不按相关性得分排序时）
          0
        ]
      }
    ]
  }
}
```







参考:[ES详解 - 入门：查询和聚合的基础使用](https://pdai.tech/md/db/nosql-es/elasticsearch-x-usage.html)