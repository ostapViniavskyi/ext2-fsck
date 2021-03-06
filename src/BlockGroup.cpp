#include "BlockGroup.h"

BlockGroup::BlockGroup(FilesystemImage &image, fs_t i):
image(image), block_group_i(i)
{
    ext2_group_desc group_desc;

    fs_t group_desc_offset = fs_align(BOOT_SIZE + sizeof(ext2_super_block), image.block_size) + sizeof(ext2_group_desc) * i;

    if (group_desc_offset + sizeof(group_desc) > image.filesystem_size) {
        errors.emplace_back("Group description out of filesystem");
        return;
    }
    image.istream.seek(group_desc_offset, biofs::beg);
    image.istream >> group_desc;
    // assigning all used by this group blocks (1 super block and 1 group description)

    inode_table_i = group_desc.bg_inode_table;
    inode_table_size = (image.inodes_per_group * image.inode_size) / image.block_size;
    if (inode_table_i + inode_table_size >= image.blocks_count) {
        errors.push_back("Inode-table is in invalid block " + std::to_string(inode_table_i));
        return;
    }
    // assigning all blocks used by the inode table;
    for (uint32_t used_block = 0; used_block < inode_table_size; ++used_block) {
        image.block_usage[inode_table_i + used_block] = true;
    }

    block_bitmap_i = group_desc.bg_block_bitmap;
    inode_bitmap_i = group_desc.bg_inode_bitmap;
    if (block_bitmap_i >= image.blocks_count)
        errors.push_back("Block bitmap references unexisting block " + std::to_string(block_bitmap_i));
    if (inode_bitmap_i >= image.blocks_count)
        errors.push_back("INode bitmap references unexisting block " + std::to_string(inode_bitmap_i));
    if (!errors.empty()) return;

    image.block_usage[block_bitmap_i] = true;
    image.block_usage[inode_bitmap_i] = true;
}

ext2_inode BlockGroup::getInode(fs_t i) {
    if (!errors.empty()) throw InvalidINode("Tried to access INode " + std::to_string(i) + " in a broken group");

    ext2_inode inode;
    image.istream.seek(inode_table_i * image.block_size + image.inode_size * ((i - 1) % image.inodes_per_group), biofs::beg);
    image.istream >> inode;
    return inode;
}

void BlockGroup::additionalFieldsCheck() {
    if (!errors.empty()) return;
    ext2_group_desc group_desc;

    uint32_t group_desc_offset = fs_align(BOOT_SIZE + sizeof(ext2_super_block), image.block_size) + sizeof(ext2_group_desc) * block_group_i;
    image.istream.seek(group_desc_offset, biofs::beg);
    image.istream.read((char*) &group_desc, sizeof(group_desc));

    fs_t block_bitmap_size = fs_ceil_division(image.blocks_per_group, 8);
    auto block_bitmap = new uint8_t[block_bitmap_size];
    image.istream.seek(block_bitmap_i * image.block_size, biofs::beg);
    image.istream.read((char*) block_bitmap, block_bitmap_size);
    fs_t free_block_count = 0;

    // skip boot
    fs_t block_offset = image.block_size <= BOOT_SIZE ? 1 : 0;
    for (fs_t i = 0; i < image.blocks_per_group; ++i) {
        fs_t block_i = image.blocks_per_group * block_group_i + i + block_offset;
        if (block_i >= image.blocks_count) break;

        bool real = image.block_usage[block_i];
        bool stored = (block_bitmap[i / 8] >> (i % 8)) & 1;
        if (real != stored)
            errors.push_back("Block " + std::to_string(block_i) + " is marked incorrectly in the bitmap (Should be " + std::to_string(real) + ")");
        if (!stored) ++free_block_count;
    }

//    std::cout << group_desc.bg_free_blocks_count << std::endl;
    if (group_desc.bg_free_blocks_count != free_block_count)
        errors.push_back("Incorrect field of free blocks count (is " + std::to_string(group_desc.bg_free_blocks_count) +
        ", should be " + std::to_string(image.blocks_per_group - free_block_count) + ")");

    fs_t inode_bitmap_size = fs_ceil_division(image.inodes_per_group, 8);
    auto inode_bitmap = new uint8_t[inode_bitmap_size];
    image.istream.seek(inode_bitmap_i * image.block_size, biofs::beg);
    image.istream.read((char*) inode_bitmap, inode_bitmap_size);
    fs_t inode_count = 0;

    for (fs_t i = 0; i < image.inodes_per_group; ++i) {
        fs_t inode_i = image.inodes_per_group * block_group_i + i;
        bool real = image.inode_usage[inode_i];
        bool stored = (inode_bitmap[i / 8] >> (i % 8)) & 1;
        if (real != stored)
            errors.push_back("INode " + std::to_string(inode_i + 1) + " is marked incorrectly in the bitmap (Should be " + std::to_string(real) + ")");
        if (stored) ++inode_count;
    }

    if (group_desc.bg_free_inodes_count != image.inodes_per_group - inode_count)
        errors.push_back("Incorrect field of free inodes count");

    delete[] block_bitmap;
    delete[] inode_bitmap;
}
