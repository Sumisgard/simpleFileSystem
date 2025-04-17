#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myfs.h"

void show_menu() {
    printf("\n--- MYFS MENU ---\n");
    printf("1. Форматировать ФС\n");
    printf("2. Создать файл\n");
    printf("3. Показать файлы\n");
    printf("4. Удалить файл\n");
    printf("0. Выход\n");
    printf("Выбор: ");
}

int main() {
    const char* fs_name = "disk.img";
    FILE* fs = NULL;
    int choice;
    char filename[256];

    while (1) {
        show_menu();
        if (scanf("%d", &choice) != 1) {
            printf("Ошибка ввода\n");
            while (getchar() != '\n'); // Очистка буфера
            continue;
        }
        getchar(); // Убираем \n

        switch (choice) {
            case 1:
                if (format_fs(fs_name)) {
                    printf("ФС успешно отформатирована\n");
                } else {
                    printf("Ошибка форматирования\n");
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
                
            case 0:
                if (fs) close_fs(fs);
                return 0;
                
            default:
                printf("Неверный выбор\n");
        }
    }
}
