# 24 | MVC分层及VO类存在的意义

## 为什么要分 MVC 三层开发？

- 分层能起到代码复用的作用
- 分层能起到隔离变化的作用
- 分层能起到隔离关注点的作用
- 分层能提高代码的可测试性
- 分层能应对系统的复杂性

## BO、VO、Entity 存在的意义是什么？

每层都会定义相应的数据对象，它们分别是 VO（View Object）、BO（Business Object）、Entity.

从设计的角度来说，VO、BO、Entity 的设计思路并不违反 DRY 原则，为了分层清晰、减
少耦合，多维护几个类的成本也并不是不能接受的。

管 VO、BO、Entity 的设计违背 OOP 的封装特性，有被随意修改的风险。但 Entity 和
VO 的生命周期是有限的，都仅限在本层范围内，相对来说是安全的。Service 层包含比较
多的业务逻辑代码，所以 BO 就存在被任意修改的风险了。为了使用方便，我们只能做一
些妥协，放弃 BO 的封装特性，由程序员自己来负责这些数据对象的不被错误使用



## 总结


![image-20210727171705855](C:\Users\Administrator\AppData\Roaming\Typora\typora-user-images\image-20210727171705855.png)