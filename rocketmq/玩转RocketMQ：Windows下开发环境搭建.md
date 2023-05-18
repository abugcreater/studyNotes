# 玩转RocketMQ：Windows下开发环境搭建

> 考虑到大部分Java开发者还是习惯于在windows环境下开发，因此本篇讲解如何在windows环境下搭建一个单机开发环境。

# 一、准备工作

运行当前最新版本的RockitMQ（v4.4.0），必须先安装64bit的JDK1.8或以上版本。

从RockitMQ官网 [`http://rocketmq.apache.org/release_notes/`](https://links.jianshu.com/go?to=http%3A%2F%2Frocketmq.apache.org%2Frelease_notes%2F) 下载最新的release包。

解压到本地目录。

![img](https://upload-images.jianshu.io/upload_images/7632473-50777ccd7cd32a9a.png?imageMogr2/auto-orient/strip|imageView2/2/w/774/format/webp)

目录结构

上图是`rocketmq-all-4.4.0-bin-release.zip`包解压后的目录结构。`bin`目录下存放可运行的脚本。

# 二、RocketMQ基本结构

在动手开发之前，我们需要了解一下RocketMQ的基本结构。

![img](https://upload-images.jianshu.io/upload_images/7632473-618eb28ea53ad892.png?imageMogr2/auto-orient/strip|imageView2/2/w/975/format/webp)

RocketMQ基本结构

如上图所示，一个正常工作的RocketMQ包括四个部分。

- **NameServer** ：基于高可用设计产生的，用于服务发现和路由。正式应用时通常采用集群部署。
- **Broker**：实现队列机制，负责消息存储和转发。正式应用时也采用集群部署。
- **Producer**：消息生产者，生成消息并发送到RocketMQ中，生产者通常是我们自己实现的应用程序。
- **Consumer**：消息消费者，从RocketMQ中接收消息并进行业务处理。这部分也通常是我们自己实现的。

> 这里并不打算深入讲解RocketMQ的架构和特性，因为我觉得，针对于初学者，太早地深入知识细节，会让人感到迷惘，学习起来吃力。为了避免浪费时间，以上的知识己经能满足本文后面要实践的内容。随着实践我们会慢慢加深对RocketMQ的理解，再回过头来一点点深入了解。

> 以上的知识都来源于RocketMQ官网的参考文档，需要更多细节的同学， 可以打开
> [`http://rocketmq.apache.org/docs/quick-start/`](https://links.jianshu.com/go?to=http%3A%2F%2Frocketmq.apache.org%2Fdocs%2Fquick-start%2F) 自行阅读。

# 三、Windows环境下启动最小应用

从上面的图可以了解到，RocketMQ自身分为 **NameServer** 和 **Broker** 两个部分，因此，用作本机开发调试用的最小应用，应该分别启动一个NameServer和一个Broker节点。

RocketMQ默认提供了 windows环境 和 linux环境 下的启动脚本。脚本位于`bin`目录下，windows的脚本以`.cmd`为文件名后缀，linux环境的脚本以`.sh`为文件名后缀。

> 本篇操作细节部分只专注于讲windows环境下的脚本，linux环境下的脚本请直接参考官网原文。

不过，通常情况下，windows下的脚本双击启动时，都是窗口一闪而过，启动失败。下面的内容就帮大家解决这些问题。

### 第一步，配置 *JAVA_HOME* 和 *ROCKETMQ_HOME* 环境变量

*JAVA_HOME* 的配置已经是老生常谈，这里不再赘述，不懂的话请自行百度。

*ROCKETMQ_HOME* 应指向解压后的`Readme.md`文件所在目录。

> 如上面的第一张图，我的 *ROCKETMQ_HOME* 应配置为`D:\programs\rocketmq\rocketmq-all-4.4.0-bin-release`

### 第二步，启动 *NameServer*

NameServer的启动脚本是`bin`目录下的`mqnamesrv.cmd`。

上文讲过，即使配置好了ROCKETMQ_HOME环境变量，`mqnamesrv.cmd`的启动通常也以失败告终。

阅读`mqnamesrv.cmd`脚本，发现其实际上是调用了`runserver.cmd`脚本来实现启动的动作。

而在`runserver.cmd`脚本，java的默认启动参数中，启动时堆内存的大小为2g，老旧一点的机器上根本没有这么多空闲内存。

因此，用编辑器修改一下`runserver.cmd`脚本。将原来的内存参数注释掉（cmd脚本使用rem关键字），修改为：



```cmd
rem set "JAVA_OPT=%JAVA_OPT% -server -Xms2g -Xmx2g -Xmn1g -XX:MetaspaceSize=128m -XX:MaxMetaspaceSize=320m"
set "JAVA_OPT=%JAVA_OPT% -server -Xms256m -Xmx512m"
```

直接双击`mqnamesrv.cmd`脚本启动NameServer。

![img](https://upload-images.jianshu.io/upload_images/7632473-2a61704654b12394.png?imageMogr2/auto-orient/strip|imageView2/2/w/660/format/webp)

NameServer启动显示

看到 `The Name Server boot success` 字样，表示NameServer己启动成功。

> windows环境下，可以在目录`%USERPROFILE%\logs\rocketmqlogs`下找到NameServer的启动日志。文件名为`namesrv.log`。

# 第三步，启动 *Broker*

Broker的启动脚本是`mqbroker.cmd`。

与`mqnamesrv.cmd`脚本类似，`mqbroker.cmd`是调用`runbroker.cmd`脚本启动Broker的。

同样的，优化一下`runbroker.cmd`的启动内存



```cmd
rem set "JAVA_OPT=%JAVA_OPT% -server -Xms2g -Xmx2g -Xmn1g"
set "JAVA_OPT=%JAVA_OPT% -server -Xms256m -Xmx512m"
```

此外，Broker脚本启动之前要指定 **NameServer的地址**。

> NameServer默认启动端口是9876，这点可以从NameServer的启动日志中找到记录。

因此，还需要修改`mqbroker.cmd`脚本，增加NameServer的地址。



```cmd
rem 添加此行，指定NameServer的地址
set "NAMESRV_ADDR=localhost:9876"

rem 在此行之前添加NameServer的地址
call "%ROCKETMQ_HOME%\bin\runbroker.cmd" org.apache.rocketmq.broker.BrokerStartup %*
```

双击mqbroker.cmd脚本启动Broker。

![img](https://upload-images.jianshu.io/upload_images/7632473-f760ca488a4d6e93.png?imageMogr2/auto-orient/strip|imageView2/2/w/679/format/webp)

Broker启动成功

看到 `The broker ... boot success` 字样，表示Broker己启动成功。

> 与NameServer类似，可以在目录`%USERPROFILE%\logs\rocketmqlogs`下找到Broker的启动日志。文件名为`broker.log`。

# 四、验证RocketMQ功能

RocketMQ自带了恬送与接收消息的脚本`tools.cmd`，用来验证RocketMQ的功能是否正常。

tool.cmd脚本需要带参数执行，无法用简单的双击方式启动。因此，我们打开一个cmd窗口，并跳转到bin目录下。

![img](https://upload-images.jianshu.io/upload_images/7632473-96eba7adf3701d7a.png?imageMogr2/auto-orient/strip|imageView2/2/w/675/format/webp)

打开cmd窗口并跳转到bin目录下

### 启动消费者

与`mqbroker.cmd`脚本类似，启动`tool.cmd`命令之前我们要指定NameServer地址。

这里我们采用命令方式指定，并启动消费者。依次执行如下命令：



```cmd
set NAMESRV_ADDR=localhost:9876
tools.cmd org.apache.rocketmq.example.quickstart.Consumer
```

![img](https://upload-images.jianshu.io/upload_images/7632473-64f03c592ebddabb.png?imageMogr2/auto-orient/strip|imageView2/2/w/669/format/webp)

启动消费者成功

### 启动生产者

再打开一个cmd窗口，依次执行如下命令：



```cmd
set NAMESRV_ADDR=localhost:9876
tools.cmd org.apache.rocketmq.example.quickstart.Producer
```

![img](https://upload-images.jianshu.io/upload_images/7632473-413f4ec863ea3cf9.png?imageMogr2/auto-orient/strip|imageView2/2/w/661/format/webp)

生产者启动命令

启动成功后，生产者会发送1000个消息，然后自动退出。

![img](https://upload-images.jianshu.io/upload_images/7632473-581d29f7eb055cb9.png?imageMogr2/auto-orient/strip|imageView2/2/w/640/format/webp)

生产者发送消息并退出

此时，在消费者界面按下`Ctrl + C`，就会收到刚刚生产者发出的消息。

![img](https://upload-images.jianshu.io/upload_images/7632473-56e329cea742e9f7.png?imageMogr2/auto-orient/strip|imageView2/2/w/640/format/webp)

消费者接收消息

### 至此，RocketMQ最小应用己经可以正常工作，能满足我们开发环境下调试代码的需求。

# 附一、RocketMQ管理工具

RocketMQ自带`mqadmin.cmd`脚本作为管理工具。它可以用来查询、管理、定位问题。这里，我们用它来查看我们刚刚启动的最小应用。

> mqadmin具体参数可参考[`http://rocketmq.apache.org/docs/cli-admin-tool/`](https://links.jianshu.com/go?to=http%3A%2F%2Frocketmq.apache.org%2Fdocs%2Fcli-admin-tool%2F)

在cmd中，执行`mqadmin.cmd clusterList`和`mqadmin.cmd clusterList -m`

![img](https://upload-images.jianshu.io/upload_images/7632473-46e86389d61a38c0.png?imageMogr2/auto-orient/strip|imageView2/2/w/663/format/webp)

mqadmin clusterList

可看到当前只有一个Broker节点，入队与出队的消息各为2000（执行了两次生产者测试）。

# 附二、cmd脚本排障

某些环境下，上述的cmd脚本可能遇到不可预知的错误。

这时，可以将cmd脚本中的第一行`@echo off`修改为`@echo on`，重新执行脚本，此时每一句命令的内容都会显示在控制台（类似于linux shell 的 -x 参数），这种方式可以帮我们迅速定位脚本问题。