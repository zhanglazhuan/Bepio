# 项目介绍
一个大屏的BB机, 主要有两类用途
1. 给儿童, 支持定位, 语音转文字(STT)后通过 NBIoT 发送给家长手机接受通知; 家长发送消息在端侧转成语音播放(TTS)
2. 给成人, 支持将手机的消息通过AI过滤后发送给BB机, 实现手机戒断, 更专注

# 软件框架
1. 使用 espidf

## MN316 TCP 测试子工程构建与烧录

在 `D:\Codes\Bepio` 目录中使用 PowerShell 执行：

```powershell
. C:\Espressif\frameworks\esp-idf-v5.5.3\export.ps1
```

若在 `AT` 阶段报 `-116`，表示等待 AT 回复超时，尚未进入 TCP 测试。
MN316 接 GPIO17（UART1 TX）→模组 RXD、GPIO18（UART1 RX）←模组 TXD，9600 8N1。
电脑连接标为 **USB** 的原生 USB 口查看日志，COM5 替换为该接口实际枚举的 Windows COM 端口。
标为 **COM** 的接口是 UART0 USB-UART 桥，可用于烧录，与 MN316 的 UART1 独立；应用日志仍从 **USB** 原生口读取。
若原生 USB 口暂未枚举，可按住 BOOT 后按 RESET 进入下载模式，再识别其 COM 端口。
新版诊断输出 AT TX/RX 和失败时的 RX 字节计数，按实板接线核对供电、共地、电平与波特率。
