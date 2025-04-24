#ifndef MYFS_H
#define MYFS_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// Основные параметры ФС
#define BLOCK_SIZE 4096         // Размер одного блока (в байтах)
#define INODE_COUNT 1024        // Общее количество inode
#define BLOCK_COUNT 4096        // Общее количество блоков данных

// Смещения в образе ФС (в байтах)
#define SUPERBLOCK_OFFSET 0             // Суперблок
#define INODE_BITMAP_OFFSET 4096        // Битовая карта inodes
#define BLOCK_BITMAP_OFFSET 8192        // Битовая карта блоков данных
#define INODE_TABLE_OFFSET 12288        // Таблица inodes
#define DATA_BLOCKS_OFFSET 53248        // Начало области с данными

// Структура суперблока (главная инфа о ФС)
typedef struct {
    uint32_t magic;          // Сигнатура ФС ("MYFS")
    uint32_t block_size;     // Размер блока
    uint32_t inode_count;    // Общее количество inodes
    uint32_t block_count;    // Общее количество блоков
    uint32_t free_inodes;    // Сколько inodes свободны
    uint32_t free_blocks;    // Сколько блоков свободны
    uint32_t inode_bitmap;   // Смещение битмапа inode
    uint32_t block_bitmap;   // Смещение битмапа блоков
    uint32_t inode_table;    // Смещение таблицы inodes
    uint32_t data_start;     // Смещение начала данных
} SuperBlock;

// Структура inode — информация о каждом файле
typedef struct {
    uint16_t mode;           // Права доступа и тип (например, обычный файл)
    uint16_t uid;            // ID пользователя (владелец)
    uint32_t size;           // Размер файла
    uint32_t mtime;          // Время последней модификации
    uint32_t blocks[12];     // Указатели на блоки с данными
    char name[256];          // Имя файла
} Inode;

// Объявления функций для управления ФС
bool format_fs(const char* filename);                         // Инициализация ФС
FILE* open_fs(const char* filename);                          // Открытие ФС
void close_fs(FILE* fs);                                      // Закрытие ФС

int create_file(FILE* fs, const char* name);                  // Создание файла
bool delete_file(FILE* fs, const char* name);                 // Удаление файла
void list_files(FILE* fs);                                    // Показ списка файлов

int write_file(FILE* fs, const char* filename, const char*);  // Перезапись файла
int write_file1(FILE* fs, const char* filename, const char*); // Дозапись в файл
int read_file(FILE* fs, const char* filename, char* buffer, size_t max_size); // Чтение файла

#endif