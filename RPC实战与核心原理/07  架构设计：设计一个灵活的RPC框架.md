# 07 | 架构设计：设计一个灵活的RPC框架

### RPC 架构

构设计呢，就是从顶层角度出发，厘清各模块组件之间数据交互的流程，让我们对系统有一个整体的宏观认识。

RPC 本质上就是一个远程调用,有多种传输协议可选,为了屏蔽网络传输的复杂性，我们需要封装一个单独的数据传输模块用来收发二进制数据，这个单独模块我们可以叫做传输模块。

用户请求的时候是基于方法调用,参数对象需要序列化,单纯的序列化还不够还需要分割不同的请求,这个过程我们叫做协议封装。将序列化和协议封装放在同一模块,统称协议模块.

如果希望RPC接口变成spring bean,统一被Spring Bean Factory 管理，可以在项目中通过 Spring 依赖注入到方式引用。RPC 调用的入口，我们一般叫做 Bootstrap 模块。

上述操作完成了**一个点对点（Point to Point）版本的 RPC 框架**

调用方找到所有服务提供方,并维护提供者地址,这就是我们常说的“服务发现”。

服务发现的地址是静态数据,需要通过连接管理维护TCP的连接状态.

提供方管理服务需要服务治理:服务提供方权重的设置、调用授权等一些常规治理手段。

![image-20220110144958956](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220110144958956.png)

### 可扩展的架构

基于Java SPI机制:需要在 Classpath 下的 META-INF/services 目录里创建一个以服务接口命名的文件，这个文件里的内容就是这个接口的具体实现类。

缺点:不能按需加载,所有实现类都会实例化;获取实现类方式不灵活;并发时使用ServiceLoader 不安全

可以参考spring boot的`@conditional`方法进行插件管理.

![image-20220110145233088](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20220110145233088.png)

**这时，整个架构就变成了一个微内核架构**，我们将每个功能点(指插件)抽象成一个接口，将这个接口作为插件的契约，然后把这个功能的接口与功能的实现分离并提供接口的默认实现。

参考:

[Java常用机制 - SPI机制详解](https://pdai.tech/md/java/advanced/java-advanced-spi.html)

[Spring Boot @Condition 注解](https://juejin.cn/post/6844903903860015111#heading-17)