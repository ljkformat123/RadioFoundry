# -*- coding: utf-8 -*-
"""CHIRP 模块：UV98 / RT880 nicFW 汉化版（管理信道表，38400 AA 协议）"""
import logging
import struct
import time

from chirp import chirp_common, directory, errors, memmap

LOG = logging.getLogger(__name__)

P4_MAP = {
    '东': 0x80,
    '乌': 0x81,
    '云': 0x82,
    '京': 0x83,
    '兰': 0x84,
    '内': 0x85,
    '凌': 0x86,
    '北': 0x87,
    '南': 0x88,
    '合': 0x89,
    '吉': 0x8A,
    '和': 0x8B,
    '咸': 0x8C,
    '哈': 0x8D,
    '商': 0x8E,
    '四': 0x8F,
    '夏': 0x90,
    '太': 0x91,
    '宁': 0x92,
    '宝': 0x93,
    '尔': 0x94,
    '山': 0x95,
    '川': 0x96,
    '州': 0x97,
    '广': 0x98,
    '庄': 0x99,
    '庆': 0x9A,
    '康': 0x9B,
    '建': 0x9C,
    '徽': 0x9D,
    '拉': 0x9E,
    '昆': 0x9F,
    '昌': 0xA0,
    '明': 0xA1,
    '春': 0xA2,
    '木': 0xA3,
    '杨': 0xA4,
    '杭': 0xA5,
    '林': 0xA6,
    '榆': 0xA7,
    '武': 0xA8,
    '汉': 0xA9,
    '江': 0xAA,
    '沈': 0xAB,
    '河': 0xAC,
    '洛': 0xAD,
    '津': 0xAE,
    '济': 0xAF,
    '浙': 0xB0,
    '浩': 0xB1,
    '渭': 0xB2,
    '港': 0xB3,
    '湖': 0xB4,
    '湾': 0xB5,
    '滨': 0xB6,
    '澳': 0xB7,
    '甘': 0xB8,
    '疆': 0xB9,
    '石': 0xBA,
    '福': 0xBB,
    '肃': 0xBC,
    '肥': 0xBD,
    '苏': 0xBE,
    '萨': 0xBF,
    '蒙': 0xC0,
    '藏': 0xC1,
    '西': 0xC2,
    '贵': 0xC3,
    '辽': 0xCD,
    '郑': 0xCE,
    '都': 0xCF,
    '铜': 0xD0,
    '银': 0xD1,
    '阳': 0xD2,
    '陕': 0xD3,
    '青': 0xD4,
    '香': 0xD5,
    '鲁': 0xD6,
    '鸡': 0xD7,
    '黑': 0xD8,
    '齐': 0xD9,
    '龙': 0xDA,
    '备': 0xDB,
    '应': 0xDC,
    '直': 0xDD,
    '下': 0xDE,
    '地': 0xDF,
    '队': 0xE0,
    '顶': 0xE1,
    '隧': 0xE2,
    '桥': 0xE3,
    '梁': 0xE4,
    '通': 0xE5,
    '联': 0xE6,
    '连': 0xE7,
    '载': 0xE8,
    '徒': 0xE9,
    '野': 0xEA,
    '市': 0xEB,
    '城': 0xEC,
    '镇': 0xED,
    '铁': 0xEE,
    '塔': 0xEF,
    '楼': 0xF0,
    '场': 0xF1,
    '医': 0xF2,
    '工': 0xF3,
    '政': 0xF4,
    '府': 0xF5,
    '边': 0xF6,
    '的': 0xF7,
    '一': 0xF8,
    '是': 0xF9,
    '不': 0xFA,
    '有': 0xFB,
    '我': 0xFC,
    '他': 0xFD,
    '这': 0xFE,
    '们': 0xFF,
}

P5_MAP = {
    '来': 0x80,
    '为': 0x81,
    '驾': 0x82,
    '沙': 0x83,
    '国': 0x84,
    '频': 0x85,
    '安': 0x86,
    '延': 0x87,
    '天': 0x88,
    '上': 0x89,
    '海': 0x8A,
    '重': 0x8B,
    '古': 0x8C,
    '门': 0x8D,
    '长': 0x8E,
    '口': 0x8F,
    '呼': 0x91,
    '特': 0x92,
    '宝': 0x96,
    '山': 0x97,
    '夏': 0x98,
    '湾': 0x99,
    '贵': 0x9A,
    '成': 0x9B,
    '中': 0x9C,
    '继': 0x9D,
}

P0_MAP = {
    '偏': 0x80,
    '入': 0x81,
    '表': 0x82,
    '准': 0x83,
    '道': 0x84,
    '功': 0x85,
    '加': 0x86,
    '卫': 0x87,
    '反': 0x88,
    '名': 0x89,
    '向': 0x8A,
    '周': 0x8B,
    '噪': 0x8C,
    '回': 0x8D,
    '射': 0x8E,
    '晶': 0x8F,
    '振': 0x91,
    '器': 0x92,
    '扰': 0x94,
    '星': 0x96,
    '显': 0x97,
    '暂': 0x98,
    '期': 0x99,
    '机': 0x9A,
    '段': 0x9B,
    '预': 0x9C,
    '设': 0x9D,
    '读': 0x9E,
    '两': 0x9F,
    '义': 0xA0,
    '交': 0xA1,
    '人': 0xA2,
    '仅': 0xA3,
    '伽': 0xA4,
    '位': 0xA5,
    '松': 0xA6,
    '体': 0xA7,
    '信': 0xA8,
    '克': 0xA9,
    '全': 0xAA,
    '公': 0xAB,
    '共': 0xAC,
    '关': 0xAD,
    '删': 0xAE,
    '制': 0xAF,
    '剧': 0xB0,
    '动': 0xB1,
    '化': 0xB2,
    '单': 0xB3,
    '占': 0xB4,
    '卡': 0xB5,
    '厂': 0xB6,
    '原': 0xB7,
    '发': 0xB8,
    '取': 0xB9,
    '台': 0xBA,
    '图': 0xBB,
    '束': 0xBC,
    '始': 0xBD,
    '定': 0xBE,
    '实': 0xBF,
    '家': 0xC0,
    '宽': 0xC1,
    '带': 0xC2,
    '庭': 0xC3,
    '开': 0xC4,
    '式': 0xC5,
    '弹': 0xC6,
    '循': 0xC7,
    '心': 0xC8,
    '态': 0xC9,
    '息': 0xCA,
    '戏': 0xCB,
    '收': 0xCC,
    '数': 0xCD,
    '新': 0xCE,
    '旅': 0xCF,
    '无': 0xD0,
    '日': 0xD1,
    '时': 0xD2,
    '滤': 0xD3,
    '极': 0xD4,
    '标': 0xD5,
    '校': 0xD6,
    '格': 0xD7,
    '正': 0xD8,
    '消': 0xD9,
    '焦': 0xDA,
    '状': 0xDB,
    '环': 0xDC,
    '用': 0xDD,
    '盘': 0xDE,
    '码': 0xDF,
    '社': 0xE0,
    '移': 0xE1,
    '空': 0xE2,
    '窄': 0xE3,
    '窗': 0xE4,
    '类': 0xE5,
    '组': 0xE6,
    '结': 0xE7,
    '者': 0xE8,
    '聚': 0xE9,
    '育': 0xEA,
    '自': 0xEB,
    '超': 0xEC,
    '行': 0xED,
    '模': 0xEE,
    '解': 0xEF,
    '起': 0xF0,
    '车': 0xF1,
    '步': 0xF2,
    '部': 0xF3,
    '锁': 0xF4,
    '闲': 0xF6,
    '进': 0xF7,
    '除': 0xF8,
    '隆': 0xF9,
    '零': 0xFA,
    '露': 0xFB,
    '马': 0xFC,
    '高': 0xFD,
}

FIRST_SLOT = 4
SLOT = 62
NAME_OFF = 32
NAME_LEN = 30
BASE_ADDR = 0x50200
IMG_ADDR = 0x50000
IMG_SIZE = 0x2000
RESERVED = (0x90, 0x93, 0x95, 0xB5, 0xC3)
TONE_OFF = 12
DCS_FLAG = 0x8000

P4R = {v: k for k, v in P4_MAP.items()}
P5R = {v: k for k, v in P5_MAP.items()}
P0R = {v: k for k, v in P0_MAP.items()}

VALID_CHARS = "".join(sorted(set(
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    " -_.+/,()#@:" +
    "".join(P0_MAP) + "".join(P4_MAP) + "".join(P5_MAP))))


def _enc(name):
    out = []
    for ch in name:
        if ord(ch) < 128:
            out.append(ord(ch))
        elif ch in P4_MAP and P4_MAP[ch] not in RESERVED:
            out += [0xB5, P4_MAP[ch]]
        elif ch in P5_MAP and P5_MAP[ch] not in RESERVED:
            out += [0xC3, P5_MAP[ch]]
        elif ch in P0_MAP and P0_MAP[ch] not in RESERVED:
            out.append(P0_MAP[ch])
        else:
            out.append(0x3F)
    return bytes(out)


def _dec(nb):
    out, i = "", 0
    while i < len(nb) and nb[i]:
        b = nb[i]
        if b == 0xB5 and i + 1 < len(nb):
            out += P4R.get(nb[i + 1], "?")
            i += 2
        elif b == 0xC3 and i + 1 < len(nb):
            out += P5R.get(nb[i + 1], "?")
            i += 2
        else:
            out += P0R.get(b, "?") if b >= 0x80 else chr(b)
            i += 1
    return out


def _drain(pipe):
    """尽量清掉串口残留，不同 CHIRP 版本方法名不同"""
    for name in ("reset_input_buffer", "flush_input", "flush"):
        f = getattr(pipe, name, None)
        if callable(f):
            try:
                f()
                return
            except Exception:
                pass


def _read_block(radio, addr):
    for _ in range(8):
        _drain(radio.pipe)
        radio.pipe.write(bytes([0xAA, 0x30]) + struct.pack("<I", addr) + b"\x55")
        time.sleep(0.18)
        r = b""
        while len(r) < 135:
            chunk = radio.pipe.read(135 - len(r))
            if not chunk:
                break
            r += chunk
        if len(r) >= 135 and r[0] == 0xAA and r[1] == 0x30:
            d = r[6:134]
            ck = (sum(d) + sum(struct.pack("<I", addr))) & 0xFF
            if ck == r[134]:
                return bytes(d)
        time.sleep(0.2)
    raise errors.RadioError("读 %06X 失败" % addr)


def _write_sector(radio, addr, data):
    ck = (sum(data) + sum(struct.pack("<I", addr))) & 0xFF
    for _ in range(4):
        _drain(radio.pipe)
        radio.pipe.write(bytes([0xAA, 0x20]) + struct.pack("<I", addr) +
                         bytes(data) + bytes([ck, 0x55]))
        ack = radio.pipe.read(2)
        if ack[:2] == b"\xAA\x20":
            time.sleep(1)
            return
        time.sleep(0.6)
    raise errors.RadioError("写扇区 %06X 失败" % addr)


def _poke(img, off, data):
    """CHIRP 的 MemoryMapBytes 不支持切片赋值，用 set() 按地址写"""
    try:
        img.set(off, bytes(data))
    except Exception:
        for i, b in enumerate(bytes(data)):
            img[off + i] = b


def _finalize(radio):
    radio.pipe.write(b"\xAA\x48\x55")
    radio.pipe.read(2)
    time.sleep(12)


@directory.register
class UV98nicFW(chirp_common.CloneModeRadio):
    """UV98 / RT880 nicFW 汉化版"""
    VENDOR = "UV98"
    MODEL = "UV98 nicFW"
    BAUD_RATE = 38400
    NEEDS_COMPAT_SERIAL = False

    @classmethod
    def get_prompts(cls):
        rp = chirp_common.RadioPrompts()
        rp.info = ("写频前请确认：本模块只写频率与中文信道名，"
                   "功率/带宽/哑音等参数请在机身上调。"
                   "在 CHIRP 里删除信道只会清空名字，不会缩短信道列表。")
        rp.pre_download = "读取信道区（约 20 秒）..."
        rp.pre_upload = "写入信道区（每扇区写后需等待约 13 秒）..."
        return rp

    def get_features(self):
        rf = chirp_common.RadioFeatures()
        rf.has_settings = False
        rf.has_bank = False
        rf.has_name = True
        rf.has_tuning_step = False
        rf.has_offset = True
        rf.can_odd_split = True
        rf.has_dtcs = True
        rf.has_rx_dtcs = False
        rf.has_ctone = True
        rf.has_cross = False
        rf.has_mode = True
        rf.has_nostep_tuning = True
        rf.has_comment = False
        rf.memory_bounds = (1, 200)
        rf.valid_modes = ["FM", "NFM"]
        rf.valid_duplexes = ["", "+", "-", "split"]
        rf.valid_tmodes = ["", "Tone", "TSQL", "DTCS"]
        rf.valid_tuning_steps = []
        rf.valid_name_length = 15
        rf.valid_characters = VALID_CHARS
        rf.valid_tones = list(chirp_common.TONES)
        rf.valid_dtcs_codes = list(chirp_common.DTCS_CODES)
        rf.valid_bands = [(136000000, 174000000), (400000000, 480000000)]
        return rf

    def process_mmap(self):
        pass

    def sync_in(self):
        st = chirp_common.Status()
        st.msg = "读取信道区..."
        st.max = IMG_SIZE // 128
        st.cur = 0
        self.status_fn(st)
        img = bytearray()
        for k in range(IMG_SIZE // 128):
            img += _read_block(self, IMG_ADDR + k * 128)
            st.cur = k + 1
            self.status_fn(st)
        self._mmap = memmap.MemoryMapBytes(bytes(img))

    def sync_out(self):
        st = chirp_common.Status()
        st.msg = "写入信道区..."
        st.max = 2
        st.cur = 0
        self.status_fn(st)
        img = self.get_mmap().get_packed()
        for i in range(2):
            _write_sector(self, IMG_ADDR + i * 0x1000, img[i * 0x1000:(i + 1) * 0x1000])
            _finalize(self)
            st.cur = i + 1
            self.status_fn(st)

    def _rec(self, number):
        slot = number + FIRST_SLOT - 1
        o = BASE_ADDR - IMG_ADDR + slot * SLOT
        return slot, o

    def get_raw_memory(self, number):
        slot, o = self._rec(number)
        return repr(bytes(self._mmap[o:o + SLOT]))

    def get_memory(self, number):
        mem = chirp_common.Memory()
        mem.number = number
        slot, o = self._rec(number)
        rec = bytes(self._mmap[o:o + SLOT])
        if len(rec) < SLOT or int.from_bytes(rec[24:26], "little") != 0xABC6:
            mem.empty = True
            return mem
        rx10 = int.from_bytes(rec[0:4], "little")
        tx10 = int.from_bytes(rec[4:8], "little")
        mem.freq = int(rx10 * 10)
        if tx10 and tx10 != rx10:
            mem.duplex = "split"
            mem.offset = int(tx10 * 10)
        else:
            mem.duplex = ""
        rv = int.from_bytes(rec[TONE_OFF:TONE_OFF + 2], "little")
        tv = int.from_bytes(rec[TONE_OFF + 2:TONE_OFF + 4], "little")
        if rv or tv:
            if (rv | tv) & DCS_FLAG:
                code = (rv & 0x7FFF) or (tv & 0x7FFF)
                mem.tmode = "DTCS"
                mem.dtcs = int(oct(code)[2:])
            elif rv and rv == tv:
                mem.tmode = "TSQL"
                mem.ctone = round(rv / 10.0, 1)
            else:
                mem.tmode = "Tone"
                mem.rtone = round((tv or rv) / 10.0, 1)
        else:
            mem.tmode = ""
        mem.name = _dec(rec[NAME_OFF:NAME_OFF + NAME_LEN])
        mem.mode = "FM"
        mem.tuning_step = 5.0
        return mem

    def set_memory(self, mem):
        slot, o = self._rec(mem.number)
        rec = bytearray(self._mmap[o:o + SLOT])
        if len(rec) < SLOT:
            return
        valid = int.from_bytes(rec[24:26], "little") == 0xABC6
        if mem.empty:
            if valid:
                # 固件遇首个 magic 非法即停止枚举，故删除只清名字，保留记录
                rec[NAME_OFF:NAME_OFF + NAME_LEN] = b"\x00" * NAME_LEN
                _poke(self._mmap, o, rec)
            return
        hz10 = int(round(mem.freq / 10))
        tx10 = hz10
        if mem.duplex == "+":
            tx10 = int(round((mem.freq + mem.offset) / 10))
        elif mem.duplex == "-":
            tx10 = int(round((mem.freq - mem.offset) / 10))
        elif mem.duplex == "split":
            tx10 = int(round(mem.offset / 10))
        rec[0:4] = struct.pack("<I", hz10)
        rec[4:8] = struct.pack("<I", tx10)
        rv = tv = 0
        if mem.tmode == "DTCS":
            rv = tv = DCS_FLAG | int(str(mem.dtcs), 8)
        elif mem.tmode == "TSQL":
            rv = tv = int(round(mem.ctone * 10))
        elif mem.tmode == "Tone":
            tv = int(round(mem.rtone * 10))
        rec[TONE_OFF:TONE_OFF + 2] = struct.pack("<H", rv & 0xFFFF)
        rec[TONE_OFF + 2:TONE_OFF + 4] = struct.pack("<H", tv & 0xFFFF)
        rec[21] = 0x06
        rec[24:26] = b"\xC6\xAB"
        nb = _enc(mem.name)[:NAME_LEN]
        rec[NAME_OFF:NAME_OFF + NAME_LEN] = nb + b"\x00" * (NAME_LEN - len(nb))
        _poke(self._mmap, o, rec)
