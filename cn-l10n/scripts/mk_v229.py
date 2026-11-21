import hashlib,os
STOCK=r"刷机工具\固件\nicFW_C2_v228.bin"
OUT  =r"刷机工具\固件\nicFW_C2_v229.bin"
img=bytearray(open(STOCK,"rb").read())
# 1) 六条串还原英文（原厂长度）
STR=[(0x285F2,b"Vehicle"),(0x286C2,b"Home"),(0x28858,b"Truck"),(0x28995,b"Human"),(0x28AF1,b"Camper"),(0x27C21,b"Both")]
for a,t in STR:
    e=a
    while img[e]!=0: e+=1
    assert e-a>=len(t),"槽 0x%X 长 %d < %d"%(a,e-a,len(t))
    img[a:e]=(t+b"\x20"*(e-a-len(t)))
    print("0x%05X 槽%dB <- %s"%(a,e-a,t.decode()))
# 2) 页0 码位改作切页符（表 0x1FE2C 起，索引=码-0x80；值=页号+1）
SW=[(0xB5,0x05,"卡->页4"),(0xC3,0x06,"庭->页5"),(0xE5,0x07,"类->页6"),(0xF1,0x08,"车->页7"),(0xFB,0x09,"露->页8")]
for c,v,note in SW:
    o=0x1FE2C+(c-0x80)
    old=img[o]
    assert old==c,"0x%02X 表位不符(现 %02X)"%(c,old)
    img[o]=v
    print("表 0x%05X: %02X -> %02X  (%s)"%(o,old,v,note))
open(OUT,"wb").write(bytes(img))
print("生成",OUT,len(img),"B md5",hashlib.md5(bytes(img)).hexdigest().upper())
# 3) 差异清单
a=open(STOCK,"rb").read()
dif=[i for i in range(len(a)) if a[i]!=img[i]]
print("差异字节 %d 处："%len(dif))
rng=[];s=dif[0];p=dif[0]
for i in dif[1:]:
    if i==p+1: p=i
    else: rng.append((s,p)); s=p=i
rng.append((s,p))
for s,e in rng: print("  0x%05X-0x%05X  %s -> %s"%(s,e,a[s:e+1].hex(" ").upper(),bytes(img[s:e+1]).hex(" ").upper()))
