# Mongo中的事务实现

## WT的事务构造

WT中的事务基于`snapshot(事务快照)`、`MVCC (多版本并发控制)`和`redo log(重做日志)`三种技术,并且WT引擎中还定义了**全局事务管理器**实现这三种技术.

事务描述对象如下:
```c
wt_transaction{

    transaction_id:    本次事务的**全局唯一的ID**，用于标示事务修改数据的版本号
    
    snapshot_object:   当前事务开始或者操作时刻其他正在执行且并未提交的事务集合,用于事务隔离
    
    operation_array:   本次事务中已执行的操作列表,用于事务回滚。
    
    redo_log_buf:      操作日志缓冲区。用于事务提交后的持久化
    
    state:             事务当前状态

}
```

### WT中的MVCC实现

不同于MySQL中的redolog 版本链,WT的MVCC是基于K/V的链表.链表结构如下:

```c
wt_mvcc{
    transaction_id:    本次修改事务的ID
    value:             本次修改后的值
}
```

新的事务会append到链表头部,每次读取也是从链表头部读取,根据链表中的事务id和本次事务id比较判断可见性.

![mongo-y-trans-1](..\img\mongo-y-trans-1.png)

从早到晚事务时间排序依次是 T0->T5,此时T5想要查看数据,会比较T4但是T4为提交,向前移动到T2,但是T2已经被回滚,再向前移动到T1,可以查看,此时T5读取的数据是11.

###  WT事务snapshot

snapshot是指事务开始或进行操作之前对WT引擎内部正在操作或即将操作的事务一次截屏,保存当时整个事务引擎的所有事务状态.通过snapshot可以判断哪些事务可见.如果T6执行了snapshot那么下图就是此时的snapshot.

![mongo-y-trans-2](..\img\mongo-y-trans-2.png)

snapshot数据格式如下
```c
snapshot_object = {

     snap_min=T1,   最小执行事务
    
     snap_max=T5,   最大事务
    
     snap_array={T1, T4, T5},  所有正在执行的写事务序列

};
```

针对T6事务来说,可以修改的区间为`[0, T1)`和`[snap_min,snap_max]`中已经提交的事务.凡是出现在snap_array中或者大于snap_max的事务对T6来说都是不可见的.这个就是snapshot的隔离机制.

### 全局事务管理器

snapshot是基于全局事务管理器实现的,全局事务管理器的数据结构如下

```c
wt_txn_global{

     current_id:       全局写事务ID产生种子,一直递增
    
     oldest_id:        系统中最早产生且还在执行的写事务ID
    
     transaction_array: 系统事务对象数组，保存系统中所有的事务对象
    
     scan_count:     正在扫描transaction_array数组的线程事务数，用于建立snapshot过程的无锁并发

}

```

其中transaction_array包含了所有正在执行的对象.在建立snapshot时会对该字段扫描确定,snapshot_object属性.通过对scan_count值的设置无锁并发.

![mongo-y-trans-3](..\img\mongo-y-trans-3.png)

### 事务ID

只有写事务才有事务ID,纯读事务,事务ID会被分配为`WT_TNX_NONE`(=0),不会被snapshot.

**当它第一次对事务进行写时，会在数据修改前通过全局事务管理器中的current_id来分配一个全局唯一的事务ID**。也是通过CAS原子替换.

## 事务过程

一般事务是两个阶段：**事务执行**和**事务提交**。事务过程有：`创建开启`、`执行`、`提交`和`回滚`。

### 事务开启

当事务开启时,全局事务管理区会创建一个事务对象,并去获取事务隔离级别,redo log刷盘方式,并将事务状态修改为执行状态.如果隔离级别是最高的`ISOLATION_SNAPSHOT`级,此时就会产生一个快照.

### 事务执行

事务对象中包含了两个属性,一个是`operation_array`记录本次事务的执行的操作,一个是`redo_log_buf`.在`operation_array`的数组单元中，包含了一个指向MVCC list对应修改版本值的指针。那么详细的更新操作流程如下：

- 创建一个mvcclist中的值单元对象(update)
- 根据事务对象的transactionid和事务状态判断是否为本次事务创建了写的事务ID，如果没有，为本次事务分配一个事务ID，并将事务状态设成HAS_TXN_ID状态。
- 将本次事务的ID设置到update单元中作为mvcc版本号。
- 创建一个operation对象，并将这个对象的值指针指向update,并将这个operation加入到本次事务对象的operation_array
- 将update单元加入到mvcc list的链表头上。
- 写入一条redo log到本次事务对象的redo_log_buf当中。

![mongo-y-trans-4](..\img\mongo-y-trans-4.png)



### 事务提交

先将要提交的事务对象中的redo_log_buf中的数据写入到redo log file(重做日志文件)中，并将redo log file持久化到磁盘上.清除提交事务对象的snapshot object,再将提交的事务对象中的`transaction_id`设置为`WT_TNX_NONE`,保证其他事务在创建系统事务snapshot时本次事务的状态是已提交状态.

### 事务回滚

回滚时先遍历`operation_array`,对元素对应的update事务id设置为`WT_TXN_ABORTED（= uint64_max）`,表示对应的事务修改单元值已经被回滚,其事务读取时跳过该值即可.

## 事务隔离

MongoDB中并没有像传统关系型数据库一样将隔离级别划分为`Read-Uncommited(未提交读)`、`Read-Commited(提交读)`、`Repeatable-Read(可重复读)`和`Serializable(串行化)`.而是根据自己的特点划分为`Read-Uncommited(未提交读)`、`Read-Commited(提交读)`和`snapshot-isolation(快照隔离)`.

不管选择哪种事务隔离方式都是基于snapshot实现的.已改图为例分别讲解

![mongo-y-trans-1](..\img\mongo-y-trans-1.png)



### Read-Uncommited

`Read-Uncommited`读未提交,选择的策略是将`snap_object.snap_array`置空,读取mvcc list版本值时,始终读取链表头部节点的数据.上图中T5读取时直接读取T4事务值.

###  Read-Commited

` Read-Commited`读已提交,该事务是指读取数据时总是读取最新的提交数据,选择策略是在每次执行操作时都对事务创建快照,然后读取快照中最新值.但是因为快照的不同,可能会出现前一次与后一次读到的数据不一致,被称之为"幻象读".

### Snapshot-Isolation

Snapshot-Isolation:快照隔离,是读取开始事务时最后提交的版本,整个过程都只会看到这个版本.选择的策略是事务开始时会对正在执行的事务做一个snapshot,这个snapshot会一直被沿用到事务提交或回滚.

## 事务日志

上述的事务修改都是在内存中进行的,那么怎么保证事务的持久性?此处WT引擎引入了`redo log`的方式实现.他的操作日志是基于K/V形式,而不是基于btree page的物理日志. 它会记录操作类型,操作key和操作的value.

```c
{
    Operation = insert,(动作)
    Key = 10,
    Value = 20
};
上述表示的是 插入一个 key = 10 value = 20的日志
```

在执行完一个操作后,该日志会append到事务对象的`redo_log_buf`属性中,等待事务提交时,会将数据同步写入磁盘文件.想要恢复数据可以通过上一个checkpoint重演这个磁盘文件来恢复提交的事务保持持久性.

在WT引擎中定义一个叫做LSN序号结构，操作日志对象是通过LSN来确定存储的位置的，LSN就是LogSequence Number(日志序列号)，它在WT的定义是文件序号加文件偏移:

```text
wt_lsn{
    file:      文件序号，指定是在哪个日志文件中
    offset:    文件内偏移位置，指定日志对象文件内的存储文开始位置
}
WT通过LSN管理重做日志对象
```

### 日志格式

事务操作日志为logrec对象,事务的每个操作都会被记录为一个logop,一个logrec包含了多个logop,logrec是通过序列化事务操作动作和参数得到的一个二进制buffer.这个buffer是通过事务和操作类型确定格式.

WT中的日志分为4类：分别是建立checkpoint的操作日志(LOGREC_CHECKPOINT)、普通事务操作日志(LOGREC_COMMIT)、btree page同步刷盘的操作日志(LOGREC_FILE_SYNC)和提供给引擎外部使用的日志(LOGREC_MESSAGE).

LOGREC_COMMIT 日志中根据操作类型不同分为LOG_PUT(增/改),LOG_REMOVE(删除).日志对象会携带btree的索引文件ID,提交事务的ID等.

![mongo-y-trans-6](..\img\mongo-y-trans-6.png)

### WAL与日志写并发

WAL(Write-Ahead Log):写前日志,WT引擎会在事务提交前将执行的操作记录到磁盘文件中.

事务日志的写入过程:

- 事务在执行第一个写操作时，先会在事务对象（wt_transaction）中的redo_log_buf的缓冲区上创建一个logrec对象，并将logrec中的事务类型设置成LOGREC_COMMIT。

- 然后在事务执行的每个写操作前生成一个logop对象，并加入到事务对应的logrec中。

- 在事务提交时，把logrec对应的内容整体写入到一个全局log对象的slot buffer中并等待写完成信号。

- Slot buffer会根据并发情况合并同时发生的提交事务的logrec，然后将合并的日志内容同步刷入磁盘（sync file），最后告诉这个slot buffer对应所有的事务提交刷盘完成。

- 提交事务的日志完成，事务的执行结果也完成了持久化。

![mongo-y-trans-7.png](..\img\mongo-y-trans-7.png)

会将同时发生提交的事务日志合并到一个slotbuffer中，先完成合并的事务线程会同步等待一个完成刷盘信号，最后完成日志数据合并的事务线程将slotbuffer中的所有日志数据sync到磁盘上并通知在这个slotbuffer中等待其他事务线程刷盘完成。

acitve_ready_slot和一个slot_pool数组结构，大致如下定义：

```c
wt_log{
    
     . . .
    
     active_slot:       准备就绪且可以作为合并logrec的slotbuffer对象
    
     slot_pool:         系统所有slot buffer对象数组，包括：正在合并的、准备合并和闲置的slot buffer。

}

wt_log_slot{

state:             当前slot的状态,ready/done/written/free这几个状态

buf:          缓存合并logrec的临时缓冲区

group_size:        需要提交的数据长度

slot_start_offset: 合并的logrec存入log file中的偏移位置

}

```

## 事务恢复

在WT中是将整个BTREE上的page做一次checkpoint并写入磁盘,让事务入库.WT中的checkpoint是以append方式管理,也就是说WT会保存多个checkpoint版本.WT可以通过重演其中任意版本恢复事务修改.WT会将checkpoint保存到元数据表中,元数据表也会有自己的checkpoint和redolog.

但是保存元数据表的checkpoint是保存在WiredTiger.wt文件中，系统重演普通表的提交事务之前，先会重演元数据事务提交修改







