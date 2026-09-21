"""Read official Codex quota; USB-first signed synchronization, BLE fallback.

No model invocation. No account credentials are read or copied by this program.
"""
import argparse
import hashlib
import hmac
import json
import math
import os
from pathlib import Path
import queue
import secrets
import signal
import subprocess
import threading
import time

import serial
from serial.tools import list_ports
from serial_lock import port_lock
from usb_serial import DeviceSerial
from task_completions import Completions

ROOT = Path(__file__).resolve().parents[1]
STATE = Path.home() / 'Library/Application Support/SalaryCounter/quota-binding.json'
CODEX = '/Applications/ChatGPT.app/Contents/Resources/codex'

def select_sample(result, now=None):
    """Choose Codex weekly only; never substitute the Spark bucket."""
    buckets = result.get('rateLimitsByLimitId')
    bucket = buckets.get('codex') if isinstance(buckets, dict) else result.get('rateLimits')
    if not isinstance(bucket, dict) or bucket.get('limitId') not in (None, 'codex'):
        bucket = {}
    windows = [bucket.get(k) for k in ('primary', 'secondary')]
    weekly = next((w for w in windows if isinstance(w, dict) and w.get('windowDurationMins') == 10080), None)
    # Unknown weekly quota is not a shorter window or an empty/full quota.
    used = weekly.get('usedPercent') if weekly else None
    valid = isinstance(used, (int, float)) and not isinstance(used, bool) and math.isfinite(used)
    reset = weekly.get('resetsAt') if weekly else None
    return {'remaining': round(max(0, min(100, 100-used))) if valid else -1,
            'minutes': 10080 if weekly else 0,
            'reset': int(reset) if isinstance(reset, (int, float)) and math.isfinite(reset) and reset > 0 else 0,
            'observed': int(time.time() if now is None else now)}

def packet(binding, nonce, sample, sequence=None):
    seq = time.time_ns() // 1000 if sequence is None else sequence
    body = f"Q1 {nonce} {seq} {sample['observed']} {sample['remaining']} {sample['minutes']} {sample['reset']}"
    return body + ' ' + hmac.new(bytes.fromhex(binding['key']), body.encode(), hashlib.sha256).hexdigest()

def notice_packet(binding, nonce, count, sequence=None, expires=0):
    seq = time.time_ns()//1000 if sequence is None else sequence
    body = f'N2 {nonce} {seq} {count} {int(expires)} {int(time.time())}'
    return body+' '+hmac.new(bytes.fromhex(binding['key']),body.encode(),hashlib.sha256).hexdigest()

def lines(process, output, kind):
    for line in process.stdout:
        output.put((kind, line.strip()))
    output.put((kind, None))

def log(message):
    print(time.strftime('%Y-%m-%d %H:%M:%S'), message, flush=True)

def save_binding(value):
    STATE.parent.mkdir(parents=True, exist_ok=True)
    temporary = STATE.with_suffix('.tmp')
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, 'w') as f:
        json.dump(value, f)
    os.replace(temporary, STATE)
    os.chmod(STATE, 0o600)

def exchange(port, command, prefix, timeout=1.2):
    port.write((command+'\n').encode()); port.flush()
    deadline = time.monotonic()+timeout
    while time.monotonic()<deadline:
        line=port.readline().decode('ascii', errors='ignore').strip()
        if line.startswith(prefix): return line
    raise TimeoutError(prefix)

def usb_sync(binding, sample, bind=False, notice=None):
    devices=[p for p in list_ports.comports() if os.environ.get('SALARY_USB_SERIAL') and p.vid==0x303A and p.pid==0x1001 and (p.serial_number or '').replace(':','').upper()==os.environ.get('SALARY_USB_SERIAL', '').replace(':','').replace('-','').upper()]
    if not devices: return False, binding
    with port_lock():
        p=DeviceSerial(port=None, baudrate=115200, timeout=.12,write_timeout=2)
        p.dtr=False; p.rts=False; p.port=devices[0].device
        try:
            p.open()
            greeting=exchange(p,'QHELLO','QHELLO ',5).split()
            if len(greeting)!=3: raise ValueError('Invalid device greeting')
            _,device,nonce=greeting
            if binding is None:
                if not bind: raise ValueError('USB binding required: run --bind')
                binding={'device':device,'key':secrets.token_hex(32)}
                # Save first so a crash after the device commits cannot lose its key.
                save_binding(binding)
            if binding['device']!=device: raise ValueError('Different USB device')
            if bind:
                if exchange(p,'QPAIR '+binding['key'],'QPAIR_')!='QPAIR_OK': raise ValueError('Device already bound to another key')
            if sample:
                result=exchange(p,packet(binding,nonce,sample),'QUOTA_')
                if result!='QUOTA_OK': raise ValueError('Device rejected quota')
            if notice is not None:
                if exchange(p,notice_packet(binding,nonce,notice[0],expires=notice[1]),'QUOTA_')!='QUOTA_OK':
                    raise ValueError('Device rejected completion notice')
            return True,binding
        finally: p.close()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--bind',action='store_true')
    parser.add_argument('--once',action='store_true')
    parser.add_argument('--ble-only',action='store_true',help='Diagnostic BLE delivery without changing device configuration')
    args=parser.parse_args()
    binding=json.loads(STATE.read_text()) if STATE.exists() else None
    if args.bind:
        ok,binding=usb_sync(binding,None,True)
        if not ok: raise SystemExit('Connect the device by USB to bind')
        log('USB device bound')
    if not binding: raise SystemExit('Run --bind with the device attached first')
    output=queue.Queue(); server=None; ble=None; ready=False; request=0
    next_server=next_ble=next_read=next_push=0
    rpc_deadline=None
    sample=None; ble_nonce=None; ble_sequence=None; usb_last=0; last_state=None; started=time.monotonic()
    ble_inflight_until=0
    completions=None
    next_completion=0; completion_total=None; notice_delivered=-1; ble_notice=None; notice_retry=0
    def request_limits():
        nonlocal request,next_read,rpc_deadline
        request+=1
        server.stdin.write(json.dumps({'id':request,'method':'account/rateLimits/read'})+'\n');server.stdin.flush()
        next_read=time.monotonic()+30
        rpc_deadline=time.monotonic()+20
    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM,stop)
    try:
        while True:
            now=time.monotonic()
            if not args.once and now>=next_completion:
                next_completion=now+2
                try:
                    if completions is None:completions=Completions(Path(os.environ.get('CODEX_HOME') or Path.home()/'.codex'),STATE.parent/'quota-completions.json')
                    completion_total=completions.poll()
                except Exception as error:
                    log('Completion metadata unavailable: '+type(error).__name__+': '+str(error))
                    next_completion=now+30
            if server is None and now>=next_server:
                server=subprocess.Popen([CODEX,'app-server'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,bufsize=1)
                threading.Thread(target=lines,args=(server,output,'server'),daemon=True).start()
                server.stdin.write(json.dumps({'id':0,'method':'initialize','params':{'clientInfo':{'name':'deskmate_quota','version':'1.0.0'}}})+'\n');server.stdin.flush()
                ready=False;next_server=now+10
                rpc_deadline=now+20
            if ble is None and not args.once and now>=next_ble:
                binary=STATE.parent/'quota_ble'
                if not binary.exists(): binary=ROOT/'tools/bin/quota_ble'
                if binary.exists():
                    ble=subprocess.Popen([str(binary),binding['device'],binding.get('peer','')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,bufsize=1)
                    threading.Thread(target=lines,args=(ble,output,'ble'),daemon=True).start()
                next_ble=now+10
            try: kind,line=output.get(timeout=.1)
            except queue.Empty: kind=line=''
            if kind=='server':
                if line is None:
                    server.wait(); server=None; ready=False; next_server=now+10;log('Quota source disconnected; retaining last sample')
                else:
                    try: event=json.loads(line)
                    except ValueError: continue
                    if event.get('id')==0 and 'result' in event:
                        server.stdin.write('{"method":"initialized"}\n');server.stdin.flush();ready=True;request_limits()
                    elif 'id' in event and 'result' in event:
                        rpc_deadline=None
                        sample=select_sample(event['result']);next_push=0
                        log(f"Quota remaining={sample['remaining']}% window={sample['minutes']}m")
                    elif event.get('method')=='account/rateLimits/updated':
                        # Re-read the full bucket map; event shapes vary between server versions.
                        if ready: request_limits()
                    elif 'error' in event:
                        rpc_deadline=None
                        log('Quota API unavailable; retaining previous sample')
                        if event.get('id')==0: server.terminate()
            elif kind=='ble':
                if line is None:
                    ble.wait();ble=None;ble_nonce=None;next_ble=now+10
                elif line.startswith('PEER '):
                    parts=line.split()
                    if len(parts)==5 and parts[2]=='QHELLO' and parts[3]==binding['device']:
                        ble_nonce=parts[4];binding['peer']=parts[1];save_binding(binding);next_push=0;log('Bound BLE device connected')
                elif line=='DISCONNECTED': ble_nonce=None;ble_inflight_until=0;ble_notice=None
                elif line.startswith('ACK '):
                    ble_inflight_until=0
                    if line == f'ACK OK {ble_sequence}':
                        if ble_notice is not None: notice_delivered=ble_notice;ble_notice=None
                        if last_state!='BLE': log('Delivery BLE confirmed');last_state='BLE'
                    else: log('BLE packet not accepted; will retry')
                elif line.startswith('STATE ') and line!='STATE 5': log('Bluetooth unavailable (state '+line.split()[-1]+')')
                elif line.startswith('TRANSPORT '): log('BLE '+line[10:])
            if ready and now>=next_read: request_limits()
            if server and rpc_deadline and now>rpc_deadline:
                server.terminate();rpc_deadline=None
                log('Quota source timed out; reconnecting')
            if sample and now>=next_push and (completion_total is None or completion_total==notice_delivered):
                next_push=now+2
                # Never re-stamp old provider data. Stop delivery after 75s without a successful read.
                if time.time()-sample['observed']<=75:
                    ok=False
                    if not args.ble_only:
                        try: ok,binding=usb_sync(binding,sample)
                        except (TimeoutError,serial.SerialException,OSError,ValueError): pass
                    if ok:
                        usb_last=now
                        if last_state!='USB':log('Delivery USB confirmed');last_state='USB'
                        if args.once:return
                    elif ble and ble_nonce and now-usb_last>=6 and now>=ble_inflight_until:
                        try:
                            ble_sequence=time.time_ns()//1000
                            ble_notice=None
                            ble.stdin.write(packet(binding,ble_nonce,sample,ble_sequence)+'\n');ble.stdin.flush()
                            ble_inflight_until=now+10
                        except (BrokenPipeError,OSError):pass
            # Independent of quota freshness: a quota API outage cannot hide a completion.
            if completion_total is not None and completion_total!=notice_delivered and now>=notice_retry:
                notice_retry=now+2
                notice=completions.next_notice(notice_delivered)
                ok=False
                if not args.ble_only:
                    try:ok,binding=usb_sync(binding,None,notice=notice)
                    except (TimeoutError,serial.SerialException,OSError,ValueError):pass
                if ok:
                    notice_delivered=notice[0];usb_last=now
                    log('Completion delivery USB confirmed')
                elif ble and ble_nonce and now-usb_last>=6 and now>=ble_inflight_until:
                    try:
                        ble_sequence=time.time_ns()//1000;ble_notice=notice[0]
                        ble.stdin.write(notice_packet(binding,ble_nonce,notice[0],ble_sequence,expires=notice[1])+'\n');ble.stdin.flush()
                        ble_inflight_until=now+10
                        next_push=max(next_push,now+1)
                    except (BrokenPipeError,OSError):pass
            if args.once and now-started>35: raise TimeoutError('No confirmed USB quota delivery within 35 seconds')
    except KeyboardInterrupt:
        pass
    finally:
        for process in (ble,server):
            if process and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:process.kill();process.wait()

if __name__=='__main__': main()
