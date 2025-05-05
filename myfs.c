#include "myfs.h"
#include <string.h>
#include <time.h>

#define FS_MAGIC 0x4D594653 // "MYFS"

// -----------------------------------------------------------------------------
// Описание: Функции для чтения и записи суперблока файловой системы
// Разработчик: Дарья
// -----------------------------------------------------------------------------

// Функция: write_superblock
// Назначение: Записывает структуру SuperBlock в файл образа ФС.
// Параметры:
//   - fs: указатель на открытый файл образа файловой системы
//   - sb: указатель на структуру SuperBlock, которую необходимо записать
// Возвращает:
//   - true, если операция записи прошла успешно
//   - false, если возникла ошибка при перемещении указателя или записи
static bool write_superblock(FILE* fs, const SuperBlock* sb) {
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0) {  // Устанавливаем указатель в начало суперблока
        perror("Ошибка fseek при записи суперблока");
        return false;
    }
    if (fwrite(sb, sizeof(SuperBlock), 1, fs) != 1) {   // Записываем структуру SuperBlock в файл
        perror("Ошибка fwrite при записи суперблока");
        return false;
    }
    return true;
}

// Функция: read_superblock
// Назначение: Считывает суперблок из файла образа файловой системы.
// Параметры:
//   - fs: указатель на открытый файл образа файловой системы
//   - sb: указатель на структуру SuperBlock, куда будут загружены данные
// Возвращает:
//   - true, если операция чтения прошла успешно
//   - false, если возникла ошибка при перемещении указателя или чтении
static bool read_superblock(FILE* fs, SuperBlock* sb) {
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0) {  // Устанавливаем указатель в начало суперблока
        perror("Ошибка fseek при чтении суперблока");
        return false;
    }
    if (fread(sb, sizeof(SuperBlock), 1, fs) != 1) {    // Считываем суперблок из файла в структуру
        perror("Ошибка fread при чтении суперблока");
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// Функция: format_fs
// Назначение: Форматирует и инициализирует структуру файловой системы.
// Используется при первом запуске, если файл с образом ФС ещё не существует.
// Автор: Игорь
// -----------------------------------------------------------------------------

bool format_fs(const char* filename) {
    // Открываем файл-образ файловой системы с обнулением содержимого
    FILE* fs = fopen(filename, "wb+");
    if (!fs) {
        perror("Не удалось открыть файл для форматирования");
        return false;
    }

    // Инициализируем суперблок — метаинформацию о структуре ФС
    SuperBlock sb = {
        .magic = FS_MAGIC,                // Сигнатура файловой системы
        .block_size = BLOCK_SIZE,        // Размер одного блока
        .inode_count = INODE_COUNT,      // Общее количество inode
        .block_count = BLOCK_COUNT,      // Общее количество блоков данных
        .free_inodes = INODE_COUNT,      // Все inode свободны
        .free_blocks = BLOCK_COUNT,      // Все блоки данных свободны
        .inode_bitmap = INODE_BITMAP_OFFSET,  // Смещение битовой карты inode
        .block_bitmap = BLOCK_BITMAP_OFFSET,  // Смещение битовой карты блоков
        .inode_table = INODE_TABLE_OFFSET,    // Смещение таблицы inode
        .data_start = DATA_BLOCKS_OFFSET      // Смещение начала данных
    };

    // Пишем суперблок в файл
    if (!write_superblock(fs, &sb)) {
        perror("Ошибка записи суперблока");
        fclose(fs);
        return false;
    }

    // Инициализируем битовые карты (всё свободно)
    uint8_t inode_bitmap[INODE_COUNT / 8] = {0};
    uint8_t block_bitmap[BLOCK_COUNT / 8] = {0};

    // Запись битовой карты inode
    fseek(fs, INODE_BITMAP_OFFSET, SEEK_SET);
    if (fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа inode");
        fclose(fs);
        return false;
    }

    // Запись битовой карты блоков
    fseek(fs, BLOCK_BITMAP_OFFSET, SEEK_SET);
    if (fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа блоков");
        fclose(fs);
        return false;
    }

    // Инициализируем все inode как пустые (включая нулевой)
    Inode empty_inode = {0};
    fseek(fs, INODE_TABLE_OFFSET, SEEK_SET);
    for (int i = 0; i < INODE_COUNT; i++) {
        if (fwrite(&empty_inode, sizeof(Inode), 1, fs) != 1) {
            perror("Ошибка записи inode таблицы");
            fclose(fs);
            return false;
        }
    }

    // Инициализируем область данных (все блоки нулями)
    uint8_t zero_block[BLOCK_SIZE] = {0};
    fseek(fs, DATA_BLOCKS_OFFSET, SEEK_SET);
    for (int i = 0; i < BLOCK_COUNT; ++i) {
        if (fwrite(zero_block, BLOCK_SIZE, 1, fs) != 1) {
            perror("Ошибка обнуления блоков данных");
            fclose(fs);
            return false;
        }
    }

    // Завершаем форматирование
    fclose(fs);
    return true;
}



/**
 * @file open_fs.c
 * @brief Открытие существующей файловой системы MYFS.
 * @author Sumisgard
 */

/**
 * Открывает существующую файловую систему
 * @param filename Имя файла-образа ФС
 * @return Указатель на FILE или NULL при ошибке
 */
FILE* open_fs(const char* filename) {
    // 1. Открываем файл в режиме чтения+записи (бинарный режим)
    FILE* fs = fopen(filename, "rb+");
    if (!fs) {
        perror("Ошибка открытия файла ФС");
        return NULL;
    }

    // 2. Читаем суперблок из начала файла
    SuperBlock sb;
    if (!read_superblock(fs, &sb)) {
        fprintf(stderr, "Ошибка: не удалось прочитать суперблок\n");
        fclose(fs);
        return NULL;
    }

    // 3. Проверка сигнатуры файловой системы (магическое число)
    if (sb.magic != FS_MAGIC) {
        fprintf(stderr, "Ошибка: файл не содержит MYFS (магическое число 0x%X)\n", sb.magic);
        fclose(fs);
        return NULL;
    }

    // 4. Проверка совместимости размеров блока
    if (sb.block_size != BLOCK_SIZE) {
        fprintf(stderr, "Ошибка: несовместимый размер блока (%u != %u)\n", 
                sb.block_size, BLOCK_SIZE);
        fclose(fs);
        return NULL;
    }

    // Файл ФС успешно открыт и проверен
    return fs;
}


/**
 * Закрывает файловую систему
 * @param fs Указатель на открытую ФС
 * Разработчик: Дарья
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
 * @file create_file.c
 * @brief Создание нового файла в файловой системе MYFS.
 * @author Игорь
 */

/**
 * Создаёт новый файл в ФС
 * @param fs     Указатель на открытую ФС
 * @param name   Имя файла (макс 255 символов)
 * @return       Номер inode или -1 при ошибке
 */
int create_file(FILE* fs, const char* name) {
    // Проверка параметров
    if (!fs || !name) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    if (strlen(name) >= 256) {
        fprintf(stderr, "Error: Filename too long\n");
        return -1;
    }

    // Чтение суперблока
    SuperBlock sb;
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 || 
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Superblock read failed");
        return -1;
    }

    // Проверка свободных inodes
    if (sb.free_inodes == 0) {
        fprintf(stderr, "Error: No free inodes\n");
        return -1;
    }

    // Поиск существующего файла и свободного inode
    uint8_t inode_bitmap[INODE_COUNT/8];
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Inode bitmap read failed");
        return -1;
    }

    int free_inode = -1;
    Inode temp;

    for (int i = 0; i < INODE_COUNT; i++) {
        if (inode_bitmap[i/8] & (1 << (i%8))) {
            if (fseek(fs, sb.inode_table + i*sizeof(Inode), SEEK_SET) != 0 ||
                fread(&temp, sizeof(Inode), 1, fs) != 1) {
                continue;
            }

            if (strcmp(temp.name, name) == 0) {
                fprintf(stderr, "Error: File '%s' already exists\n", name);
                return -1;
            }
        } 
        else if (free_inode == -1) {
            free_inode = i;
        }
    }

    if (free_inode == -1) {
        fprintf(stderr, "Error: No free inodes found\n");
        return -1;
    }

    // Выделение блоков при создании (1 блок по умолчанию)
    uint8_t block_bitmap[BLOCK_COUNT/8];
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Block bitmap read failed");
        return -1;
    }

    Inode new_inode = {0};
    strncpy(new_inode.name, name, sizeof(new_inode.name)-1);
    new_inode.mtime = time(NULL);
    
    // Выделяем 1 начальный блок
    int blocks_allocated = 0;
    for (int i = 0; i < BLOCK_COUNT && blocks_allocated < 1; i++) {
        if (!(block_bitmap[i/8] & (1 << (i%8)))) {
            block_bitmap[i/8] |= (1 << (i%8));
            new_inode.blocks[blocks_allocated] = i;
            blocks_allocated++;
            sb.free_blocks--;

            // Инициализация блока нулями
            char zero_block[BLOCK_SIZE] = {0};
            if (fseek(fs, sb.data_start + i*BLOCK_SIZE, SEEK_SET) != 0 ||
                fwrite(zero_block, BLOCK_SIZE, 1, fs) != 1) {
                perror("Block initialization failed");
                return -1;
            }
        }
    }

    if (blocks_allocated == 0) {
        fprintf(stderr, "Error: No free blocks available\n");
        return -1;
    }

    // Обновление структур
    inode_bitmap[free_inode/8] |= (1 << (free_inode%8));
    sb.free_inodes--;

    // Запись изменений
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Inode bitmap update failed");
        return -1;
    }

    if (fseek(fs, sb.inode_table + free_inode*sizeof(Inode), SEEK_SET) != 0 ||
        fwrite(&new_inode, sizeof(Inode), 1, fs) != 1) {
        perror("Inode write failed");
        return -1;
    }

    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Block bitmap update failed");
        return -1;
    }

    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Superblock update failed");
        return -1;
    }

    fflush(fs);
    return free_inode;
}

/**
 * Удаляет файл по имени из ФС.
 * @param fs    Указатель на открытую файловую систему
 * @param name  Имя файла для удаления
 * @return      true при успехе, false при ошибке
 * @author Татьяна
 */
bool delete_file(FILE* fs, const char* name) {
    SuperBlock sb;

    // Чтение суперблока
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return false;
    }

    // Загрузка битмапов
    uint8_t inode_bitmap[INODE_COUNT / 8];
    uint8_t block_bitmap[BLOCK_COUNT / 8];

    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return false;
    }

    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа блоков");
        return false;
    }

    // Поиск файла по имени
    int inode_num = -1;
    Inode inode;

    for (int i = 0; i < INODE_COUNT; i++) {
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) continue;

        if (fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET) != 0 ||
            fread(&inode, sizeof(Inode), 1, fs) != 1) {
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

    // Освобождение всех блоков, связанных с файлом
    for (int i = 0; i < 12; ++i) {
        if (inode.blocks[i] != 0) {
            block_bitmap[inode.blocks[i] / 8] &= ~(1 << (inode.blocks[i] % 8));
            sb.free_blocks++;
        }
    }

    // Очистка inode
    inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));

    // Обновление битмапов
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа блоков");
        return false;
    }

    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа inode");
        return false;
    }

    // Обновление суперблока
    sb.free_inodes++;
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка записи суперблока");
        return false;
    }

    printf("Файл '%s' (inode %d) успешно удалён\n", name, inode_num);
    return true;
}
/**
 * Выводит список всех файлов в файловой системе
 * @param fs Указатель на открытую файловую систему
 */
void list_files(FILE* fs) {
    SuperBlock sb;

    // Чтение суперблока
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return;
    }

    // Чтение битовой карты inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return;
    }

    printf("\n%-6s %-15s %-10s %-8s %-20s %-6s\n", 
           "INODE", "NAME", "TYPE", "SIZE", "MTIME", "BLOCKS");

    for (int i = 0; i < INODE_COUNT; i++) {
        // Проверяем, занят ли inode
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
            Inode node;
            
            // Читаем inode из таблицы
            if (fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET) != 0 ||
                fread(&node, sizeof(Inode), 1, fs) != 1) {
                fprintf(stderr, "Ошибка чтения inode %d\n", i);
                continue;
            }

            // Подсчёт используемых блоков
            int used_blocks = 0;
            for (int j = 0; j < 12; j++) {
                if (node.blocks[j] != 0) used_blocks++;
            }

            if (node.size == 0)
                used_blocks = 0;

            // Форматирование имени (обрезаем до 15 символов)
            char short_name[16];
            strncpy(short_name, node.name, 15);
            short_name[15] = '\0';

            // Определение типа файла
            const char* type = "file";
            if (strstr(node.name, ".txt")) type = "text";
            else if (strstr(node.name, ".dat")) type = "data";

            /****************************************************
             * КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: корректное преобразование времени
             * 1. Проверяем, что время не нулевое
             * 2. Правильно приводим тип к time_t
             * 3. Проверяем разумные границы времени
             ****************************************************/
            char timebuf[20] = "unknown";
            if (node.mtime > 0) {
                time_t mtime = (time_t)node.mtime;
                
                // Проверка на разумные временные рамки (1970-2100)
                if (mtime > 0 && mtime < 4102444800) {
                    struct tm* tm_info = localtime(&mtime);
                    if (tm_info) {
                        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm_info);
                    }
                }
            }

            // Вывод информации о файле
            printf("%-6d %-15s %-10s %-8u %-20s %-6d\n",
                   i, short_name, type, node.size, timebuf, used_blocks);
        }
    }
}

/**
 * Записывает данные в файл файловой системы
 * @param fs       Указатель на открытую ФС
 * @param filename Имя файла (макс 255 символов + '\0')
 * @param data     Данные для записи
 * @return         1 при успехе, 0 при ошибке
 */
int write_file(FILE* fs, const char* filename, const char* data) {
    // Проверка параметров
    if (!fs || !filename || !data) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return 0;
    }

    size_t data_len = strlen(data);
    if (data_len == 0) {
        fprintf(stderr, "Error: Empty data\n");
        return 0;
    }

    // Чтение метаданных
    SuperBlock sb;
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 || 
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Superblock read failed");
        return 0;
    }

    // Поиск файла
    Inode node;
    int inode_idx = -1;
    uint8_t inode_bitmap[INODE_COUNT/8];
    
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Inode bitmap read failed");
        return 0;
    }

    for (int i = 0; i < INODE_COUNT; i++) {
        if (!(inode_bitmap[i/8] & (1 << (i%8)))) continue;

        if (fseek(fs, sb.inode_table + i*sizeof(Inode), SEEK_SET) != 0 ||
            fread(&node, sizeof(Inode), 1, fs) != 1) {
            perror("Inode read failed");
            continue;
        }

        if (strcmp(node.name, filename) == 0) {
            inode_idx = i;
            break;
        }
    }

    if (inode_idx == -1) {
        fprintf(stderr, "Error: File '%s' not found\n", filename);
        return 0;
    }

    // Расчет необходимых блоков
    size_t required_blocks = (data_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (required_blocks > 12) {
        fprintf(stderr, "Error: File too large (max 12 blocks)\n");
        return 0;
    }

    // Проверка выделенных блоков
    int current_blocks = 0;
    while (current_blocks < 12 && node.blocks[current_blocks] != 0) {
        current_blocks++;
    }

    // Выделение дополнительных блоков при необходимости
    if (required_blocks > current_blocks) {
        uint8_t block_bitmap[BLOCK_COUNT/8];
        if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
            fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
            perror("Block bitmap read failed");
            return 0;
        }

        int blocks_needed = required_blocks - current_blocks;
        int blocks_allocated = 0;
        
        for (int i = 0; i < BLOCK_COUNT && blocks_allocated < blocks_needed; i++) {
            if (!(block_bitmap[i/8] & (1 << (i%8)))) {
                block_bitmap[i/8] |= (1 << (i%8));
                node.blocks[current_blocks + blocks_allocated] = i;
                blocks_allocated++;
                sb.free_blocks--;

                // Инициализация нового блока
                char zero_block[BLOCK_SIZE] = {0};
                if (fseek(fs, sb.data_start + i*BLOCK_SIZE, SEEK_SET) != 0 ||
                    fwrite(zero_block, BLOCK_SIZE, 1, fs) != 1) {
                    perror("Block initialization failed");
                    return 0;
                }
            }
        }

        if (blocks_allocated < blocks_needed) {
            fprintf(stderr, "Error: Not enough free blocks\n");
            return 0;
        }

        // Обновление битмапа блоков
        if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
            fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
            perror("Block bitmap update failed");
            return 0;
        }
    }

    // Запись данных
    for (int i = 0; i < required_blocks; i++) {
        size_t offset = i * BLOCK_SIZE;
        size_t to_write = (offset + BLOCK_SIZE > data_len) 
                        ? data_len - offset 
                        : BLOCK_SIZE;

        if (fseek(fs, sb.data_start + node.blocks[i]*BLOCK_SIZE, SEEK_SET) != 0 ||
            fwrite(data + offset, 1, to_write, fs) != to_write) {
            perror("Data write failed");
            return 0;
        }
    }

    // Обновление метаданных
    node.size = data_len;
    node.mtime = time(NULL);


    if (fseek(fs, sb.inode_table + inode_idx*sizeof(Inode), SEEK_SET) != 0 ||
        fwrite(&node, sizeof(Inode), 1, fs) != 1) {
        perror("Inode update failed");
        return 0;
    }

    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Superblock update failed");
        return 0;
    }

    fflush(fs);
    return 1;
}

/**
 * Читает содержимое файла из файловой системы
 * @param fs        Указатель на открытую файловую систему
 * @param filename  Имя файла для чтения
 * @param buffer    Буфер для записи содержимого файла
 * @param max_size  Максимальный размер данных для чтения (включая '\0')
 * @return          Количество прочитанных байт (без учета '\0') или 0 при ошибке
 * 
 * Примечания:
 * 1. Буфер всегда завершается нуль-терминатором
 * 2. Если файл больше max_size - будет прочитано только max_size-1 байт
 * 3. Для бинарных файлов нужно использовать другой подход (без null-termination)
 */
int read_file(FILE* fs, const char* filename, char* buffer, size_t max_size) {
    // Проверка входных параметров
    if (!fs || !filename || !buffer || max_size == 0) {
        fprintf(stderr, "Ошибка: некорректные параметры (fs=%p, filename=%p, buffer=%p, max_size=%zu)\n",
                fs, filename, buffer, max_size);
        return 0;
    }

    // Чтение суперблока - основной метаинформации ФС
    SuperBlock sb;
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0) {
        perror("Ошибка позиционирования суперблока");
        return 0;
    }
    if (fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return 0;
    }

    // Проверка валидности файловой системы
    if (sb.magic != FS_MAGIC) {
        fprintf(stderr, "Ошибка: неверная файловая система (magic=0x%X, ожидалось 0x%X)\n",
                sb.magic, FS_MAGIC);
        return 0;
    }

    // Чтение битовой карты inode (отмечает занятые/свободные inode)
    uint8_t inode_bitmap[INODE_COUNT / 8];
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0) {
        perror("Ошибка позиционирования битмапа inode");
        return 0;
    }
    if (fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return 0;
    }

    // Поиск файла по имени в таблице inode
    Inode node;
    int found_inode = -1;
    for (int i = 0; i < INODE_COUNT; i++) {
        // Пропускаем свободные inode
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) continue;

        // Читаем inode из таблицы
        if (fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET) != 0) {
            perror("Ошибка позиционирования inode");
            continue;
        }
        if (fread(&node, sizeof(Inode), 1, fs) != 1) {
            perror("Ошибка чтения inode");
            continue;
        }

        // Сравниваем имя файла
        if (strncmp(node.name, filename, sizeof(node.name)) == 0) {
            found_inode = i;
            break;
        }
    }

    if (found_inode == -1) {
        fprintf(stderr, "Файл '%s' не найден в файловой системе\n", filename);
        return 0;
    }

    // Проверка размера файла
    if (node.size == 0) {
        buffer[0] = '\0';
        return 0;  // Файл существует, но пустой
    }

    // Вычисляем сколько байт будем читать (оставляем место для '\0')
    size_t to_read = (node.size < max_size - 1) ? node.size : max_size - 1;
    size_t bytes_read = 0;

    // Чтение данных из блоков
    for (int i = 0; i < 12 && bytes_read < to_read; i++) {
        if (node.blocks[i] == 0) break;  // Нет больше блоков

        // Проверка валидности номера блока
        if (node.blocks[i] >= BLOCK_COUNT) {
            fprintf(stderr, "Ошибка: недопустимый номер блока %u\n", node.blocks[i]);
            break;
        }

        // Позиционируемся на начало блока
        long block_offset = sb.data_start + node.blocks[i] * BLOCK_SIZE;
        if (fseek(fs, block_offset, SEEK_SET) != 0) {
            perror("Ошибка позиционирования блока данных");
            break;
        }

        // Вычисляем сколько читать из текущего блока
        size_t remaining = to_read - bytes_read;
        size_t chunk = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
        

        // Читаем данные
        size_t actually_read = fread(buffer + bytes_read, 1, chunk, fs);
        if (actually_read != chunk) {
            perror("Ошибка чтения данных блока");
            fprintf(stderr, "Ожидалось %zu, прочитано %zu байт\n", chunk, actually_read);
            break;
        }

        bytes_read += actually_read;
    }

    // Гарантируем null-terminated строку
    buffer[bytes_read] = '\0';

    // Дополнительная проверка целостности
    if (bytes_read < to_read && bytes_read < node.size) {
        fprintf(stderr, "Предупреждение: прочитано %zu из %u байт файла '%s'\n",
                bytes_read, node.size, filename);
    }

    return (int)bytes_read;
}

int write_file1(FILE* fs, const char* filename, const char* data) {
    if (!fs || !filename || !data) {
        fprintf(stderr, "Ошибка: некорректные параметры\n");
        return 0;
    }

    SuperBlock sb;
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 || 
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return 0;
    }

    uint8_t inode_bitmap[INODE_COUNT / 8];
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 || 
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode");
        return 0;
    }

    Inode node;
    int found_inode = -1;
    for (int i = 0; i < INODE_COUNT; i++) {
        if (inode_bitmap[i / 8] & (1 << (i % 8))) {
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
    }

    if (found_inode == -1) {
        fprintf(stderr, "Файл '%s' не найден\n", filename);
        return 0;
    }

    const char* newline = "\n";
    size_t prefix_len = (node.size > 0) ? strlen(newline) : 0;
    size_t data_len = strlen(data);
    size_t total_data_len = prefix_len + data_len;

    size_t current_size = node.size;
    size_t total_size = current_size + total_data_len;

    size_t current_blocks = (current_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t required_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (required_blocks > 12) {
        fprintf(stderr, "Файл слишком большой (максимум 12 блоков)\n");
        return 0;
    }

    uint8_t block_bitmap[BLOCK_COUNT / 8];
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 || 
        fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа блоков");
        return 0;
    }

    // Выделение недостающих блоков
    for (int i = current_blocks; i < required_blocks; i++) {
        int allocated = 0;
        for (int j = 0; j < BLOCK_COUNT; j++) {
            if (!(block_bitmap[j / 8] & (1 << (j % 8)))) {
                block_bitmap[j / 8] |= (1 << (j % 8));
                node.blocks[i] = j;
                sb.free_blocks--;
                allocated = 1;
                break;
            }
        }
        if (!allocated) {
            fprintf(stderr, "Недостаточно свободных блоков\n");
            return 0;
        }
    }

    // Пишем перевод строки, если нужно
    size_t file_offset = current_size;
    if (prefix_len > 0) {
        size_t block_index = file_offset / BLOCK_SIZE;
        size_t offset_in_block = file_offset % BLOCK_SIZE;
        long write_pos = sb.data_start + node.blocks[block_index] * BLOCK_SIZE + offset_in_block;
        if (fseek(fs, write_pos, SEEK_SET) != 0 ||
            fwrite(newline, 1, prefix_len, fs) != prefix_len) {
            perror("Ошибка записи перевода строки");
            return 0;
        }
        file_offset += prefix_len;
    }

    // Пишем основное содержимое
    size_t data_offset = 0;
    while (data_offset < data_len) {
        size_t block_index = file_offset / BLOCK_SIZE;
        size_t offset_in_block = file_offset % BLOCK_SIZE;
        size_t space_in_block = BLOCK_SIZE - offset_in_block;
        size_t to_write = (data_len - data_offset < space_in_block) ? (data_len - data_offset) : space_in_block;

        long write_pos = sb.data_start + node.blocks[block_index] * BLOCK_SIZE + offset_in_block;
        if (fseek(fs, write_pos, SEEK_SET) != 0 ||
            fwrite(data + data_offset, 1, to_write, fs) != to_write) {
            perror("Ошибка записи данных");
            return 0;
        }

        file_offset += to_write;
        data_offset += to_write;
    }

    node.size = (uint32_t)total_size;
    node.mtime = time(NULL); // корректная метка времени


    // Запись inode
    if (fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET) != 0 ||
        fwrite(&node, sizeof(Inode), 1, fs) != 1) {
        perror("Ошибка обновления inode");
        return 0;
    }

    // Запись битмапа
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа блоков");
        return 0;
    }

    // Запись суперблока
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка записи суперблока");
        return 0;
    }

    return 1;
}


