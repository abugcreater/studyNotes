# Nginx 原理简述

## 进程模型与架构原理

Nginx 服务器启动后，产生一个 Master 进程（Master Process），Master 进程执行一系列工作后产生一个或者多个 Worker 进程（Worker Processes)(一般与CPU核心数量一致)。Master线程负责接收外界的信号,同时通知worker线程.woker彼此独立相互竞争.

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL3N6X21tYml6X3BuZy9IVjR5VEk2UGpiSlJndklEOHlLTG9RbzJsdkIycFVZTFYxMHdOeUN1dDM3Z1VTN0FiQVNQaWJsa2lhdkFXMHdGaWNnSk10b3ZpY2oxVDRXdVpaajNVMGliNmVBLzY0MA?x-oss-process=image/format,png)

重启Nginx时使用命令`./nginx -s reload`,该命令会让Nginx启动新进程,该进程解析参数后会通知master.master会重新加载配置文件,并启动新的woker进程,然后通知老的worker进程可以光荣退休了.

当我们操作Nginx时,woker进程是相互竞争的.

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL3N6X21tYml6X3BuZy9IVjR5VEk2UGpiSlJndklEOHlLTG9RbzJsdkIycFVZTHdJdERLdXRETGlieUdWdUlzeWlhWnU0VG5pY2w1ZHNzckFhbHpSTll3aWM5aE1WeVkwSE5UdGNGSkEvNjQw?x-oss-process=image/format,png)

worker进程由master进程fork出来,master进程需要建立好 listen 的 socket（listenfd）之后,再fork出worker进程.所有 Worker 进程的 listenfd 会在新连接到来时变得可读,为了保证只有一个进程处理该连接,worker在注册Linster读事件时需要获取互斥锁accept_mutex.Worker 进程在 accept 这个连接之后，就开始读取、解析、处理请求，在产生数据后再返回给客户端，最后才断开连接.一个请求是由worker进程完成.

优点:worker进程是独立的进程在处理时不需要加锁,并且不会相互影响.

除了master和worker进程外还有:缓存加载器进程（Cache Loader ）和 缓存管理器进程（Cache Manager）

Cache Loader :Nginx启动时创建,在重建缓存元数据后退出.

Cache Manage:存在与整个生命周期,负责对缓存进行管理.

目前Nginx进程模型如下:

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL3N6X21tYml6X3BuZy9IVjR5VEk2UGpiSlJndklEOHlLTG9RbzJsdkIycFVZTHdtN284dE90aWFKRWJwaHZ5RmhCVUVEV2hpYllzSU1ja0lsaWJrNVJyc2Q1NGVyaWNjOWJ5MkpYc1EvNjQw?x-oss-process=image/format,png)

## 热升级

第一步是把旧的 Nginx binary 文件替换为新的,但是通常替换binary文件不够,新编译的文件必须要指定config配置及log文件等.

第二步，我们向现有老的 Master (Old) 进程发生  USR2 信号，之后 Master (Old) 进程会将修改 pid 文件名，添加 后缀 .oldbin.这一步是因为新的master需要使用pid.bin文件.

第三步，使用新的 binary 文件启动新的 Master (New) 进程.new其实是master(old)进程的子进程,不过这个子进程是使用了新的 binary 文件带入来启动的

第四步，向 Master(Old)  进程发送 QUIT 信号.通过ps或.oldbin 文件查找获取

在此期间,Master(Old)  进程是一直存活的,当出现问题时方便回滚.向 Master(Old)  进程发送 HUP 信号,相当于执行了一次reload.然后向 Master (New)发送QUIT信号.

## 模块化

高度模块化的设计是 Nginx 的架构基础.内核的设计非常微小和简洁，完成的工作也非常简单。当它接到一个 HTTP 请求时，它仅仅是通过查找配置文件将此次请求映射到一个 location block，而此 location 中所配置的各个指令则会启动不同的模块去完成工作.Nginx的各种功能和操作都由模块来实现.

模块从结构上分为：

- 核心模块（HTTP模块、EVENT模块和MAIL模块）

- 基础模块（HTTP Access模块、HTTP FastCGI模块 公共网关、HTTP Proxy模块 代理和HTTP Rewrite模块 重定向）

- 第三方模块（ HTTP Upstream Request Hash模块 负载均衡、Notice模块和HTTP Access Key模块 权限）

模块从结构上分为：

- Handlers（处理器模块）：此类模块直接处理请求，并进行输出内容和修改 headers 信息等操作。Handlers 处理器模块一般只能有一个。

- Filters（过滤器模块）：此类模块主要对其他处理器模块输出的内容进行修改操作，最后由 Nginx 输出。

- Proxies（代理类模块）：此类模块是 Nginx 的 HTTP Upstream 之类的模块，这些模块主要与后端一些服务比如FastCGI 等进行交互，实现服务代理和负载均衡等功能
  

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL3N6X21tYml6X3BuZy9IVjR5VEk2UGpiSlJndklEOHlLTG9RbzJsdkIycFVZTG51MnRtUWJ3T3pPVXpPMld0NkpubjF6Qmc3MExpYmgyVXYwRkRYdTNXTlREM1FlMzJ5RHdBaFEvNjQw?x-oss-process=image/format,png)

## 常用使用场景

### 正向代理

隐藏访问的客户端,由Nginx代替客户端去访问.

配置信息

```

resolver 114.114.114.114 8.8.8.8;
server {
	resolver_timeout 5s;
	listen 81;
	location / {
		proxy_pass http://$host$request_uri;
	}
}
```

### 反向代理

隐藏访问的服务端,客户端的访问打到Nginx上,转发给服务器

配置信息
```
server {
      listen      80;
      server_name  localhost;
      location / {
          proxy_pass http://www.google.com;

          proxy_set_header Host $http_host;
          proxy_set_header X-Real-IP $remote_addr; #获取客户端真实IP
      }
  }
```

### 负载均衡

而Nginx目前支持自带3种负载均衡策略，还有2种常用的第三方策略：

1. 轮询（RR）：默认的策略。
2. 权重（weight）：可以给不同的后端服务器设置一个权重值（weight）
3. ip_hash：每个请求按照发起客户端的 ip 的 hash 结果进行匹配
4. fair：智能调整调度算法，动态的根据后端服务器的请求处理到响应的时间进行均衡分配,需要配置upstream_fair 模块
5. url_hash：按照访问的 URL 的 hash 结果分配请求，每个请求的 URL 会指向后端固定的某个服务器

配置信息
```
upstream #自定义组名 {
    server x1.google.com;    #可以是域名
    server x2.google.com;
    #server x3.google.com
                            #down         不参与负载均衡
                            #weight=5;    权重，越高分配越多
                            #backup;      预留的备份服务器
                            #max_fails    允许失败的次数
                            #fail_timeout 超过失败次数后，服务暂停时间
                            #max_coons    限制最大的接受的连接数
                            #根据服务器性能不同，配置适合的参数
    
    #server 106.xx.xx.xxx;        可以是ip
    #server 106.xx.xx.xxx:8080;   可以带端口号
    #server unix:/tmp/xxx;        支出socket方式
}

```

### HTTP服务器

Nginx本身也是一个静态资源的服务器

``` 
 server {
      listen      80;
      server_name  localhost;
      location / {
              root  /root/website/;  #静态资源路径
              index  index.html;	#首页
          }
  }
```

### 动静分离

动静分离是让动态网站里的动态网页根据一定规则把不变的资源和经常变的资源区分开来

```
upstream dynamic_server{
      server 192.168.0.2:8080;
      server 192.168.0.3:8081;
  }
  server {
      listen      80;
      server_name  localhost;
      location / {
          root   /root/website/;
          index  index.html;
      }
      # 所有静态请求都由nginx处理，存放目录为html
      location ~ \.(gif|jpg|jpeg|png|bmp|swf|css|js)$ {
          root     /root/website/;
      }
      # 所有动态请求都转发给tomcat处理
      location ~ \.(jsp|do)$ {
          proxy_pass  http://dynamic_server;
      }
      error_page  500 502 503 504  /50x.html;
      location = /50x.html {
          root  /root/website/;
      }
  }
```












参考:[原创 | Nginx 架构原理科普](https://blog.csdn.net/u013256816/article/details/105591839?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522164991711016782094834010%2522%252C%2522scm%2522%253A%252220140713.130102334.pc%255Fblog.%2522%257D&request_id=164991711016782094834010&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~blog~first_rank_ecpm_v1~rank_v31_ecpm-2-105591839.nonecase&utm_term=nginx&spm=1018.2226.3001.4450)

[Nginx event框架](https://cloud.tencent.com/developer/article/1387320)