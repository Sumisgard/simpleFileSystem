#include "myfs.h"
#include <string.h>
#include <time.h>

#define FS_MAGIC 0x4D594653 // "MYFS"

static bool write_superblock(FILE* fs, const SuperBlock* sb) {
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    return fwrite(sb, sizeof(SuperBlock), 1, fs) == 1;
}

static bool read_superblock(FILE* fs, SuperBlock* sb) {
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    return fread(sb, sizeof(SuperBlock), 1, fs) == 1;
}

bool format_fs(const char* filename) {
    FILE* fs = fopen(filename, "wb+");
    if (!fs) return false;

    // Инициализация суперблока
    SuperBlock sb = {
        .magic = FS_MAGIC,
        .block_size = BLOCK_SIZE,
        .inode_count = INODE_COUNT,
        .block_count = BLOCK_COUNT,
        .free_inodes = INODE_COUNT - 1, // 0 занят корневым каталогом
        .free_blocks = BLOCK_COUNT,
        .inode_bitmap = INODE_BITMAP_OFFSET,
        .block_bitmap = BLOCK_BITMAP_OFFSET,
        .inode_table = INODE_TABLE_OFFSET,
        .data_start = DATA_BLOCKS_OFFSET
    };

    // Записываем суперблок
    if (!write_superblock(fs, &sb)) {
        fclose(fs);
        return false;
    }

    // Инициализация битмапов
    uint8_t inode_bitmap[INODE_COUNT / 8] = {0};
    uint8_t block_bitmap[BLOCK_COUNT / 8] = {0};
    
    // Занимаем inode 0 (корневой каталог)
    inode_bitmap[0] = 0x01;

    // Записываем битмапы
    fseek(fs, INODE_BITMAP_OFFSET, SEEK_SET);
    fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs);
    
    fseek(fs, BLOCK_BITMAP_OFFSET, SEEK_SET);
    fwrite(block_bitmap, sizeof(block_bitmap), 1, fs);

    // Инициализация таблицы inode
    Inode root_inode = {
        .mode = 040755,  // Каталог с правами 755
        .uid = 0,
        .size = 0,
        .mtime = time(NULL)
    };
    
    fseek(fs, INODE_TABLE_OFFSET, SEEK_SET);
    fwrite(&root_inode, sizeof(Inode), 1, fs);

    fclose(fs);
    return true;
}

FILE* open_fs(const char* filename) {
    FILE* fs = fopen(filename, "rb+");
    if (!fs) return NULL;
    
    SuperBlock sb;
    if (!read_superblock(fs, &sb) || sb.magic != FS_MAGIC) {
        fclose(fs);
        return NULL;
    }
    
    return fs;
}

void close_fs(FILE* fs) {
    if (fs) fclose(fs);
}

int create_file(FILE* fs, const char* name) {
    SuperBlock sb;
    
    // Читаем суперблок
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    if (fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return -1;
    }

    // Ищем свободный inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return -1;
    }

    int free_inode = -1;
    for (int i = 0; i < INODE_COUNT; i++) {
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) {
            free_inode = i;
            break;
        }
    }

    if (free_inode == -1) {
        printf("Нет свободных inode\n");
        return -1;
    }

    // Помечаем inode как занятый
    inode_bitmap[free_inode / 8] |= (1 << (free_inode % 8));
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа inode");
        return -1;
    }

    // Создаем новый inode
   Inode new_inode = {
        .mode = 0100644,
        .uid = 0,
        .size = 0,
        .mtime = time(NULL),
        .blocks = {0}
    };
    
    // Копируем имя файла (не более 255 символов + '\0')
    strncpy(new_inode.name, name, sizeof(new_inode.name) - 1);
    new_inode.name[sizeof(new_inode.name) - 1] = '\0'; // Гарантируем завершающий ноль

    // Записываем inode в таблицу
    fseek(fs, sb.inode_table + free_inode * sizeof(Inode), SEEK_SET);
    if (fwrite(&new_inode, sizeof(Inode), 1, fs) != 1) {
        perror("Ошибка записи inode");
        return -1;
    }

    // Обновляем суперблок
    sb.free_inodes--;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    if (fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка обновления суперблока");
        return -1;
    }

    printf("Создан файл с inode %d\n", free_inode);
    return free_inode;
}


bool delete_file(FILE* fs, const char* name) {
    // В текущей реализации удаляем по номеру inode
    // В будущем нужно добавить поиск по имени через каталоги
    
    int inode_num;
    printf("Введите номер inode для удаления: ");
    if (scanf("%d", &inode_num) != 1 || inode_num <= 0 || inode_num >= INODE_COUNT) {
        printf("Неверный номер inode\n");
        return false;
    }
    getchar(); // Сброс \n

    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    if (fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return false;
    }

    // Проверяем существование inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return false;
    }

    if (!(inode_bitmap[inode_num / 8] & (1 << (inode_num % 8)))) {
        printf("Inode %d не существует\n", inode_num);
        return false;
    }

    // Читаем inode для освобождения блоков
    Inode node;
    fseek(fs, sb.inode_table + inode_num * sizeof(Inode), SEEK_SET);
    if (fread(&node, sizeof(Inode), 1, fs) != 1) {
        perror("Ошибка чтения inode");
        return false;
    }

    // Освобождаем блоки данных
    uint8_t block_bitmap[BLOCK_COUNT / 8];
    fseek(fs, sb.block_bitmap, SEEK_SET);
    if (fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа блоков");
        return false;
    }

    for (int i = 0; i < 12; i++) {
        if (node.blocks[i] != 0) {
            block_bitmap[node.blocks[i] / 8] &= ~(1 << (node.blocks[i] % 8));
            sb.free_blocks++;
        }
    }

    // Обновляем битмап блоков
    fseek(fs, sb.block_bitmap, SEEK_SET);
    if (fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа блоков");
        return false;
    }

    // Освобождаем inode
    inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа inode");
        return false;
    }

    // Обновляем суперблок
    sb.free_inodes++;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    if (fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка обновления суперблока");
        return false;
    }

    printf("Файл (inode %d) успешно удален\n", inode_num);
    return true;
}

void list_files(FILE* fs) {
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    if (fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return;
    }

    // Читаем битмап inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return;
    }

    printf("\nСписок файлов:\n");
    printf("INODE\tNAME\t\tMODE\tSIZE\tBLOCKS\n");  // Добавляем заголовок NAME
    
    for (int i = 1; i < INODE_COUNT; i++) {
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
            Inode node;
            fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET);
            if (fread(&node, sizeof(Inode), 1, fs) != 1) {
                printf("Ошибка чтения inode %d\n", i);
                continue;
            }

            int used_blocks = 0;
            for (int j = 0; j < 12; j++) {
                if (node.blocks[j] != 0) used_blocks++;
            }

            // Выводим имя файла (обрезаем до 15 символов для компактности)
            char short_name[16];
            strncpy(short_name, node.name, 15);
            short_name[15] = '\0';
            
            printf("%d\t%-15s\t%o\t%u\t%d\n", 
                  i, short_name, node.mode, node.size, used_blocks);
        }
    }
}


int write_file(FILE* fs, const char* filename, const char* data) {
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fs);

    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);

    // Ищем inode по имени
    Inode node;
    int found_inode = -1;
    for (int i = 1; i < INODE_COUNT; i++) {
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
            fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET);
            fread(&node, sizeof(Inode), 1, fs);

            if (strncmp(node.name, filename, sizeof(node.name)) == 0) {
                found_inode = i;
                break;
            }
        }
    }

    if (found_inode == -1) {
        printf("Файл '%s' не найден\n", filename);
        return 0;
    }

    // Подготовка к записи
    size_t data_len = strlen(data);
    size_t required_blocks = (data_len + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (required_blocks > 12) {
        printf("Файл слишком большой (макс 12 блоков)\n");
        return 0;
    }

    // Читаем битмап блоков
    uint8_t block_bitmap[BLOCK_COUNT / 8];
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fread(block_bitmap, sizeof(block_bitmap), 1, fs);

    // Выделяем блоки
    int blocks_allocated = 0;
    for (int i = 0; i < BLOCK_COUNT && blocks_allocated < required_blocks; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) {
            block_bitmap[i / 8] |= (1 << (i % 8));
            node.blocks[blocks_allocated] = i;
            blocks_allocated++;
        }
    }

    if (blocks_allocated < required_blocks) {
        printf("Недостаточно свободных блоков\n");
        return 0;
    }

    // Запись данных по блокам
    for (int i = 0; i < blocks_allocated; i++) {
        size_t offset = i * BLOCK_SIZE;
        size_t to_write = BLOCK_SIZE;
        if (offset + to_write > data_len)
            to_write = data_len - offset;

        fseek(fs, sb.data_start + node.blocks[i] * BLOCK_SIZE, SEEK_SET);
        fwrite(data + offset, 1, to_write, fs);
    }

    // Обновляем inode
    node.size = (uint32_t)data_len;
    node.mtime = time(NULL);
    fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET);
    fwrite(&node, sizeof(Inode), 1, fs);

    // Обновляем битмап и суперблок
    sb.free_blocks -= blocks_allocated;
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fwrite(block_bitmap, sizeof(block_bitmap), 1, fs);

    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fwrite(&sb, sizeof(SuperBlock), 1, fs);

    return 1;
}


int read_file(FILE* fs, const char* filename, char* buffer, size_t max_size) {
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fs);

    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);

    // Ищем inode по имени
    Inode node;
    int found_inode = -1;
    for (int i = 1; i < INODE_COUNT; i++) {
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
            fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET);
            fread(&node, sizeof(Inode), 1, fs);

            if (strncmp(node.name, filename, sizeof(node.name)) == 0) {
                found_inode = i;
                break;
            }
        }
    }

    if (found_inode == -1) {
        printf("Файл '%s' не найден\n", filename);
        return 0;
    }

    // Читаем данные из блоков
    size_t to_read = (node.size < max_size - 1) ? node.size : max_size - 1;
    size_t bytes_read = 0;

    for (int i = 0; i < 12 && bytes_read < to_read; i++) {
        if (node.blocks[i] == 0) break;

        fseek(fs, sb.data_start + node.blocks[i] * BLOCK_SIZE, SEEK_SET);
        size_t chunk = BLOCK_SIZE;
        if (bytes_read + chunk > to_read)
            chunk = to_read - bytes_read;

        fread(buffer + bytes_read, 1, chunk, fs);
        bytes_read += chunk;
    }

    buffer[bytes_read] = '\0';
    return (int)bytes_read;
}


int write_file1(FILE* fs, const char* filename, const char* data) {
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fs);

    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);

    // Ищем inode по имени
    Inode node;
    int found_inode = -1;
    for (int i = 1; i < INODE_COUNT; i++) {
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
            fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET);
            fread(&node, sizeof(Inode), 1, fs);
            if (strncmp(node.name, filename, sizeof(node.name)) == 0) {
                found_inode = i;
                break;
            }
        }
    }

    if (found_inode == -1) {
        printf("Файл '%s' не найден\n", filename);
        return 0;
    }

    // Подготовка
    size_t data_len = strlen(data);
    size_t current_size = node.size;
    size_t total_size = current_size + data_len;

    size_t current_blocks = (current_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t required_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (required_blocks > 12) {
        printf("Файл слишком большой (макс 12 блоков)\n");
        return 0;
    }

    uint8_t block_bitmap[BLOCK_COUNT / 8];
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fread(block_bitmap, sizeof(block_bitmap), 1, fs);

    // Выделяем недостающие блоки
    for (int i = current_blocks; i < required_blocks; i++) {
        for (int j = 0; j < BLOCK_COUNT; j++) {
            if (!(block_bitmap[j / 8] & (1 << (j % 8)))) {
                block_bitmap[j / 8] |= (1 << (j % 8));
                node.blocks[i] = j;
                sb.free_blocks--;
                break;
            }
        }

        if (node.blocks[i] == 0) {
            printf("Недостаточно свободных блоков\n");
            return 0;
        }
    }

    // Запись данных в конец файла
    size_t remaining = data_len;
    size_t data_offset = 0;
    size_t file_offset = current_size;

    while (remaining > 0) {
        size_t block_index = file_offset / BLOCK_SIZE;
        size_t offset_in_block = file_offset % BLOCK_SIZE;
        size_t space_in_block = BLOCK_SIZE - offset_in_block;
        size_t to_write = (remaining < space_in_block) ? remaining : space_in_block;

        long write_position = sb.data_start + node.blocks[block_index] * BLOCK_SIZE + offset_in_block;
        fseek(fs, write_position, SEEK_SET);
        fwrite(data + data_offset, 1, to_write, fs);

        data_offset += to_write;
        file_offset += to_write;
        remaining -= to_write;
    }

    // Обновляем inode
    node.size = (uint32_t)total_size;
    node.mtime = time(NULL);
    fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET);
    fwrite(&node, sizeof(Inode), 1, fs);

    // Обновляем битмап блоков и суперблок
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fwrite(block_bitmap, sizeof(block_bitmap), 1, fs);

    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fwrite(&sb, sizeof(SuperBlock), 1, fs);

    return 1;
}


