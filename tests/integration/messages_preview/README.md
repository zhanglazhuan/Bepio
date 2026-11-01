# Messages 实际 LVGL 主机预览

编译真实 messaging service、model、controller、view，采用单线程 host mutex。
不依赖开发板，使用和演示固件相同的 mock fixtures；不访问蜂窝网络。

在仓库根目录执行（需本机 CMake、MinGW，以及已获取的 managed_components/lvgl__lvgl）：

```powershell
cmake -S tests/integration/messages_preview -B build/messages-preview -G "MinGW Makefiles" -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/mingw64/bin/g++.exe -DCMAKE_ASM_COMPILER=C:/mingw64/bin/gcc.exe -DCMAKE_MAKE_PROGRAM=C:/mingw64/bin/mingw32-make.exe
cmake --build build/messages-preview -j 8
./build/messages-preview/messages_preview.exe --interactive
```

Windows 原生窗口以 2 倍显示 320×240 屏幕。鼠标点击相当于触摸，滚轮上下滚动，
也支持按住鼠标拖动页面。点击输入框呼出屏幕键盘，标题右上角的发送图标用于提交消息。
通过 Back 返回上一级。关闭窗口退出，重新启动重置 mock 数据。

运行自动检查与截图：

```powershell
./build/messages-preview/messages_preview.exe build/messages-preview
python tests/unittest/messages/run.py
```

自动检查向真实 LVGL 对象发送事件，覆盖默认列表、已读、新建、编辑、
点击发送图标直接排队，并确认会话详情没有删除按钮；输出 home/inbox/detail/compose/queued.ppm。
`docs/assets/messages-preview.png` 是上述页面的拼图。
这些检查不证明实板触摸、SPI、FreeRTOS 调度或真实网络发送正确。


已接入公共 `lv_page` / `lv_status_bar`：返回键固定在左上，底部短横条上划弹出退出确认。
Windows 指针输入经过同一全局手势监控。退出确认后关闭预览中的 app；重启预览可重新打开。
状态栏时间、蜂窝和电量未接入数据源时显示未知。


默认进入 Inbox 消息列表，不显示 Inbox/Outbox 页签及搜索栏。无背景的 `+` 位于公共标题栏右上角，
不占列表内容行。自动检查包含默认列表、标题按钮位置/隐藏/恢复、返回层级和空箱状态。
新增 `empty.ppm` 与 `outbox.ppm`；最新截图为 `docs/assets/messages-inbox.png` 和 `messages-preview.png`。


收件人选择验证编译真实 Contacts model/controller/app（仅其 view 使用测试替身）。
预览注入 Alex、Home、Test Person 三个内存联系人，不写Flash。
检查只有正文文本框、下拉姓名/号码选项、选择保留正文、草稿自动选中、无选择/无联系人/旧号码未知时发送被禁用。
`recipients.ppm` 展示展开的联系人列表，`no-contacts.ppm` 展示空联系人状态。
