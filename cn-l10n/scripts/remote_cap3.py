# -*- coding: utf-8 -*-
"""remote_cap3：远程镜像抓当前整屏（终态快照，按 y/x 排布），不按方向键"""
import serial, time

SYM = {32: "空格", 33: "锁", 34: "PTT-ID", 35: "VOX", 36: "扫描", 37: "暂停", 38: "上折",
       39: "钥匙", 40: "循环", 41: "上箭头", 42: "下箭头", 43: "左箭头", 44: "右箭头",
       45: "-", 46: "+", 47: "警告三角", 48: "XB", 49: "月亮", 50: "雨云", 51: "音符",
       52: "充电", 53: "实心圆", 54: "GPS无锁", 55: "GPS锁定", 56: "APRS图标",
       57: "APRS+信标", 58: "静音", 59: "APRS999无自动信标", 60: "APRS999有自动信标",
       61: "循环灰", 62: "循环G组"}

WIDTH = {0: 8, 1: 8, 2: 16, 3: 16, 4: 24, 5: 24, 6: 16}

ser = serial.Serial("COM3", 38400, timeout=0.2, write_timeout=3)
time.sleep(0.4)
ser.reset_input_buffer()
ser.write(b"\xaa\x51")
time.sleep(0.4)

buf = bytearray()
cells = {}      # (y,x) -> (text, font)

def parse():
    while True:
        i = buf.find(0x55)
        if i < 0:
            buf.clear(); return
        del buf[:i]
        if len(buf) < 3:
            return
        t = buf[1]
        if t == 0x02:
            z = buf.find(0, 10)
            if z < 0 or len(buf) < z + 2:
                return
            cells[(buf[3] | (buf[4] << 8), buf[2])] = (bytes(buf[10:z]), buf[5])
            del buf[:z + 2]
        elif t == 0x01:
            if len(buf) < 11:
                return
            del buf[:11]
        elif t == 0x03:
            if len(buf) < 4:
                return
            del buf[:4]
        else:
            del buf[:1]

t0 = time.time(); last = 0
while time.time() - t0 < 3.0:
    if time.time() - last > 0.35:
        ser.write(b"\xaa"); last = time.time()
    buf.extend(ser.read(ser.in_waiting or 0))
    parse()
    time.sleep(0.02)
ser.write(b"\x52")
time.sleep(0.2)
ser.close()

print("拿到 %d 个文字格" % len(cells))
for y in sorted(set(k[0] for k in cells)):
    segs = [(x, cells[(y, x)]) for x in sorted(k[1] for k in cells if k[0] == y)]
    print("\ny=%d" % y)
    for x, (txt, font) in segs:
        if font == 6:
            s = "/".join(SYM.get(b, "<%d>" % b) for b in txt)
        else:
            s = txt.decode("ascii", "replace")
        print("   x=%3d f=%d  %r  ->  %s" % (x, font, txt, s))
