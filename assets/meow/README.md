# 喵喵素材

frame-0.png、frame-1.png、frame-2.png 是用户原图，未覆盖。

grooming-atlas-v2.png 使用 imagegen 内置工具生成，以 frame-1.png 为角色参考。最终提示词要求：同一只橘白加菲猫，4×4 共 16 帧，固定机位与尺寸，依次抬爪、舔爪、擦脸、放下，首尾衔接；深色背景，无文字和紫色描边。AI 生成的是连续动作图集，tools/compile_meow.py 切分、缩放和编码为 baseline JPEG；工具本身不是视频生成器。

固件使用 ESP32-C6 ROM TJpgDec 解码，各帧约 140ms。预览为 evidence/meow-v2-preview.gif，原三帧版本保留为 meow-preview.gif。

## 开心版与双动画（2026-09-10）

happy-atlas-v3.png 使用 imagegen 技能与内置生成工具，以 grooming-atlas-v2.png 为角色参考生成。提示词摘要：4×4 连续动画图集，同一只橘白圆脸猫，固定机位、尺寸和脚部基线；每一帧均为开心的弯月笑眼、小嘴吐舌微笑，抬爪友好挥手，轻微歪头；深色背景，无文字、装饰或描边。

从开心图集中选取 0,1,2,3,11,10,9,8 帧组成伸出—收回的循环，避免逐行读图时突然缩回爪子。原版保留全部 16 帧。两套 160×160 JPEG 共 88,387 字节（含索引），quality=65，优化 Huffman；运行时共用解码缓存，不同时展开两套图片。显示为 320×320，每帧 140ms。默认开心版，左右滑动切换，退出应用再进入保留选择，重启回到开心版。未删除任何应用，未改分区或清除设置。

实际 C++ 渲染预览：evidence/meow-happy-v3-preview.gif、evidence/meow-grooming-v3-preview.gif。主机测试覆盖双动画逐帧分块一致性、电量保留区、手势切换和取消。串口集成测试不是实物手指操作测试。
