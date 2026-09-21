"""Shared advisory lock for the companion, flashing and manual serial tools."""
import contextlib
import fcntl
import time
from pathlib import Path

@contextlib.contextmanager
def port_lock(timeout=0):
    folder=Path.home()/'Library/Application Support/SalaryCounter'
    folder.mkdir(parents=True,exist_ok=True)
    with (folder/'serial.lock').open('a') as stream:
        deadline=time.monotonic()+timeout
        while True:
            try:
                fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic()>=deadline:
                    raise TimeoutError('Salary Counter serial port is in use')
                time.sleep(.1)
        try:
            yield
        finally:
            fcntl.flock(stream,fcntl.LOCK_UN)
