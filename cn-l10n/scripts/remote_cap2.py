# -*- coding: utf-8 -*-
"""remote_cap2：COM3 进远程模式，按上下键强制重绘，重建当前页文字镜像"""
import serial, time

SYM = {32: "空格", 33: "锁", 34: "PTT-ID", 35: "VOX", 36: "扫描", 37: "暂停", 38: "上折",
       39: "钥匙", 40: "循环", 41: "上箭头", 42: "下箭头", 43: "左箭头", 44: "右箭头",
       45: "-", 46: "+", 47: "警告三角", 48: "XB", 49: "月亮", 50: "雨云", 51: "音符",
       52: "充电", 53: "实心圆", 54: "GPS无锁", 55: "GPS锁定", 56: "APRS图标",
       57: "APRS+信标", 58: "静音", 59: "APRS999无自动信标", 60: "APRS999有自动信标",
       61: "循环灰", 62: "循环G组"}

ser = serial.Serial("COM3", 38400, timeout=0.2, write_timeout=3)
time.sleep(0.4)
ser.reset_input_buffer()
ser.write(b"\xaa\x51")
time.sleep(0.4)

buf = bytearray()
texts = []   # 全部文本包（按到达顺序）
rects = []

def parse():
    while True:
        i = buf.find(0x55)
        if i < 0:
            buf.clear()
            return
        del buf[:i]
        if len(buf) < 3:
            return
        t = buf[1]
        if t == 0x02:
            z = buf.find(0, 10)
            if z < 0 or len(buf) < z + 2:
                return
            texts.append((buf[2], buf[3] | (buf[4] << 8), buf[5], bytes(buf[10:z])))
            del buf[:z + 2]
        elif t == 0x01:
            if len(buf) < 11:
                return
            rects.append((buf[2], buf[3] | (buf[4] << 8), buf[5], buf[6] | (buf[7] << 8)))
            del buf[:11]
        elif t == 0x03:
            if len(buf) < 4:
                return
            del buf[:4]
        else:
            del buf[:1]

def pump(sec):
    t0 = time.time(); last = 0
    while time.time() - t0 < sec:
        if time.time() - last > 0.35:
            ser.write(b"\xaa"); last = time.time()
        buf.extend(ser.read(ser.in_waiting or 0))
        parse()
        time.sleep(0.02)

def key(b):
    ser.write(bytes([b])); time.sleep(0.15); ser.write(bytes([0xFF]))

pump(1.2)
key(0x0E)          # DOWN
pump(1.4)
key(0x0D)          # UP 回原位
pump(1.4)
ser.write(b"\x52")
time.sleep(0.2)
ser.close()

print("文本包 %d 条, 矩形 %d 条" % (len(texts), len(rects)))
print("\n=== 按 y 排序的全部文本包 ===")
for x, y, font, raw in sorted(texts, key=lambda t: (t[1], t[0])):
    if font == 6:
        s = "/".join(SYM.get(b, "<%d>" % b) for b in raw)
    else:
        s = raw.decode("ascii", "replace")
    print("y=%3d x=%3d font=%d %-46r %s" % (y, x, font, raw, s))

print("\n=== 矩形 ===")
for x, y, w, h in sorted(rects, key=lambda t: (t[1], t[0])):
    print("y=%3d x=%3d w=%3d h=%3d" % (y, x, w, h))
