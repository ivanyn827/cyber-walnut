"""Generate compact alpha glyphs from the OFL Noto font; no system fonts used."""
from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
chars = ''.join(sorted(set('0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ¥%:.-/，工资实时计算器年 月 日 周一二三四五六今天已赚薪时距离下班未开始工作中收休息认真搬砖悄变富请连接电脑校估算午计入双本完成享受生活准备出发充电时间无效等待恢复')))
for source in ['ui.h','app_host.h','connectivity_ui.h','connectivity.h','emotions.h','lightboard.h','quota.h']:
    chars += ''.join(re.findall(r'[\u4e00-\u9fff，。：！]', (ROOT/'firmware/salary_counter'/source).read_text()))
out = ['#pragma once', '#include <stdint.h>',
       'struct Glyph { uint32_t code, offset; uint8_t width, height, advance; int8_t dx, dy; };',
       'struct Font { const Glyph *glyphs; const uint8_t *pixels; int count; };']
for size, weight in [(18,450),(20,450),(22,500),(24,550),(36,650),(64,700)]:
    font = ImageFont.truetype(str(ROOT/'assets/NotoSansSC.ttf'), size)
    font.set_variation_by_axes([weight])
    baseline = -font.getbbox('0', anchor='ls')[1]
    chars = ''.join(sorted(set(chars+''.join(chr(i) for i in range(32,127))+'月薪设置每月收入元取消保存删除长按金额修改未检测到电池读取失败输入有效范围保存失败请重试已充满外接电源电量未知或应用桌面助手点击打开首页时钟日期与倒啦！蓝牙网络扫描连接密码开启关闭忘记选择设备名称仅支持低功耗不耳机音频可被工具发现返回空格符号正在至少位断存暂此认证：，无需启动广播')))
    subset = chars if size < 36 else '0123456789¥%:.- '
    if size == 24:
        # 24px is for fixed titles/statuses plus ASCII marquee characters.
        # Arbitrary Chinese marquee/input uses the separate full lightfont.
        subset = ''.join(sorted(set(''.join(chr(i) for i in range(32,127))+
            '喵时钟请连接电脑校时应用工资实时计算数器网络设置蓝牙灵动表情灯牌输入文字月薪年月日今天已赚下班啦！精力值形象档位等待电脑同步等我回血彻底蔫了有点虚了还能继续稳得很我能打十个任务完成')))
    glyphs, pixels = [], []
    for ch in subset:
        l,t,r,b = font.getbbox(ch, anchor='ls')
        w,h = max(0,r-l),max(0,b-t)
        img = Image.new('L',(max(1,w),max(1,h)))
        ImageDraw.Draw(img).text((-l,-t),ch,font=font,fill=255,anchor='ls')
        data = list(img.getdata()) if w*h else []
        glyphs.append(f'{{{ord(ch)},{len(pixels)},{w},{h},{round(font.getlength(ch))},{l},{t+baseline}}}')
        pixels.extend(data)
    out += [f'static const uint8_t pixels{size}[] = {{'+','.join(map(str,pixels))+'};',
            f'static const Glyph glyphs{size}[] = {{'+','.join(glyphs)+'};',
            f'static const Font font{size} = {{glyphs{size}, pixels{size}, {len(glyphs)}}};']
(ROOT/'firmware/salary_counter/fonts.h').write_text('\n'.join(out)+'\n')
print('Generated fonts.h')
