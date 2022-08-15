# InnoDB之REDO LOG

## 1. 为什么需要记录REDO

InnoDB会把数据缓存在内存中(InnoDB Buffer Pool),当进程崩溃时会导致数据丢失.为了数据一致性和持久性,REDO LOG会早于对应的page落盘,即WAL(Write Ahead Log).当故障发生后,InnoDB会在重启时,重放REDO,将Page恢复到崩溃前状态.

## 2. 需要什么样的REDO

为了保证REDO的写盘时间,**REDO的数据量要尽量少**;为了保证REDO的重放可重入**REDO操作要保证幂等**;为了加快重启恢复速度,REDO应该是**基于Page**的，即一个REDO只涉及一个Page的修改.

据量小是**Logical Logging**的优点，而幂等以及基于Page正是**Physical Logging**的优点，因此InnoDB采取了一种称为**Physiological Logging**的方式.以page为单位,在page中以逻辑方式记录.

举例说明:`MLOG_REC_UPDATE_IN_PLACE`类型的REDO中记录了对Page中一个Record的修改，方法如下：

> （Page ID，Record Offset，(Filed 1, Value 1) … (Filed i, Value i) … )

其中，PageID指定要操作的Page页，Record Offset记录了Record在Page内的偏移位置，后面的Field数组，记录了需要修改的Field以及修改后的Value.

但是这种记录方式会存在两个**问题**:

**1，需要基于正确的Page状态上重放REDO**

InnoDB默认的Page大小是16KB,大于文件系统默认值4KB,因此可能出现page内容成功一半的情况.InnoDB中采用了**Double Write Buffer**的方式来通过写两次的方式保证恢复的时候找到一个正确的Page状态

**2，需要保证REDO重放的幂等**

InnoDB给每个REDO记录一个全局唯一递增的标号**LSN(Log Sequence Number)**,Page在修改时，会将对应的REDO记录的LSN记录在Page上（FIL_PAGE_LSN字段），这样恢复重放REDO时，就可以来判断跳过已经应用的REDO，从而实现重放的幂等。.

## 3. REDO中记录了什么内容

目前一共有65中REDO类型,根据REDO记录不同的作用对象，可以将这65中REDO划分为三个大类：作用于Page，作用于Space以及提供额外信息的Logic类型。

**1，作用于Page的REDO**

根据作用的Page的不同类型又可以细分为，Index Page REDO，Undo Page REDO，Rtree PageREDO等。比如MLOG_REC_INSERT，MLOG_REC_UPDATE_IN_PLACE，MLOG_REC_DELETE三种类型分别对应于Page中记录的插入，修改以及删除. 具体内容如下:

![redo_insert](http://catkang.github.io/assets/img/innodb_redo/redo_insert.png)

其中，Type就是MLOG_REC_UPDATE_IN_PLACE类型，Space ID和Page Number唯一标识一个Page页，这三项是所有REDO记录都需要有的头信息，后面的是MLOG_REC_UPDATE_IN_PLACE类型独有的，其中Record Offset用给出要修改的记录在Page中的位置偏移，Update Field Count说明记录里有几个Field要修改，紧接着对每个Field给出了Field编号(Field Number)，数据长度（Field Data Length）以及数据（Filed Data）

**2，作用于Space的REDO**

这类REDO针对一个Space文件的修改，如MLOG_FILE_CREATE，MLOG_FILE_DELETE，MLOG_FILE_RENAME分别对应对一个Space的创建，删除以及重命名。文件操作的REDO是在文件操作结束后才记录,在恢复过程中看到这类日志,说明文件操作成功了.因此在恢复过程中大多只是做对文件状态的检查，以MLOG_FILE_CREATE来看看其中记录的内容:

![redo_space](http://catkang.github.io/assets/img/innodb_redo/redo_space.png)

由于是针对Page的操作，这里的Page Number永远是0。在此之后记录了创建的文件flag以及文件名，用作重启恢复时的检查.

**3，提供额外信息的Logic REDO**

还有少数的几个REDO类型不涉及具体的数据修改，只是为了记录一些需要的信息，比如最常见的MLOG_MULTI_REC_END就是为了标识一个REDO组，也就是一个完整的原子操作的结束。

## 4. REDO是如何组织的

REDO从上到下分为三层：逻辑REDO层、物理REDO层和文件层。

#### **逻辑REDO层**

是真正的REDO内容,REDO由多个不同Type的多个REDO记录首尾相连组成,有全局唯一递增偏移**sn**,InnoDB会在全局*log_sys*中维护当前sn的最大值，并在每次写入数据时将*sn*增加REDO内容长度。如下图所示：

![logic_redo](http://catkang.github.io/assets/img/innodb_redo/logic_redo.png)

#### **物理REDO层**

磁盘是块设备，InnoDB中也用Block的概念来读写数据，一个Block的长度OS_FILE_LOG_BLOCK_SIZE等于磁盘扇区的大小512B，每次IO读写的最小单位都是一个Block。Block保存了除REDO外的额外信息,

下图所示一个Log Block的的组成，包括12字节的**Block Header**

前4字节中Flush Flag占用最高位bit，标识一次IO的第一个Block,剩下的31个个bit是Block编号;

之后是2字节的数据长度，取值在[12，508];

紧接着2字节的First Record Offset用来指向Block中第一个REDO组的开始，这个值的存在使得我们对任何一个Block都可以找到一个合法的的REDO开始位置；

最后的4字节Checkpoint Number记录写Block时的*next_checkpoint_number*，用来发现文件的循环使用;

Block末尾是4字节的**Block Tailer**，记录当前Block的Checksum，通过这个值，读取Log时可以明确Block数据有没有被完整写盘。



![image-20200216201419532](http://catkang.github.io/assets/img/innodb_redo/image-20200216201419532.png)

Block中剩余的中间498个字节就是REDO真正内容的存放位置，也就是我们上面说的逻辑REDO.一个Block可能有多个REDO,也有可能一个REDO被拆分到多个Block中.

如下图所示，棕色和红色分别代表Block Header和Tailer，中间的REDO记录由于前一个Block剩余空间不足，而被拆分在连续的两个Block中。

![physical_redo](http://catkang.github.io/assets/img/innodb_redo/physical_redo.png)

由于增加了Block Header和Tailer的字节开销，在物理REDO空间中用LSN来标识偏移，可以看出LSN和SN之间有简单的换算关系：

```c++
constexpr inline lsn_t log_translate_sn_to_lsn(lsn_t sn) {
  return (sn / LOG_BLOCK_DATA_SIZE * OS_FILE_LOG_BLOCK_SIZE +
          sn % LOG_BLOCK_DATA_SIZE + LOG_BLOCK_HDR_SIZE);
}
```

#### **文件层**

REDO会被写入到REDO日志文件中，以ib_logfile0、ib_logfile1…命名，为了避免创建文件及初始化空间带来的开销，InnoDB的REDO文件会循环使用,通过参数*innodb_log_files_in_group*可以指定REDO文件的个数。多个文件首尾相连顺序写入REDO内容,每个文件已Block为单位划分,每个文件的开头固定预留4个Block来记录一些额外的信息，其中第一个Block称为**Header Block**，之后的3个Block在0号文件上用来存储Checkpoint信息，而在其他文件上留空：

![image-20200216222949045](http://catkang.github.io/assets/img/innodb_redo/file_header.png)

其中第一个Header Block的数据区域记录了一些文件信息，如下图所示，4字节的Formate字段记录Log的版本，不同版本的LOG，会有REDO类型的增减，这个信息是8.0开始才加入的；8字节的Start LSN标识当前文件开始LSN，通过这个信息可以将文件的offset与对应的lsn对应起来；最后是最长32位的Creator信息，正常情况下会记录MySQL的版本。

![redo_file_header](http://catkang.github.io/assets/img/innodb_redo/redo_file_header.png)

现在我们将REDO放到文件空间中，如下图所示，逻辑REDO是真正需要的数据，用*sn*索引，逻辑REDO按固定大小的Block组织，并添加Block的头尾信息形成物理REDO，以*lsn*索引，这些Block又会放到循环使用的文件空间中的某一位置，文件中用*offset*索引：

![redo_file](http://catkang.github.io/assets/img/innodb_redo/redo_file.png)

虽然通过LSN可以唯一标识一个REDO位置，但最终对REDO的读写还需要转换到对文件的读写IO，这个时候就需要表示文件空间的offset，他们之间的换算方式如下：

```c++
const auto real_offset =
      log.current_file_real_offset + (lsn - log.current_file_lsn);
```

切换文件时会在内存中更新当前文件开头的文件offset，*current_file_real_offset*，以及对应的LSN，*current_file_lsn*，通过这两个值可以方便地用上面的方式将LSN转化为文件offset。注意这里的offset是相当于整个REDO文件空间而言的，由于InnoDB中读写文件的space层实现支持多个文件，因此，可以将首位相连的多个REDO文件看成一个大文件，那么这里的offset就是这个大文件中的偏移。

## 5. 如何高效地写REDO

REDO从生成到落盘完整过程包括:REDO内容的产生，REDO写入InnoDB Log Buffer，从InnoDB Log Buffer写入操作系统Page Cache，以及REDO刷盘，之后还需要唤醒等待的用户线程完成Commit.

#### **REDO产生**

我们知道事务在写入数据的时候会产生REDO，一次原子的操作可能会包含多条REDO记录，这些REDO可能是访问同一Page的不同位置，也可能是访问不同的Page（如Btree节点分裂）.InnoDB中通过*min-transaction(mtr)*实现REDO的连续.在需要原子操作时调用*mtr_start*,生成mtr,mtr维护一个动态增长的m_log,然后原子操作将需要写的所有REDO先写到m_log,操作结束后调用*mtr_commit*将这些*m_log*中的数据拷贝到InnoDB的Log Buffer。

#### **写入InnoDB Log Buffer**

多个mtr并发写的时候,不同mtr会先用自己的REDO长度,原子地对全局便宜*log.sn*做*fetch_add*，得到自己在Log Buffer中独享的空间.之后不同mtr并行将数据拷贝到各自独享的空间内.

```c++
/* Reserve space in sequence of data bytes: */
const sn_t start_sn = log.sn.fetch_add(len);
```

#### **写入Page Cache**

InnoDB使用*log_writer*完成数据写入到Page Cache.因为Log Buffer中的数据是不同mtr并发写入的,所以会存在空洞,所以*log_writer*需要感知当前Log Buffer中连续日志的末尾，将连续日志通过*pwrite*系统调用写入操作系统Page Cache.

为了应尽可能不影响后续mtr进行数据拷贝，InnoDB在这里引入一个叫做*link_buf*的数据结构，如下图所示：

![link_buf](http://catkang.github.io/assets/img/innodb_redo/link_buf.png)

*link_buf*是一个循环使用的数组，对每个lsn取模可以得到其在*link_buf*上的一个槽位，在这个槽位中记录REDO长度.另外一个线程从开始遍历这个*link_buf*，通过槽位中的长度可以找到这条REDO的结尾位置，一直遍历到下一位置为0的位置，可以认为之后的REDO有空洞，而之前已经连续，这个位置叫做*link_buf*的*tail*。

下面看看*log_writer*和众多mtr是如何利用这个*link_buf*数据结构的。这里的这个*link_buf*为*log.recent_written*，如下图所示：

![link_buf2](http://catkang.github.io/assets/img/innodb_redo/link_buf2.png)

图中上半部分是REDO日志示意图，*write_lsn*是当前log_writer*已经写入到Page Cache中日志末尾，*current_lsn*是当前已经分配给mtr的的最大lsn位置，而*buf_ready_for_write_lsn*是当前*log_writer*找到的Log Buffer中已经连续的日志结尾，从*write_lsn*到*buf_ready_for_write_lsn*是下一次*log_writer*可以连续调用pwrite写入Page Cache的范围，而从*buf_ready_for_write_lsn*到*current_lsn*是当前mtr正在并发写Log Buffer的范围。下面的连续方格便是*log.recent_written*的数据结构，可以看出由于中间的两个全零的空洞导致*buf_ready_for_write_lsn*无法继续推进*

*假如reserve到中间第一个空洞的mtr也完成了写Log Buffer，并更新了*log.recent_written*，如下图：

![redo-next-write-to-log-buffer](http://catkang.github.io/assets/img/innodb_redo/redo-next-write-to-log-buffer.png)

这时，*log_writer*从当前的*buf_ready_for_write_lsn*向后遍历*log.recent_written*，发现这段已经连续：

![redo-next-write-to-log-buffer-2](http://catkang.github.io/assets/img/innodb_redo/redo-next-write-to-log-buffer-2.png)

因此提升当前的*buf_ready_for_write_lsn，*并将*log.recent_written*的tail位置向前滑动，之后的位置清零，供之后循环复用：

![redo-next-write-to-log-buffer-3](http://catkang.github.io/assets/img/innodb_redo/redo-next-write-to-log-buffer-3.png)紧接*log_writer*将连续的内容刷盘并提升*write_lsn*。

#### **刷盘**

*log_writer*提升*write_lsn*之后会通知*log_flusher*线程，*log_flusher*线程会调用*fsync*将REDO刷盘，至此完成了REDO完整的写入过程。

#### **唤醒用户线程**

为了保证数据正确，只有REDO写完后事务才可以*commit*，因此在REDO写入的过程中，大量的用户线程会*block*等待，直到自己的最后一条日志结束写入.默认情况下*innodb_flush_log_at_trx_commit = 1*，需要等REDO完成刷盘，这也是最安全的方式。当然，也可以通过设置*innodb_flush_log_at_trx_commit = 2*，这样，只要REDO写入Page Cache就认为完成了写入，极端情况下，掉电可能导致数据丢失。

大量的用户线程调用*log_write_up_to*等待在自己的*lsn*位置，为了避免大量无效的唤醒，InnoDB将阻塞的条件变量拆分为多个，*log_write_up_to*根据自己需要等待的*lsn*所在的block取模对应到不同的条件变量上去。同时，为了避免大量的唤醒工作影响*log_writer*或*log_flusher*线程，InnoDB中引入了两个专门负责唤醒用户的线程：*log_wirte_notifier*和*log_flush_notifier*，当超过一个条件变量需要被唤醒时，*log_writer*和*log_flusher*会通知这两个线程完成唤醒工作。下图是整个过程的示意图：

![innodb_notify](http://catkang.github.io/assets/img/innodb_redo/innodb_notify.png)

如图所示,当log_writer写完时会通知log_writer_notifier和log_fluser当flush完成时,会通知log_flush_notifier.这两条线程分别对应为2和为1的情况.

多个线程通过一些内部数据结构的辅助，完成了高效的从REDO产生，到REDO写盘，再到唤醒用户线程的流程，下面是整个这个过程的时序图：

![log_sequence](http://catkang.github.io/assets/img/innodb_redo/log_sequence.png)

## 6. 如何安全地清除REDO

重启恢复的时候，只需要从最新的Checkpoint开始回放后边的REDO，因此Checkpoint之前的REDO就可以删除或被复用.

为了保证REDO产生的所有内存脏页都已经刷盘.最直接的，可以从Buffer Pool中获得当前所有脏页对应的最小REDO LSN：*lwm_lsn*。

但光有这个还不够，因为有一部分min-transaction的REDO对应的Page还没有来的及加入到Buffer Pool的脏页中去，如果checkpoint打到这些REDO的后边，一旦这时发生故障恢复，这部分数据将丢失，因此还需要知道当前已经加入到Buffer Pool的REDO lsn位置：*dpa_lsn*。取二者的较小值作为最终checkpoint的位置.

核心逻辑如下:

```c++
/* LWM lsn for unflushed dirty pages in Buffer Pool */
lsn_t lwm_lsn = buf_pool_get_oldest_modification_lwm();

/* Note lsn up to which all dirty pages have already been added into Buffer Pool */
const lsn_t dpa_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);

lsn_t checkpoint_lsn = std::min(lwm_lsn, dpa_lsn);
```

由于并发写脏页这种乱序的存在，lwm_lsn的值并不能简单的获取当前Buffer Pool中的最老的脏页的LSN，保守起见，还需要减掉一个*recent_closed*的容量大小，也就是最大的乱序范围，简化后的代码如下：

```c++
/* LWM lsn for unflushed dirty pages in Buffer Pool */
const lsn_t lsn = buf_pool_get_oldest_modification_approx();
const lsn_t lag = log.recent_closed.capacity();
lsn_t lwm_lsn = lsn - lag;

/* Note lsn up to which all dirty pages have already been added into Buffer Pool */
const lsn_t dpa_lsn = log_buffer_dirty_pages_added_up_to_lsn(log);

lsn_t checkpoint_lsn = std::min(lwm_lsn, dpa_lsn);
```

ib_logfile0文件的前三个Block有两个被预留作为Checkpoint Block，这两个Block会在打Checkpiont的时候交替使用，这样来避免写Checkpoint过程中的崩溃导致没有可用的Checkpoint。Checkpoint Block中的内容如下：

![log_checkpoint](http://catkang.github.io/assets/img/innodb_redo/log_checkpoint.png)

首先8个字节的Checkpoint Number，通过比较这个值可以判断哪个是最新的Checkpiont记录，之后8字节的Checkpoint LSN为打Checkpoint的REDO位置，恢复时会从这个位置开始重放后边的REDO。之后8个字节的Checkpoint Offset，将Checkpoint LSN与文件空间的偏移对应起来。最后8字节是前面提到的Log Buffer的长度，这个值目前在恢复过程并没有使用。











参考:[庖丁解InnoDB之REDO LOG](https://catkang.github.io/2020/02/27/mysql-redo.html)