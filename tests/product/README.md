# product

当前启动验证（需实物，尚未执行）：

1. 按 `docs/hardware.md` 核对主控、晶振、供电和串口跳线。
2. 构建 ESP32-S3 目标，通过 ESP-IDF `idf.py -p <PORT> flash` 烧录；通过原生 USB Serial/JTAG 口连接终端（可设 115200）；UART1 GPIO17/18 只接 MN316。
3. 模组预先开机后复位主控，确认出现 `Bepio starting` 和 `MN316 answered AT`。
4. 断开模组 UART 后复位，确认约 2 秒后报告超时，主程序不无限等待。
5. 模组返回 `ERROR` 时，应记录失败而非报告 AT 成功。

后续补充入网、消息、断网恢复与功耗验证；当前尚无这些功能。

Settings 验证（原生 USB shell）：

1. 复位后确认启动日志显示默认 hour_24=1、notifications=1。
2. 执行 `settings set hour_24 0`，再执行 `settings show`，确认值为 0。
3. 执行 `settings about` 后连续两次 `settings back`，应先回主页、再关闭。
4. 执行 `settings show` 重新打开，hour_24 仍为 0。
5. 执行 `settings defaults`，两个值均恢复 1。
6. 输入 `settings set unknown 1` 或 `settings set hour_24 2` 应报错。
7. 修改后复位主控，确认配置恢复默认值（当前未做持久化）。

Contacts 验证（原生 USB shell）：

1. 从 Settings 主页执行 `settings back`，然后 `contacts list`，应显示空列表。
2. 执行 `contacts add "妈妈" 8613800000000`，记录返回 ID。
3. 用 `contacts show <id>` 查看，再用 `contacts update <id> "母亲" 8613800000001` 修改。
4. 重复添加相同号码、输入空姓名或带字母的号码，应拒绝且不改变原记录。
5. 从详情连续执行两次 `contacts back`，应先返回列表、再退出；重新 list 数据仍保留。
6. 执行 `contacts delete <id>`，确认该记录消失，其他 ID 不变；删除未知 ID 应报错。
7. 达到容量后新增应报满；删除后可继续新增，新 ID 不复用旧 ID。
8. 重启主控后联系人清空；实际 UART 和硬件验证尚未执行。

Messages 验证（当前尚未上板执行）：

1. 关闭当前应用，执行 `messages home`，应显示离线、空箱和 RAM-only。
2. `messages new 13800000000 "妈妈"`，`messages text "我到家了。"`。
3. `messages back` 应提示保存/放弃/继续编辑；cancel 后正文保留。
4. save 后在 outbox 找到草稿，edit 该 ID，send 应展示完整收件人与正文。
5. cancel 不发送；重新 send、confirm 后状态为 queued，普通固件不会伪报成功。
6. 使用演示构建 `./build.ps1 -MessagesDemo`，通过 demo_receive 注入测试收件。
7. inbox 列表不清除未读；show in 后清除该条未读；reply 使用原发送人。
8. delete in/out 只打开确认，cancel 保留，confirm 才删除；不能删除 sending。
9. demo_send fail、retry、demo_send ok 后检查明确的 SIMULATED 标签与不变的 ID。
10. 关闭重开仍有消息和草稿；满箱不覆盖旧消息；重启则清空。
11. 验证中文输入的字节计数、256 字节边界、较长 shell 行以及实际 shell 栈余量。


显示滚动性能（本轮尚未上板验证）：

1. 启动日志应显示40MHz、async DMA、double draw buffers、refresh=20ms、touch=10ms。
2. Contacts持续上下滚动10秒，检查滚动和触摸响应。
3. 检查快速划动、突然松手、点开联系人、编辑键盘、左上返回和底边退出是否正确；观察花屏/错色/撕裂。
4. 检查GUI与触摸任务栈余量及内部DMA堆，重复开关应用应无持续泄漏。
5. 显示DMA超时须停止GUI并保留传输内存；触摸读取失败须停止采样并使输入松手。
   错误路径有host替身回归，实际断线/延迟场景仍需受控实板验证。
6. UART1仍保持TX17/RX18、9600 8N1，确认USB/串口日志正常，modem缺失时仍按原有有界超时返回。
