# Kubernetes 容器

> 每个运行的容器都是可重复的； 包含依赖环境在内的标准，意味着无论你在哪里运行它都会得到相同的行为。

## 1. 镜像

> 容器镜像（Image）所承载的是封装了应用程序及其所有软件依赖的二进制数据。 容器镜像是可执行的软件包，可以单独运行；该软件包对所处的运行时环境具有良定（Well Defined）的假定

容器中的 `image` 字段支持与 `docker` 命令一样的语法，包括私有镜像仓库和标签。

例如：`my-registry.example.com:5000/example/web-example:v1.0.1` 由如下几个部分组成：

```
my-registry.example.com:5000/example/web-example:v1.0.1
registry 地址 |registry端口|repository 名字|image 名字|image 标签
```





不指定仓库,Kubernetes 默认使用 Docker 公共仓库.

### 1.1 更新镜像

当创建包含Pod模版的对象,如[Deployment](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/)、 [StatefulSet](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/statefulset/)、Pod时,Pod中所有容器默认镜像拉取策略是`IfNotPresent`,如果本机有镜像的情况下,不会向镜像仓库抓取镜像

**镜像拉取策略**

容器的 `imagePullPolicy` 和镜像的标签会影响 [kubelet](https://kubernetes.io/zh-cn/docs/reference/command-line-tools-reference/kubelet/) 尝试拉取（下载）指定的镜像,主要的`imagePullPolicy` 有一下几种:

- **IfNotPresent**:只有当镜像在本地不存在时才会拉取。
- **Always**:每次启动 Pod 时，从镜像仓库抓取
- **Never**:Kubernetes 假设本地存在该镜像，并且不会尝试从镜像仓库抓取镜像

**应该尽量避免使用 `:latest` 标签.**为了确保 Pod 总是使用相同版本的容器镜像，你可以指定镜像的摘要； 将 `<image-name>:<tag>` 替换为 `<image-name>@<digest>`，例如 `image@sha256:45b23dee08af5e43a7fea6c4cf9c25ccf269ee113168c19722f87876677c5cb2`

**ImagePullBackOff**

当 kubelet 使用容器运行时创建 Pod 时，容器可能因为 `ImagePullBackOff` 导致状态为 [Waiting](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#container-state-waiting).`ImagePullBackOff` 状态意味着容器无法启动， 因为 Kubernetes 无法拉取容器镜像.`BackOff` 部分表示 Kubernetes 将继续尝试拉取镜像，并增加回退延迟。

Kubernetes 会增加每次尝试之间的延迟，直到达到编译限制，即 300 秒

### 1.2 使用私有仓库

从私有仓库读取镜像时可能需要密钥。 凭证可以用以下方式提供:

- 配置节点向私有仓库进行身份验证
  - 所有 Pod 均可读取任何已配置的私有仓库
  - 需要集群管理员配置节点
- 预拉镜像
  - 所有 Pod 都可以使用节点上缓存的所有镜像
  - 需要所有节点的 root 访问权限才能进行设置
- 在 Pod 中设置 ImagePullSecrets
  - 只有提供自己密钥的 Pod 才能访问私有仓库
- 特定于厂商的扩展或者本地扩展
  - 如果你在使用定制的节点配置，你（或者云平台提供商）可以实现让节点向容器仓库认证的机制

## 2. 容器环境

容器环境给容器提供了一下几种重要资源:

- 文件系统，其中包含一个[镜像](https://kubernetes.io/zh-cn/docs/concepts/containers/images/) 和一个或多个的[卷](https://kubernetes.io/zh-cn/docs/concepts/storage/volumes/)
- 容器自身的信息
- 集群中其他对象的信息

容器的 **hostname** 是该容器运行所在的 Pod 的名称,通过`hostname` 命令或者调用 libc 中的 [`gethostname`](https://man7.org/linux/man-pages/man2/gethostname.2.html) 函数获取.

用户所定义的环境变量也可在容器中使用，就像在 container 镜像中静态指定的任何环境变量一样。例如ConfigMap.

## 3. 容器运行时类

> 使用 RuntimeClass 这一特性可以为容器选择运行时的容器引擎。

**设计目标**

通过RuntimeClass ,在不同Pod使用不同的容器引起,在性能和安全之间取的平衡.

也可以通过 RuntimeClass 配置，使不同的 Pod 使用相同的容器引擎和不同的容器引擎配置参数。

### 3.1 设置

**1. 在节点上配置 CRI 实现**

如需通过RuntimeClass进行配置，是依赖于 Container Runtime Interface（CRI）的具体实现的。需要留意其 `handler` 名称,RuntimeClass中将引用该名称。

安装详情请参考文档 [CRI installation](https://kubernetes.io/docs/setup/production-environment/container-runtimes/)

**2. 创建对应的 RuntimeClass**

每个配置都会有一个 `handler` 名称，用来唯一地标识该 CRI 的配置。此时，我们需要为每一个 handler 创建一个对应的 RuntimeClass api 对象

```yaml
# RuntimeClass 定义于 node.k8s.io API 组
apiVersion: node.k8s.io/v1beta1
kind: RuntimeClass
metadata:
  name: myclass # RuntimeClass 没有名称空间
handler: myconfiguration  # 对应 CRI 配置的 handler 名称
```

该对象主要有两个主要字段:RuntimeClass 名 (`metadata.name`) 和 handler (`handler`)

### 3.2 使用

在 Pod 的定义中指定 `runtimeClassName` 即可，例如：

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: mypod
spec:
  runtimeClassName: myclass
  # ...
```

这一设置会告诉 kubelet 使用所指的 RuntimeClass 来运行该 pod。 如果所指的 RuntimeClass 不存在或者 CRI 无法运行相应的 handler， 那么 pod 将会进入 `Failed` 终止[阶段](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-phase)。

### 3.3 主要功能

-  RuntimeClass 是 Kubernetes 一种内置的集群资源，主要用来解决多个容器运行时混用的问题； 
-  RuntimeClass 中配置 Scheduling 可以让 Pod 自动调度到运行了指定容器运行时的节点上。但前提是需要用户提前为这些 Node 设置好 label； 
-  RuntimeClass 中配置 Overhead，可以把 Pod 中业务运行所需以外的开销统计进来，让调度、ResourceQuota、Kubelet Pod 驱逐等行为更准确。

## 4. 容器生命周期回调

Kubernetes 中，也为容器提供了对应的生命周期钩子函数，使得容器可以获知其所在运行环境对其进行管理的生命周期事件，以便容器可以响应该事件，并执行对应的代码

### 4.1 容器钩子

Kubernetes中为容器提供了两个 hook:

- `PostStart`:这个回调在容器被创建之后立即被执行。没有参数,不能保证在容器入口点（ENTRYPOINT）之前执行。
- `PreStop`:此钩子函数在容器被 terminate（终止）之前执行。如果容器已经被关闭或者进入了 `completed` 状态，preStop 钩子函数的调用将失败。kubernetes 将在该函数完成执行之后才删除容器。

**回调处理程序的实现**

容器可以通过实现和注册该回调的处理程序来访问该回调。 

- Exec - 在容器的名称空进和 cgroups 中执行一个指定的命令，例如 `pre-stop.sh`。该命令所消耗的 CPU、内存等资源，将计入容器可以使用的资源限制。
- HTTP - 向容器的指定端口发送一个 HTTP 请求

**Hook handler的执行**

Kubernetes 根据回调动作执行处理程序,`httpGet` 和 `tcpSocket` 在 kubelet 进程执行，而 `exec` 则由容器内执行。

对于 Pod 而言，hook handler 的调用是同步的.如果是`PostStart`,容器的`ENTRYPOINT`同时触发。如果回调运行或挂起的时间太长，则容器无法达到 `running` 状态。

`PreStop` hook 的行为与此相似。如果 hook 在执行过程中挂起了，Pod phase 将停留在 `Terminating` 的状态，并且在 `terminationGracePeriodSeconds` 超时之后，Pod被删除。

果 `PostStart` 或者 `PreStop` hook 执行失败，则 **Kubernetes 将 kill（杀掉）该容器**。

**Hook触发的保证**

回调的递送应该是**至少一次**，这意味着对于任何给定的事件， 例如 `PostStart` 或 `PreStop`，回调可以被调用多次。 如何正确处理被多次调用的情况，是回调实现所要考虑的问题。

**调试 hook handler**

回调处理程序的日志不会在 Pod 事件中公开。 如果处理程序由于某种原因失败，它将播放一个事件(**Failed + $HookName**)。

可以执行命令 `kubectl describe pod $(pod_name)` 以查看这些事件，例如：

```
Events:
  Type     Reason               Age              From               Message
  ----     ------               ----             ----               -------
  Normal   Scheduled            7s               default-scheduler  Successfully assigned default/lifecycle-demo to ip-XXX-XXX-XX-XX.us-east-2...
  Normal   Pulled               6s               kubelet            Successfully pulled image "nginx" in 229.604315ms
  Normal   Pulling              4s (x2 over 6s)  kubelet            Pulling image "nginx"
  Normal   Created              4s (x2 over 5s)  kubelet            Created container lifecycle-demo-container
  Normal   Started              4s (x2 over 5s)  kubelet            Started container lifecycle-demo-container
  Warning  FailedPostStartHook  4s (x2 over 5s)  kubelet            Exec lifecycle hook ([badcommand]) for Container "lifecycle-demo-container" in Pod "lifecycle-demo_default(30229739-9651-4e5a-9a32-a8f1688862db)" failed - error: command 'badcommand' exited with 126: , message: "OCI runtime exec failed: exec failed: container_linux.go:380: starting container process caused: exec: \"badcommand\": executable file not found in $PATH: unknown\r\n"
  Normal   Killing              4s (x2 over 5s)  kubelet            FailedPostStartHook
  Normal   Pulled               4s               kubelet            Successfully pulled image "nginx" in 215.66395ms
  Warning  BackOff              2s (x2 over 3s)  kubelet            Back-off restarting failed container
```







参考:[容器](https://kubernetes.io/zh-cn/docs/concepts/containers/)

[容器镜像](https://kuboard.cn/learning/k8s-intermediate/container/images.html)

[什么是容器化](https://www.redhat.com/zh/topics/cloud-native-apps/what-is-containerization)

[从零开始入门 K8s：理解 RuntimeClass 与使用多容器运行时](https://cloud.tencent.com/developer/news/609235)