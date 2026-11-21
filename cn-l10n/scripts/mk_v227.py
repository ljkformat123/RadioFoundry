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
cmp fp,#0xa
beq Lout
cmp fp,#0xd
beq Lout
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
P=bytes(ks.asm(pre_src.encode(),0x1FDDC)[0])
G=bytes(ks.asm(gly_src.encode(),0x1FEAC)[0])
print("entry %d  pre %d  gly %d  末端 0x%05X"%(len(E),len(P),len(G),0x1FDDC+len(P)))
assert 0x1FDDC+len(P)<=0x1FE2C
def bad(code,base):
    out=[]
    for i in md.disasm(code,base):
        if i.mnemonic in ("b","b.w") or i.mnemonic.startswith("b") and i.mnemonic not in ("bl","blx"):
            t=i.op_str.lstrip("#")
            try: t=int(t,16)
            except: continue
            if not (0x1FD00<=t<=0x7680): out.append((hex(i.address),i.mnemonic,hex(t)))
    return out
print("pre 异常分支:",bad(P,0x1FDDC))
print("gly 异常分支:",bad(G,0x1FEAC))
print("--- pre ---")
for i in md.disasm(P,0x1FDDC): print("   0x%05X %-12s %-8s %s"%(i.address,i.bytes.hex(),i.mnemonic,i.op_str))
print("--- gly ---")
for i in md.disasm(G,0x1FEAC): print("   0x%05X %-12s %-8s %s"%(i.address,i.bytes.hex(),i.mnemonic,i.op_str))
img[0x1FDCC:0x1FEAC]=o[0x1FDCC:0x1FEAC]
for i in range(0x1FDCC,0x1FE2C): img[i]=0
img[0x1FDCC:0x1FDCC+len(E)]=E
for i in range(0x1FDCC+len(E),0x1FDDC): img[i]=0
img[0x1FDDC:0x1FDDC+len(P)]=P
for i in range(128): img[0x1FE2C+i]=0x80+i
img[0x1FEAC:0x1FEAC+len(G)]=G
for addr,t in ((0x75D6,0x1FDCC),(0x75EA,0x1FDDC),(0x7610,0x1FEAC)):
    b=bytes(ks.asm(("b.w 0x%05X"%t).encode(),addr)[0]); assert len(b)==4
    img[addr:addr+4]=b
img2=bytes(img)
open(r"刷机工具\固件\nicFW_C2_v227.bin","wb").write(img2)
print("v227 md5=%s"%hashlib.md5(img2).hexdigest().upper())
for a in (0x75D6,0x75EA,0x7610):
    for i in md.disasm(img2[a:a+4],a): print("  钩子 0x%05X %-12s %s %s"%(i.address,i.bytes.hex(),i.mnemonic,i.op_str))
print("  表头:",img2[0x1FE2C:0x1FE3C].hex(" ").upper(),"  表尾:",img2[0x1FEA8:0x1FEAC].hex(" ").upper())
