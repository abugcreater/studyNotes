# 使用 kubectl 创建 Deployment

### Kubernetes 部署

在K8S上部署容器化应用,需要创建 Kubernetes **Deployment** 配置.Deployment 指挥 Kubernetes 如何创建和更新应用程序的实例。创建 Deployment 后，Kubernetes master 将应用程序实例调度到集群中的各个节点上。

Kubernetes Deployment 会持续监控这些事例,如果实例的节点关闭或被删除,则 Deployment 控制器会将该实例替换为集群中另一个节点上的实例。 **这提供了一种自我修复机制来解决机器故障维护问题。**

![img](https://d33wubrfki0l68.cloudfront.net/8700a7f5f0008913aa6c25a1b26c08461e4947c7/cfc2c/docs/tutorials/kubernetes-basics/public/images/module_02_first_app.svg)

K8S集群概况如上图.

使用Kubernetes命令行 **Kubectl** 创建和管理 Deployment.。Kubectl 使用 Kubernetes API 与集群进行交互。

## 实际上手

#### 1. kubectl 基础

kubectl 命令的常用格式是：kubectl action resource.这会对指定的资源（如节点、容器）执行指定的操作（如创建、描述）.可以通过`--help`参数显示相关命令.

首先获取kubectl配置:`kubectl version`

返回结果如下:

```json

Client Version: version.Info{Major:"1", Minor:"20", GitVersion:"v1.20.4", GitCommit:"e87da0bd6e03ec3fea7933c4b5263d151aafd07c", GitTreeState:"clean", BuildDate:"2021-02-18T16:12:00Z", GoVersion:"go1.15.8", Compiler:"gc", Platform:"linux/amd64"}
Server Version: version.Info{Major:"1", Minor:"20", GitVersion:"v1.20.2", GitCommit:"faecb196815e248d3ecfb03c680a4507229c2a56", GitTreeState:"clean", BuildDate:"2021-01-13T13:20:00Z", GoVersion:"go1.15.5", Compiler:"gc", Platform:"linux/amd64"}

```

该返回结果包括了客户端和服务端的版本信息.

如果想要获取集群中的节点信息可以通过`kubectl get nodes`

返回结果

```
NAME       STATUS   ROLES                  AGE     VERSION
minikube   Ready    control-plane,master   4m24s   v1.20.2
```

我们能看到当前存活的节点信息.

#### 2.部署我们的应用

我们需要通过`kubectl create deployment`命令部署应用.除此之外,我们还需要提供部署名称和应用程序镜像位置.

`kubectl create deployment kubernetes-bootcamp --image=gcr.io/google-samples/kubernetes-bootcamp:v1`

上述命令部署了`kubernetes-bootcamp`应用,该应用的镜像位置是`gcr.io/google-samples/kubernetes-bootcamp:v1`

除上述操作外我们还可以:

- 搜索可以运行应用程序实例的合适节点
- 安排应用程序在该节点运行
- 跟进新节点需要配置集群调度



使用`get deployments`展示所有信息:`kubectl get deployments`

```
NAME                  READY   UP-TO-DATE   AVAILABLE   AGE
kubernetes-bootcamp   1/1     1            1           19s
```

我们看到有一个部署应用了的单个实例在容器中运行.

#### 3. 查看我们的应用

在 Kubernetes 中运行的 Pod 运行在一个私有、隔离的网络上。默认情况下，它们对同一 kubernetes 集群中的其他 pod 和服务可见，但在该网络之外不可见。当我们使用 kubectl 时，我们通过 API 端点进行交互以与我们的应用程序进行通信。

我们需要使用其他选项去暴露我们的应用到kubernetes 集群外.

`kubectl`命令能创建代理去帮助我们把通信转发到集群外.

打开第二个终端,并输入以下命令:

```
echo -e "\n\n\n\e[92mStarting Proxy. After starting it will not output a response. Please click the first Terminal Tab\n"; 
kubectl proxy
```

现在我们在host和k8s集群中创建了链接代理支持从这些终端直接访问 API。

`curl http://localhost:8001/version` curl访问,会返回相关信息.

API Server 将根据 pod 名称自动为每个 pod 创建一个端点，该端点也可以通过代理访问。 首先我们需要**获取 Pod 名称**，我们将存储在环境变量 POD_NAME 中：

```
export POD_NAME=$(kubectl get pods -o go-template --template '{{range .items}}{{.metadata.name}}{{"\n"}}{{end}}') echo Name of the Pod: $POD_NAME
```

此时,Pod名称已经被被存储到变量**POD_NAME **中了,您可以通过运行以下 API 访问 Pod：

```
curl http://localhost:8001/api/v1/namespaces/default/pods/$POD_NAME/
```

为了在不使用代理的情况下访问新部署，需要一个服务







参考:[使用 kubectl 创建 Deployment](https://kubernetes.io/zh-cn/docs/tutorials/kubernetes-basics/deploy-app/deploy-intro/)