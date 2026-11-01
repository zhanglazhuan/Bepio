# Messages

第三个应用，沿用 MVC、app_manager 与串口 shell。消息数据由 `system/services/messaging/`
持有，不依赖应用是否打开。当前 view 已接入 LVGL 屏幕，保留串口 shell。

## 功能

目标消息链路：按住录音键采集语音，实时转写成文本；确认文本后通过 MN316 发送。接收端收到的也是文本，Flash 只保存文本及消息状态。会话中的语音条只是文本消息的播放入口，点击时才实时调用 TTS 合成并播放，音频流用完即弃，不保存录音或合成音频。当前先支持中文；英文语音留待后续。

当前进度：中文文本消息点击语音条可请求后台实时 TTS；手动输入文本可进入待发送队列。主固件尚未接入任意口述转写、按住录音交互；MN316 EMQX 消息传输已接入，配置本地 MQTT 凭据后后台发送排队消息。独立 `debug/t_multinet` 只识别预设命令，不提供自由听写。

- 默认页直接显示收件列表，标题栏右上角 无背景的 `+` 新建；不显示 Inbox / Outbox 页签。
- 屏幕不再显示首页统计、STAY IN TOUCH 或 Offline / stored in memory 文案；串口诊断仍可查看统计。
- 收件箱：最新在前、摘要、未读标记；详情显示全文，打开后标记已读。
- 发件箱：包含草稿、待发送、发送中、已发送和失败记录；失败可重试。
- 屏幕编辑：收件人只能从 Contacts 下拉选择（姓名与号码），不提供号码输入框；支持正文修改、回复、草稿编辑。
- 串口诊断仍保留手动号码和 contacts ID 命令。
- 界面发送图标直接将消息加入队列；详情页不显示删除按钮。串口删除命令仍需确认。
- 删除收件/发件：取消不会改变消息；删除待发送消息会取消排队，发送中禁止删除。

## 常用命令

先关闭当前应用，例如从 Settings 主页执行 `settings back`。

```text
messages home
messages inbox
messages outbox
messages new 8613800000000 "妈妈"
messages text "我到家了。"
messages send
messages confirm
messages outbox
```

确认后进入待发送队列。配置 EMQX MQTT 凭据且蜂窝 TLS/MQTT 连接成功后，后台自动发送；
匹配的 MQTT PUBACK 才会标记已发送（broker 确认，不代表收件人收到）。收件人设备 ID 为纯数字，发送到 `pager/demo/{数字ID}/inbox`。

| 命令 | 操作 |
|---|---|
| `messages contacts` | 浏览联系人列表，不切换应用、不丢失编辑内容 |
| `messages to <contact-id>` | 按联系人 ID 新建消息或修改当前收件人，不必打开 Contacts |
| `messages recipient <phone> [name]` | 修改当前编辑器收件人，保留正文 |
| `messages text "正文"` | 替换正文并显示剩余字节数 |
| `messages save` | 保存草稿，返回发件箱；也可从未保存返回提示中执行 |
| `messages edit <draft-id>` | 继续编辑草稿；排队后正文与收件人不可修改 |
| `messages reply <inbox-id>` | 创建回复，使用原发送人快照，正文为空 |
| `messages show <in|out> <id>` | 查看详情；收件标记已读 |
| `messages delete <in|out> <id>` | 预览并请求删除，随后 confirm 或 cancel |
| `messages retry <outbox-id>` | 失败记录重新排队，保留原消息 ID 和内容 |
| `messages cancel` | 取消确认；在未保存提示中返回编辑 |
| `messages discard` | 放弃编辑内容；不删除已有草稿 |
| `messages back` | 收件详情→收件列表→退出；发件详情→发件列表→收件列表→退出；有未保存编辑时先提示 |

`messages new ""` 可创建尚未填写收件人的草稿；发送时必须有合法设备 ID/号码和非空白正文。
保存/发送遇到满箱错误时，编辑内容仍保留；可以放弃编辑后删除旧消息，或稍后重试。
应用被强制关闭时保留编辑器，重新打开继续编辑；普通返回不会静默丢弃改动。

## 数据约定

- 收件、发件箱各默认 20 条；草稿占发件箱容量，满箱不覆盖旧消息。
- 正文最多 256 个 UTF-8 字节，不截断中文；拒绝非法 UTF-8、ESC 等控制字符。
- 姓名最多 48 字节、设备 ID 最多 24 位，只接受数字。
- 联系人被修改或删除不影响消息保存的姓名、号码快照。
- 时间由传输适配层提供。未同步时明确显示 unknown/unsynchronized，按本地创建/接收顺序排列。
- 主固件把消息正文、状态和去重记录持久化到 Flash 的 `/storage/messages.txt` 与 `/storage/messages.tmp` 两个交替快照；文件采用带序号和校验的十六进制文本编码，可读取旧版单文件快照，不保存 PCM 或其他音频。
- ID 单调递增并跨重启恢复。存储写入失败会向调用方报错；损坏的快照不会静默覆盖。

## 本地 mock 数据

普通 `./build.ps1` 默认包含 Messages mock。刷入 `build/` 的完整镜像后，系统启动时会补入缺少的示例对话，不会清除已有消息；打开 Messages 只负责渲染。可在 Kconfig 中关闭 `BEPIO_MESSAGES_DEMO`。

如果另行启用 `CONFIG_BEPIO_CONSOLE` 和 `CONFIG_BEPIO_MESSAGES_SHELL`，还可使用以下命令：

```text
messages demo_receive remote-1 13900000000 "请早点回家" "爸爸"
messages inbox
messages show in <实际ID>
messages reply <实际ID>
messages text "好的，马上回来。"
messages send
messages confirm
messages demo_send fail
messages retry <实际发件ID>
messages demo_send ok
messages outbox
```

`demo_send` 处理最早的待发送消息。所有模拟收件、发送、失败结果都带 `SIMULATED` 标记，
既不访问网络，也不代表服务端或收件人已收到消息。

## 验证

```powershell
python tests/unittest/messages/run.py
```

覆盖服务、应用、shell 处理函数和联系人集成。上板前另见 `tests/product/README.md`。

## Flash 文本与自动清理

首次使用会挂载 Storage SPIFFS；若该新分区尚未初始化则格式化一次。后续挂载失败不会格式化已有消息。Settings → Storage 可选 3、7 或 14 天，默认 7 天，选择保存在 NVS。服务每小时清理过期的收件、已发送、失败和草稿；排队或发送中的消息保留。系统时钟未可信同步或消息无有效时间戳时不清理，避免误删。

持久化回归：`python tests/unittest/messages_storage/run.py`。首次应用新分区表需完整 `flash`，仅 `app-flash` 不会写入音色。实板 Flash 读写、断电恢复、中文发声与内存峰值仍需验证。

## 屏幕交互与 mock 预览

页面控件由当前页面的上下文持有，跳转时释放旧上下文。页面位置、邮箱和选中消息保存在 model 的临时 `flow` 中；编辑正文和确认状态也由 model 持有，均不作为音频写入 Flash。
`view.c` 只负责注册页面和退出时释放上下文；controller 根据 model 的 `flow` 使用 `page_navigator` 导航。每个页面的 builder 自行调用 `lv_page_create()` 并构建内容；`pages/messages_list.c`、
`messages_detail.c`、`messages_compose.c`、`messages_confirm.c` 分别绘制列表、
会话详情、编辑和确认页。共同的按钮及提示位于 `messages_page_common.c`，
每个页面文件都有对应的 `.h` 声明入口；共同控件声明位于
`messages_page_common.h`，页面上下文结构只在内部的
`messages_page_context.h` 中定义。

320×240 横屏界面默认显示 Inbox，空箱显示 No messages yet。列表按号码合并，同一号码只显示最近一条；点开后按本机消息 ID 顺序展示收发对话，收到的在左、发出的在右。现有消息记录没有 contact ID，联系人改号后旧消息仍按旧号码归组。支持未读标记、回复、草稿编辑和失败重试。
列表直接显示消息，不显示搜索栏。
新建时从联系人下拉列表选择收件人并输入正文，点击标题栏右侧发送图标直接排队。详情页不显示删除按钮。
编辑中返回可保存、放弃或继续编辑。长页面上下滑动，点击输入框使用屏幕键盘，完成键收起键盘。
正文仍受服务的 256 UTF-8 字节限制，超限或非法输入会提示错误。
界面控件采用英文，中文正文以语音条展示；点击时在后台线程实时调用 ESP-TTS 和 ES8311 播放。英文消息暂显示文本，后续接英语引擎。

普通 `./build.ps1` 启动时补入缺少的三条模拟收件和两条模拟发件；已有真实消息会保留，其中 Alex 的中文往返消息显示为语音条。
关闭重开不会重复填充，重启从 Flash 恢复。排队不产生真实网络发送。

Windows 可交互预览及验证见 [messages_preview](../../tests/integration/messages_preview/README.md)。
标题栏新建按钮在列表滚动时保持可见，编辑/确认时隐藏，返回列表时恢复。

截图为实际 LVGL 离屏渲染：[messages-preview.png](../../docs/assets/messages-preview.png)，不是实板照片。


收件人下拉列表通过 Contacts 的只读控制器接口加载，不必打开或切换 Contacts。
选择时以稳定联系人 ID 重新查找姓名/号码，不影响已输入的正文；只允许正文呼出键盘。
未选择收件人时不能进入发送确认。无联系人时禁用下拉和发送入口，提示先在 Contacts 添加；仍可保存未指定收件人的草稿。
回复/草稿按号码匹配当前联系人并自动选中；原号码已不在 Contacts 中则需重新选择后发送。
历史消息与草稿的快照不会因展示下拉列表而直接被修改。截图见 [messages-recipients.png](../../docs/assets/messages-recipients.png)。
