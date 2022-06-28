# 慢SQL定位及优化

## 1. 慢SQL日志

MySQL 中与慢 SQL 有关的几个重要系统变量如下：

| 参数                | 含义                                                         |
| ------------------- | ------------------------------------------------------------ |
| slow_query_log      | 是否启用慢查询日志，ON 为启用，OFF 为未启用，默认为 OFF。开启会影响性能，MySQL 重启会失效。 |
| slow_query_log_file | 指定慢查询日志文件的路径和名字，缺省文件名 host_name-slow.log。 |
| long_query_time     | 执行时间超过该值才记录到慢查询日志，单位为秒，默认为 10。    |
| log_output          | 日志输出位置，默认为 FILE，即保存为文件，若设置为 TABLE，则将日志记录到 mysql.show_log 表中，支持设置多种格式。 |

可以通过配置文件或者语句实现配置:

**语句**

会在数据库重启后失效

```sql
// session级别设置阈值：
set session long_query_time = 1;
// 全局级别设置阈值
set global long_query_time = 1;
```

**配置文件**

修改配置文件 `vim /etc/my.cnf`，在 [mysqld] 段落在加入如下配置：

```yaml
[mysqld] slow_query_log=1 slow_query_log_file=/var/lib/mysql/data/slow.log long_query_time=3 log_output=FILE,TABLE
```

## 2. SQL执行流程

![sql执行流程](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4633043945ff4fbbb03c5c307a2fc9b5~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp?)

1. 若查询缓存打开则会优先查询缓存，若命中则直接直接返回结果给客户端

2. 若缓存未命中，此时MySQL需要搞清楚这条语句需要做什么，则通过分析器进行词法分析、语法分析

3. 搞清楚要做什么之后，MySQL需要通过优化器进行优化执行计划

4. 最后通过执行器与存储引擎提供的接口进行交互，将结果返回给客户端

**SQL优化器**

SQL优化器成本预估计算效率最高方式,预估维度:I/O成本+CPU成本

- I/O成本:从磁盘读取数据到内存的开销,成本常数为1.0
- CPU成本:从磁盘读取数据后放到内存中处理数据的开销,其成本常数为0.2

计算成本方式:

1. 根据搜索条件，找出所有可能使用的索引，也就是explain的possible_keys
2. 计算全表扫描的开销
3. 计算使用不同索引执行查询的开销
4. 对比各种执行方案的开销，开销最小的那一个

**成本计算公式**

> 全表扫描成本计算：I/O成本+CPU成本
>
> 1. I/O成本 = 页数 * 1.0(成本常数) + 1.1(微调数)
> 2. CPU成本 = 数据行数 * 0.2(成本常数) + 1.0(微调数)
>
> 使用索引查询成本计算：I/O成本+CPU成本+回表I/O成本+回表CPU成本
>
> 1. I/O成本 = 扫描区间 * 1.0(成本常数)
> 2. CPU成本 = 数据行数 * 0.2(成本常数) + 0.01(微调数)
> 3. 回表I/O成本 = 扫描区间 * 1.0(成本常数)
> 4. 回表CPU成本 = 数据行数 * 0.2(成本常数)
>
> 注：mysql规定，当读取索引扫描的时候，每当读取一个扫描区间或者范围区间的IO成本，和读取一个页面的IO成本，是一样的，都是1.0。

CPU成本和I/O成本都会影响SQL的执行效率

1. I/O成本
   - 数据量：数据量越大需要的I/O次数越多。
   - 数据从哪读取：从缓存读取还是从磁盘读取；是否通过索引快速查找；
2. CPU成本
   - 数据处理方式：排序、子查询等，需要先把数据取到临时表中，再对数据进行加工。

## 3. 找到慢SQL

### 3.1 查看日志

MySQL中各种类型的活动都会被记录到日志文件中,常见的如

- 错误日志（error log）：记录MySQL启动、运行、关闭时的问题。

- 二进制日志（binlog）：记录对MySQL数据库执行更改的所有操作。

- 慢查询日志（slow query log）：记录运行时间超过long_query_time阈值的所有SQL语句。

- 查询日志（log）：记录了所有对MySQL数据库请求的信息。

```shell
cat /var/lib/mysql/data/slow.log
```

![慢SQL日志](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e669fc56e40643239df0e619afecea17~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

### 3.2 mysqldumpslow

MySQL 内置了 mysqldumpslow 这个工具来帮我们分析慢查询日志

![mysqldumpslow 工具](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2d57739b907a430e9f0c0de3829a8293~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

```shell
#得到返回记录集最多的10个SQL
mysqldumpslow -s r -t 10 /var/lib/mysql/data/slow.log
#得到访问次数最多的10个SQL
mysqldumpslow -s c -t 10 /var/lib/mysql/data/slow.log
#得到按照时间排序的前10条里面含有左连接的查询语句
mysqldumpslow -s t -t 10 -g "LEFT JOIN" /var/lib/mysql/data/slow.log
#结合| more使用，防止爆屏情况
mysqldumpslow -s r -t 10 /var/lib/mysql/data/slow.log | more

s：表示按何种方式排序
c：访问次数
l：锁定时间
r：返回记录
t：查询时间
al：平均锁定时间
ar：平均返回记录数
at：平均查询时间
t：返回前面多少条的数据
g：后边搭配一个正则匹配模式，大小写不敏感

```

### 3.3 pt-query-digest

pt-query-digest 是一款很强大的慢查询日志分析工具，可以分析 MySQL 数据库的 binary log 、 general log 日志，同时也可以使用 show processlist 或从 tcpdump 抓取的 MySQL 协议数据来进行分析。

## 4. 优化慢SQL

![慢sql分析方法](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ffc9863fbeb3443b9e8893571af05a7d~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp?)



### 4.1 explain

使用方法 `explain select * from a where 1 = 1`

返回结果

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/cbb6c206b1c740658e74bb602798d099~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp?)

| 列名          | 含义                                                         |
| ------------- | ------------------------------------------------------------ |
| select_type   | select子句类型                                               |
| partitions    | 匹配的分区                                                   |
| type          | 访问类型，即怎么找数据行的方式(ALL, index, range, ref, eq_ref, const, system, NULL) |
| possible_keys | 能使用的索引                                                 |
| key           | 预测使用的索引                                               |
| key_len       | 索引使用的字节数                                             |
| ref           | 连接匹配条件                                                 |
| rows          | 估算出所查到的数据行数                                       |
| filtered      | 通过条件过滤出的行数所占百分比估计值，1~100，100表示没有做任何过滤 |
| Extra         | 该列包含MySQL解决查询的详细信息                              |

索引使用问题，通过 `possible_keys(能用到的索引)` 和 `key(实际用到的索引)` 两个字段查看：

- 没有使用索引
- 优化器选择了错误索引
- 没有实现覆盖索引

I/O开销问题，通过`rows(执行当前查询要遍历的行数)`和`filtered(有效行数/扫描行数比值)`字段来查看：

- 扫描的行数过多
- 返回无用列且无用列有明显I/O性能开销(比如text、blob、json 等类型）

### 4.2 Show Profile

explain只能分析到SQL的预估执行计划，无法分析到SQL实际执行过程中的耗时，可以通过配置profiling参数来进行SQL执行分析。开启参数后，后续执行的SQL语句都会记录其资源开销，包括IO，上下文切换，CPU，Memory等等，我们可以根据这些开销进一步分析当前慢SQL的瓶颈再进一步进行优化

默认关闭。开启后，会在后台保存最近 15 次的运行结果，然后通过 Show Profile 命令查看结果。

**查看耗时**

1. 查看系统变量，判断该功能是否开启 :`SHOW VARIABLES LIKE 'profiling%';`

2. 设置系统变量，开启开关:`set profiling=1;`

3. 执行SQL语句。

4. 查看profiles:`show profiles;`

   ![SHOW profiles 查看 SQL 的耗时](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3a7b7767684b4f9595a9535d35d86e91~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

5. 查看指定id的SQL语句开销详细信息:`show profile for query id;`,会展示sql每个阶段执行的耗时.

   ![SQL 整个生命周期的耗时](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c9f977447bb34560aecb69e26f8c38aa~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp)

查询时可选参数:type

```
ALL # 显示所有的开销信息
BLOCK IO # 显示块IO相关开销
CONTEXT SWITCHES # 上下文切换相关开销
CPU # 显示CPU相关开销信息
IPC # 显示发送和接收相关开销信息
MEMORY # 显示内存相关开销信息
PAGE FAULTS # 显示页面错误相关开销信息
SOURCE # 显示和 Source_function，Source_file，Source_line 相关的开销信息
SWAPS # 显示交换次数相关开销的信息
```

我们可以通过查看SQL语句执行时每一个阶段的耗时，也可以通过`show profile all for query id;`命令查看完整列的执行情况。

完整列返回:

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1b31fea67c7746c09c0dd186552e541c~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp?)

| 列名                  | 含义           |
| --------------------- | -------------- |
| "Status"              | 执行阶段       |
| "Duration"            | 持续时间       |
| "CPU_user"            | cpu用户        |
| "CPU_system"          | cpu系统        |
| "Context_voluntary"   | 上下文主动切换 |
| "Context_involuntary" | 上下文被动切换 |
| "Block_ops_in"        | 阻塞的输入操作 |
| "Block_ops_out"       | 阻塞的输出操作 |
| "Messages_sent"       | 消息发出       |
| "Messages_received"   | 消息接收       |
| "Page_faults_major"   | 主分页错误     |
| "Page_faults_minor"   | 次分页错误     |
| "Swaps"               | 交换次数       |
| "Source_function"     | 源功能         |
| "Source_file"         | 源文件         |
| "Source_line"         | 源代码行       |

**危险状态**

converting HEAP to MyISAM # 查询结果太大，内存不够用了，在往磁盘上搬。

Creating tmp table # 创建了临时表，回先把数据拷贝到临时表，用完后再删除临时表。

Copying to tmp table on disk # 把内存中临时表复制到磁盘

locked # 记录被锁了


### 4.3 Optimizer Trace

profile只能查看到SQL的执行耗时，但是无法看到SQL真正执行的过程信息。Optimizer Trace是一个跟踪功能，它可以跟踪执行语句的解析优化执行的全过程，可以开启该功能进行执行语句的分析。 诚如上述所说，explain只能判断出SQL预估的执行计划，预估时根据成本模型进行成本计算进而比较出理论最优执行计划，但在实际过程中，预估不代表完全正确，因此我们需要通过追踪来看到它在执行过程中的每一环是否真正准确。

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0744f346a1c74d13ab6dab94d4dc7d25~tplv-k3u1fbpfcp-zoom-in-crop-mark:3024:0:0:0.awebp?)

可查看分析其执行树：

- join_preparation：准备阶段,包括查询语句转换，转换成嵌套循环语句等

- join_optimization：分析阶段包括以下主要阶段

   condition_processing ：处理where条件部分，主要包括等式处理、常量处理、多余条件处理

   table_dependencies ：表依赖检查

   ref_optimizer_key_uses ：评估可用的索引

   rows_estimation    ：评估访问单表的方式，及扫描的行数与代价

   considered_execution_plans ：评估最终可使用的执行计划

   condition_on_constant_tables :检查带常量表的条件

   attaching_conditions_to_tables ：将常量条件作用到表

   refine_plan 改进计划，不理解

- join_execution：执行阶段

```json
root@(none) 09:39:58> select * from information_schema.optimizer_trace\G;
*************************** 1. row ***************************
                            QUERY: SELECT * FROM SSS.DEPARTMENT d LEFT JOIN ppp.shop s ON d.DEPTID = s.DEPTID WHERE d.DEPTID = '00001111'
                            TRACE: {
  "steps": [
   
  #准备阶段
    {
      "join_preparation": {
        "select#": 1,
        "steps": [
          {  
          #expanded_query，解析查询语句，"*" 转换成字段，left join on 处转化成on((`SSS`.`d`.`Deptid` = convert(`ppp`.`s`.`Deptid` using utf8mb4))))
            "expanded_query": "/* select#1 */ select `SSS`.`d`.`Organid` AS `Organid`,。。。`s`.`Status` AS `Status`,`ppp`.`s`.`Stylecategoryid` AS `Stylecategoryid`,`ppp`.`s`.`Turnontime` AS `Turnontime` from (`SSS`.`department` `d` left join `ppp`.`shop` `s` on((`SSS`.`d`.`Deptid` = convert(`ppp`.`s`.`Deptid` using utf8mb4)))) where (`SSS`.`d`.`Deptid` = '00001111')"
          },
          {
          #转化成的nested join语句：
            "transformations_to_nested_joins": {
              "transformations": [
                "parenthesis_removal"
              ] /* transformations */,
              "expanded_query": "/* select#1 */ select `SSS`.`d`.`Organid`。。。 `SSS`.`d`.`Guidecode` AS `Guidecode`,`SSS`.`d`.`Createdate` AS `Createdate`,`SSS`.`d`.`Plateformuser` AS `Plateformuser`,`SSS`.`d`.`Plateformdept` AS `Plateformdept`,`SSS`.`d`.`Agentuser` AS `Agentuser`,`SSS`.`d`.`Agentdept` AS `Agentdept`,`SSS`.`d`.`Shopstatus` AS `Shopstatus`,`SSS`.`d`.`Deptshortname` AS `Deptshortname`,`SSS`.`d`.`Storetype` AS `Storetype`,`SSS`.`d`.`Depttype` AS `Depttype`,`ppp`.`s`.`Shopid` AS `Shopid`,`ppp`.`s`.`Objectid` AS `Objectid`,`ppp`.`s`.`Shopname` AS `Shopname`,`ppp`Tel`,`ppp`.`s`.`Introduce` AS `Introduce`,`ppp`.`s`.`Industry` AS `Industry`,`ppp`.`s`.`Address` AS `Address`,`ppp`.`s`.`Shop360image` AS `Shop360image`,`ppp`.`s`.`Domain` AS `Domain`,`ppp`.`s`.`Organid` AS `Organid`,`ppp`.`s`.`Deptid` AS `Deptid`,`ppp`.`s`.`Brandids` AS `Brandids`,`ppp`.`s`.`Extdata` AS `Extdata`,`ppp`.`s`.`Ranking` AS `Ranking`,`ppp`.`s`.`Isdelete` AS `Isdelete`,`ppp`.`s`.`District` AS `District`,`ppp`.`s`.`City` AS `City`,`ppp`.`s`.`Province` AS `Province`,`ppp`.`s`.`Phone` AS `Phone`,`ppp`.`s`.`Watermarkimage` AS `Watermarkimage`,`ppp`.`s`.`Drawingimage` AS `Drawingimage`,`ppp`.`s`.`Contactuser` AS `Contactuser`,`ppp`.`s`.`Panoloadingimage` AS `Panoloadingimage`,`ppp`.`s`.`Lngandlat` AS `Lngandlat`,`ppp`.`s`.`Createtime` AS `Createtime`,`ppp`.`s`.`Shoptype` AS `Shoptype`,`ppp`.`s`.`Status` AS `Status`,`ppp`.`s`.`Stylecategoryid` AS `Stylecategoryid`,`ppp`.`s`.`Turnontime` AS `Turnontime` from `SSS`.`department` `d` left join `ppp`.`shop` `s` on((`SSS`.`d`.`Deptid` = convert(`ppp`.`s`.`Deptid` using utf8mb4))) where (`SSS`.`d`.`Deptid` = '00001111')"
            } /* transformations_to_nested_joins */
          }
        ] /* steps */
      } /* join_preparation */
    },#准备阶段结束
     
     
    {
    #优化阶段：
      "join_optimization": {
        "select#": 1,
        "steps": [
          { 
          #处理where条件部分，化简条件：
            "condition_processing": {
              "condition": "WHERE",
              "original_condition": "(`SSS`.`d`.`Deptid` = '00001111')",---原始条件
              "steps": [
                {
                  "transformation": "equality_propagation", ----等式处理
                  "resulting_condition": "(`SSS`.`d`.`Deptid` = '00001111')"
                },
                {
                  "transformation": "constant_propagation",-----常量处理
                  "resulting_condition": "(`SSS`.`d`.`Deptid` = '00001111')"
                },
                {
                  "transformation": "trivial_condition_removal",----去除多余无关的条件处理
                  "resulting_condition": "(`SSS`.`d`.`Deptid` = '00001111')"
                }
              ] /* steps */
            } /* condition_processing */
          },#结束，因为这里已经够简化了，所以三次处理后都是同样的。
           
          {
          #替代产生的字段
            "substitute_generated_columns": {
            } /* substitute_generated_columns */
          },
           
          {
          #表依赖关系检查
            "table_dependencies": [
              {
                "table": "`SSS`.`department` `d`", ------表d
                "row_may_be_null": false,
                "map_bit": 0,
                "depends_on_map_bits": [
                ] /* depends_on_map_bits */
              },
              {
                "table": "`ppp`.`shop` `s`", --------表s
                "row_may_be_null": true,
                "map_bit": 1,
                "depends_on_map_bits": [
                  0
                ] /* depends_on_map_bits */
              }
            ] /* table_dependencies */
          }, #表依赖关系检查结束
           
           
          {#找出可使用索引的字段：
            "ref_optimizer_key_uses": [
              {
                "table": "`SSS`.`department` `d`",
                "field": "Deptid", ----------可用的是Deptid
                "equals": "'00001111'",
                "null_rejecting": false ---
              },
              {
                "table": "`SSS`.`department` `d`",
                "field": "Deptid",
                "equals": "'00001111'",
                "null_rejecting": false
              }
            ] /* ref_optimizer_key_uses */
          },
           
          {#评估每个表单表访问行数及相应代价。
            "rows_estimation": [
              {
                "table": "`SSS`.`department` `d`",
                "rows": 1,   ---返回1行
                "cost": 1,   ---代价为1
                "table_type": "const",  ---d表使用的方式是const，即根据主键索引获取
                "empty": false
              },
              {
                "table": "`ppp`.`shop` `s`",
                "table_scan": { -------s表直接使用全表扫描
                  "rows": 978662, ------扫描978662行
                  "cost": 8109    ------代价为8109
                } /* table_scan */
              }
            ] /* rows_estimation */
          },
           
           
          {#评估执行计划，这里考虑两表连接
            "considered_execution_plans": [
              {
                "plan_prefix": [------------------执行计划的前缀，这里是d表，因为是left join 我认为指的应该是驱动表的意思
                  "`SSS`.`department` `d`"
                ] /* plan_prefix */,
                "table": "`ppp`.`shop` `s`",
                "best_access_path": {-------最优访问路径
                  "considered_access_paths": [考虑的访问路径
                    {
                      "rows_to_scan": 978662,---扫描978662行
                      "access_type": "scan",--------全表扫描的方式
                      "resulting_rows": 978662,
                      "cost": 203841,----------使用代价
                      "chosen": true-------选中
                    }
                  ] /* considered_access_paths */
                } /* best_access_path */,
                "condition_filtering_pct": 100,条件过滤率100%，指的是这里与上一个表进行行过滤的行数
                "rows_for_plan": 978662,------执行计划的扫描行数978662
                "cost_for_plan": 203841,-------执行计划的cost203841
                "chosen": true---------选中
              }
            ] /* considered_execution_plans */
          },
           
           
          {#检查带常量表的条件
            "condition_on_constant_tables": "('00001111' = '00001111')",
            "condition_value": true
          },
           
           
          {   #将常量条件作用到表，这里主要是将d表的中的deptid条件作用到s表的deptid
            "attaching_conditions_to_tables": {
              "original_condition": "('00001111' = '00001111')",
              "attached_conditions_computation": [
              ] /* attached_conditions_computation */,
              "attached_conditions_summary": [
                {
                  "table": "`ppp`.`shop` `s`",
                  "attached": "<if>(is_not_null_compl(s), ('00001111' = convert(`ppp`.`s`.`Deptid` using utf8mb4)), true)"
                }
              ] /* attached_conditions_summary */
            } /* attaching_conditions_to_tables */
          },
           
           
          {
            "refine_plan": [
              {
                "table": "`ppp`.`shop` `s`"
              }
            ] /* refine_plan */
          }
        ] /* steps */
      } /* join_optimization */
    },
     
    {
      "join_execution": {
        "select#": 1,
        "steps": [
        ] /* steps */
      } /* join_execution */
    }
     
  ] /* steps */
}
MISSING_BYTES_BEYOND_MAX_MEM_SIZE: 0
          INSUFFICIENT_PRIVILEGES: 0
1 row in set (0.00 sec)
```





参考:[你平时怎么排查并调优慢 SQL 的](https://juejin.cn/post/7085344995120513055)

[慢SQL优化一点小思路](https://juejin.cn/post/7048974570228809741)

[Chapter 8 Tracing the Optimizer](https://dev.mysql.com/doc/internals/en/optimizer-tracing.html)

慢sql

没索引

索引失效

等flush

等锁

