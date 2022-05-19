# mysql 执行计划

## 简介 

在正常执行的select语句前加上 explain关键字可以看到mysql的执行计划.

执行计划是数据库根据 SQL 语句和相关表的统计信息作出的一个查询方案，这个方案是由查询优化器自动分析产生的。通过查看执行计划,我们可以知道mysql是如何处理sql语句的.

## 执行计划

执行计划的返回字段如下:

![img](https://img-blog.csdnimg.cn/2018112215033993.png)

### 返回字段信息

#### id

id表示Select 语句的执行顺序,一般有几个select就有几个id.

-  id值不同时,id的大的会被优先执行. 在该语句中子查询的id=2,子查询的会被优先执行
  - ````sql
    EXPLAIN SELECT * FROM task WHERE user_id = 1199946  and finish_time > 0 and grade_id = (SELECT grade_id FROM user_agency WHERE user_id = 1199946);
    ````
  
- id值相同时,按照顺序从上到下执行

-     id值为空,表示这是一个结果集,不需要用他进行查询,存在场景 `union`关键字结合结果

#### select_type

select_type表示,查询的类型，主要用于区分普通查询、联合查询、子查询等复杂的查询

- simple:简单查询,不包括union和子查询,此时最外层的查询为simple,且只有一个

- primary:当查询语句中包含union或子查询,此时最外层的查询为primary,且只有一个

- derived：from 列表中出现的子查询，也叫做衍生表；mysql 或者递归执行这些子查询，把结果放在临时表里。

- subquery：除了 from 子句中包含的子查询外，其他地方出现的子查询都可能是 subquery

- union：若第二个 select 出现在 union 之后，则被标记为 union；若 union 包含在 from 子句的子查询中，外层 select 将被标记为 derived。

  ```sql
  explain select * from t3 where id=3952602 union all select * from t3
  ```
  
- union result：从 union 表获取结果的 select ，因为它不需要参与查询，所以 id 字段为 null

- dependent union：与 union 一样，出现在 union 或 union all 语句中，但是这个查询要受到外部查询的影响；

- dependent subquery：与 dependent union 类似，子查询中的第一个 SELECT，这个 subquery 的查询要受到外部表查询的影响。

#### table

table表示explain的这一行正在访问哪个表.如果使用了别名则显示别名;如果没有涉及数据表则显示null,如果显示尖括号则表示是临时表;如果是尖括号括起来的<union M,N>,表示结果来自于union查询的id为M,N的结果集

#### type

type表示访问类型,即 MySQL 决定如何查找表中的行.依次从好到差：system > const > eq_ref > ref > fulltext > ref_or_null > index_merge > unique_subquery > index_subquery > range > index > ALL.除all之外其他都表示使用了索引.

> - system：表中只有一行数据（等于系统表），这是 const 类型的特例，平时不会出现，可以忽略不计。

> - const：使用唯一索引或者主键，表示通过索引一次就找到了，const 用于比较 primary key 或者 unique 索引。因为只需匹配一行数据，所有很快。如果将主键置于 where 列表中，mysql 就能将该查询转换为一个 const。

> - eq_ref：唯一性索引扫描，对于每个索引键，表中只有一行数据与之匹配。常见于主键或唯一索引扫描。

> - ref：非唯一性索引扫描，返回匹配某个单独值的所有行。本质也是一种索引。

> - fulltext：全文索引检索，全文索引的优先级很高，若全文索引和普通索引同时存在时，mysql 不管代价，优先选择使用全文索引。

> - ref_or_null：与 ref 方法类似，只是增加了 null 值的比较。

> - index_merge：表示查询使用了两个以上的索引，索引合并的优化方法，最后取交集或者并集，常见 and ，or 的条件使用了不同的索引。

> - unique_subquery：用于 where 中的 in 形式子查询，子查询返回不重复值唯一值；

> - index_subquery：用于 in 形式子查询使用到了辅助索引或者 in 常数列表，子查询可能返回重复值，可以使用索引将子查询去重。

> - range：索引范围扫描，常见于使用`>`,`<`,`between` ,`in` ,`like`等运算符的查询中。

> - index：索引全表扫描，把索引树从头到尾扫一遍；

> - all：遍历全表以找到匹配的行（Index 与 ALL 虽然都是读全表，但 index 是从索引中读取，而 ALL 是从硬盘读取）

> - NULL: MySQL 在优化过程中分解语句，执行时甚至不用访问表或索引。

#### possible_keys

- 显示查询可能使用到的索引。

#### key

- 显示查询实际使用哪个索引来优化对该表的访问；
- select_type 为 index_merge 时，这里可能出现两个以上的索引，其他的 select_type 这里只会出现一个。

#### key_len

- 用于处理查询的索引长度，表示索引中使用的**最大**字节数(而非实际)。通过这个值，可以得出一个多列索引里实际使用了哪一部分。只在where条件后出现,排序和分组不出现

#### ref

显示哪个字段或者常数与 key 一起被使用。

1. 如果是使用的常数等值查询，这里会显示 const。
2. 如果是连接查询，被驱动表的执行计划这里会显示驱动表的关联字段。
3. 如果是条件使用了表达式或者函数，或者条件列发生了内部隐式转换，这里可能显示为 func。

#### rows

- 表示 MySQL 根据表统计信息及索引选用情况，大致估算的找到所需的目标记录所需要读取的行数，不是精确值。

#### Extra

- 不适合在其他列中显示但十分重要的额外信息。
- 这个列可以显示的信息非常多，有几十种，常用的有：

### filtered

- 这个字段表示存储引擎返回的数据在 server 层过滤后，剩下多少满足查询的记录数量的比例，注意是百分比，不是具体记录数

## 执行计划局限性

1. EXPLAIN 不会告诉你关于触发器、存储过程的信息或用户自定义函数对查询的影响情况；

2. EXPLAIN 不考虑各种 Cache；

3. EXPLAIN 不能显示 MySQL 在执行查询时所作的优化工作；

4. 部分统计信息是估算的，并非精确值；

5. EXPALIN 只能解释 SELECT 操作，其他操作要重写为 SELECT 后查看。



参考:[Mysql探索之Explain执行计划详解](https://juejin.cn/post/6865571676902391821#comment)