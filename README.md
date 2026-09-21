# 赛博核桃 · 完整固件分享包

版本：2026-09-21。仅适配 **Waveshare ESP32-C6-Touch-AMOLED-2.16，16MB Flash，480×480**，不是 1.83 英寸型号。

包含工资计算器、Codex 精灵、灵动表情、灯牌、双动画喵喵、时钟与系统设置。支持触摸、按键、电池状态和四方向旋转。

## 最新功能

- 工资：长按金额设置月薪、上下班时间、每月计薪天数；天数 0 为当月周一至周五，固定值支持 1–31 及两位小数。周末休息、午休计薪；不自动处理节假日调休。
- Codex 精灵：USB / BLE 同步；完成提醒超过 60 秒失效。需要自己电脑上的已登录 Codex 与本地伴侣，不随固件提供任何账户或额度。
- BOOT 短按熄屏/亮屏，KEY 回应用列表；KEY 在熄屏时不唤醒。

## 文件

- `firmware/`：完整固件源码与已生成的字库、图片资源。
- `release/`：当前实机版本的应用、引导器和分区表；`SHA256SUMS` 用于校验。
- `tools/`：构建、烧录、校时与可选 macOS 伴侣源码。
- `assets/`：素材、字体及相关许可证；`vendor/`：所需 XPowersLib 及许可证。
- `tests/`：主机单元测试（不包含会操作设备的历史实机脚本）。

不包含原设备全盘/NVS 转储、工资配置、Wi-Fi 密码、BLE 绑定密钥、账户登录文件、历史日志或个人申报材料。所有应用已在同一固件内，不用逐个安装。

## 直接烧录

安装 Python 3，进入本目录：

```sh
python3 -m venv .venv
.venv/bin/pip install esptool==4.12.0 pyserial==3.5 Pillow==11.3.0
# macOS 示例；替换成自己的串口。Windows 使用 .venv\Scripts\python.exe 和 COM 端口。
.venv/bin/python tools/flash_release.py --port /dev/cu.usbmodem101
.venv/bin/python tools/device.py --port /dev/cu.usbmodem101 --sync
```

烧录脚本先验证固件校验和，再写入 bootloader、分区表、boot_app0 及 app；不擦除或写入 NVS。已有相同分区布局的本项目设备可保留设置。其他布局应先自行备份，勿把本包用于别的开发板。

全新设备首次默认月薪为演示值 100000 元，请长按金额修改。首次校时后 RTC 可由电池保持；自动旋转需要按源码中的校准流程设置设备摆放轴向。

## 从源码构建

安装 Arduino CLI，将可执行文件放在 `tools/arduino-cli`，然后：

```sh
bash tools/setup.sh
bash tools/build.sh
# 烧录刚构建的版本，而不是 release 中的预编译版本
.venv/bin/python tools/flash_release.py --port /dev/cu.usbmodem101 --build
```

固定 Arduino-ESP32 3.3.0、huge_app、16MB Flash。分区不支持 OTA，应用上限 3MB，后续加功能须检查空间。构建脚本按 macOS 编写；发布固件由相同源码重新构建，仅移除了诊断字符串中的个人目录路径。
字库和图像头文件已提交，普通构建无需重生成。需要改素材时另装 `pypinyin==0.55.0` 并运行对应生成脚本。

## 可选：macOS Codex / BLE 同步

伴侣依赖 macOS Swift/CoreBluetooth，以及 `/Applications/ChatGPT.app/Contents/Resources/codex`；其他安装路径自行调整分享版脚本。Windows/Linux 的伴侣安装目前未适配。

分享版不包含原设备编号。先列出设备，再设置自己的 USB 序列号（去掉冒号和短横线）：

```sh
.venv/bin/python -m serial.tools.list_ports -v
export SALARY_USB_SERIAL=自己的设备序列号
.venv/bin/python tools/quota_companion.py --bind --once
.venv/bin/python tools/install_quota_companion.py
```

分享版安装器把该序列号加入后台服务环境。首次蓝牙提示请允许；更新二进制后可能需要重新注册其蓝牙权限。绑定密钥仅本地生成，不要上传或发送给别人。
`tools/device.py --sync` 可单次校时；`tools/auto_sync.py` 是可选前台自动校时工具，同样读取上述环境变量。

任务完成监听依赖 Codex 内部 SQLite 格式，升级可能需要适配。Mac 休眠期间不更新；BLE 无线只代替数据线，不是独立云端读取额度。

## 许可证与验证范围

字体遵循 `assets/OFL.txt`，pypinyin 和 XPowersLib 保留各自许可证。素材包含用户参考图和 AI 生成内容；此分享不额外授予未知第三方素材的商业权利，项目自身暂未指定开源许可证。
固件已在原型上运行，工资设置与持久化通过实机验证；分享包安装流程经过静态检查，未在另一台全新设备上验收。
