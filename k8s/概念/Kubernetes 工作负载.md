# Kubernetes 工作负载

> 工作负载(workload)是在Kubwenetes上运行的应用程序.

在 Kubernetes 中,你可以通过一组Pod运行他.在 Kubernetes 中，`Pod` 代表的是集群上处于运行状态的一组 [容器](https://kubernetes.io/zh-cn/docs/concepts/overview/what-is-kubernetes/#why-containers) 的集合。

在 Kubernetes 中,通过负载资源(***workload resources***)替用户管理一组Pod.这些负载资源通过配置 [控制器](https://kubernetes.io/zh-cn/docs/concepts/architecture/controller/) 来确保正确类型的、处于运行状态的 Pod 个数是正确的，与用户所指定的状态相一致。

Kubernetes 提供若干种**内置的工作负载资源**：

- [`Deployment`](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/) 和 [`ReplicaSet`](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/replicaset/) （替换原来的资源 [ReplicationController](https://kubernetes.io/zh-cn/docs/reference/glossary/?all=true#term-replication-controller)）。`Deployment` 很适合用来管理你的集群上的无状态应用，`Deployment` 中的所有 `Pod` 都是相互等价的，并且在需要的时候被替换。
- [StatefulSet](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/statefulset/) 让你能够运行一个或者多个以某种方式跟踪应用状态的 Pod。`StatefulSet` 中各个 `Pod` 内运行的代码可以将数据复制到同一 `StatefulSet` 中的其它 `Pod` 中以提高整体的服务可靠性。
- [DaemonSet](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/daemonset/) 定义提供节点本地支撑设施的 `Pod`。每次你向集群中添加一个新节点时，如果该节点与某 `DaemonSet` 的规约匹配，则控制平面会为该 `DaemonSet` 调度一个 `Pod` 到该新节点上运行。
- [Job](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/job/) 和 [CronJob](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/cron-jobs/)。 定义一些一直运行到结束并停止的任务。`Job` 用来执行一次性任务，而 `CronJob` 用来执行的根据时间规划反复运行的任务。

## 2. Pod

> **Pod**是Kubernetes 中创建和管理的,最小的可部署的计算单元.

Pod中有一组(一个或单个容器),这些容器共享存储,网络,以及容器声明.

除了应用容器，Pod 还可以包含在 Pod 启动期间运行的 [Init 容器](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/init-containers/)。 你也可以在集群支持[临时性容器](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/ephemeral-containers/)的情况下， 为调试的目的注入临时性容器。

**定义**

**Pod**的共享上下文包括一组Linux名字空间,控制组（cgroup）和可能一些其他的隔离方面， 即用来隔离[容器](https://kubernetes.io/zh-cn/docs/concepts/overview/what-is-kubernetes/#why-containers)的技术.在Pod上下文中,每个独立的应用可能会进一步实施隔离.

Pod 类似于共享名字空间并共享文件系统卷的一组容器。

### 2.1 Pod基础知识



































参考:[工作负债](https://kubernetes.io/zh-cn/docs/concepts/workloads/)