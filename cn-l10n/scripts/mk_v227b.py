from keystone import *
from capstone import *
import hashlib
ks=Ks(KS_ARCH_ARM, KS_MODE_THUMB)
md=Cs(CS_ARCH_ARM, CS_MODE_THUMB)
o=open(r"刷机工具\固件\nicfw880_v5.18.07.bin","rb").read()
v226=open(r"刷机工具\固件\nicFW_C2_v226.bin","rb").read()
img=bytearray(v226)

entry_src="mov.w sl,#1\nmovs r0,#0\nstr r0,[sp,#8]\nb.w 0x75dc\n"
pre_src="""
cmp fp,#0x20
bhs Lout
sub.w r0,fp,#0xa
cmp r0,#4
blo Lout
subs.w fp,fp,#1
ldrh r1,[r6,#8]
lsls r1,r1,#8
mul r1,r1,fp
str r1,[sp,#8]
uxtb.w r0,sl
ldrb.w fp,[r7,r0]
add.w sl,sl,#1
cmp fp,#0
beq Lend
Lout:
b.w 0x7604
Lend:
b.w 0x765c
"""
gly_src="""
ldr r0,[r6]
cmp fp,#0x80
blo Lno
ldr r3,[sp,#8]
cbz r0,Lno
add r0,r3
sub.w r3,fp,#0x20
b.w 0x7618
Lno:
sub.w r3,fp,#0x20
b.w 0x762c
"""
E=bytes(ks.asm(entry_src.encode(),0x1FDCC)[0])
pre_addr=0x1FDCC+len(E)
P=bytes(ks.asm(pre_src.encode(),pre_addr)[0])
gly_addr=pre_addr+len(P)
G=bytes(ks.asm(gly_src.encode(),gly_addr)[0])
end=gly_addr+len(G)
print("entry %d@1FDCC  pre %d@%05X  gly %d@%05X  末端 %05X (上限 1FE2C)"%(len(E),len(P),pre_addr,len(G),gly_addr,end))
assert end<=0x1FE2C
img[0x1FDCC:0x1FEAC]=o[0x1FDCC:0x1FEAC]
for i in range(0x1FDCC,0x1FE2C): img[i]=0
img[0x1FDCC:0x1FDCC+len(E)]=E
img[pre_addr:pre_addr+len(P)]=P
img[gly_addr:gly_addr+len(G)]=G
for i in range(128): img[0x1FE2C+i]=0x80+i
for addr,t in ((0x75D6,0x1FDCC),(0x75EA,pre_addr),(0x7610,gly_addr)):
    b=bytes(ks.asm(("b.w 0x%05X"%t).encode(),addr)[0]); assert len(b)==4
    img[addr:addr+4]=b
img2=bytes(img)
open(r"刷机工具\固件\nicFW_C2_v227b.bin","wb").write(img2)
print("v227b md5=%s"%hashlib.md5(img2).hexdigest().upper())
print("--- pre ---")
for i in md.disasm(P,pre_addr): print("   0x%05X %-12s %-8s %s"%(i.address,i.bytes.hex(),i.mnemonic,i.op_str))
print("--- gly ---")
for i in md.disasm(G,gly_addr): print("   0x%05X %-12s %-8s %s"%(i.address,i.bytes.hex(),i.mnemonic,i.op_str))
print("钩子:",[img2[a:a+4].hex() for a in (0x75D6,0x75EA,0x7610)])
print("0x1FEAC 段保持原厂:",img2[0x1FEAC:0x1FEB0].hex(" ").upper(),"| 原厂:",o[0x1FEAC:0x1FEB0].hex(" ").upper())
