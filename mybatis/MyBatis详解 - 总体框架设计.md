# MyBatis详解 - 总体框架设计

## 1. MyBatis架构概览

![mybatis-y-arch-1](..\img\mybatis-y-arch-1.png)

## 2. 层级介绍

###  2.1 接口层-和数据库交互的方式

MyBatis和数据库的交互有两种方式：

- 使用传统的MyBatis提供的API；
- 使用Mapper接口

####  2.1.1 使用传统的MyBatis提供的API

传递Statement Id 和查询参数给SqlSession 对象，使用 SqlSession对象完成和数据库的交互.

![mybatis-y-arch-2](..\img\mybatis-y-arch-2.png)

该方法是先创建和数据库连接的SqlSession对象,然后根据Statement Id 和参数来操作数据库,虽然该方法简单实用,但是不符合面向对象这个概念.

#### 2.1.2 使用Mapper接口

MyBatis 将配置文件中的每一个`<mapper>` 节点抽象为一个 Mapper 接口，而这个接口中声明的方法和跟`<mapper>` 节点中的`<select|update|delete|insert>` 节点项对应，即`<select|update|delete|insert>` 节点的id值为Mapper 接口中的方法名称，parameterType 值表示Mapper 对应方法的入参类型，而resultMap 值则对应了Mapper 接口表示的返回值类型或者返回结果集的元素类型。

![mybatis-y-arch-3](..\img\mybatis-y-arch-3.png)

MyBatis 会根据相应的接口声明的方法信息，通过动态代理机制生成一个Mapper 实例，我们使用Mapper 接口的某一个方法时，MyBatis 会根据这个方法的方法名和参数类型，确定Statement Id,然后还是使用传统方法与数据库交互.

### 2.2 数据处理层

数据处理层可以说是MyBatis 的核心，从大的方面上讲，它要完成两个功能：

- 通过传入参数构建动态SQL语句；
- SQL语句的执行以及封装查询结果集成`List<E>`

#### 2.2.1 参数映射和动态SQL语句生成

动态语句生成可以说是MyBatis框架非常优雅的一个设计，MyBatis 通过传入的参数值，使用 Ognl 来动态地构造SQL语句，使得MyBatis 有很强的灵活性和扩展性。

参数映射指的是对于java 数据类型和jdbc数据类型之间的转换：这里有包括两个过程：查询阶段，我们要将java类型的数据，转换成jdbc类型的数据，通过 preparedStatement.setXXX() 来设值；另一个就是对resultset查询结果集的jdbcType 数据转换成java 数据类型。

#### 2.2.2 SQL语句的执行以及封装查询结果集成List

动态SQL语句生成之后，MyBatis 将执行SQL语句，并将可能返回的结果集转换成`List<E>` 列表。MyBatis 在对结果集的处理中，支持结果集关系一对多和多对一的转换，并且有两种支持方式，一种为嵌套查询语句的查询，还有一种是嵌套结果集的查询。

### 2.3  框架支撑层

- **事务管理**

事务管理机制的质量也是考量一个ORM框架是否优秀的一个标准

- **连接池管理机制**

为了数据库连接的可复用,连接池设计非常重要

- **缓存机制**

对于查询提供会话级别的数据缓存,将某次查询放置到SqlSession 中,在一段时间内,相同查询返回缓存结果.

- **SQL语句的配置方式**

基于XML和注解两种配置方式,对注解配置SQL 语句提供了有限的支持，某些高级功能还是要依赖XML配置文件配置SQL 语句

### 2.4 引导层

MyBatis 提供两种方式来引导MyBatis ：基于XML配置文件的方式和基于Java API 的方式

## 3. 主要构件及其相互关系

从MyBatis代码实现的角度来看，主体构件和关系如下：

![mybatis-y-arch-4](..\img\mybatis-y-arch-4.png)

主要的核心部件解释如下：

- `SqlSession` 作为MyBatis工作的主要顶层API，表示和数据库交互的会话，完成必要数据库增删改查功能
- `Executor` MyBatis执行器，是MyBatis 调度的核心，负责SQL语句的生成和查询缓存的维护
- `StatementHandler` 封装了JDBC Statement操作，负责对JDBC statement 的操作，如设置参数、将Statement结果集转换成List集合。
- `ParameterHandler` 负责对用户传递的参数转换成JDBC Statement 所需要的参数，
- `ResultSetHandler` 负责将JDBC返回的ResultSet结果集对象转换成List类型的集合；
- `TypeHandler` 负责java数据类型和jdbc数据类型之间的映射和转换
- `MappedStatement` MappedStatement维护了一条`<select|update|delete|insert>`节点的封装，
- `SqlSource` 负责根据用户传递的parameterObject，动态地生成SQL语句，将信息封装到BoundSql对象中，并返回
- `BoundSql` 表示动态生成的SQL语句以及相应的参数信息
- `Configuration` MyBatis所有的配置信息都维持在Configuration对象之中。

















参考:[MyBatis详解 - 总体框架设计](https://pdai.tech/md/framework/orm-mybatis/mybatis-y-arch.html)