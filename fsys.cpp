#include "fsys.hpp"

FileHandle::FileHandle(DirectoryEntry dentry, FAT16* fat) : dentry(dentry), fat(fat), current_pos(0) {
    buf = new uint8_t[fat->bpb.sector_length * fat->bpb.cluster_length];
    current_cluster = dentry.head_cluster;
    int x = fat->bpb.sector_length * fat->bpb.cluster_length;
    std::cout << "bufsize: " << x << std::endl;
}

FileHandle::~FileHandle() {
    delete[] buf;
}

int FileHandle::read(char* b, size_t n) {
    size_t i = 0;
    size_t p = current_pos % (fat->bpb.cluster_length * fat->bpb.sector_length);
    size_t c = (fat->bpb.cluster_length * fat->bpb.sector_length) - p;
    std::cout << "p=" << p << ", c=" << c << std::endl;
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

uint16_t FileHandle::update_cluster_num(uint16_t index) {
    std::cout << index << std::endl;
    uint32_t fat_start = (fat->bpb.sector_length * fat->bpb.preserved_sectors);
    fat->fst.seekg(fat_start+(index*2));
    std::cout << fat_start+(index*2) << std::endl;
    uint16_t num;
    fat->fst.read((char*)&num, 2);
    std::cout << "num=" << num << std::endl;
    return num;
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
    pos++;
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
    fst.read((char*)&bpb, 63);
    fst.seekg(bpb.preserved_sectors * bpb.sector_length);
    fat_size = bpb.fat_sectors * bpb.sector_length;
    data_start = bpb.preserved_sectors * bpb.sector_length + (bpb.fat_sectors * bpb.sector_length * bpb.fat_count) + (32 * bpb.root_entry_count);
    root_directory_entry = new DirectoryEntry[bpb.root_entry_count];
    for (int i = 0; i < bpb.root_entry_count; i++) {
        fst.read((char*)directory_entry_buffer, 32);
        parse_directory_entry(directory_entry_buffer, &root_directory_entry[i]);
    }
}
        
FileHandle FAT16::open(char* filename) {
    ptrdiff_t diff;
    DirectoryEntry *de;
    std::size_t s = std::strlen(filename);
    char *c = std::strrchr(filename, '.');
    if (c != NULL && (diff=(c-filename)) >= 11 || s > 11 || diff > 8) {
        throw std::runtime_error("invalid basename");
    } else if (s - (diff+1) > 3) {
        std::cout << "s=" << s << " diff=" << diff << std::endl;
        throw std::runtime_error("invalid suffix");
    }
    if (c == NULL) {
        diff = s;
    }
    char name[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char suffix[3] = {' ', ' ', ' '};
    memcpy(name, filename, diff);
    if (c != NULL) {
        if (s - diff <= 3) {
            std::memcpy(suffix, filename+diff, s-diff);
        }
    }
    for (int i = 0; i < bpb.root_entry_count; i++) {
        int m = namecmp(root_directory_entry+i, filename, suffix);
        if (m == 0) {
            DirectoryEntry* de = root_directory_entry + i;
            int cluster_num = de->head_cluster-2;
            fst.seekg(data_start + ((bpb.sector_length*bpb.cluster_length)*cluster_num));
            break;
        }
    }
    return FileHandle(*de, this);
}
        
        
void FAT16::read_cluster(char* buf, uint16_t cluster_num) {
    uint64_t data_start = bpb.sector_length * bpb.preserved_sectors + bpb.fat_sectors * bpb.sector_length * bpb.fat_count + 32 * bpb.root_entry_count;
    fst.seekg(data_start + (bpb.sector_length * bpb.cluster_length)*(cluster_num-2));
    fst.read(buf, bpb.sector_length * bpb.cluster_length);
}

int FAT16::namecmp(DirectoryEntry *de, char *basename, char *suffix) {
    int m1 = std::memcmp(de->filename, basename, 8);
    if (m1 != 0) {
        return m1;
    }
    int m2 = std::memcmp(de->suffix, suffix, 3);
    return m2;
}

int main() {
    std::shared_ptr<FAT16> p1(new FAT16("fs"));
    FileHandle f = p1->open("SPECIAL.TXT");
    char t[7];
    f.read(t, 5);
    std::cout << t << std::endl;
}
