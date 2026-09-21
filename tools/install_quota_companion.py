"""Install the authorized local companion; not a Codex scheduled task."""
import os
from pathlib import Path
import plistlib
import subprocess
ROOT=Path(__file__).resolve().parents[1]
folder=Path.home()/'Library/Application Support/SalaryCounter'
folder.mkdir(parents=True,exist_ok=True)
binary=folder/'quota_ble'
binary.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(['swiftc',str(ROOT/'tools/quota_ble.swift'),'-o',str(binary),'-framework','CoreBluetooth','-Xlinker','-sectcreate','-Xlinker','__TEXT','-Xlinker','__info_plist','-Xlinker',str(ROOT/'tools/quota_ble_Info.plist')],check=True)
agent=Path.home()/'Library/LaunchAgents/local.deskmate.quota.plist'
config={'Label':'local.deskmate.quota','ProgramArguments':[str(ROOT/'.venv/bin/python'),str(ROOT/'tools/quota_companion.py')],
        'WorkingDirectory':str(folder),'RunAtLoad':True,'KeepAlive':True,'ThrottleInterval':15,'ProcessType':'Interactive',
        'StandardOutPath':str(folder/'quota.log'),'StandardErrorPath':str(folder/'quota-error.log')}
config['EnvironmentVariables']={'SALARY_USB_SERIAL':os.environ['SALARY_USB_SERIAL']}
agent.parent.mkdir(parents=True,exist_ok=True)
if agent.exists():subprocess.run(['launchctl','bootout',f'gui/{os.getuid()}',str(agent)],check=False)
with agent.open('wb') as f:plistlib.dump(config,f)
subprocess.run(['launchctl','bootstrap',f'gui/{os.getuid()}',str(agent)],check=True)
print('Installed local.deskmate.quota; existing clock companion unchanged')
