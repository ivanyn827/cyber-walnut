"""Native USB-JTAG serial: never issue modem-line ioctls during open.

pyserial applies DTR then RTS separately even if both were assigned before open.
ESP32-C6 interprets intermediate control-line combinations as reset/boot signals.
This board uses neither hardware flow control nor modem lines for application I/O.
Flashing continues to use esptool's explicit reset sequence, not this class.
"""
import serial
class DeviceSerial(serial.Serial):
    def _update_dtr_state(self):
        pass
    def _update_rts_state(self):
        pass
