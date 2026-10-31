# 2.8 寸 SPI 显示与触摸子板

接线声明位于 `boards/esp32s3/bepio_display.h`，由现有 `esp32s3` 头文件组件导出。
主固件已接入 ST7789T3 + LVGL launcher，上电自动点亮；`debug/t_display` 和 `debug/t_touch` 保留独立诊断。
FT6336 硬件访问位于 `drivers/ft6336`，参考模组测试程序，使用本板 I2C1 接线。

| 模块信号 | GPIO | 主控方向 |
|---|---:|---|
| TP_INT | 39 | 输入 |
| TP_SDA | 38 | 双向，I2C 数据 |
| TP_RST | 3 | 输出 |
| TP_SCL | 45 | 输出，I2C 时钟 |
| LCD_MISO | 13 | 输入 |
| LCD_LED | 8 | 背光控制，驱动方式待确认 |
| LCD_SCK | 21 | 输出 |
| LCD_MOSI | 14 | 输出 |
| LCD_DC | 47 | 输出 |
| LCD_RESET | 40 | 输出 |
| LCD_CS | 48 | 输出 |

触摸分配 I2C1，与音频 I2C0（SDA41/SCL42）分开；上述 GPIO 与现有音频、MN316
UART1 TX17/RX18、UART0 TX43/RX44 和原生 USB19/20 接线无重叠。

LCD 已确认为 ST7789T3，240×320；此前按 GT911 测试 0x5D/0x14 均无应答；新提供的 CL28CK230-18A_CTP_230801
模组参考程序使用 FT6336（7 位地址 0x38）。当前优先测试 FT6336 兼容协议，实际型号与通信待上板确认。
独立测试见 `debug/t_touch` 和 `debug/t_display`。
实际触摸 I2C 地址、SPI 模式/频率、复位与中断极性、
背光控制电路及有效电平尚未确认，不根据“2.8 寸 SPI”推定具体驱动参数。
背光 LED 引脚是否可以直接由 GPIO 控制需检查模块电路；信号电平需匹配 3.3 V。

## 引脚限制

- GPIO45 是启动配置引脚，复位采样电平可影响 VDD_SPI 电压选择（受 eFuse 配置影响）。
  I2C SCL 通常需要上拉，因此必须核对模块上拉与实际模组的 eFuse/供电配置，
  避免接屏后无法启动。软件启动后再配置不能改变复位时的采样结果。
- GPIO3 也是启动配置引脚，涉及 JTAG 信号来源；需核对触摸复位电路的上拉/下拉。
- GPIO39/40 与音频的 GPIO41/42 共占用外部 JTAG 管脚，不应同时启用外部管脚 JTAG。
- 部分 DevKitC 的 GPIO48 接板载 RGB LED；本接线将其用于 LCD_CS，需确认板载 LED
  电路影响，并禁止其他代码再驱动 GPIO48。

依据：[ESP32-S3 数据手册，GPIO 限制与 Boot Configurations](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf)。

## 主固件横屏方案

主界面逻辑分辨率为320×240，顺时针横屏；launcher采用BP机风格的绿灰底与深色图标。
LCD底层仍按原生240×320寻址，LVGL刷新块旋转后发送。触摸同步采用
`screen_x = 319 - raw_y, screen_y = raw_x`，不改变FT6336原始读数或接线。
原始四角(0,0)、(239,0)、(0,319)、(239,319)分别映射至屏幕
(319,0)、(319,239)、(0,0)、(0,239)。独立debug显示测试保持原生尺寸。
通讯录列表、详情、确认框、搜索和编辑键盘适配横屏；底部上滑返回继续按逻辑屏幕坐标判断。
