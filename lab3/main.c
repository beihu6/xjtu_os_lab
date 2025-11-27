
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VOLUME_NAME         "EXT2FS"    //卷名
#define BLOCK_SIZE          512         //块大小
#define NUM_BLOCK           4611        //磁盘总块数

#define GDT_START           0
#define GD_SIZE             32

#define BLOCK_BITMAP        512
#define INODE_BITMAP        1024
#define INODE_TABLE         1536
#define INODE_SIZE          64
#define INODE_TABLE_COUNTS  4096
#define DATA_BLOCK          263680
#define DATA_BLOCK_COUNTS   4096

#define USER_MAX            3
#define FOPEN_TABLE_MAX     16

#define MAX_FILE_SIZE     (2*1024*1024-18*512)

#define FILE_UNKNOWN 0
#define FILE_FILE 1
#define FILE_DIR 2

/* 目录项 (柔性数组) */
#define MAX_NAME_LEN 255
#define DIR_ENTRY_HEADER_SIZE 6  // inode(2) + rec_len(2) + name_len(1) + file_type(1)
struct dir_entry {
    unsigned short inode;
    unsigned short rec_len;
    unsigned char name_len;
    unsigned char file_type;
    char name[]; // 柔性数组
};

static inline unsigned short calculate_rec_len(unsigned char name_len)
{
    unsigned short base_len = DIR_ENTRY_HEADER_SIZE + name_len;
    return (base_len + 3) & ~3;  // 4字节对齐
}
static inline unsigned short calculate_actual_len(unsigned char name_len)
{
    return DIR_ENTRY_HEADER_SIZE + name_len;
}

/* 组描述符 / inode */
struct group_desc {
    char bg_volume_name[16];
    unsigned short bg_block_bitmap;
    unsigned short bg_inode_bitmap;
    unsigned short bg_inode_table;
    unsigned short bg_free_blocks_count;
    unsigned short bg_free_inodes_count;
    unsigned short bg_used_dirs_count;
    char bg_pad[4];
};

struct inode {
    unsigned short i_mode;
    unsigned short i_blocks;
    unsigned short i_uid;
    unsigned short i_gid;
    unsigned short i_links_count;
    unsigned short i_flags;
    unsigned long i_size;
    unsigned long i_atime;
    unsigned long i_ctime;
    unsigned long i_mtime;
    unsigned long i_dtime;
    unsigned short i_block[8]; // 0-5 direct, 6: single indirect, 7: double
    char i_pad[16];
};

struct user {
    char username[10];
    char password[10];
    char disk_name[10];
    unsigned short u_uid;
    unsigned short u_gid;
} User[USER_MAX];

static FILE *fp;
static unsigned short last_alloc_inode;
static unsigned short last_alloc_block;
static unsigned short current_dir;
static unsigned short current_dirlen;
static short fopen_table[FOPEN_TABLE_MAX];

static struct group_desc group_desc_buf;
static struct inode inode_buf;
static unsigned char bitbuf[BLOCK_SIZE];
static unsigned char ibuf[BLOCK_SIZE];
static char Buffer[BLOCK_SIZE];
static char tempbuf[MAX_FILE_SIZE];
static char dir_buf[BLOCK_SIZE];

char current_path[256];
char current_user[10];
char current_disk[64];

int user_num;
int user_index;


/* ========== 磁盘 I/O ========== */

static void update_group_desc() {
    if (!fp) return;
    fseek(fp, GDT_START, SEEK_SET);
    fwrite(&group_desc_buf, GD_SIZE, 1, fp);
    fflush(fp);
}
static void load_group_desc() {
    if (!fp) return;
    fseek(fp, GDT_START, SEEK_SET);
    fread(&group_desc_buf, GD_SIZE, 1, fp);
}
static void update_block_bitmap() {
    if (!fp) return;
    fseek(fp, BLOCK_BITMAP, SEEK_SET);
    fwrite(bitbuf, BLOCK_SIZE, 1, fp);
    fflush(fp);
}
static void load_block_bitmap() {
    if (!fp) return;
    fseek(fp, BLOCK_BITMAP, SEEK_SET);
    fread(bitbuf, BLOCK_SIZE, 1, fp);
}
static void update_inode_bitmap() {
    if (!fp) return;
    fseek(fp, INODE_BITMAP, SEEK_SET);
    fwrite(ibuf, BLOCK_SIZE, 1, fp);
    fflush(fp);
}
static void load_inode_bitmap() {
    if (!fp) return;
    fseek(fp, INODE_BITMAP, SEEK_SET);
    fread(ibuf, BLOCK_SIZE, 1, fp);
}
static void update_inode_entry(unsigned short i) {
    if (!fp) return;
    fseek(fp, INODE_TABLE + (i - 1) * INODE_SIZE, SEEK_SET);
    fwrite(&inode_buf, INODE_SIZE, 1, fp);
    fflush(fp);
}
static void load_inode_entry(unsigned short i) {
    if (!fp) return;
    fseek(fp, INODE_TABLE + (i - 1) * INODE_SIZE, SEEK_SET);
    fread(&inode_buf, INODE_SIZE, 1, fp);
}
static void update_dir(unsigned short i) {
    if (!fp) return;
    fseek(fp, DATA_BLOCK + i * BLOCK_SIZE, SEEK_SET);
    fwrite(dir_buf, BLOCK_SIZE, 1, fp);
    fflush(fp);
}
static void load_dir(unsigned short i) {
    if (!fp) return;
    fseek(fp, DATA_BLOCK + i * BLOCK_SIZE, SEEK_SET);
    fread(dir_buf, BLOCK_SIZE, 1, fp);
}
static void update_data_block(unsigned short i) {
    if (!fp) return;
    fseek(fp, DATA_BLOCK + i * BLOCK_SIZE, SEEK_SET);
    fwrite(Buffer, BLOCK_SIZE, 1, fp);
    fflush(fp);
}
static void load_data_block(unsigned short i) {
    if (!fp) return;
    fseek(fp, DATA_BLOCK + i * BLOCK_SIZE, SEEK_SET);
    fread(Buffer, BLOCK_SIZE, 1, fp);
}

//临时将新的inode读到指定缓冲区
static void load_inode_to_buf(unsigned short i, struct inode* buf) {
    if (!fp) return;
    fseek(fp, INODE_TABLE + (i - 1) * INODE_SIZE, SEEK_SET);
    fread(buf, INODE_SIZE, 1, fp);
}
/* ========== 分配/回收 ========== */

static int alloc_block() {
    load_block_bitmap();
    if (group_desc_buf.bg_free_blocks_count == 0) return 0;
    for (unsigned short byte = 0; byte < BLOCK_SIZE; ++byte) {
        if (bitbuf[byte] != 0xff) {
            for (int b = 0; b < 8; ++b) {
                unsigned char mask = 1 << (7 - b);
                if (!(bitbuf[byte] & mask)) {
                    bitbuf[byte] |= mask;
                    unsigned short blockno = byte * 8 + b;
                    last_alloc_block = blockno;
                    update_block_bitmap();
                    group_desc_buf.bg_free_blocks_count--;
                    update_group_desc();
                    return blockno;
                }
            }
        }
    }
    return 0;
}

static int alloc_inode() {
    load_inode_bitmap();
    if (group_desc_buf.bg_free_inodes_count == 0) return 0;
    for (unsigned short byte = 0; byte < BLOCK_SIZE; ++byte) {
        if (ibuf[byte] != 0xff) {
            for (int b = 0; b < 8; ++b) {
                unsigned char mask = 1 << (7 - b);
                if (!(ibuf[byte] & mask)) {
                    ibuf[byte] |= mask;
                    unsigned short ino = byte * 8 + b + 1; // inode 从1开始
                    last_alloc_inode = ino;
                    update_inode_bitmap();
                    group_desc_buf.bg_free_inodes_count--;
                    update_group_desc();
                    return ino;
                }
            }
        }
    }
    return 0;
}

static void remove_block(unsigned short block_no) {
    load_block_bitmap();
    unsigned short byte_index = block_no >> 3;
    unsigned char bit_position = block_no % 8;
    unsigned char mask = ~(1 << (7 - bit_position));
    bitbuf[byte_index] &= mask;
    update_block_bitmap();
    group_desc_buf.bg_free_blocks_count++;
    update_group_desc();
}

static void remove_inode(unsigned short inode_no) {
    if (inode_no == 0) return;
    load_inode_bitmap();
    unsigned short byte_index = (inode_no - 1) >> 3;
    unsigned char bit_position = (inode_no - 1) % 8;
    unsigned char mask = ~(1 << (7 - bit_position));
    ibuf[byte_index] &= mask;
    update_inode_bitmap();
    group_desc_buf.bg_free_inodes_count++;
    update_group_desc();
}

/* ========== 多级索引辅助 ========== */

unsigned short get_index_one(unsigned short logic_block_num, unsigned short inode_i) {
    load_inode_entry(inode_i);
    if (inode_buf.i_block[6] == 0) return 0;
    unsigned short idx = logic_block_num - 6;
    if (idx >= 256) return 0;
    load_data_block(inode_buf.i_block[6]);
    unsigned short blocknum;
    memcpy(&blocknum, Buffer + idx * 2, 2);
    return blocknum;
}

unsigned short get_index_two(unsigned short logic_block_num, unsigned short inode_i) {
    load_inode_entry(inode_i);
    if (inode_buf.i_block[7] == 0) return 0;
    unsigned int off = logic_block_num - 6 - 256;
    unsigned short second_index = off / 256;
    unsigned short first_index_offset = off % 256;
    load_data_block(inode_buf.i_block[7]);
    unsigned short first_block;
    memcpy(&first_block, Buffer + second_index * 2, 2);
    if (first_block == 0) return 0;
    load_data_block(first_block);
    unsigned short blocknum;
    memcpy(&blocknum, Buffer + first_index_offset * 2, 2);
    return blocknum;
}

/* ========== 目录项构造辅助 ========== */

struct dir_entry* create_dir_entry(unsigned short inode, unsigned char file_type, const char* name) {
    size_t nlen = strlen(name);
    unsigned char name_len = (unsigned char)nlen;
    unsigned short rec_len = calculate_rec_len(name_len);
    struct dir_entry* entry = (struct dir_entry*)malloc(rec_len);
    if (!entry) return NULL;
    entry->inode = inode;
    entry->rec_len = rec_len;
    entry->name_len = name_len;
    entry->file_type = file_type;
    memset(entry->name, 0, name_len + 1);
    memcpy(entry->name, name, name_len);
    return entry;
}

/* ========== 初始化 ========== */

void initialize_disk() {
    if (strlen(current_disk) == 0) strcpy(current_disk, "disk1.img");
    if (fp) fclose(fp);
    fp = fopen(current_disk, "wb+");
    if (!fp) {
        printf("Cannot create disk %s\n", current_disk);
        exit(1);
    }
    memset(Buffer, 0, BLOCK_SIZE);
    for (int i = 0; i < NUM_BLOCK; ++i) fwrite(Buffer, BLOCK_SIZE, 1, fp);

    // 初始化组描述符与位图
    memset(&group_desc_buf, 0, sizeof(group_desc_buf));
    group_desc_buf.bg_block_bitmap = BLOCK_BITMAP;
    group_desc_buf.bg_inode_bitmap = INODE_BITMAP;
    group_desc_buf.bg_inode_table = INODE_TABLE;
    group_desc_buf.bg_free_blocks_count = DATA_BLOCK_COUNTS;
    group_desc_buf.bg_free_inodes_count = INODE_TABLE_COUNTS;
    group_desc_buf.bg_used_dirs_count = 0;
    update_group_desc();

    memset(bitbuf, 0, BLOCK_SIZE);
    memset(ibuf, 0, BLOCK_SIZE);
    update_block_bitmap();
    update_inode_bitmap();

    last_alloc_block = 0;
    last_alloc_inode = 1;
}

/* init_dir_entry: 为新 inode 初始化目录块（只做第一个数据块，写 . 和 ..）
   注意：调用前 current_dir 应该指向父目录（若为根目录，调用前 current_dir 已设置为 root inode）
*/
static void init_dir_entry(unsigned short inode_i, unsigned char file_type, unsigned short mode) {
    load_inode_entry(inode_i);
    time_t now = time(NULL);
    if (file_type == FILE_DIR) {
        unsigned short b = alloc_block();
        if (b == 0) {
            printf("There is no block to be allocated for dir entry!\n");
            return;
        }
        // 设置 inode 的第0块
        inode_buf.i_block[0] = b;
        inode_buf.i_blocks = 1;
        inode_buf.i_mode = 0x4000 | (mode & 0x7); // 目录类型 + 权限
        inode_buf.i_atime = now;
        inode_buf.i_ctime = now;
        inode_buf.i_mtime = now;
        inode_buf.i_size = BLOCK_SIZE;

        // 初始化目录块：写入 "." 和 ".."
        memset(dir_buf, 0, BLOCK_SIZE);
        // "." entry
        struct dir_entry* dot = create_dir_entry(inode_i, FILE_DIR, ".");
        // ".." entry: 指向 parent = current_dir (调用 init_dir_entry 的时候，current_dir 应该指父)
        struct dir_entry* dotdot = create_dir_entry(current_dir, FILE_DIR, "..");

        if (dot && dotdot) {
            // 设置 rec_len: dot 占最小长度，dotdot 占剩余整个块
            unsigned short dot_len = calculate_rec_len(1);
            unsigned short dotdot_len = BLOCK_SIZE - dot_len;
            dot->rec_len = dot_len;
            dotdot->rec_len = dotdot_len;
            // 写入
            memcpy(dir_buf, dot, dot->rec_len);
            memcpy(dir_buf + dot->rec_len, dotdot, dotdot->rec_len);
            update_dir(b);
            free(dot);
            free(dotdot);
        } else {
            if (dot) free(dot);
            if (dotdot) free(dotdot);
            printf("Failed to create . or .. entries\n");
        }
        update_inode_entry(inode_i);
    } else {
        inode_buf.i_mode = 0x8000 | (mode & 0x7);
        inode_buf.i_size = 0;
        inode_buf.i_blocks = 0;
        inode_buf.i_atime = now;
        inode_buf.i_ctime = now;
        inode_buf.i_mtime = now;
        update_inode_entry(inode_i);
    }
}

/* ========== 查找 / 列表相关 ========== */

/* find_file: 在 current_dir 下查找 name，返回 inode_num / block逻辑索引 / start_byte / prev_start_byte
   file_type: FILE_FILE 或 FILE_DIR
*/
static unsigned short find_file(const char *name, unsigned char file_type,
                                unsigned short* inode_num, unsigned short* block_num,
                                unsigned short* start_byte, unsigned short* prev_start_byte) {
    load_inode_entry(current_dir);
    // 直接块
    for (unsigned short i = 0; i < inode_buf.i_blocks && i < 6; ++i) {
        load_dir(inode_buf.i_block[i]);
        unsigned short prev_offset = 0;
        unsigned short offset = 0;
        while (offset + 8 <= BLOCK_SIZE) {
            struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
            if (entry->rec_len == 0) break;
            if (entry->inode != 0) {
                unsigned short nlen = entry->name_len;
                if (nlen == strlen(name) && strncmp(entry->name, name, nlen) == 0 && entry->file_type == file_type) {
                    *inode_num = entry->inode;
                    *block_num = i;
                    *start_byte = offset;
                    *prev_start_byte = prev_offset;
                    return 1;
                }
            }
            prev_offset = offset;
            offset += entry->rec_len;
        }
    }

    // 一级索引
    if (inode_buf.i_blocks > 6 && inode_buf.i_block[6] != 0) {
        load_data_block(inode_buf.i_block[6]);
        unsigned short i_block_index = 0;
        unsigned short first_block_num;
        memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        while (first_block_num != 0) {
            load_dir(first_block_num);
            unsigned short prev_offset = 0;
            unsigned short offset = 0;
            while (offset + 8 <= BLOCK_SIZE) {
                struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                if (entry->rec_len == 0) break;
                if (entry->inode != 0) {
                    unsigned short nlen = entry->name_len;
                    if (nlen == strlen(name) && strncmp(entry->name, name, nlen) == 0 && entry->file_type == file_type) {
                        *inode_num = entry->inode;
                        *block_num = 6 + i_block_index;
                        *start_byte = offset;
                        *prev_start_byte = prev_offset;
                        return 1;
                    }
                }
                prev_offset = offset;
                offset += entry->rec_len;
            }
            ++i_block_index;
            memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        }

        // 二级索引
        if (inode_buf.i_blocks > 6 + 256 && inode_buf.i_block[7] != 0) {
            load_data_block(inode_buf.i_block[7]);
            unsigned short j_block_index = 0;
            unsigned short second_block_num;
            memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
            while (second_block_num != 0) {
                load_data_block(second_block_num);
                unsigned short k_block_index = 0;
                unsigned short first_block_num2;
                memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
                while (first_block_num2 != 0) {
                    load_dir(first_block_num2);
                    unsigned short prev_offset = 0;
                    unsigned short offset = 0;
                    while (offset + 8 <= BLOCK_SIZE) {
                        struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                        if (entry->rec_len == 0) break;
                        if (entry->inode != 0) {
                            unsigned short nlen = entry->name_len;
                            if (nlen == strlen(name) && strncmp(entry->name, name, nlen) == 0 && entry->file_type == file_type) {
                                *inode_num = entry->inode;
                                *block_num = (unsigned short)(6 + 256 + j_block_index*256 + k_block_index);
                                *start_byte = offset;
                                *prev_start_byte = prev_offset;
                                return 1;
                            }
                        }
                        prev_offset = offset;
                        offset += entry->rec_len;
                    }
                    ++k_block_index;
                    memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
                }
                ++j_block_index;
                load_data_block(inode_buf.i_block[7]);
                memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
            }
        }
    }

    return 0;
}

/* find_name_by_inode: 在 current_dir 中寻找 inode 并把名称复制到 name_buf */
static void find_name_by_inode(unsigned short inode_num, char *name_buf) {
    load_inode_entry(current_dir);
    // 直接块
    for (unsigned short i = 0; i < inode_buf.i_blocks && i < 6; ++i) {
        load_dir(inode_buf.i_block[i]);
        unsigned short offset = 0;
        while (offset + 8 <= BLOCK_SIZE) {
            struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
            if (entry->rec_len == 0) break;
            if (entry->inode == inode_num) {
                unsigned short n = entry->name_len;
                memcpy(name_buf, entry->name, n);
                name_buf[n] = '\0';
                return;
            }
            offset += entry->rec_len;
        }
    }
    // 一级索引
    if (inode_buf.i_blocks > 6 && inode_buf.i_block[6] != 0) {
        load_data_block(inode_buf.i_block[6]);
        unsigned short i_block_index = 0;
        unsigned short first_block_num;
        memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        while (first_block_num != 0) {
            load_dir(first_block_num);
            unsigned short offset = 0;
            while (offset + 8 <= BLOCK_SIZE) {
                struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                if (entry->rec_len == 0) break;
                if (entry->inode == inode_num) {
                    unsigned short n = entry->name_len;
                    memcpy(name_buf, entry->name, n);
                    name_buf[n] = '\0';
                    return;
                }
                offset += entry->rec_len;
            }
            ++i_block_index;
            memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        }
    }
    // 二级索引
    if (inode_buf.i_blocks > 6 + 256 && inode_buf.i_block[7] != 0) {
        load_data_block(inode_buf.i_block[7]);
        unsigned short j_block_index = 0;
        unsigned short second_block_num;
        memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        while (second_block_num != 0) {
            load_data_block(second_block_num);
            unsigned short k_block_index = 0;
            unsigned short first_block_num2;
            memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
            while (first_block_num2 != 0) {
                load_dir(first_block_num2);
                unsigned short offset = 0;
                while (offset + 8 <= BLOCK_SIZE) {
                    struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                    if (entry->rec_len == 0) break;
                    if (entry->inode == inode_num) {
                        unsigned short n = entry->name_len;
                        memcpy(name_buf, entry->name, n);
                        name_buf[n] = '\0';
                        return;
                    }
                    offset += entry->rec_len;
                }
                ++k_block_index;
                memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
            }
            ++j_block_index;
            load_data_block(inode_buf.i_block[7]);
            memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        }
    }
    name_buf[0] = '\0';
}

/* get_dir_size：用于打印目录大小（递归） */
int get_dir_size(unsigned short inode_no) {
    if (inode_no == 0) return 0;
    unsigned long total_size = 0;
    load_inode_entry(inode_no);
    unsigned short i;
    for (i = 0; i < inode_buf.i_blocks && i < 6; ++i) {
        load_dir(inode_buf.i_block[i]);
        unsigned short offset = 0;
        while (offset + 8 <= BLOCK_SIZE) {
            struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
            if (entry->rec_len == 0) break;
            if (entry->inode != 0) {
                if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0) { offset += entry->rec_len; continue; }
                if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0) { offset += entry->rec_len; continue; }
                load_inode_entry(entry->inode);
                if (entry->file_type == FILE_FILE) total_size += inode_buf.i_size;
                else if (entry->file_type == FILE_DIR) total_size += get_dir_size(entry->inode);
            }
            offset += entry->rec_len;
        }
    }
    // 一级索引
    if (inode_buf.i_blocks > 6 && inode_buf.i_block[6] != 0) {
        load_data_block(inode_buf.i_block[6]);
        unsigned short i_block_index = 0;
        unsigned short first_block_num;
        memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        while (first_block_num != 0) {
            load_dir(first_block_num);
            unsigned short offset = 0;
            while (offset + 8 <= BLOCK_SIZE) {
                struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                if (entry->rec_len == 0) break;
                if (entry->inode != 0) {
                    if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0) { offset += entry->rec_len; continue; }
                    if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0) { offset += entry->rec_len; continue; }
                    load_inode_entry(entry->inode);
                    if (entry->file_type == FILE_FILE) total_size += inode_buf.i_size;
                    else if (entry->file_type == FILE_DIR) total_size += get_dir_size(entry->inode);
                }
                offset += entry->rec_len;
            }
            ++i_block_index;
            memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        }
    }
    // 二级索引
    if (inode_buf.i_blocks > 6 + 256 && inode_buf.i_block[7] != 0) {
        load_data_block(inode_buf.i_block[7]);
        unsigned short j_block_index = 0;
        unsigned short second_block_num;
        memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        while (second_block_num != 0) {
            load_data_block(second_block_num);
            unsigned short k_block_index = 0;
            unsigned short first_block_num2;
            memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
            while (first_block_num2 != 0) {
                load_dir(first_block_num2);
                unsigned short offset = 0;
                while (offset + 8 <= BLOCK_SIZE) {
                    struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                    if (entry->rec_len == 0) break;
                    if (entry->inode != 0) {
                        if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0) { offset += entry->rec_len; continue; }
                        if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0) { offset += entry->rec_len; continue; }
                        load_inode_entry(entry->inode);
                        if (entry->file_type == FILE_FILE) total_size += inode_buf.i_size;
                        else if (entry->file_type == FILE_DIR) total_size += get_dir_size(entry->inode);
                    }
                    offset += entry->rec_len;
                }
                ++k_block_index;
                memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
            }
            ++j_block_index;
            load_data_block(inode_buf.i_block[7]);
            memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        }
    }
    
    return (int)total_size;
}

/*根据inode创建格式化时间*/
char* format_time(unsigned long raw_time) {
    static char time_str[20];
    time_t t = (time_t)raw_time;
    struct tm* tm_info = localtime(&t);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
    return time_str;
}

/* print_file_info */
//打印文件名，类型，以及各个时间
void print_file_info(char* name, unsigned char file_type,unsigned short indoe_no) {
    struct inode temp;
    load_inode_to_buf(indoe_no, &temp);
    char* type_str = (file_type == FILE_DIR) ? "DIR" : "FILE";
    char* atime_str = format_time(temp.i_atime);
    char* ctime_str = format_time(temp.i_ctime);
    char* mtime_str = format_time(temp.i_mtime);
    printf("%-10s %-4s  %s   %s   %s\n",
           name, type_str,  atime_str, ctime_str, mtime_str);
}

void ls() {
    // 载入父目录 inode 并备份
    load_inode_entry(current_dir);

    unsigned short i;
    // 直接块
    for (i = 0; i < inode_buf.i_blocks && i < 6; ++i) {
        unsigned short blockno = inode_buf.i_block[i];
        if (blockno == 0) continue;
        load_dir(blockno);
        unsigned short offset = 0;
        while (offset < BLOCK_SIZE) {
            struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
            if (entry->rec_len == 0) break;
            if (entry->inode != 0) {
                unsigned short n = entry->name_len;
                if (n > MAX_NAME_LEN) n = MAX_NAME_LEN;
                char name_tmp[MAX_NAME_LEN + 1];
                memcpy(name_tmp, entry->name, n);
                name_tmp[n] = '\0';
                print_file_info(name_tmp, entry->file_type, entry->inode);
            }
            offset += entry->rec_len;
        }
    }

    // 一级索引块
    if (inode_buf.i_blocks > 6 && inode_buf.i_block[6] != 0) {
        load_data_block(inode_buf.i_block[6]);
        unsigned short i_block_index = 0;
        unsigned short first_block_num;
        memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        while (first_block_num != 0) {
            load_dir(first_block_num);
            unsigned short offset = 0;
            while (offset <= BLOCK_SIZE) {
                struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                if (entry->rec_len == 0) break;
                if (entry->inode != 0) {
                    unsigned short n = entry->name_len;
                    if (n > MAX_NAME_LEN) n = MAX_NAME_LEN;
                    char name_tmp[MAX_NAME_LEN + 1];
                    memcpy(name_tmp, entry->name, n);
                    name_tmp[n] = '\0';
                    print_file_info(name_tmp, entry->file_type, entry->inode);
                }
                offset += entry->rec_len;
            }
            ++i_block_index;
            memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        }
    }

    // 二级索引块
    if (inode_buf.i_blocks > 6 + 256 && inode_buf.i_block[7] != 0) {
        load_data_block(inode_buf.i_block[7]);
        unsigned short j_block_index = 0;
        unsigned short second_block_num;
        memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        while (second_block_num != 0) {
            load_data_block(second_block_num);
            unsigned short k_block_index = 0;
            unsigned short first_block_num2;
            memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
            while (first_block_num2 != 0) {
                load_dir(first_block_num2);
                unsigned short offset = 0;
                while (offset <= BLOCK_SIZE) {
                    struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                    if (entry->rec_len == 0) break;
                    if (entry->inode != 0) {
                        unsigned short n = entry->name_len;
                        if (n > MAX_NAME_LEN) n = MAX_NAME_LEN;
                        char name_tmp[MAX_NAME_LEN + 1];
                        memcpy(name_tmp, entry->name, n);
                        name_tmp[n] = '\0';
                        print_file_info(name_tmp, entry->file_type, entry->inode);
                    }
                    offset += entry->rec_len;
                }
                ++k_block_index;
                memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
            }
            ++j_block_index;
            load_data_block(inode_buf.i_block[7]);
            memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        }
    }
}




/* ========== 创建/插入目录项 ========== */

/* help_code: 在 current_dir 下创建文件或目录（会分配 inode 并在父目录插入目录项）
   逻辑：
     - 检查是否已存在
     - 分配 inode，初始化 (init_dir_entry)
     - 在父目录（current_dir）中查找空闲或分裂位置插入目录项
*/

void help_code(const char *name, unsigned char file_type) {
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    load_dir(current_dir);
    unsigned short found = find_file(name, file_type, &inode_num, &logic_block_num, &start_byte, &prev_start_byte);
    unsigned short new_entry_len = calculate_rec_len((unsigned char)strlen(name));
    if (found == 1) {
        printf("Entry already exists!\n");
        return;
    }
    unsigned short new_inode = alloc_inode();
    if (new_inode == 0) {
        printf("No inode available!\n");
        return;
    }
    // 初始化新 inode (如果是目录会分配数据块并写入 . 和 ..，其中 .. 指向 current_dir)
    init_dir_entry(new_inode, file_type, 7);

    // 现在把新目录项插入到 current_dir 中
    load_inode_entry(current_dir);

    // 首先试直接块
    unsigned short i;
    for (i = 0; i < 6; ++i) {
        if (i < inode_buf.i_blocks) {
            unsigned short blockno = inode_buf.i_block[i];
            load_dir(blockno);
            unsigned short offset = 0;
            while (offset + 8 <= BLOCK_SIZE) {
                struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                if (entry->inode == 0) break;
                if (entry->inode != 0) {
                    unsigned short actual_len = calculate_actual_len(entry->name_len);
                    if (actual_len < entry->rec_len && (entry->rec_len - actual_len) >= new_entry_len) {
                        
                        struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name);
                        memcpy((char*)entry + actual_len, new_entry, new_entry_len);
                        entry->rec_len = actual_len;
                        free(new_entry);
                        update_dir(blockno);
                        inode_buf.i_size += new_entry_len;
                        update_inode_entry(current_dir);
                        return;
                    }
                    offset += entry->rec_len;
                } else {
                    unsigned short rest_len = BLOCK_SIZE - offset;
                    if (rest_len >= new_entry_len) {
                        
                        struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name);
                        memcpy((char*)entry, new_entry, new_entry_len);
                        entry->rec_len = new_entry_len;
                        free(new_entry);
                        update_dir(blockno);
                        inode_buf.i_size += new_entry_len;
                        update_inode_entry(current_dir);
                        return;
                    }
                    break;
                }
            }
        } else {
            
            unsigned short new_block = alloc_block();
            if (new_block == 0) {
                remove_inode(new_inode);
                printf("No block available to extend directory\n");
                return;
            }
            inode_buf.i_block[i] = new_block;
            inode_buf.i_blocks++;
            memset(dir_buf, 0, BLOCK_SIZE);
            struct dir_entry* new_entry = create_dir_entry(new_inode, file_type, name);
            memcpy(dir_buf, new_entry, new_entry_len);
            free(new_entry);
            update_dir(new_block);
            inode_buf.i_size += new_entry_len;
            update_inode_entry(current_dir);
            return;
        }
    }

    // 一级索引处理
    if (inode_buf.i_block[6] == 0) {
        unsigned short new_index = alloc_block();
        if (new_index == 0) { remove_inode(new_inode); return; }
        inode_buf.i_block[6] = new_index;
        update_inode_entry(current_dir);
    }
    load_data_block(inode_buf.i_block[6]);
    unsigned short i_block_index = 0;
    unsigned short first_block_num;
    memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
    while (first_block_num != 0) {
        load_dir(first_block_num);
        unsigned short offset = 0;
        while (offset + 8 <= BLOCK_SIZE) {
            struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
            if (entry->inode == 0) break;
            if (entry->inode != 0) {
                unsigned short actual_len = calculate_actual_len(entry->name_len);
                if (actual_len < entry->rec_len && (entry->rec_len - actual_len) >= new_entry_len) {
                    struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name);
                    memcpy((char*)entry + actual_len, new_entry, new_entry_len);
                    entry->rec_len = actual_len;
                    free(new_entry);
                    update_dir(first_block_num);
                    inode_buf.i_size += new_entry_len;
                    update_inode_entry(current_dir);
                    return;
                }
                offset += entry->rec_len;
            } else {
                unsigned short rest_len = BLOCK_SIZE - offset;
                if (rest_len >= new_entry_len) {
                    struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name);
                    memcpy((char*)entry, new_entry, new_entry_len);
                    entry->rec_len = new_entry_len;
                    free(new_entry);
                    update_dir(first_block_num);
                    inode_buf.i_size += new_entry_len;
                    update_inode_entry(current_dir);
                    return;
                }
                break;
            }
        }
        ++i_block_index;
        memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
    }
    // 如果没有可用一级块，追加
    if (i_block_index < 256) {
        unsigned short new_block = alloc_block();
        if (new_block == 0) { remove_inode(new_inode); return; }
        memcpy(Buffer + i_block_index * 2, &new_block, 2);
        update_data_block(inode_buf.i_block[6]);
        memset(dir_buf, 0, BLOCK_SIZE);
        struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name);
        memcpy(dir_buf, new_entry, new_entry_len);
        free(new_entry);
        update_dir(new_block);
        inode_buf.i_size += new_entry_len;
        inode_buf.i_blocks++;
        update_inode_entry(current_dir);
        return;
    }

    //二级索引处理
    if (inode_buf.i_block[7] == 0) {
        unsigned short new_index = alloc_block();
        if (new_index == 0) { remove_inode(new_inode); return; }
        inode_buf.i_block[7] = new_index;
        update_inode_entry(current_dir);
    }
    load_data_block(inode_buf.i_block[7]);
    unsigned short j_block_index = 0;
    unsigned short second_block_num;
    memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
    while (second_block_num != 0) {
        load_data_block(second_block_num);
        unsigned short k_block_index = 0;
        unsigned short first_block_num2;
        memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
        while (first_block_num2 != 0) {
            load_dir(first_block_num2);
            unsigned short offset = 0;
            while (offset + 8 <= BLOCK_SIZE) {
                struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                if (entry->inode == 0) break;
                if (entry->inode != 0) {
                    unsigned short actual_len = calculate_actual_len(entry->name_len);
                    if (actual_len < entry->rec_len && (entry->rec_len - actual_len) >= new_entry_len) {
                        struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name);
                        memcpy((char*)entry + actual_len, new_entry, new_entry_len);
                        entry->rec_len = actual_len;
                        free(new_entry);
                        update_dir(first_block_num2);
                        inode_buf.i_size += new_entry_len;
                        update_inode_entry(current_dir);
                        return;
                    }
                    offset += entry->rec_len;
                } else {
                    unsigned short rest_len = BLOCK_SIZE - offset;
                    if (rest_len >= new_entry_len) {
                        struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name); 
                        memcpy((char*)entry, new_entry, new_entry_len);
                        entry->rec_len = new_entry_len;
                        free(new_entry);
                        update_dir(first_block_num2);
                        inode_buf.i_size += new_entry_len;
                        update_inode_entry(current_dir);
                        return;
                    }
                    break;
                }
            }
            ++k_block_index;
            memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
        }
        ++j_block_index;
        load_data_block(inode_buf.i_block[7]);
        memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
    }
    // 如果没有可用二级块，追加
    if (j_block_index < 256) {
        unsigned short new_second_block = alloc_block();
        if (new_second_block == 0) { remove_inode(new_inode); return; }
        memcpy(Buffer + j_block_index * 2, &new_second_block, 2);
        update_data_block(inode_buf.i_block[7]);
        // 新增第一个一级块
        unsigned short new_first_block = alloc_block();
        if (new_first_block == 0) { remove_inode(new_inode); return; }
        memset(Buffer, 0, BLOCK_SIZE);
        memcpy(Buffer, &new_first_block, 2);
        update_data_block(new_second_block);
        // 在第一个一级块中写入目录项
        memset(dir_buf, 0, BLOCK_SIZE);
        struct dir_entry *new_entry = create_dir_entry(new_inode, file_type, name);
        memcpy(dir_buf, new_entry, new_entry_len);
        free(new_entry);
        update_dir(new_first_block);
        inode_buf.i_size += new_entry_len;
        inode_buf.i_blocks += 1; 
        update_inode_entry(current_dir);
        return;
    }
    printf("Directory is full, cannot create entry.\n");
    remove_inode(new_inode);
}

/* mkdir / create wrappers */
void mkdir_cmd(char *name) {
    help_code(name, FILE_DIR);
}

void create_file_cmd(char *name) {
    help_code(name, FILE_FILE);
}

/* ========== 删除实现 ========== */

//多级索引的辅助代码,用于获取索引块中的空闲数据块号
unsigned char get_empty_index(unsigned short block_num){
    load_data_block(block_num);
    unsigned char i=0;
    while(i<BLOCK_SIZE/2){
        unsigned short temp;
        memcpy(&temp,Buffer+i*2,2);
        if(temp==0){
            break;
        }
        i++;
    }
    return i;
}

// 
void del_help(const char *name, unsigned char file_type) {
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    unsigned short found = find_file(name, file_type, &inode_num, &logic_block_num, &start_byte, &prev_start_byte);
    if (!found) { printf("No such file or dir: %s\n", name); return; }

    // 关闭打开表项（如果存在）
    if (file_type == FILE_FILE) {
        int idx = -1;
        for (int i = 0; i < FOPEN_TABLE_MAX; ++i) if (fopen_table[i] == inode_num) { idx = i; break; }
        if (idx != -1) fopen_table[idx] = 0;
    }

    // 回收被删除 inode 的数据块并释放 inode
    load_inode_entry(inode_num);
    while (inode_buf.i_blocks > 0) {
        if (inode_buf.i_blocks <= 6) {
            inode_buf.i_blocks--;
            unsigned short b = inode_buf.i_block[inode_buf.i_blocks];
            if (b != 0) remove_block(b);
            inode_buf.i_block[inode_buf.i_blocks] = 0;
        } else if (inode_buf.i_blocks == 7) {
            load_data_block(inode_buf.i_block[6]);
            unsigned short idx = 0;
            unsigned short bn;
            memcpy(&bn, Buffer + idx*2, 2);
            while (bn != 0) { remove_block(bn); idx++; memcpy(&bn, Buffer + idx*2, 2); }
            remove_block(inode_buf.i_block[6]);
            inode_buf.i_block[6] = 0; inode_buf.i_blocks--;
        } else {
            load_data_block(inode_buf.i_block[7]);
            unsigned short second_block_index = 0;
            unsigned short first_index_block;
            memcpy(&first_index_block, Buffer + second_block_index*2, 2);
            while (first_index_block != 0) {
                load_data_block(first_index_block);
                unsigned short idx = 0; unsigned short b;
                memcpy(&b, Buffer + idx*2, 2);
                while (b != 0) { remove_block(b); idx++; memcpy(&b, Buffer + idx*2, 2); }
                remove_block(first_index_block);
                second_block_index++; memcpy(&first_index_block, Buffer + second_block_index*2, 2);
            }
            remove_block(inode_buf.i_block[7]);
            inode_buf.i_block[7] = 0;
            inode_buf.i_blocks = 0;
        }
    }
    inode_buf.i_size = 0; inode_buf.i_blocks = 0; update_inode_entry(inode_num);
    remove_inode(inode_num);

    // 更新父目录：删除目录项（注意先保存被删除项长度）
    load_inode_entry(current_dir); // 载入父目录 inode
    unsigned short data_block_num = 0;
    if (logic_block_num < 6) data_block_num = inode_buf.i_block[logic_block_num];
    else if (logic_block_num >= 6 && logic_block_num < 6 + 256) data_block_num = get_index_one(logic_block_num, current_dir);
    else data_block_num = get_index_two(logic_block_num, current_dir);

    if (data_block_num == 0) return;
    load_dir(data_block_num);
    struct dir_entry* entry = (struct dir_entry*)(dir_buf + start_byte);
    unsigned short deleted_len = entry->rec_len; // 保留待调整父目录大小使用

    if (start_byte == 0) {
        // 如果是块内第一个目录项，标记为已删除（inode置0）
        entry->inode = 0;
    } else {
        // 将删除项的空间并入前一项
        struct dir_entry* prev_entry = (struct dir_entry*)(dir_buf + prev_start_byte);
        prev_entry->rec_len = prev_entry->rec_len + entry->rec_len;
        // 标记删除（清 inode）
        entry->inode = 0;
        // 保持 entry->rec_len 原值或置0 均可，但为了安全不破坏内存结构不强制置0
    }

    // 调整父目录大小（使用删除前的长度）
    if (inode_buf.i_size >= deleted_len) inode_buf.i_size -= deleted_len;
    update_dir(data_block_num);
    update_inode_entry(current_dir);

    // 检查该数据块是否已全部为空（没有任何非0 inode）
    unsigned short offset = 0;
    load_dir(data_block_num);
    unsigned char is_empty = 1;
    while (offset + DIR_ENTRY_HEADER_SIZE <= BLOCK_SIZE) {
        struct dir_entry* check_entry = (struct dir_entry*)(dir_buf + offset);
        if (check_entry->rec_len == 0) break;
        if (check_entry->inode != 0) { is_empty = 0; break; }
        offset += check_entry->rec_len;
    }

    if (is_empty) {
        // 删除该数据块并从父 inode 的块数组中移除对应项（直接块/一级/二级）
        unsigned short orig_blocks = inode_buf.i_blocks;
        remove_block(data_block_num);

        if (logic_block_num < 6) {
            // shift direct blocks left
            unsigned short i;
            for (i = logic_block_num; i + 1 < orig_blocks && i + 1 < 6; ++i) {
                inode_buf.i_block[i] = inode_buf.i_block[i+1];
            }
            // 清空最后一个剩余槽
            if (orig_blocks <= 6) inode_buf.i_block[orig_blocks-1] = 0;
            else inode_buf.i_block[5] = 0;
            if (inode_buf.i_blocks > 0) inode_buf.i_blocks--;
            update_inode_entry(current_dir);
        }
        else if (logic_block_num >= 6 && logic_block_num < 6 + 256) {
            // 在一级索引中移位
            load_data_block(inode_buf.i_block[6]);
            unsigned short idx = logic_block_num - 6;
            unsigned short cur = idx;
            unsigned short next_block;
            memcpy(&next_block, Buffer + (cur+1)*2, 2);
            while (cur < 255) {
                memcpy(Buffer + cur*2, Buffer + (cur+1)*2, 2);
                cur++;
                memcpy(&next_block, Buffer + (cur+1)*2, 2);
                if (cur >= 255) break;
            }
            // 将末尾清0
            memcpy(Buffer + 255*2, "\0\0", 2);
            update_data_block(inode_buf.i_block[6]);
            if (inode_buf.i_blocks > 0) inode_buf.i_blocks--;
            // 若一级索引已空则释放
            if (get_empty_index(inode_buf.i_block[6]) == 0) {
                remove_block(inode_buf.i_block[6]);
                inode_buf.i_block[6] = 0;
            }
            update_inode_entry(current_dir);
        }
        else {
            // 二级索引：只做局部移位与空检查（保留原思路，不改变数据结构）
            load_data_block(inode_buf.i_block[7]);
            unsigned short second_idx = (logic_block_num - 6 - 256) / 256;
            unsigned short first_idx = (logic_block_num - 6 - 256) % 256;
            unsigned short first_block;
            memcpy(&first_block, Buffer + second_idx*2, 2);
            if (first_block != 0) {
                load_data_block(first_block);
                unsigned short cur = first_idx;
                while (cur < 255) {
                    memcpy(Buffer + cur*2, Buffer + (cur+1)*2, 2);
                    cur++;
                }
                memcpy(Buffer + 255*2, "\0\0", 2);
                update_data_block(first_block);
                // 如果该一级索引块为空则释放并在二级索引中移位
                if (get_empty_index(first_block) == 0) {
                    remove_block(first_block);
                    // 在二级索引块中移位对应的一级块指针
                    unsigned short j = second_idx;
                    unsigned short next_first;
                    memcpy(&next_first, Buffer + (j+1)*2, 2);
                    while (next_first != 0) {
                        memcpy(Buffer + j*2, &next_first, 2);
                        j++;
                        memcpy(&next_first, Buffer + (j+1)*2, 2);
                    }
                    memcpy(Buffer + j*2, "\0\0", 2);
                    update_data_block(inode_buf.i_block[7]);
                }
                // 若二级索引本身为空则释放
                if (get_empty_index(inode_buf.i_block[7]) == 0) {
                    remove_block(inode_buf.i_block[7]);
                    inode_buf.i_block[7] = 0;
                }
            }
            if (inode_buf.i_blocks > 0) inode_buf.i_blocks--;
            update_inode_entry(current_dir);
        }
    }
}


/* rmdir：仅允许空目录 */
void rmdir_cmd(char *name) {
    unsigned short inode_num, block_num, start_byte, prev_start_byte;
    unsigned short found = find_file(name, FILE_DIR, &inode_num, &block_num, &start_byte, &prev_start_byte);
    if (!found) { printf("No such directory!\n"); return; }
    load_inode_entry(inode_num);
    unsigned short i;
    unsigned char is_empty = 1;
    // 检查目录是否为空
    // 检查直接块
    for (i = 0; i < inode_buf.i_blocks && i < 6; ++i) {
        load_dir(inode_buf.i_block[i]);
        unsigned short offset = 0;
        while (offset + 8 <= BLOCK_SIZE) {
            struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
            if (entry->rec_len == 0) break;
            if (entry->inode != 0) {
                if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0) { offset += entry->rec_len; continue; }
                if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0) { offset += entry->rec_len; continue; }
                is_empty = 0; break;
            }
            offset += entry->rec_len;
        }
        if (!is_empty) break;
    }
    // 检查一级索引
    if (is_empty && inode_buf.i_blocks > 6 && inode_buf.i_block[6] != 0) {
        load_data_block(inode_buf.i_block[6]);
        unsigned short i_block_index = 0;
        unsigned short first_block_num;
        memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        while (first_block_num != 0) {
            load_dir(first_block_num);
            unsigned short offset = 0;
            while (offset + 8 <= BLOCK_SIZE) {
                struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                if (entry->rec_len == 0) break;
                if (entry->inode != 0) {
                    if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0) { offset += entry->rec_len; continue; }
                    if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0) { offset += entry->rec_len; continue; }
                    is_empty = 0; break;
                }
                offset += entry->rec_len;
            }
            if (!is_empty) break;
            ++i_block_index;
            memcpy(&first_block_num, Buffer + i_block_index * 2, 2);
        }
    }
    // 检查二级索引
    if (is_empty && inode_buf.i_blocks > 6 + 256 && inode_buf.i_block[7] != 0) {
        load_data_block(inode_buf.i_block[7]);
        unsigned short j_block_index = 0;
        unsigned short second_block_num;
        memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        while (second_block_num != 0) {
            load_data_block(second_block_num);
            unsigned short k_block_index = 0;
            unsigned short first_block_num2;
            memcpy(&first_block_num2, Buffer + k_block_index * 2, 2);
            while (first_block_num2 != 0) {
                load_dir(first_block_num2);
                unsigned short offset = 0;
                while (offset + 8 <= BLOCK_SIZE) {
                    struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
                    if (entry->rec_len == 0) break;
                    if (entry->inode != 0) {
                        if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0) { offset += entry->rec_len; continue; }
                        if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0) { offset += entry->rec_len; continue; }
                        is_empty = 0; break;
                    }
                    offset += entry->rec_len;
                }
                if (!is_empty) break;
                ++k_block_index;
                memcpy(&first_block_num2, Buffer + k_block_index * 2, 2 );
            }
            if (!is_empty) break;
            ++j_block_index;
            load_data_block(inode_buf.i_block[7]);
            memcpy(&second_block_num, Buffer + j_block_index * 2, 2);
        }
    }

    if (!is_empty) { printf("Directory is not empty!\n"); return; }
    del_help(name, FILE_DIR);
}

/* remove_dir: 递归删除目录及其内容 */
void remove_dir(unsigned short inode_no) {
    if (inode_no == 0) return;
    unsigned short save_current = current_dir;
    current_dir = inode_no;
    load_inode_entry(inode_no);

    unsigned short i;
    for (i = 0; i < inode_buf.i_blocks && i < 6; ++i) {
        load_dir(inode_buf.i_block[i]);
        unsigned short offset = 0;
        while (offset + 8 <= BLOCK_SIZE) {
            struct dir_entry* entry = (struct dir_entry*)(dir_buf + offset);
            if (entry->rec_len == 0) break;
            if (entry->inode != 0) {
                if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0) { offset += entry->rec_len; continue; }
                if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0) { offset += entry->rec_len; continue; }
                if (entry->file_type == FILE_FILE) {
                    del_help(entry->name, FILE_FILE);
                    load_dir(inode_buf.i_block[i]); // 重新载入以防被修改
                } else if (entry->file_type == FILE_DIR) {
                    unsigned short child = entry->inode;
                    remove_dir(child);
                    load_dir(inode_buf.i_block[i]);
                }
            }
            offset += entry->rec_len;
        }
    }

    current_dir = save_current;
    char name_buf[256];
    find_name_by_inode(inode_no, name_buf);
    if (strlen(name_buf) > 0) del_help(name_buf, FILE_DIR);
}

/* rm: 递归删除目录 */
void rm_cmd(char *name) {
    unsigned short inode_num, block_num, start_byte, prev_start_byte;
    unsigned short found = find_file(name, FILE_DIR, &inode_num, &block_num, &start_byte, &prev_start_byte);
    if (!found) { printf("No such directory!\n"); return; }
    remove_dir(inode_num);
}

/* ========== 打开/读写 ========== */

static int find_open_table_index(unsigned short inode) {
    for (int i = 0; i < FOPEN_TABLE_MAX; ++i) if (fopen_table[i] == inode) return i;
    return -1;
}

void open_file(char *name) {
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    if (!find_file(name, FILE_FILE, &inode_num, &logic_block_num, &start_byte, &prev_start_byte)) {
        printf("No such file!\n"); return;
    }
    if (find_open_table_index(inode_num) != -1) { printf("File already opened!\n"); return; }
    for (int i = 0; i < FOPEN_TABLE_MAX; ++i) {
        if (fopen_table[i] == 0) {
            fopen_table[i] = inode_num;
            load_inode_entry(inode_num);
            time_t t = time(NULL);
            inode_buf.i_atime = t;
            update_inode_entry(inode_num);
            printf("File %s opened successfully!\n", name);
            return;
        }
    }
    printf("Too many open files!\n");
}

void close_file(char *name) {
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    if (!find_file(name, FILE_FILE, &inode_num, &logic_block_num, &start_byte, &prev_start_byte)) {
        printf("No such file!\n"); return;
    }
    int idx = find_open_table_index(inode_num);
    if (idx != -1) { fopen_table[idx] = 0; printf("File %s closed successfully!\n", name); return; }
    printf("File is not opened!\n");
}

void read_file(char *name) {
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    if (!find_file(name, FILE_FILE, &inode_num, &logic_block_num, &start_byte, &prev_start_byte)) {
        printf("No such file!\n"); return;
    }
    if (find_open_table_index(inode_num) == -1) { printf("File is not opened!\n"); return; }
    load_inode_entry(inode_num);
    time_t t = time(NULL);
    inode_buf.i_atime = t;
    update_inode_entry(inode_num);

    unsigned long file_size = inode_buf.i_size;
    unsigned long bytes_read = 0;
    unsigned short current_logic_block = 0;
    while (bytes_read < file_size) {
        unsigned short blocknum;
        if (current_logic_block < 6) blocknum = inode_buf.i_block[current_logic_block];
        else if (current_logic_block >= 6 && current_logic_block < 6+256) blocknum = get_index_one(current_logic_block, inode_num);
        else blocknum = get_index_two(current_logic_block, inode_num);
        if (blocknum == 0) break;
        load_data_block(blocknum);
        unsigned short off = bytes_read % BLOCK_SIZE;
        while (off < BLOCK_SIZE && bytes_read < file_size) {
            putchar(Buffer[off]);
            off++; bytes_read++;
        }
        current_logic_block++;
    }
    putchar('\n');
}

void write_file(char *name) {
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    if (!find_file(name, FILE_FILE, &inode_num, &logic_block_num, &start_byte, &prev_start_byte)) {
        printf("No such file!\n"); return;
    }
    if (find_open_table_index(inode_num) == -1) { printf("File is not opened!\n"); return; }
    load_inode_entry(inode_num);
    time_t t = time(NULL);
    inode_buf.i_atime = t; inode_buf.i_mtime = t;
    update_inode_entry(inode_num);

    // 从 stdin 读一行作为文件内容
    unsigned long bytes_written = 0;
    int ch;
    while ((ch = getchar()) != EOF && ch != '\n' && bytes_written < MAX_FILE_SIZE) {
        tempbuf[bytes_written++] = (char)ch;
    }
    tempbuf[bytes_written] = '\0';
    unsigned long total = bytes_written;

    // 释放旧数据
    load_inode_entry(inode_num);
    while (inode_buf.i_blocks > 0) {
        if (inode_buf.i_blocks <= 6) {
            inode_buf.i_blocks--;
            remove_block(inode_buf.i_block[inode_buf.i_blocks]);
            inode_buf.i_block[inode_buf.i_blocks] = 0;
        } else if (inode_buf.i_blocks == 7) {
            load_data_block(inode_buf.i_block[6]);
            unsigned short idx = 0; unsigned short b;
            memcpy(&b, Buffer + idx*2, 2);
            while (b != 0) { remove_block(b); idx++; memcpy(&b, Buffer + idx*2, 2); }
            remove_block(inode_buf.i_block[6]);
            inode_buf.i_block[6] = 0; inode_buf.i_blocks--;
        } else {
            load_data_block(inode_buf.i_block[7]);
            unsigned short second_block_index = 0; unsigned short first_index_block;
            memcpy(&first_index_block, Buffer + second_block_index*2, 2);
            while (first_index_block != 0) {
                load_data_block(first_index_block);
                unsigned short idx = 0; unsigned short b;
                memcpy(&b, Buffer + idx*2, 2);
                while (b != 0) { remove_block(b); idx++; memcpy(&b, Buffer + idx*2, 2); }
                remove_block(first_index_block);
                second_block_index++; memcpy(&first_index_block, Buffer + second_block_index*2, 2);
            }
            remove_block(inode_buf.i_block[7]);
            inode_buf.i_block[7] = 0;
            inode_buf.i_blocks = 0;
        }
    }
    inode_buf.i_size = 0; inode_buf.i_blocks = 0; update_inode_entry(inode_num);

    // 写新数据（按逻辑块追加）
    unsigned long written = 0;
    unsigned short current_logic_block = 0;
    while (written < total) {
        unsigned short blockno = 0;
        if (current_logic_block < 6) {
            if (current_logic_block >= inode_buf.i_blocks) {
                blockno = alloc_block();
                if (!blockno) { printf("No space!\n"); return; }
                inode_buf.i_block[current_logic_block] = blockno;
                inode_buf.i_blocks++;
                update_inode_entry(inode_num);
            } else blockno = inode_buf.i_block[current_logic_block];
        } else if (current_logic_block >= 6 && current_logic_block < 6+256) {
            blockno = get_index_one(current_logic_block, inode_num);
            if (blockno == 0) {
                blockno = alloc_block();
                if (!blockno) { printf("No space!\n"); return; }
                // 写入一级索引
                load_inode_entry(inode_num);
                if (inode_buf.i_block[6] == 0) {
                    unsigned short idxblk = alloc_block();
                    inode_buf.i_block[6] = idxblk;
                    update_inode_entry(inode_num);
                }
                load_data_block(inode_buf.i_block[6]);
                unsigned short idx = current_logic_block - 6;
                memcpy(Buffer + idx*2, &blockno, 2);
                update_data_block(inode_buf.i_block[6]);
            }
        } else {
            blockno = get_index_two(current_logic_block, inode_num);
            if (blockno == 0) {
                blockno = alloc_block();
                if (!blockno) { printf("No space!\n"); return; }
                printf("Large files > single indirect not fully supported in write demo.\n");
                return;
            }
        }
        load_data_block(blockno);
        unsigned short off = 0;
        while (off < BLOCK_SIZE && written < total) {
            Buffer[off++] = tempbuf[written++];
        }
        update_data_block(blockno);
        current_logic_block++;
    }
    load_inode_entry(inode_num);
    inode_buf.i_size = total;
    update_inode_entry(inode_num);
}

void append_file(char *name) {
    //实现
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    if (!find_file(name, FILE_FILE, &inode_num, &logic_block_num, &start_byte, &prev_start_byte)) {
        printf("No such file!\n"); return;
    }
    if (find_open_table_index(inode_num) == -1) { printf("File is not opened!\n"); return; }
    load_inode_entry(inode_num);
    time_t t = time(NULL);
    inode_buf.i_atime = t; inode_buf.i_mtime = t;
    update_inode_entry(inode_num);
    // 从 stdin 读一行作为追加内容
    unsigned long bytes_appended = 0;
    int ch;
    while ((ch = getchar()) != EOF && ch != '\n' && bytes_appended < MAX_FILE_SIZE) {
        tempbuf[bytes_appended++] = (char)ch;
    }
    tempbuf[bytes_appended] = '\0';
    unsigned long total = bytes_appended;
    unsigned long file_size = inode_buf.i_size;
    unsigned long written = 0;
    unsigned short current_logic_block = file_size / BLOCK_SIZE;
    unsigned short offset_in_block = file_size % BLOCK_SIZE;
    while (written < total) {
        unsigned short blockno = 0;
        if (current_logic_block < 6) {
            if (current_logic_block >= inode_buf.i_blocks) {
                blockno = alloc_block();
                if (!blockno) { printf("No space!\n"); return; }
                inode_buf.i_block[current_logic_block] = blockno;
                inode_buf.i_blocks++;
                update_inode_entry(inode_num);
            } else blockno = inode_buf.i_block[current_logic_block];
        } else if (current_logic_block >= 6 && current_logic_block < 6+256) {
            blockno = get_index_one(current_logic_block, inode_num);
            if (blockno == 0) {
                blockno = alloc_block();
                if (!blockno) { printf("No space!\n"); return; }
                // 写入一级索引
                load_inode_entry(inode_num);
                if (inode_buf.i_block[6] == 0) {
                    unsigned short idxblk = alloc_block();
                    inode_buf.i_block[6] = idxblk;
                    update_inode_entry(inode_num);
                }
                load_data_block(inode_buf.i_block[6]);
                unsigned short idx = current_logic_block - 6;
                memcpy(Buffer + idx*2, &blockno, 2);
                update_data_block(inode_buf.i_block[6]);
            }
        } else {
            blockno = get_index_two(current_logic_block, inode_num);
            if (blockno == 0) {
                blockno = alloc_block();
                if (!blockno) { printf("No space!\n"); return; }
                printf("Large files > single indirect not fully supported in append demo.\n");
                return;
            }
        }
        load_data_block(blockno);
        unsigned short off = offset_in_block;
        while (off < BLOCK_SIZE && written < total) {
            Buffer[off++] = tempbuf[written++];
        }
        update_data_block(blockno);
        current_logic_block++;
        offset_in_block = 0; 
    }
    load_inode_entry(inode_num);
    inode_buf.i_size += total;
    update_inode_entry(inode_num);


}
/* ========== cd 实现（使用 find_file 查找目标 inode） ========== */

static int cd_onestep(const char *name) {
    if (strcmp(name, ".") == 0) {
        // 仅更新时间
        load_inode_entry(current_dir);
        inode_buf.i_atime = time(NULL);
        update_inode_entry(current_dir);
        return 1;
    }

    // 使用 find_file 去查找目标 directory 的 inode（包括 ".."）
    unsigned short inode_num, logic_block_num, start_byte, prev_start_byte;
    unsigned short flag = find_file(name, FILE_DIR, &inode_num, &logic_block_num, &start_byte, &prev_start_byte);
    if (!flag) {
        printf("No such directory: %s\n", name);
        return 0;
    }

    // 切换 current_dir
    current_dir = inode_num;

    // 更新 current_path 字符串：如果 name == ".." 则去掉最后一个组件，否则追加 name/
    if (strcmp(name, "..") == 0) {
        
        char *p = (char*)strrchr(current_path, '/');
        if (p != NULL) {
           
            *p = '\0';
            
            char *q = (char*)strrchr(current_path, '/');
            if (q != NULL) {
                
                *(q+1) = '\0';
            } else {
                
                strcpy(current_path + strlen(current_path), "/");
            }
        }
       
        if (current_path[strlen(current_path)-1] == '/') current_dirlen = 0;
        else {
            char *last = strrchr(current_path, '/');
            if (last) current_dirlen = strlen(last+1);
            else current_dirlen = 0;
        }
    } else {
        
        if (current_path[strlen(current_path)-1] != '/') strcat(current_path, "/");
        strcat(current_path, name);
        strcat(current_path, "/");
        current_dirlen = strlen(name);
    }

    // 更新时间
    load_inode_entry(current_dir);
    inode_buf.i_atime = time(NULL);
    update_inode_entry(current_dir);

    return 1;
}

void cd(char *path) {
    if (!path) return;
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    char *p = strtok(tmp, "/");
    while (p) {
        if (!cd_onestep(p)) return;
        p = strtok(NULL, "/");
    }
}

/* ========== 其它工具 ========== */

void check_disk() {
    printf("The disk status is as follows:\n");
    printf("The name of the volume is: %s\n", VOLUME_NAME);
    printf("The name of the disk is: %s\n", current_disk);
    printf("The size of the Block is: %d\n", BLOCK_SIZE);
    printf("The number of the Block is: %d\n", NUM_BLOCK);
    float total = BLOCK_SIZE * NUM_BLOCK / 1024.0f / 1024.0f;
    printf("The total size of the disk is: %.2fMB\n", total);
}

void show_help() {
    printf("Available commands:\n");
    printf("create <filename>\n");
    printf("mkdir <dirname>\n");
    printf("delete <filename>\n");
    printf("rmdir <dirname>\n");
    printf("rm <dirname>\n");
    printf("open <filename>\n");
    printf("close <filename>\n");
    printf("read <filename>\n");
    printf("write <filename>\n");
    printf("ls\n");
    printf("cd <path>\n");
    printf("format\n");
    printf("ckdisk\n");
    printf("help\n");
    printf("exit\n");
}

/* ========== 用户管理 ========== */

void initialize_user() {
    FILE *uf = fopen("./user/userlist.txt", "r");
    if (!uf) {
        printf("No user in userlist.txt! create an admin!\n");
        strcpy(User[0].username, "admin");
        strcpy(User[0].password, "admin");
        strcpy(User[0].disk_name, "disk1.img");
        uf = fopen("./user/userlist.txt", "w");
        if (!uf) { printf("Can't create userlist.txt\n"); exit(1); }
        fprintf(uf, "%s %s %s\n", User[0].username, User[0].password, User[0].disk_name);
        fclose(uf);
        user_num = 1;
        return;
    }
    int i = 0;
    while (!feof(uf) && i < USER_MAX) {
        if (fscanf(uf, "%s %s %s\n", User[i].username, User[i].password, User[i].disk_name) == 3) ++i;
        else break;
    }
    fclose(uf);
    user_num = i;
}

int login(const char *username, const char *password) {
    for (int i = 0; i < user_num; ++i) {
        if (strcmp(User[i].username, username) == 0 && strcmp(User[i].password, password) == 0) {
            strcpy(current_disk, User[i].disk_name);
            return 1;
        }
    }
    return 0;
}

/* ========== 命令解析与主循环 ========== */

void execute_command(char *input) {
    char *args[10];
    char *token = strtok(input, " \n");
    int argc = 0;
    while (token && argc < 10) { args[argc++] = token; token = strtok(NULL, " \n"); }
    if (argc == 0) return;

    if (strcmp(args[0], "cd") == 0) {
        if (argc > 1) cd(args[1]);
    } else if (strcmp(args[0], "ls") == 0) {
        ls();
    } else if (strcmp(args[0], "mkdir") == 0) {
        if (argc > 1) mkdir_cmd(args[1]);
    } else if (strcmp(args[0], "rmdir") == 0) {
        if (argc > 1) rmdir_cmd(args[1]);
    } else if (strcmp(args[0], "create") == 0) {
        if (argc > 1) create_file_cmd(args[1]);
    } else if (strcmp(args[0], "delete") == 0) {
        if (argc > 1) del_help(args[1], FILE_FILE);
    } else if (strcmp(args[0], "rm") == 0) {
        if (argc > 1) rm_cmd(args[1]);
    } else if (strcmp(args[0], "open") == 0) {
        if (argc > 1) open_file(args[1]);
    } else if (strcmp(args[0], "close") == 0) {
        if (argc > 1) close_file(args[1]);
    } else if (strcmp(args[0], "read") == 0) {
        if (argc > 1) read_file(args[1]);
    } else if (strcmp(args[0], "write") == 0) {
        if (argc > 2 && strcmp(args[1], "-a") == 0) append_file(args[2]);
        else if (argc > 1) write_file(args[1]);
    } else if (strcmp(args[0], "format") == 0) {
        printf("Confirm to format the file system? (y/n): ");
        char c = getchar(); while (getchar() != '\n');
        if (c == 'y' || c == 'Y') { initialize_disk(); initialize_memory(); printf("Formatted\n"); }
    } else if (strcmp(args[0], "ckdisk") == 0) check_disk();
    else if (strcmp(args[0], "help") == 0) show_help();
    else if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) exit(0);
    else printf("Unknown command: %s\n", args[0]);
}

void shell_loop() {
    char input[256];
    printf("===============================================\n");
    printf("        Welcome to the EXT2 File System Simulator\n");
    printf("===============================================\n");
    printf("Type 'help' to see available commands.\n");
    while (1) {
        printf("%s]$ ", current_path);
        if (!fgets(input, sizeof(input), stdin)) break;
        execute_command(input);
    }
}

/* initialize_memory: 打开磁盘，或创建默认磁盘并初始化 root inode */
void initialize_memory() {
    int i;
    last_alloc_inode = 1;
    last_alloc_block = 0;
    for (i = 0; i < FOPEN_TABLE_MAX; ++i) fopen_table[i] = 0;
    strcpy(current_path, "[");
    strcat(current_path, current_user);
    strcat(current_path, "@ext2 /");
    current_dir = 0;
    current_dirlen = 0;

    if (strlen(current_disk) == 0) strcpy(current_disk, "disk1.img");

    fp = fopen(current_disk, "rb+");
    if (fp == NULL) {
        printf("The File system does not exist!\n");
        initialize_disk();
        fp = fopen(current_disk, "rb+");
        if (fp == NULL) { printf("Failed to create disk\n"); exit(1); }
    }

    // 检查是否为空
    fseek(fp, 0, SEEK_SET);
    char c; int flag = 0;
    while (!feof(fp)) {
        if (fread(&c, 1, 1, fp) == 1 && c != 0) { flag = 1; break; }
        if (feof(fp)) break;
    }
    if (!flag) {
        printf("The File system does not exist!\n");
        initialize_disk();
        fp = fopen(current_disk, "rb+");
        if (fp == NULL) { printf("Failed to create disk\n"); exit(1); }
    }

    load_group_desc();
    load_block_bitmap();
    load_inode_bitmap();

    // 分配 root inode 并初始化 root 目录
    current_dir = alloc_inode();
    if (current_dir == 0) { printf("No inode for root!\n"); exit(1); }
    // For root, current_dir is set to root before init_dir_entry so that .. points to itself
    init_dir_entry(current_dir, FILE_DIR, 7);
    current_dirlen = 0;
}

/* ========== main ========== */

int main() {
    memset(current_user, 0, sizeof(current_user));
    memset(current_disk, 0, sizeof(current_disk));
    initialize_user();

    char username[10], password[10];
    int login_success = 0;
    while (!login_success) {
        printf("Username: ");
        if (scanf("%9s", username) != 1) exit(1);
        printf("Password: ");
        if (scanf("%9s", password) != 1) exit(1);
        while (getchar() != '\n');
        if (login(username, password)) {
            login_success = 1;
            strcpy(current_user, username);
            printf("Welcome, %s!\n", current_user);
        } else {
            printf("Invalid username or password. Please try again.\n");
        }
    }

    initialize_memory();
    shell_loop();
    return 0;
}
