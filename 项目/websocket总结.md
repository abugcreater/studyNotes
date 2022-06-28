# websocket总结

## 1. WebSocket是什么

WebSocket是一种浏览器与服务器进行全双工通讯的网络技术，属于应用层协议。它基于TCP传输协议，并复用HTTP的握手通道

1. WebSocket可以在浏览器里使用
2. 支持双向通信
3. 使用很简单

**WebSocket优点**

对比HTTP协议,支持双向通信,更灵活,高效,可扩展性好.

1. 支持双向通信,实时性强
2. 更好的二进制支持
3. 控制开销更少.ws的数据包头部较小,不包含头部的情况下,服务端到客户端包头只有2~10字节,而客户端到服务端需要额外的4字节验码.HTTP协议每次通信都需要携带完整头部
4. 支持扩展.用户可以扩展协议,或者自定义子协议

## 2. 建立连接

WebSocket复用HTTP的握手通道,然后通过HTTP请求与服务端协商升级协议.升级完成后,后续的数据交换遵照WebSocket协议.

### 2.1 客户端:申请协议升级

客户端使用标准HTTP报文格式GET请求申请协议升级,下面的报文省略了cookie等参数

```http
GET / HTTP/1.1
Host: localhost:8080    
Origin: http://127.0.0.1:3000
Connection: Upgrade
Upgrade: websocket
Sec-WebSocket-Version: 13
Sec-WebSocket-Key: w4v7O6xFTi36lq3RNcgctw==

```

各字段意义如下

- `Connection: Upgrade`:表示协议升级

- `Upgrade: websocket`:需要升级的协议为websocket

- `Sec-WebSocket-Version: 13`:表示对应的websocket协议版本号为13

- `Sec-WebSocket-Key`:提供验证,防止恶意/无意连接,与服务端头部`Sec-WebSocket-Accept`对应

### 2.2 服务端:响应协议升级

服务端返回报文,状态代码`101`表示协议切换.完成升级后,后续数据要按新协议来

```http
HTTP/1.1 101 Switching Protocols
Connection:Upgrade
Upgrade: websocket
Sec-WebSocket-Accept: Oy4NRAQ13jhfONC7bP8dTKb4PTU=

```

每个header以`\r\n`结尾,最后加一个额外空行`\r\n`.

`Sec-WebSocket-Accept` 是通过客户端的`Sec-WebSocket-Key`根`258EAFA5-E914-47DA-95CA-C5AB0DC85B11`拼接,之后通过SHA1计算摘要,并转成base64字符串生成.

## 3. 数据传输

WebSocket客户端、服务端通信的最小单位是帧（frame），由1个或多个帧组成一条完整的消息（message）。

 ### 3.1 数据帧格式概览

1. 从左到右，单位是比特。比如`FIN`、`RSV1`各占据1比特，`opcode`占据4比特。
2. 内容包括了标识、操作代码、掩码、数据、数据长度等。（下一小节会展开）

```lua
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+

```

### 3.2 数据帧格式详解

每个字段的意义如下:

**FIN**:消息结尾标志,占据1个比特.如果是1表示是消息的最后一个分片(fragment),如果是0表示不是最后一个分片

**RSV1,RSV2,RSV3**:扩展协议标识,各占1个比特.一般全为0,当采用`ws`扩展协议时,三个标志位可以非0,由扩展协议自定义.如果不全为0,但还是原来的`ws`协议,则连接出错.

**Opcode**:决定如何解析后续的数据载荷(data payload),4个比特.如果操作码不认得,会导致接收端断开连接(fail the connection).可选操作码如下:

- %x0：表示一个延续帧。当Opcode为0时，表示本次数据传输采用了数据分片，当前收到的数据帧为其中一个数据分片。
- %x1：表示这是一个文本帧（frame）
- %x2：表示这是一个二进制帧（frame）
- %x3-7：保留的操作代码，用于后续定义的非控制帧。
- %x8：表示连接断开。
- %x9：表示这是一个ping操作。
- %xA：表示这是一个pong操作。
- %xB-F：保留的操作代码，用于后续定义的控制帧。

**Mask**: 标识是否要对数据载荷进行验码操作,1比特.从客户端向服务端发送数据时,需要进行验码操作;否则不需要.如果服务端收到的数据没有进行过验码操作,则断开链接.

如果标识位是1,则会在Masking-key中定义一个验码键,并用这个验码键对数据载荷进行反掩码.

**Payload length**:数据载荷长度,长度单位是字节.为7位,或7+16位,或1+64位

假设数Payload length === x，如果

- x为0~126：数据的长度为x字节
- x为126：后续2个字节代表一个16位的无符号整数，该无符号整数的值为数据的长度
- x为127：后续8个字节代表一个64位的无符号整数（最高位为0），该无符号整数的值为数据的长度

此外，如果payload length占用了多个字节的话，payload length的二进制表达采用网络序（big endian，重要的位在前）

**Masking-key**:验码,长度为0或4字节.对数据载荷进行验码操作.当Mask为1,则携带4字节的Masking-key,Mask为0,则不携带.

**Payload data**:数据载荷,包括扩展数据和应用数据.其中扩展数据X字节,应用数据Y字节

扩展数据:当没有使用扩展时,长度为0字节.使用扩展必须声明扩展数据长度,扩展的使用在握手阶段协商完成.如果有扩展数据,那么数据载荷长度包含了扩展数据长度.

应用数据:应用数据在扩展数据之后,占据剩余数据帧位置.

### 3.3 验码算法

掩码键（Masking-key）是由客户端挑选出来的32位的随机数

基本概念如下:

- original-octet-i：为原始数据的第i字节。
- transformed-octet-i：为转换后的数据的第i字节。
- j：为`i mod 4`的结果。
- masking-key-octet-j：为mask key第j字节。

算法描述为： original-octet-i 与 masking-key-octet-j 异或后，得到 transformed-octet-i。

> j = i MOD 4
> transformed-octet-i = original-octet-i XOR masking-key-octet-j

## 4. 数据传输

WebSocket建立链接后,后续的操作都是基于数据帧的传递.

WebSocket根据`opcode`来区分操作的类型。比如`0x8`表示断开连接，`0x0`-`0x2`表示数据交互。

### 4.1 数据分片

WebSocket每条消息能被切分成多个数据帧,接收方通过判断`FIN`值,确定是否是最后一个数据帧.当`FIN=1`时表示接受方已经完整接受到数据,可以对数据进行处理,否则还需要继续接受剩余数据.

`opcode`在数据交换时表示数据类型,编码`0x00`比较特殊,表示延续帧(continuation frame),完整的消息的数据帧还没接受完.

### 4.2 分片案例

当客户端向服务端发送两条消息时.

**第一条消息**

`FIN=1`, 表示是当前消息的最后一个数据帧。服务端收到当前数据帧后，可以处理消息。`opcode=0x1`，表示客户端发送的是文本类型。

**第二条消息**

1. FIN=0，opcode=0x1，表示发送的是文本类型，且消息还没发送完成，还有后续的数据帧。
2. FIN=0，opcode=0x0，表示消息还没发送完成，还有后续的数据帧，当前的数据帧需要接在上一条数据帧之后。
3. FIN=1，opcode=0x0，表示消息已经发送完成，没有后续的数据帧，当前的数据帧需要接在上一条数据帧之后。服务端可以将关联的数据帧组装成完整的消息。

**所有报文如下**

```
Client: FIN=1, opcode=0x1, msg="hello"
Server: (process complete message immediately) Hi.
Client: FIN=0, opcode=0x1, msg="and a"
Server: (listening, new message containing text started)
Client: FIN=0, opcode=0x0, msg="happy new"
Server: (listening, payload concatenated to previous message)
Client: FIN=1, opcode=0x0, msg="year!"
Server: (process complete message) Happy new year to you too!

```

## 5. 保持连接

WebSocket为了保持客户端、服务端的实时双向通信，需要确保客户端、服务端之间的TCP通道保持连接没有断开.但是保持连接需要耗费资源,长时间保持连接可以采用心跳实现.

- 发送方->接收方：ping
- 接收方->发送方：pong

ping、pong的操作，对应的是WebSocket的两个控制帧，`opcode`分别是`0x9`、`0xA`

`Netty`中保持心跳的方式如下

```java
ctx.channel().write(new PongWebSocketFrame().retain());
ctx.channel().write(new PingWebSocketFrame().retain());
```

## 6. Sec-WebSocket-Key/Accept的作用

`Sec-WebSocket-Key/Sec-WebSocket-Accept`在主要作用在于提供基础的防护，减少恶意连接、意外连接.

**主要作用如下**

1. 避免服务端收到非法的websocket连接（比如http客户端不小心请求连接websocket服务，此时服务端可以直接拒绝连接）
2. 确保服务端理解websocket连接。因为ws握手阶段采用的是http协议，因此可能ws连接是被一个http服务器处理并返回的，此时客户端可以通过Sec-WebSocket-Key来确保服务端认识ws协议。（并非百分百保险，比如总是存在那么些无聊的http服务器，光处理Sec-WebSocket-Key，但并没有实现ws协议。。。）
3. 用浏览器里发起ajax请求，设置header时，Sec-WebSocket-Key以及其他相关的header是被禁止的。这样可以避免客户端发送ajax请求时，意外请求协议升级（websocket upgrade）
4. 可以防止反向代理（不理解ws协议）返回错误的数据。比如反向代理前后收到两次ws连接的升级请求，反向代理把第一次请求的返回给cache住，然后第二次请求到来时直接把cache住的请求给返回（无意义的返回）。
5. Sec-WebSocket-Key主要目的并不是确保数据的安全性，因为Sec-WebSocket-Key、Sec-WebSocket-Accept的转换计算公式是公开的，而且非常简单，最主要的作用是预防一些常见的意外情况（非故意的）。

> 强调：Sec-WebSocket-Key/Sec-WebSocket-Accept 的换算，只能带来基本的保障，但连接是否安全、数据是否安全、客户端/服务端是否合法的 ws客户端、ws服务端，其实并没有实际性的保证

## 7. 数据掩码的作用

数据验码的作用是增强协议的安全性,但数据验码并不是为了保护数据本身,因为算法公开且简单.除了加密通道本身，似乎没有太多有效的保护通信安全的办法.

数据验码最重要的作用是为了防止早期版本的协议中存在的代理缓存污染攻击（proxy cache poisoning attacks）等问题

### 7.1 代理缓存污染攻击

下面摘自2010年关于安全的一段讲话。其中提到了代理服务器在协议实现上的缺陷可能导致的安全问题。[猛击出处](http://w2spconf.com/2011/papers/websocket.pdf)。

> “We show, empirically, that the current version of the WebSocket  consent mechanism is vulnerable to proxy cache poisoning attacks. Even  though the WebSocket handshake is based on HTTP, which should be  understood by most network intermediaries, the handshake uses the  esoteric “Upgrade” mechanism of HTTP [5]. In our experiment, we find  that many proxies do not implement the Upgrade mechanism properly, which causes the handshake to succeed even though subsequent traffic over the socket will be misinterpreted by the proxy.”
>
> [TALKING]  Huang, L-S., Chen, E., Barth, A., Rescorla, E., and C.
>  Jackson, "Talking to Yourself for Fun and Profit", 2010,

在正式描述攻击步骤之前，我们假设有如下参与者：

- 攻击者、攻击者自己控制的服务器（简称“邪恶服务器”）、攻击者伪造的资源（简称“邪恶资源”）
- 受害者、受害者想要访问的资源（简称“正义资源”）
- 受害者实际想要访问的服务器（简称“正义服务器”）
- 中间代理服务器

攻击步骤一：

1. **攻击者**浏览器 向 **邪恶服务器** 发起WebSocket连接。根据前文，首先是一个协议升级请求。
2. 协议升级请求 实际到达 **代理服务器**。
3. **代理服务器** 将协议升级请求转发到 **邪恶服务器**。
4. **邪恶服务器** 同意连接，**代理服务器** 将响应转发给 **攻击者**。

由于 upgrade 的实现上有缺陷，**代理服务器** 以为之前转发的是普通的HTTP消息。因此，当**协议服务器** 同意连接，**代理服务器** 以为本次会话已经结束。

攻击步骤二：

1. **攻击者** 在之前建立的连接上，通过WebSocket的接口向 **邪恶服务器** 发送数据，且数据是精心构造的HTTP格式的文本。其中包含了 **正义资源** 的地址，以及一个伪造的host（指向**正义服务器**）。（见后面报文）
2. 请求到达 **代理服务器** 。虽然复用了之前的TCP连接，但 **代理服务器** 以为是新的HTTP请求。
3. **代理服务器** 向 **邪恶服务器** 请求 **邪恶资源**。
4. **邪恶服务器** 返回 **邪恶资源**。**代理服务器** 缓存住 **邪恶资源**（url是对的，但host是 **正义服务器** 的地址）。

到这里，受害者可以登场了：

1. **受害者** 通过 **代理服务器** 访问 **正义服务器** 的 **正义资源**。
2. **代理服务器** 检查该资源的url、host，发现本地有一份缓存（伪造的）。
3. **代理服务器** 将 **邪恶资源** 返回给 **受害者**。
4. **受害者** 卒。

附：前面提到的精心构造的“HTTP请求报文”。

```yaml
Client → Server:
POST /path/of/attackers/choice HTTP/1.1 Host: host-of-attackers-choice.com Sec-WebSocket-Key: <connection-key>
Server → Client:
HTTP/1.1 200 OK
Sec-WebSocket-Accept: <connection-key>
```

### 2、当前解决方案

最初的提案是对数据进行加密处理。基于安全、效率的考虑，最终采用了折中的方案：对数据载荷进行掩码处理。

需要注意的是，这里只是限制了浏览器对数据载荷进行掩码处理，但是坏人完全可以实现自己的WebSocket客户端、服务端，不按规则来，攻击可以照常进行。

但是对浏览器加上这个限制后，可以大大增加攻击的难度，以及攻击的影响范围。如果没有这个限制，只需要在网上放个钓鱼网站骗人去访问，一下子就可以在短时间内展开大范围的攻击。





















https://www.cnblogs.com/chyingp/p/websocket-deep-in.html