import datetime
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import auto_sync

class Port:
    def __init__(self,lines): self.lines=list(lines); self.writes=[]
    def reset_input_buffer(self): pass
    def write(self,data): self.writes.append(data)
    def readline(self): return (self.lines.pop(0)+'\n').encode()

class TestSync(unittest.TestCase):
    @patch.object(auto_sync, "DEVICE_SERIAL", "0123456789AB")
    def test_identity(self):
        p=SimpleNamespace(vid=0x303A,pid=0x1001,serial_number='01:23:45:67:89:AB')
        self.assertTrue(auto_sync.matches(p))
        p.serial_number='another'; self.assertFalse(auto_sync.matches(p))
        p.serial_number=None; self.assertFalse(auto_sync.matches(p))
    def test_roundtrip(self):
        epoch=datetime.datetime(2026,9,8,7,30,tzinfo=datetime.timezone.utc).timestamp()
        p=Port(['unrelated log','SALARY_CLOCK_V1','TIME_OK','STATUS clock=1 date=2026-09-08 time=15:30:00'])
        with patch.object(auto_sync.time,'time',return_value=epoch): auto_sync.exchange(p)
        self.assertEqual(p.writes,[b'HELLO\n',f'TIME {int(epoch)}\n'.encode()])
    def test_unknown_firmware_not_modified(self):
        p=Port([])
        with patch.object(auto_sync,'read_until',side_effect=TimeoutError), self.assertRaises(TimeoutError): auto_sync.exchange(p)
        self.assertEqual(p.writes,[b'HELLO\n'])
    def test_rejected_rtc(self):
        p=Port(['SALARY_CLOCK_V1','TIME_ERROR'])
        with self.assertRaises(RuntimeError): auto_sync.exchange(p)
    def test_invalid_readback(self):
        p=Port(['SALARY_CLOCK_V1','TIME_OK','STATUS clock=0'])
        with self.assertRaises(RuntimeError): auto_sync.exchange(p)
    def test_stale_readback(self):
        p=Port(['SALARY_CLOCK_V1','TIME_OK','STATUS clock=1 date=2026-09-08 time=15:30:00'])
        with patch.object(auto_sync.time,'time',return_value=0), self.assertRaises(RuntimeError): auto_sync.exchange(p)

if __name__=='__main__': unittest.main()
