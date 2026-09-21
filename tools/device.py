"""USB clock sync/status/capture for the salary MVP (pyserial; Pillow for PNG)."""
import argparse
import struct
import time
from pathlib import Path
import serial
from serial_lock import port_lock
from usb_serial import DeviceSerial

p=argparse.ArgumentParser()
p.add_argument('--port',default='/dev/cu.usbmodem101')
p.add_argument('--sync',action='store_true')
p.add_argument('--reboot',action='store_true')
p.add_argument('--capture',type=Path)
p.add_argument('--seconds',type=int,default=3)
a=p.parse_args()
with port_lock(timeout=10), DeviceSerial(a.port,115200,timeout=2,write_timeout=10) as s:
    time.sleep(.3)
    s.reset_input_buffer()
    if a.sync:
        s.write(f'TIME {int(time.time())}\n'.encode())
        deadline=time.monotonic()+5
        confirmed=False
        while time.monotonic()<deadline:
            line=s.readline().decode(errors='replace').strip()
            if line: print(line)
            if line=='TIME_OK': confirmed=True; break
            if line=='TIME_ERROR': raise SystemExit('Clock sync rejected')
        if not confirmed: raise SystemExit('Clock sync not acknowledged')
    if a.reboot:
        s.write(b'REBOOT\n')
        print('Reboot requested; reconnect after device reappears.')
    elif a.capture:
        s.write(b'FRAME\n')
        deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            header=s.readline()
            if header.startswith(b'FRAME '): break
        else: raise SystemExit('No frame header')
        _,w,h,n=header.split(); w,h,n=map(int,(w,h,n))
        if (w,h,n)!=(480,480,460800): raise SystemExit('Unexpected frame geometry')
        data=bytearray()
        deadline=time.monotonic()+20
        while len(data)<n and time.monotonic()<deadline:
            data.extend(s.read(n-len(data)))
        if len(data)!=n: raise SystemExit(f'Short frame {len(data)}/{n}')
        if s.readline()!=b'\n' or s.readline()!=b'FRAME_END\n': raise SystemExit('Frame trailer mismatch')
        from PIL import Image
        rgb=bytearray()
        for (v,) in struct.iter_unpack('<H',data):
            rgb.extend((((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
        Image.frombytes('RGB',(w,h),bytes(rgb)).save(a.capture)
        print('Captured device framebuffer:',a.capture)
    else:
        s.write(b'STATUS\n')
        deadline=time.monotonic()+a.seconds
        while time.monotonic()<deadline:
            line=s.readline().decode(errors='replace').strip()
            if line: print(line)
