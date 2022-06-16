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

WebSocket每条消息能被切分成多个数据帧,接收方通过判断`FIN`值,确定是否是最后一个数据帧.





















https://www.cnblogs.com/chyingp/p/websocket-deep-in.html