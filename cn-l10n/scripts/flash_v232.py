import serial, time, sys
FW = r"刷机工具\固件\nicFW_C2_v232.bin"
img = open(FW, "rb").read()
pad = (1024 - len(img) % 1024) % 1024
if pad:
    img = img + b"\xFF" * pad
blocks = len(img) // 1024

s = serial.Serial("COM3", 115200, timeout=5)
time.sleep(0.3)
s.reset_input_buffer()

def pkt(t, addr, data, seed):
    p = bytes([t, (addr >> 8) & 0xFF, addr & 0xFF]) + data
    cs = seed
    for b in p: cs = (cs + b) & 0xFF
    return p + bytes([cs])

def ack():
    r = s.read(1)
    return r[0] if r else -1

seed = None
for sd in (0x43, 0x52):
    s.write(pkt(0x39, 0x3305, b"\x10", sd))
    if ack() == 0x06:
        seed = sd
        break
if seed is None:
    print("!! 种子无应答"); sys.exit(1)
print("种子 0x%02X" % seed)

s.write(pkt(0x39, 0x3305, b"\x10", seed))
if ack() != 0x06: print("!! 擦除1无应答"); sys.exit(1)
s.write(pkt(0x39, 0x3305, b"\x55", seed))
if ack() != 0x06: print("!! 擦除2无应答"); sys.exit(1)
print("已擦除，开始写入 %d 块" % blocks)

t0 = time.time()
for i in range(blocks):
    a = i * 1024
    s.write(pkt(0x57, a, img[a:a+1024], seed))
    r = ack()
    if r != 0x06:
        print("!! 块 %d/%d 失败 应答=%s" % (i+1, blocks, r)); sys.exit(1)
    if (i+1) % 20 == 0 or i+1 == blocks:
        print("  写入 %d/%d  %.1f 秒" % (i+1, blocks, time.time()-t0))
final = ack()
print("末次应答:", "OK" if final == 0x06 else "无（正常现象）")
s.close()
print("刷写完成")
