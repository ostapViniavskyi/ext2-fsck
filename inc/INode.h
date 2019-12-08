#ifndef EXT2_FSCK_INODE_H
#define EXT2_FSCK_INODE_H

#include <tuple>
#include <cmath>
#include <ext2fs/ext2_fs.h>
#include <vector>
#include <ctime>

#include "FilesystemImage.h"

enum file_type {
    UNKNOWN,
    FIFO,
    CHAR_DEVICE,
    DIRECTORY,
    BLOCK_DEVICE,
    REGULAR,
    SYMLINK,
    SOCKET
};

std::vector<std::string> file_type_names = {"unknown", "fifo", "character device", "directory", "block device", "regular", "symlink", "socket"};

inline file_type get_file_type(const ext2_inode& inode) {
    int mode = inode.i_mode >> 12;
    switch(mode) {
        case 1: return FIFO;
        case 2: return CHAR_DEVICE;
        case 4: return DIRECTORY;
        case 6: return BLOCK_DEVICE;
        case 8: return REGULAR;
        case 10: return SYMLINK;
        case 12: return SOCKET;
        default: return UNKNOWN;
    }
}

class INode {
public:
    unsigned int inode_i;
    ext2_inode inode;
    std::vector<uint32_t> blocks;
    file_type type = UNKNOWN;

    std::vector<std::string> errors;

    using dir_entry_t = std::tuple<std::string, uint32_t>;
    std::vector<dir_entry_t> children;

    void getIndirectBlocks(FilesystemImage& image, uint32_t block, uint32_t& blocks_found, uint32_t& block_count, int depth=1) {
        for (int i = 0; i < image.block_size / sizeof(uint32_t); ++i) {
            if (blocks_found >= block_count) return;

            // TODO check the blocks
            uint32_t value;
            image.istream.seekg(block * image.block_size + i * sizeof(uint32_t), std::ios::beg);
            image.istream.get((char *) &value, sizeof(value));

            if (depth == 1) {
                blocks.push_back(value);
                ++block_count;
            } else {
                getIndirectBlocks(image, value, blocks_found, block_count, depth-1);
            }
        }
    }

    std::vector<uint32_t> getBlocks(FilesystemImage& image) {
        uint32_t block_count = std::ceil((double) inode.i_size / image.block_size);
        uint32_t blocks_found = 0;
        std::vector<uint32_t> blocks(block_count);

        for (blocks_found = 0; blocks_found < block_count && blocks_found < EXT2_NDIR_BLOCKS; ++blocks_found) {
            blocks[blocks_found] = inode.i_block[blocks_found];
        }

        // TODO handle indirect blocks
        // TODO check the blocks
        getIndirectBlocks(image, inode.i_block[EXT2_IND_BLOCK], blocks_found, block_count, 1);
        getIndirectBlocks(image, inode.i_block[EXT2_DIND_BLOCK], blocks_found, block_count, 2);
        getIndirectBlocks(image, inode.i_block[EXT2_TIND_BLOCK], blocks_found, block_count, 3);

        return blocks;
    }

    void readDirectory(FilesystemImage& image) {
        children = {};

        uint32_t block_i = 0;
        uint32_t inside_offset = 0;
        char filename[EXT2_NAME_LEN + 1];

        ext2_dir_entry_2 entry;
        while(true) {
            if (inside_offset >= image.block_size) {
                ++block_i;
                if (block_i >= blocks.size()) return;
            }

            image.istream.seekg(blocks[block_i] * image.block_size + inside_offset, std::ios::beg);
            image.istream.read((char *) &entry.inode, sizeof(entry.inode));
            image.istream.read((char *) &entry.rec_len, sizeof(entry.rec_len));
            image.istream.read((char *) &entry.name_len, sizeof(entry.name_len));
            image.istream.read((char *) &entry.file_type, sizeof(entry.file_type));
            image.istream.read((char *) &entry.name, entry.name_len);

            std::copy(entry.name, entry.name + entry.name_len, filename);
            filename[entry.name_len] = '\0';

            if (entry.name_len)
                children.emplace_back(filename, entry.inode);
            inside_offset += entry.rec_len;
        }
    }

    void readSymLink(FilesystemImage &image) {
        // TODO implement
    }

public:
    friend std::ostream& operator<<(std::ostream& out, INode& file) {
        out << "[INode" << std::endl;
        out << "\tinode number: " << file.inode_i << std::endl;
        out << "\ttype: " << file_type_names[file.type] << std::endl;
        out << "\tblocks number: " << file.blocks.size() << std::endl;
        out << "\tfile size: " << file.inode.i_size << std::endl;

        std::time_t creation_time = file.inode.i_ctime;
        std::time_t modification_time = file.inode.i_mtime;

        out << "\tcreation date: " << std::asctime(std::localtime(&creation_time));
        out << "\tmodification date: " << std::asctime(std::localtime(&modification_time));

        if (file.type == DIRECTORY) {
            out << "\tchildren: [" << std::endl;
            for (auto &child: file.children) {
                out << "\t\t" << std::get<1>(child) << " " << std::get<0>(child) << std::endl;
            }
            out << "\t]" << std::endl;
        }
        out << "]";
        return out;
    };

    INode() = default;
    INode(FilesystemImage& image, ext2_inode inode, unsigned int inode_i):
        inode(inode), inode_i(inode_i)
    {
        blocks = getBlocks(image);

        for (unsigned int block : blocks) {
            if (block >= image.blocks_count)
                errors.push_back("File referencing invalid block " + std::to_string(block));
        }

        type = get_file_type(inode);
    }
};

#endif //EXT2_FSCK_INODE_H