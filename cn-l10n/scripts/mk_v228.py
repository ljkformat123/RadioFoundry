import hashlib,json
v=open(r"刷机工具\固件\nicFW_C2_v227b.bin","rb").read()
img=bytearray(v)
m=json.load(open("_hancode_map.json",encoding="utf-8"))
p0=m["0"]; used={int(k,16) for k in p0}
free=[c for c in range(0x80,0x100) if c not in used]
print("页0 空槽:",[hex(c) for c in free])
# 把空槽改造成切页符：0x90->页1(前缀02)、0x93->页2(03)、0x95->页3(04)
for code,prefix in ((0x90,0x02),(0x93,0x03),(0x95,0x04)):
    img[0x1FE2C+(code-0x80)]=prefix
out=bytes(img)
open(r"刷机工具\固件\nicFW_C2_v228.bin","wb").write(out)
print("v228 md5=%s"%hashlib.md5(out).hexdigest().upper())
print("改动前后:",v[0x1FE3C],v[0x1FE3F],v[0x1FE41],"->",out[0x1FE3C],out[0x1FE3F],out[0x1FE41])
