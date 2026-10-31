# 主工程 launcher 验证记录

## 自动验证

- ESP-IDF v5.5.3 根工程完整构建通过，Bepio.bin 约543KB，2MB分区余量充足。
- App Manager/Settings、Contacts、Messages、Console 四组主机回归通过。
- 实际 LVGL 9.5.0 主机渲染通过，检查 Settings 图标路由及 Home 返回。
- 预览为实际 launcher 源码输出，见 [launcher.png](assets/launcher.png)。

主机 mock 不验证 FreeRTOS 调度、SPI DMA、实际触摸或完整业务视图；截图不是实板照片。

## 上板检查

主固件启动应出现 `Home ready`、`Launcher displayed: portrait 240x320 RGB`。
无需串口命令；背光高电平，显示 BEPIO 首页及已注册应用。
需实板确认：全屏覆盖、RGB色序、方向、背光、FT6336四角坐标、应用点击与Home返回。
modem 断开/无应答时首页应保持显示；触摸初始化失败时首页也应保持显示。
Contacts 已增加触摸搜索、详情、新增、编辑、删除确认及显式 mock 数据；Settings/Messages 仍为摘要页，未实现消息发送交互。

本轮烧录使用已选定 COM8（CH340）；若串口占用则必须先释放，不能把构建通过视为已上板。

## 本轮 COM8 实板启动

已释放串口占用并通过 COM8 完成 bootloader/分区表/应用标准布局烧录，写入 hash 验证通过。
随后复位读取约7秒日志（本地 `build/launcher-boot.log`）：

- 459ms：`Home ready: 3 visible applications`。
- 719ms：`Launcher displayed: portrait 240x320 RGB, ST7789T3 SPI2 10MHz`。
- 1039ms：FT6336兼容设备在0x38应答，状态00。
- 3049ms：modem不可用，主程序继续并从app_main返回，GUI独立任务持续运行。

此记录确认初始化/传输调用完成；实际颜色、方向、亮度以及手指点击和返回仍需用户观察。
串口读取已结束，未留后台monitor占用COM8。

## Contacts 增删改查扩展

- 四组主机回归通过，Contacts 新增 mock/搜索/草稿/保存/放弃/删除确认覆盖。
- 真实LVGL交互测试通过：字母和数字键盘、加号按键、键盘边界、搜索、CRUD、
  重复号码/非法输入、取消与放弃、分页、关闭重开保留本次修改。
- 主机渲染预览见 `docs/assets/contacts-*.png`；界面为240×280内容区+40px系统返回区。
- 根ESP-IDF固件构建通过，约591KB，2MB分区余量充足。
- 数据仅RAM，开机注入8条明确标记DEMO的虚构联系人；不拨号、不发送消息。

实际手指输入、滚动手势和触摸坐标仍需用户在屏幕上检查。

## 320×240横屏验证

固件构建及四组业务主机回归通过。真实LVGL横屏交互测试通过：launcher路由、
共享页头、底部上滑退出确认，以及Contacts搜索、CRUD、键盘边界与分页。
`python tests/unittest/orientation/run.py`遍历全部76800个原始触摸坐标，并验证
非零偏移刷新块的像素旋转与触摸变换互逆。

当前横屏预览：`docs/assets/home-landscape-320x240.png`、
`docs/assets/contacts-list-320x240.png`、`docs/assets/contacts-edit-320x240.png`。
上方竖屏验证记录为历史记录；当前状态栏20px、页头32px、底部手势区16px。
实际面板观看方向与手指点击四角仍需上板观察。


## 滚动性能管线优化（代码与host验证）

本轮保持40MHz SPI，改为两个16行绘制缓冲 + 一个16行旋转传输缓冲，内部DMA内存合计30KB。
采用异步提交与LVGL flush-wait回收，绘制与上一块传输重叠；旋转和RGB565字节序转换合并。
显示刷新设20ms，独立触摸任务与LVGL输入定时器设10ms；GUI不执行触摸I²C事务。

- 真实LCD驱动host回归：异步借用、忙拒绝、非阻塞轮询、正常完成、同步兼容、提交错误、超时、晚到ISR后拒绝复用。
- 真实LVGL + LCD/显示管线集成：5个场景通过，覆盖绘制与模拟DMA重叠、缓冲不变、完整旋转后的RGB565图像、
  跨帧完成时间、分配失败清理及提交/等待超时路径。
- 全坐标方向回归与融合转换回归通过；业务四组host回归、Launcher/Contacts真实LVGL交互回归通过。
- ESP-IDF主工程与演示构建通过；这些结果不证明40MHz电气稳定性、真实ISR/FreeRTOS时序或实际帧率。

按 `tests/product/README.md` 在实板检查滚动与触摸响应；不能把20ms目标周期当作实测50FPS。


## 用户回传的异步管线实板日志

已收到实板启动日志确认40MHz、双绘制缓冲、20ms目标刷新和10ms触摸采样。
滚动窗口：单帧平均66.6～73.8ms，DMA每块约2.2ms、剩余等待约26～38us；
采样最大间隔约15～19ms，但GUI消费输入最大间隔约100ms，切页曾达140ms。
这证明采样与GUI消费之间仍有延迟，不能把配置20ms当作实测50FPS。
静止窗口无刷新、触摸采样/消费各约100次每秒，未见显示超时或触摸错误日志。

据此为图形组件启用可关闭的 `-O2`；实际编译命令已核对优化参数顺序。
此处保留的是O2前的用户数据；O2后的实板效果尚待验证。
