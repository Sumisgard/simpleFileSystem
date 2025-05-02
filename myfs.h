#ifndef MYFS_H
#define MYFS_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

// -----------------------------
// Основные параметры файловой системы
// -----------------------------

#define BLOCK_SIZE 4096         // Размер одного блока данных (в байтах)
#define INODE_COUNT 1024        // Общее количество inode (табличных записей для файлов)
#define BLOCK_COUNT 4096        // Общее количество блоков данных

// -----------------------------
// Смещения системных структур в файле-образе
// -----------------------------

#define SUPERBLOCK_OFFSET 0             // Смещение суперблока (начало файла)
#define INODE_BITMAP_OFFSET 4096        // Смещение битовой карты inodes
#define BLOCK_BITMAP_OFFSET 8192        // Смещение битовой карты блоков данных
#define INODE_TABLE_OFFSET 12288        // Смещение таблицы inode
#define DATA_BLOCKS_OFFSET 53248        // Смещение начала области данных (где хранятся содержимое файлов)

// -----------------------------
// Структура суперблока файловой системы
// -----------------------------

typedef struct {
    uint32_t magic;          // Уникальная сигнатура файловой системы для проверки корректности ("MYFS")
    uint32_t block_size;     // Размер одного блока
    uint32_t inode_count;    // Общее количество inode в системе
    uint32_t block_count;    // Общее количество блоков
    uint32_t free_inodes;    // Количество свободных inode
    uint32_t free_blocks;    // Количество свободных блоков
    uint32_t inode_bitmap;   // Смещение битовой карты inodes от начала файла
    uint32_t block_bitmap;   // Смещение битовой карты блоков данных от начала файла
    uint32_t inode_table;    // Смещение таблицы inode от начала файла
    uint32_t data_start;     // Смещение начала области хранения данных
} SuperBlock;

// -----------------------------
// Структура inode (информация о каждом файле)
// -----------------------------

typedef struct {
    uint32_t size;           // Размер файла (в байтах)
    time_t mtime;          // Время последнего изменения файла (Unix-время)
    uint32_t blocks[12];     // Массив номеров блоков, в которых хранятся данные файла (прямая адресация)
    char name[256];          // Имя файла 
} Inode;

// -----------------------------
// Объявления основных функций работы с ФС
// -----------------------------

bool format_fs(const char* filename);                           // Форматирует (инициализирует) файловую систему
FILE* open_fs(const char* filename);                           // Открывает файл с образом файловой системы
void close_fs(FILE* fs);                                       // Закрывает файловую систему

int create_file(FILE* fs, const char* name);                   // Создаёт новый файл
bool delete_file(FILE* fs, const char* name);                  // Удаляет файл
void list_files(FILE* fs);                                     // Выводит список файлов

int write_file(FILE* fs, const char* filename, const char* data);     // Записывает данные в файл (реализация 1)
int write_file1(FILE* fs, const char* filename, const char* data);    // Альтернативная реализация записи

int read_file(FILE* fs, const char* filename, char* buffer, size_t max_size);  // Считывает содержимое файла

#endif

