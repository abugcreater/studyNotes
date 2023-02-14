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

#### 2.1.1 使用Pod

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: nginx
spec:
  containers:
  - name: nginx
    image: nginx:1.14.2
    ports:
    - containerPort: 80
```

然后使用命令

```
kubectl apply -f https://k8s.io/examples/pods/simple-pod.yaml
```

就能创建包含一个我运行镜像 `nginx:1.14.2` 的容器组成。Pod 通常不是直接创建的，而是使用工作负载资源创建的。 

**用于管理Pod的工作负载资源**

我们不需要直接创建Pod,只需要使用 [Deployment](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/) 或 [Job](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/job/) 这类工作负载资源来创建 Pod。如果Pod需要跟踪状态,可以考虑 [StatefulSet](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/statefulset/).

Kubernetes 集群中的 Pod 主要有两种用法：

- **运行单个容器的 Pod**。这种比较常见;可以将Pod看作单个容器的包装器,并且Kubernetes 直接管理Pod,而不是容器
- **运行多个协同工作的容器的 Pod**。**Pod**包含了多个紧密耦合,共享资源且始终在一起运行的容器.这些容器可能形成**单个内聚的服务单元**—— 一个容器将文件从共享卷提供给公众， 而另一个单独的 “边车”（sidecar）容器则刷新或更新这些文件。 Pod 将这些容器和存储资源打包为一个可管理的实体。

如果您想要水平扩展您的应用程序（运行多个实例），您运行多个 Pod 容器组，每一个代表应用程序的一个实例。Kubernetes 中，称其为 replication（复制副本）。Kubernetes 中 Controller（控制器）负责为应用程序创建和管理这些复制的副本

**Pod 怎样管理多个容器**

Pod 的设计目的是用来支持多个互相协同的容器，使得他们形成一个有意义的服务单元。Pod 中的容器被自动安排到集群中的同一物理机或虚拟机上，并可以一起进行调度。容器之间可以**共享资源和依赖**、**彼此通信**、**协调何时以及何种方式终止自身**。

>  将多个容器运行于同一个容器组中是一种相对高级复杂的使用方法。只有在您的容器相互之间紧密耦合是，您才应该使用这种方式。例如：您可能有一个容器是 web server，用来将共享数据卷中的文件作为网站发布出去，同时您有另一个 "sidecar" 容器从远程抓取并更新这些文件。如下图所示：<img src="https://kuboard.cn/assets/img/pod.f3ec9ad1.svg" alt="Kubernetes教程：Pod中的多个容器" style="zoom:33%;" />

某些 Pod 除了使用 app container （工作容器）以外，还会使用 init container （初始化容器），初始化容器运行并结束后，工作容器才开始启动。

Pod 天生地为其成员容器提供了两种共享资源：[网络](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/#pod-networking)和[存储](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/#pod-storage)

- 网络 Networking

  每一个 Pod 被分配一个独立的 IP 地址。Pod 中的所有容器共享一个网络名称空间(IP:port)

- 存储 Storage

  Pod 中可以定义一组共享的数据卷。所有容器都能读写这些数据

#### 2.1.2 使用Pod容器组

在 Kubernetes 的设计中 Pod 是一个相对来说存活周期短暂，且随时会丢弃的实体。在 Pod 被创建后,将被调度到集群中的一个节点上运行.Pod 会保持在该节点上运行，直到 **Pod 结束执行、Pod 对象被删除、Pod 因资源不足而被驱逐**或者**节点失效为止**。

**容器组和控制器**

用户应该始终使用控制器来创建 Pod，而不是直接创建 Pod，控制器可以提供如下特性：

- 水平扩展（运行 Pod 的多个副本）
- rollout（版本更新）
- self-healing（故障恢复）

在 Kubernetes 中，广泛使用的控制器有：

- Deployment
- StatefulSet
- DaemonSet

**Pod 模板**

Pod Template 是关于 Pod 的定义，但是被包含在其他的 Kubernetes 对象中（例如 Deployment、StatefulSet、DaemonSet 等控制器）。控制器通过 `Pod Template` 信息来创建 Pod。 `PodTemplate` 是你用来运行应用时指定的负载资源的目标状态的一部分。

*Pod 模板 示例*

```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: hello
spec:
  template:
    # 这里是 Pod 模板
    spec:
      containers:
      - name: hello
        image: busybox:1.28
        command: ['sh', '-c', 'echo "Hello, Kubernetes!" && sleep 3600']
      restartPolicy: OnFailure
    # 以上为 Pod 模板
```

如果改变工作负载资源的 Pod 模板，工作负载资源需要使用更新后的模板来创建 Pod， 并使用新创建的 Pod 替换旧的 Pod。

#### 2.1.3 Pod 更新与替换

Kubernetes 并不禁止你直接管理 Pod。对运行中的 Pod 的某些字段执行就地更新操作还是可能的。不过，类似 [`patch`](https://kubernetes.io/docs/reference/generated/kubernetes-api/v1.25/#patch-pod-v1-core) 和 [`replace`](https://kubernetes.io/docs/reference/generated/kubernetes-api/v1.25/#replace-pod-v1-core) 这类更新操作有一些限制.

**资源共享和通信Pod** :使它的成员容器间能够进行数据共享和通信。

**Pod 中的存储**:一个 Pod 可以设置一组共享的存储[卷](https://kubernetes.io/zh-cn/docs/concepts/storage/volumes/)。 Pod 中的所有容器都可以访问该共享卷，从而允许这些容器共享数据。 卷还允许 Pod 中的持久数据保留下来，即使其中的容器需要重新启动。

**Pod 联网**:每个 Pod 都在每个地址族中获得一个唯一的 IP 地址。所有容器共享网络名字空间，包括 IP 地址和网络端口.可以使用localhost通信

#### 2.1.4 静态 Pod

不同于普通的Pod.**静态 Pod（Static Pod）** 直接由特定节点上的 `kubelet` 守护进程管理， 不需要 [API 服务器](https://kubernetes.io/zh-cn/docs/concepts/overview/components/#kube-apiserver)看到它们。`kubelet` **直接监控每个静态 Pod**，并在其失效时重启之。

静态 Pod 通常绑定到某个节点上的 [kubelet](https://kubernetes.io/docs/reference/generated/kubelet)。 其主要用途是运行自托管的控制面。 在自托管场景中，使用 `kubelet` 来管理各个独立的[控制面组件](https://kubernetes.io/zh-cn/docs/concepts/overview/components/#control-plane-components)。

#### 2.1.5 容器探针

**Probe** 是由 kubelet 对容器执行的定期诊断。要执行诊断，kubelet 可以执行三种动作：

- `ExecAction`（借助容器运行时执行）
- `TCPSocketAction`（由 kubelet 直接检测）
- `HTTPGetAction`（由 kubelet 直接检测）

### 2.2 Pod生命周期

>  Pod 遵循预定义的生命周期，起始于 `Pending` [阶段](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-phase)， 如果至少其中有一个主要容器正常启动，则进入 `Running`，之后取决于 Pod 中是否有容器以失败状态结束而进入 `Succeeded` 或者 `Failed` 阶段。

Pod 在其生命周期中只会被[调度](https://kubernetes.io/zh-cn/docs/concepts/scheduling-eviction/)一次。 一旦 Pod 被调度（分派）到某个节点，Pod 会一直在该节点运行，直到 Pod 停止或者被[终止](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-termination)。

如果某物声称其生命期与某 Pod 相同，例如存储[卷](https://kubernetes.io/zh-cn/docs/concepts/storage/volumes/)， 这就意味着该对象在此 Pod （UID 亦相同）存在期间也一直存在。 如果 Pod 因为任何原因被删除，甚至某完全相同的替代 Pod 被创建时， 这个相关的对象（例如这里的卷）也会被删除并重建。

#### 2.2.1 Pod 阶段

Pod 的阶段（Phase）是 Pod 在其生命周期中所处位置的简单宏观概述。 该阶段并不是对容器或 Pod 状态的综合汇总，也不是为了成为完整的状态机。`phase` 可能的值：

| 取值                | 描述                                                         |
| :------------------ | :----------------------------------------------------------- |
| `Pending`（悬决）   | Pod 已被 Kubernetes 系统接受，但有一个或者多个容器尚未创建亦未运行。此阶段包括等待 Pod 被调度的时间和通过网络下载镜像的时间。 |
| `Running`（运行中） | Pod 已经绑定到了某个节点，Pod 中所有的容器都已被创建。至少有一个容器仍在运行，或者正处于启动或重启状态。 |
| `Succeeded`（成功） | Pod 中的所有容器都已成功终止，并且不会再重启。               |
| `Failed`（失败）    | Pod 中的所有容器都已终止，并且至少有一个容器是因为失败终止。也就是说，容器以非 0 状态退出或者被系统终止。 |
| `Unknown`（未知）   | 因为某些原因无法取得 Pod 的状态。这种情况通常是因为与 Pod 所在主机通信失败。 |

> 当一个 Pod 被删除时，执行一些 kubectl 命令会展示这个 Pod 的状态为 `Terminating`（终止）。 这个 `Terminating` 状态并不是 Pod 阶段之一。 

如果某节点死掉或者与集群中其他节点失联，Kubernetes 会实施一种策略，将失去的节点上运行的所有 Pod 的 `phase` 设置为 `Failed`。

#### 2.2.2 容器状态

Kubernetes 会跟踪 Pod 中每个容器的状态，就像它跟踪 Pod 总体上的[阶段](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-phase)一样。器的状态有三种：`Waiting`（等待）、`Running`（运行中）和 `Terminated`（已终止）。

**`Waiting` （等待）**

处于 `Waiting` 状态的容器仍在运行它完成启动所需要的操作：例如， 从某个容器镜像仓库拉取容器镜像，或者向容器应用 [Secret](https://kubernetes.io/zh-cn/docs/concepts/configuration/secret/) 数据等等。

**`Running`（运行中）**

`Running` 状态表明容器正在执行状态并且没有问题发生。 如果配置了 `postStart` 回调，那么该回调已经执行且已完成。 

**`Terminated`（已终止）**

处于 `Terminated` 状态的容器已经开始执行并且或者正常结束或者因为某些原因失败。 

#### 2.2.3 容器重启策略

Pod 的 `spec` 中包含一个 `restartPolicy` 字段，其可能取值包括 Always、OnFailure 和 Never。默认值是 Always。

#### 2.2.4 Pod 状况

Pod 有一个 PodStatus 对象，其中包含一个 [PodConditions](https://kubernetes.io/docs/reference/generated/kubernetes-api/v1.25/#podcondition-v1-core) 数组。Pod 可能通过也可能未通过其中的一些状况测试。 Kubelet 管理以下 PodCondition：

- `PodScheduled`：Pod 已经被调度到某节点；
- `PodHasNetwork`：Pod 沙箱被成功创建并且配置了网络（Alpha 特性，必须被[显式启用](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-has-network)）；
- `ContainersReady`：Pod 中所有容器都已就绪；
- `Initialized`：所有的 [Init 容器](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/init-containers/)都已成功完成；
- `Ready`：Pod 可以为请求提供服务，并且应该被添加到对应服务的负载均衡池中。

#### 2.2.5 容器检查

Probe 是指 kubelet 周期性地检查容器的状况。有三种类型的 Probe(探针)：

- **ExecAction：** 在容器内执行一个指定的命令。如果该命令的退出状态码为 0，则成功
- **TCPSocketAction：** 探测容器的指定 TCP 端口，如果该端口处于 open 状态，则成功
- **HTTPGetAction：** 探测容器指定端口/路径上的 HTTP Get 请求，如果 HTTP 响应状态码在 200 到 400（不包含400）之间，则成功

Probe 有三种可能的结果：

- **Success：** 容器通过检测
- **Failure：** 容器未通过检测
- **Unknown：** 检测执行失败，此时 kubelet 不做任何处理

Kubelet 可以在两种情况下对运行中的容器执行 Probe：

- **就绪检查 readinessProbe：** 确定容器是否已经就绪并接收服务请求。如果就绪检查失败，kubernetes 将该 Pod 的 IP 地址从所有匹配的 Service 的资源池中移除掉。
- **健康检查 livenessProbe：** 确定容器是否正在运行。如果健康检查失败，kubelete 将结束该容器，并根据 restart policy（重启策略）确定是否重启该容器。

### 2.3 Init容器

Init 容器是一种特殊容器，在 [Pod](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/) 内的应用容器启动之前运行。Init 容器可以包括一些应用镜像中不存在的实用工具和安装脚本。

#### 2.3.1 初始化容器介绍

每个 [Pod](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/) 中可以包含多个容器， 应用运行在这些容器里面，同时 Pod 也可以有一个或多个先于应用容器启动的 Init 容器。

初始化容器与工作容器完全相同，除了如下几点：

- 初始化容器总是运行并自动结束
- kubelet 按顺序执行 Pod 中的初始化容器，前一个初始化容器成功结束后，下一个初始化容器才开始运行。所有的初始化容器成功执行后，才开始启动工作容器
- 如果 Pod 的任意一个初始化容器执行失败，kubernetes 将反复重启该 Pod，直到初始化容器全部成功（除非 Pod 的 restartPolicy 被设定为 Never）
- 初始化容器的 Resource request / limits 处理不同，请参考 Resources
- 初始化容器不支持 就绪检查 readiness probe，因为初始化容器必须在 Pod ready 之前运行并结束

#### 2.3.2 使用初始化容器

初始化容器可以指定不同于工作容器的镜像，这使得初始化容器相较于直接在工作容器中编写启动相关的代码更有优势：

- 初始化容器可以包含工作容器中没有的工具代码或者自定义代码。例如：您无需仅仅为了少量的 setup 工作（使用 sed, awk, python 或 dig 进行环境设定）而重新从一个基础镜像制作另外一个镜像
- 初始化容器可以更安全地执行某些使工作容器变得不安全的代码
- 应用程序的镜像构建者和部署者可以各自独立地工作，而无需一起构建一个镜像
- 初始化容器相较于工作容器，可以以另外一个视角处理文件系统。例如，他们可以拥有访问 Secrets 的权限，而工作容器却不一定被授予该权限
- 初始化容器在任何工作容器启动之前结束运行，这个特性使得我们可以阻止或者延迟工作容器的启动，直到某些前提条件得到满足。一旦前提条件满足，所有的工作容器将同时并行启动

#### 2.3.3 初始化容器的行为

- Pod 的启动时，首先初始化网络和数据卷，然后按顺序执行每一个初始化容器。任何一个初始化容器都必须成功退出，才能开始下一个初始化容器。如果某一个容器启动失败或者执行失败，kubelet 将根据 Pod 的 restartPolicy 决定是否重新启动 Pod。
- 只有所有的初始化容器全都执行成功，Pod 才能进入 ready 状态。初始化容器的端口是不能够通过 kubernetes Service 访问的。Pod 在初始化过程中处于 Pending 状态，并且同时有一个 type 为 `initializing` status 为 `True` 的 [Condition](https://kuboard.cn/learning/k8s-intermediate/workload/pod-lifecycle.html#pod-conditions)
- 如果 Pod 重启，所有的初始化容器也将被重新执行。
- 您可以重启、重试、重新执行初始化容器，因此初始化容器中的代码必须是 **幂等** 的。具体来说，向 emptyDir 写入文件内容的代码应该考虑到该文件已经存在的情况。请参考 [幂等](https://kuboard.cn/glossary/idempotent.html) 获得更多信息
- 您可以组合使用就绪检查和 activeDeadlineSeconds **Kuboard 暂不支持**，以防止初始化容器始终失败。
- Pod 中不能包含两个同名的容器（初始化容器和工作容器也不能同名）

#### 2.3.4 初始化容器资源使用

- 所有初始化容器中最高的 resource request/limit 是最终生效的 request/limit
- 对于 Pod 来说，最终生效的 resource request/limit 是如下几个当中较高的一个：
  - 所有工作容器某一个 resource request/limit 的和
  - 最终生效的初始化容器的 request/limit 的和
- Kubelet 依据最终生效的 request/limit 执行调度，这意味着，在执行初始化容器时，就已经为 Pod 申请了其资源需求

> **Request:** 容器使用的最小资源需求
>
> **Limit:** 容器能使用资源的资源的最大值

#### 2.3.5 Pod 重启的原因

Pod 重启的原因可能有：

- Pod 容器基础设施被重启（例如 docker engine），这种情况不常见，通常只有 node 节点的 root 用户才可以执行此操作
- Pod 中所有容器都已经结束，restartPolicy 是 Always，且初始化容器执行的记录已经被垃圾回收，此时将重启整个 Pod

### 2.4 干扰（Disruptions）

想要构建高可用的应用程序的应用程序管理员，为此，您需要理解有哪些毁坏（disruption）可能发生在Pod上。

#### 2.4.1 自愿干扰和非自愿干扰

除非有人（人或者控制器）销毁Pod，或者出现不可避免的硬件/软件故障，Pod不会凭空消失。我们把这些不可避免的情况称为应用的**非自愿干扰（Involuntary Disruptions）**

我们称其他情况为**自愿干扰（Voluntary Disruptions）**。 包括由应用所有者发起的操作和由集群管理员发起的操作。

#### 2.4.2 处理干扰

我们可以提升硬件方面,或者对应用进行高可用部署来避免**非自愿干扰**

集群管理员或托管提供商可能运行一些可能**导致自愿干扰**的额外服务。例如，节点软 更新可能导致自愿干扰,或者自定义实现的自动伸缩程序

Kubernetes 提供了 **Disruption Budget(干扰预算)** 这一特性，以帮助我们在高频次自愿干扰会发生的情况下，仍然运行高可用的应用程序。

#### 2.4.3 干扰预算

`PodDisruptionBudget`（PDB）将限制在同一时间因自愿干扰导致的多副本应用中发生宕机的 Pod 数量。

集群管理员和托管提供商应该使用遵循 PodDisruptionBudgets 的接口 （通过调用[Eviction API](https://kubernetes.io/zh-cn/docs/tasks/administer-cluster/safely-drain-node/#the-eviction-api)）， 而不是直接删除 Pod 或 Deployment。

PDB 无法防止非自愿干扰； 但它们确实计入预算。

 但是工作负载资源（如 Deployment 和 StatefulSet） 在进行滚动升级时不受 PDB 的限制。 应用更新期间的故障处理方式是在对应的工作负载资源的 `spec` 中配置的。

#### 2.4.4 Pod 干扰状况

 `DisruptionTarget` [状况](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-conditions)， 用来表明该 Pod 因为发生[干扰](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/disruptions/)而被删除.终止原因如下:

- **PreemptionByKubeScheduler**:Pod 将被调度器[抢占](https://kubernetes.io/zh-cn/docs/concepts/scheduling-eviction/pod-priority-preemption/#preemption)， 目的是接受优先级更高的新 Pod
- **DeletionByTaintManager**:由于 Pod 不能容忍 `NoExecute` 污点，Pod 将被 Taint Manager删除
- **EvictionByEvictionAPI**:Pod 已被标记为[通过 Kubernetes API 驱逐](https://kubernetes.io/zh-cn/docs/concepts/scheduling-eviction/api-eviction/)
- **DeletionByPodGC**:绑定到一个不再存在的 Node 上的 Pod 将被 [Pod 垃圾收集](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-garbage-collection)删除。

### 2.5 临时容器

该容器在现有 [Pod](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/) 中临时运行，以便完成用户发起的操作，例如故障排查。

可以在现有 Pod 中运行临时容器来检查现有Pod状态并运行任意命令。

临时容器与其他容器的**不同之处在于**，它们缺少对资源或执行的保证，并且永远不会自动重启， 因此不适用于构建应用程序。

## 3. 工作负载资源

Kubernetes 通过引入 Controller（控制器）的概念来管理 Pod 实例。在 Kubernetes 中，您应该始终通过创建 Controller 来创建 Pod，而不是直接创建 Pod

### 3.1 控制器 - ReplicaSet

Kubernetes 中，ReplicaSet 用来维护一个数量稳定的 Pod 副本集合，可以保证某种定义一样的 Pod 始终有指定数量的副本数在运行。

> **Tip:不建议使用,建议使用Deployment **

#### 3.1.1 ReplicaSet的工作原理

ReplicaSet的定义中，包含：

- `selector`： 用于指定哪些 Pod 属于该 ReplicaSet 的管辖范围
- `replicas`： 副本数，用于指定该 ReplicaSet 应该维持多少个 Pod 副本
- `template`： Pod模板，在 ReplicaSet 使用 Pod 模板的定义创建新的 Pod

ReplicaSet 控制器将通过创建或删除 Pod，以使得当前 Pod 数量达到 `replicas` 指定的期望值。ReplicaSet 创建的 Pod 中，都有一个字段 [metadata.ownerReferences](https://kuboard.cn/learning/k8s-intermediate/workload/gc.html#所有者和从属对象) 用于标识该 Pod 从属于哪一个 ReplicaSet。

### 3.2 控制器 - Deployments

你负责描述 Deployment 中的 **目标状态**，而 Deployment [控制器（Controller）](https://kubernetes.io/zh-cn/docs/concepts/architecture/controller/) 以**受控速率**更改实际状态， 使其变为期望状态。你可以定义 Deployment 以创建新的 ReplicaSet，或删除现有 Deployment， 并通过新的 Deployment 收养其资源。

 Deployment 是最常用的用于部署无状态服务的方式。Deployment 控制器使得您能够以声明的方式更新 Pod（容器组）和 ReplicaSet（副本集）。

**Deployment 状态**

Deployment 的生命周期中会有许多状态。上线新的 ReplicaSet 期间可能处于 [Progressing（进行中）](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/#progressing-deployment)，可能是 [Complete（已完成）](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/#complete-deployment)，也可能是 [Failed（失败）](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/#failed-deployment)以至于无法继续进行

- **进行中的 Deployment: 创建新的ReplicaSet;正在扩容,缩容;新的Pod已经就绪**
- **完成的 Deployment:关联所有副本都已更新完成;所有副本可用;没有未运行旧副本**
- **失败的 Deployment:尝试部署最新的ReplicaSet受挫,比如镜像拉取失败,权限不足等**

### 3.3 StatefulSet

StatefulSet 顾名思义，用于管理 Stateful（有状态）的应用程序。StatefulSet 用来管理某 [Pod](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/) 集合的部署和扩缩， 并为这些 Pod 提供持久存储和持久标识符。

StatefulSet 管理Pod时,给每个Pod维护了一个按顺序增长的ID,每个Pod的ID永久不变.

ID包括**顺序标识**、**稳定的网络标识**和**稳定的存储**

**和其他所有控制器一样**,StatefulSet 也使用相同的模式运作：用户在 StatefulSet 中定义自己期望的结果，StatefulSet 控制器执行需要的操作，以使得该结果被达成。

### 3.3.1 StatefulSet使用场景

- 稳定、唯一的网络标识（dnsname）
- 每个Pod始终对应各自的稳定持久的存储路径（PersistantVolumeClaimTemplate）
- 按顺序地增加副本、减少副本，并在减少副本时执行清理
- 按顺序自动地执行滚动更新

 如果应用程序不需要任何稳定的标识符或有序的部署、删除或扩缩， 则应该使用由一组无状态的副本控制器提供的工作负载来部署应用程序，比如 [Deployment](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/) 或者 [ReplicaSet](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/replicaset/) 可能更适用于你的无状态应用部署需要

> 有状态服务:服务依赖局部的状态数据,通常存在于分布式架构
>
> 无状态服务:服务不依赖自身状态,数据可以维护在内存中,通常存在于单体架构的集群中

#### 3.3.2 限制

-  Pod 的存储要么由 storage class 对应的 [PersistentVolume Provisioner (opens new window)](https://github.com/kubernetes/examples/blob/master/staging/persistent-volume-provisioning/README.md)提供，要么由集群管理员事先创建
- 删除或者扩缩 StatefulSet 并**不会**删除它关联的存储卷。 这样做是为了保证数据安全
- StatefulSet 当前需要[无头服务](https://kubernetes.io/zh-cn/docs/concepts/services-networking/service/#headless-services)来负责 Pod 的网络标识。你需要负责创建此服务。
- 当删除一个 StatefulSet 时，该 StatefulSet 不提供任何终止 Pod 的保证。 为了实现 StatefulSet 中的 Pod 可以有序且体面地终止，可以在删除之前将 StatefulSet 缩容到 0。
- 在默认 [Pod 管理策略](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/statefulset/#pod-management-policies)(`OrderedReady`) 时使用[滚动更新](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/statefulset/#rolling-updates)，可能进入一个错误状态，可能进入需要[人工干预](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/statefulset/#forced-rollback)才能修复的损坏状态

### 3.4 DaemonSet

DaemonSet 控制器确保所有（或一部分）的节点都运行了一个指定的 Pod 副本。删除DaemonSet 时,他创建的所有Pod都被回收了,当节点移除时Pod也会被垃圾回收.

DaemonSet 的**典型使用场景**有：

- 集群的存储守护进程
- 日志收集守护进程
- 监控守护进程

#### 3.4.1  DaemonSet 的替代方案

- **init 脚本:** 直接在节点上启动一个守护进程。
- **裸Pod:**直接创建Pod,并指定在某一个节点上运行
- **静态 Pod:**您可以在 Kubelet 监听的目录下创建一个 Pod 的 yaml 文件，这种形式的 Pod 叫做 
- **Deployment:**DaemonSet 和 Deployment 一样，他们都创建长时间运行的 Pod

### 3.5 Job

Kubernetes中的 Job 对象将创建一个或多个 Pod，并确保指定数量的 Pod 可以成功执行到进程正常结束.Job将跟踪成功Pod数量,数量达到阈值时,Job结束.

删除Job会清除所创建的全部Pod,挂起Job的操作会删除Job的所有活跃Pod,直到Job被再次回复执行.

Job中当第一个Pod失败或者被删除时,Job对象会启动一个新的Pod

#### 3.5.1 编写 Job 规约

**Pod模版**

Job 的 `.spec` 中只有 `.spec.template` 是必需的字段.模版中不需要`apiVersion` 或 `kind` 字段。

Job 中 Pod 的 [`RestartPolicy`](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#restart-policy) 只能设置为 `Never` 或 `OnFailure` 之一。

**Pod 选择算符**

字段 `.spec.selector` 是可选的。在绝大多数场合，你都不需要为其赋值。

**Job 的并行执行**

- 非并行Job:只有一个Pod执行任务
- 具有**确定完成计数**的并行 Job:当有规定数量的Pod完成时视为任务成功
- 带**工作队列**的并行 Job:Pod 之间必须相互之间自行协调并发，或者使用一个外部服务决定每个 Pod 各自执行哪些任务

#### 3.5.2 Job 终止与清理

Job 基于前述的 `spec.backoffLimit` 来决定是否以及如何重试。 一旦**重试次数到达 `.spec.backoffLimit` 所设的上限**，Job 会被标记为失败， 其中运行的 Pod 都会被终止

终止 Job 的另一种方式是**设置一个活跃期限**。 你可以为 Job 的 `.spec.activeDeadlineSeconds` 设置一个秒数值。 该值适用于 Job 的整个生命期，无论 Job 创建了多少个 Pod。 一旦 Job 运行时间达到 `activeDeadlineSeconds` 秒，其所有运行中的 Pod 都会被终止， 并且 Job 的状态更新为 `type: Failed` 及 `reason: DeadlineExceeded`。(优先级较高)

### 3.6 CronJob

*CronJob* 创建基于时隔重复调度的 [Jobs](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/job/)。

一个 CronJob 对象就像 *crontab* (cron table) 文件中的一行。 它用 [Cron](https://en.wikipedia.org/wiki/Cron) 格式进行编写， 并周期性地在给定的调度时间执行 Job。

#### 3.6.1 CronJob 的限制

一个 CronJob 在时间计划中的每次执行时刻，都创建 **大约** 一个 Job 对象。这里用到了 **大约** ，是因为在少数情况下会创建两个 Job 对象，或者不创建 Job 对象。所以Job程序必须是幂等的

如果 `startingDeadlineSeconds` 设置为很大的数值或未设置（默认），并且 `concurrencyPolicy` 设置为 `Allow`，则作业将始终至少运行一次。

> `startingDeadlineSeconds` 低于10s可能无法执行,因为CronJob 控制器每 10 秒钟执行一次检查。

CronJob 只负责按照时间计划的规定创建 Job 对象，由 Job 来负责管理具体 Pod 的创建和执行。











参考:[工作负载](https://kubernetes.io/zh-cn/docs/concepts/workloads/)

[k8s总结之边车模式sidecar](https://www.361way.com/k8s-sidecar/6689.html)

[ServiceMesh的关键：边车模式（sidecar）](https://cloud.tencent.com/developer/article/1707554)

[容器特权模式与非特权模式的区别](https://mozillazg.com/2021/11/docker-container-difference-between-privileged-mode-and-non-privileged-mode.html)

[Kubernetes 资源分配之 Request 和 Limit 解析](https://cloud.tencent.com/developer/article/1004976)

[延伸阅读_可观测性：你的应用健康吗？](https://kuboard.cn/learning/k8s-intermediate/workload/pod-health.html)

[有状态服务和无状态服务](https://zhuanlan.zhihu.com/p/347379130)