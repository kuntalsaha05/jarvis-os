#!/usr/bin/env python3
"""
JARVIS-FS Host-Side Storage & Image Inspection Tool
Formats, inspects, injects, and extracts files from JARVIS-FS disk images.

Disk Layout (64 KiB = 128 blocks * 512 bytes):
  Block 0: Superblock (magic: 0x4A415256 "JARV", total: 128, free: 120, inodes: 16)
  Block 1-2: Inode Table (16 inodes, each 48 bytes)
  Block 3: Block Allocation Bitmap (512 bytes, bits 0..119 represent data blocks 4..123)
  Blocks 4-123: Data Blocks (120 blocks * 512 bytes = 60 KiB data payload)
"""

import sys
import struct
import os

FS_MAGIC = 0x4A415256
FS_BLOCK_SIZE = 512
FS_TOTAL_BLOCKS = 128
FS_FIRST_DATA_BLOCK = 4
FS_TOTAL_DATA_BLOCKS = 120
FS_MAX_FILES = 16
FS_MAX_FILENAME = 28
FS_DIRECT_BLOCKS = 8
FS_MAX_FILE_SIZE = FS_DIRECT_BLOCKS * FS_BLOCK_SIZE  # 4096 bytes

INODE_FORMAT = "<28sII8IB3x"  # name (28), size (4), blocks (8*4 = 32), used (1), 3 pad = 68 bytes
SUPERBLOCK_FORMAT = "<IIII"    # magic, total_blocks, free_blocks, inode_count = 16 bytes


class JarvisFSImage:
    def __init__(self, data: bytearray):
        if len(data) < FS_TOTAL_BLOCKS * FS_BLOCK_SIZE:
            data.extend(b"\x00" * (FS_TOTAL_BLOCKS * FS_BLOCK_SIZE - len(data)))
        self.data = data

    @classmethod
    def format_new(cls) -> "JarvisFSImage":
        data = bytearray(FS_TOTAL_BLOCKS * FS_BLOCK_SIZE)
        # Superblock at block 0
        struct.pack_into(SUPERBLOCK_FORMAT, data, 0, FS_MAGIC, FS_TOTAL_BLOCKS, FS_TOTAL_DATA_BLOCKS, FS_MAX_FILES)
        return cls(data)

    @classmethod
    def load(cls, path: str) -> "JarvisFSImage":
        with open(path, "rb") as f:
            return cls(bytearray(f.read()))

    def save(self, path: str):
        with open(path, "wb") as f:
            f.write(self.data)

    def get_superblock(self):
        magic, total_blocks, free_blocks, inode_count = struct.unpack_from(SUPERBLOCK_FORMAT, self.data, 0)
        return {
            "magic": magic,
            "magic_hex": hex(magic),
            "total_blocks": total_blocks,
            "free_blocks": free_blocks,
            "inode_count": inode_count,
            "valid": (magic == FS_MAGIC)
        }

    def _read_inode(self, idx: int):
        offset = FS_BLOCK_SIZE + idx * 68
        raw = self.data[offset:offset + 68]
        fields = struct.unpack_from("<28sI8IB3x", raw, 0)
        name = fields[0].split(b"\x00", 1)[0].decode("ascii", errors="replace")
        size = fields[1]
        blocks = list(fields[2:10])
        used = fields[10]
        return {
            "index": idx,
            "name": name,
            "size": size,
            "blocks": blocks,
            "used": bool(used)
        }

    def _write_inode(self, idx: int, name: str, size: int, blocks: list, used: bool):
        offset = FS_BLOCK_SIZE + idx * 68
        name_bytes = name.encode("ascii")[:FS_MAX_FILENAME - 1]
        name_padded = name_bytes.ljust(FS_MAX_FILENAME, b"\x00")
        blk_arr = list(blocks) + [0] * (FS_DIRECT_BLOCKS - len(blocks))
        raw = struct.pack("<28sI8IB3x", name_padded, size, *blk_arr[:FS_DIRECT_BLOCKS], 1 if used else 0)
        self.data[offset:offset + 68] = raw

    def list_files(self):
        files = []
        for i in range(FS_MAX_FILES):
            inode = self._read_inode(i)
            if inode["used"]:
                files.append(inode)
        return files

    def add_file(self, filename: str, content: bytes) -> bool:
        if len(content) > FS_MAX_FILE_SIZE:
            raise ValueError(f"File exceeds maximum direct capacity of {FS_MAX_FILE_SIZE} bytes")

        # Check existing
        for i in range(FS_MAX_FILES):
            inode = self._read_inode(i)
            if inode["used"] and inode["name"] == filename:
                raise ValueError(f"File '{filename}' already exists")

        # Find free inode
        free_slot = -1
        for i in range(FS_MAX_FILES):
            inode = self._read_inode(i)
            if not inode["used"]:
                free_slot = i
                break
        if free_slot == -1:
            raise RuntimeError("Inode table full")

        needed_blocks = (len(content) + FS_BLOCK_SIZE - 1) // FS_BLOCK_SIZE
        # Find free data blocks in bitmap (block 3)
        bitmap_offset = 3 * FS_BLOCK_SIZE
        allocated_blocks = []
        for blk in range(FS_TOTAL_DATA_BLOCKS):
            byte_idx = bitmap_offset + (blk // 8)
            bit_mask = 1 << (blk % 8)
            if (self.data[byte_idx] & bit_mask) == 0:
                self.data[byte_idx] |= bit_mask
                allocated_blocks.append(FS_FIRST_DATA_BLOCK + blk)
                if len(allocated_blocks) == needed_blocks:
                    break

        if len(allocated_blocks) < needed_blocks:
            # Rollback
            for b in allocated_blocks:
                blk = b - FS_FIRST_DATA_BLOCK
                self.data[bitmap_offset + (blk // 8)] &= ~(1 << (blk % 8))
            raise RuntimeError("Disk full")

        # Write data to blocks
        for idx, blk_num in enumerate(allocated_blocks):
            blk_offset = blk_num * FS_BLOCK_SIZE
            chunk = content[idx * FS_BLOCK_SIZE:(idx + 1) * FS_BLOCK_SIZE]
            self.data[blk_offset:blk_offset + len(chunk)] = chunk

        # Update inode
        self._write_inode(free_slot, filename, len(content), allocated_blocks, True)

        # Update superblock free blocks
        sb = self.get_superblock()
        struct.pack_into("<I", self.data, 8, sb["free_blocks"] - needed_blocks)
        return True

    def read_file(self, filename: str) -> bytes:
        for i in range(FS_MAX_FILES):
            inode = self._read_inode(i)
            if inode["used"] and inode["name"] == filename:
                out = bytearray()
                bytes_left = inode["size"]
                for b in inode["blocks"]:
                    if b == 0 or bytes_left == 0:
                        break
                    blk_offset = b * FS_BLOCK_SIZE
                    take = min(bytes_left, FS_BLOCK_SIZE)
                    out.extend(self.data[blk_offset:blk_offset + take])
                    bytes_left -= take
                return bytes(out)
        raise FileNotFoundError(f"File '{filename}' not found in JARVIS-FS")


def run_self_test():
    print("Running JARVIS-FS image tool verification...")
    img = JarvisFSImage.format_new()
    sb = img.get_superblock()
    assert sb["valid"], "Superblock magic mismatch"
    assert sb["free_blocks"] == 120, "Free blocks mismatch"

    # Add file 1
    sample_text = b"JARVIS-OS Host-Side File Injection Test\nversion=1.0\n"
    img.add_file("kernel.cfg", sample_text)

    # Add file 2
    log_text = b"[AI] Test log entry for automated persistence validation.\n"
    img.add_file("test.log", log_text)

    # List files
    files = img.list_files()
    assert len(files) == 2, f"Expected 2 files, got {len(files)}"
    assert files[0]["name"] == "kernel.cfg"
    assert files[1]["name"] == "test.log"

    # Read back and compare
    r1 = img.read_file("kernel.cfg")
    assert r1 == sample_text, "Content mismatch on kernel.cfg readback"
    r2 = img.read_file("test.log")
    assert r2 == log_text, "Content mismatch on test.log readback"

    print("JARVIS-FS Host Image Tool Verification: PASS")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--test":
        run_self_test()
    elif len(sys.argv) > 2 and sys.argv[1] == "inspect":
        img = JarvisFSImage.load(sys.argv[2])
        sb = img.get_superblock()
        print(f"JARVIS-FS Image: {sys.argv[2]}")
        print(f"Magic: {sb['magic_hex']} (Valid: {sb['valid']})")
        print(f"Total Blocks: {sb['total_blocks']} ({sb['total_blocks'] * FS_BLOCK_SIZE // 1024} KiB)")
        print(f"Free Blocks:  {sb['free_blocks']}")
        print("\nDirectory Listing:")
        for f in img.list_files():
            print(f"  {f['name']:<24} {f['size']:>6} bytes  blocks: {f['blocks'][:(f['size']+511)//512]}")
    else:
        run_self_test()
