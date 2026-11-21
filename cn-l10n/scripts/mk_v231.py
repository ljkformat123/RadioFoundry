import hashlib
img=bytearray(open(r"刷机工具\固件\nicFW_C2_v230.bin","rb").read())
for code in (0xE5,0xF1,0xFB,0xE8):
    a=0x1FE2C+(code-0x80); print("表%02X: %02X -> %02X"%(code,img[a],code)); img[a]=code
out=r"刷机工具\固件\nicFW_C2_v231.bin"
open(out,"wb").write(bytes(img))
print("生成 %s  %d B  md5 %s"%(out,len(img),hashlib.md5(bytes(img)).hexdigest().upper()))
print("保留: 表B5=%02X 表C3=%02X"%(img[0x1FE2C+0x35],img[0x1FE2C+0x43]))
