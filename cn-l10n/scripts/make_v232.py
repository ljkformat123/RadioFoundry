"""生成 v232：只改 0x28E61 那 8 字节（Off Duty 的译文 状态指示 -> 休息）

编码：前缀值 = 页号+1；休 = 页1/码8D（前缀 02），息 = 页0/码CA（前缀 01）。
"""
import hashlib

src = r"nicFW_C2_v231.bin"
dst = r"nicFW_C2_v232.bin"

fw = bytearray(open(src, "rb").read())
print("原槽:", fw[0x28E61:0x28E69].hex(" ").upper())
fw[0x28E61:0x28E69] = bytes([0x02, 0x8D, 0x01, 0xCA, 0x20, 0x20, 0x20, 0x20])
open(dst, "wb").write(fw)
print("新槽:", fw[0x28E61:0x28E69].hex(" ").upper())
print("md5:", hashlib.md5(bytes(fw)).hexdigest().upper())
