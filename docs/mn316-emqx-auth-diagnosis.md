# EMQX MQTT 登录拒绝诊断（2026-11-01）

`Pager_MN316` 拼写正确。已确认根因是沿用归档中的旧 MQTT 账号凭据，
而当前部署使用用户随后确认的新账号。更换凭据后 MQTT 登录成功，COM8 实板发出的 `hahaha` 已由 PC 实际收到。
连接地址、Client ID、证书和主题均无需再改变。本文保留修正前的对照证据。

## 身份与连接信息核对

| 字段 | 核对结果 |
|---|---|
| Host / TLS 端口 | `p235d1e1.ala.cn-hangzhou.emqxsl.cn:8883` |
| WSS 入口 | `wss://p235d1e1.ala.cn-hangzhou.emqxsl.cn:8084/mqtt` |
| MQTT Client ID | `Pager_MN316`，11 个 UTF-8 字节 |
| JSON From | `Pager_MN316` |
| 排查时的旧 MQTT username | `bepio_mn316_5f014590` |
| 修正后的 MQTT username | `Pager_MN316`；用户名与 Client ID 仍是两个独立字段 |
| MQTT password | 不记录；已核对本地文件与构建镜像完全匹配 |
| TLS / SNI | 使用完整 Host；验证 CA、域名、有效期，未关闭证书验证 |
| CONNECT 标志 | `0xC2`：用户名、密码、clean session；无 Will |
| Keep Alive | 60 秒 |
| 收件主题 | `pager/demo/Pager_MN316/inbox` |
| 发给 PC 的主题 | `pager/demo/Pager_PC/inbox` |

`pager_message.h` 定义的设备 ID 被直接编码到 CONNECT 的 Client ID 字段。
已检查 `debug/t_mn316/build/t_mn316.bin` 确实包含该 ID，以及与当前本地配置一致的用户名和密码。
修正前三个 `emqx_local.h` 凭据完全一致，均无首尾空白、控制字符或非 ASCII 字符。
修正后主工程与调试工程使用新凭据，归档文件保留原值。
归档本地凭据文件的修改时间为 2026-10-29，此时间只说明本地文件历史，不证明服务器仍保留该账号。

## 修正前的实际对照连接

只发送 CONNECT，成功时立即 DISCONNECT；未订阅、未发布、未修改服务器认证数据。

| 客户端 / 入口 | 协议 | Client ID | 结果 |
|---|---|---|---|
| ESP32 + MN316 / 8883 | MQTT 3.1.1 / TLS 1.2 | `Pager_MN316` | CONNACK `0x05` |
| PC / 8883 | MQTT 3.1.1 / TLS 1.2 | `Pager_MN316` | CONNACK `0x05` |
| PC / 8883 | MQTT 3.1.1 / TLS 1.3 | 随机诊断 ID | CONNACK `0x05` |
| PC / 8883 | MQTT 5 / TLS 1.2 | `Pager_MN316` | CONNACK `0x87` |
| PC / 8883 | MQTT 5 / TLS 1.3 | 随机诊断 ID | CONNACK `0x87` |
| 归档原始 Python 客户端 / 8883 | MQTT 3.1.1 | `Pager_MN316` | CONNACK body `0005` |
| PC / 8084 | MQTT 5 / WSS | `Pager_MN316` | WebSocket 升级成功，CONNACK `0x87` |

PC 对照时间：2026-11-01 UTC 15:40:55–15:40:58（北京时间 23:40:55–23:40:58）。
WSS 对照时间：UTC 15:43:22（北京时间 23:43:22）。

原始证据保存在调试构建目录（不提交）：

- `emqx-hahaha.log`：设备侧 TLS 成功、MQTT code=5、UART 恢复 9600。
- `emqx-auth-diagnosis.json`：四组 PC 对照及镜像凭据匹配结果。
- `emqx-archive-comparison.json`：归档原始客户端复测结果。
- `emqx-wss-diagnosis.json`：8084 WSS 复测结果。

## 已确认的根因与修复验证

固件此前提交的 username 为旧账号 `bepio_mn316_5f014590`。用户随后确认当前部署的
MQTT username 为 `Pager_MN316`，并提供了对应密码。仅替换本地用户名和密码，
保持 Host、8883 端口、CA、SNI、Client ID 和 CONNECT 编码不变，结果如下：

1. PC MQTT 5 / TLS 1.2 使用 `Pager_MN316` Client ID，CONNACK 从 `0x87` 变为 `0x00`。
2. 新固件烧录至 COM8 后执行 `emqx_send 30 "hahaha"`。
3. MN316 蜂窝 DNS、TCP、TLS 1.2、MQTT 登录和订阅成功。
4. 独立 PC 客户端实际从 `pager/demo/Pager_PC/inbox` 收到：

   ```json
   {"From":"Pager_MN316","To":"Pager_PC","Text":"hahaha","Datetime":"2026-11-01T15:47:21Z","At":null}
   ```

这证明本次失败由旧认证凭据引起，不是 Client ID 拼写、主题缺少前缀或 TLS 故障。
此证据仍不说明旧账号究竟已删除、移到另一部署，还是服务器认证配置变化；未读取服务端认证数据，
不能进一步声称旧账号删除事实。所有修正前 PC 测试都在 CONNECT 阶段被拒绝，且无 Will，
没有提交消息主题，故发布/订阅 ACL 不是当时登录失败的直接原因。

已更新 `debug/t_mn316/main/emqx_local.h` 和 `system/mn316_ctrl/emqx_local.h`。
它们均被 Git 忽略，密码不写入报告。归档账号保持原样作为历史资料。
实际成功日志为 `debug/t_mn316/build/emqx-hardware.log`；
PC 新凭据认证结果为 `debug/t_mn316/build/emqx-auth-updated.json`。
当前实板运行 debug 固件；主工程仅同步配置并构建，其后台 QoS 1 队列仍需单独实测。

EMQX 官方资料将 `not_authorized` 与未匹配到认证用户名关联，将
`bad_username_or_password` 与密码校验失败关联，这指导了优先核对账号的过程。
控制台未显示日志不推翻真实收到 CONNACK 的事实。

## 官方依据

- [默认认证：用户名和密码在部署中单独管理](https://docs.emqx.com/zh/cloud/latest/deployments/default_auth.html)
- [部署日志：认证失败与主题 ACL 拒绝的区别](https://docs.emqx.com/zh/cloud/latest/deployments/logs.html)
- [MQTT 错误码](https://docs.emqx.com/zh/cloud/latest/connect_to_deployments/mqtt_client_error_codes.html)
- [Serverless 端口、认证与 SNI 要求](https://docs.emqx.com/en/cloud/latest/deployments/port_guide_serverless.html)
- [EMQX 原因码及 MQTT 3.1.1 兼容映射源码](https://github.com/emqx/emqx/blob/master/apps/emqx/src/emqx_reason_codes.erl)

运行四组 PC 诊断：`python debug/t_mn316/diagnose_emqx.py`。
该脚本读取本地忽略的 MQTT 凭据，不输出密码，也不发布消息。

2026-11-02 封装说明：当前唯一凭据文件已迁至 `system/mn316_ctrl/emqx_local.h`；
CA 为 `system/mn316_ctrl/certs/emqx_ca.pem`。上文调试目录凭据是排查当时的旧路径，当前调试代码不再读取它。
