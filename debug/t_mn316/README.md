# MN316 EMQX 消息测试

独立 ESP-IDF 测试工程。直接引用 `boards/esp32s3/bepio_devkit.h` 的
UART 端口、TX/RX 和波特率定义，复用 `drivers/mn316` 与 `system/mn316_ctrl`，不保留另一份通信实现。
启动后最多尝试三次 AT 探测，随后启动 UART0 控制台。此工程不依赖 archive。

复制 `system/mn316_ctrl/emqx_local.example.h` 到同目录 `emqx_local.h`，填入普通 MQTT 账号和密码。
这是主工程和调试工程唯一使用的凭据文件；旧 `debug/t_mn316/main/emqx_local.h` 不再读取。
本地凭据已被 Git 忽略。服务地址与 CA 沿用蜂窝服务，使用验证证书的 TLS 8883。

```powershell
./build.ps1 -Application debug/t_mn316
```

烧录与监视请显式选择实际端口：

```powershell
cd debug/t_mn316
idf.py -p <PORT> flash monitor
```

ESP32-S3 GPIO17 TX 接 MN316 RX，GPIO18 RX 接 MN316 TX，两板共地。
控制台默认走 UART0；MN316 使用 UART1 9600 8N1。`PASS` 只证明
收到完整 AT `OK`，不证明注册、附着、IP 或 MQTT/TLS 连接。

## 发送到数字测试 ID

- 当前设备 ID 从 eFuse MAC 转为十进制，启动日志显示该数字 ID。
- 订阅：`pager/demo/{本机数字ID}/inbox`。
- 消息 To：`100000000000002`；发布主题：`pager/demo/100000000000002/inbox`。
- 统一格式：`pager/demo/{数字ID}/inbox`。

| 接收者 | 必须订阅的收件箱 | 谁向此主题发布 |
|---|---|---|
| 本机数字 ID | `pager/demo/{本机数字ID}/inbox` | PC 发送给设备 |
| 100000000000002 | `pager/demo/100000000000002/inbox` | 设备发送给 PC |

PC 若订阅设备的 inbox，看不到发给 PC 的上行消息。Client ID 不会替代显式订阅。
默认消息不保留；订阅应在发送前成功，测试结束后设备断开，也不会继续出现在在线客户端列表。
`test_emqx.py` 的 `PC_RECEIVE_PASS` 仅说明独立测试订阅者收到，不能代表用户的实际 PC 程序收到。

电脑先订阅 `pager/demo/100000000000002/inbox`，再在 `mn316>` 控制台执行：

```text
check
emqx_send 30
emqx_send 5 hahaha
```

`check` 检查 AT、信号、注册、附着和 IP。`emqx_send [1..600] [text]` 等待入网，
完成 TLS/MQTT 登录和订阅后发送一次指定正文（默认 `MN316 TLS test message`），再监听指定秒数。
此命令复用有界诊断会话：上行为 QoS 0、非保留；有效下行会自动回显。
发送 JSON 使用 `From`、`To`、`Text`、`Datetime`、`At`，无定位时 `At: null`。
成功写入 TLS 不证明 PC 收到，应以 PC 实际接收为准。

自动开启独立 PC 校验订阅者，并从串口触发实板发送：

```powershell
python debug/t_mn316/test_emqx.py --port COM8 --device-id <本机数字ID>
```

脚本通过电脑网络连接 broker；设备通过 MN316 蜂窝 TCP/TLS 发送，不使用 Wi-Fi。
PC 收到并核对 From/To/Text 后打印 `PC_RECEIVE_PASS`，会话正常退出后打印最终 PASS。
串口及成功收件日志位于 `build/emqx-hardware.log`，不会保存凭据。

会话临时将模组 UART 切换为 115200，退出恢复 9600；若中途复位导致不同步，需给模组重新上电。
此工程烧录会替换主应用和分区表，但不擦除或烧录模型分区；恢复主工程需要重新烧录主工程。

## 2026-11-01 首轮实板复测（旧凭据，COM8）

已构建并烧录，设备名为 `Pager_MN316`，目标为 `Pager_PC`，主题使用完整设备 ID。
`check` 返回 CSQ=22、CEREG=0,1、CGATT=1，并取得有效蜂窝 IPv4。
修复无空格 AT 回应的入网解析，以及自动 NSORF 数据超过旧 3099 字节行缓冲的问题；
接收上限现在为 2048 二进制字节，行缓冲为其十六进制编码加 128 字节头部。

蜂窝 DNS、TCP 8883 和 TLS 1.2 均通过，使用
`TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256`，CA、主机名与有效期校验成功。
随后 broker 对设备返回 `MQTT CONNACK rejected code=5`（未授权）。独立 PC 校验客户端
也返回 CONNACK 0x05，因此本次未发布消息，未验证 PC 收件。
连接地址正确不代表 MQTT 账号获授权，仍需核对当前 MQTT 用户名、密码和 Client ID 授权策略。
退出后已恢复 UART 9600；完整设备日志见 `build/emqx-device-attempt.log`。

调试工程、主工程构建及 `python tests/unittest/mn316_mqtt/run.py` 通过。

详细认证排查见 [EMQX 登录诊断](../../docs/mn316-emqx-auth-diagnosis.md)。
`python debug/t_mn316/diagnose_emqx.py` 可复现四组不发布消息的 PC 登录对照。

## 更新 MQTT 凭据后的成功复测（COM8）

用户确认当前 MQTT 账号后，替换主工程及调试工程本地忽略文件中的旧凭据，归档保留原值。
使用新凭据，PC MQTT 5 登录返回 CONNACK `0x00`。重新构建并烧录本工程后执行
`emqx_send 30 "hahaha"`，设备 TLS、MQTT 登录和订阅通过。
独立 PC 客户端从 `pager/demo/Pager_PC/inbox` 实际收到 From=`Pager_MN316`、
To=`Pager_PC`、Text=`hahaha`，Datetime=`2026-11-01T15:47:21Z`、At=null。
收到 PINGRESP，会话正常退出且 UART 恢复 9600，`EMQX_SEND_DONE result=0`。

成功日志：`build/emqx-hardware.log`。根因为沿用了旧 MQTT 账号凭据，Client ID 一直正确。

2026-11-02：硬件迁入 `drivers/mn316`，软件、CA 和凭据集中于独立组件 `system/mn316_ctrl`。
当前迁移版已完成主机测试和构建；迁移后的实板验证与主工程 Contacts 收发尚未执行。
