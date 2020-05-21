#include "fsys.hpp"
#include <cstdio>

FileHandle::FileHandle(DirectoryEntry dentry, FAT16* fat) : dentry(dentry), fat(fat), current_pos(0) {
    buf = new uint8_t[fat->bpb.sector_length * fat->bpb.cluster_length];
    current_cluster = dentry.head_cluster;
    int x = fat->bpb.sector_length * fat->bpb.cluster_length;
}

FileHandle::~FileHandle() {
    delete[] buf;
}

int FileHandle::read(char* b, size_t n) {
    size_t i = 0;
    size_t p = current_pos % (fat->bpb.cluster_length * fat->bpb.sector_length);
    size_t c = (fat->bpb.cluster_length * fat->bpb.sector_length) - p;
    fat->read_cluster((char*)buf, current_cluster);
    if (n < c) {
        std::memcpy(b, buf+p, n);
        return n;
    }
    else {
        std::memcpy(b, buf+p, c);
    }
    i += c;
    current_cluster = update_cluster_num(current_cluster);
    size_t m2 = (current_pos+n - current_pos+c) % (fat->bpb.cluster_length * fat->bpb.sector_length);
    size_t cluster_read_times = ((current_pos+n - current_pos+c)-m2) / (fat->bpb.cluster_length * fat->bpb.sector_length);
    for (int j = 0; j < cluster_read_times; j++) {
        if (current_cluster == 0xffff) {
            break;
        }
        fat->read_cluster(b, current_cluster);
        current_cluster = update_cluster_num(current_cluster);
    }
    std::memcpy(b, buf, m2);
    return n;
}

uint32_t FileHandle::write(uint8_t* buf, size_t n) {
    int32_t write_size = n;
    size_t cluster_size = fat->bpb.sector_length * fat->bpb.cluster_length;
    size_t rest_length = cluster_size - (current_pos / cluster_size);
    size_t j = n < rest_length ? n : rest_length;
    fat->fst.write((char*)buf, j);
    current_pos += j;
    n -= j;
    if (n <= 0) {
        return n;
    }
    current_cluster = update_cluster_num(current_cluster);

    uint32_t cluster_num = n / cluster_size;
    for (uint32_t i = 0; i < cluster_num; i++) {
        fat->fst.seekg(fat->data_start + cluster_size*current_cluster);
        fat->fst.write((char*)(buf+j+(cluster_size*i)), cluster_size);
        uint16_t next_cluster = update_cluster_num(current_cluster);
        if (next_cluster == 0xff) {
            uint16_t new_cluster = fat->find_empty_cluster();
            if (new_cluster == -1) {
                throw std::runtime_error("FULL");
            }
            fat->set_cluster_num(current_cluster, new_cluster);
            current_cluster = new_cluster;
        }
        else {
            current_cluster = next_cluster;
        }
    }
    n -= cluster_num * cluster_size;
    if (n == 0) {
        return write_size;
    }
    fat->fst.seekg(fat->data_start + cluster_size*current_cluster);
    n -= cluster_num * cluster_size;
    fat->fst.write((char*)(buf+j+(cluster_num*cluster_size)), n);
    return write_size;
}

uint16_t FileHandle::update_cluster_num(uint16_t index) {
    uint32_t fat_start = (fat->bpb.sector_length * fat->bpb.preserved_sectors);
    fat->fst.seekg(fat_start+(index*2));
    uint16_t num;
    fat->fst.read((char*)&num, 2);
    return num;
}

char* strrchr(char* str, char c) {
    size_t len = strlen(str);
    for (size_t i = len - 1; i >= 0; i--) {
        if (str[i] == c) {
            return str + i;
        }
    }
    return NULL;
}


int parse_path(char* path/*[in]*/, char* filename/*[out]*/, char* suffix/*[out]*/) {
    memcpy(filename, "        ", 8);
    memcpy(suffix, "   ", 3);
    size_t len = strlen(path);
    char* p = strchr(path, '.');
    size_t suffix_len = (len-1) - (p - path);

    if (p != NULL) {
        size_t suffix_len = (len-1) - (p - path);
        size_t diff = p - path;
        if (diff >= 8 || suffix_len > 3) {
            filename = NULL;
            suffix = NULL;
            return -1;
        }
        memcpy(suffix, p+1, suffix_len);
        memcpy(filename, path, diff);;
    }
    else {
        memcpy(filename, path, std::min((size_t)8, strlen(path)));
    }
    return 0;
}

void parse_directory_entry(uint8_t* ar, DirectoryEntry* de) { // independ function
    int pos = 0;
    for (int i = 0; i < 8; i++) {
        de->filename[i] = (char)ar[pos];
        pos++;
    }
    for (int i = 0; i < 3; i++) {
        de->suffix[i] = ar[pos];
        pos++;
    }
    de->attribute = ar[pos];
    pos += 2;
    de->ts = ar[pos];
    pos++;
    de->created_time = ar[pos+1] << 8 | ar[pos];
    pos += 2;
    de->created_date = ar[pos+1] << 8 | ar[pos];
    pos += 2;
    de->accessed_date = ar[pos+1] << 8 | ar[pos];
    pos += 4;
    de->updated_time = ar[pos+1] << 8 | ar[pos];
    pos += 2;
    de->updated_time = ar[pos+1] << 8 | ar[pos];
    pos += 2;
    de->head_cluster = ar[pos+1] << 8 | ar[pos];
    pos += 2;
    uint32_t u = 0;
    for (int i = 0; i < 4; i++) {
        u |= ar[pos] << (8*i);
        pos++;
    }
    de->filesize = u;
}

FAT16::FAT16(std::string filename) : filename(filename) {
    fst = std::fstream(filename, std::ios_base::binary | std::ios_base::in | std::ios_base::out);
    fst.seekg(0);
    fst.read((char*)&bpb, 61);
    fst.seekg(bpb.preserved_sectors * bpb.sector_length + (bpb.fat_sectors * bpb.sector_length * bpb.fat_count));
    fat_size = bpb.fat_sectors * bpb.sector_length;
    data_start = bpb.preserved_sectors * bpb.sector_length + (bpb.fat_sectors * bpb.sector_length * bpb.fat_count) + (32 * bpb.root_entry_count);
    root_directory_entry = new DirectoryEntry[bpb.root_entry_count];
    for (int i = 0; i < bpb.root_entry_count; i++) {
        fst.read((char*)directory_entry_buffer, 32);
        parse_directory_entry(directory_entry_buffer, root_directory_entry + i);
    }
}

int64_t FAT16::read_path(char* pathname, DirectoryEntry *de, uint32_t entry_count, DirectoryEntry *out, bool is_root) { //再帰でクラスターを読んでいく
    char filename[8];
    char suffix[3];
    if (pathname == NULL) {
        out = NULL;
        return -1;
    }
    if (is_root) { // is_rootがtrueなら deを無視して、root_directory_entryを使う(rootなので)
        for (int i = 0; i < entry_count; i++) {
            DirectoryEntry d2 = root_directory_entry[i];
            if (parse_path(pathname, filename, suffix) == -1) {
                throw std::runtime_error("invalid path");
            }
            if ((d2.attribute & 0x10) && memcmp(filename, d2.filename, 8) == 0 && memcmp(suffix, d2.suffix, 3) == 0) {
                std::cout << "directory" << std::endl;
                char* new_path = strtok(NULL, "/");
                if (new_path == NULL) {
                    throw std::runtime_error("Not such file or directory");
                }
                return read_path(new_path, &d2, d2.filesize/32, out, false);
            }
            else if ((d2.attribute & 0x20) && memcmp(filename, d2.filename, 8) == 0 && memcmp(suffix, d2.suffix, 3) == 0) {
                std::cout << "file in rootdir" << std::endl;
                *out = d2;
                return 0;
            }
        }
    }
    else {
        std::cout << "file in subdir" << std::endl;
        uint16_t cluster_index = de->head_cluster;
        uint32_t table_size = de->filesize;
        uint32_t cluster_size = this->bpb.sector_length * this->bpb.cluster_length;
        uint8_t d_buf[32];
        char temp_buf[cluster_size];
        uint32_t already_read = 0;
        uint32_t cluster_read_num = 0;
        uint32_t cluster_num = std::ceil((long double)table_size / (long double)cluster_size);
        while (cluster_read_num < cluster_num) {
            std::cout << cluster_index << std::endl;
            read_cluster(temp_buf, cluster_index);
            std::cout << std::endl;
            for (int i = 0; i < cluster_size; i+=32) {
                char filename[8];
                char suffix[3];
                memcpy(d_buf, temp_buf+i, 32);
                DirectoryEntry d4;
                parse_directory_entry((uint8_t *)d_buf, &d4);
                parse_path(pathname, filename, suffix);
                if (d4.attribute == 0x10 && memcmp(d4.filename, filename, 8) == 0) {
                    char *new_path = strtok(NULL, "/");
                    return read_path(new_path, &d4, d4.filesize/32, out, false);
                }
                else if (memcmp(d4.filename, filename, 8) == 0 && memcmp(d4.suffix, suffix, 3) == 0) {
                    *out = d4;
                    return 0;
                }
            }
            cluster_read_num++;
            cluster_index = this->get_next_cluster_num(cluster_index);
        }
        out = NULL;
        return -1;
    }
}

uint16_t FAT16::get_next_cluster_num(uint16_t current_cluster_num) {
    uint16_t ret;
    uint32_t preserved_bytes = this->bpb.sector_length * this->bpb.cluster_length * this->bpb.preserved_sectors;
    
    this->fst.seekg(preserved_bytes + current_cluster_num * 2);
    fst.read((char*)&ret, 2);
    return ret;
    
}

FileHandle FAT16::open(char* path) {
    DirectoryEntry de;
    std::cout << "path=" << path << std::endl;
    int res;
    char* p = strtok(path, "/"); // 最初にパスを区切る
    res = read_path(p, root_directory_entry, bpb.root_entry_count, &de, true);
    if (res == -1) {
        std::cout << "something error" << std::endl;
        throw std::runtime_error("Not such file or directory in open()");
    }
    return FileHandle(de, this);
}
        
        
void FAT16::read_cluster(char* buf, uint16_t cluster_num) { //与えられたクラスター番号のデータを読む
    uint64_t data_start = bpb.sector_length * bpb.preserved_sectors + bpb.fat_sectors * bpb.sector_length * bpb.fat_count + 32 * bpb.root_entry_count;
    fst.seekg(data_start + (bpb.sector_length * bpb.cluster_length)*(cluster_num-2));
    fst.read(buf, bpb.sector_length * bpb.cluster_length);
}

int32_t FAT16::find_empty_cluster() {
    for (uint16_t i = 4; i < fat_size; i += 2) {
        fst.seekg(fat_start+i);
        uint16_t n;
        fst.read((char*)&n, 2);
        if (n == 0x00) {
            return i / 2;
        }
    }
    return -1;
}

int FAT16::namecmp(DirectoryEntry *de, char *basename, char *suffix) {
    int m1 = std::memcmp(de->filename, basename, 8);
    if (m1 != 0) {
        return m1;
    }
    int m2 = std::memcmp(de->suffix, suffix, 3);
    return m2;
}

void FAT16::set_cluster_num(uint16_t index, uint16_t num) {
    fst.seekg(fat_start+(index*2));
    fst.write((char*)&num, 2);
}

int main() {
    std::shared_ptr<FAT16> p1(new FAT16("fs"));
    char *fname = strdup("/DIR1/TEXT2.TXT");
    FileHandle f = p1->open(fname);
    char t[7];
    f.read(t, 5);
    std::cout << t << std::endl;
    delete fname;
}
