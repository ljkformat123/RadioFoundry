import hashlib
src=r"刷机工具\固件\nicFW_C2_v228.bin"
img=bytearray(open(src,"rb").read())
LAB=[(0x28E28,"Primary"),(0x285F2,"Vehicle"),(0x28607,"Mobile"),(0x28AF1,"Camper"),(0x286C2,"Home"),
     (0x285E9,"Portable"),(0x28897,"Special"),(0x28995,"Human"),(0x28408,"BBS"),(0x28734,"I-Gate"),
     (0x28AC8,"Weather"),(0x28742,"Satellite"),(0x28AF8,"Digipeater"),(0x28D17,"Experiment"),(0x28858,"Truck"),
     (0x28E05,"Emergency"),(0x27C21,"Both")]
log=[]
for off,en in LAB:
    old=bytes(img[off:off+len(en)])
    # 断言：原位之后紧跟 0 或 空格填充，不会越界串词
    assert img[off+len(en)] in (0,0x20), hex(off)
    img[off:off+len(en)]=en.encode()
    log.append("%06X %-14s <- %s"%(off,en,old.hex(" ")))
TAB=[(0xB5,0x05),(0xC3,0x06),(0xE5,0x07),(0xF1,0x08),(0xFB,0x09),(0xE8,0x0E)]
tlog=[]
for code,val in TAB:
    a=0x1FE2C+(code-0x80)
    assert img[a]==code, (hex(a),img[a])
    img[a]=val
    tlog.append("表%02X: %02X -> %02X"%(code,code,val))
out=r"刷机工具\固件\nicFW_C2_v230.bin"
open(out,"wb").write(bytes(img))
h=hashlib.md5(bytes(img)).hexdigest().upper()
print("生成 %s  %d B  md5 %s"%(out,len(img),h))
print("--- 字符串还原 ---"); print("\n".join(log))
print("--- 表改扳机 ---"); print("\n".join(tlog))
