# 查看 pod 和工作节点

## Kubernetes Pods

创建 Deployment 时, Kubernetes 添加了一个 **Pod** 来托管你的应用实例。Pod是Kubernetes 的抽象概念,表示一组一个或多个应用容器.以及这些容器的一些共享资源.

这些资源包括:

- 共享存储，当作卷
- 网络，作为唯一的集群 IP 地址
- 有关每个容器如何运行的信息，例如容器镜像版本或要使用的特定端口。

Pod 为特定于应用程序的“逻辑主机”建模，并且可以包含相对紧耦合的不同应用容器。Pod 中的容器共享 IP 地址和端口，始终位于同一位置并且共同调度，并在同一工作节点上的共享上下文中运行。

Pod是 Kubernetes 平台上的**原子单元**。 当我们在 Kubernetes 上创建 Deployment 时，该 Deployment 会在其中**创建包含容器的 Pod** （而不是直接创建容器）。每个 Pod 都与调度它的工作节点绑定，并保持在那里直到终止（根据重启策略）或删除。 如果工作节点发生故障，则会在集群中的其他可用工作节点上调度相同的 Pod

**Pod 概览**

![img](https://d33wubrfki0l68.cloudfront.net/fe03f68d8ede9815184852ca2a4fd30325e5d15a/98064/docs/tutorials/kubernetes-basics/public/images/module_03_pods.svg)

## 工作节点

pod运行与**工作节点(work node/node)**.工作节点可以是虚拟机或物理计算机,取决于集群.由master管理,一个工作几点可以有多个pod.

 Kubernetes 工作节点至少运行:

- Kubelet，负责 Kubernetes 主节点和工作节点之间通信的过程; 它管理 Pod 和机器上运行的容器。
- 容器运行时（如 Docker）负责从仓库中提取容器镜像，解压缩容器以及运行应用程序

**工作节点概览**

![img](https://d33wubrfki0l68.cloudfront.net/5cb72d407cbe2755e581b6de757e0d81760d5b86/a9df9/docs/tutorials/kubernetes-basics/public/images/module_03_nodes.svg)

## 使用 kubectl 进行故障排除

最常见的操作可以使用以下 kubectl 命令完成:

- **kubectl get** - 列出资源
- **kubectl describe** - 显示有关资源的详细信息
- **kubectl logs** - 打印 pod 和其中容器的日志
- **kubectl exec** - 在 pod 中的容器上执行命令

可以使用这些命令查看应用程序的部署时间，当前状态，运行位置以及配置

## 实际执行

### 1.检查应用配置

首先通过`kubectl get pods`命令获取当前pods信息.然后你能看到当前Pod相关信息.如果想要看pod内容器及容器中的镜像信息需要运行命令:` kubectl describe pods`.

获取到详细的容器信息:

```yml

Name:         kubernetes-bootcamp-fb5c67579-rmb6b
Namespace:    default
Priority:     0
Node:         minikube/10.0.0.13
Start Time:   Thu, 10 Nov 2022 08:35:20 +0000
Labels:       app=kubernetes-bootcamp
              pod-template-hash=fb5c67579
Annotations:  <none>
Status:       Running
IP:           172.18.0.3
IPs:
  IP:           172.18.0.3
Controlled By:  ReplicaSet/kubernetes-bootcamp-fb5c67579
Containers:
  kubernetes-bootcamp:
    Container ID:   docker://cdd742d8c31e806c0314f4b415445513bad8f721ebd7c0a9ba3c97dead68915a
    Image:          gcr.io/google-samples/kubernetes-bootcamp:v1
    Image ID:       docker-pullable://jocatalin/kubernetes-bootcamp@sha256:0d6b8ee63bb57c5f5b6156f446b3bc3b3c143d233037f3a2f00e279c8fcc64af
    Port:           8080/TCP
    Host Port:      0/TCP
    State:          Running
      Started:      Thu, 10 Nov 2022 08:35:23 +0000
    Ready:          True
    Restart Count:  0
    Environment:    <none>
    Mounts:
      /var/run/secrets/kubernetes.io/serviceaccount from default-token-dwp6t (ro)
Conditions:
  Type              Status
  Initialized       True 
  Ready             True 
  ContainersReady   True 
  PodScheduled      True 
Volumes:
  default-token-dwp6t:
    Type:        Secret (a volume populated by a Secret)
    SecretName:  default-token-dwp6t
    Optional:    false
QoS Class:       BestEffort
Node-Selectors:  <none>
Tolerations:     node.kubernetes.io/not-ready:NoExecute op=Exists for 300s
                 node.kubernetes.io/unreachable:NoExecute op=Exists for 300s
Events:
  Type    Reason     Age   From               Message
  ----    ------     ----  ----               -------
  Normal  Scheduled  116s  default-scheduler  Successfully assigned default/kubernetes-bootcamp-fb5c67579-rmb6b to minikube
  Normal  Pulled     113s  kubelet            Container image "gcr.io/google-samples/kubernetes-bootcamp:v1" already present on machine
  Normal  Created    113s  kubelet            Created container kubernetes-bootcamp
  Normal  Started    113s  kubelet            Started container kubernetes-bootcamp
```

从返回信息中我们能看到容器的IP地址,端口以及与 Pod 生命周期相关的事件列表.

### 2. 在终端展示应用程序

pod运行时隔离,拥有单独的网络,所以我们需要代理去链接他们.我们将使用` kubectl proxy `命令在第二个终端窗口中运行代理.

```shell
echo -e "\n\n\n\e[92mStarting Proxy. After starting it will not output a response. Please click the first Terminal Tab\n"; kubectl proxy
```

现在，我们将再次获取 Pod 名称并直接通过代理查询该 pod。获取 Pod 名称并将其存储在 POD_NAME 环境变量中:

```shell
export POD_NAME=$(kubectl get pods -o go-template --template '{{range .items}}{{.metadata.name}}{{"\n"}}{{end}}')
echo Name of the Pod: $POD_NAME
```

通过curl查看应用输出:

```
curl http://localhost:8001/api/v1/namespaces/default/pods/$POD_NAME/proxy/
```

### 3.查看容器内日志

应用程序通常发送到 STDOUT 的任何内容都会成为 Pod 中容器的日志。我们可以使用 kubectl logs 命令检索这些日志:`kubectl logs $POD_NAME`

### 4. 在容器内执行命令

当容器在Pod中运行时我们能直接对应用执行命令.我们通过`exec`命令和Pod名称作为参数.让我们列出环境变量:`kubectl exec $POD_NAME -- env`

输出如下内容:

```properties
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
HOSTNAME=kubernetes-bootcamp-fb5c67579-rmb6b
KUBERNETES_SERVICE_HOST=10.96.0.1
KUBERNETES_SERVICE_PORT=443
KUBERNETES_SERVICE_PORT_HTTPS=443
KUBERNETES_PORT=tcp://10.96.0.1:443
KUBERNETES_PORT_443_TCP=tcp://10.96.0.1:443
KUBERNETES_PORT_443_TCP_PROTO=tcp
KUBERNETES_PORT_443_TCP_PORT=443
KUBERNETES_PORT_443_TCP_ADDR=10.96.0.1
NPM_CONFIG_LOGLEVEL=info
NODE_VERSION=6.3.1
HOME=/root
```

当我们Pod中只有一个容器时容器的名称是可以被省略的.

让我们开始对**容器的session**:`kubectl exec -ti $POD_NAME -- bash`

当前打开的容器中运行这一个NodeJS应用,如果想要看源代码直接运行:`cat server.js`查看就可以了.

如果想要检查应用是否运行正常,直接使用curl命令即可:

```sh
# curl localhost:8080
Hello Kubernetes bootcamp! | Running on: kubernetes-bootcamp-fb5c67579-rmb6b | v=1
```







参考:[查看 pod 和工作节点](https://kubernetes.io/zh-cn/docs/tutorials/kubernetes-basics/explore/explore-intro/)