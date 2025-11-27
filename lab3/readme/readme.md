# 类EXT2文件系统设计

### 一些宏定义
```c
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
```
### 结构体定义
```c
struct dir_entry {
    unsigned short inode;
    unsigned short rec_len;
    unsigned char name_len;
    unsigned char file_type;
    char name[]; // 柔性数组
};

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
```
为了实现变长目录项，我的dir定义中使用了柔性数组，并且为了后面的目录项初始化，我定义了根据名字长度来计算填充后的长度的函数

```c
static inline unsigned short calculate_rec_len(unsigned char name_len)
{
    unsigned short base_len = DIR_ENTRY_HEADER_SIZE + name_len;
    return (base_len + 3) & ~3;  // 4字节对齐
}//填充长度
static inline unsigned short calculate_actual_len(unsigned char name_len)
{
    return DIR_ENTRY_HEADER_SIZE + name_len;
}//实际长度
```

### 全局变量定义，用来模拟内存
```c
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
```

### 关于磁盘io，也就是将一些inode，或者block读到内存缓冲区的函数
```c
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
```

### 数据块与inode的分配与回收
```c
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
```

基本策略就是：每次需要读inode或者block时，都调用load_xxx函数将数据读到对应的缓冲区，每次修改完缓冲区的数据后，都调用update_xxx函数将缓冲区的数据写回磁盘。

为了实现快速定位，使用了fseek函数直接定位到对应的偏移位置进行读写。

### 关于多级索引
我的设计就是，直接块0-5存放直接数据块号，第6块存放一级索引块号，第7块存放二级索引块号。

直接块直接存放数据块号，一级索引块存放256个数据块号，二级索引块存放256个一级索引块号。

并且我的inode结构体存放着关于块数的信息，我对于块数的定义，只有指向数据的块数，不包括索引块。

因此直接块的逻辑块号0-5对应块数0-5，一级索引块对应块数6-261，二级索引块对应块数262-65797。

并且将某一个块删除之后，我实现了后续块的迁移，因此可以根据逻辑块号快速定位到对应的数据块号，而不需要遍历整个索引结构

为了将逻辑块号映射到物理块号，我实现了如下函数：

```c
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
```

### 关于mkdir或者create file操作

关于mkdir和create file的实现，逻辑上是类似的，都是在当前目录下创建一个新的目录项，指向新创建的文件或目录。

对于目录的话，除了创建目录项之外，还需要初始化新目录的inode和数据块，写入"."和".."两个目录项。而对于文件，只需要创建目录项和初始化inode即可。数据块在写入数据时分配。

因此我定义help_code函数来实现这个功能，传入文件名和文件类型（目录或文件）。

* mkdir

首先分配一个新的inode和数据块，然后初始化这个inode为目录类型，设置好各个时间戳和块指针。接着初始化数据块，写入"."和".."两个目录项。最后在当前目录中添加一个新的目录项，指向新创建的目录。

为了更好的管理变长目录，我定义了构建目录项结构体的函数

```c
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
```
他会将rec_len计算好，并且用memset和memcpy将name拷贝进去，避免脏数据。这两个函数都是基于字节操作的，能够很好地处理变长目录项。

在创建的时候，首先要判断是否已经存在同名文件或目录，如果存在则报错返回。然后分配inode和数据块，初始化inode和数据块，最后在当前目录中插入目录项。

因此定义了find_file函数，这个函数不仅返回inode，还返回他在该目录inode中的逻辑块号，以及它的起始字节和他的前一个目录项的起始字节，方便后续操作。

这样就能用offest加memcpy来精准的在指定位置写入了，实现了变长目录项

### 关于删除

关于删除操作，首先要找到要删除的目录项，获取它的inode号，然后将该目录项从当前目录中删除。接着释放该inode和它所占用的数据块。

我的删除的基本思想是：将要删除的目录项的前一个目录项的rec_len增加要删除目录项的rec_len，从而覆盖掉要删除的目录项，实现逻辑上的删除。

这也是为什么find_file函数要返回前一个目录项的起始字节的原因。

### 关于cd操作

就是将当前目录的inode信息读入内存，然后用offset加memcpy可以实现字节级别的构建结构体，然后输出目录项的基本内容，对于时间信息的打印，

要读入目录项的inode，为了防止污染全局的inode_buf，我实现了load_inode_to_buf函数，将指定inode读入指定缓冲区。

### 关于写操作

有write覆盖写入和append追加写入两种模式。

对于write，直接将所有数据块清空，然后重新分配数据块，写入数据。

对于append，先将文件指针移动到文件末尾，然后从文件末尾开始写入数据，如果当前数据块写满了，则分配新的数据块继续写入。



