# 使用 Service 暴露你的应用

## Kubernetes Service 总览

### Pod生命周期相关

Pod虽然相对临时性,拥有[生命周期](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/)。如果一个节点下线,那么该节点上的Pod也会被计划在给定超时期限结束后删除. [ReplicaSet](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/replicaset/) 会自动地通过创建新的 Pod 驱动集群回到目标状态，以保证应用程序正常运行。

**Pod生命周期**主要有:

- **Pending（悬决）**	Pod 已被 Kubernetes 系统接受，但有一个或者多个容器尚未创建亦未运行。此阶段包括等待 Pod 被调度的时间和通过网络下载镜像的时间
- **Running（运行中）**	Pod 已经绑定到了某个节点，Pod 中所有的容器都已被创建。至少有一个容器仍在运行，或者正处于启动或重启状态。
- **Succeeded（成功）**Pod 中的所有容器都已成功终止，并且不会再重启
- **Failed（失败）**	Pod 中的所有容器都已终止，并且至少有一个容器是因为失败终止。也就是说，容器以非 0 状态退出或者被系统终止。
- **Unknown（未知）**	因为某些原因无法取得 Pod 的状态。这种情况通常是因为与 Pod 所在主机通信失败。

### Service定义

Kubernetes 中的服务(Service)是一种抽象概念，它定义了 Pod 的逻辑集和访问 Pod 的协议。Service 使从属 Pod 之间的松耦合成为可能。 Service 推荐用YAML定义.Service 下的一组 Pod 通常由 *LabelSelector* .

#### 示例

比如有如下定义

```yaml
kind: Service
apiVersion: v1
metadata:
  name: my-service
spec:
  selector:
    app: MyApp
  ports:
    - protocol: TCP
      port: 80
      targetPort: 9376
```

该配置创建了一个名称为` my-service`的service,它会将请求代理到 9376 TCP 端口，具有标签 `"app=MyApp"` 的 `Pod` 上。这个 `Service` 将被指派一个 IP 地址（通常称为 “Cluster IP”），它会被服务的代理使用（见下面）。`Service` selector的控制器将会持续扫描符合条件的`Pod`，扫描结果会更新到名称为`my-service`的`Endpoints`对象上。

另外，需要注意的是有一些 Service 的用例没有在 spec 中定义`selector`。 一个没有`selector`创建的 Service 也不会创建相应的端点对象。这允许用户手动将服务映射到特定的端点。没有 selector 的另一种可能是你严格使用`type: ExternalName`来标记。



#### Service暴露方式

Service 允许你的应用程序接收流量。Service 也可以用在 ServiceSpec 标记`type`的方式暴露

- *ClusterIP* (默认) - 在集群的内部 IP 上公开 Service 。这种类型使得 Service 只能从集群内访问。
- *NodePort* - 使用 NAT 在集群中每个选定 Node 的相同端口上公开 Service 。使用`<NodeIP>:<NodePort>` 从集群外部访问 Service。是 ClusterIP 的超集。
- *LoadBalancer* - 在当前云中创建一个外部负载均衡器(如果支持的话)，并为 Service 分配一个固定的外部IP。是 NodePort 的超集。
- *ExternalName* - 通过返回带有该名称的 CNAME 记录，使用任意名称(由 spec 中的`externalName`指定)公开 Service

### Service 和 Label

Service 匹配一组 Pod 是使用 [标签(Label)和选择器(Selector)](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/labels), 它们是允许对 Kubernetes 中的对象进行逻辑操作的一种分组原语。标签(Label)是附加在对象上的键/值对，可以以多种方式使用:

- 指定用于开发，测试和生产的对象
- 嵌入版本标签
- 使用 Label 将对象进行分类

标签（`labels`）：标签是用来连接一组对象的，比如容器组。标签可以被用来组织和选择子对象。标签(Label)可以在创建时或之后附加到对象上。他们可以随时被修改。

## 实际操作

### 1. 创建新容器

首先查看当前存在的Pods信息,然后查看当前集群中的services信息.

要创建新的service并将其公开给外部流量，我们将使用以 NodePort 作为参数的公开命令.

```sh
kubectl expose deployment/kubernetes-bootcamp --type="NodePort" --port 8080
```

当我们再次查看services时发现新创建的service信息

```
$ kubectl get services
NAME                  TYPE        CLUSTER-IP       EXTERNAL-IP   PORT(S)          AGE
kubernetes            ClusterIP   10.96.0.1        <none>        443/TCP          6m52s
kubernetes-bootcamp   NodePort    10.108.176.188   <none>        8080:32293/TCP   105s
```

我们现在有一个名为 kubernetes-bootcamp 的正在运行的服务。在这里，我们看到服务收到了一个唯一的集群 IP、一个内部端口和一个外部 IP.

然后通过` describe servic`命令找到外部打开的端口.

创建变量`NODE_PORT `保存开放端口:

```
export NODE_PORT=$(kubectl get services/kubernetes-bootcamp -o go-template='{{(index .spec.ports 0).nodePort}}')
echo NODE_PORT=$NODE_PORT
```

然后测试端口是否已经开放,能被外部访问.

```
$curl localhost:$NODE_PORT
 Hello Kubernetes bootcamp! | Running on: kubernetes-bootcamp-fb5c67579-kjdc5 | v=1
```

### 2. 使用标签

Deployment 自动为我们的 Pod 创建了一个标签。使用 describe deployment 命令，您可以看到标签的名称：`kubectl describe deployment`

返回如下:

```
Name:                   kubernetes-bootcamp
Namespace:              default
CreationTimestamp:      Mon, 14 Nov 2022 07:40:03 +0000
Labels:                 app=kubernetes-bootcamp
Annotations:            deployment.kubernetes.io/revision: 1
Selector:               app=kubernetes-bootcamp
Replicas:               1 desired | 1 updated | 1 total | 1 available | 0 unavailable
StrategyType:           RollingUpdate
MinReadySeconds:        0
RollingUpdateStrategy:  25% max unavailable, 25% max surge
Pod Template:
  Labels:  app=kubernetes-bootcamp
  Containers:
   kubernetes-bootcamp:
    Image:        gcr.io/google-samples/kubernetes-bootcamp:v1
    Port:         8080/TCP
    Host Port:    0/TCP
    Environment:  <none>
    Mounts:       <none>
  Volumes:        <none>
Conditions:
  Type           Status  Reason
  ----           ------  ------
  Available      True    MinimumReplicasAvailable
  Progressing    True    NewReplicaSetAvailable
OldReplicaSets:  <none>
NewReplicaSet:   kubernetes-bootcamp-fb5c67579 (1/1 replicas created)
Events:
  Type    Reason             Age   From                   Message
  ----    ------             ----  ----                   -------
  Normal  ScalingReplicaSet  101s  deployment-controller  Scaled up replica set kubernetes-bootcamp-fb5c67579 to 1
```

通过`kubectl get pods`命令添加 `-l` 参数获取某个标签下的pod信息.

`kubectl get pods -l app=kubernetes-bootcamp`

通过同样方法,我们也能获取到相关的service信息.`kubectl get services -l app=kubernetes-bootcamp`

将获取到的变量名称存储到环境变量中:`export POD_NAME=$(kubectl get pods -o go-template --template '{{range .items}}{{.metadata.name}}{{"\n"}}{{end}}') echo Name of the Pod: $POD_NAME`

**应用新标签**我们需要使用标签命令,并制定应用对象:` kubectl label pods $POD_NAME version=v1`

此时我们用descibe命令查看该pod时会显示该pod的标签版本号已经变成v1.

我们在这里看到标签现在附加到我们的 Pod 上。我们现在可以使用新标签查询 pod 列表：`kubectl get pods -l version=v1`;

### 3. 删除服务

可以通过`delete service`命令删除服务,标签页可以用相关指令删除.`kubectl delete service -l app=kubernetes-bootcamp`

当我们再次获取service时发现service已经被删除.从外部访问应用已经无法访问.当我们运行:`kubectl exec -ti $POD_NAME -- curl localhost:8080`

发现应用程序还是正常运行.





参考:[使用 Service 暴露你的应用](https://kubernetes.io/zh-cn/docs/tutorials/kubernetes-basics/expose/expose-intro/)

[生命周期](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/)

[Service](https://jimmysong.io/kubernetes-handbook/concepts/service.html)