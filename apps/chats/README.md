# Chats

主固件的独立 Chats 应用。页面左侧 30% 显示联系人列表，右侧 70% 显示选中号码的会话；对方发来的气泡靠左，自己发出的气泡靠右。真实消息按本机消息 ID 排列，演示气泡随后显示，并标记 `Demo`。中文文本显示播放按钮，点击后通过 Messages 的后台 TTS 即时合成，不保存音频。英文暂显示文本。

`model` 读取已有 Contacts 和 Messages。启用 `CONFIG_BEPIO_MESSAGES_DEMO` 时，在 Chats 内存中补入 Alex、Home、Bepio Lab 三位演示联系人及七条收发示例；号码已存在时不重复补联系人。生产配置关闭此选项，演示内容不会写入 Contacts 或 Messages 的 Flash。`controller` 负责选择联系人、读取当前会话和请求播放；`view` 只绘制交互。演示构建打开 Chats 默认选择 Alex，切换联系人立即更新右侧。真实联系人无消息时显示 `No messages yet`。

点击气泡可显示该条消息的删除图标：收件气泡右侧、发件气泡左侧。点击图标直接删除该条消息，无二次确认。真实消息从持久化消息服务删除；纯演示气泡只在当前 Chats 会话中隐藏，重新打开演示应用会恢复。

右侧底部固定显示由 `assets/mic_24dp_000000_FILL0_wght400_GRAD0_opsz24.png` 转换的 24×24 麦克风按钮。按住调用 `bepio_chats_voice_input_t.start`，松开调用 `stop` 获取 UTF-8 文本并写入 Messages 发送队列；滑离按钮取消。录音和转写后端须在打开 Chats 后通过 `bepio_chats_controller_set_voice_input()` 注册，并负责临时音频的采集和清理。当前主固件没有自由口述转写后端，按住按钮会提示不可用，不会产生空消息。队列传输至 MN316 尚未接入，界面显示 `Queued for sending`，不宣称已送达。

Chats 的单页位于 `pages/chats_main.c`，由 `lv_page_create()` 构建；页面上下文随屏幕销毁，app 不再持有页面指针。回归：`python tests/unittest/chats/run.py`；实际 LVGL 交互预览：`build/launcher-preview/chats_preview.exe build/launcher-preview`。
