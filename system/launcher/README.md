# Launcher

主工程上电自动进入的 LVGL 9 图标网格，参考 `D:/Codes/NomadCast/system/launcher`。
图标和点击路由参考 NomadCast/EPOS 思路，保留 GPL-3.0-only 声明。
按应用管理器注册项枚举非隐藏应用，不写死应用数量，不生成演示业务数据。
当前240×320竖屏显示 Settings、Contacts、Messages，支持点击进入、左上角返回、底部上划确认退出。
应用页由各自 view 层渲染：Contacts 已实现触摸搜索、详情、新增、编辑和删除确认；
Settings 显示设置值；Messages 已支持收件/发件列表、搜索、详情、草稿和删除确认。Contacts 使用显式 mock 数据，见其 README。

`bepio_launcher_home()` 与 app-manager GUI 调用仅在 LVGL 所有者任务执行。
主工程默认关闭串口 REPL，保留 UART0 日志；无需命令启动显示。
首页先于 modem 探测显示，蜂窝模组缺失不阻止启动页。
截图见 [主机渲染预览](../../docs/assets/launcher.png)，不是实板照片。


公共页面统一使用 `lv_page_create()`（继承 DeepStoa 页面骨架），状态栏跨 app 常驻。
左上角返回走 `bepio_app_manager_exit_app()`，由 app 消费层级返回；
底部上划确认后走 `bepio_app_manager_delete()`，结束生命周期后重建首页。
取消退出不修改 app 或其编辑器。触摸手势与弹窗由 uilv widgets 提供，Launcher 只负责生命周期路由。
截图见 [系统导航预览](../../docs/assets/navigation-preview.png)。
