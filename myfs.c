#include "myfs.h"
#include <string.h>
#include <time.h>

#define FS_MAGIC 0x4D594653 // "MYFS"

// Записывает суперблок в файл ФС
// Возвращает true при успехе, false — при ошибке
static bool write_superblock(FILE* fs, const SuperBlock* sb) {
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0) {
        perror("Ошибка fseek при записи суперблока");
        return false;
    }
    if (fwrite(sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка fwrite при записи суперблока");
        return false;
    }
    return true;
}

// Читает суперблок из файла ФС
// Возвращает true при успехе, false — при ошибке
static bool read_superblock(FILE* fs, SuperBlock* sb) {
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0) {
        perror("Ошибка fseek при чтении суперблока");
        return false;
    }
    if (fread(sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка fread при чтении суперблока");
        return false;
    }
    return true;
}

// Функция форматирует и инициализирует файловую систему
// Используется при первом запуске программы, когда ФС ещё не существует
bool format_fs(const char* filename) {
    // Открываем файл, представляющий ФС, на запись с обнулением содержимого
    FILE* fs = fopen(filename, "wb+");
    if (!fs) {
        perror("Не удалось открыть файл для форматирования");
        return false;
    }

    // Создаём суперблок — основную структуру метаданных ФС
    SuperBlock sb = {
        .magic = FS_MAGIC,                // Магическое число для проверки валидности ФС
        .block_size = BLOCK_SIZE,        // Размер одного блока
        .inode_count = INODE_COUNT,      // Общее количество inode
        .block_count = BLOCK_COUNT,      // Общее количество блоков данных
        .free_inodes = INODE_COUNT - 1,  // Свободные inode (один занят под корень)
        .free_blocks = BLOCK_COUNT,      // Пока все блоки свободны
        .inode_bitmap = INODE_BITMAP_OFFSET,  // Смещение битмапа inode
        .block_bitmap = BLOCK_BITMAP_OFFSET,  // Смещение битмапа блоков
        .inode_table = INODE_TABLE_OFFSET,    // Смещение таблицы inode
        .data_start = DATA_BLOCKS_OFFSET      // Начало области с данными
    };

    // Записываем суперблок в файл
    if (!write_superblock(fs, &sb)) {
        perror("Ошибка записи суперблока");
        fclose(fs);
        return false;
    }

    // Инициализируем битмапы inode и блоков: всё свободно, кроме inode 0
    uint8_t inode_bitmap[INODE_COUNT / 8] = {0};
    uint8_t block_bitmap[BLOCK_COUNT / 8] = {0};

    inode_bitmap[0 / 8] |= (1 << (0 % 8)); // Устанавливаем бит для корневого inode

    // Записываем битмап inode
    fseek(fs, INODE_BITMAP_OFFSET, SEEK_SET);
    if (fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа inode");
        fclose(fs);
        return false;
    }

    // Записываем битмап блоков
    fseek(fs, BLOCK_BITMAP_OFFSET, SEEK_SET);
    if (fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа блоков");
        fclose(fs);
        return false;
    }

    // Создаём root inode (каталог, inode 0)
    Inode root_inode = {
        .mode = 040755,     // Тип — каталог, права 755 (rwxr-xr-x)
        .uid = 0,           // Пользователь root
        .size = 0,          // Пустой каталог (если не используется структура директорий)
        .mtime = time(NULL) // Время создания
    };

    Inode empty_inode = {0};

    // Записываем таблицу inode: корневой + пустые
    fseek(fs, INODE_TABLE_OFFSET, SEEK_SET);
    for (int i = 0; i < INODE_COUNT; ++i) {
        if (fwrite((i == 0) ? &root_inode : &empty_inode, sizeof(Inode), 1, fs) != 1) {
            perror("Ошибка записи inode таблицы");
            fclose(fs);
            return false;
        }
    }

    // Инициализируем область данных нулями
    uint8_t zero_block[BLOCK_SIZE] = {0};
    fseek(fs, DATA_BLOCKS_OFFSET, SEEK_SET);
    for (int i = 0; i < BLOCK_COUNT; ++i) {
        if (fwrite(zero_block, BLOCK_SIZE, 1, fs) != 1) {
            perror("Ошибка обнуления блоков данных");
            fclose(fs);
            return false;
        }
    }

    fclose(fs);
    return true;
}

/**
 * Открывает существующую файловую систему
 * @param filename Имя файла-образа ФС
 * @return Указатель на FILE или NULL при ошибке
 */
FILE* open_fs(const char* filename) {
    // 1. Открываем файл в режиме чтения+записи (бинарный)
    FILE* fs = fopen(filename, "rb+");
    if (!fs) {
        perror("Ошибка открытия файла ФС");
        return NULL;
    }

    // 2. Читаем суперблок для проверки
    SuperBlock sb;
    if (!read_superblock(fs, &sb)) {
        fprintf(stderr, "Ошибка: не удалось прочитать суперблок\n");
        fclose(fs);
        return NULL;
    }

    // 3. Проверяем "магическое число" (подпись ФС)
    if (sb.magic != FS_MAGIC) {
        fprintf(stderr, "Ошибка: это не файл MYFS (магическое число 0x%X)\n", sb.magic);
        fclose(fs);
        return NULL;
    }

    // 4. Проверяем совместимость параметров
    if (sb.block_size != BLOCK_SIZE) {
        fprintf(stderr, "Ошибка: несовместимый размер блока (%u != %u)\n", 
                sb.block_size, BLOCK_SIZE);
        fclose(fs);
        return NULL;
    }

    return fs;
}


/**
 * Закрывает файловую систему
 * @param fs Указатель на открытую ФС
 */
void close_fs(FILE* fs) {
    if (!fs) return;

    // 1. Сбрасываем буферы на диск
    if (fflush(fs) != 0) {
        perror("Предупреждение: ошибка сброса буферов");
    }

    // 2. Закрываем файл
    if (fclose(fs) != 0) {
        perror("Предупреждение: ошибка при закрытии файла");
    }
}

/**
 * Создаёт новый файл в ФС
 * @param fs     Указатель на открытую ФС
 * @param name   Имя файла (макс 255 символов)
 * @return       Номер inode или -1 при ошибке
 */
int create_file(FILE* fs, const char* name) {
    // Проверка длины имени
    if (strlen(name) >= 256) {
        fprintf(stderr, "Ошибка: имя файла слишком длинное\n");
        return -1;
    }

    SuperBlock sb;
    // Чтение суперблока с проверкой ошибок
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return -1;
    }

    // Проверка свободных inodes
    if (sb.free_inodes == 0) {
        fprintf(stderr, "Ошибка: нет свободных inodes\n");
        return -1;
    }

    // Чтение битмапа inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return -1;
    }

    /* Поиск существующего файла и свободного inode */
    int free_inode = -1;
    Inode temp;

    for (int i = 1; i < INODE_COUNT; ++i) { // Пропускаем inode 0
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
            // Чтение существующего inode для проверки имени
            if (fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET) != 0 ||
                fread(&temp, sizeof(Inode), 1, fs) != 1) {
                continue;
            }

            if (strncmp(temp.name, name, sizeof(temp.name)) == 0) {
                printf("Ошибка: файл '%s' уже существует (inode %d)\n", name, i);
                return -1;
            }
        } else if (free_inode == -1) {
            free_inode = i; // Запоминаем первый свободный
        }
    }

    if (free_inode == -1) {
        fprintf(stderr, "Ошибка: нет свободных inodes (несмотря на sb.free_inodes)\n");
        return -1;
    }

    /* Создание нового файла */
    Inode new_inode = {
        .mode = 0100644,     // -rw-r--r--
        .uid = 0,
        .size = 0,
        .mtime = time(NULL),
        .blocks = {0}        // Пока нет блоков данных
    };
    strncpy(new_inode.name, name, sizeof(new_inode.name) - 1);
    new_inode.name[sizeof(new_inode.name) - 1] = '\0';

    // Запись inode
    if (fseek(fs, sb.inode_table + free_inode * sizeof(Inode), SEEK_SET) != 0 ||
        fwrite(&new_inode, sizeof(Inode), 1, fs) != 1) {
        perror("Ошибка записи inode");
        return -1;
    }

    // Обновление битмапа
    inode_bitmap[free_inode / 8] |= (1 << (free_inode % 8));
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка обновления битмапа inode");
        return -1;
    }

    // Обновление суперблока
    sb.free_inodes--;
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка обновления суперблока");
        return -1;
    }

    printf("Создан файл '%s' (inode %d)\n", name, free_inode);
    return free_inode;
}
bool delete_file(FILE* fs, const char* name) {
    SuperBlock sb;

    // Читаем суперблок
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return false;
    }

    // Загружаем битмапы
    uint8_t inode_bitmap[INODE_COUNT / 8];
    uint8_t block_bitmap[BLOCK_COUNT / 8];

    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return false;
    }

    fseek(fs, sb.block_bitmap, SEEK_SET);
    if (fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа блоков");
        return false;
    }

    // Ищем файл по имени
    int inode_num = -1;
    Inode inode;
    for (int i = 0; i < INODE_COUNT; ++i) {
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) continue;

        fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET);
        if (fread(&inode, sizeof(Inode), 1, fs) != 1) {
            perror("Ошибка чтения inode");
            return false;
        }

        if (strcmp(inode.name, name) == 0) {
            inode_num = i;
            break;
        }
    }

    if (inode_num == -1) {
        printf("Файл '%s' не найден\n", name);
        return false;
    }

    // Освобождаем занятые блоки
    for (int i = 0; i < 12; ++i) {
        if (inode.blocks[i] != 0) {
            block_bitmap[inode.blocks[i] / 8] &= ~(1 << (inode.blocks[i] % 8));
            sb.free_blocks++;
        }
    }

    // Стираем inode
    inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));

    // Обновляем битмапы и суперблок
    fseek(fs, sb.block_bitmap, SEEK_SET);
    if (fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа блоков");
        return false;
    }

    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа inode");
        return false;
    }

    sb.free_inodes++;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    if (fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка обновления суперблока");
        return false;
    }

    printf("Файл '%s' (inode %d) успешно удален\n", name, inode_num);
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

    printf("\n%-6s %-15s %-10s %-8s %-20s %-6s\n", 
       "INODE", "NAME", "TYPE", "SIZE", "MTIME", "BLOCKS");


    for (int i = 1; i < INODE_COUNT; i++) {
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
            Inode node;
            fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET);
            if (fread(&node, sizeof(Inode), 1, fs) != 1) {
                printf("Ошибка чтения inode %d\n", i);
                continue;
            }

            // Подсчёт используемых блоков
            int used_blocks = 0;
            for (int j = 0; j < 12; j++) {
                if (node.blocks[j] != 0) used_blocks++;
            }

            // Преобразуем mode в строку прав доступа
            char permissions[11] = "----------";
            if ((node.mode & 0100000) == 0100000) permissions[0] = '-';  // обычный файл
            if (node.mode & 0400) permissions[1] = 'r';
            if (node.mode & 0200) permissions[2] = 'w';
            if (node.mode & 0100) permissions[3] = 'x';
            if (node.mode & 0040) permissions[4] = 'r';
            if (node.mode & 0020) permissions[5] = 'w';
            if (node.mode & 0010) permissions[6] = 'x';
            if (node.mode & 0004) permissions[7] = 'r';
            if (node.mode & 0002) permissions[8] = 'w';
            if (node.mode & 0001) permissions[9] = 'x';

            // Поскольку у нас все файлы — текстовые, тип фиксирован
            const char* type = "text";

            // Обрезаем имя до 15 символов
            char short_name[16];
            strncpy(short_name, node.name, 15);
            short_name[15] = '\0';

char timebuf[20];
struct tm* tm_info = localtime((time_t*)&node.mtime);
strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm_info);

            // Вывод строки файла
            printf("%-6d %-15s %-10s %-8u %-20s %-6d\n",
       i, short_name, type, node.size, timebuf, used_blocks);

        }
    }
}

/**
 * Записывает данные в файл файловой системы
 * @param fs       Указатель на открытую ФС
 * @param filename Имя файла
 * @param data     Данные для записи
 * @return         1 при успехе, 0 при ошибке
 */
int write_file(FILE* fs, const char* filename, const char* data) {
    // Проверка входных параметров
    if (!fs || !filename || !data) {
        fprintf(stderr, "Ошибка: неверные параметры\n");
        return 0;
    }

    SuperBlock sb;
    // Чтение суперблока с проверкой ошибок
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return 0;
    }

    // Чтение битмапа inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return 0;
    }

    /* Поиск файла по имени */
    Inode node;
    int found_inode = -1;
    
    for (int i = 1; i < INODE_COUNT; i++) { // Пропускаем inode 0 (корневой каталог)
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) continue;

        // Чтение inode с проверкой ошибок
        if (fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET) != 0 ||
            fread(&node, sizeof(Inode), 1, fs) != 1) {
            perror("Ошибка чтения inode");
            continue;
        }

        if (strncmp(node.name, filename, sizeof(node.name)) == 0) {
            found_inode = i;
            break;
        }
    }

    if (found_inode == -1) {
        fprintf(stderr, "Файл '%s' не найден\n", filename);
        return 0;
    }

    /* Освобождение старых блоков */
    uint8_t block_bitmap[BLOCK_COUNT / 8];
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа блоков");
        return 0;
    }

    unsigned freed_blocks = 0;
    for (int i = 0; i < 12; i++) {
        if (node.blocks[i] != 0) {
            uint32_t block_num = node.blocks[i];
            // Проверка валидности номера блока
            if (block_num >= BLOCK_COUNT) {
                fprintf(stderr, "Предупреждение: некорректный номер блока %u\n", block_num);
                continue;
            }
            block_bitmap[block_num / 8] &= ~(1 << (block_num % 8));
            node.blocks[i] = 0;
            freed_blocks++;
            sb.free_blocks++;
        }
    }

    /* Выделение новых блоков */
    size_t data_len = strlen(data);
    size_t required_blocks = (data_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    if (required_blocks > 12) {
        fprintf(stderr, "Ошибка: файл слишком большой (требуется %zu блоков, макс 12)\n", 
                required_blocks);
        return 0;
    }

    // Поиск свободных блоков
    int blocks_allocated = 0;
    for (int i = 0; i < BLOCK_COUNT && blocks_allocated < required_blocks; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) {
            block_bitmap[i / 8] |= (1 << (i % 8));
            node.blocks[blocks_allocated++] = i;
            sb.free_blocks--;
        }
    }

    if (blocks_allocated < required_blocks) {
        fprintf(stderr, "Ошибка: недостаточно свободных блоков (найдено %d из %zu)\n",
                blocks_allocated, required_blocks);
        // Возвращаем выделенные блоки
        for (int i = 0; i < blocks_allocated; i++) {
            block_bitmap[node.blocks[i] / 8] &= ~(1 << (node.blocks[i] % 8));
            sb.free_blocks++;
        }
        return 0;
    }

    /* Запись данных */
    for (int i = 0; i < blocks_allocated; i++) {
        size_t offset = i * BLOCK_SIZE;
        size_t to_write = (offset + BLOCK_SIZE > data_len) ? 
                          data_len - offset : BLOCK_SIZE;


        if (fseek(fs, sb.data_start + node.blocks[i] * BLOCK_SIZE, SEEK_SET) != 0 ||
            fwrite(data + offset, 1, to_write, fs) != to_write) {
            perror("Ошибка записи данных");
            // Откатываем изменения
            for (int j = 0; j < blocks_allocated; j++) {
                block_bitmap[node.blocks[j] / 8] &= ~(1 << (node.blocks[j] % 8));
                sb.free_blocks++;
            }
            return 0;
        }
    }

    /* Обновление метаданных */
    node.size = (uint32_t)data_len;
    node.mtime = time(NULL);

    // Запись обновленного inode
    if (fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET) != 0 ||
        fwrite(&node, sizeof(Inode), 1, fs) != 1 ||
        fflush(fs) != 0) {
        perror("Ошибка записи inode");
        return 0;
    }

    // Обновление битмапа блоков
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1 ||
        fflush(fs) != 0) {
        perror("Ошибка записи битмапа блоков");
        return 0;
    }

    // Обновление суперблока
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1 ||
        fflush(fs) != 0) {
        perror("Ошибка обновления суперблока");
        return 0;
    }

    printf("Данные записаны в файл '%s' (inode %d). Использовано %d блоков.\n",
           filename, found_inode, blocks_allocated);
    return 1;
}

int read_file(FILE* fs, const char* filename, char* buffer, size_t max_size) {
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fs);

    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);

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

    // Учитываем реальный размер данных и ограничение буфера
    size_t to_read = (node.size < max_size - 1) ? node.size : max_size - 1;
    size_t bytes_read = 0;

    for (int i = 0; i < 12 && bytes_read < to_read; i++) {
        if (node.blocks[i] == 0) break;

        size_t offset = sb.data_start + node.blocks[i] * BLOCK_SIZE;
        fseek(fs, offset, SEEK_SET);

        size_t remaining = to_read - bytes_read;
        size_t chunk = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

        fread(buffer + bytes_read, 1, chunk, fs);
        bytes_read += chunk;
    }

    buffer[bytes_read] = '\0';

    // Опционально можно обрезать невидимые символы в конце, если они есть:
    // while (bytes_read > 0 && (buffer[bytes_read-1] == '\0' || buffer[bytes_read-1] == '\r' || buffer[bytes_read-1] == '\n'))
    //     buffer[--bytes_read] = '\0';

    return (int)bytes_read;
}
int write_file1(FILE* fs, const char* filename, const char* data) {
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fs);

    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);

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

    // Добавим перевод строки если файл не пустой
    const char* newline = "\n";
    size_t prefix_len = (node.size > 0) ? strlen(newline) : 0;
    size_t data_len = strlen(data);
    size_t total_data_len = prefix_len + data_len;

    size_t current_size = node.size;
    size_t total_size = current_size + total_data_len;

    size_t current_blocks = (current_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t required_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (required_blocks > 12) {
        printf("Файл слишком большой (макс 12 блоков)\n");
        return 0;
    }

    uint8_t block_bitmap[BLOCK_COUNT / 8];
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fread(block_bitmap, sizeof(block_bitmap), 1, fs);

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

    // Записываем перевод строки если нужно
    size_t remaining = total_data_len;
    size_t file_offset = current_size;
    size_t data_offset = 0;

    if (prefix_len > 0) {
        size_t block_index = file_offset / BLOCK_SIZE;
        size_t offset_in_block = file_offset % BLOCK_SIZE;
        long write_pos = sb.data_start + node.blocks[block_index] * BLOCK_SIZE + offset_in_block;
        fseek(fs, write_pos, SEEK_SET);
        fwrite(newline, 1, prefix_len, fs);
        file_offset += prefix_len;
    }

    // Запись основного содержимого
    while (data_offset < data_len) {
        size_t block_index = file_offset / BLOCK_SIZE;
        size_t offset_in_block = file_offset % BLOCK_SIZE;
        size_t space_in_block = BLOCK_SIZE - offset_in_block;
        size_t to_write = (data_len - data_offset < space_in_block) ? (data_len - data_offset) : space_in_block;

        long write_position = sb.data_start + node.blocks[block_index] * BLOCK_SIZE + offset_in_block;
        fseek(fs, write_position, SEEK_SET);
        fwrite(data + data_offset, 1, to_write, fs);

        data_offset += to_write;
        file_offset += to_write;
    }

    // Обновляем inode
    node.size = (uint32_t)total_size;
    node.mtime = time(NULL);
    fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET);
    fwrite(&node, sizeof(Inode), 1, fs);

    // Обновляем битмап и суперблок
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fwrite(block_bitmap, sizeof(block_bitmap), 1, fs);

    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fwrite(&sb, sizeof(SuperBlock), 1, fs);

    return 1;
}


