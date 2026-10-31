# tools

根目录 `build.ps1` 激活指定 ESP-IDF 安装并调用 idf.py，不烧录。
支持 -IdfPath、-Application、-BuildDirectory、-MessagesDemo 与 -Pristine。
旧 Zephyr esptool 入口已归档到 archive/zephyr/tools/，当前由 ESP-IDF 管理工具链。

## 资源与固件工具

以下四个 Python 工具迁移自 `D:/Codes/NomadCast/tools`，源项目文件保留。
`LVGLImage.py` 保持原样；图标输出路径和固件检查默认值已适配 Bepio。

在仓库根目录安装资源工具依赖：

```powershell
python -m pip install -r tools/requirements.txt
```

- `check_firmware_size.py`：默认检查本仓库 `build/Bepio.bin`，也可传入其他镜像路径。
  当前 `partitions.csv` 的 factory 分区为 2 MiB，阈值为 1887436 字节（保留至少 10%）。
  修改分区大小时需同步修改脚本中的 `MAX_APP_BYTES`；超限返回非零退出码。
- `download_icons.py`：下载 Google Material Symbols，生成 PNG 和 LVGL C 数组。
  默认 PNG 放在 `assets/`，C 文件放在 `system/uilv/imgs/<size>x<size>/`。
  需要网络；支持 `--assets-dir`、`--c-output-dir` 覆盖输出目录。
- `LVGLImage.py`：独立图片转换器，支持 LVGL 9 的图片格式，使用 `--help` 查看参数。
  部分调色板处理需要额外安装 `pngquant` 并加入 PATH。
- `c_font.py`：提供 `convert_font(font_path, size, output_dir, ranges)`。
  需要先安装 Node.js 工具 `npm install -g lv_font_conv`。
  原脚本直接运行的示例依赖 `fonts/SimSun.ttf` 和已有 `output/` 目录；
  实际使用时传入自己的字体路径，并提前创建输出目录。

```powershell
python tools/check_firmware_size.py
python tools/download_icons.py settings --size 24
python tools/LVGLImage.py assets/settings.png --name ic_settings --cf ARGB8888 -o system/uilv/imgs/24x24
python -c "from tools.c_font import convert_font; from pathlib import Path; Path('output').mkdir(exist_ok=True); convert_font('C:/Windows/Fonts/arial.ttf', 24, 'output')"
```

生成的字体和图片不会自动加入固件构建；需要在对应组件的 `CMakeLists.txt` 中显式注册。
