#ifndef EXT2_FSCK_INODE_H
#define EXT2_FSCK_INODE_H

#include <tuple>
#include <cmath>
#include <ext2fs/ext2fs.h>
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

const std::vector<std::string> file_type_names = {"unknown", "fifo", "character device", "directory", "block device", "regular", "symlink", "socket"};
file_type get_file_type(const ext2_inode& inode);

class INode {
public:
    fs_t inode_i;
    ext2_inode inode;
    std::vector<fs_t> blocks;
    file_type type = UNKNOWN;

    std::vector<std::string> errors;

    using dir_entry_t = std::tuple<std::string, uint32_t>;
    std::vector<dir_entry_t> children;

    void getIndirectBlocks(FilesystemImage& image, std::vector<fs_t>& blocks, fs_t block, fs_t& blocks_found, fs_t& block_count, int depth=1);
    std::vector<fs_t> getBlocks(FilesystemImage& image);

public:
    INode() = default;
    INode(FilesystemImage& image, ext2_inode inode, fs_t inode_i);

    friend std::ostream& operator<<(std::ostream& out, INode& file);
    std::string shortInfo();

    void readDirectory(FilesystemImage& image);
    void readSymLink(FilesystemImage &image);
};

#endif //EXT2_FSCK_INODE_H
