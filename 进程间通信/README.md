# 进程间通信-管道

用管道进行进程间通信

## Features

1.Server端的主管道/tmp/talk/myfifo用来接收Client发送的通信请求

2.Client和Server端交互的包类型分为4种：登录、会话、心跳、退出。

3.Server端的管道类型都是阻塞型。Client的私有读管道、读终端都是非阻塞型，写主管道是阻塞型。

4.心跳包是Server用来判断Client是否掉线。

## 知识点

- 管道只能一端读，一端写

- 用access来判断管道是否建立成功

- Client和Server之间约定的数据包类型是定长的，read/write只需要读定长的数据包就行

- 非阻塞read/write的时候，返回值小于0，需要判断errno的值是否EGAGAIN

- 私有管道是一个临时文件，unlink

## 遗留问题

1. 当多个Client给同一个Client发消息的时候，有可能出现被覆盖现象


