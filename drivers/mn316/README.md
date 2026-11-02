# MN316 硬件与传输驱动

驱动只处理 UART、AT、蜂窝网络、TCP Socket 和经过证书验证的 TLS 字节流。
不包含 EMQX 地址、MQTT 凭据、设备 ID、主题或消息 JSON；这些由
[`system/mn316_ctrl`](../../system/mn316_ctrl/README.md) 管理。

| 文件 | 职责 |
|---|---|
| `mn316.c` | 板级 UART 配置、启动 AT 探测 |
| `mn316_control.c` | 模组互斥锁、有限等待的 AT 命令、注册/附着/IP 检查 |
| `mn316_network.h` | 同时解析有空格、无空格的 CEREG/CGATT/CGPADDR 回应 |
| `mn316_transport.c` | DNS、TCP URC、网络校时、mbedTLS BIO、资源清理及波特率恢复 |
| `mn316.h` | 上层可用的驱动与 TLS 字节流 API |
| `mn316_internal.h` | 复合 AT 事务用的锁接口，调用者必须先持锁 |

接线引用 `boards/esp32s3/bepio_devkit.h`：UART1 TX=17、RX=18，默认 9600 8N1；
不改变原生 USB GPIO19/20。TLS 会话临时切至 115200、关闭 STANDBY，退出恢复配置，
不写 NV。会话中 ESP32 复位可能使波特率不同步，需要重新给模组上电。

`bepio_mn316_driver_tls()` 接收上层传入的 host、port、CA 和回调，独占模组直到回调返回。
回调取得的字节流只能在回调期间使用；read 返回字节数、EOF 或负 errno，
write 可能只消费部分数据。其他 AT 请求在锁等待超时后返回 `-EBUSY`。

保留已验证的 32 KiB TLS 收流缓冲，单条自动 NSORF 最大 2048 二进制字节，
行缓冲为其十六进制编码加 128 字节头部；超长、坏帧、接收溢出均关闭会话。
运营商时间无效时拒绝握手，始终验证 CA、域名、有效期并发送 SNI。

TLS 前先查询 `AT+MQTTSTAT?`：0 表示模组内置客户端未初始化，跳过 DISC/DEL；
1/2 仅删除已初始化或已断开的客户端，3..7 先断开再删除，并确认回到 0。
查询或清理失败仍会报错并停止本次连接。原厂 AT 手册第 132 页定义 CME 8002
为“操作不允许，通常为状态机不符合”；它不是 EMQX CONNACK 授权错误。
模组明确拒绝命令时不再重复打印成“failed/timeout”，真实等待超时仍单独记录。
ESP32 单独复位后，MN316 可能保留已分配的 TCP socket。若创建 socket 返回
CME 8002，驱动查询 `AT+NSOCFG?`，只关闭查询到的 socket，再重试创建；
模组仍未立即释放时交由后台会话按 10 秒间隔重试。实板复位复测中查询到
`mask=0x3`，清理后下一轮完成 TCP、TLS 和 MQTT 上线。

`tests/unittest/mn316_mqtt/run.py` 覆盖网络回应解析及上层字节流分段、超时行为，
不证明 FreeRTOS 调度、真实无线网络或硬件 TLS 正确。迁移前实板记录见
[`debug/t_mn316`](../../debug/t_mn316/README.md)。
