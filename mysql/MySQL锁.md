# MySQL锁

## 锁分类

MySQL中不同的**存储引擎**支持不同的锁机制.


> MyISAM和MEMORY存储引擎采用的是表级锁（table-level locking）；

> BDB存储引擎采用的是页面锁（page-level locking），但也支持表级锁；

> InnoDB存储引擎既支持行级锁（row-level locking），也支持表级锁，但默认情况下是采用行级锁。

按照不同的的加锁粒度可以归纳如下:


>开销、加锁速度、死锁、粒度、并发性能
>表级锁：开销小，加锁快；不会出现死锁；锁定粒度大，发生锁冲突的概率最高,并发度最低。
>
>​	- 存储引擎通过一次性获取所有需要的锁以及按相同顺序获取表锁来避免死锁
>
>​	- 表锁适合已查询为主,并发用户少,只有少量按索引条件更新数据的应用

>行级锁：开销大，加锁慢；会出现死锁；锁定粒度最小，发生锁冲突的概率最低,并发度也最高。
>
>​	最大程度支持并发,也带来最大的锁开销
>
>​	在InnoDB中,除单个SQL组成的事务外,锁是逐步获得的,所以发生死锁是可能的
>
>​	只在存储引擎层实现,适合于大量按索引条件并发更新少量不同数据,同事又有并发查询的应用如(OLTP)系统

>页面锁：开销和加锁时间界于表锁和行锁之间；会出现死锁；锁定粒度界于表锁和行锁之间，并发度一般。

## MyISAM表锁

>  MyISAM存储引擎只支持表锁.

### 查询表级锁争用情况

```
可以通过检查table_locks_waited和table_locks_immediate状态变量来分析系统上的表锁定争夺：
mysql> show status like 'table%';
+-----------------------+-------+
| Variable_name         | Value |
+-----------------------+-------+
| Table_locks_immediate | 2979  |
| Table_locks_waited    | 0     |
+-----------------------+-------+
2 rows in set (0.00 sec))
如果Table_locks_waited的值比较高，则说明存在着较严重的表级锁争用情况。
```

### MySQL表级锁的锁模式

表锁有两种模式:表共享读锁（Table Read Lock）和表独占写锁（Table Write Lock）。锁模式的兼容性如表所示。

![img](https://xuzhongcn.github.io/mysql/03/mdpic/1.png)

> 读请求相互共享,而写请求会阻塞表的读写请求.

默认情况下，**写锁比读锁具有更高的优先级**：当一个锁释放时，这个锁会优先给写锁队列中等候的获取锁请求，然后再给读锁队列中等候的获取锁请求。

可以设置改变读锁和写锁的优先级：

- 通过指定启动参数low-priority-updates，使MyISAM引擎默认给予读请求以优先的权利。
- 通过执行命令SET LOW_PRIORITY_UPDATES=1，使该连接发出的更新请求优先级降低。
- 通过指定INSERT、UPDATE、DELETE语句的LOW_PRIORITY属性，降低该语句的优先级。
- 给系统参数max_write_lock_count设置一个合适的值，当一个表的读锁达到这个值后，MySQL就暂时将写请求的优先级降低，给读进程一定获得锁的机会。

下图展示表锁冲突时的场景:

![img](https://xuzhongcn.github.io/mysql/03/mdpic/2.png)

### 如何加表锁

> MyISAM在执行查询语句（SELECT）前，会自动给涉及的所有表加读锁，在执行更新操作（UPDATE、DELETE、INSERT等）前，会自动给涉及的表加写锁.

MyISAM存储引擎支持**并发插入**，以减少给定表的读和写操作之间的争用：如果MyISAM表在数据文件中间没有空闲块，则行始终插入数据文件的末尾.此时INSERT和SELECT语句可以并发使用.而文件**中间有空闲块会时会禁止该操作**.所有空闲块都填充有新数据时，它又会自动重新启用.

如果你使用LOCK TABLES显式获取表锁，则可以请求READ LOCAL锁而不是READ锁，以便在锁定表时，其他会话可以使用并发插入

```sql
Select sum(total) from orders;
Select sum(subtotal) from order_detail;
--这时，如果不先给两个表加锁，就可能产生错误的结果，因为第一条语句执行过程中，order_detail表可能已经发生了改变。因此，正确的方法应该是：
Lock tables orders read local, order_detail read local;
Select sum(total) from orders;
Select sum(subtotal) from order_detail;
Unlock tables;
```

MySQL不支持锁升级。也就是说，在执行LOCK TABLES后，只能访问显式加锁的这些表，不能访问未加锁的表.如果加的是读锁那么只能执行查询.

> 当使用LOCK TABLES时，不仅需要一次锁定用到的所有表，而且，同一个表在SQL语句中出现多少次，就要通过与SQL语句中相同的别名锁定多少次，否则也会出错！举例说明如下。

```sql
（1）对actor表获得读锁：
mysql> lock table actor read;
Query OK, 0 rows affected (0.00 sec)
（2）但是通过别名访问会提示错误：
mysql> select a.first_name,a.last_name,b.first_name,b.last_name from actor a,actor b where a.first_name = b.first_name and a.first_name = 'Lisa' and a.last_name = 'Tom' and a.last_name <> b.last_name;
ERROR 1100 (HY000): Table 'a' was not locked with LOCK TABLES
（3）需要对别名分别锁定：
mysql> lock table actor as a read,actor as b read;
Query OK, 0 rows affected (0.00 sec)
（4）按照别名的查询可以正确执行：
mysql> select a.first_name,a.last_name,b.first_name,b.last_name from actor a,actor b where a.first_name = b.first_name and a.first_name = 'Lisa' and a.last_name = 'Tom' and a.last_name <> b.last_name;
+------------+-----------+------------+-----------+
| first_name | last_name | first_name | last_name |
+------------+-----------+------------+-----------+
| Lisa       | Tom       | LISA       | MONROE    |
+------------+-----------+------------+-----------+
1 row in set (0.00 sec)
```

## InnoDB锁问题

> InnoDB与MyISAM的最大不同有两点：一是支持事务（TRANSACTION）；二是采用了行级锁。行级锁与表级锁本来就有许多不同之处，另外，事务的引入也带来了一些新问题。

### 1．事务（Transaction）及其ACID属性

```
事务是由一组SQL语句组成的逻辑处理单元，事务具有以下4个属性，通常简称为事务的ACID属性。
原子性（Atomicity）：事务是一个原子操作单元，其对数据的修改，要么全都执行，要么全都不执行。
一致性（Consistent）：在事务开始和完成时，数据都必须保持一致状态。这意味着所有相关的数据规则都必须应用于事务的修改，以保持数据的完整性；事务结束时，所有的内部数据结构（如B树索引或双向链表）也都必须是正确的。
隔离性（Isolation）：数据库系统提供一定的隔离机制，保证事务在不受外部并发操作影响的“独立”环境执行。这意味着事务处理过程中的中间状态对外部是不可见的，反之亦然。
持久性（Durable）：事务完成之后，它对于数据的修改是永久性的，即使出现系统故障也能够保持。
银行转帐就是事务的一个典型例子。
```

### 2．并发事务处理带来的问题

1. 更新丢失（Lost Update）：事务并发发生时,由于不知道其他事务的存在导致最后的更新覆盖了由其他事务所做的更新

2. .脏读（Dirty Reads）：事务读取了其他事务正在更新的未提交数据

3. 不可重复读（Non-Repeatable Reads）: 事务读取过程中前后读取的同一行数据不一致
4. 幻读（Phantom Reads）：一个事务按相同的查询条件重新读取以前检索过的数据，却发现其他事务插入了满足其查询条件的新数据

### 3．事务隔离级别

数据库实现事务隔离的方式，基本上可分为以下两种。

1.一种是在读取数据前，对其加锁，阻止其他事务对数据进行修改。

2.另一种是不用加任何锁，通过一定机制生成一个数据请求时间点的一致性数据快照（Snapshot)，并用这个快照来提供一定级别（语句级或事务级）的一致性读取。

数据库的事务隔离越严格，并发副作用越小，但付出的代价也就越大，因为事务隔离实质上就是使事务在一定程度上 “串行化”进行，这显然与“并发”是矛盾的。

![img](https://xuzhongcn.github.io/mysql/03/mdpic/5.png)

## 获取InnoDB行锁争用情况

可以通过检查InnoDB_row_lock状态变量来分析系统上的行锁的争夺情况：

```
mysql> show status like 'innodb_row_lock%';
+-------------------------------+-------+
| Variable_name                 | Value |
+-------------------------------+-------+
| InnoDB_row_lock_current_waits | 0     |
| InnoDB_row_lock_time          | 0     |
| InnoDB_row_lock_time_avg      | 0     |
| InnoDB_row_lock_time_max      | 0     |
| InnoDB_row_lock_waits         | 0     |
+-------------------------------+-------+
5 rows in set (0.01 sec)
```

如InnoDB_row_lock_waits和InnoDB_row_lock_time_avg的值比较高，表示锁争用比较严重,可以通过设置InnoDB Monitors来进一步观察发生锁冲突的表、数据行等，并分析锁争用的原因.

```sql
mysql> CREATE TABLE innodb_monitor(a INT) ENGINE=INNODB; --创建监视器
Query OK, 0 rows affected (0.14 sec)
然后就可以用下面的语句来进行查看：
mysql> Show engine innodb status\G;
*************************** 1. row ***************************
  Type: InnoDB
  Name:
Status:
…
…
------------
TRANSACTIONS
------------
Trx id counter 0 117472192
Purge done for trx's n:o < 0 117472190 undo n:o < 0 0
History list length 17
Total number of lock structs in row lock hash table 0
LIST OF TRANSACTIONS FOR EACH SESSION:
---TRANSACTION 0 117472185, not started, process no 11052, OS thread id 1158191456
MySQL thread id 200610, query id 291197 localhost root
---TRANSACTION 0 117472183, not started, process no 11052, OS thread id 1158723936
MySQL thread id 199285, query id 291199 localhost root
Show innodb status
…
```

监视器可以通过发出下列语句来停止查看：

```sql
mysql> DROP TABLE innodb_monitor;
Query OK, 0 rows affected (0.05 sec)
```

> 设置监视器后，在SHOW INNODB STATUS的显示内容中，会有详细的当前锁等待的信息，包括表名、锁类型、锁定记录的情况等，便于进行进一步的分析和问题的确定。

> 打开监视器以后，默认情况下每15秒会向日志中记录监控的内容，如果长时间打开会导致.err文件变得非常的巨大，所以用户在确认问题原因之后，要记得删除监控表以关闭监视器，或者通过使用“--console”选项来启动服务器以关闭写日志文件。

## InnoDB的行锁模式及加锁方法

### InnoDB行锁类型


- 共享锁（S）：允许一个事务去读一行，阻止其他事务获得相同数据集的排他锁。
- 排他锁（X)：允许获得排他锁的事务更新数据，阻止其他事务取得相同数据集的共享读锁和排他写锁。

为了实现多粒度,内部还使用了意向锁(**都是表锁**)

![img](https://xuzhongcn.github.io/mysql/03/mdpic/6.png)



意向锁是InnoDB**自动**加的，不需用户干预。对于UPDATE、DELETE和INSERT语句，InnoDB会自动给涉及数据集加排他锁（X)；对于普通SELECT语句，InnoDB不会加任何锁；事务可以通过以下语句显示给记录集加共享锁或排他锁。

```sql
共享锁（S）：SELECT * FROM table_name WHERE ... LOCK IN SHARE MODE。
排他锁（X)：SELECT * FROM table_name WHERE ... FOR UPDATE
```

用`SELECT ... IN SHARE MODE`获得共享锁,主要用于确认数据是否存在,并确保没人去更新该数据,如果当前事务对该数据更新可能会造成死锁.案例如下:

![img](https://xuzhongcn.github.io/mysql/03/mdpic/7.png)

排他锁例子如下:

![img](https://xuzhongcn.github.io/mysql/03/mdpic/8.png)



## InnoDB行锁实现方式

> InnoDB行锁是通过给索引上的索引项加锁来实现的,如果不是通过索引条件查询数据,会导致InnoDB使用表锁

**1. 在不通过索引条件查询的时候，InnoDB确实使用的是表锁，而不是行锁。**

演示案例:

```sql
mysql> create table tab_no_index(id int,name varchar(10)) engine=innodb;
Query OK, 0 rows affected (0.15 sec)
mysql> insert into tab_no_index values(1,'1'),(2,'2'),(3,'3'),(4,'4');
Query OK, 4 rows affected (0.00 sec)
Records: 4  Duplicates: 0  Warnings: 0
```

![img](https://xuzhongcn.github.io/mysql/03/mdpic/9.png)

session1只对id=1加了排他锁,但是session2在请求其他行排他锁时出现了锁等待.

**2. 由于MySQL的行锁是针对索引加的锁，不是针对记录加的锁，所以虽然是访问不同行的记录，但是如果是使用相同的索引键，是会出现锁冲突的。应用设计的时候要注意这一点**

```
在如表20-11所示的例子中，表tab_with_index的id字段有索引，name字段没有索引：
mysql> alter table tab_with_index drop index name;
Query OK, 4 rows affected (0.22 sec)
Records: 4  Duplicates: 0  Warnings: 0
mysql> insert into tab_with_index  values(1,'4');
Query OK, 1 row affected (0.00 sec)
mysql> select * from tab_with_index where id = 1;
+------+------+
| id   | name |
+------+------+
| 1    | 1    |
| 1    | 4    |
+------+------+
2 rows in set (0.00 sec)
```

![img](https://xuzhongcn.github.io/mysql/03/mdpic/11.png)

**3.当表有多个索引的时候，不同的事务可以使用不同的索引锁定不同的行，另外，不论是使用主键索引、唯一索引或普通索引，InnoDB都会使用行锁来对数据加锁**

```
在如表20-12所示的例子中，表tab_with_index的id字段有主键索引，name字段有普通索引：
mysql> alter table tab_with_index add index name(name);
Query OK, 5 rows affected (0.23 sec)
Records: 5  Duplicates: 0  Warnings: 0
```

![img](https://xuzhongcn.github.io/mysql/03/mdpic/12.png)

 **4. 即便在条件中使用了索引字段，但是否使用索引来检索数据是由MySQL通过判断不同执行计划的代价来决定的**


如果MySQL认为全表扫描效率更高，比如对一些很小的表，它就不会使用索引，这种情况下InnoDB将使用表锁，而不是行锁。

## 间隙锁（Next-Key锁）

当范围查询时,InnoDB对已有数据记录的索引项加锁,针对键值在条件范围内但不存在的记录,叫做**间隙（GAP)**,也会加**间隙锁(next-key lock)**.

InnoDB使用间隙锁的目的，一方面是为了**防止幻读**;另外一方面，是为了**满足其恢复和复制的需要**;

特别注意:InnoDB除了通过范围条件加锁时使用间隙锁外，如果使用相等条件请求给一个不存在的记录加锁，InnoDB也会使用间隙锁.

![img](https://xuzhongcn.github.io/mysql/03/mdpic/13.png)

## 恢复和复制的需要，对InnoDB锁机制的影响

MySQL的恢复机制（复制其实就是在Slave Mysql不断做基于BINLOG的恢复）有以下特点:

1. MySQL的恢复是SQL语句级的，也就是重新执行BINLOG中的SQL语句。这与Oracle数据库不同，Oracle是基于数据库文件块的
2. MySQL的Binlog是按照事务提交的先后顺序记录的，恢复也是按这个顺序进行的。这点也与Oralce不同，Oracle是按照系统更新号（System Change Number，SCN）来恢复数据的，每个事务开始时，Oracle都会分配一个全局唯一的SCN，SCN的顺序与事务开始的时间顺序是一致的

这导致了无论是RC还是RR下,InnoDB都要使用间隙锁.

对于`insert into target_tab select * from source_tab where ...`和`create table new_tab ...select ... From source_tab where ...(CTAS)`这种SQL语句，用户并没有对source_tab做任何更新操作，但MySQL对这种SQL语句做了特别处理.
示例如下:

![img](https://xuzhongcn.github.io/mysql/03/mdpic/14.png)

InnoDB给source_tab加了共享锁,是为了保证恢复和复制的正确性。因为不加锁的话，如果在上述语句执行过程中，其他事务对source_tab做了更新操作，就可能导致数据恢复的结果错误。

如果一定要使用这种SQL来实现业务逻辑,又不希望对源表的并发更新产生影响,可以采取以下两种措施:

1. 将innodb_locks_unsafe_for_binlog的值设置为“on”，强制MySQL使用多版本数据一致性读。但付出的代价是可能无法用binlog正确地恢复或复制数据，因此，不推荐使用这种方式
2. 通过使用“select * from source_tab ... Into outfile”和“load data infile ...”语句组合来间接实现，采用这种方式MySQL不会给source_tab加锁。

InnoDB在不同隔离级别下的一致性读及锁的差异

![img](https://xuzhongcn.github.io/mysql/03/mdpic/16.png)

对于许多SQL，隔离级别越高，InnoDB给记录集加的锁就越严格（尤其是使用范围条件的时候），产生锁冲突的可能性也就越高，从而对并发性事务处理性能的影响也就越大。

因此，我们在应用中，应该尽量使用较低的隔离级别，以减少锁争用的机率。实际上，通过优化事务逻辑，大部分应用使用Read Commited隔离级别就足够了。

## 什么时候使用表锁

InnoDB中一般使用行锁,但是在个别特殊事务中，也可以考虑使用表级锁.

1. 事务需要更新大部分或全部数据，表又比较大，如果使用默认的行锁，不仅这个事务执行效率低，而且可能造成其他事务长时间锁等待和锁冲突，这种情况下可以考虑使用表锁来提高该事务的执行速度
2. 事务涉及多个表，比较复杂，很可能引起死锁，造成大量事务回滚。这种情况也可以考虑一次性锁定事务涉及的表，从而避免死锁、减少数据库因事务回滚带来的开销。

### 使用表锁注意点

1. LOCK TABLES由MySQL Server负责,仅当autocommit=0、innodb_table_locks=1（默认设置）时,InnoDB才能自动识别涉及表锁的死锁,否则InnoDB将无法自动检测并处理这种死锁.
2. 使用 LOCK TABLES要将AUTOCOMMIT设为0,否则MySQL不会给表加锁;事务提交前不能解锁表,因为UNLOCK TABLES会隐含地提交事务；表锁必须通过UNLOCK TABLES释放

```
例如，如果需要写表t1并从表t读，可以按如下做：
SET AUTOCOMMIT=0;
LOCK TABLES t1 WRITE, t2 READ, ...;
[do something with tables t1 and t2 here];
COMMIT;
UNLOCK TABLES;
```

## 关于死锁

但在InnoDB中，除单个SQL组成的事务外，锁是逐步获得的，这就决定了在InnoDB中发生死锁是可能的.示例如下:

![img](https://xuzhongcn.github.io/mysql/03/mdpic/17.png)

发生死锁后，InnoDB一般都能自动检测到，并使一个事务释放锁并回退，另一个事务获得锁，继续完成事务。但在涉及外部锁，或涉及表锁的情况下，InnoDB并不能完全自动检测到死锁，这需要通过设置锁等待超时参数innodb_lock_wait_timeout来解决.

**避免死锁建议**

1. 尽量约定以相同的顺序来访问表
2. 在程序以批量方式处理数据的时候，如果事先对数据排序，保证每个线程按固定的顺序来处理记录，也可以大大降低出现死锁的可能。
3. 如果要更新记录，应该直接申请足够级别的锁，即排他锁，而不应先申请共享锁，更新时再申请排他锁，因为当用户申请排他锁时，其他事务可能又已经获得了相同记录的共享锁，从而造成锁冲突，甚至死锁
4. 在REPEATABLE-READ隔离级别下，如果两个线程同时对相同条件记录用SELECT...FOR UPDATE加排他锁，在没有符合该条件记录情况下，两个线程都会加锁成功。程序发现记录尚不存在，就试图插入一条新记录，如果两个线程都这么做，就会出现死锁。这种情况下，将隔离级别改成READ COMMITTED，就可避免问题
5. 当隔离级别为READ COMMITTED时，如果两个线程都先执行SELECT...FOR UPDATE，判断是否存在符合条件的记录，如果没有，就插入记录。此时，只有一个线程能插入成功，另一个线程会出现锁等待，当第1个线程提交后，第2个线程会因主键重出错，但虽然这个线程出错了，却会获得一个排他锁！这时如果有第3个线程又来申请排他锁，也会出现死锁。

## 乐观锁、悲观锁

**乐观锁(Optimistic Lock)**：假设不会发生并发冲突，只在提交操作时检查是否违反数据完整性。 乐观锁不能解决脏读的问题。乐观锁适用于多读的应用类型，这样可以提高吞吐量，像数据库如果提供类似于write_condition机制的其实都是提供的乐观锁。

**悲观锁(Pessimistic Lock)**：假定会发生并发冲突，屏蔽一切可能违反数据完整性的操作。



# 小结

- （1）共享读锁（S）之间是兼容的，但共享读锁（S）与排他写锁（X）之间，以及排他写锁（X）之间是互斥的，也就是说读和写是串行的。
- （2）在一定条件下，MyISAM允许查询和插入并发执行，我们可以利用这一点来解决应用中对同一表查询和插入的锁争用问题。
- （3）MyISAM默认的锁调度机制是写优先，这并不一定适合所有应用，用户可以通过设置LOW_PRIORITY_UPDATES参数，或在INSERT、UPDATE、DELETE语句中指定LOW_PRIORITY选项来调节读写锁的争用。
- （4）由于表锁的锁定粒度大，读写之间又是串行的，因此，如果更新操作较多，MyISAM表可能会出现严重的锁等待，可以考虑采用InnoDB表来减少锁冲突。

对于InnoDB表，本章主要讨论了以下几项内容。

1. InnoDB的行锁是基于锁引实现的，如果不通过索引访问数据，InnoDB会使用表锁。
2. 介绍了InnoDB间隙锁（Next-key)机制，以及InnoDB使用间隙锁的原因。
3. 在不同的隔离级别下，InnoDB的锁机制和一致性读策略不同。
4. MySQL的恢复和复制对InnoDB锁机制和一致性读策略也有较大影响。
5. 锁冲突甚至死锁很难完全避免。

在了解InnoDB锁特性后，用户可以通过设计和SQL调整等措施减少锁冲突和死锁，包括：

1. 尽量使用较低的隔离级别；
2. 精心设计索引，并尽量使用索引访问数据，使加锁更精确，从而减少锁冲突的机会；
3. 选择合理的事务大小，小事务发生锁冲突的几率也更小；
4. 给记录集显示加锁时，最好一次性请求足够级别的锁。比如要修改数据的话，最好直接申请排他锁，而不是先申请共享锁，修改时再请求排他锁，这样容易产生死锁；
5. 不同的程序访问一组表时，应尽量约定以相同的顺序访问各表，对一个表而言，尽可能以固定的顺序存取表中的行。这样可以大大减少死锁的机会；
6. 尽量用相等条件访问数据，这样可以避免间隙锁对并发插入的影响；
7. 不要申请超过实际需要的锁级别；除非必须，查询时不要显示加锁；
8. 对于一些特定的事务，可以使用表锁来提高处理速度或减少死锁的可能。








参考:[一张图彻底搞懂 MySQL 的锁机制](https://learnku.com/articles/39212?order_by=vote_count&#replies)

[MySQL锁总结](https://zhuanlan.zhihu.com/p/29150809)

 [MySQL中的锁机制 ](https://www.cnblogs.com/jojop/p/13982679.html)

[mysql 索引加锁分析](https://www.jianshu.com/p/13f5777966dd)