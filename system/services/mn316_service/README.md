# MN316 蜂窝服务

`drivers/mn316` 负责 UART1 初始化和 AT 探测。此服务串行化后续 UART1
调用，提供同步、有限等待的 AT 命令、网络注册/附着/IP 检查、OneNET
模组内置 MQTT 登录与发布/订阅，以及 ESP32 上的 TLS/MQTT 测试会话。
`system/controller/bepio_mn316_manager.h` 是主工程的调用入口。

启动只执行 AT 探测。网络和云端连接由调用方按需触发；不要在 LVGL
回调或 ISR 中执行这些阻塞 API。OneNET 密钥、EMQX 用户名和密码由调用方
传入，不从 debug 工程的本地头文件复制。TLS 测试会话仍使用已验证的
EMQX 主机、主题和 CA，结束后关闭连接并恢复 UART 波特率。

`mqtt_publish` 的成功只代表模组接受 AT 命令；业务送达须由平台回执确认。
主固件构建通过不等于实板注册、MQTT 或 TLS 验证通过。
