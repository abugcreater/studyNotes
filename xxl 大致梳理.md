xxl 大致梳理

通过springboot的自动装配,在启动时获取job任务,向admin注册,admin将任务写入到mysql表中.
将job装饰成jobThread保存在一个map中,当遇到channel调用时,根据类型从map中取出,先写入日志,然后执行任务
详细调用.并触发回调线程,负责将结果写入到日志.