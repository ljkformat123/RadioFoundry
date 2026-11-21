"""nicFW 引导器刷机（无界面）。用法：python flash_fw.py <固件.bin> [COM3]

电台须先进入刷机模式：关机 → 按住 PTT 开机 → 屏全黑。
"""
import sys, time, serial

def pkt(t, addr, data, seed):
    p = bytes([t, (addr >> 8) & 0xFF, addr & 0xFF]) + data
    cs = seed
    for b in p:
        cs = (cs + b) & 0xFF
    return p + bytes([cs])

def main():
    fw = sys.argv[1] if len(sys.argv) > 1 else r"nicFW_C2_v232.bin"
    port = sys.argv[2] if len(sys.argv) > 2 else "COM3"
    img = open(fw, "rb").read()
    pad = (1024 - len(img) % 1024) % 1024
    if pad:
        img += b"\xff" * pad
    blocks = len(img) // 1024

    s = serial.Serial(port, 115200, timeout=5)
    time.sleep(0.3)
    s.reset_input_buffer()

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
        print("!! 无应答：确认已进刷机模式、串口正确")
        return 1
    print("种子 0x%02X" % seed)

    s.write(pkt(0x39, 0x3305, b"\x10", seed))
    if ack() != 0x06:
        print("!! 擦除 1 无应答"); return 1
    s.write(pkt(0x39, 0x3305, b"\x55", seed))
    if ack() != 0x06:
        print("!! 擦除 2 无应答"); return 1
    print("已擦除，写入 %d 块" % blocks)

    t0 = time.time()
    for i in range(blocks):
        a = i * 1024
        s.write(pkt(0x57, a, img[a:a + 1024], seed))
        if ack() != 0x06:
            print("!! 块 %d/%d 失败" % (i + 1, blocks)); return 1
        if (i + 1) % 20 == 0 or i + 1 == blocks:
            print("  %d/%d  %.1f 秒" % (i + 1, blocks, time.time() - t0))
    final = ack()
    s.close()
    print("末次应答:", "OK" if final == 0x06 else "无（正常）")
    print("完成")
    return 0

if __name__ == "__main__":
    sys.exit(main())
