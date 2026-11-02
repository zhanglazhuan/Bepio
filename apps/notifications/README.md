# Notifications

手机通知聚合应用，遵循 app/model/view/controller 分层。启动器使用 assets 中 notifications 图标，
经仓库工具转换为 40×40 ARGB8888，并通过应用描述符的 icon 注册。
列表按最近新增/更新排列；卡片左上角的直角三角形表示未读（已读不显示标记），点击打开完整正文，返回回到列表，可删除通知。
MQTT 后台仅写线程安全模型，LVGL 在自己的线程每 500ms 检查数据版本并刷新。

## 手机转发协议

沿用现有 EMQX TLS 连接，手机或转发服务向设备自己的主题发布：
`pager/demo/<设备数字ID>/inbox`。无需新增订阅，联系人操作不影响通知订阅。
主固件收到 Type=notification 时进入本应用，Type=message 或省略 Type 时仍进入 Messages。

```json
{
  "Version": 1,
  "Type": "notification",
  "Id": "11111111-2222-4333-8444-555555555555",
  "From": "101",
  "To": "30962488619320",
  "App": "WeChat",
  "Title": "张三",
  "Text": "下午三点见。",
  "Datetime": "2026-11-02T01:00:37Z",
  "Location": null
}
```

To 必须与本机数字 ID 相同，From 为手机/转发端的数字 ID。
App 必填，最多 64 UTF-8 字节，建议稳定使用应用包名或固定名称。
Title 最多 96 字节，可省略；Text 最多 384 字节、可为空，但 Title 与 Text 不能同时为空。
Datetime 必填，使用既有 ISO 8601 校验；Version 默认 1，Location 默认 null。
通知必须明确写 Type=notification。

Id 建议由手机针对一条原生通知生成 UUID v4，更新/重投复用该 Id。
接收按 From + App + Id 更新：相同内容重投不增加记录、不改变已读状态；内容/时间改变则移到顶部并标未读。
Id 省略时按 `["notification", From, To, App, Title, Text, Datetime]` 的紧凑 JSON 数组计算 SHA-256，
取前 16 字节十六进制并加 legacy: 前缀；JSON 空白和字段顺序不影响 ID。
缺少 Id 的更新若改变内容，会生成新记录；要关联更新，手机需保留 UUID。
显式非法、空串、null Id 仍拒绝。所有消息仍受 960 字节线长上限约束。

## 生命周期和容量

`bepio_notifications_init()` 在 main 显式初始化，应用关闭后仍可在后台接收。
`bepio_notifications_receive()` 为传输无关接收入口；直接调用者须提供合法协议字段。
当前最多保留 16 条，满时淘汰最旧一条。数据仅在 RAM，不写 Flash，设备重启会清空。
这是设备端应用和 MQTT 接收实现，不包含手机伴侣应用、系统通知授权或手机端转发器。

## 验证

- `python tests/unittest/notifications/run.py`
- `python tests/unittest/mn316_mqtt/run.py`
- `cmake --build build/launcher-preview --target notifications_preview -j 6`
- `./build/launcher-preview/notifications_preview.exe build/launcher-preview`
- `./build.ps1` 和 `./build.ps1 -Application debug/t_mn316`

开发固件默认开启 `BEPIO_NOTIFICATIONS_DEMO`，启动时在 RAM 中加载 Mail、Calendar、WeChat、System 四条示例通知，供页面交互预览；可在 menuconfig 关闭，生产配置会自动禁用。示例通知重启后重新出现，删除仅作用于本次启动。GUI 预览使用同一组示例数据。
截图见 docs/assets/launcher-notifications-order.png、notifications-demo.png、notifications-detail.png。
开发固件已烧录到 COM8，Flash 写入校验通过；手机到 EMQX 再到实板的通知转发仍需硬件验证。

通知卡片宽度为容器的 96%，居中排列；列表仅允许纵向滚动，卡片内部不滚动，文本过长省略。

列表卡片支持左滑删除、右滑标记已读。横向位移至少 48px、全程纵向偏移不超过 24px，
且横向位移至少为纵向偏移的两倍时才执行；纵向滚动、短滑和斜滑不会执行操作。
右滑保持列表，不打开详情；已读通知右滑保持已读。滑动识别与界面刷新均在 LVGL 线程，
接触过程中推迟后台刷新，避免替换手指下的卡片。
