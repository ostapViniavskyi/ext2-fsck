#ifndef EXT2_FSCK_FIESYSTEM_H
#define EXT2_FSCK_FIESYSTEM_H

#include <ext2fs/ext2fs.h>
#include <fstream>
#include <vector>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include "BlockGroup.h"
#include "FilesystemImage.h"
#include "FilesystemErrors.h"
#include "INode.h"
#include <sstream>

const uint32_t ROOT_INODE_I = 2;

class Filesystem {
public:
    FilesystemImage image{};
    std::vector<BlockGroup> block_groups;
    std::vector<ext2_group_desc> block_group_descriptions;

    std::unordered_map<fs_t, INode> inodes;
    ext2_inode getInode(fs_t i);
    void createFilesystemTree(INode& directory);
    int readSuperBlock();
    void initBlockUsageTable();
    int readBlockGroupTable();

public:
    std::vector<std::string> errors;

    explicit Filesystem(std::string path);

    friend std::ostream& operator<<(std::ostream& out, Filesystem& fs);
    std::string getAllErrors();

    void __fileTreeStringOneDepth(INode& directory, int depth, std::ostringstream& out, std::unordered_set<fs_t>& usedMap);
    std::string fileTreeString(INode& directory);
    std::string fileTreeString();

};


#endif //EXT2_FSCK_FIESYSTEM_H
