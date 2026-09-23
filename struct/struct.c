#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SECTOR_SIZE = 512,  // 磁盘扇区大小：512 字节
    RESERVED_SIZE = 494 // 保留区大小：494 字节 18B给头部
};
typedef struct __attribute__((packed)) FileHeader {
    uint32_t magic;   // 魔数，标识文件类型 4B
    uint16_t version;   // 文件格式版本号 2B
    uint64_t file_size; // 文件总大小 8B
    uint32_t align_offset;// 对齐偏移量 4B
    char reserved[RESERVED_SIZE];// 保留区（预留空间）494B 18B给头部
} FileHeader; 

//编译时检查FileHeader大小是否为512字节,不是直接编译失败
_Static_assert(sizeof(FileHeader) == SECTOR_SIZE, "FileHeader must be 512 bytes");
//打印结构体内存布局
//offsetof(结构体名， 字段名) + <stddef.h>
//返回结构体中字段距离开头的偏移量
static void print_layout(void) {
    printf("FileHeader layout\n");
    //%zu专门打印sizeof和offsetof返回的无符号整数
    printf("  sizeof(FileHeader): %zu bytes\n", sizeof(FileHeader)); //512
    printf("  offsetof(magic):        %zu (0x%02zx)\n", offsetof(FileHeader, magic), offsetof(FileHeader, magic));
    printf("  offsetof(version):      %zu (0x%02zx)\n", offsetof(FileHeader, version), offsetof(FileHeader, version));
    printf("  offsetof(file_size):    %zu (0x%02zx)\n", offsetof(FileHeader, file_size), offsetof(FileHeader, file_size));
    printf("  offsetof(align_offset): %zu (0x%02zx)\n", offsetof(FileHeader, align_offset), offsetof(FileHeader, align_offset));
    printf("  offsetof(reserved):     %zu (0x%02zx)\n", offsetof(FileHeader, reserved), offsetof(FileHeader, reserved));
}

static int write_header(const char *path, const FileHeader *header) {
    //stdio.h
    FILE *fp = fopen(path, "wb");
    size_t written;
    //strerror（把错误码转成文字)
    if (fp == NULL) {
        fprintf(stderr, "fopen('%s') failed: %s\n", path, strerror(errno));
        return 1;
    }
    //返回成功写入的元素个数
    //sizeof(header) 是指针大小（8 字节），不是结构体大小！
    written = fwrite(header, sizeof(*header),1,fp);
    if (written != 1) {
        fprintf(stderr, "fwrite('%s') failed: wrote %zu block(s)\n", path, written);
        fclose(fp);
        return 1;
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "fclose('%s') failed: %s\n", path, strerror(errno));
        return 1;
    }

    return 0;
}

int main(void) {
    const char *output_path = "header.bin";
    FileHeader header;
    //void *memset(要清空的地址, 设置的值, 要清空的字节数);
    memset(&header, 0, sizeof(header));
    header.magic = UINT32_C(0x44484D47);
    header.version = UINT16_C(0x0102);
    header.file_size = UINT64_C(0x1122334455667788);
    header.align_offset = UINT32_C(0xAABBCCDD);

    print_layout();
    printf("\nHeader values\n");
    //PRIX32/PRIX63跨平台打印固定大小整数
    printf("  magic:        0x%08" PRIX32 "\n", header.magic);
    printf("  version:      0x%04" PRIX16 "\n", header.version);
    printf("  file_size:    0x%016" PRIX64 "\n", header.file_size);
    printf("  align_offset: 0x%08" PRIX32 "\n", header.align_offset);

    if (write_header(output_path, &header) != 0) {
        return EXIT_FAILURE;
    }

    printf("\nWrote %zu bytes to %s\n", sizeof(header), output_path);
    printf("Inspect with: hexdump -C %s\n", output_path);
    printf("Expected field offsets: magic=0x00, version=0x04, file_size=0x06, align_offset=0x0e, reserved=0x12\n");

    return EXIT_SUCCESS;
}
