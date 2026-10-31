# SPI LCD 独立显示测试

ESP-IDF v5.5.3，UART0/CH340 控制台（当前 COM8）。不启动触摸、音频或 MN316。
LCD 接线继承 `boards/esp32s3/bepio_display.h`，使用 SPI2、mode 0、10 MHz、RGB565。
使用 ESP-IDF 自带 esp_lcd ST7789 驱动，屏幕为 ST7789T3，显式设置竖屏：宽 240、高 320，关闭 XY 交换和镜像。

仓库根目录：

```powershell
./build.ps1 -Application debug/t_display
idf.py -C debug/t_display -B debug/t_display/build -p COM8 flash monitor
```

上电自动初始化 LCD、绘制色条，并将 GPIO8 置高开启背光，无需输入命令。
失败时打印错误，按 RESET 重试。背光目前按高电平有效处理，实际极性仍需上板确认。

从左到右绘制红、绿、蓝、白、黑五个色条，用于观察颜色和覆盖范围。根据上板红蓝互换结果改为 RGB，无旋转、镜像和坐标偏移，
若模块需要特殊初始化、偏移或 RGB 次序，需根据实际资料调整。
DMA 使用 16 行缓冲区，每批等完成后再复用；完成等待超时会停止后续绘制，重启恢复。

GPIO48 的 LCD_CS 可能连接板载 RGB LED，禁止其他代码同时驱动。
接线、供电与 GPIO45 启动限制见[子板说明](../../docs/display-board.md)。
本工程只编译验证，背光极性、色序、初始化序列与实际显示效果均需上板确认。
