# Kubernetes 入门


## K8S概览

### 1.1 K8S是什么

K8S全称**[Kubernetes](https://kubernetes.io/zh/docs/concepts/overview/what-is-kubernetes/) **,官方定义如下:

> Kubernetes is an open source system for managing [containerized applications](https://kubernetes.io/docs/concepts/overview/what-is-kubernetes/) across multiple hosts. It provides basic mechanisms for deployment, maintenance, and scaling of applications.
>
> 用于自动部署、扩展和管理“容器化（containerized）应用程序”的开源系统。

**K8S 是 负责自动化运维管理多个容器的集群**

### 1.2 为什么是K8S

**K8S能自动化运维管理容器化程序**

当服务需要部署,更新,卸载和扩容,缩容时,K8S能自动介入,而不需要人工介入

### **1.3 K8S 怎么做？**

K8S的宏观架构如下图,图片来自文章[Components of Kubernetes Architecture](https://medium.com/@kumargaurav1247/components-of-kubernetes-architecture-6feea4d5c712)：

![图片](https://mmbiz.qpic.cn/mmbiz_png/j3gficicyOvauLok8egCcGJbUzUQjEd0XwwXk0rxy9CzHyow9mHPQiaNm0VB6R8R1Amz9q18cQt9KcDicVNQyBjiaVA/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

K8S属于**主从设备模型(Master-Slave架构)**,由Master节点负责核心的调度,管理和运维,Slave节点执行用户程序.主节点称为**Master/Head Node**,从节点称为**Worker Node**或**Node**

主从节点是分别安装了K8S的Master和Woker组件的实体服务器,**所有主/从节点共同组成了K8S集群**.一个集群可能保存多个主节点和从节点



**Master Node**包含以下四种组件

- **API Server:K8S 的请求入口服务**.负责接收所有请求,然后根据请求通知其他组件
- **Scheduler:K8S所有Worker Node的调度器**.Scheduler会选择最合适的Worker Node来部署
- **Controller Manager:K8S所有Worker Node的监控器**.在文章[Components of Kubernetes Architecture](https://medium.com/@kumargaurav1247/components-of-kubernetes-architecture-6feea4d5c712)中提到的有 Node Controller、Service Controller、Volume Controller 等.Controller复杂监控和调整Worker Node上部署的服务状态
- **etcd:K8S 的存储服务**.存储了K8S配置和用户信息,仅API Server才具备读写权限（见[Kubernetes Works Like an Operating System](https://thenewstack.io/how-does-kubernetes-work/)）

**Worker Node**包含以下五种组件

- **Kubelet:Worker Node 的监视器，以及与 Master Node 的通讯器**.它定期会向Woker Node汇报运行的服务状态,并接收来自Master Node调整
- **Kube-Proxy:K8S 的网络代理**.负责Node在K8S的网络通讯,以及对外部网络流量的负载均衡
- **Container Runtime:Worker Node 的运行环境**
- **Logging Layer:K8S 的监控状态收集器**
- **Add-Ons:K8S 管理运维 Worker Node 的插件组件**

综上所述:**K8S 的 Master Node 具备：请求入口管理（API Server），Worker Node 调度（Scheduler），监控和自动调节（Controller Manager），以及存储功能（etcd）；而 K8S 的 Worker Node 具备：状态和监控收集（Kubelet），网络和负载均衡（Kube-Proxy）、保障容器化运行环境（Container Runtime）、以及定制化功能（Add-Ons）**

## **K8S 重要概念**

### **2.1 Pod 实例**

**Pod**是可以在kubernetes中创建和管理的,最小的可部署计算单元.

**最小 xx 单位“要么就是事物的衡量标准单位，要么就是资源的闭包、集合**

简单来说,**Pod可以被理解成一群可以共享网络,存储和计算资源的容器化服务的集合**

如下图所示,同一个Pod中的container可以通过localhost互相访问,并且可以挂载pod内所有的数据卷;但是不同的pod间的容器不能用localhost访问,也不能挂载其他pod的数据卷

![图片](https://mmbiz.qpic.cn/mmbiz_png/j3gficicyOvauLok8egCcGJbUzUQjEd0Xw8tMnpYpyNAQn3o2FicHmuayH2do04NuzfQlvsrP66EyjE8O8omthHBA/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)



#### Pod配置解析

K8S中的所有对象都可以通过yaml表示,关于pod的yaml如下所示:

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: memory-demo
  namespace: mem-example
spec:
  containers:
  - name: memory-demo-ctr
    image: polinux/stress
    resources:
      limits:
        memory: "200Mi"
      requests:
        memory: "100Mi"
    command: ["stress"]
    args: ["--vm", "1", "--vm-bytes", "150M", "--vm-hang", "1"]
    volumeMounts:
    - name: redis-storage
      mountPath: /data/redis
  volumes:
  - name: redis-storage
    emptyDir: {}
```

配置文件详解:

- `apiVersion`:记录K8S的API Server,目前看到的都是V1
- `kind`:记录yaml的对象类型,如果是Pod配置文件则值就是`Pod`
- `metadata`:记录了Pod的自身元素,包括名字和命名空间
- `spec`:记录了Pod内部的资源信息(**重要**):
  - `containers`:记录了内部容器信息,容器信息包括了`name`容器名，`image`容器的镜像地址，`resources`容器需要的 CPU、内存、GPU 等资源，`command`容器的入口命令，`args`容器的入口参数，`volumeMounts`容器要挂载的 Pod 数据卷等。可以看到，**上述这些信息都是启动容器的必要和必需的信息**
  - `volumes`:记录了Pod内的数据卷信息

### **2.2 Volume 数据卷**

`Volume`就是**需要手动mount(挂载)的磁盘,数据卷volume的Pod内部的磁盘资源**

>  简单来说volume可以看做外接u盘

配置中提到的`volumeMounts`和`volume`是什么关系呢?

`volume`是K8S的对象,对应一个实体的数据卷;而`volumeMounts`只是container的挂载点,对应container的其中一个参数.**但是`volumeMounts`依赖于`volume`**,只要当有`volume`资源时,该Pod内部的container才可能有`volumeMounts`

### **2.3 Container 容器**

**一个 Pod 内可以有多个容器 container**。在Pod中,容器也有很多种类型:

- **标准容器 Application Container**。
- **初始化容器 Init Container**。
- **边车容器 Sidecar Container**。
- **临时容器 Ephemeral Container**。

一般来说我们部署的大多是**标准容器 (Application Container)**

### **2.4 Deployment 和 ReplicaSet（简称 RS）**

Deployment 的官方解释如下:

>  一个 *Deployment* 控制器为 [Pods](https://kubernetes.io/docs/concepts/workloads/pods/pod-overview/) 和 [ReplicaSets](https://kubernetes.io/zh/docs/concepts/workloads/controllers/replicaset/) 提供声明式的更新能力。你负责描述 Deployment 中的 *目标状态*，而 Deployment 控制器以受控速率更改实际状态， 使其变为期望状态。你可以定义 Deployment 以创建新的 ReplicaSet，或删除现有 Deployment，并通过新的 Deployment 收养其资源。

翻译成白话就是:**Deployment 的作用是管理和控制Pod和ReplicaSet,管控他们运行在用户期望的状态中.**就好比Deployment 是负责人,能监督Pod的干活状态,当发现干活的Pod数量少于用户期望时,在重新拉一个过来顶替.

ReplicaSets 的官方定义如下:

> ReplicaSet 的目的是维护一组在任何时候都处于运行状态的 Pod 副本的稳定集合。因此，它通常用来保证给定数量的、完全相同的 Pod 的可用性。

形象的解释就是**ReplicaSet 就是总包工头手下的小包工头**

![图片](https://mmbiz.qpic.cn/mmbiz_png/j3gficicyOvauLok8egCcGJbUzUQjEd0Xw4ickITAbMdSSn2JYyhlT8n7rf3TOMZ3Th1vdniab4p4nwTGJogVytPbA/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

可以看到Deloyment和Replica Set职责相同,都是为了管控Pod,那么为什么要分成两级呢



从 K8S 使用者角度来看，用户会直接操作 Deployment 部署服务，而当 Deployment 被部署的时候，K8S 会自动生成要求的 ReplicaSet 和 Pod。在[K8S 官方文档](https://www.kubernetes.org.cn/replicasets)中也指出用户只需要关心 Deployment 而不操心 ReplicaSet：

### **2.5 Service 和 Ingress**

[官方文档](https://kubernetes.io/zh/docs/concepts/services-networking/service/)中 Service 的定义:

> 将运行在一组 [Pods](https://kubernetes.io/docs/concepts/workloads/pods/pod-overview/) 上的应用程序公开为网络服务的抽象方法。
>
> 使用 Kubernetes，您无需修改应用程序即可使用不熟悉的服务发现机制。Kubernetes 为 Pods 提供自己的 IP 地址，并为一组 Pod 提供相同的 DNS 名， 并且可以在它们之间进行负载均衡。

即K8S中的服务(Service)并不是我们常说的"服务"含义,而更像是网关层,是若干个Pod的流量入口和均衡器.

> Kubernetes [Pod](https://kubernetes.io/docs/concepts/workloads/pods/pod-overview/) 是有生命周期的。它们可以被创建，而且销毁之后不会再启动。如果您使用 [Deployment](https://kubernetes.io/zh/docs/concepts/workloads/controllers/deployment/) 来运行您的应用程序，则它可以动态创建和销毁 Pod。
>
> 每个 Pod 都有自己的 IP 地址，但是在 Deployment 中，在同一时刻运行的 Pod 集合可能与稍后运行该应用程序的 Pod 集合不同

那么此时我们需要service帮助我们将请求转移到相关的Pod上.

>  推荐阅读[k8s 外网如何访问业务应用](https://www.jianshu.com/p/50b930fa7ca3)对于 Service 的介绍

**Service 是 K8S 服务的核心，屏蔽了服务细节，统一对外暴露服务接口，真正做到了“微服务”**.用户只需要关注service入口,而不需要操心请求到哪一个Pod.

Service的又是如下:

- **外部用户不需要感知Pod上服务的意外崩溃,K8S重新拉起Pod而造成的IP变更.**
- **外部用户也不需要感知因升级、变更服务带来的 Pod 替换而造成的 IP 变化**
- **Service还可以做流量负载均衡**

那么集群外部怎么访问集群内部呢?

这个时候就需要Ingress.

 **Ingress是对集群中服务的外部访问进行管理的 API 对象，典型的访问方式是 HTTP。**

**Ingress 可以提供负载均衡、SSL 终结和基于名称的虚拟托管**

简单来说Ingress是整个K8S集群的接入层,实现集群内外通讯

![图片](https://mmbiz.qpic.cn/mmbiz_png/j3gficicyOvauLok8egCcGJbUzUQjEd0XwaaRkshAhKGmeJuRwZNbz7aWQh5f9ftA33iaBarA62rfKQ34VKNpuxzg/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)



### **2.6 namespace 命名空间**

namespace 这个概念是为了服务整个K8S集群的.

>  Kubernetes 支持多个虚拟集群，它们底层依赖于同一个物理集群。这些虚拟集群被称为名字空间。

简单来说:**namespace是为了把一个K8S集群划分为若干个资源不可共享的虚拟集群而诞生的**.也就是说**可以通过在 K8S 集群内创建 namespace 来分隔资源和对象**















参考:[Kubernetes 入门&进阶实战](https://mp.weixin.qq.com/s/mUF0AEncu3T2yDqKyt-0Ow)

[Components of Kubernetes Architecture](https://medium.com/@kumargaurav1247/components-of-kubernetes-architecture-6feea4d5c712)

[Kubernetes Works Like an Operating System](https://thenewstack.io/how-does-kubernetes-work/)

[HOW DO APPLICATIONS RUN ON KUBERNETES](https://thenewstack.io/how-do-applications-run-on-kubernetes/)

[中文文档](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/)

[细数k8s支持的4种类型的container](https://segmentfault.com/a/1190000022814515)