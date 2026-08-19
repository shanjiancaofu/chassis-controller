# LCD 页面预览

这些 PNG 按 STM32 LCD 的 320x240 分辨率绘制，使用当前 BSP 的 5x7 字模、RGB565 颜色、Logo、
坐标和一组停止状态示例数据。它们用于在烧录前检查文字是否重叠、越界或层级不合理，不代替目标
板上的 LCD 视觉确认。

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
