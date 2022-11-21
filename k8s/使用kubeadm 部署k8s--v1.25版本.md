# 使用kubeadm 部署k8s--v1.25版本

## 1. 环境设置(**所有节点执行**)

　1、配置hostname

　　　　方法一、# vim /etc/hostname  配置相应的主机名

　　　　方法二、hostnamectl set-hostname *master*   
　　2、配置时钟，保持所以主机时钟一致。

　　3、　关闭防火墙和selinux　　



```
systemctl stop firewalld
systemctl   disable firewalld
sed -i s/SELINUX=enforcing/SELINUX=disabled/g /etc/selinux/config
setenforce 0 
```

　　4、关闭swap

　　　　禁用交换分区。为了保证 kubelet 正常工作，你必须禁用交换分区。

　　　　# swapoff -a

　　　　# sed -i 's/.*swap.*/#&/' /etc/fstab

　　5、配置repo 安装 containerd、kubelet、kubeadm、kubectl

```
###### 配置docker-ce源安装 containerd
Step 1: 安装必要的一些系统工具
    yum install -y yum-utils device-mapper-persistent-data lvm2
Step 2: 添加软件源信息
    yum-config-manager --add-repo https://mirrors.aliyun.com/docker-ce/linux/centos/docker-ce.repo
Step 3
    sed -i 's+download.docker.com+mirrors.aliyun.com/docker-ce+' /etc/yum.repos.d/docker-ce.repo


######配置kubernetes 源
cat <<EOF > /etc/yum.repos.d/kubernetes.repo
[kubernetes]
name=Kubernetes
baseurl=https://mirrors.aliyun.com/kubernetes/yum/repos/kubernetes-el7-x86_64/
enabled=1
gpgcheck=1
epo_gpgcheck=1
gpgkey=https://mirrors.aliyun.com/kubernetes/yum/doc/yum-key.gpg
EOF
```

## **2.安装及配置（所有节点执行）**

1、部署 containerd

　　Kubernates 中 pod 的运行必须有容器运行环境，以前 kubernetes 是使用 Docker，后面主要使用 containerd 等，只安装 containerd 即可

　　　　a、移除可能安装过的 Docker 包

　　　　　　# yum remove docker docker-client docker-client-latest docker-common docker-latest docker-latest-logrotate docker-logrotate docker-engine

　　　　b、安装 yum-utils

　　　　　　# yum install -y yum-utils

　　　　c、安装 containerd

　　　　　　> # yum install containerd.io -y

　　　　d、配置 containerd

　　　　　　I、设置开机自启和

　　　　　　　　# systemctl enable containerd

　　　　　　　　# systemctl start containerd

　　　　　　II、修改 containerd 使用 systemd　　　



```
containerd  config default > /etc/containerd/config.toml
sed -i 's#k8s.gcr.io#registry.aliyuncs.com/google_containers#g' /etc/containerd/config.toml
sed -i 's/SystemdCgroup = false/SystemdCgroup = true/g' /etc/containerd/config.toml

正常情况下，国内你是拉取不到k8s.gcr.io/pause:3.8镜像的，这个镜像是一切的pod的基础，要么自己手动导入进来，要么改成国内的镜像，通过设置以下配置来覆盖默认的沙盒镜像：

[plugins."io.containerd.grpc.v1.cri"]
  sandbox_image = "kubebiz/pause:3.8"
```

　　　　　　

III、重启 containerd



```
# systemctl daemon-reload
# systemctl enable --now containerd
# systemctl restart  containerd
```



**注意:需要安装 CNI 插件**

安装流程 

1.  [https://github.com/containernetworking/plugins/releases](https://www.orchome.com/fwd?link=https://github.com/containernetworking/plugins/releases) 下载 cni 插件，安装到`/opt/cni/`：

```
$ mkdir -p /opt/cni/bin
$ tar Cxzvf /opt/cni/bin cni-plugins-linux-amd64-v1.1.1.tgz
```

2、部署 kubernetes组件 kubelet、kubeadm、kubectl　　

　　　　a、安装并配置自启动

　　　　　　# yum install -y --nogpgcheck kubelet kubeadm kubectl

　　　　　　# systemctl enable kubelet

 

　　　　b、配置 kubernetes

　　　　　　I、确认iptables 参数 为一：
　　　　　　　　# cat /proc/sys/net/bridge/bridge-nf-call-ip6tables
　　　　　　　　# cat /proc/sys/net/bridge/bridge-nf-call-iptables

　　　　　　II、通用配置项



```
cat <<EOF | sudo tee /etc/modules-load.d/k8s.conf
overlay
br_netfilter
EOF
```

　　　　　　　　# modprobe overlay

　　　　　　　　# modprobe br_netfilter



```
cat <<EOF | sudo tee /etc/sysctl.d/k8s.conf
net.bridge.bridge-nf-call-iptables  = 1
net.bridge.bridge-nf-call-ip6tables = 1
net.ipv4.ip_forward                 = 1
EOF
```

　　　　　　　　# sysctl --system    #生效以上配置

 

　　　　c、安装和设置 `crictl`

　　　　　　# VERSION="v1.23.0"

　　　　　　# curl -L https://github.com/kubernetes-sigs/cri-tools/releases/download/$VERSION/crictl-${VERSION}-linux-amd64.tar.gz --output crictl-${VERSION}-linux-amd64.tar.gz

　　　　　　#crictl config --set runtime-endpoint=unix:///run/containerd/containerd.sock

## 3.**初始化集群**

1、初始化master （只在master节点执行）

　　　　# kubeadm init --image-repository=registry.aliyuncs.com/google_containers --pod-network-cidr=192.168.101.0/24 --service-cidr=192.168.102.0/24 --ignore-preflight-errors=all



```
Your Kubernetes control-plane has initialized successfully!

To start using your cluster, you need to run the following as a regular user:

  mkdir -p $HOME/.kube
  sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
  sudo chown $(id -u):$(id -g) $HOME/.kube/config

Alternatively, if you are the root user, you can run:

  export KUBECONFIG=/etc/kubernetes/admin.conf

You should now deploy a pod network to the cluster.
Run "kubectl apply -f [podnetwork].yaml" with one of the options listed at:
  https://kubernetes.io/docs/concepts/cluster-administration/addons/

Then you can join any number of worker nodes by running the following on each as root:

kubeadm join 192.168.100.50:6443 --token iudv2k.654tkstlx3if0ox0 --discovery-token-ca-cert-hash sha256:e2c640adbc1cc16c3819360ead74e78236de9c1a1190fcc747019924d1d17288 
```

　　2、按照成功提示 执行相应操作（只在master节点执行）：



```
mkdir -p $HOME/.kube
sudo cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config
```

　　

　　3、node节点加入集群（只在node节点执行在master 初始化完成时的提示命令）：

　　　　# kubeadm join 192.168.100.50:6443 --token iudv2k.654tkstlx3if0ox0 --discovery-token-ca-cert-hash sha256:e2c640adbc1cc16c3819360ead74e78236de9c1a1190fcc747019924d1d17288 

 

　　4、查看结果： kubectl get nodes 



```
kubectl get nodes
NAME      STATUS     ROLES           AGE   VERSION
master   NotReady   control-plane   47s   v1.25.2
node1     NotReady   <none>          19s   v1.25.2
node2     NotReady   <none>          2s    v1.25.2
```

　　　　节点加入集群后，起初是`NotReady`的,这是正常的,我们需要进行网络配置，安装 calico



## 4. **配置网络插件 calico**

　1、安装插件：

　　　　# kubectl apply -f https://docs.projectcalico.org/manifests/calico.yaml

　　　　操作完成后稍等一会儿就会自动处于Ready状态。

　　　　[![img](https://img2022.cnblogs.com/blog/638293/202210/638293-20221015230235001-665180636.png)](https://img2022.cnblogs.com/blog/638293/202210/638293-20221015230235001-665180636.png)

 　　　

　　2、如果一直没有变为Ready状态，切查看message 日志一直在报错：cni plugin not initialized

　　　　[![img](https://img2022.cnblogs.com/blog/638293/202210/638293-20221015231045846-370922403.png)](https://img2022.cnblogs.com/blog/638293/202210/638293-20221015231045846-370922403.png) 

 　　　解决办法如下，具体参考：https://github.com/flannel-io/flannel/issues/1236



```
cat <<EOL > /etc/cni/net.d/10-flannel.conflist 
{
  "name": "cbr0",
  "cniVersion": "0.3.1",
  "plugins": [
    {
      "type": "flannel",
      "delegate": {
        "hairpinMode": true,
        "isDefaultGateway": true
      }
    },
    {
      "type": "portmap",
      "capabilities": {
        "portMappings": true
      }
    }
  ]
}
EOL

#查看
cat /etc/cni/net.d/10-flannel.conflist 
ifconfig cni0
```



## 相关指令

查看kubelet运行日志

> journalctl -xefu kubelet

初始化节点

> kubeadm init \
> --image-repository=registry.aliyuncs.com/google_containers \
> --v 6 \
>  --pod-network-cidr=172.17.108.0/16	 \
>  --ignore-preflight-errors=all





## 踩坑

1. 踩过的坑 1.25 k8s 不支持docker 需要卸载docker 并安装containerd
2. containerd 需要安装cni 网络插件
3. 需要更新containerd容器的镜像config内容拉取镜像
4. 查看kubelet日志需要多看几行









参考:[使用kubeadm 部署k8s--v1.25.2版本](https://www.cnblogs.com/weijie0717/p/16795337.html)

[containerd安装](https://www.orchome.com/16586#item-3)