# Chat

主固件的独立 Chat 应用。页面左侧 30% 显示联系人列表，右侧 70% 显示选中号码的会话；对方发来的气泡靠左，自己发出的气泡靠右。真实消息按本机消息 ID 排列，演示气泡随后显示，并标记 `Demo`。中文文本显示播放按钮，点击后通过 Messages 的后台 TTS 即时合成，不保存音频。英文暂显示文本。

`model` 读取已有 Contacts 和 Messages，并在 Chat 内存中补入 Alex、Home、Bepio Lab 三位演示联系人及七条收发示例；号码已存在时不重复补联系人。演示内容不会写入 Contacts 或 Messages 的 Flash。`controller` 负责选择联系人、读取当前会话和请求播放；`view` 只绘制交互。打开 Chat 默认选择 Alex，切换联系人立即更新右侧。真实联系人无消息时显示 `No messages yet`。发送端的语音转写与 MN316 传输仍待接入。

回归：`python tests/unittest/chat/run.py`。
