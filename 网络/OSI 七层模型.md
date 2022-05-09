# OSI 七层模型

## 基础知识

OSI（Open System Interconnect），即开放式系统互联。 一般都叫OSI参考模型，是ISO（国际标准化组织）组织在1985年研究的网络互联模型。该体系结构标准定义了网络互连的七层框架（物理层、数据链路层、网络层、传输层、会话层、表示层和应用层），即ISO开放系统互连参考模型。在这一框架下进一步详细规定了每一层的功能，以实现开放系统环境中的互连性、互操作性和应用的可移植性。

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/20/ac3ea7262b3c4525d784722dc0f46634~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)



发送方从最高层开始，从上到下按顺序传输数据，每一层接收到由上层处理的数据时，添加该层的首部并可能会对数据进行处理（如表示层）。而接收端则将顺序反过来，从首层开始，将数据的内容与该层对应的首部拆开，传给上一层。

## 应用层

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/21/f117da35365a7b869c5ba3d52438e28a~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

它的作用是为应用程序提供服务并规定应用程序中通讯相关的细节，也就是为应用提供服务。常见的协议有 `HTTP`，`FTP`，`TELNET`、`SMTP` 等。

## 表示层

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/21/63e3c50c9d75dd521be86921a3715f34~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

表示层的作用是将应用处理的信息转换为适合网络传输的格式，或者将来自下一层的数据转换为上层能处理的格式。它主要负责数据格式的转换。具体来说，就是讲设备固有的数据格式转换为网络标准格式。常见的协议有 `ASCII`、`SSL/TLS` 等。

## 会话层

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/21/582d6e6540bb2ab8ca58566fe070c2d1~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

会话层作用是负责建立和断开通信连接（数据流动的逻辑通路），以及数据的分割等数据传输相关的管理。常见的协议有 `ADSP`、`RPC` 等。

## 传输层

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/21/9040cb27a7fc7f75f749f4928653ada9~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

传输层起着可靠传输的作用。只在通信双方节点进行处理，而不需在路由器上处理。此层有两个具有代表性的协议： `TCP` 与 `UDP`。

传输层有一个重要作用，就是指定通信端口。

## 网络层

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/21/52fbe5b28bd36b31618f59316ece7132~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

网络层负责将数据传输到目标地址。目标地址可以使多个网络通过路由器连接而成的某一个地址。因此这一层主要负责寻址和路由选择。主要由 `IP`、`ICMP` 两个协议组成。

## 数据链路层

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/21/b1da31fda546362f1622f35918bfebed~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

该层负责物理层面上互连的节点之间的通信传输。例如与1个以太网相连的两个节点间的通讯。常见的协议有 `HDLC`、`PPP`、`SLIP` 等。

## 物理层

![img](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2017/10/21/981a59733f3d4e1a92ae7db56cba3df9~tplv-t2oaga2asx-zoom-in-crop-mark:1304:0:0:0.awebp)

物理层负责0、1比特流（0、1序列）与电压高低、光的闪灭之间的互换。典型的协议有 `RS 232C`、`RS 449/422/423`、`V.24` 和 `X.21`、`X.21bis` 等。



## 隐私 TCP/IP模型

![Snipaste_2022-01-04_16-26-44.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1f4895a1fd3e4f3cb66324f479e08d81~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp?)



参考:[或许这样能帮你了解 OSI 七层模型](https://juejin.cn/post/6844903505111547918)
