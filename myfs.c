#include "myfs.h"
#include <string.h>
#include <time.h>

#define FS_MAGIC 0x4D594653 // проверка валидности файловой системы

// Записывает суперблок в файл ФС
// Возвращает true при успехе, false — при ошибке
static bool write_superblock(FILE* fs, const SuperBlock* sb) {
    // Устанавливаем указатель на нужное место в файле (суперблок должен быть записан по смещению SUPERBLOCK_OFFSET)
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0) {
        perror("Ошибка fseek при записи суперблока"); // Выводим ошибку, если fseek не удалось
        return false;
    }
    // Записываем данные суперблока в файл
    if (fwrite(sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка fwrite при записи суперблока"); // Выводим ошибку, если запись не удалась
        return false;
    }
    return true;
}

// Читает суперблок из файла ФС
// Возвращает true при успехе, false — при ошибке
static bool read_superblock(FILE* fs, SuperBlock* sb) {
    // Устанавливаем указатель на нужное место в файле (суперблок должен быть считан по смещению SUPERBLOCK_OFFSET)
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0) {
        perror("Ошибка fseek при чтении суперблока"); // Выводим ошибку, если fseek не удалось
        return false;
    }
    // Читаем данные суперблока из файла
    if (fread(sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка fread при чтении суперблока"); // Выводим ошибку, если чтение не удалось
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
        perror("Не удалось открыть файл для форматирования"); // Ошибка открытия файла
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
        fclose(fs); // Закрываем файл в случае ошибки
        return false;
    }

    // Инициализируем битмапы inode и блоков: всё свободно, кроме inode 0
    uint8_t inode_bitmap[INODE_COUNT / 8] = { 0 };  // Все inode свободны
    uint8_t block_bitmap[BLOCK_COUNT / 8] = { 0 };  // Все блоки свободны

    inode_bitmap[0 / 8] |= (1 << (0 % 8)); // Устанавливаем бит для корневого inode

    // Записываем битмап inode в файл
    fseek(fs, INODE_BITMAP_OFFSET, SEEK_SET);
    if (fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа inode");
        fclose(fs); // Закрываем файл в случае ошибки
        return false;
    }

    // Записываем битмап блоков в файл
    fseek(fs, BLOCK_BITMAP_OFFSET, SEEK_SET);
    if (fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка записи битмапа блоков");
        fclose(fs); // Закрываем файл в случае ошибки
        return false;
    }


    // Создаём root inode (каталог, inode 0)
    Inode root_inode = {
        .mode = 040755,     // Тип — каталог, права 755 (rwxr-xr-x)
        .uid = 0,           // Пользователь root
        .size = 0,          // Пустой каталог (если не используется структура директорий)
        .mtime = time(NULL) // Время создания
    };

    Inode empty_inode = { 0 };  // Пустой inode для остальных мест

    // Записываем таблицу inode: корневой + пустые
    fseek(fs, INODE_TABLE_OFFSET, SEEK_SET);
    for (int i = 0; i < INODE_COUNT; ++i) {
        if (fwrite((i == 0) ? &root_inode : &empty_inode, sizeof(Inode), 1, fs) != 1) {
            perror("Ошибка записи inode таблицы");
            fclose(fs); // Закрываем файл в случае ошибки
            return false;
        }
    }

    // Инициализируем область данных нулями
    uint8_t zero_block[BLOCK_SIZE] = { 0 };  // Создаём блок данных, заполненный нулями
    fseek(fs, DATA_BLOCKS_OFFSET, SEEK_SET);
    for (int i = 0; i < BLOCK_COUNT; ++i) {
        if (fwrite(zero_block, BLOCK_SIZE, 1, fs) != 1) {
            perror("Ошибка обнуления блоков данных");
            fclose(fs); // Закрываем файл в случае ошибки
            return false;
        }
    }

    fclose(fs);  // Закрываем файл после успешного форматирования
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

    // 4. Проверяем совместимость параметров (например, размер блока)
    if (sb.block_size != BLOCK_SIZE) {
        fprintf(stderr, "Ошибка: несовместимый размер блока (%u != %u)\n",
            sb.block_size, BLOCK_SIZE);
        fclose(fs);
        return NULL;
    }

    return fs;  // Возвращаем указатель на открытый файл ФС
}


/**
 * Закрывает файловую систему
 * @param fs Указатель на открытую ФС
 */
void close_fs(FILE* fs) {
    if (!fs) return;  // Если указатель на ФС NULL, ничего не делаем

    // 1. Сбрасываем буферы на диск (если есть отложенные данные)
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
        return -1;  // Если имя слишком длинное, возвращаем ошибку
    }

    SuperBlock sb;
    // Чтение суперблока с проверкой ошибок
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока");
        return -1;
    }
}
// Проверка, если нет свободных inode
if (sb.free_inodes == 0) {
    fprintf(stderr, "Ошибка: нет свободных inodes\n"); // Выводим ошибку, если нет свободных inode
    return -1; // Возвращаем -1, указывая на ошибку
}

// Чтение битмапа inode для проверки занятости
uint8_t inode_bitmap[INODE_COUNT / 8];
if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 || // Переходим в позицию для чтения битмапа
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) { // Читаем битмап inode
    perror("Ошибка чтения битмапа inode"); // Обработка ошибки чтения
    return -1; // Возвращаем -1 при ошибке
}

/* Поиск существующего файла и свободного inode */
int free_inode = -1; // Переменная для хранения свободного inode
Inode temp;

for (int i = 1; i < INODE_COUNT; ++i) { // Пропускаем inode 0 (корневой)
    if (inode_bitmap[i / 8] & (1 << (i % 8))) { // Если inode занят
        // Чтение существующего inode для проверки имени
        if (fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET) != 0 || // Переход в позицию inode в таблице
            fread(&temp, sizeof(Inode), 1, fs) != 1) { // Чтение inode
            continue; // Если чтение не удалось, продолжаем цикл
        }

        if (strncmp(temp.name, name, sizeof(temp.name)) == 0) { // Если файл с таким именем уже существует
            printf("Ошибка: файл '%s' уже существует (inode %d)\n", name, i); // Выводим ошибку
            return -1; // Возвращаем -1, так как файл уже существует
        }
    }
    else if (free_inode == -1) { // Если inode свободен и мы еще не нашли свободный
        free_inode = i; // Запоминаем первый свободный inode
    }
}

if (free_inode == -1) { // Если свободных inode не найдено
    fprintf(stderr, "Ошибка: нет свободных inodes (несмотря на sb.free_inodes)\n"); // Выводим ошибку
    return -1; // Возвращаем -1 при отсутствии свободных inode
}

/* Создание нового файла */
Inode new_inode = {
    .mode = 0100644,     // Права доступа файла -rw-r--r--
    .uid = 0,            // Идентификатор пользователя (root)
    .size = 0,           // Размер файла (пока 0)
    .mtime = time(NULL), // Время последней модификации (время создания)
    .blocks = {0}        // Пока файл не использует блоки данных
};
strncpy(new_inode.name, name, sizeof(new_inode.name) - 1); // Копируем имя файла в inode
new_inode.name[sizeof(new_inode.name) - 1] = '\0'; // Обеспечиваем завершение строки

// Запись нового inode в таблицу inode
if (fseek(fs, sb.inode_table + free_inode * sizeof(Inode), SEEK_SET) != 0 || // Переход к нужному месту в таблице inode
    fwrite(&new_inode, sizeof(Inode), 1, fs) != 1) { // Записываем новый inode
    perror("Ошибка записи inode"); // Обработка ошибки записи
    return -1; // Возвращаем -1 при ошибке записи
}

// Обновление битмапа inode, отмечаем новый inode как занят
inode_bitmap[free_inode / 8] |= (1 << (free_inode % 8));
if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 || // Переход к месту записи битмапа inode
    fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) { // Запись обновленного битмапа
    perror("Ошибка обновления битмапа inode"); // Обработка ошибки записи
    return -1; // Возвращаем -1 при ошибке записи
}

// Обновление суперблока: уменьшаем количество свободных inode
sb.free_inodes--;
if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 || // Переход к месту записи суперблока
    fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) { // Запись обновленного суперблока
    perror("Ошибка обновления суперблока"); // Обработка ошибки записи
    return -1; // Возвращаем -1 при ошибке записи
}

printf("Создан файл '%s' (inode %d)\n", name, free_inode); // Уведомляем о создании файла
return free_inode; // Возвращаем номер inode созданного файла
}

bool delete_file(FILE* fs, const char* name) {
    SuperBlock sb;

    // Чтение суперблока для проверки состояния ФС
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) { // Чтение суперблока
        perror("Ошибка чтения суперблока"); // Обработка ошибки
        return false; // Возвращаем false при ошибке
    }


    // Загружаем битмапы inode и блоков
    uint8_t inode_bitmap[INODE_COUNT / 8];
    uint8_t block_bitmap[BLOCK_COUNT / 8];

    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) { // Чтение битмапа inode
        perror("Ошибка чтения битмапа inode"); // Обработка ошибки
        return false; // Возвращаем false при ошибке
    }

    fseek(fs, sb.block_bitmap, SEEK_SET);
    if (fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) { // Чтение битмапа блоков
        perror("Ошибка чтения битмапа блоков"); // Обработка ошибки
        return false; // Возвращаем false при ошибке
    }

    // Ищем файл с нужным именем
    int inode_num = -1;
    Inode inode;
    for (int i = 0; i < INODE_COUNT; ++i) { // Проходим по всем inode
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) continue; // Пропускаем свободные inode

        fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET); // Переход к нужному inode
        if (fread(&inode, sizeof(Inode), 1, fs) != 1) { // Чтение inode
            perror("Ошибка чтения inode"); // Обработка ошибки
            return false; // Возвращаем false при ошибке
        }

        if (strcmp(inode.name, name) == 0) { // Если нашли файл с нужным именем
            inode_num = i; // Запоминаем номер inode
            break; // Выход из цикла
        }
    }

    if (inode_num == -1) { // Если файл не найден
        printf("Файл '%s' не найден\n", name); // Выводим сообщение об ошибке
        return false; // Возвращаем false, так как файл не найден
    }

    // Освобождаем занятые блоки данных файла
    for (int i = 0; i < 12; ++i) { // Проходим по блокам файла
        if (inode.blocks[i] != 0) { // Если блок занят
            block_bitmap[inode.blocks[i] / 8] &= ~(1 << (inode.blocks[i] % 8)); // Освобождаем блок
            sb.free_blocks++; // Увеличиваем количество свободных блоков
        }
    }

    // Стираем inode (отмечаем его как свободный)
    inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));

    // Обновляем битмапы и суперблок
    fseek(fs, sb.block_bitmap, SEEK_SET);
    if (fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) { // Запись обновленного битмапа блоков
        perror("Ошибка записи битмапа блоков"); // Обработка ошибки
        return false; // Возвращаем false при ошибке записи
    }

    fseek(fs, sb.inode_bitmap, SEEK_SET);
    if (fwrite(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) { // Запись обновленного битмапа inode
        perror("Ошибка записи битмапа inode"); // Обработка ошибки
        return false; // Возвращаем false при ошибке записи
    }

    sb.free_inodes++; // Увеличиваем количество свободных inode
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    if (fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1) { // Запись обновленного суперблока
        perror("Ошибка обновления суперблока"); // Обработка ошибки
        return false; // Возвращаем false при ошибке записи
    }

    printf("Файл '%s' (inode %d) успешно удален\n", name, inode_num); // Уведомляем о успешном удалении
    return true; // Возвращаем true, операция успешна
}

void list_files(FILE* fs) {
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET); // Переход к суперблоку
    if (fread(&sb, sizeof(SuperBlock), 1, fs) != 1) { // Чтение суперблока
        perror("Ошибка чтения суперблока"); // Обработка ошибки
        return; // Завершаем функцию при ошибке
    }

    // Чтение битмапа inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET); // Переход к битмапу inode
    if (fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) { // Чтение битмапа inode
        perror("Ошибка чтения битмапа inode"); // Обработка ошибки
        return; // Завершаем функцию при ошибке
    }

    // Выводим заголовок списка файлов
    printf("\n%-6s %-15s %-10s %-8s %-20s %-6s\n",
        "INODE", "NAME", "TYPE", "SIZE", "MTIME", "BLOCKS");

    for (int i = 1; i < INODE_COUNT; i++) { // Проходим по всем inode
        if (inode_bitmap[i / 8] & (1 << (i % 8))) { // Если inode занят
            Inode node;
            fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET); // Переход к inode
            if (fread(&node, sizeof(Inode), 1, fs) != 1) { // Чтение inode
                printf("Ошибка чтения inode %d\n", i); // Выводим ошибку
                continue; // Переход к следующему inode
            }


            // Подсчёт используемых блоков
            int used_blocks = 0;
            for (int j = 0; j < 12; j++) { // Проверяем все 12 блоков
                if (node.blocks[j] != 0) used_blocks++; // Если блок занят, увеличиваем счётчик
            }

            // Преобразуем mode в строку прав доступа
            char permissions[11] = "----------";
            if ((node.mode & 0100000) == 0100000) permissions[0] = '-';  // обычный файл
            if (node.mode & 0400) permissions[1] = 'r'; // Проверка прав доступа
            if (node.mode & 0200) permissions[2] = 'w';
            if (node.mode & 0100) permissions[3] = 'x';
            if (node.mode & 0040) permissions[4] = 'r';
            if (node.mode & 0020) permissions[5] = 'w';
            if (node.mode & 0010) permissions[6] = 'x';
            if (node.mode & 0004) permissions[7] = 'r';
            if (node.mode & 0002) permissions[8] = 'w';
            if (node.mode & 0001) permissions[9] = 'x';
            // Поскольку у нас все файлы — текстовые, тип фиксирован
            const char* type = "text"; // Определяем тип файла как "text" (для простоты)

            char short_name[16]; // Массив для хранения обрезанного имени файла
            strncpy(short_name, node.name, 15); // Копируем первые 15 символов имени файла
            short_name[15] = '\0'; // Обеспечиваем, что строка заканчивается нулевым символом

            char timebuf[20]; // Буфер для хранения времени в строковом формате
            struct tm* tm_info = localtime((time_t*)&node.mtime); // Получаем локальное время из метки времени
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", tm_info); // Форматируем время в строку

            // Выводим информацию о файле в консоль в табличном виде
            printf("%-6d %-15s %-10s %-8u %-20s %-6d\n",
                i, short_name, type, node.size, timebuf, used_blocks); // Выводим inode, имя, тип, размер, время модификации и количество блоков
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
    // Проверка входных параметров на null
    if (!fs || !filename || !data) {
        fprintf(stderr, "Ошибка: неверные параметры\n"); // Если параметры некорректны, выводим ошибку
        return 0; // Возвращаем 0, чтобы указать на ошибку
    }

    SuperBlock sb;
    // Чтение суперблока с проверкой ошибок
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fread(&sb, sizeof(SuperBlock), 1, fs) != 1) {
        perror("Ошибка чтения суперблока"); // Ошибка при чтении суперблока
        return 0; // Возвращаем 0 при ошибке
    }

    // Чтение битмапа inode с проверкой ошибок
    uint8_t inode_bitmap[INODE_COUNT / 8];
    if (fseek(fs, sb.inode_bitmap, SEEK_SET) != 0 ||
        fread(inode_bitmap, sizeof(inode_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа inode"); // Ошибка при чтении битмапа inode
        return 0; // Возвращаем 0 при ошибке
    }

    /* Поиск файла по имени */
    Inode node;
    int found_inode = -1; // Инициализируем переменную для хранения найденного inode

    // Ищем файл по имени в таблице inode
    for (int i = 1; i < INODE_COUNT; i++) { // Пропускаем inode 0 (корневой каталог)
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) continue; // Пропускаем занятые inode

        // Чтение inode с проверкой ошибок
        if (fseek(fs, sb.inode_table + i * sizeof(Inode), SEEK_SET) != 0 ||
            fread(&node, sizeof(Inode), 1, fs) != 1) {
            perror("Ошибка чтения inode"); // Ошибка при чтении inode
            continue;
        }

        if (strncmp(node.name, filename, sizeof(node.name)) == 0) { // Сравниваем имена
            found_inode = i; // Если имя совпадает, сохраняем номер inode
            break; // Выход из цикла после нахождения файла
        }
    }

    if (found_inode == -1) { // Если файл не найден
        fprintf(stderr, "Файл '%s' не найден\n", filename); // Выводим сообщение об ошибке
        return 0; // Возвращаем 0, указывая на ошибку
    }

    /* Освобождение старых блоков */
    uint8_t block_bitmap[BLOCK_COUNT / 8];
    // Чтение битмапа блоков с проверкой ошибок
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fread(block_bitmap, sizeof(block_bitmap), 1, fs) != 1) {
        perror("Ошибка чтения битмапа блоков"); // Ошибка при чтении битмапа блоков
        return 0; // Возвращаем 0 при ошибке
    }


    unsigned freed_blocks = 0; // Переменная для подсчета освобожденных блоков
    for (int i = 0; i < 12; i++) { // Для каждого из 12 возможных блоков inode
        if (node.blocks[i] != 0) { // Если блок занят
            uint32_t block_num = node.blocks[i]; // Получаем номер блока
            // Проверка валидности номера блока
            if (block_num >= BLOCK_COUNT) {
                fprintf(stderr, "Предупреждение: некорректный номер блока %u\n", block_num);
                continue; // Пропускаем неверные блоки
            }
            block_bitmap[block_num / 8] &= ~(1 << (block_num % 8)); // Освобождаем блок
            node.blocks[i] = 0; // Обнуляем ссылку на блок в inode
            freed_blocks++; // Увеличиваем счетчик освобожденных блоков
            sb.free_blocks++; // Увеличиваем количество свободных блоков
        }
    }

    /* Выделение новых блоков */
    size_t data_len = strlen(data); // Получаем длину данных, которые нужно записать
    size_t required_blocks = (data_len + BLOCK_SIZE - 1) / BLOCK_SIZE; // Вычисляем количество требуемых блоков

    if (required_blocks > 12) { // Проверка на превышение максимального числа блоков
        fprintf(stderr, "Ошибка: файл слишком большой (требуется %zu блоков, макс 12)\n",
            required_blocks); // Сообщаем об ошибке
        return 0; // Возвращаем 0
    }

    // Поиск свободных блоков
    int blocks_allocated = 0; // Переменная для подсчета выделенных блоков
    for (int i = 0; i < BLOCK_COUNT && blocks_allocated < required_blocks; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) { // Если блок свободен
            block_bitmap[i / 8] |= (1 << (i % 8)); // Занимаем блок
            node.blocks[blocks_allocated++] = i; // Записываем номер блока в inode
            sb.free_blocks--; // Уменьшаем количество свободных блоков
        }
    }

    if (blocks_allocated < required_blocks) { // Если не удалось выделить все блоки
        fprintf(stderr, "Ошибка: недостаточно свободных блоков (найдено %d из %zu)\n",
            blocks_allocated, required_blocks); // Сообщаем об ошибке
        // Возвращаем выделенные блоки
        for (int i = 0; i < blocks_allocated; i++) {
            block_bitmap[node.blocks[i] / 8] &= ~(1 << (node.blocks[i] % 8)); // Освобождаем выделенные блоки
            sb.free_blocks++; // Увеличиваем количество свободных блоков
        }
        return 0; // Возвращаем 0 при ошибке
    }

    /* Запись данных */
    for (int i = 0; i < blocks_allocated; i++) { // Для каждого выделенного блока
        size_t offset = i * BLOCK_SIZE; // Вычисляем смещение для записи данных
        size_t to_write = (offset + BLOCK_SIZE > data_len) ?
            data_len - offset : BLOCK_SIZE; // Вычисляем количество данных для записи

        // Запись данных в файл
        if (fseek(fs, sb.data_start + node.blocks[i] * BLOCK_SIZE, SEEK_SET) != 0 ||
            fwrite(data + offset, 1, to_write, fs) != to_write) {
            perror("Ошибка записи данных"); // Ошибка при записи данных
            // Откатываем изменения
            for (int j = 0; j < blocks_allocated; j++) {
                block_bitmap[node.blocks[j] / 8] &= ~(1 << (node.blocks[j] % 8)); // Освобождаем блоки
                sb.free_blocks++; // Увеличиваем количество свободных блоков
            }
            return 0; // Возвращаем 0 при ошибке
        }
    }

    /* Обновление метаданных */
    node.size = (uint32_t)data_len; // Обновляем размер файла
    node.mtime = time(NULL); // Обновляем метку времени

    // Запись обновленного inode
    if (fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET) != 0 ||
        fwrite(&node, sizeof(Inode), 1, fs) != 1 ||
        fflush(fs) != 0) {
        perror("Ошибка записи inode"); // Ошибка при записи inode
        return 0; // Возвращаем 0 при ошибке
    }


    unsigned freed_blocks = 0; // Переменная для подсчета освобожденных блоков
    for (int i = 0; i < 12; i++) { // Для каждого из 12 возможных блоков inode
        if (node.blocks[i] != 0) { // Если блок занят
            uint32_t block_num = node.blocks[i]; // Получаем номер блока
            // Проверка валидности номера блока
            if (block_num >= BLOCK_COUNT) {
                fprintf(stderr, "Предупреждение: некорректный номер блока %u\n", block_num);
                continue; // Пропускаем неверные блоки
            }
            block_bitmap[block_num / 8] &= ~(1 << (block_num % 8)); // Освобождаем блок
            node.blocks[i] = 0; // Обнуляем ссылку на блок в inode
            freed_blocks++; // Увеличиваем счетчик освобожденных блоков
            sb.free_blocks++; // Увеличиваем количество свободных блоков
        }
    }

    /* Выделение новых блоков */
    size_t data_len = strlen(data); // Получаем длину данных, которые нужно записать
    size_t required_blocks = (data_len + BLOCK_SIZE - 1) / BLOCK_SIZE; // Вычисляем количество требуемых блоков

    if (required_blocks > 12) { // Проверка на превышение максимального числа блоков
        fprintf(stderr, "Ошибка: файл слишком большой (требуется %zu блоков, макс 12)\n",
            required_blocks); // Сообщаем об ошибке
        return 0; // Возвращаем 0
    }

    // Поиск свободных блоков
    int blocks_allocated = 0; // Переменная для подсчета выделенных блоков
    for (int i = 0; i < BLOCK_COUNT && blocks_allocated < required_blocks; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) { // Если блок свободен
            block_bitmap[i / 8] |= (1 << (i % 8)); // Занимаем блок
            node.blocks[blocks_allocated++] = i; // Записываем номер блока в inode
            sb.free_blocks--; // Уменьшаем количество свободных блоков
        }
    }

    if (blocks_allocated < required_blocks) { // Если не удалось выделить все блоки
        fprintf(stderr, "Ошибка: недостаточно свободных блоков (найдено %d из %zu)\n",
            blocks_allocated, required_blocks); // Сообщаем об ошибке
        // Возвращаем выделенные блоки
        for (int i = 0; i < blocks_allocated; i++) {
            block_bitmap[node.blocks[i] / 8] &= ~(1 << (node.blocks[i] % 8)); // Освобождаем выделенные блоки
            sb.free_blocks++; // Увеличиваем количество свободных блоков
        }
        return 0; // Возвращаем 0 при ошибке
    }

    /* Запись данных */
    for (int i = 0; i < blocks_allocated; i++) { // Для каждого выделенного блока
        size_t offset = i * BLOCK_SIZE; // Вычисляем смещение для записи данных
        size_t to_write = (offset + BLOCK_SIZE > data_len) ?
            data_len - offset : BLOCK_SIZE; // Вычисляем количество данных для записи

        // Запись данных в файл
        if (fseek(fs, sb.data_start + node.blocks[i] * BLOCK_SIZE, SEEK_SET) != 0 ||
            fwrite(data + offset, 1, to_write, fs) != to_write) {
            perror("Ошибка записи данных"); // Ошибка при записи данных
            // Откатываем изменения
            for (int j = 0; j < blocks_allocated; j++) {
                block_bitmap[node.blocks[j] / 8] &= ~(1 << (node.blocks[j] % 8)); // Освобождаем блоки
                sb.free_blocks++; // Увеличиваем количество свободных блоков
            }
            return 0; // Возвращаем 0 при ошибке
        }
    }

    /* Обновление метаданных */
    node.size = (uint32_t)data_len; // Обновляем размер файла
    node.mtime = time(NULL); // Обновляем метку времени

    // Запись обновленного inode
    if (fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET) != 0 ||
        fwrite(&node, sizeof(Inode), 1, fs) != 1 ||
        fflush(fs) != 0) {
        perror("Ошибка записи inode"); // Ошибка при записи inode
        return 0; // Возвращаем 0 при ошибке
    }


    // Обновление битмапа блоков
    if (fseek(fs, sb.block_bitmap, SEEK_SET) != 0 ||
        fwrite(block_bitmap, sizeof(block_bitmap), 1, fs) != 1 ||
        fflush(fs) != 0) {
        perror("Ошибка записи битмапа блоков"); // Ошибка при записи битмапа блоков
        return 0; // Возвращаем 0 при ошибке
    }

    // Обновление суперблока
    if (fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET) != 0 ||
        fwrite(&sb, sizeof(SuperBlock), 1, fs) != 1 ||
        fflush(fs) != 0) {
        perror("Ошибка обновления суперблока"); // Ошибка при обновлении суперблока
        return 0; // Возвращаем 0 при ошибке
    }

    // Выводим информацию о записи данных
    printf("Данные записаны в файл '%s' (inode %d). Использовано %d блоков.\n",
        filename, found_inode, blocks_allocated); // Сообщаем об успешной записи данных
    return 1; // Возвращаем 1 при успешной записи
}
// Чтение содержимого файла из виртуальной файловой системы
int read_file(FILE* fs, const char* filename, char* buffer, size_t max_size) {
    // Чтение суперблока
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fs);

    // Чтение битмапа inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);

    // Поиск inode по имени файла
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

    // Если файл не найден
    if (found_inode == -1) {
        printf("Файл '%s' не найден\n", filename);
        return 0;
    }

    // Определение объема данных для чтения с учетом ограничения буфера
    size_t to_read = (node.size < max_size - 1) ? node.size : max_size - 1;
    size_t bytes_read = 0;

    // Чтение данных по блокам
    for (int i = 0; i < 12 && bytes_read < to_read; i++) {
        if (node.blocks[i] == 0) break;

        size_t offset = sb.data_start + node.blocks[i] * BLOCK_SIZE;
        fseek(fs, offset, SEEK_SET);

        size_t remaining = to_read - bytes_read;
        size_t chunk = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

        fread(buffer + bytes_read, 1, chunk, fs);
        bytes_read += chunk;
    }

    // Завершение строки нулевым символом
    buffer[bytes_read] = '\0';

    return (int)bytes_read;
}

// Дописать данные в конец существующего файла (в отличие от полной перезаписи)
int write_file1(FILE* fs, const char* filename, const char* data) {
    // Чтение суперблока
    SuperBlock sb;
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fread(&sb, sizeof(SuperBlock), 1, fs);

    // Чтение битмапа inode
    uint8_t inode_bitmap[INODE_COUNT / 8];
    fseek(fs, sb.inode_bitmap, SEEK_SET);
    fread(inode_bitmap, sizeof(inode_bitmap), 1, fs);

    // Поиск inode по имени файла
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

    // Если файл не найден
    if (found_inode == -1) {
        printf("Файл '%s' не найден\n", filename);
        return 0;
    }

    // Добавляем перевод строки, если файл не пустой
    const char* newline = "\n";
    size_t prefix_len = (node.size > 0) ? strlen(newline) : 0;
    size_t data_len = strlen(data);
    size_t total_data_len = prefix_len + data_len;

    size_t current_size = node.size;
    size_t total_size = current_size + total_data_len;

    // Определяем количество блоков до и после добавления
    size_t current_blocks = (current_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t required_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Проверка, помещается ли файл в допустимые 12 блоков
    if (required_blocks > 12) {
        printf("Файл слишком большой (макс 12 блоков)\n");
        return 0;
    }

    // Чтение битмапа блоков
    uint8_t block_bitmap[BLOCK_COUNT / 8];
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fread(block_bitmap, sizeof(block_bitmap), 1, fs);

    // Выделение недостающих блоков
    for (int i = current_blocks; i < required_blocks; i++) {
        for (int j = 0; j < BLOCK_COUNT; j++) {
            if (!(block_bitmap[j / 8] & (1 << (j % 8)))) {
                block_bitmap[j / 8] |= (1 << (j % 8));
                node.blocks[i] = j;
                sb.free_blocks--;
                break;
            }
        }


        // Если не удалось выделить блок
        if (node.blocks[i] == 0) {
            printf("Недостаточно свободных блоков\n");
            return 0;
        }
    }

    // Позиция текущей записи
    size_t remaining = total_data_len;
    size_t file_offset = current_size;
    size_t data_offset = 0;

    // Запись перевода строки, если требуется
    if (prefix_len > 0) {
        size_t block_index = file_offset / BLOCK_SIZE;
        size_t offset_in_block = file_offset % BLOCK_SIZE;
        long write_pos = sb.data_start + node.blocks[block_index] * BLOCK_SIZE + offset_in_block;
        fseek(fs, write_pos, SEEK_SET);
        fwrite(newline, 1, prefix_len, fs);
        file_offset += prefix_len;
    }

    // Основная запись содержимого по блокам
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

    // Обновление inode: размер и время модификации
    node.size = (uint32_t)total_size;
    node.mtime = time(NULL);
    fseek(fs, sb.inode_table + found_inode * sizeof(Inode), SEEK_SET);
    fwrite(&node, sizeof(Inode), 1, fs);

    // Сохраняем обновлённый битмап блоков
    fseek(fs, sb.block_bitmap, SEEK_SET);
    fwrite(block_bitmap, sizeof(block_bitmap), 1, fs);

    // Сохраняем обновлённый суперблок
    fseek(fs, SUPERBLOCK_OFFSET, SEEK_SET);
    fwrite(&sb, sizeof(SuperBlock), 1, fs);

    return 1;
}
