# MN316 驱动探测

独立 ESP-IDF 测试工程。直接引用 `boards/esp32s3/bepio_devkit.h` 的
UART 端口、TX/RX 和波特率定义，并调用 `drivers/mn316/mn316_probe()`；
不在测试代码中复制 UART 初始化。启动后最多尝试三次 AT 探测，
每次有界等待，成功即停止。

```powershell
./build.ps1 -Application debug/t_mn316
```

烧录与监视请显式选择实际端口：

```powershell
cd debug/t_mn316
idf.py -p <PORT> flash monitor
```

ESP32-S3 GPIO17 TX 接 MN316 RX，GPIO18 RX 接 MN316 TX，两板共地。
控制台默认走 UART0；MN316 使用 UART1 9600 8N1。`PASS` 只证明
收到完整 AT `OK`，不证明注册、附着、IP 或 MQTT/TLS 连接。
