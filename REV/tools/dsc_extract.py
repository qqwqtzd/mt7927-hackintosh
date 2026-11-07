#!/usr/bin/env python3
# dsc_extract.py: 从 Apple dyld shared cache 提取单个镜像为可分析 Mach-O
# 用法: dsc_extract.py <cache> <镜像路径子串> <out.macho>
# 原理：cache 的 image 在文件内是"原样"存放（segment fileoff 即为 cache 内绝对偏移），
# 我们把 header/load commands/各 segment 内容复制进新文件并重写 segment fileoff。
import struct, sys

LC_SEGMENT_64 = 0x19

def cstr(data, off):
    end = data.find(b'\0', off)
    return data[off:end].decode(errors='ignore')

def main(cache_path, needle, out_path):
    cache = open(cache_path, 'rb').read()
    magic = cache[:4]
    print('magic:', magic)
    hdr = struct.unpack_from('<16sIIIQ', cache, 0)  # magic + mappingOffset,count + imagesOffset,count
    mapping_offset, mapping_count = hdr[2], hdr[3]
    images_offset, images_count = hdr[4], hdr[5]
    print(f'mappings @0x{mapping_offset:x} x{mapping_count}; images @0x{images_offset:x} x{images_count}')

    # images 表
    target = None
    for i in range(images_count):
        e = images_offset + i * 32
        addr, _, _, pfo, _pad = struct.unpack_from('<QQQII', cache, e)
        name = cstr(cache, pfo)
        if needle in name:
            target = (addr, name, pfo)
            break
    if not target:
        print('image not found'); return 1
    addr, name, pfo = target
    print('image:', name, 'unslid base', hex(addr))

    # 找 image 所在 mapping
    mfile = None
    for i in range(mapping_count):
        e = mapping_offset + i * 40
        ma, msz, mfo, _p, _q = struct.unpack_from('<QQQII', cache, e)
        if ma <= addr < ma + msz:
            mfile = mfo + (addr - ma)
            break
    if mfile is None:
        print('no mapping for image'); return 1
    print('mach-o header at cache offset', hex(mfile))
    magic2 = cache[mfile:mfile+4]
    print('magic2:', magic2.hex())

    # 从 header 读 load commands
    ncmds, sizeofcmds = struct.unpack_from('<II', cache, mfile + 16)
    hdrsize = 32 + sizeofcmds
    d = bytearray(cache[mfile : mfile + hdrsize])   # header + load commands
    off = 32
    segs = []
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from('<II', d, off)
        if cmd == LC_SEGMENT_64:
            segname = d[off+8:off+24].split(b'\0')[0].decode()
            vmaddr = struct.unpack_from('<Q', d, off+0x18)[0]
            vmsize = struct.unpack_from('<Q', d, off+0x20)[0]
            fo = struct.unpack_from('<Q', d, off+0x28)[0]
            fs = struct.unpack_from('<Q', d, off+0x30)[0]
            segs.append((segname, vmaddr, vmsize, fo, fs, off + 0x28))
        off += cmdsize

    print('segments:')
    for s in segs: print('  ', s[0], hex(s[1]), 'size', s[2], 'fileoff(cache)', hex(s[3]), 'filesize', s[4])

    # 新文件布局：header+cmds 之后按序放各 segment 内容（fileoff 改为新文件相对）
    cursor = (len(d) + 0xfff) & ~0xfff
    for segname, vmaddr, vmsize, fo, fs, fld_off in segs:
        if fs == 0:
            continue
        data = cache[fo : fo + fs]
        print('  copying', segname, hex(fs), 'bytes ->', hex(cursor))
        d += b'\x00' * (cursor - len(d))
        d += data
        struct.pack_into('<Q', d, fld_off, cursor)
        cursor += fs

    open(out_path, 'wb').write(bytes(d))
    print('written', len(d), '->', out_path)
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2], sys.argv[3]))
