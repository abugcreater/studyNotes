# Kubernetes 对象

## 1.对象的概念

**Kubernetes 对象** 是持久化的实体。 Kubernetes 使用这些实体去表示整个集群的状态。 比较特别地是，它们描述了如下信息：

- 哪些容器化应用正在运行（以及在哪些节点上运行）
- 可以被应用使用的资源
- 关于应用运行时表现的策略，比如重启策略、升级策略以及容错策略

一旦创建该对象,K8S会持续保证他已期望方式运行,这就是 Kubernetes 集群所谓的 **期望状态（Desired State）**

### 1.1 对象规约（Spec）与状态（Status）

K8S中对象包含两个嵌套字段,管理对象的配置:**`spec`（规约）** 和  **`status`（状态）**.

- `spec`:创建对象时必须设置内容,描述期望具有的特征:**期望状态（Desired State）**

- `status`:描述对象的当前状态,由Kubernetes系统维护.

### 1.2 描述 Kubernetes 对象

可以通过Kubernetes API创建对象,请求包含JSON格式信息.或者,**需要提供 `.yaml` 文件为 kubectl 提供这些信息**

示例参考:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nginx-deployment
spec:
  selector:
    matchLabels:
      app: nginx
  replicas: 2 # 告知 Deployment 运行 2 个与该模板匹配的 Pod
  template:
    metadata:
      labels:
        app: nginx
    spec:
      containers:
      - name: nginx
        image: nginx:1.14.2
        ports:
        - containerPort: 80
```

该配置描述了我们期望创建一个对象类型,容器镜像,选择器,标签,副本集等.想要让对象生效需要使用以下指令:

```sh
$ kubectl apply -f https://k8s.io/examples/application/deployment.yaml
deployment.apps/nginx-deployment created
```

**yaml配置文件中必要字段**

- `apiVersion` - 创建该对象所使用的 Kubernetes API 的版本
- `kind` - 想要创建的对象的类别
- `metadata` - 帮助唯一标识对象的一些数据，包括一个 `name` 字符串、`UID` 和可选的 `namespace`
- `spec` - 你所期望的该对象的状态,每种对象的格式不同,参考 [Kubernetes API ](https://kubernetes.io/zh-cn/docs/reference/kubernetes-api/)

## 2. Kubernetes 对象管理

对象管理有三种类型:指令式命令(kubectl指令),指令式对象配置通过yaml文件,声明式对象配置Kustomize 对对象管理.

**指令性的命令行**

用户通过向 `kubectl` 命令提供参数的方式，直接操作集群中的 Kubernetes 对象。此时，用户无需编写或修改 `.yaml` 文件。示例等效语句创建deployment对象:

```sh
kubectl run nginx --image nginx
kubectl create deployment nginx --image nginx
```

**指令性的对象配置**

在指令式对象配置中，kubectl 命令指定操作（创建，替换等），可选标志和 至少一个文件名。指定的文件必须包含 YAML 或 JSON 格式的对象的完整定义.

示例:

创建配置文件中定义的对象：

```sh
kubectl create -f nginx.yaml
```

删除两个配置文件中定义的对象：

```sh
kubectl delete -f nginx.yaml -f redis.yaml
```

通过覆盖活动配置来更新配置文件中定义的对象：

```sh
kubectl replace -f nginx.yaml
```

**声明式对象配置**

声明式对象配置将保留其他用户对Kubernetes对象的更新，即使这些更新没有合并到对象配置文件中。因为当Kubernetes中已经存在该对象时，声明式对象配置使用 `patch` API接口，此时会把变化的内容更新进去，而不是使用 `replace` API接口，该接口替换整个 spec 信息

示例:

处理 `configs` 目录中的所有对象配置文件，创建并更新活跃对象。 可以首先使用 `diff` 子命令查看将要进行的更改，然后在进行应用：

```sh
kubectl diff -f configs/
kubectl apply -f configs/
```

递归处理目录：

```sh
kubectl diff -R -f configs/
kubectl apply -R -f configs/
```

## 3. 对象名称和 ID

Kubernetes REST API 中，所有的对象都是通过 `name` 和 `UID` 唯一性确定.在同一个[命名空间](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/namespaces/) 中有一个名为 `myapp-1234` 的 Pod，但是可以命名一个 Pod 和一个 Deployment 同为 `myapp-1234`

**名称**

可以通过 `namespace` + `name` 唯一性地确定一个 RESTFUL 对象，例如：

```
/api/v1/namespaces/{namespace}/pods/{name}
```

比较常见的四种资源命名约束**:DNS 子域名,RFC 1123 标签名,RFC 1035 标签名,路径分段名称**.

**UID**

UID 是由 Kubernetes 系统生成的，唯一标识某个 Kubernetes 对象的字符串。用于区分多次创建的同名对象,对象可能同名,但不会有同一个UID.

Kubernetes 中的 UID 是全局唯一的标识符

## 4. 标签和选择算符

### 4.1 **标签（Labels）**

**标签（Labels）** 附加到对象的键值对,旨在用于指定对用户有意义且相关的对象的标识属性.

```json
"metadata": {
  "labels": {
    "key1" : "value1",
    "key2" : "value2"
  }
}
```

标签能够支持高效的查询和监听操作，对于用户界面和命令行是很理想的

**为什么要使用标签?**

使用标签，用户可以按照自己期望的形式组织 Kubernetes 对象之间的结构，而无需对 Kubernetes 有任何修改

**标签语法**

标签是键值对,标签键有两端:可选的前缀和必选的名称.前缀符合DNS子域,如`xxx.xx/`.其中`kubernetes.io/` 和 `k8s.io/` 前缀保留不可用.

### 4.2 标签选择算符(Selectors)

通过**标签选择算符**，客户端/用户可以识别一组对象。标签选择算符是 Kubernetes 中的核心分组原语。API 目前支持两种类型的选择算符：**基于等值的**和**基于集合的**.

 **基于等式的选择方式**

Equality- 或者 inequality-based 选择器可以使用标签的名和值来执行过滤选择.只有匹配所有条件的对象才被选中,使用三种操作符 `=`、`==`、`!=`.

 **基于集合的选择方式**

Set-based 标签选择器可以根据标签名的一组值进行筛选。支持的操作符有三种：`in`、`notin`、`exists`

**两种选择方式可以混用**,例如：`partition in (customerA, customerB),environment!=qa`

### 4.3 API

LIST 和 WATCH 操作可以使用查询参数指定标签选择算符过滤一组对象，但是要符合 URL 编码，例如：

- 基于等式的选择方式： `?labelSelector=environment%3Dproduction,tier%3Dfrontend`
- 基于集合的选择方式： `?labelSelector=environment+in+%28production%2Cqa%29%2Ctier+in+%28frontend%29`

**kubectl** 中的操作方式:

可以使用**基于等值**的标签选择算符可以这么写：

```shell
kubectl get pods -l environment=production,tier=frontend
```

或者使用**基于集合的**需求：

```shell
kubectl get pods -l 'environment in (production),tier in (frontend)'
```

#### Service 和 ReplicationController

一个 `Service` 指向的一组 Pod 是由标签选择算符定义的。同样，一个 `ReplicationController` 应该管理的 Pod 的数量也是由标签选择算符定义的。

两个对象的标签选择算符都是在 `json` 或者 `yaml` 文件中使用映射定义的，并且只支持 **基于等值**需求的选择算符：

```yaml
selector:
    component: redis
```



比较新的资源，例如 [`Job`](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/job/)、 [`Deployment`](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/deployment/)、 [`ReplicaSet`](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/replicaset/) 和 [`DaemonSet`](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/daemonset/)， 也支持**基于集合的**需求。

```yaml
selector:
  matchLabels:
    component: redis
  matchExpressions:
    - {key: tier, operator: In, values: [cache]}
    - {key: environment, operator: NotIn, values: [dev]}
```

## 5. 命名空间

> Kubernetes通过名称空间（namespace）在同一个物理集群上支持多个虚拟集群

名字空间适用于存在很多跨多个团队或项目的用户的场景。对于只有几到几十个用户的集群，根本不需要创建或考虑名字空间。当需要名字空间提供的功能时，请开始使用它们。

名称空间内部的同类型对象不能重名，但是跨名称空间可以有同名同类型对象。名称空间不可以嵌套，任何一个Kubernetes对象只能在一个名称空间中。

**初始命名空间**

Kubernetes 会创建四个初始名字空间：

- `default` 没有指明使用其它名字空间的对象所使用的默认名字空间
- `kube-system` Kubernetes 系统创建对象所使用的名字空间
- `kube-public` 这个名字空间是自动创建的，所有用户（包括未经过身份验证的用户）都可以读取它。 这个名字空间主要用于集群使用，以防某些资源在整个集群中应该是可见和可读的。 这个名字空间的公共方面只是一种约定，而不是要求。
- `kube-node-lease` 此名字空间用于与各个节点相关的 [租约（Lease）](https://kubernetes.io/docs/reference/kubernetes-api/cluster-resources/lease-v1/)对象。 节点租期允许 kubelet 发送[心跳](https://kubernetes.io/zh-cn/docs/concepts/architecture/nodes/#heartbeats)，由此控制面能够检测到节点故障。

**如何使用?**

> 名字空间的创建和删除在[名字空间的管理指南文档](https://kubernetes.io/zh-cn/docs/tasks/administer-cluster/namespaces/)描述。

查看命名空间 : `kubectl get namespace`

未请求设置命名空间:

```sh
kubectl run nginx --image=nginx --namespace=<名字空间名称>
kubectl get pods --namespace=<名字空间名称>
```

设置命名空间偏好:

```sh
kubectl config set-context --current --namespace=<名字空间名称>
# 验证
kubectl config view --minify | grep namespace:
```

**命名空间与DNS**

当您创建一个 Service 时，Kubernetes 为其创建一个对应的 [DNS 条目](https://kuboard.cn/learning/k8s-intermediate/service/dns.html)。该 DNS 记录的格式为 `<service-name>.<namespace-name>.svc.cluster.local`，也就是说，如果在容器中只使用 `<service-name>`，其DNS将解析到同名称空间下的 Service。这个特点在多环境的情况下非常有用



Kubernetes 控制面会为所有名字空间设置一个不可变更的 [标签](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/labels/) `kubernetes.io/metadata.name`，只要 `NamespaceDefaultLabelName` 这一[特性门控](https://kubernetes.io/zh-cn/docs/reference/command-line-tools-reference/feature-gates/)被启用。 标签的值是名字空间的名称。

## 6. 注解annotation

>  注解（annotation）可以用来向 Kubernetes 对象的 `metadata.annotations` 字段添加任意的信息。Kubernetes 的客户端或者自动化工具可以存取这些信息以实现其自定义的逻辑

注解和标签一样，是键/值对：

```json
"metadata": {
  "annotations": {
    "key1" : "value1",
    "key2" : "value2"
  }
}
```

类似于下面的信息可以记录在注解中：

- 声明式配置层用到的状态信息。
- Build、release、image信息，例如 timestamp、release ID、git branch、PR number、image hash、registry address
- 日志、监控、分析、审计系统的参数
- 第三方工具所需要的信息，例如 name、version、build information、URL
- 轻量级的发布工具用到的信息，例如，config、checkpoint
- 负责人的联系方式，例如，电话号码、网址、电子信箱
- 用户用来记录备忘信息的说明，例如，对标准镜像做了什么样的修改、维护过程中有什么特殊信息需要记住
- 从用户到最终运行的指令，以修改行为或使用非标准功能。

示例:

```yaml
metadata:
  annotations:
    deployment.kubernetes.io/revision: 7  # 由Deployment控制器添加，用于记录当前发布的修改次数
    k8s.eip.work/displayName: busybox   # Kuboard添加，Deployment显示在Kuboard界面上的名字
    k8s.eip.work/ingress: false     # Kuboard添加，根据此参数显示Deployment是否配置了Ingress
    k8s.eip.work/service: none      # Kuboard添加，根据此参数显示Deployment是否配置了Service
```

## 7. 字段选择器

> 字段选择器（Field Selector）可以用来基于的一个或多个字段的取值来选取一组Kubernetes对象。

下面这个 `kubectl` 命令将筛选出 [`status.phase`](https://kubernetes.io/zh-cn/docs/concepts/workloads/pods/pod-lifecycle/#pod-phase) 字段值为 `Running` 的所有 Pod：

```shell
kubectl get pods --field-selector status.phase=Running
```

**不同的对象可查询的字段不一样**.所有的对象类型都支持的两个字段是 `metadata.name` 和 `metadata.namespace`. 

字段选择器中**可以使用的操作符**有 `=`、`==`、`!=` （`=` 和 `==` 含义相同）

可以指定**多个字段选择器**，用逗号 `,` 分隔。如:

```shell
kubectl get pods --field-selector=status.phase!=Running,spec.restartPolicy=Always
```

字段选择器可以**跨资源类型使用**。如:

```shell
kubectl get statefulsets,services --all-namespaces --field-selector metadata.namespace!=default
```

## 8. Finalizers

> Finalizer 是带有命名空间的键，告诉 Kubernetes 等到特定的条件被满足后， 再完全删除被标记为删除的资源。 Finalizer 提醒[控制器](https://kubernetes.io/zh-cn/docs/concepts/architecture/controller/)清理被删除的对象拥有的资源。

当你告诉 Kubernetes 删除一个指定了 Finalizer 的对象时， Kubernetes API 通过填充 `.metadata.deletionTimestamp` 来标记要删除的对象， 并返回 `202` 状态码(HTTP "已接受") 使其进入只读状态。 此时控制平面或其他组件会采取 Finalizer 所定义的行动， 而目标对象仍然处于终止中（Terminating）的状态。 这些行动完成后，控制器会删除目标对象相关的 Finalizer。 当 `metadata.finalizers` 字段为空时，Kubernetes 认为删除已完成并删除对象。

**如何工作**

删除资源包含`metadata.finalizers`字段,会执行如下操作:

- 修改对象，将你开始执行删除的时间添加到 `metadata.deletionTimestamp` 字段。
- 禁止对象被删除，直到其 `metadata.finalizers` 字段为空。
- 返回 `202` 状态码（HTTP "Accepted"）。

**属主引用**

[属主引用](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/owners-dependents/) (Owner references), 描述了 Kubernetes 中对象之间的关系

例如，当 [Job](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/job/) 创建一个或多个 Pod 时,控制器会为这些 Pod 添加了“属主引用”，指向创建 Pod 的 Job。 如果你在这些 Pod 运行的时候删除了 Job， Kubernetes 会使用属主引用（而不是标签）来确定集群中哪些 Pod 需要清理。

在某些情况下，Finalizers 会阻止依赖对象的删除

## 9. 属主与附属

在 Kubernetes 中，一些对象是其他对象的“属主（Owner）”。 例如，[ReplicaSet](https://kubernetes.io/zh-cn/docs/concepts/workloads/controllers/replicaset/) 是一组 Pod 的属主。 具有属主的对象是属主的“附属（Dependent）”。

**属主引用(Owner references)**

附属对象有一个 `metadata.ownerReferences` 字段，用于引用其属主对象。该字段值包含同一命名空间下的对象名称和UID.

附属对象还有一个 `ownerReferences.blockOwnerDeletion` 字段，该字段使用布尔值， 用于控制特定的附属对象是否可以阻止垃圾收集删除其属主对象。Kubernetes 会自动设置 `blockOwnerDeletion` 的值为 `true`。 你也可以手动设置 `blockOwnerDeletion` 字段的值，以控制哪些附属对象会阻止垃圾收集。











参考:[使用 Kubernetes 对象](https://kubernetes.io/zh-cn/docs/concepts/overview/working-with-objects/)

[Kubernetes API 约定](https://git.k8s.io/community/contributors/devel/sig-architecture/api-conventions.md)

[使用配置文件对 Kubernetes 对象进行声明式管理](https://kubernetes.io/zh-cn/docs/tasks/manage-kubernetes-objects/declarative-config/)