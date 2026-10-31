# 独立触摸测试

上电自动运行，无需输入命令。使用现有接线 SDA38/SCL45/RST3/INT39、I2C1 100 kHz。

参考 `D:/Proj/EasyNetRadio/CL28CK230-18A_CTP_230801` 中 STM32F1 demo 的
`HARDWARE/TOUCH/ctp.c` 和 `ctp.h`，该模组资料使用 FT6336，非 GT911。
FT_CMD_WR=0x70、FT_CMD_RD=0x71 对应 7 位地址 0x38；8 位寄存器地址，
复位拉低 20 ms、释放后等待 300 ms，INT 保持输入。

按模组参考程序测试 FT6336 兼容协议（0x38），不依赖 GT911 驱动。
0x38 应答和状态读取仅确认协议可用，不独立证明确切芯片型号。
FT6336 最多解析两点，从 0x02 读取触点数、0x03/0x09 读取坐标，
打印按下/移动的 ID、X/Y 和事件，松开打印 release。保留原始坐标，不交换 XY。
不改写触摸配置，传输超时 100 ms，每 20 ms 轮询。失败打印错误，复位重试。

```powershell
./build.ps1 -Application debug/t_touch
idf.py -C debug/t_touch -B debug/t_touch/build -p <PORT> flash monitor
```

编译不能证明硬件正常。上板检查地址应答、单点/双点、四角方向、释放及断线行为。
GPIO3/45 启动配置限制见 [接线说明](../../docs/display-board.md)。
