#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <bit>
#include <cstdbool>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <cmath>
#include <algorithm>

struct DirectoryEntry {
    char filename[8];
    char suffix[3];
    uint8_t attribute;
    uint8_t ts;
    uint16_t created_time;
    uint16_t created_date;
    uint16_t accessed_date;
    uint16_t updated_time;
    uint16_t updated_date;
    uint16_t head_cluster;
    uint32_t filesize;
};

class FAT16;

struct FileHandle {
    FAT16* fat;
    DirectoryEntry dentry;
    uint64_t current_pos;
    uint16_t current_cluster;
    uint8_t *buf;
    int read(char* b, size_t n);
    uint16_t update_cluster_num(uint16_t index); 
    FileHandle(DirectoryEntry dentry, FAT16* fat);
    ~FileHandle();
};

#pragma pack(1)
struct BPB {
    uint8_t boot_code[3];
    char oem_name[8];
    uint16_t sector_length;
    uint8_t cluster_length;
    uint16_t preserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t fat_sectors;
    uint16_t track_sectors;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t reserved;
    uint8_t boot_flag;
    uint32_t serial_num;
    char volume_label[11];
    char fat_type[8];
};

#pragma pack()


void parse_directory_entry(uint8_t* ar, DirectoryEntry* de);
int parse_path(char* path, char* filename, char* suffix);

class FAT16 {
    public:
        std::string filename;
        BPB bpb;
        FAT16(std::string filename); 
        uint32_t fat_size;
        DirectoryEntry* root_directory_entry;
        uint32_t data_area_size;
        uint32_t data_start;
        uint32_t fat_start;
        FileHandle open(char *filename);
        void read_cluster(char* buf, uint16_t cluster_num);
        std::fstream fst;
        DirectoryEntry read_subdir(char *dirname, DirectoryEntry* dentry);
        uint16_t get_next_cluster_num(uint16_t cluster_num);
        int64_t read_path(char* pathname, DirectoryEntry *de, uint32_t entry_count, DirectoryEntry *out, bool is_root);
    private:
        uint8_t directory_entry_buffer[32];
        int namecmp(DirectoryEntry *de, char *basename, char *suffix);
};
