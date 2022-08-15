# InnoDB之Undo LOG

## Undo Log的作用

Undo Log用来记录每次修改之前的历史值，配合Redo Log用于故障恢复。这也就是InnoDB中Undo Log的第一个作用：

### 1. 事务回滚

针对原子性问题,直观的想法是等到事务真正提交时，才能允许这个事务的任何修改落盘，也就是No-Steal策略。但是这种做法一方面造成很大的内存空间压力，另一方面提交时的大量随机IO会极大的影响性能.数据库实现中通常会在正常事务进行中，就不断的连续写入Undo Log，来记录本次修改之前的历史值,当Crash发生时通过Recovery过程将未提交事务修改抹掉.

### 2. MVCC（Multi-Versioin Concurrency Control）

为了避免写事务等待读事务,几乎所有主流数据库都采用了多版本并发控制（MVCC）的方式,InnoDB在这里复用了Undo Log中已经记录的历史版本数据来满足MVCC的需求。

## 什么样的Undo Log

Undo Log的设计思路不同于Redo Log，Undo Log需要的是事务之间的并发，以及方便的多版本数据维护，其重放逻辑不希望因DB的物理存储变化而变化。因此，InnoDB中的Undo Log采用了基于事务的**Logical Logging**的方式。

Undo Log日志本身也像其他的数据库数据一样，会写自己对应的Redo Log，通过Redo Log来保证自己的原子性。因此，更合适的称呼应该是**Undo Data**。

## Undo Record中的内容

每当InnoDB中需要修改某个Record时，都会将其历史版本写入一个Undo Log中，对应的Undo Record是Update类型。当插入新的Record时，还没有一个历史版本，但为了方便事务回滚时做逆向（Delete）操作，这里还是会写入一个Insert类型的Undo Record。

### Insert类型的Undo Record

该类型对应TRX_UNDO_INSERT_REC类型.Insert Undo Record仅仅是为了可能的事务回滚准备的,只记录对应的Record的key,供回滚时Record位置即可.

![insert_undo_record](http://catkang.github.io/assets/img/innodb_undo/insert_undo_record.png)

其中Undo Number是Undo的一个递增编号，Table ID用来表示是哪张表的修改。下面因为Fields长度不定,主键可能由多个field组成,所以这里需要记录Record完整的主键信息，回滚的时候可以通过这个信息在索引中定位到对应的Record。在Undo Record的头尾还各留了两个字节用户记录其前序和后继Undo Record的位置。

### Update类型的Undo Record

当Record某个历史版本还在使用,那么该record不能被真正删除,只是修改对应的Delete Mark标记.如果Record又重新插入,也只是修改Delete Mark标记,这两种情况将delete和insert变成了update操作.这里的Update Undo Record会对应三种Type：`TRX_UNDO_UPD_EXIST_REC`、`TRX_UNDO_DEL_MARK_REC`和`TRX_UNDO_UPD_DEL_REC`.他们的存储内容类似:

![update_undo_record](http://catkang.github.io/assets/img/innodb_undo/update_undo_record.png)

相对于Insert Undo Record中的信息,增加了

- Transaction Id记录了产生这个历史版本事务Id，用作后续MVCC中的版本可见性判断

- Rollptr指向的是该记录的上一个版本的位置，包括space number，page number和page内的offset。沿着Rollptr可以找到一个Record的所有历史版本。
- Update Fields中记录的就是当前这个Record版本相对于其之后的一次修改的Delta信息，包括所有被修改的Field的编号，长度和历史值。

## Undo Record的组织方式

### 逻辑组织方式 - Undo Log

事务修改记录时会产生一组undo record,这些Undo Record首尾相连就组成了**Undo Log**.Log中除了一个个Undo Record外还在开头增加了一个Undo Log Header来记录一些必要的控制信息,结构如下所示:

![undo_log](http://catkang.github.io/assets/img/innodb_undo/undo_log.png)

Header包括的信息如下:

- Trx ID: 记录产生这个Undo Log的事务ID
- Trx No:记录事务提交顺序,可以用来判断是否能清除
- Delete Mark:记录Undo Log中有没有TRX_UNDO_DEL_MARK_REC类型的Undo Record,避免Purge时不必要的扫描
- Log Start Offset:记录Undo Log Header的结束位置,方便之后增加内容
- Next Undo Log,Prev Undo Log:标记前后两个Undo Log
- History List Node:将自己挂载到为Purge准备的History List中

索引中的同一个Record被不同事务修改，会产生不同的历史版本，这些历史版本又通过**Rollptr(滚动指针)**穿成一个链表，供MVCC使用。如下图所示：

![undo_logicial](http://catkang.github.io/assets/img/innodb_undo/undo_logical.png)

I，J，K三个事务分别有自己的逻辑上连续的三条Undo Log，每条Undo Log有自己的Undo Log Header.从索引中的这条Record沿着Rollptr可以依次找到这三个事务Undo Log中关于这条记录的历史版本.Insert类型的Undo Record只记录对应的主键值:id=1,而 update类型的还记录了对于的事务的Trx_id以及被修改的field a历史值.

### 物理组织格式 - Undo Segment

Undo Log写入磁盘时以固定块大小为单位,默认为16KB.为了实现高效的空间分配,复用,避免空间浪费.InnoDB的基本思路是让**多个较小的Undo Log紧凑存在一个Undo Page中**,而**较大的Undo Log随着不断写入,按需分配足够多的Undo Page分散承载**.存储方式如下图:

![undo_physical](http://catkang.github.io/assets/img/innodb_undo/undo_physical.png)

每个写事务开始前都需持有一个Undo Segment,该段的空间管理由一个FSP的 Segment控制.这个索引中的Leaf Node Segment和Non-Leaf Node Segment的管理方式是一致的.

每个Segment至少会持有一个Undo Page,每个page会在开头38到56字节记录Header.其中包括了Page的类型,最后一条Record 的位置,当前page还空闲部分你的开头.

segment中第一个page还会在56到86字节记录segment Header,这个就是这个Undo Segment中磁盘空间管理的Handle.记录了这个Undo Segment的状态,比如TRX_UNDO_CACHED、TRX_UNDO_TO_PURGE等;这个Undo Segment中最后一条Undo Record的位置;这个FSP Segment的Header;记忆分配出来所有Undo Page的链表.

Undo Page剩余的空间都是用来存放Undo Log的.对于比较短的Undo Log,为了避免Page内的空间浪费,InnoDB会复用Page来存放多个.而对于比较长的Undo Log可能会分配多个Undo Page来存放.Undo Page的复用只会发生在第一个Page

### 文件组织方式 - Undo Tablespace

每个事务至少持有一个Undo Segment，当有大量写事务并发运行时，就需要存在多个Undo Segment.InnoDB中准备了大量的槽位,按照1024一组分为**Rollback Segment**.

每个Undo Tablespace最多会包含128个Rollback Segment,Undo Tablespace文件中的第三个Page会固定作为这128个Rollback Segment的目录,也就是**Rollback Segment Arrary Header**,其中会有指针指向各个Rollback Segment Header所在的Page.

**Rollback Segment Header**是按需分配的，其中包含1024个Slot，每个Slot占四个字节，指向一个**Undo Segment**的First Page.

除此之外还会记录该Rollback Segment中已提交事务的History List,后续的Purge过程会顺序从这里开始回收工作.

可以看出Rollback Segment的个数会直接影响InnoDB支持的最大事务并发数.MySQL8.0支持最多127个独立的Undo Tablespace,避免了ibdata1的膨胀，方便undo空间回收,同事也增加了最大并发写事务数.

![undo_tablespace](http://catkang.github.io/assets/img/innodb_undo/undo_tablespace.png)

### 内存组织结构

在内存中也会维护对应的数据结构来管理Undo Log，如下图所示：

![image-20220801160927367](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220801160927367.png)

对应每个磁盘Undo Tablespace会有一个**undo::Tablespace**的内存结构，其中最主要的就是一组trx_rseg_t的集合，**trx_rseg_t**对应的就是上面介绍过的一个Rollback Segment Header，除了一些基本的元信息之外，trx_rseg_t中维护了四个trx_undo_t的链表，**Update List**中是正在被使用的用于写入Update类型Undo的Undo Segment；**Update Cache List**中是空闲空间比较多，可以被后续事务复用的Update类型Undo Segment;对应的，**Insert List**和**Insert Cache List**分别是正在使用中的Insert类型Undo Segment，和空间空间较多，可以被后续复用的Insert类型Undo Segment。因此**trx_undo_t**对应的就是上面介绍过的Undo Segment。

## Undo的写入

**获得可用Undo Segment**

当写事务开始时，会先通过**trx_assign_rseg_durable**分配一个Rollback Segment，该事务的内存结构trx_t也会通过rsegs指针指向对应的trx_rseg_t内存结构，这里的分配策略很简单，就是依次尝试下一个Active的Rollback Segment。之后当第一次真正产生修改需要写Undo Record的时，会调用**trx_undo_assign_undo**来获得一个Undo Segment。这里会优先复用trx_rseg_t上Cached List中的trx_undo_t，也就是已经分配出来但没有被正在使用的Undo Segment，如果没有才调用**trx_undo_create**创建新的Undo Segment，trx_undo_create中会轮询选择当前Rollback Segment中可用的Slot，也是就值FIL_NUL的Slot，申请新的Undo Page，初始化Undo Page Header，Undo Segment Header等信息，创建新的trx_undo_t内存结构并挂到trx_rseg_t的对应List中。

**初始化Head 写入内容**

获得了可用的Undo Segment之后，该事务会在合适的位置初始化自己的Undo Log Header，之后，其所有修改产生的Undo Record都会顺序的通过**trx_undo_report_row_operation**顺序的写入当前的Undo Log，其中会根据是insert还是update类型，分别调用**trx_undo_page_report_insert**或者**trx_undo_page_report_modify**

当一个Page写满后，会调用**trx_undo_add_page**来在当前的Undo Segment上添加新的Page，新Page写入Undo Page Header之后继续供事务写入Undo Record，为了方便维护，这里有一个限制就是单条Undo Record不跨page，如果当前Page放不下，会将整个Undo Record写入下一个Page

**回收空间**

当事务结束（commit或者rollback）之后，如果只占用了一个Undo Page，且当前Undo Page使用空间小于page的3/4，这个Undo Segment会保留并加入到对应的insert/update cached list中。否则，insert类型的Undo Segment会直接回收，而update类型的Undo Segment会等待后台的Purge做完后回收。根据不同的情况，Undo Segment Header中的State会被从TRX_UNDO_ACTIVE改成TRX_UNDO_TO_FREE，TRX_UNDO_TO_PURGE或TRX_UNDO_CACHED，这个修改其实就是**InnoDB的事务结束的标志**，无论是Rollback还是Commit，在这个修改对应的Redo落盘之后，就可以返回用户结果，并且Crash Recovery之后也不会再做回滚处理。

## Undo for Rollback

无论是因为什么发生回滚,在Undo层面来看，这些回滚的操作是一致的，基本的过程就是从该事务的Undo Log中，从后向前依次读取Undo Record，并根据其中内容做逆向操作，恢复索引记录。

回滚的入口是函数**row_undo**，其中会先调用**trx_roll_pop_top_rec_of_trx**获取并删除该事务的最后一条Undo Record。当回滚发生时,先找到事务最后一条Undo Page的Header,然后通过Header定位Record的位置,然后根据前序指针依次回滚.

拿到一个Undo Record之后，自然地，就是对其中内容的解析，这里会调用**row_undo_ins_parse_undo_rec**，从Undo Record中获取修改行的table，解析出其中记录的主键信息，如果是update类型，还会拿到一个update vector记录其相对于更新的一个版本的变化。

**TRX_UNDO_INSERT_REC**类型的Undo回滚在**row_undo_ins**中进行，insert的逆向操作当然就是delete，根据从Undo Record中解析出来的主键，用**row_undo_search_clust_to_pcur**定位到对应的ROW， 分别调用**row_undo_ins_remove_sec_rec**和**row_undo_ins_remove_clust_rec**在二级索引和主索引上将当前行删除。

完成回滚的Undo Log部分，会调用**trx_roll_try_truncate**进行回收，对不再使用的page调用**trx_undo_free_last_page**将磁盘空间交还给Undo Segment，这个是写入过程中**trx_undo_add_page**的逆操作。

## Undo for MVCC

所谓历史版本就是假设在该只读事务开始的时候对整个DB打一个快照，之后该事务的所有读请求都从这个快照上获取.InnoDB的做法，是在读事务第一次读取的时候获取一份ReadView，并一直持有，其中记录所有当前活跃的写事务ID，由于写事务的ID是自增分配的，通过这个ReadView我们可以知道在这一瞬间，哪些事务已经提交哪些还在运行，根据Read Committed的要求，对修改的可见性判断.

作为存储历史版本的Undo Record，其中记录的trx_id就是做这个可见性判断的，对应的主索引的Record上也有这个值。

如下图所示，事务R开始需要查询表t上的id为1的记录，R开始时事务I已经提交，事务J还在运行，事务K还没开始，这些信息都被记录在了事务R的ReadView中。事务R从索引中找到对应的这条Record[1, C]，对应的trx_id是K，不可见。沿着Rollptr找到Undo中的前一版本[1, B]，对应的trx_id是J，不可见。继续沿着Rollptr找到[1, A]，trx_id是I可见，返回结果。

![undo_mvcc](http://catkang.github.io/assets/img/innodb_undo/undo_mvcc.png)

作为Logical Log，Undo中记录的其实是前后两个版本的diff信息，而读操作最终是要获得完整的Record内容的，也就是说这个沿着rollptr指针一路查找的过程中需要用Undo Record中的diff内容依次构造出对应的历史版本，这个过程在函数**row_search_mvcc**中，其中**trx_undo_prev_version_build**会根据当前的rollptr找到对应的Undo Record位置，这里如果是rollptr指向的是insert类型，或者找到了已经Purge了的位置，说明到头了，会直接返回失败。否则，就会解析对应的Undo Record，恢复出trx_id、指向下一条Undo Record的rollptr、主键信息，diff信息update vector等信息。之后通过**row_upd_rec_in_place**，用update vector修改当前持有的Record拷贝中的信息，获得Record的这个历史版本。之后调用自己ReadView的**changes_visible**判断可见性，如果可见则返回用户。完成这个历史版本的读取。

## Undo for Crash Recovery

崩溃恢复时,需要利用Undo中的信息将未提交的事务的所有影响回滚，以保证数据库的Failure Atomic(原子性).

Undo本身也有自己对应的Redo Log类型：**MLOG_UNDO_INIT**类型在Undo Page初始化的时候记录初始化；在分配Undo Log的时候，需要重用Undo Log Header或需要创建新的Undo Log Header的时候，会分别记录**MLOG_UNDO_HDR_REUSE**和**MLOG_UNDO_HDR_CREATE**类型的Redo Record；**MLOG_UNDO_INSERT**是最常见的，在Undo Log里写入新的Undo Record都对应的写这个日志记录写入Undo中的所有内容；最后，**MLOG_UNDO_ERASE_END** 对应Undo Log跨Undo Page时抹除最后一个不完整的Undo Record的操作。

遍历所有的Rollback Segment和其中所有的Undo Segment，通过读取Undo Segment Header中的State，可以知道在Crash前，最后持有这个Undo Segment的事务状态。如果是TRX_UNDO_ACTIVE，说明当时事务需要回滚，否则说明事务已经结束，可以继续清理Undo Segment的逻辑。之后，就可以恢复出Undo Log的内存组织模式，包括活跃事务的内存结构trx_t，Rollback Segment的内存结构trx_rseg_t，以及其中的trx_undo_t的四个链表.

Crash Recovery完成之前，会启动在**srv_dict_recover_on_restart**中启动异步回滚线程**trx_recovery_rollback_thread**，其中对Crash前还活跃的事务，通过**trx_rollback_active**进行回滚，这个过程更上面提到的Undo for Rollback是一致的。

## Undo的清理

InnoDB中每个写事务结束时都会拿一个递增的编号**trx_no**作为事务的提交序号，而每个读事务会在自己的ReadView中记录自己开始的时候看到的最大的trx_no为**m_low_limit_no**。那么，如果一个事务的trx_no小于当前所有活跃的读事务Readview中的这个**m_low_limit_no**，说明这个事务在所有的读开始之前已经提交了，其修改的新版本是可见的， 因此不再需要通过undo构建之前的版本，这个事务的Undo Log也就可以被清理了.

![undo_purge](http://catkang.github.io/assets/img/innodb_undo/undo_purge.png)

上图中的Transaction J不再被读事务看到,所以可以被清理.

Undo的清理工作是由专门的后台线程**srv_purge_coordinator_thread**进行扫描和分发， 并由多个**srv_worker_thread**真正清理的。coordinator会首先在函数**trx_purge_attach_undo_recs**中扫描innodb_purge_batch_size配置个Undo Records，作为一轮清理的任务分发给worker。

### 扫描一批要清理Undo Records

事务结束的时候，对于需要Purge的Update类型的Undo Log，会按照事务提交的顺序trx_no，挂载到Rollback Segment Header的**History List**上.然后安装trx_no升序遍历,进行清理.每个History List内部事务有序，但还需要从多个History List上找一个trx_no全局有序的序列，如下图所示：

![undo_purge_1](http://catkang.github.io/assets/img/innodb_undo/undo_purge_1.png)

图中的事务编号是按照InnoDB这里引入了一个堆结构**purge_queue**，用来依次从所有History List中找到下一个拥有最小trx_no的事务。purge_queue中记录了所有等待Purge的Rollback Segment和其History中trx_no最小的事务，**trx_purge_choose_next_log**依次从purge_queue中pop出拥有全局最小trx_no的Undo Log。调用**trx_purge_get_next_rec**遍历对应的Undo Log，处理每一条Undo Record。之后继续调用**trx_purge_rseg_get_next_history_log**从purge_queue中获取下一条trx_no最小的Undo Log，并且将当前Rollback Segment上的下一条Undo Log继续push进purge_queue，等待后续的顺序处理。对应上图的处理过程和对应的函数调用，如下图所示：

```
[trx_purge_choose_next_log] Pop T1 from purge_queue;
[trx_purge_get_next_rec] Iterator T1;
[trx_purge_rseg_get_next_history_log] Get T1 next: T5;
[trx_purge_choose_next_log] Push T5 into purge_queue;

[trx_purge_choose_next_log] Pop T4 from purge_queue;
[trx_purge_get_next_rec] Iterator T4;
[trx_purge_rseg_get_next_history_log] Get T4 next: ...;
[trx_purge_choose_next_log] Push ... into purge_queue;

[trx_purge_choose_next_log] Pop T5 from purge_queue;
[trx_purge_get_next_rec] Iterator T5;
[trx_purge_rseg_get_next_history_log] Get T5 next: T6;
[trx_purge_choose_next_log] Push T6 into purge_queue;
......
```

其中，**trx_purge_get_next_rec**会从上到下遍历一个Undo Log中的所有Undo Record,先获取Log Start Offset,然后获取后续的Undo Record,并处理.

![undo_purge_2](http://catkang.github.io/assets/img/innodb_undo/undo_purge_2.png)

对每个要Purge的Undo Record，在真正删除它本身之前，可能还需要处理一些索引上的信息，这是由于正常运行过程中，当需要删除某个Record时，为了保证其之前的历史版本还可以通过Rollptr找到，**Record是没有真正删除的，只是打了Delete Mark的标记**，并作为一种特殊的Update操作记录了Undo Record。那么在对应的TRX_UNDO_DEL_MARK_REC类型的Undo Record被清理之前，需要先从索引上真正地删除这个Delete Mark的记录。

因此Undo Record的清理工作会分为两个过程：

- TRX_UNDO_DEL_MARK_REC类型Undo Record对应的Record的真正删除，称为**Undo Purge**；
- 以及Undo Record本身从旧到新的删除，称为**Undo Truncate**。

除此之外，当配置的独立Undo Tablespace大于两个的时候，InnoDB支持通过重建来缩小超过配置大小的Undo Tablespace：

- Undo Tablespace的重建缩小，称为**Undo Tablespace Truncate**

### Undo Purge

这一步主要针对的是TRX_UNDO_DEL_MARK_REC类型的Undo Record，用来真正的删除索引上被标记为Delete Mark的Record。worker线程会在**row_purge**函数中，循环处理coordinator分配来的每一个Undo Records，先通过**row_purge_parse_undo_rec**，依次从Undo Record中解析出type、table_id、rollptr、对应记录的主键信息以及update vector。之后，针对TRX_UNDO_DEL_MARK_REC类型，调用**row_purge_remove_sec_if_poss**将需要删除的记录从所有的二级索引上删除，调用**row_purge_remove_clust_if_poss**从主索引上删除。另外，TRX_UNDO_UPD_EXIST_REC类型的Undo虽然不涉及主索引的删除，但可能需要做二级索引的删除，也是在这里处理的.

**处理二级索引及主索引信息**

### Undo Truncate

coordinator线程会等待所有的worker完成一批Undo Records的Purge工作，之后尝试清理不再需要的Undo Log，**trx_purge_truncate**函数中会遍历所有的Rollback Segment中的所有Undo Segment，如果其**状态是TRX_UNDO_TO_PURGE，调用trx_purge_free_segment释放占用的磁盘空间并从History List中删除**。否则，说明该Undo Segment正在被使用或者还在被cache（TRX_UNDO_CACHED类型），那么只通过**trx_purge_remove_log_hd**将其从History List中删除。

频次是由参数**innodb_rseg_truncate_frequency**控制的，也就是说要攒**innodb_rseg_truncate_frequency**个batch才进行一次，前面提到每一个batch中会处理**innodb_purge_batch_size**个Undo Records，这也就是为什么我们从show engine innodb status中看到的Undo History List的缩短是跳变的。

### Undo Tablespace Truncate

如果**innodb_trx_purge_truncate**配置打开，在函数**trx_purge_truncate**中还会去尝试重建Undo Tablespaces以缩小文件空间占用。Undo Truncate之后，会在函数**trx_purge_mark_undo_for_truncate**中扫描所有的Undo Tablespace，文件大小大于配置的**innodb_max_undo_log_size**的Tablespace会被标记为inactive，每一时刻最多有一个Tablespace处于inactive，inactive的Undo Tablespace上的所有Rollback Segment都不参与给新事物的分配，等该文件上所有的活跃事务退出，并且所有的Undo Log都完成Purge之后，这个Tablespace就会被通过**trx_purge_initiate_truncate**重建，包括重建Undo Tablespace中的文件结构和内存结构，之后被重新标记为active，参与分配给新的事务使用



参考:[庖丁解InnoDB之Undo LOG](https://catkang.github.io/2021/10/30/mysql-undo.html)