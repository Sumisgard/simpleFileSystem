#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myfs.h"

// Цвета
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

void show_menu() {
    printf("\n%s========== %sMYFS МЕНЮ%s ==========%s\n", CYAN, BOLD, CYAN, RESET);
    printf("%s 1.%s 📦 Форматирование/инициализация ФС\n", GREEN, RESET);
    printf("%s 2.%s 📄 Создать файл\n", GREEN, RESET);
    printf("%s 3.%s 📂 Показать список файлов\n", GREEN, RESET);
    printf("%s 4.%s ❌ Удалить файл\n", GREEN, RESET);
    printf("%s 5.%s ✏️  Перезаписать файл\n", GREEN, RESET);
    printf("%s 6.%s ➕ Дозаписать в файл\n", GREEN, RESET);
    printf("%s 7.%s 📖 Прочитать файл\n", GREEN, RESET);
    printf("%s 8.%s ℹ️  Описание команд\n", GREEN, RESET);
    printf("%s 0.%s 🚪 Выход\n", RED, RESET);
    printf("%sВыбор:%s ", BOLD, RESET);
}

void show_help() {
    printf("\n%sОПИСАНИЕ КОМАНД:%s\n", CYAN, RESET);
    printf("1. Форматирование/инициализация — создаёт новую файловую систему на 'disk.img'\n");
    printf("2. Создать файл — добавляет новый пустой файл\n");
    printf("3. Показать файлы — отображает таблицу всех файлов в системе\n");
    printf("4. Удалить файл — удаляет файл и освобождает блоки\n");
    printf("5. Перезаписать файл — удаляет старое содержимое и записывает новое\n");
    printf("6. Дозаписать — добавляет текст в конец файла\n");
    printf("7. Прочитать файл — выводит содержимое файла\n");
    printf("8. Описание — выводит эту справку\n");
    printf("0. Выход — завершает программу\n");
}

// Автор: Тимур
char* read_multiline_input() {
    printf("Введите данные (завершите Ctrl+D или Ctrl+Z):\n\n");

    size_t size = 1024;
    size_t len = 0;
    char* buffer = malloc(size);
    if (!buffer) return NULL;

    int c;
    while ((c = getchar()) != EOF) {
        if (len + 1 >= size) {
            size *= 2;
            buffer = realloc(buffer, size);
            if (!buffer) return NULL;
        }
        buffer[len++] = (char)c;
    }
    buffer[len] = '\0';
    return buffer;
}

int main() {
    const char* fs_name = "disk.img";
    FILE* fs = NULL;

    // Автоинициализация ФС
    FILE* test = fopen(fs_name, "rb");
    if (!test) {
        printf("%s[ИНФО]%s Файл ФС не найден. Выполняется инициализация...\n", YELLOW, RESET);
        if (!format_fs(fs_name)) {
            fprintf(stderr, "%s[ОШИБКА]%s Не удалось инициализировать ФС\n", RED, RESET);
            return 1;
        }
        printf("%s[ОК]%s ФС успешно инициализирована\n", GREEN, RESET);
    } else {
        fclose(test);
    }

    int choice;
    char filename[256];

    while (1) {
        show_menu();

        if (scanf("%d", &choice) != 1) {
            printf("Ошибка ввода\n");
            while (getchar() != '\n');
            continue;
        }
        getchar();  // очищаем \n

        switch (choice) {
            case 1:
                if (format_fs(fs_name)) {
                    printf("%s[ОК]%s ФС успешно отформатирована и инициализирована\n", GREEN, RESET);
                } else {
                    printf("%s[ОШИБКА]%s Ошибка форматирования\n", RED, RESET);
                }
                break;

            case 2:
                if (!fs) fs = open_fs(fs_name);
                if (fs) {
                    printf("Имя файла: ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = '\0';
                    if (create_file(fs, filename) >= 0) {
                        printf("Файл создан\n");
                    } else {
                        printf("Ошибка создания файла\n");
                    }
                }
                break;

            case 3:
                if (!fs) fs = open_fs(fs_name);
                if (fs) list_files(fs);
                break;
                

            case 4:
                if (!fs) fs = open_fs(fs_name);
                if (fs) {
                    printf("Имя файла: ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = '\0';
                    if (delete_file(fs, filename)) {
                        printf("Файл удалён\n");
                    } else {
                        printf("Ошибка удаления\n");
                    }
                }
                break;

            case 5:
                if (!fs) fs = open_fs(fs_name);
                if (fs) {
                    printf("Имя файла: ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = '\0';

                    getchar();
                    char* data = read_multiline_input();
                    if (!data) {
                        printf("Ошибка чтения данных\n");
                        break;
                    }

                    if (write_file(fs, filename, data)) {
                        printf("\nЗапись успешна\n");
                    } else {
                        printf("\nОшибка записи\n");
                    }
                    free(data);
                }
                break;

            case 6:
                if (!fs) fs = open_fs(fs_name);
                if (fs) {
                    printf("Имя файла: ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = '\0';

                    getchar();
                    char* data = read_multiline_input();
                    if (!data) {
                        printf("Ошибка чтения данных\n");
                        break;
                    }

                    if (write_file1(fs, filename, data)) {
                        printf("\nЗапись успешна\n");
                    } else {
                        printf("\nОшибка записи\n");
                    }
                    free(data);
                }
                break;

            case 7:
                if (!fs) fs = open_fs(fs_name);
                if (fs) {
                    printf("Имя файла: ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = '\0';

                    char buffer[1024];
                    if (read_file(fs, filename, buffer, sizeof(buffer))) {
                        printf("Содержимое файла:\n%s\n", buffer);
                    } else {
                        printf("Ошибка чтения\n");
                    }
                }
                break;

            case 8:
                show_help();
                break;

            case 0:
                if (fs) close_fs(fs);
                printf("%sДо свидания!%s\n", CYAN, RESET);
                return 0;

            default:
                printf("%sНеверный выбор. Повторите попытку.%s\n", RED, RESET);
        }

        clearerr(stdin);
    }
}

