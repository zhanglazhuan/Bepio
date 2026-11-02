# assets

图标、字体与其他 UI 资源。

Notifications 启动器图标转换（使用已有 PNG，不重复下载）：
```powershell
python tools/LVGLImage.py assets/notifications_40dp_000000_FILL0_wght400_GRAD0_opsz40.png --name ic_notifications --cf ARGB8888 --resize 40x40 -o system/uilv/imgs/icon_40x40
```
