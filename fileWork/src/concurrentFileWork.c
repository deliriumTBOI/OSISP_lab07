#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>

#define FILENAME "/home/antimagnus/tar_working_dir/Антимонов М.В./lab07/gen_prog/student_records.dat"
#define MAX_RECORDS 100

struct record_s {
    char name[80];
    char address[80];
    uint8_t semester;
};

void list_records(int fd);
struct record_s get_record(int fd, int rec_no);
void put_record(int fd, struct record_s *record, int rec_no);
void lock_record(int fd, int rec_no);
void unlock_record(int fd, int rec_no);
void modify_record(struct record_s *record);

int main() {
    int fd;
    char command[10];
    int rec_no;
    struct record_s rec, rec_wrk, rec_new;
    bool retry_save;
    char save_choice[4] = "YES";
    
    fd = open(FILENAME, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        printf("\nДоступные команды:\n");
        printf("LST - отобразить все записи\n");
        printf("GET <номер> - получить запись по номеру\n");
        printf("MOD - модифицировать последнюю прочитанную запись\n");
        printf("PUT - сохранить последнюю модифицированную запись\n");
        printf("EXIT - выйти из программы\n");
        printf("Введите команду: ");
        
        scanf("%s", command);
        
        if (strcmp(command, "LST") == 0) {
            list_records(fd);
        } else if (strcmp(command, "GET") == 0) {
            scanf("%d", &rec_no);
            rec = get_record(fd, rec_no);
            printf("Запись #%d:\n", rec_no);
            printf("  Имя: %s\n", rec.name);
            printf("  Адрес: %s\n", rec.address);
            printf("  Семестр: %d\n", rec.semester);
        } else if (strcmp(command, "MOD") == 0) {
            rec_wrk = rec;
            modify_record(&rec_wrk);
        } else if (strcmp(command, "PUT") == 0) {
            if (memcmp(&rec_wrk, &rec, sizeof(struct record_s)) == 0) {
                printf("Запись не была изменена.\n");
                continue;
            }
            
            printf("\nТекущие изменения:\n");
            printf("  Имя: %s\n", rec_wrk.name);
            printf("  Адрес: %s\n", rec_wrk.address);
            printf("  Семестр: %d\n", rec_wrk.semester);
            
            retry_save = true;
            
            while (retry_save) {
                lock_record(fd, rec_no);
                rec_new = get_record(fd, rec_no);
                
                if (memcmp(&rec_new, &rec, sizeof(struct record_s)) != 0) {
                    unlock_record(fd, rec_no);
                    printf("Предупреждение: запись была изменена другим процессом!\n");
                    printf("Новое значение записи #%d:\n", rec_no);
                    printf("  Имя: %s\n", rec_new.name);
                    printf("  Адрес: %s\n", rec_new.address);
                    printf("  Семестр: %d\n", rec_new.semester);
                    
                    printf("\nВсё равно сохранить ваши изменения? (YES or press any button and then Enter): ");
                    scanf("%4s", save_choice);
                    
                    if (strcmp(save_choice, "YES") != 0) {
                        printf("Изменения не сохранены.\n");
                        retry_save = false;
                        rec = rec_new;
                    } else {
                        rec = rec_new;
                        printf("\nАвтоматическое повторение попытки сохранения...\n");
                    }
                } else {
                    put_record(fd, &rec_wrk, rec_no);
                    unlock_record(fd, rec_no);
                    rec = rec_wrk;
                    printf("Запись успешно сохранена.\n");
                    retry_save = false;
                }
            }
        } else if (strcmp(command, "EXIT") == 0) {
            break;
        } else {
            printf("Неизвестная команда!\n");
        }
    }
    
    close(fd);
    return 0;
}

void list_records(int fd) {
    struct record_s record;
    off_t file_size;
    int num_records;
    
    file_size = lseek(fd, 0, SEEK_END);
    num_records = file_size / sizeof(struct record_s);
    
    if (num_records == 0) {
        printf("Файл пуст или не содержит записей.\n");
        return;
    }
    
    lseek(fd, 0, SEEK_SET);
    
    printf("Список всех записей (%d):\n", num_records);
    for (int i = 0; i < num_records; i++) {
        read(fd, &record, sizeof(struct record_s));
        printf("Запись #%d:\n", i+1);
        printf("  Имя: %s\n", record.name);
        printf("  Адрес: %s\n", record.address);
        printf("  Семестр: %d\n", record.semester);
        printf("\n");
    }
}

struct record_s get_record(int fd, int rec_no) {
    struct record_s record;
    
    if (rec_no < 1) {
        printf("Ошибка: номер записи должен быть положительным числом.\n");
        memset(&record, 0, sizeof(struct record_s));
        return record;
    }
    
    off_t offset = (rec_no - 1) * sizeof(struct record_s);
    lseek(fd, offset, SEEK_SET);
    
    ssize_t bytes_read = read(fd, &record, sizeof(struct record_s));
    if (bytes_read != sizeof(struct record_s)) {
        printf("Ошибка: запись с номером %d не существует.\n", rec_no);
        memset(&record, 0, sizeof(struct record_s));
    }
    
    return record;
}

void put_record(int fd, struct record_s *record, int rec_no) {
    off_t offset = (rec_no - 1) * sizeof(struct record_s);
    lseek(fd, offset, SEEK_SET);
    write(fd, record, sizeof(struct record_s));
}

void lock_record(int fd, int rec_no) {
    struct flock lock;
    off_t offset = (rec_no - 1) * sizeof(struct record_s);
    
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(struct record_s);
    
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Ошибка блокировки");
        exit(EXIT_FAILURE);
    }
}

void unlock_record(int fd, int rec_no) {
    struct flock lock;
    off_t offset = (rec_no - 1) * sizeof(struct record_s);
    
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(struct record_s);
    
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("Ошибка разблокировки");
        exit(EXIT_FAILURE);
    }
}

void modify_record(struct record_s *record) {
    char buffer[81];
    int semester;
    
    printf("Введите новые данные (нажмите Enter, чтобы оставить текущее значение):\n");
    
    printf("Имя [%s]: ", record->name);
    getchar();
    if (fgets(buffer, sizeof(buffer), stdin) != NULL && buffer[0] != '\n') {
        buffer[strcspn(buffer, "\n")] = 0;
        strncpy(record->name, buffer, sizeof(record->name) - 1);
        record->name[sizeof(record->name) - 1] = '\0';
    }
    
    printf("Адрес [%s]: ", record->address);
    if (fgets(buffer, sizeof(buffer), stdin) != NULL && buffer[0] != '\n') {
        buffer[strcspn(buffer, "\n")] = 0;
        strncpy(record->address, buffer, sizeof(record->address) - 1);
        record->address[sizeof(record->address) - 1] = '\0';
    }
    
    printf("Семестр [%d]: ", record->semester);
    if (fgets(buffer, sizeof(buffer), stdin) != NULL && buffer[0] != '\n') {
        semester = atoi(buffer);
        if (semester > 0 && semester <= 12) {
            record->semester = (uint8_t)semester;
        } else {
            printf("Ошибка: семестр должен быть от 1 до 12. Оставлено текущее значение.\n");
        }
    }
}
