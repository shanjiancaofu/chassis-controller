# LCD 页面预览

这些 PNG 来自固件 `app/ui/lcd/lcd_ui.c` 的真实逐行 C 渲染器，画布为 STM32 LCD 的 320x240
物理坐标，并使用同一 5x7 字模、RGB565 颜色、Logo 和停止状态示例数据。Python 只负责编译
主机预览程序、将 RGB565 转为 RGB，以及按最近邻放大 2 倍保存。它们用于烧录前检查重叠、
越界和层级，不代替目标板视觉确认。

重新生成：

```text
python3 tools/lcd/render_ui_preview.py
```

文件：

- `overview.png`：总览页
- `motor.png`：电机页
- `sensors.png`：传感器页
- `system.png`：系统页
- `all_pages.png`：四页纵向汇总
