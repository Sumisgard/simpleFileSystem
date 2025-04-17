#ifndef MYFS_H
#define MYFS_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define BLOCK_SIZE 4096
#define INODE_COUNT 1024
#define BLOCK_COUNT 4096

// Расположение структур в образе
#define SUPERBLOCK_OFFSET 0
#define INODE_BITMAP_OFFSET 4096
#define BLOCK_BITMAP_OFFSET 8192
#define INODE_TABLE_OFFSET 12288
#define DATA_BLOCKS_OFFSET 53248

typedef struct {
    uint32_t magic;          // Сигнатура ФС
    uint32_t block_size;     // Размер блока
    uint32_t inode_count;    // Всего inode
    uint32_t block_count;    // Всего блоков
    uint32_t free_inodes;    // Свободных inode
    uint32_t free_blocks;    // Свободных блоков
    uint32_t inode_bitmap;   // Смещение битмапа inode
    uint32_t block_bitmap;   // Смещение битмапа блоков
    uint32_t inode_table;    // Смещение таблицы inode
    uint32_t data_start;     // Начало данных
} SuperBlock;

typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t mtime;
    uint32_t blocks[12];
    char name[256];  // Добавляем поле для имени файла
} Inode;


// Функции ФС
bool format_fs(const char* filename);
FILE* open_fs(const char* filename);
void close_fs(FILE* fs);

int create_file(FILE* fs, const char* name);
bool delete_file(FILE* fs, const char* name);
void list_files(FILE* fs);

int write_file(FILE* fs, const char* filename, const char* data);
int write_file1(FILE* fs, const char* filename, const char* data);

int read_file(FILE* fs, const char* filename, char* buffer, size_t max_size);


#endif
