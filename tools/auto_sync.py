import os
"""macOS companion: sync only this device once per USB attachment, then close it."""
import argparse
import datetime
import time
import serial
from serial.tools import list_ports
from usb_serial import DeviceSerial
from serial_lock import port_lock

DEVICE_SERIAL=os.environ.get('SALARY_USB_SERIAL', '').replace(':','').replace('-','').upper()
def matches(port):
    identity=(port.serial_number or '').replace(':','').replace('-','').upper()
    return bool(DEVICE_SERIAL) and port.vid==0x303A and port.pid==0x1001 and identity==DEVICE_SERIAL

def read_until(conn,predicate,timeout=5):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        line=conn.readline().decode('ascii',errors='replace').strip()
        if predicate(line): return line
    raise TimeoutError('Device did not acknowledge clock protocol')

def exchange(conn):
    conn.reset_input_buffer()
    conn.write(b'HELLO\n')
    read_until(conn,lambda s:s=='SALARY_CLOCK_V1')
    conn.write(f'TIME {int(time.time())}\n'.encode())
    reply=read_until(conn,lambda s:s in ('TIME_OK','TIME_ERROR'))
    if reply!='TIME_OK': raise RuntimeError('RTC rejected clock sync')
    line=read_until(conn,lambda s:s.startswith('STATUS '))
    fields=dict(part.split('=',1) for part in line.split()[1:] if '=' in part)
    if fields.get('clock')!='1': raise RuntimeError('RTC still invalid after sync')
    local=datetime.datetime.strptime(fields['date']+' '+fields['time'],'%Y-%m-%d %H:%M:%S')
    observed=local.replace(tzinfo=datetime.timezone(datetime.timedelta(hours=8))).timestamp()
    if abs(observed-time.time())>3: raise RuntimeError('RTC readback differs from computer time')

def sync_device(port):
    with port_lock():
        # Do not toggle DTR/RTS: a clock update must not reset the running counter.
        conn=DeviceSerial(port=None,baudrate=115200,timeout=.5,write_timeout=2)
        conn.dtr=False; conn.rts=False; conn.port=port
        try:
            conn.open()
            exchange(conn)
        finally:
            conn.close()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--once',action='store_true')
    args=parser.parse_args()
    completed=set(); retry={}
    while True:
        present={p.device for p in list_ports.comports() if matches(p)}
        completed.intersection_update(present)
        retry={p:t for p,t in retry.items() if p in present}
        if args.once and not present: raise SystemExit('Salary Counter USB device not connected')
        for port in sorted(present-completed):
            if time.monotonic()<retry.get(port,0): continue
            try:
                sync_device(port)
                completed.add(port)
                print(datetime.datetime.now().isoformat(timespec='seconds'),'SYNC_OK',port,flush=True)
            except (OSError,serial.SerialException,TimeoutError,RuntimeError,ValueError,KeyError) as exc:
                if args.once: raise SystemExit(str(exc))
                print(datetime.datetime.now().isoformat(timespec='seconds'),'SYNC_RETRY',type(exc).__name__,flush=True)
                retry[port]=time.monotonic()+15
        if args.once: return
        time.sleep(2)

if __name__=='__main__': main()
