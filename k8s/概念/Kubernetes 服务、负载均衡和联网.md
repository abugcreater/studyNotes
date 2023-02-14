# Kubernetes 服务、负载均衡和联网

## 1. Kubernetes 网络模型

集群中每一个 [`Pod`](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/) 都会获得自己的、 独一无二的 IP 地址， 这就意味着你不需要显式地在 `Pod` 之间创建链接，你几乎不需要处理容器端口到主机端口之间的映射。

在这个模型里，从端口分配、命名、服务发现、 [负载均衡](https://kubernetes.io/zh-cn/docs/concepts/services-networking/ingress/#load-balancing)、 应用配置和迁移的角度来看，`Pod` 可以被视作虚拟机或者物理主机。

**Kubernetes 强制要求所有网络设施都满足以下基本要求**（从而排除了有意隔离网络的策略）：

- Pod 能够与所有其他[节点](https://kubernetes.io/zh-cn/docs/concepts/architecture/nodes/)上的 Pod 通信， 且不需要网络地址转译（NAT）
- 节点上的代理（比如：系统守护进程、kubelet）可以和节点上的所有 Pod 通信

Kubernetes 的 IP 地址存在于 `Pod` 范围内 —— 容器共享它们的网络命名空间 —— 包括它们的 IP 地址和 MAC 地址。 这就意味着 **`Pod` 内的容器都**可以通过 `localhost` 到达对方端口。

Kubernetes 网络**解决四方面的问题**：

- 一个 Pod 中的容器之间[通过本地回路（loopback）通信](https://kubernetes.io/zh-cn/docs/concepts/services-networking/dns-pod-service/)。
- 集群网络在不同 Pod 之间提供通信。
- Service 资源允许你向外暴露 Pod 中运行的应用， 以支持来自于集群外部的访问。
  - [Ingress](https://kubernetes.io/zh-cn/docs/concepts/services-networking/ingress/) 提供专门用于暴露 HTTP 应用程序、网站和 API 的额外功能。
- 你也可以使用 Service 来[发布仅供集群内部使用的服务](https://kubernetes.io/zh-cn/docs/concepts/services-networking/service-traffic-policy/)。

## 2. 服务（Service）

Service是将运行在一组 [Pods](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/) 上的应用程序公开为网络服务的抽象方法。 为 Pod 提供自己的 IP 地址，并为一组 Pod 提供相同的 DNS 名， 并且可以在它们之间进行负载均衡。

### 2.1 为何需要 Service

旧Pod的销毁以及新Pod的创建,导致统一集群中的pod节点变化后端Pod,无法为前端Pod提供功能,此时需要service找出并跟踪要连接的 IP 地址，以便前端可以使用提供工作负载的后端部分

### 2.2 Service的作用

Kubernetes 中 Service 是一个 API 对象，通过 kubectl + YAML 或者 Kuboard，定义一个 Service，可以将符合 Service 指定条件的 Pod 作为可通过网络访问的服务提供给服务调用者。

Service 是 Kubernetes 中的**一种服务发现机制**：

- Pod 有自己的 IP 地址
- Service 被赋予一个唯一的 dns name
- Service 通过 label selector 选定一组 Pod
- Service 实现负载均衡，可将请求均衡分发到选定这一组 Pod 中

Kubernetes 通过引入 Service 的概念，将前端与后端解耦。

在 Kubernetes 集群中，每个 Node 运行一个 `kube-proxy` 进程。 `kube-proxy` 负责为 Service 实现了一种 VIP（虚拟 IP）的形式，而不是 [`ExternalName`](https://kubernetes.io/zh-cn/docs/concepts/services-networking/service/#externalname) 的形式。

### 2.3 定义service

**EndpointSlice **

[EndpointSlices](https://kubernetes.io/zh-cn/docs/concepts/services-networking/endpoint-slices/) 对象表示针对服务的后备网络端点的子集（**切片**）

 Kubernetes 集群会跟踪每个 EndpointSlice 表示的端点数量。一旦现有 EndpointSlice 都包含至少 100 个端点，Kubernetes 就会创建一个新的 EndpointSlice。 在需要添加额外的端点之前，Kubernetes 不会创建新的 EndpointSlice。

**Endpoints**

[Endpoints](https://kubernetes.io/zh-cn/docs/reference/kubernetes-api/service-resources/endpoints-v1/) （该资源类别为复数）定义了网络端点的列表，通常由 Service 引用，以定义可以将流量发送到哪些 Pod。

推荐用 EndpointSlice API 替换 Endpoints

### 2.4 流量策略

**外部流量策略**

你可以通过设置 `spec.externalTrafficPolicy` 字段来控制来自于外部的流量是如何路由的。 可选值有 `Cluster` 和 `Local`。字段设为 `Cluster` 会将外部流量路由到所有就绪的端点， 设为 `Local` 会只路由到当前节点上就绪的端点。 如果流量策略设置为 `Local`，而且当前节点上没有就绪的端点，kube-proxy 不会转发请求相关服务的任何流量。

**内部流量策略**

使用`spec.internalTrafficPolicy`字段控制,分为local和cluster,其他基本与外部流量策略一致

### 2.5 无头服务(Headless Services)

有时不需要或不想要负载均衡，以及单独的 Service IP。 遇到这种情况，可以通过指定 Cluster IP（`spec.clusterIP`）的值为 `"None"` 来创建 `Headless` Service。

对于无头 `Services` 并不会分配 Cluster IP，kube-proxy 不会处理它们， 而且平台也不会为它们进行负载均衡和路由。 DNS 如何实现自动配置，依赖于 Service 是否定义了选择算符

## 3. Ingress

Ingress 是 Kubernetes 的一种 API 对象，将集群内部的 Service 通过 HTTP/HTTPS 方式暴露到集群外部，并通过规则定义 HTTP/HTTPS 的路由。

Ingress 可以提供负载均衡、SSL 终结和基于名称的虚拟托管。

> - 边缘路由器（Edge Router）: 在集群中强制执行防火墙策略的路由器。可以是由云提供商管理的网关，也可以是物理硬件。
> - 集群网络（Cluster Network）: 一组逻辑的或物理的连接，根据 Kubernetes [网络模型](https://kubernetes.io/zh-cn/docs/concepts/cluster-administration/networking/)在集群内实现通信。

### 3.1 Ingress 是什么？

[Ingress](https://kubernetes.io/docs/reference/generated/kubernetes-api/v1.25/#ingress-v1beta1-networking-k8s-io) 公开从集群外部到集群内[服务](https://kubernetes.io/zh-cn/docs/concepts/services-networking/service/)的 HTTP 和 HTTPS 路由。 流量路由由 Ingress 资源上定义的规则控制。

![ingress-diagram](https://d33wubrfki0l68.cloudfront.net/4f01eaec32889ff16ee255e97822b6d165b633f0/a54b4/zh-cn/docs/images/ingress.svg)

 [Ingress 控制器](https://kubernetes.io/zh-cn/docs/concepts/services-networking/ingress-controllers) 通常负责通过负载均衡器来实现 Ingress，尽管它也可以配置边缘路由器或其他前端来帮助处理流量。

取决于你的 Ingress 控制器，你可能可以使用集群范围设置的参数或某个名字空间范围的参数。

### 3.2 Ingress定义

```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: minimal-ingress
  annotations:
    nginx.ingress.kubernetes.io/rewrite-target: /
spec:
  ingressClassName: nginx-example
  rules:   # Ingress 中定义 L7 路由规则
  - http:
      paths:
      - path: /testpath
        pathType: Prefix
        backend:
          service:
            name: test   # 指定后端的 Service 为之前创建的test
            port:
              number: 80
```

Ingress [规约](https://git.k8s.io/community/contributors/devel/sig-architecture/api-conventions.md#spec-and-status) 提供了配置负载均衡器或者代理服务器所需的所有信息。 最重要的是，其中包含与所有传入请求匹配的规则列表。 Ingress 资源仅支持用于转发 HTTP(S) 流量的规则。

**Ingress 规则**

- 可选的 `host`,该规则适用于通过指定 IP 地址的所有入站 HTTP 通信
- 路径列表 paths,每个路径都有一个由 `serviceName` 和 `servicePort` 定义的关联后端
- `backend`（后端）是 [Service 文档](https://kubernetes.io/zh-cn/docs/concepts/services-networking/service/)中所述的服务和端口名称的组合

## 4. EndpointSlice 

**端点切片（EndpointSlices）** 提供了一种简单的方法来跟踪 Kubernetes 集群中的网络端点（network endpoints）。 它们为 Endpoints 提供了一种可扩缩和可拓展的替代方案。 

EndpointSlices包含了对与 Service 选择算符匹配的所有 Pod 的引用,通过唯一的协议,端口号和Service 名称将网络端点组织在一起。 EndpointSlice 的名称必须是合法的 [DNS 子域名](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/names#dns-subdomain-names)。

EndpointSlice API 存储了可能对使用者有用的、有关端点的状况。 这三个状况分别是 `ready`、`serving` 和 `terminating`。

**Ready:映射 Pod 的 `Ready` 状况的。 对于处于运行中的 Pod，它的 `Ready` 状况被设置为 `True`**

**Serving:与Ready不同,不考虑终止状态**

**Terminating:表示端点是否处于终止中的状况**











参考:[服务、负载均衡和联网](https://kubernetes.io/zh-cn/docs/concepts/services-networking/)

[网络地址转换(**N**etwork **A**ddress **T**ranslation)](https://zh.wikipedia.org/zh-cn/%E7%BD%91%E7%BB%9C%E5%9C%B0%E5%9D%80%E8%BD%AC%E6%8D%A2)

[确定k8s的Annotation与Labels你用对了？](https://cloud.tencent.com/developer/article/1833485)