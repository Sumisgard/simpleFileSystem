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
