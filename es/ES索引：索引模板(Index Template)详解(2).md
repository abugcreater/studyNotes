# ES索引：索引模板(Index Template)详解

## 索引模板

> 索引模版告诉ES在创建索引时如何配置索引

**使用方式**

创建索引前先配置模版,ES在创建索引时,模版设置将作用创建索引的基础

### 索引模版定义

索引模版告诉ES在创建索引时如何配置索引.对于数据流,索引模版在创建流的后备索引时对其进行配置.在创建索引之前先配置模版,然后在手动创建索引或通过对文档建立索引创建索引时,模版设置将用作创建爱你索引的基础. 

### 模板类型

模板有两种类型：**索引模板**和**组件模板**。

1. **组件模板**是可重用的构建块，用于配置映射，设置和别名；它们不会直接应用于一组索引
2. **索引模板**可以包含组件模板的集合，也可以直接指定设置，映射和别名

### 索引模板中的优先级

1. 可组合模版优先于旧模板.如果没有可组合模版匹配给定索引,则旧版模版可能仍匹配并应用
2. 如果使用显示设置创建爱你索引并且该索引也与索引模版匹配,则创建索引请求中的设置优先于索引模版及其组件模版中指定的设置
3. 如果新数据流或索引与多个索引模版匹配,则使用优先级最高的索引模版

### 内置索引模板

Elasticsearch具有内置索引模板，每个索引模板的优先级为100，适用于以下索引模式：

1. `logs-*-*`
2. `metrics-*-*`
3. `synthetics-*-*`

所以在涉及内建索引模板时，要避免索引模式冲突。更多可以参考[这里](https://www.elastic.co/guide/en/elasticsearch/reference/current/index-templates.html)

### 案例

- 首先**创建两个索引组件模板**：

```sh
#创建模版
PUT _component_template/component_template1
{
  "template": {
    "mappings": {
      "properties": {
        "@timestamp": {
          "type": "date"
        }
      }
    }
  }
}

PUT _component_template/runtime_component_template
{
  "template": {
    "mappings": {
      "runtime": { #读时建模
        "day_of_week": { 
          "type": "keyword",
          "script": {
            "source": "emit(doc['@timestamp'].value.dayOfWeekEnum.getDisplayName(TextStyle.FULL, Locale.ROOT))"
          }
        }
      }
    }
  }
}
```

执行结果如下

```json
{
  "acknowledged" : true
}
```

- **创建使用组件模板的索引模板**

```sh
PUT _index_template/template_1
{
  "index_patterns": [
    "bar*"
  ],
  "template": {
    "settings": {
      "number_of_shards": 1
    },
    "mappings": {
      "_source": {
        "enabled": true
      },
      "properties": {
        "host_name": {
          "type": "keyword"
        },
        "created_at": {
          "type": "date",
          "format": "EEE MMM dd HH:mm:ss Z yyyy"
        }
      }
    },
    "aliases": {
      "mydata": {}
    }
  },
  "priority": 500,
  "composed_of": [
    "component_template1",
    "runtime_component_template"
  ],
  "version": 3,
  "_meta": {
    "description": "my custom"
  }
}
```

- 创建一个匹配`bar*`的索引`bar-test`

```
PUT /bar_test
```

- 查看创建索引的映射

```
GET /bar_test/_mapping
```

返回

```json
{
  "bar_test" : {
    "mappings" : {
      "runtime" : {
        "day_of_week" : {
          "type" : "keyword",
          "script" : {
            "source" : "emit(doc['@timestamp'].value.dayOfWeekEnum.getDisplayName(TextStyle.FULL, Locale.ROOT))",
            "lang" : "painless"
          }
        }
      },
      "properties" : {
        "@timestamp" : {
          "type" : "date"
        },
        "created_at" : {
          "type" : "date",
          "format" : "EEE MMM dd HH:mm:ss Z yyyy"
        },
        "host_name" : {
          "type" : "keyword"
        }
      }
    }
  }
}
```

## 模拟多组件模板

ES考虑到多个索引模版的组合结果,提供了API进行模拟组合后的模版的配置

### 模拟某个索引结果

比如上面的template_1, 我们不用创建bar*的索引(这里模拟bar-pdai-test)，也可以模拟计算出索引的配置：

```
#模拟模版结果
POST _index_template/_simulate_index/bar_tbtb_test
```

返回结果

```json
{
  "template" : {
    "settings" : {
      "index" : {
        "number_of_shards" : "1"
      }
    },
    "mappings" : {
      "runtime" : {
        "day_of_week" : {
          "type" : "keyword",
          "script" : {
            "source" : "emit(doc['@timestamp'].value.dayOfWeekEnum.getDisplayName(TextStyle.FULL, Locale.ROOT))",
            "lang" : "painless"
          }
        }
      },
      "properties" : {
        "@timestamp" : {
          "type" : "date"
        },
        "created_at" : {
          "type" : "date",
          "format" : "EEE MMM dd HH:mm:ss Z yyyy"
        },
        "host_name" : {
          "type" : "keyword"
        }
      }
    },
    "aliases" : {
      "mydata" : { }
    }
  },
  "overlapping" : [ ]
}
```

### 模拟组件模板结果

由于template_1模版是由两个组件模版组合的,我们也可以模拟template_1被组合后的索引配置

```
#模拟组件模版结果
POST _index_template/_simulate/template_1
```

返回结果

```json
{
  "template" : {
    "settings" : {
      "index" : {
        "number_of_shards" : "1"
      }
    },
    "mappings" : {
      "runtime" : {
        "day_of_week" : {
          "type" : "keyword",
          "script" : {
            "source" : "emit(doc['@timestamp'].value.dayOfWeekEnum.getDisplayName(TextStyle.FULL, Locale.ROOT))",
            "lang" : "painless"
          }
        }
      },
      "properties" : {
        "@timestamp" : {
          "type" : "date"
        },
        "created_at" : {
          "type" : "date",
          "format" : "EEE MMM dd HH:mm:ss Z yyyy"
        },
        "host_name" : {
          "type" : "keyword"
        }
      }
    },
    "aliases" : {
      "mydata" : { }
    }
  },
  "overlapping" : [ ]
}
```

### 模拟组件模板和自身模板结合后的结果

 **创建两个组合模版**

```sh
PUT _component_template/ct1
{
  "template":{
    "settings":{
      "index.number_of_shards" : 2
    }
  }
}

PUT _component_template/ct2
{
  "template":{
    "settings":{
      "index.number_of_replicas":0
    },
    "mappings":{
      "properties":{
        "@timestamp":{
          "type":"date"
        }
      }
    }
  }
}
```

**模拟在两个组件模版的基础上,添加自身模版的配置**

```sh
POST _index_template/_simulate
{
  "index_patterns": ["my*"],
  "template":{
    "settings" : {
        "index.number_of_shards" : 3
    }
  },
  "composed_of":["ct1","ct2"]
}
```

**返回结果**

```json
{
  "template" : {
    "settings" : {
      "index" : {
        "number_of_shards" : "3",
        "number_of_replicas" : "0"
      }
    },
    "mappings" : {
      "properties" : {
        "@timestamp" : {
          "type" : "date"
        }
      }
    },
    "aliases" : { }
  },
  "overlapping" : [ ]
}
```







参考:[ES详解 - 索引：索引模板(Index Template)详解](https://pdai.tech/md/db/nosql-es/elasticsearch-x-index-template.html)

[ Index templates](https://www.elastic.co/guide/en/elasticsearch/reference/7.12/index-templates.html#index-templates)

[Elasticsearch7索引模板](https://rstyro.github.io/blog/2020/09/23/Elasticsearch7%E7%B4%A2%E5%BC%95%E6%A8%A1%E6%9D%BF/)

[Elasticsearch 运行时类型 Runtime fields 深入详解](https://blog.csdn.net/laoyang360/article/details/120574142)

[Introduction to Runtime Fields in ElasticSearch](https://kartheek91.github.io/2021/02/25/introduction-to-runtime-fields-in-elasticsearch.html)