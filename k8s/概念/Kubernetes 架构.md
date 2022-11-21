# Kubernetes 架构

## 1. 节点

> 节点（Node）可以是虚拟机或物理机,组件包括了 [kubelet](https://kubernetes.io/docs/reference/generated/kubelet) (kubelet 是在每个 Node 节点上运行的主要 “节点代理”)、 [容器运行时](https://kubernetes.io/zh-cn/docs/setup/production-environment/container-runtimes)以及 [kube-proxy](https://kubernetes.io/zh-cn/docs/reference/command-line-tools-reference/kube-proxy/),容器运行在节点的Pod中,节点由 [control plane](https://kubernetes.io/docs/reference/glossary/?all=true#term-control-plane) 管理.

**管理**

添加节点包括通过节点上的 `kubelet` 向控制面注册;手动添加一个Node对象.添加完成后会检查Node对象是否合法.

示例

```json
{
  "kind": "Node",
  "apiVersion": "v1",
  "metadata": {
    "name": "10.240.79.157",
    "labels": {
      "name": "my-first-k8s-node"
    }
  }
}
```

Kubernetes 检查 `kubelet` 向 API 服务器注册节点时使用的 `metadata.name` 字段是否匹配,在节点健康前,所有集群都会忽略该节点.

PS: 节点的[名称](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/names#names)用来标识 Node 对象,名称唯一. Kubernetes 还假定名字相同的资源是同一个对象,拥有相同状态.

### 1.1 节点自注册

当 kubelet 标志 `--register-node` 为 true（默认）时，它会尝试向 API 服务注册自己。 这是首选模式，被绝大多数发行版选用。

对于自注册模式，kubelet 使用下列参数启动：

- `--kubeconfig` - 用于向 API 服务器执行身份认证所用的凭据的路径。
- `--cloud-provider` - 与某[云驱动](https://kubernetes.io/zh-cn/docs/reference/glossary/?all=true#term-cloud-provider) 进行通信以读取与自身相关的元数据的方式。
- `--register-node` - 自动向 API 服务注册。
- `--register-with-taints` - 使用所给的[污点](https://kubernetes.io/zh-cn/docs/concepts/scheduling-eviction/taint-and-toleration/)列表 （逗号分隔的 `<key>=<value>:<effect>`）注册节点。当 `register-node` 为 false 时无效。
- `--node-ip` - 节点 IP 地址。
- `--node-labels` - 在集群中注册节点时要添加的[标签](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/labels/)。 （参见 [NodeRestriction 准入控制插件](https://kubernetes.io/zh-cn/docs/reference/access-authn-authz/admission-controllers/#noderestriction)所实施的标签限制）。
- `--node-status-update-frequency` - 指定 kubelet 向 API 服务器发送其节点状态的频率。

### 1.2 手动节点管理

手动创建节点需要设置kubelet 标志 `--register-node=false`。可以结合使用Node上的标签和Pod上的选择算符来控制调度.限制某Pod只能在符合要求的节点子集上运行.

如果标记节点为不可调度（unschedulable），将阻止新 Pod 调度到该 Node 之上， 但不会影响任何已经在其上的 Pod。 这是重启节点或者执行其他维护操作之前的一个有用的准备步骤.需要执行以下命令:

```shell
kubectl cordon $NODENAME
```

## 1.3 节点状态

当我们查看节点详细信息时可以使用以下命令: ` kubectl describe nodes $NODENAME`

```yaml
Name:               master
Roles:              control-plane
Labels:             beta.kubernetes.io/arch=amd64
                    beta.kubernetes.io/os=linux
                    kubernetes.io/arch=amd64
                    kubernetes.io/hostname=master
                    kubernetes.io/os=linux
                    node-role.kubernetes.io/control-plane=
                    node.kubernetes.io/exclude-from-external-load-balancers=
Annotations:        kubeadm.alpha.kubernetes.io/cri-socket: unix:///var/run/containerd/containerd.sock
                    node.alpha.kubernetes.io/ttl: 0
                    projectcalico.org/IPv4Address: 172.25.108.133/20
                    projectcalico.org/IPv4IPIPTunnelAddr: 172.17.219.64
                    volumes.kubernetes.io/controller-managed-attach-detach: true
CreationTimestamp:  Tue, 08 Nov 2022 14:23:52 +0800
Taints:             node-role.kubernetes.io/control-plane:NoSchedule
Unschedulable:      false
Lease:
  HolderIdentity:  master
  AcquireTime:     <unset>
  RenewTime:       Thu, 17 Nov 2022 10:42:08 +0800
Conditions: #状况
  Type                 Status  LastHeartbeatTime                 LastTransitionTime                Reason                       Message
  ----                 ------  -----------------                 ------------------                ------                       -------
  NetworkUnavailable   False   Tue, 08 Nov 2022 14:49:19 +0800   Tue, 08 Nov 2022 14:49:19 +0800   CalicoIsUp                   Calico is running on this node
  MemoryPressure       False   Thu, 17 Nov 2022 10:39:41 +0800   Tue, 08 Nov 2022 14:23:47 +0800   KubeletHasSufficientMemory   kubelet has sufficient memory available
  DiskPressure         False   Thu, 17 Nov 2022 10:39:41 +0800   Tue, 08 Nov 2022 14:23:47 +0800   KubeletHasNoDiskPressure     kubelet has no disk pressure
  PIDPressure          False   Thu, 17 Nov 2022 10:39:41 +0800   Tue, 08 Nov 2022 14:23:47 +0800   KubeletHasSufficientPID      kubelet has sufficient PID available
  Ready                True    Thu, 17 Nov 2022 10:39:41 +0800   Tue, 08 Nov 2022 14:49:06 +0800   KubeletReady                 kubelet is posting ready status
Addresses: #地址
  InternalIP:  172.25.108.133
  Hostname:    master
Capacity: #容量
  cpu:                1
  ephemeral-storage:  41931756Ki
  hugepages-1Gi:      0
  hugepages-2Mi:      0
  memory:             1790120Ki
  pods:               110
Allocatable: #可分配
  cpu:                1
  ephemeral-storage:  38644306266
  hugepages-1Gi:      0
  hugepages-2Mi:      0
  memory:             1687720Ki
  pods:               110
System Info: #信息
  Machine ID:                 20200218155604663518171520219378
  System UUID:                3017108d-7ead-40ad-a20f-7933418c2429
  Boot ID:                    571d1dfd-7e74-4f0c-a495-d670c60a1cd6
  Kernel Version:             4.18.0-147.5.1.el8_1.x86_64
  OS Image:                   CentOS Linux 8 (Core)
  Operating System:           linux
  Architecture:               amd64
  Container Runtime Version:  containerd://1.6.9
  Kubelet Version:            v1.25.3
  Kube-Proxy Version:         v1.25.3
PodCIDR:                      172.17.0.0/24
PodCIDRs:                     172.17.0.0/24
Non-terminated Pods:          (8 in total)
  Namespace                   Name                                       CPU Requests  CPU Limits  Memory Requests  Memory Limits  Age
  ---------                   ----                                       ------------  ----------  ---------------  -------------  ---
  kube-system                 calico-kube-controllers-f79f7749d-vdmwn    0 (0%)        0 (0%)      0 (0%)           0 (0%)         8d
  kube-system                 calico-node-pt9l4                          250m (25%)    0 (0%)      0 (0%)           0 (0%)         8d
  kube-system                 coredns-c676cc86f-85xp7                    100m (10%)    0 (0%)      70Mi (4%)        170Mi (10%)    8d
  kube-system                 etcd-master                                100m (10%)    0 (0%)      100Mi (6%)       0 (0%)         8d
  kube-system                 kube-apiserver-master                      250m (25%)    0 (0%)      0 (0%)           0 (0%)         8d
  kube-system                 kube-controller-manager-master             200m (20%)    0 (0%)      0 (0%)           0 (0%)         8d
  kube-system                 kube-proxy-tqvp7                           0 (0%)        0 (0%)      0 (0%)           0 (0%)         8d
  kube-system                 kube-scheduler-master                      100m (10%)    0 (0%)      0 (0%)           0 (0%)         8d
Allocated resources:
  (Total limits may be over 100 percent, i.e., overcommitted.)
  Resource           Requests     Limits
  --------           --------     ------
  cpu                1 (100%)     0 (0%)
  memory             170Mi (10%)  170Mi (10%)
  ephemeral-storage  0 (0%)       0 (0%)
  hugepages-1Gi      0 (0%)       0 (0%)
  hugepages-2Mi      0 (0%)       0 (0%)
Events:              <none>
```

可以看到输出中主要包括以下几部分:

- 地址（Addresses）
- 状况（Condition）
- 容量与可分配（Capacity）
- 信息（Info）

**地址**

地址部分包含三块信息:

- HostName:主机名称,可以通过 `--hostname-override` 参数覆盖
- ExternalIP:节点可以外部路由的IP地址
- InternalIP:节点仅可以在集群内部路由的地址

**状况**

`conditions` 字段描述了所有 `Running` 节点的状况,包括以下几种状态:

- Ready:节点健康是True;False节点不健康且不能接受Pod;Unknown表示控制器在最近 `node-monitor-grace-period` 期间（默认 40 秒）没有收到节点的消息
- DiskPressure:True表示节点存在磁盘空间压力，即磁盘可用量低, 否则为 False
- MemoryPressure:True表示节点存在内存压力，即节点内存可用量低，否则为 False
- PIDPressure:True 表示节点存在进程压力，即节点上进程过多；否则为 False
- NetworkUnavailable:True 表示节点网络配置不正确；否则为 False

**容量（Capacity）与可分配（Allocatable**

这两个值描述节点上的可用资源：CPU、内存和可以调度到节点上的 Pod 的个数上限。

`capacity` 块中的字段标示节点拥有的资源总量。 `allocatable` 块指示节点上可供普通 Pod 消耗的资源量。

**信息（Info）**

Info 指的是节点的一般信息，如内核版本、Kubernetes 版本（`kubelet` 和 `kube-proxy` 版本）、 容器运行时详细信息，以及节点使用的操作系统。 `kubelet` 从节点收集这些信息并将其发布到 Kubernetes API。

### 1.4 心跳

节点有两种心跳:

- 更新节点的 `.status`
- `kube-node-lease` [名字空间](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/namespaces/)中的 [Lease（租约）](https://kubernetes.io/zh-cn/docs/reference/kubernetes-api/cluster-resources/lease-v1/)对象。 每个节点都有一个关联的 Lease 对象。

**Lease 是一种轻量级资源**,使用它表示心跳可以在大型集群中可以减少更新对性能的影响.

kubelet 负责创建和更新节点的 `.status`，以及更新它们对应的 Lease。

- 当节点状态发生变化时，或者在配置的时间间隔内没有更新事件时，kubelet 会更新 `.status`。 `.status` 更新的默认间隔为 5 分钟（比节点不可达事件的 40 秒默认超时时间长很多）。
- `kubelet` 会创建并每 10 秒（默认更新间隔时间）更新 Lease 对象。 Lease 的更新独立于 Node 的 `.status` 更新而发生。 如果 Lease 的更新操作失败，kubelet 会采用指数回退机制，从 200 毫秒开始重试， 最长重试间隔为 7 秒钟。

### 1.5 节点控制器

节点控制器(node controller)是Kubernetes 控制面组件， 管理节点的方方面面.

节点控制器扮演角色:

- 在节点注册时为它分配一个[CIDR](https://blog.csdn.net/qq_43141726/article/details/105520510)（Classless Inter-Domain Routing，无类域间路由选择）区段
- 保持节点控制器里节点列表与云服务商提供的可用机器列表同步,在节点不健康时询问虚拟机是否可用
- 监控节点健康情况
  - 节点不可达的情况下，在 Node 的 `.status` 中更新为`Unknown`
  - 如果节点任然无法访问:对不可达节点上的所有Pod触发[发起的逐出](https://kubernetes.io/zh-cn/docs/concepts/scheduling-eviction/api-eviction/)操作.默认是5分钟后提交第一个请求

**逐出速率限制**

默认情况下节点控制器逐出速率限制在每秒 `--node-eviction-rate` 个（默认为 0.1）,表示每10s最多从一个节点驱逐Pod

当一个可用区域（Availability Zone）中的节点变为不健康时,节点控制器会跟进可用区域中不健康节点(`Ready` 状况为 `Unknown` 或 `False`)的百分比调整驱逐行为.

- 不健康节点的比例超过 `--unhealthy-zone-threshold` （默认为 0.55）， 驱逐速率将会降低
- 集群较小（意即小于等于 `--large-cluster-size-threshold` 个节点 - 默认为 50）， 驱逐操作将会停止
- 否则驱逐速率将降为每秒 `--secondary-node-eviction-rate` 个(默认为0.01)

节点控制器还负责驱逐运行在拥有 `NoExecute` 污点的节点上的 Pod， 除非这些 Pod 能够容忍此污点。 节点控制器还负责根据节点故障（例如节点不可访问或没有就绪） 为其添加[污点](https://kubernetes.io/zh-cn/docs/concepts/scheduling-eviction/taint-and-toleration/)。 这意味着调度器不会将 Pod 调度到不健康的节点上.

> 污点是一个重要对象,会阻止节点或节点组上调度Pod.**污点（Taint）**:它使节点能够排斥一类特定的 Pod.污点和容忍度（Toleration）相互配合，可以用来避免 Pod 被分配到不合适的节点上

**资源容量跟踪**

[自注册](https://kubernetes.io/zh-cn/docs/concepts/architecture/nodes/#self-registration-of-nodes)机制生成的 Node 对象会在注册期间报告自身容量,手动添加的需要在添加时手动设置容量.

Kubernetes [调度器](https://kubernetes.io/zh-cn/docs/reference/command-line-tools-reference/kube-scheduler/) 保证节点上有足够的资源供其上的所有 Pod 使用。 它会检查节点上所有容器的请求的总和不会超过节点的容量。 总的请求包括由 kubelet 启动的所有容器，但不包括由容器运行时直接启动的容器， 也不包括不受 `kubelet` 控制的其他进程。

### 1.6 节点体面关闭(优雅停机)

kubelet 会尝试检测节点系统关闭事件并终止在节点上运行的所有 Pod。

在节点终止期间，kubelet 保证 Pod 遵从常规的 [Pod 终止流程](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-termination)。

> PS:大致是更新Pod对象,更新状态,然后关闭本地Pod,控制器将Pod从EndpointSlice移除.最后删除Pod的API对象

节点体面关闭特性依赖于 systemd，因为它要利用 [systemd 抑制器锁](https://www.freedesktop.org/wiki/Software/systemd/inhibit/)机制.

在体面关闭节点过程中，kubelet 分两个阶段来终止 Pod：

1. 终止在节点上运行的常规 Pod。
2. 终止在节点上运行的[关键 Pod](https://kubernetes.io/zh-cn/docs/tasks/administer-cluster/guaranteed-scheduling-critical-addon-pods/#marking-pod-as-critical)。

### 1.7 节点非体面关闭

   kubelet 的节点关闭管理器没有检测到节点关闭,会导致关闭节点上属于 StatefulSet 的 Pod 将停滞于终止状态,不能被移动到新节点.因为已关闭节点上的 kubelet 已不存在，亦无法删除 Pod， 因此 StatefulSet 无法创建同名的新 Pod.所以，**那些以 StatefulSet 形式运行的应用无法正常工作.**当节点恢复时,Kubelet将删除Pod,将新的Pod在其他节点上创建;没有恢复,那些Pod将永远滞留在终止状态

解决方案:添加手动添加污点,标记为无法服务,由集群将Pod强制删除,将被终止的 Pod 将立即进行卷分离操作.

在非体面关闭期间，Pod 分两个阶段终止：

1. 强制删除没有匹配的 `out-of-service` 容忍度的 Pod。
2. 立即对此类 Pod 执行分离卷操作。

## 2. 节点与控制面之间的通信

### **2.1 节点到控制面**

Kubernetes 采用的是**中心辐射型（Hub-and-Spoke）API 模式**,所有节点发出的API调用终止于API服务器. API 服务器监听**HTTPS端口(443)**,启用客户端[身份认证](https://kubernetes.io/zh-cn/docs/reference/access-authn-authz/authentication/)机制,允许匿名请求.

连接API服务器的Pod,在实例化时,Kubernetes把公共证书和有效的持有者令牌注入到Pod里.kubernetes服务配置一个虚拟IP,用于kube-proxy转发请求到API服务器的HTTPS端口.

### **2.2 控制面到节点**

从控制面到节点有两种通信路径.

- 控制面到集群中每个节点上运行的kubelet 进程;
- 控制面通过它的代理功能连接到任何节点、Pod 或者服务

**API 服务器到 kubelet**

该连接用于:

- 获取Pod日志
- 挂接到运行中的Pod
- 提供Kubelet端口转发功能

 默认情况下，API 服务器不检查 kubelet 的服务证书,在外网中运行**不安全.**可以提供根证书,或者在 API 服务器和 kubelet 之间使用 [SSH 隧道](https://kubernetes.io/zh-cn/docs/concepts/architecture/control-plane-node-communication/#ssh-tunnels),最后启用 [Kubelet 认证/鉴权](https://kubernetes.io/zh-cn/docs/reference/access-authn-authz/kubelet-authn-authz/) 来保护 kubelet API.

**API 服务器到节点、Pod 和服务**

从 API 服务器到节点、Pod 或服务的连接默认为纯 HTTP 方式，因此**既没有认证，也没有加密**。这些连接 **目前还不能安全地** 在非受信网络或公共网络上运行。

### 2.3 Konnectivity 服务

Konnectivity 服务提供 TCP 层的代理，以便支持从控制面到集群的通信.含两个部分：Konnectivity 服务器和 Konnectivity 代理， 分别运行在控制面网络和节点网络中.Konnectivity 代理建立并维持到 Konnectivity 服务器的网络连接

## 3. 控制器

> 控制回路（Control Loop）是一个非终止回路，用于调节系统状态。让**当前状态（Current State）**接近**期望状态（Desired State）**

### 3.1 控制器模式

**通过 API 服务器来控制**

[Job](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/job/) 控制器是一个 Kubernetes 内置控制器,通过和集群API交互管理状态.Job保证可以运行正确数量的Pod来完成工作,通过通知 API 服务器来创建或者移除 Pod.。 [控制面](https://kubernetes.io/zh-cn/docs/reference/glossary/?all=true#term-control-plane)中的其它组件 根据新的消息作出反应（调度并运行新 Pod）并且最终完成工作

Job 控制器会让 Job 的当前状态不断接近期望状态

**直接控制**

有些控制器需要对集群外的一些东西进行修改,他的过程是:做出了一些变更以使得事物更接近你的期望状态， 之后将当前状态报告给集群的 API 服务器.

## 4. cgroup v2

> 在 Linux 上，[控制组](https://kubernetes.io/zh-cn/docs/reference/glossary/?all=true#term-cgroup)约束分配给进程的资源.

> [kubelet](https://kubernetes.io/docs/reference/generated/kubelet) 和底层容器运行时都需要对接 cgroup 来强制执行[为 Pod 和容器管理资源](https://kubernetes.io/zh-cn/docs/concepts/configuration/manage-resources-containers/)， 这包括为容器化工作负载配置 CPU/内存请求和限制。

cgroup v2 是 Linux `cgroup` API 的下一个版本。cgroup v2 提供了一个具有增强资源管理能力的统一控制系统.

cgroup v2 对 cgroup v1 进行了多项改进，例如：

- API 中单个统一的层次结构设计
- 更安全的子树委派给容器
- 更新的功能特性， 例如[压力阻塞信息（Pressure Stall Information，PSI）](https://www.kernel.org/doc/html/latest/accounting/psi.html)
- 跨多个资源的增强资源分配管理和隔离
  - 统一核算不同类型的内存分配（网络内存、内核内存等）
  - 考虑非即时资源变化，例如页面缓存回写

使用 cgroup v2 的推荐方法是使用一个默认启用 cgroup v2 的 Linux 发行版。

## 5. 垃圾收集

> 垃圾收集（Garbage Collection）是 Kubernetes 用于清理集群资源的各种机制的统称。

 垃圾收集允许系统清理如下资源：

- [终止的 Pod](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-garbage-collection)
- [已完成的 Job](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/ttlafterfinished/)
- [不再存在属主引用的对象](https://kubernetes.io/zh-cn/docs/concepts/architecture/garbage-collection/#owners-dependents)
- [未使用的容器和容器镜像](https://kubernetes.io/zh-cn/docs/concepts/architecture/garbage-collection/#containers-images)
- [动态制备的、StorageClass 回收策略为 Delete 的 PV 卷](https://kubernetes.io/zh-cn/docs/concepts/storage/persistent-volumes/#delete)
- [阻滞或者过期的 CertificateSigningRequest (CSR)](https://kubernetes.io/zh-cn/docs/reference/access-authn-authz/certificate-signing-requests/#request-signing-process)
- 在以下情形中删除了的节点对象：
  - 当集群使用[云控制器管理器](https://kubernetes.io/zh-cn/docs/concepts/architecture/cloud-controller/)运行于云端时；
  - 当集群使用类似于云控制器管理器的插件运行在本地环境中时。
- [节点租约对象](https://kubernetes.io/zh-cn/docs/concepts/architecture/nodes/#heartbeats)

### 5.1 属主与依赖

属主引用（Owner Reference）可以告诉控制面哪些对象依赖于其他对象。Kubernetes 使用属主引用来为控制面以及其他 API 客户端在删除某对象时提供一个清理关联资源的机会。 

 属主引用可以帮助 Kubernetes 中的不同组件避免干预并非由它们控制的对象。

> 属主必须存在同一命名空间,当属主不存在时,依赖对象会被删除.

### 5.2 级联删除

当你删除某个对象时，你可以控制 Kubernetes 是否去自动删除该对象的依赖对象， 这个过程称为 **级联删除（Cascading Deletion）**

级联删除有两种类型，分别如下：

- 前台级联删除
- 后台级联删除

**前台级联删除**

前台级联删除时,如果删除的属主对象首先进入 **deletion in progress** 状态.针对属主对象会发生以下事情：

- 将 `metadata.deletionTimestamp` 字段设置为对象要被删除的时间点
- 会将 `metadata.finalizers` 字段设置为 `foregroundDeletion`
- 在删除过程完成之前，通过 Kubernetes API 仍然可以看到该对象

当属主对象进入删除过程中状态后，控制器删除其依赖对象。控制器在删除完所有依赖对象之后， 删除属主对象。这时，通过 Kubernetes API 就无法再看到该对象。

**后台级联删除**

在后台级联删除过程中，Kubernetes 服务器立即删除属主对象，控制器在后台清理所有依赖对象。 默认情况下，Kubernetes 使用后台级联删除方案，除非你手动设置了要使用前台删除， 或者选择遗弃依赖对象。

**被遗弃的依赖对象**

当 Kubernetes 删除某个属主对象时，被留下来的依赖对象被称作被遗弃的（Orphaned）对象。 默认情况下，Kubernetes 会删除依赖对象。

### 5.3 未使用容器和镜像的垃圾收集

[kubelet](https://kubernetes.io/docs/reference/generated/kubelet) 会每五分钟对未使用的镜像执行一次垃圾收集， 每分钟对未使用的容器执行一次垃圾收集。

**容器镜像生命周期**

Kubernetes 通过其**镜像管理器（Image Manager）** 来管理所有镜像的生命周期,工作时与 [cadvisor](https://github.com/google/cadvisor/) 协同.

磁盘用量超出所配置的 `HighThresholdPercent` 值时会触发垃圾收集,按使用时间顺序删除.持续删除镜像，直到磁盘用量到达 `LowThresholdPercent` 值为止

**容器垃圾收集**

kubelet 会基于如下变量对所有未使用的容器执行垃圾收集操作:

- `MinAge`：kubelet 可以垃圾回收某个容器时该容器的最小年龄。设置为 `0` 表示禁止使用此规则。
- `MaxPerPodContainer`：每个 Pod 可以包含的已死亡的容器个数上限。设置为小于 `0` 的值表示禁止使用此规则。
- `MaxContainers`：集群中可以存在的已死亡的容器个数上限。设置为小于 `0` 的值意味着禁止应用此规则。

除以上变量之外，kubelet 还会垃圾收集除无标识的以及已删除的容器，通常从最近未使用的容器开始。

最大数量的容器（`MaxPerPodContainer`）会使得全局的已死亡容器个数超出上限 （`MaxContainers`）时,Kubelet调整`MaxPerPodContainer`.缩小该值,会驱逐最近未使用的容器,当隶属于某已被删除的 Pod 的容器的年龄超过 `MinAge` 时，它们也会被删除





参考:[Kubernetes 架构](https://kubernetes.io/zh-cn/docs/concepts/architecture/)