# OAuth2.0

### OAuth2.0 相关概念

#### 角色

1. Resource Owner:资源拥有者,通常指用户
2. Resource Server:资源服务器,保存Resource Owner资源的服务器
3. Client:资源适用方
4. Authorization Server:授权服务器,负责验证client,并提供授权

#### 术语

1. Client id:client 唯一标识
2. Client secret:与Client id配对的唯一秘钥
3. User-Agent:用户浏览器或者app
4. Access token:授权成功后Authorization Server提供的票据.一般格式为JWT或 bear Token
   1. JWT:一个 JWT 字符串分为 Header，Payload，Signature 三部分，他们的原始字符串经过编码后由小数点分隔连接起来
   2. Bearer Token :以数字、大小写字母、破折号、小数点、下划线、波浪线、加号、正斜杠、等号结尾组成的 Base64 编码字符串
5. Refresh token:用于刷新Access token,可选不一定实现

### OAuth2.0 Flow

#### Authorization Code：授权码模式

![image-20220119142407487](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220119142407487.png)

client向server提供client Id,重定向地址等相关信息用于认证;用于确认并确认授权信息;sever 确认验证信息后返回 code;client使用code和重定向地址访问server.server验证code与重定向地址后返回access token.

#### Implicit：隐式授权

wen-hosted client 客户端前端资源容器

![image-20220119142843734](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220119142843734.png)

client向server提供client Id,重定向地址等相关信息用于认证;用于确认并确认授权信息;.server验证client id等信息后与使用重定向地址将token放在fragment中返回.user-agent通过重定向地址访问web resource,并在本地保留fragment;Web-Hosted Client Resource 返回一个网页（通常是带有嵌入式脚本的 HTML），该网页能够提取 URI 中的 Fragment 和其他参数;user-agent解析获取access token;最后传递给client.

#### Resource Owner Password Credentials：密码认证模式

![image-20220119143538654](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220119143538654.png)

resource owner提供用户名密码给client,但是client 不保存只做验证;Client 提供owner用户名密码给server;server 验证后提供access token.

#### Client Credentials：客户端认证模式

Client以自己的名义进行认证,不存在用户授权

![image-20220119143816470](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220119143816470.png)

Client 向 Authorization Server 进行身份认证，并请求 token;Authorization Server 对 Client 信息进行认证，有效则发放 token

#### PKCE(Proof Key for Code Exchange):代码交换证明密钥

t_m 加密算法 code_verifier 通过 t_m算法后生成 code_challenge

![image-20220119143949229](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220119143949229.png)

1. Client 随机生成一串字符并作 URL-Safe 的 Base64 编码处理, 结果用作 code_verifier。
2. 将这串字符通过 SHA256 哈希，并用 URL-Safe 的 Base64 编码处理，结果用作 code_challenge。
3. Client 使用把 code_challenge，请求 Authorization Server，获取 Authorization Code。（步骤 A）
4. Authorization Server 认证成功后，返回 Authorization Code（步骤 B）。
5. Client 把 Authorization Code 和 code_verifier 请求 Authorization Server，换取 Access Token。
6. Authorization Server 返回 token。





参考:

[OAuth2.0 面面观](https://xie.infoq.cn/article/41ac7535056b5000f8e3870cf)

[浅谈CSRF攻击方式](https://www.cnblogs.com/hyddd/archive/2009/04/09/1432744.html)



