# **Kubernetes 存储**

## 1. 卷

Kubernetes Volume（数据卷）主要解决了如下两方面问题：

- 数据持久性：通常情况下，容器运行起来之后，写入到其文件系统的文件暂时性的。当容器崩溃后，kubelet 将会重启该容器，此时原容器运行后写入的文件将丢失，因为容器将重新从镜像创建。
- 数据共享：同一个 Pod（容器组）中运行的容器之间，经常会存在共享文件/文件夹的需求

在 Kubernetes 里，Volume（数据卷）存在明确的生命周期（与包含该数据卷的容器组相同）。因此，Volume（数据卷）的生命周期比同一容器组中任意容器的生命周期要更长，不管容器重启了多少次，数据都能被保留下来。

 [Pod](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/) 可以同时使用任意数目的卷类型。 **临时卷类型的生命周期与 Pod 相同，但持久卷可以比 Pod 的存活期长**。 当 Pod 不再存在时，Kubernetes 也会销毁临时卷；不过 Kubernetes 不会销毁持久卷。

### 1.1 挂载卷的传播

挂载卷的传播能力允许将容器安装的卷共享到同一 Pod 中的其他容器，甚至共享到同一节点上的其他 Pod。

卷的挂载传播特性由 `Container.volumeMounts` 中的 `mountPropagation` 字段控制。 它的值包括：

- **None**:容器所创建的卷挂载在主机上是不可见的(默认)
- **HostToContainer**:如果主机在此挂载卷中挂载任何内容，容器将能看到它被挂载在那里
- **Bidirectional**:这种卷挂载和 `HostToContainer` 挂载表现相同。 另外，容器创建的卷挂载将被传播回至主机和使用同一卷的所有 Pod 的所有容器。



### 1.2 容器与数据卷之间的关系

我们现在通过下图来理解 容器组、容器、挂载点、数据卷、存储介质（nfs、PVC、ConfigMap）等几个概念之间的关系：

- 一个容器组可以包含多个数据卷、多个容器
- 一个容器通过挂载点决定某一个数据卷被挂载到容器中的什么路径
- 不同类型的数据卷对应不同的存储介质（图中列出了 nfs、PVC、ConfigMap 三种存储介质，接下来将介绍更多）

![Kubernetes教程：数据卷](https://kuboard.cn/assets/img/image-20190904201849792.70b324a5.png)





## 2. 持久卷

为了更好的管理存储，Kubernetes 引入了 **持久卷（PersistentVolume，PV）**和 **持久卷声明（PersistentVolumeClaim，PVC）**两个概念，将存储管理抽象成如何提供存储以及如何使用存储两个关注点。

**持久卷（PersistentVolume，PV）**是集群中的一块存储空间，由集群管理员管理、或者由 Storage Class（存储类）自动管理。PV（存储卷）和 node（节点）一样，是集群中的资源.它们拥有独立于任何使用 PV 的 Pod 的生命周期。

**持久卷声明（PersistentVolumeClaim，PVC）**代表用户使用存储的请求。Pod 容器组消耗 node 计算资源，PVC 存储卷声明消耗 PersistentVolume 存储资源。

### 2.1 持久卷和持久卷声明的关系

- PersistentVolume 是集群中的存储资源，通常由集群管理员创建和管理
- StorageClass 用于对 PersistentVolume 进行分类，如果正确配置，StorageClass 也可以根据 PersistentVolumeClaim 的请求动态创建 Persistent Volume
- PersistentVolumeClaim 是使用该资源的请求，通常由应用程序提出请求，并指定对应的 StorageClass 和需求的空间大小
- PersistentVolumeClaim 可以做为数据卷的一种，被挂载到容器组/容器中使用

![Kubernetes教程：存储卷PersistentVolume](https://kuboard.cn/assets/img/image-20190906074512760.92b15a35.png)



### 2.2  存储卷声明的管理过程

下图主要描述的是 PV 和 PVC 的管理过程，因为绘制空间的问题，将挂载点与Pod关联了，实际结构应该如上图所示：

- Pod 中添加数据卷，数据卷关联PVC
- Pod 中包含容器，容器挂载数据卷

![Kubernetes教程：存储卷/存储卷声明的管理过程](https://kuboard.cn/assets/img/image-20191016133601950.cb105979.png)

### 2.3 回收 Reclaiming

当用户不在需要其数据卷时，可以删除掉其 PersistentVolumeClaim，此时其对应的 PersistentVolume 将被集群回收并再利用。

当前支持的回收策略有：Retained（保留）、Recycled（重复利用）、Deleted（删除）

- **保留 Retain**

  保留策略需要集群管理员手工回收该资源。当绑定的 PersistentVolumeClaim 被删除后，PersistentVolume 仍然存在，并被认为是”已释放“。但是此时该存储卷仍然不能被其他 PersistentVolumeClaim 绑定，因为前一个绑定的 PersistentVolumeClaim 对应容器组的数据还在其中。集群管理员可以通过如下步骤回收该 PersistentVolume：

  - 删除该 PersistentVolume。PV 删除后，其数据仍然存在于对应的外部存储介质中（nfs、cefpfs、glusterfs 等）
  - 手工删除对应存储介质上的数据
  - 手工删除对应的存储介质，您也可以创建一个新的 PersistentVolume 并再次使用该存储介质

- **删除 Delete**

  删除策略将从 kubernete 集群移除 PersistentVolume 以及其关联的外部存储介质（云环境中的 AWA EBS、GCE PD、Azure Disk 或 Cinder volume）。

- **再利用 Recycle** **Kuboard 暂不支持**

  - 再利用策略将在 PersistentVolume 回收时，执行一个基本的清除操作（rm -rf /thevolume/*），并使其可以再次被新的 PersistentVolumeClaim 绑定。
  - 集群管理员也可以自定义一个 recycler pod template，用于执行清除操作。



## 3. 存储类

存储类(StorageClass)用于描述集群中可以提供的存储的类型。不同的存储类可能对应着不同的：

- 服务等级（quality-of-service level）
- 备份策略
- 集群管理员自定义的策略

### 3.1 回收策略 Reclaim Policy

由 StorageClass 动态创建的 PersistentVolume 将使用 StorageClass 中定义的回收策略。可选项有：

- 回收后删除 Delete
- 回收后保留 Retain

同一 StorageClass 中，手动创建的 PersistentVolume，将使用创建时手动指定的回收策略。

### 3.2 存储卷绑定模式 Volume Binding Mode

StorageClass 根据存储卷绑定模式的选项，确定何时执行 存储卷与存储卷声明的绑定、何时执行动态存储卷提供（动态创建存储卷）。可选项有：

- 即刻绑定 Immediate

  存储卷声明创建后，立刻动态创建存储卷并将其绑定到存储卷声明。

- 首次使用时绑定 WaitForFirstConsumer

  直到存储卷声明第一次被容器组使用时，才创建存储卷，并将其绑定到存储卷声明。



















参考:[存储](https://kubernetes.io/zh-cn/docs/concepts/storage/)

[数据卷Volume](https://kuboard.cn/learning/k8s-intermediate/persistent/volume.html#%E6%95%B0%E6%8D%AE%E5%8D%B7%E6%A6%82%E8%BF%B0)