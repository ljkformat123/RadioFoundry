# -*- coding: utf-8 -*-
"""生成 v232：只改 0x28E61 那 8 字节（Off Duty 的译文 状态指示 -> 休息）"""
src = r"交付_v231_写频全套\2_固件\nicFW_C2_v231.bin"
dst = r"刷机工具\固件\nicFW_C2_v232.bin"

fw = bytearray(open(src, "rb").read())
old = bytes(fw[0x28E61:0x28E69])
# 休 = 页1 码8D（前缀 0x02=页1）；息 = 页0 码CA（先切回页0，前缀 0x01）
new = bytes([0x02, 0x8D, 0x01, 0xCA, 0x20, 0x20, 0x20, 0x20])
fw[0x28E61:0x28E69] = new

open(dst, "wb").write(fw)

# 复核
chk = open(dst, "rb").read()
print("原槽    :", " ".join("%02X" % x for x in old))
print("新槽    :", " ".join("%02X" % x for x in chk[0x28E61:0x28E69]))
print("槽后 4B :", " ".join("%02X" % x for x in chk[0x28E69:0x28E6D]))
diff = [i for i in range(len(fw)) if bytes(fw)[i] != bytes(open(src, "rb").read())[i]]
print("全镜像差异字节数:", len(diff), "位置:", ["0x%05X" % d for d in diff])
print("大小:", len(chk))
import hashlib
print("v231 md5:", hashlib.md5(open(src, "rb").read()).hexdigest().upper())
print("v232 md5:", hashlib.md5(chk).hexdigest().upper())
