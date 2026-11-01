# MN316 已迁移

此目录只保留迁移说明，不再是 ESP-IDF 构建组件。

- UART、AT、入网、TCP/TLS 驱动：[drivers/mn316](../../../drivers/mn316/README.md)。
- MQTT、设备身份、主题、CA 与消息队列：[system/mn316_ctrl](../../mn316_ctrl/README.md)。
- 主工程与 debug/t_mn316 统一使用新组件，无 archive 构建依赖。
