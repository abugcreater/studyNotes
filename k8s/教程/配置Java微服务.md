# 配置Java微服务

> 使用 MicroProfile、ConfigMaps、Secrets 实现外部化应用配置

## 准备开始

### 创建 Kubernetes ConfigMaps 和 Secrets

Kubernetes ConfigMaps、和 Kubernetes Secrets可以去设置环境变量,环境变量的值将注入到微服务中.使用ConfigMaps和secrets的好处是他们能在多个容器间复用,比如赋值给不同的容器中的不同环境变量.

ConfigMaps是存储非机密键值对的API.

Secrets 也是用来存储键值对,区别与ConfigMaps:它针对机密/敏感数据，且存储格式为 Base64 编码。 secrets 的这种特性使得它适合于存储证书、密钥、令牌.

### 从代码外部化配置

为了使配置能根据外部环境的不同而变化,我们需要使用外部化配置.我们使用 Java 上下文和依赖注入（Contexts and Dependency Injection, CDI）、MicroProfile 配置实现该功能.

CDI 提供一套标准的依赖注入能力，使得应用程序可以由相互协作的、松耦合的 beans 组装而成。 MicroProfile Config 为 app 和微服务提供从各种来源，比如应用、运行时、环境，获取配置参数的标准方法。 基于来源定义的优先级，属性可以自动的合并到单独一组应用可以通过 API 访问到的属性。

## 实际操作

### 1. 构建部署Java微服务

首先需要导航到项目目录,该目录包含了MicroProfile 微服务实现,MicroProfile 运行时的配置和 Kubernetes 配置.

```
cd sample-kubernetes-config/start/
```

该目录是最终目录,包含了教程中已完成的代码以供参考.

我们将要部署的两个微服务名称叫做`system`和`inventory`.`system `微服务提供JVM属性,`inventory `将`system`的属性添加到`inventory`中.这两个微服务演示了K8S中两个pods间的应用如何通信.

先使用maven构建项目:

```
mvn package -pl system
mvn package -pl inventory
-- pl后跟模块名称表示对某个某块打包
```

当两个模块都构建完成后,我们需要将他们部署到K8S.

可以去[文档](https://kubernetes.io/docs/concepts/cluster-administration/manage-deployment/)中学习更多的K8S清单.

应用一下K8S配置,会创建`system-deployment`和`inventory-deployment`以及相应的service.

```
kubectl apply -f kubernetes.yaml
```

**kubectl apply：将配置应用于资源。 如果资源不在那里，那么它将被创建。 kubectl apply命令可以第二次运行，因为它只是应用如下所示的配置。 在这种情况下，配置没有改变。 所以，资源没有改变.可只更新现有的资源配置**

```yaml

apiVersion: apps/v1
kind: Deployment
metadata:
  name: system-deployment
  labels:
    app: system
spec:
  selector:
    matchLabels:
      app: system
  template:
    metadata:
      labels:
        app: system
    spec:
      containers:
      - name: system-container
        image: system:1.0-SNAPSHOT
        ports:
        - containerPort: 9080
        readinessProbe:
          httpGet:
            path: /health/ready
            port: 9080
          initialDelaySeconds: 30
          periodSeconds: 10
          timeoutSeconds: 3
          failureThreshold: 1
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: inventory-deployment
  labels:
    app: inventory
spec:
  selector:
    matchLabels:
      app: inventory
  template:
    metadata:
      labels:
        app: inventory
    spec:
      containers:
      - name: inventory-container
        image: inventory:1.0-SNAPSHOT
        ports:
        - containerPort: 9080
        readinessProbe:
          httpGet:
            path: /health/ready
            port: 9080
          initialDelaySeconds: 30
          periodSeconds: 10
          timeoutSeconds: 3
          failureThreshold: 1
---
apiVersion: v1
kind: Service
metadata:
  name: system-service
spec:
  type: NodePort
  selector:
    app: system
  ports:
  - protocol: TCP
    port: 9080
    targetPort: 9080
    nodePort: 31000
---
apiVersion: v1
kind: Service
metadata:
  name: inventory-service
spec:
  type: NodePort
  selector:
    app: inventory
  ports:
  - protocol: TCP
    port: 9080
    targetPort: 9080
    nodePort: 32000
```

### 2. 向微服务发起请求

通过以下指令能检查Pods状态检查是否处于ready状态.该指令提供了给pod赋值标签的操作.

```
kubectl wait --for=condition=ready pod -l app=inventory
kubectl wait --for=condition=ready pod -l app=system
-- wait 等待一种或多种资源的特定状况。
```

当我们看到输出**condition met**时说明我们的微服务已经变为**Ready** 状态,可以被请求了.

使用curl的get请求,同时附上用户名,密码.

```
curl -u bob:bobpwd http://localhost:31000/system/properties
```

请求后可以看到jvm参数返回.同样的在请求inventory服务

库存服务会调用系统服务并将响应数据存储在库存服务中，然后再返回结果.

使用 Kubernetes ConfigMap 修改 X-App-Name: 响应标头。通过运行以下 curl 命令查看它们的当前值：

```
curl -# -I -u bob:bobpwd -D - http://$( minikube ip ):31000/system/properties | grep -i ^X-App-Name:
```

### 3. 修改System服务

系统服务被硬编码为以 system 作为应用程序名称。要使其可配置，您将添加 appName 成员和代码以将 X-App-Name 设置到文件中;需要修改Java代码为:

```java
package system;

// CDI
import javax.enterprise.context.RequestScoped;
import javax.inject.Inject;
import javax.ws.rs.GET;
// JAX-RS
import javax.ws.rs.Path;
import javax.ws.rs.Produces;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;

import org.eclipse.microprofile.config.inject.ConfigProperty;

@RequestScoped
@Path("/properties")
public class SystemResource {

  @Inject
  @ConfigProperty(name = "APP_NAME")
  private String appName;

  @Inject
  @ConfigProperty(name = "HOSTNAME")
  private String hostname;

  @GET
  @Produces(MediaType.APPLICATION_JSON)
  public Response getProperties() {
    return Response.ok(System.getProperties())
      .header("X-Pod-Name", hostname)
      .header("X-App-Name", appName)
      .build();
  }
}

```

这些更改使用 MicroProfile Config 和 CDI 将名为 APP_NAME 的环境变量的值注入到 SystemResource 类的 appName 成员中。 MicroProfile Config 支持许多配置源，从中接收配置，包括环境变量

### 4.修改Inventory微服务

Inventory同样也是硬编码,需要修改相关配置.将其中硬编码代码替换成下列代码:

```
@Inject
  @ConfigProperty(name = "SYSTEM_APP_USERNAME")
  private String username;

  @Inject
  @ConfigProperty(name = "SYSTEM_APP_PASSWORD")
  private String password;
```

这些更改使用 MicroProfile Config 和 CDI 将环境变量 SYSTEM_APP_USERNAME 和 SYSTEM_APP_PASSWORD 的值注入到 SystemClient 类中。

### 5.创建ConfigMap 和 Secret

有以下几种方式修改容器中配置.将使用`Kubernetes ConfigMap`和`Kubernetes secret`设置这些值.这是K8S提供资源向容器提供配置值的一种方式.一个好处是它们可以在多个容器中重复使用，包括为不同的容器分配不同的环境变量

创建ConfigMap 去配置应用名称需要使用以下指令:

```
kubectl create configmap sys-app-name --from-literal name=my-system
```

该指令在集群中创建了一个名称为`sys-app-name`的ConfigMap .他包含了一个值为`my-system`key为`name` 的键值对.`--from-literal`参数允许你将指定键值对存储在ConfigMap中.可以去[ConfigMaps文档](https://kubernetes.io/docs/concepts/configuration/configmap/)查看其它指令



创建一个证书让inventory服务用来校验系统权限,使用以下指令:

```
kubectl create secret generic sys-app-credentials --from-literal username=bob --from-literal password=bobpwd
```

该指令与创建ConfigMap的指令非常相似,唯一不同的是多了`generic`.该词说明了创建的secret是通用的.有许多不同类型的secrets,比如保存容器凭证键值对的等.

### 6. 更新Kubernetes资源

您现在将根据 ConfigMap 和 Secret 中配置的值更新您的 Kubernetes 部署以在您的容器中设置环境变量。编辑 kubernetes.yaml 文件（位于开始目录中）。该文件定义了 Kubernetes 部署。注意 valueFrom 字段。这指定了环境变量的值，并且可以从各种来源进行设置。源包括一个 ConfigMap、一个 Secret 和有关集群的信息。在此示例中，configMapKeyRef 将密钥名称设置为 ConfigMap sys-app-name 的值。同样，secretKeyRef 使用 Secret sys-app-credentials 中的值设置密钥用户名和密码。

替换一下文件内容:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: system-deployment
  labels:
    app: system
spec:
  selector:
    matchLabels:
      app: system
  template:
    metadata:
      labels:
        app: system
    spec:
      containers:
      - name: system-container
        image: system:1.0-SNAPSHOT
        ports:
        - containerPort: 9080
        # Set the APP_NAME environment variable
        env:
        - name: APP_NAME
          valueFrom:
            configMapKeyRef:
              name: sys-app-name
              key: name
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: inventory-deployment
  labels:
    app: inventory
spec:
  selector:
    matchLabels:
      app: inventory
  template:
    metadata:
      labels:
        app: inventory
    spec:
      containers:
      - name: inventory-container
        image: inventory:1.0-SNAPSHOT
        ports:
        - containerPort: 9080
        # Set the SYSTEM_APP_USERNAME and SYSTEM_APP_PASSWORD environment variables
        env:
        - name: SYSTEM_APP_USERNAME
          valueFrom:
            secretKeyRef:
              name: sys-app-credentials
              key: username
        - name: SYSTEM_APP_PASSWORD
          valueFrom:
            secretKeyRef:
              name: sys-app-credentials
              key: password
---
apiVersion: v1
kind: Service
metadata:
  name: system-service
spec:
  type: NodePort
  selector:
    app: system
  ports:
  - protocol: TCP
    port: 9080
    targetPort: 9080
    nodePort: 31000
---
apiVersion: v1
kind: Service
metadata:
  name: inventory-service
spec:
  type: NodePort
  selector:
    app: inventory
  ports:
  - protocol: TCP
    port: 9080
    targetPort: 9080
    nodePort: 32000
```

### 7. 更新改变

当配置变更后需要重新部署使更新生效.先重新打包服务.

然后需要删除老的deployment 然后部署新的deployment ,使用以下命令:

```sh
$ kubectl replace --force -f kubernetes.yaml
deployment.apps "system-deployment" deleted
deployment.apps "inventory-deployment" deleted
service "system-service" deleted
service "inventory-service" deleted
deployment.apps/system-deployment replaced
deployment.apps/inventory-deployment replaced
service/system-service replaced
service/inventory-service replaced
```

使用以下命令检查服务的 pod 状态：

```
kubectl get --watch pods
```

pods状态为已完成部署时重新使用curl命令去获取`X-App-Name`属性,发现已经变更为`my-system`

通过发出以下 curl 请求验证inventory服务现在是否正在使用 Kubernetes Secret 作为凭据：

```
curl http://$( minikube ip ):32000/inventory/systems/system-service
```

到此为止所有配置都已经被修改









参考:[使用 MicroProfile、ConfigMaps、Secrets 实现外部化应用配置](https://kubernetes.io/zh-cn/docs/tutorials/configuration/configure-java-microservice/configure-java-microservice/)

[互动教程 - 配置 java 微服务](https://kubernetes.io/zh-cn/docs/tutorials/configuration/configure-java-microservice/configure-java-microservice-interactive/)

[k8s源码解析 - apply命令的实现](https://blog.csdn.net/u014618114/article/details/105168869)

[命令行工具 (kubectl)](https://kubernetes.io/zh-cn/docs/reference/kubectl/)