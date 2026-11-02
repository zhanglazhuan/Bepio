# LVGL launcher 主机预览

编译实际 `system/launcher/bepio_launcher.c` 和已获取的 LVGL 9.5.0，
仅用三个应用描述符/生命周期入口 mock，不模拟显示硬件或 FT6336。
输出240×320 RGB PPM，编译公共 page/status/navigation widgets。
通过真实 LVGL pointer read 验证底边上划、短划/横划/中部滚动排除、退出取消/确认；
验证左上返回可被 app 消费、退出后回首页、状态栏跨页面保持单实例与未知状态。
不证明实际 SPI、背光、触摸坐标或完整业务视图正确；应用管理器另有主机回归。

先运行根 `build.ps1` 获取 managed_components；Windows/MinGW 示例：

```powershell
cmake -S tests/integration/launcher_preview -B build/launcher-preview -G "MinGW Makefiles" -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/mingw64/bin/g++.exe -DCMAKE_ASM_COMPILER=C:/mingw64/bin/gcc.exe -DCMAKE_MAKE_PROGRAM=C:/mingw64/bin/mingw32-make.exe
cmake --build build/launcher-preview -j 6
./build/launcher-preview/launcher_preview.exe build/launcher-preview/home.ppm
```

`docs/assets/launcher.png` 为该实际 LVGL 渲染的 PNG 转换，不是实板照片。

## Contacts GUI

同一构建增加 `contacts_preview`，编译真实 Contacts MVC、LVGL view 与应用管理器，
只替换单线程 mutex/log 平台入口。不运行真实 FreeRTOS、屏幕或触摸驱动。

```powershell
cmake --build build/launcher-preview -j 6
./build/launcher-preview/contacts_preview.exe build/launcher-preview
```

测试控件事件和模型结果：搜索、新增、编辑、重复号码与非法输入、保存、取消/放弃、
删除确认、重开数据、分页和键盘可见区域/实际加号按键输入。
生成 `contacts-*.ppm`；对应PNG保存于 `docs/assets/contacts-*.png`。


附加截图为 `home.ppm.app.ppm` 和 `home.ppm.exit.ppm`；
`docs/assets/navigation-preview.png` 拼合 Launcher、真实 Messages/Contacts 与退出弹窗。
Contacts 与 Messages 的预览均使用和固件相同的公共页面骨架。


## 异步显示管线回归

`display_pipeline_preview` 编译真实 LVGL、`bepio_ui_display.c` 和 `bepio_lcd.c`，
只替换 ESP-IDF SDK、信号量/时间和硬件入口。模拟DMA延迟，验证前一块传输期间发生下一块绘制，
借用的面板缓冲内容保持不变、最终320×240输出旋转与RGB565字节序正确、跨刷新完成时间统计正确。
另覆盖非阻塞轮询、部分分配失败清理、提交失败、flush-wait超时和晚到完成事件。

```powershell
cmake --build build/launcher-preview -j 8
ctest --test-dir build/launcher-preview --output-on-failure -R display_pipeline
python tests/unittest/display_async/run.py
python tests/unittest/orientation/run.py
```

此测试不是实板SPI吞吐测试，不证明FreeRTOS双任务调度、ISR唤醒或40MHz走线稳定性。
实际显示性能需要在实板验证。

## Notifications GUI

```powershell
cmake -S tests/integration/launcher_preview -B build/launcher-preview
cmake --build build/launcher-preview --target notifications_preview launcher_preview -j 6
./build/launcher-preview/notifications_preview.exe build/launcher-preview
./build/launcher-preview/launcher_preview.exe build/launcher-preview/notifications-home.ppm
```

使用明确的主机通知数据验证实时列表、详情/已读、返回、删除、关闭重开与应用 40×40 图标；
launcher_preview 还检查应用描述符的自定义图标被实际展示。
截图位于 docs/assets/notifications-*.png；不证明手机通知权限、MQTT 或实板显示正常。
