#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>

#define NAME_LEN 32
#define CATEGORY_LEN 32
#define DESC_LEN 128

typedef struct {
    int id;
    char inspector[NAME_LEN];
    float latitude;
    float longitude;
    char category[CATEGORY_LEN];
    int severity;
    time_t timestamp;
    char description[DESC_LEN];
} Report;

void check_symlinks() {
    DIR *dir = opendir(".");
    if (!dir) return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "active_reports-", 15) == 0) {

            struct stat st;

            if (lstat(entry->d_name, &st) == -1) {
                continue;
            }

            if (S_ISLNK(st.st_mode)) {
                char target[200];
                int len = readlink(entry->d_name, target, sizeof(target) - 1);

                if (len != -1) {
                    target[len] = '\0';

                    if (access(target, F_OK) == -1) {
                        printf("Warning: dangling symlink %s -> %s\n", entry->d_name, target);
                    }
                }
            }
        }
    }

    closedir(dir);
}


int has_permission(const char *path, const char *role, int permission) {
    struct stat st;

    if (stat(path, &st) == -1) {
        perror("stat error");
        return 0;
    }

    if (strcmp(role, "manager") == 0) {
        return (st.st_mode & permission);
    } else {
        return (st.st_mode & (permission >> 3));
    }
}

void print_permissions(mode_t mode) {
    char perms[10];

    perms[0] = (mode & S_IRUSR) ? 'r' : '-';
    perms[1] = (mode & S_IWUSR) ? 'w' : '-';
    perms[2] = (mode & S_IXUSR) ? 'x' : '-';

    perms[3] = (mode & S_IRGRP) ? 'r' : '-';
    perms[4] = (mode & S_IWGRP) ? 'w' : '-';
    perms[5] = (mode & S_IXGRP) ? 'x' : '-';

    perms[6] = (mode & S_IROTH) ? 'r' : '-';
    perms[7] = (mode & S_IWOTH) ? 'w' : '-';
    perms[8] = (mode & S_IXOTH) ? 'x' : '-';

    perms[9] = '\0';

    printf("Permissions: %s\n", perms);
}

void create_district(const char *name) {
    mkdir("districts", 0750);
    chmod("districts", 0750);

    char full_path[100];

    strcpy(full_path, "districts/");
    strcat(full_path, name);

    if (mkdir(full_path, 0750) == -1) {
        if (errno != EEXIST) {
            perror("Error creating directory");
            return;
        }
    }

    chmod(full_path, 0750);

    char reports_path[150];
    strcpy(reports_path, full_path);
    strcat(reports_path, "/reports.dat");

    int fd1 = open(reports_path, O_CREAT | O_RDWR, 0664);
    if (fd1 == -1) {
        perror("Error creating reports.dat");
        return;
    }
    close(fd1);
    chmod(reports_path, 0664);

    char cfg_path[150];
    strcpy(cfg_path, full_path);
    strcat(cfg_path, "/district.cfg");

    int fd2 = open(cfg_path, O_CREAT | O_RDWR, 0640);
    if (fd2 == -1) {
        perror("Error creating district.cfg");
        return;
    }
    close(fd2);
    chmod(cfg_path, 0640);

    char log_path[150];
    strcpy(log_path, full_path);
    strcat(log_path, "/logged_district");

    int fd3 = open(log_path, O_CREAT | O_RDWR, 0644);
    if (fd3 == -1) {
        perror("Error creating logged_district");
        return;
    }
    close(fd3);
    chmod(log_path, 0644);
}

void log_action(const char *district, const char *role, const char *user, const char *action) {
    char path[150];

    strcpy(path, "districts/");
    strcat(path, district);
    strcat(path, "/logged_district");

    int fd = open(path, O_WRONLY | O_APPEND);

    if (fd == -1) {
        perror("Error opening log file");
        return;
    }

    time_t now = time(NULL);

    dprintf(fd, "[%ld] %s %s %s\n", now, role, user, action);

    close(fd);
}


int notify_monitor(void) {
    int fd = open(".monitor_pid", O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    char buffer[32];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes <= 0) {
        return -1;
    }

    buffer[bytes] = '\0';
    pid_t pid = (pid_t)atoi(buffer);

    if (pid <= 0) {
        return -1;
    }

    if (kill(pid, SIGUSR1) == -1) {
        return -1;
    }

    return 0;
}

void add_report(const char *district, const char *user, const char *role) {
    char path[150];

    strcpy(path, "districts/");
    strcat(path, district);
    strcat(path, "/reports.dat");

    if (!has_permission(path, role, S_IWUSR)) {
        printf("Write access denied for this role!\n");
        return;
    }

    int fd = open(path, O_WRONLY | O_APPEND);

    if (fd == -1) {
        perror("Error opening reports file");
        return;
    }

    struct stat st;

    if (stat(path, &st) == -1) {
        perror("stat error");
        close(fd);
        return;
    }

    int count = st.st_size / sizeof(Report);
    int next_id = count + 1;

    Report r;

    r.id = next_id;
    strncpy(r.inspector, user, NAME_LEN - 1);
    r.inspector[NAME_LEN - 1] = '\0';

   printf("Enter latitude: ");
scanf("%f", &r.latitude);

printf("Enter longitude: ");
scanf("%f", &r.longitude);

printf("Enter category: ");
scanf("%s", r.category);

printf("Enter severity (1-3): ");
scanf("%d", &r.severity);

getchar();

printf("Enter description: ");
fgets(r.description, DESC_LEN, stdin);

r.description[strcspn(r.description, "\n")] = '\0';

r.timestamp = time(NULL);

    if (write(fd, &r, sizeof(Report)) != sizeof(Report)) {
        perror("Error writing report");
        close(fd);
        return;
    }

    close(fd);

    printf("Report added with ID %d!\n", r.id);

    if (notify_monitor() == 0) {
        log_action(district, role, user, "monitor informed about new report");
    } else {
        log_action(district, role, user, "monitor could not be informed about new report");
    }
}

void list_reports(const char *district, const char *role) {
    char path[150];

    strcpy(path, "districts/");
    strcat(path, district);
    strcat(path, "/reports.dat");

    struct stat st;

    if (stat(path, &st) == -1) {
        perror("stat error");
        return;
    }

    if (!has_permission(path, role, S_IRUSR)) {
        printf("Read access denied for this role!\n");
        return;
    }

    int fd = open(path, O_RDONLY);

    if (fd == -1) {
        perror("Error opening reports file");
        return;
    }

    printf("File size: %ld bytes\n", st.st_size);
    printf("Last modified: %s", ctime(&st.st_mtime));
    print_permissions(st.st_mode);

    Report r;

    printf("---- Reports ----\n");

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        printf("ID: %d | Inspector: %s | Category: %s | Severity: %d\n",
               r.id, r.inspector, r.category, r.severity);
    }

    close(fd);
}

void view_report(const char *district, const char *role, int id) {
    char path[150];

    strcpy(path, "districts/");
    strcat(path, district);
    strcat(path, "/reports.dat");

    if (!has_permission(path, role, S_IRUSR)) {
        printf("Read access denied for this role!\n");
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening reports file");
        return;
    }

    Report r;
    int found = 0;

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == id) {
            printf("---- Report Details ----\n");
            printf("ID: %d\n", r.id);
            printf("Inspector: %s\n", r.inspector);
            printf("Latitude: %.2f\n", r.latitude);
            printf("Longitude: %.2f\n", r.longitude);
            printf("Category: %s\n", r.category);
            printf("Severity: %d\n", r.severity);
            printf("Timestamp: %s", ctime(&r.timestamp));
            printf("Description: %s\n", r.description);

            found = 1;
            break;
        }
    }

    if (!found) {
        printf("Report with ID %d not found.\n", id);
    }

    close(fd);
}

void remove_report(const char *district, const char *role, int id) {
    if (strcmp(role, "manager") != 0) {
        printf("Only manager can remove reports!\n");
        return;
    }

    char path[150];
    strcpy(path, "districts/");
    strcat(path, district);
    strcat(path, "/reports.dat");

    if (!has_permission(path, role, S_IWUSR)) {
        printf("Write access denied for this role!\n");
        return;
    }

    int fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("Error opening reports file");
        return;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        perror("stat error");
        close(fd);
        return;
    }

    int total = st.st_size / sizeof(Report);
    int found_index = -1;
    Report r;

    for (int i = 0; i < total; i++) {
        if (read(fd, &r, sizeof(Report)) != sizeof(Report)) {
            break;
        }

        if (r.id == id) {
            found_index = i;
            break;
        }
    }

    if (found_index == -1) {
        printf("Report with ID %d not found.\n", id);
        close(fd);
        return;
    }

    for (int i = found_index; i < total - 1; i++) {
        lseek(fd, (i + 1) * sizeof(Report), SEEK_SET);
        read(fd, &r, sizeof(Report));

        lseek(fd, i * sizeof(Report), SEEK_SET);
        write(fd, &r, sizeof(Report));
    }

    if (ftruncate(fd, (total - 1) * sizeof(Report)) == -1) {
        perror("Error truncating file");
        close(fd);
        return;
    }

    close(fd);

    printf("Report with ID %d removed successfully!\n", id);
}

void update_threshold(const char *district, const char *role, int value) {
    if (strcmp(role, "manager") != 0) {
        printf("Only manager can update threshold!\n");
        return;
    }

    char path[150];
    strcpy(path, "districts/");
    strcat(path, district);
    strcat(path, "/district.cfg");

    struct stat st;
    if (stat(path, &st) == -1) {
        perror("stat error");
        return;
    }

    if ((st.st_mode & 0777) != 0640) {
        printf("Permission error: district.cfg must be 640!\n");
        return;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd == -1) {
        perror("Error opening config file");
        return;
    }

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%d\n", value);

    write(fd, buffer, strlen(buffer));

    close(fd);

    printf("Threshold updated to %d\n", value);
}

int parse_condition(const char *input, char *field, char *op, char *value) {
    char temp[100];
    strcpy(temp, input);

    char *part1 = strtok(temp, ":");
    char *part2 = strtok(NULL, ":");
    char *part3 = strtok(NULL, ":");

    if (part1 == NULL || part2 == NULL || part3 == NULL) {
        return 0;
    }

    strcpy(field, part1);
    strcpy(op, part2);
    strcpy(value, part3);

    return 1;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int v = atoi(value);

        if (strcmp(op, "==") == 0) return r->severity == v;
        if (strcmp(op, "!=") == 0) return r->severity != v;
        if (strcmp(op, "<") == 0) return r->severity < v;
        if (strcmp(op, "<=") == 0) return r->severity <= v;
        if (strcmp(op, ">") == 0) return r->severity > v;
        if (strcmp(op, ">=") == 0) return r->severity >= v;
    }

    if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->category, value) != 0;
    }

    if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->inspector, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->inspector, value) != 0;
    }

    if (strcmp(field, "timestamp") == 0) {
        time_t v = (time_t)atol(value);

        if (strcmp(op, "==") == 0) return r->timestamp == v;
        if (strcmp(op, "!=") == 0) return r->timestamp != v;
        if (strcmp(op, "<") == 0) return r->timestamp < v;
        if (strcmp(op, "<=") == 0) return r->timestamp <= v;
        if (strcmp(op, ">") == 0) return r->timestamp > v;
        if (strcmp(op, ">=") == 0) return r->timestamp >= v;
    }

    return 0;
}

void filter_reports(const char *district, const char *role, int condition_count, char *conditions[]) {
    char path[150];

    strcpy(path, "districts/");
    strcat(path, district);
    strcat(path, "/reports.dat");

    if (!has_permission(path, role, S_IRUSR)) {
        printf("Read access denied for this role!\n");
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening reports file");
        return;
    }

    Report r;
    int any_found = 0;

    printf("---- Filtered Reports ----\n");

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int matches_all = 1;

        for (int i = 0; i < condition_count; i++) {
            char field[32], op[4], value[64];

            if (!parse_condition(conditions[i], field, op, value)) {
                printf("Invalid condition: %s\n", conditions[i]);
                close(fd);
                return;
            }

            if (!match_condition(&r, field, op, value)) {
                matches_all = 0;
                break;
            }
        }

        if (matches_all) {
            printf("ID: %d | Inspector: %s | Category: %s | Severity: %d | Description: %s\n",
                   r.id, r.inspector, r.category, r.severity, r.description);
            any_found = 1;
        }
    }

    if (!any_found) {
        printf("No reports matched the condition(s).\n");
    }

    close(fd);
}

void remove_district(const char *district, const char *role) {
    if (strcmp(role, "manager") != 0) {
        printf("Only manager can remove districts!\n");
        return;
    }

    char district_path[150];
    char linkname[100];

    strcpy(district_path, "districts/");
    strcat(district_path, district);

    strcpy(linkname, "active_reports-");
    strcat(linkname, district);

    /*
       safety check:
       do not allow empty district names or paths containing ..
       this prevents dangerous rm -rf usage
    */
    if (strlen(district) == 0 || strstr(district, "..") != NULL || strchr(district, '/') != NULL) {
        printf("Invalid district name. Refusing to delete.\n");
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork error");
        return;
    }

    if (pid == 0) {
        execlp("rm", "rm", "-rf", district_path, NULL);

        perror("execlp error");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            unlink(linkname);
            printf("District '%s' removed successfully.\n", district);
        } else {
            printf("Error removing district '%s'.\n", district);
        }
    }
}
void create_symlink(const char *district) {
    char target[150];
    char linkname[100];

    strcpy(target, "districts/");
    strcat(target, district);
    strcat(target, "/reports.dat");

    strcpy(linkname, "active_reports-");
    strcat(linkname, district);

    unlink(linkname);

    if (symlink(target, linkname) == -1) {
        perror("Error creating symlink");
        return;
    }

    printf("Symlink created: %s -> %s\n", linkname, target);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Usage:\n");
        printf("./city_manager --role <role> --user <name> --add <district>\n");
        printf("./city_manager --role <role> --user <name> --list <district>\n");
        return 1;
    }
    check_symlinks();

    char role[20] = "";
    char user[50] = "";
    char district[50] = "";

    int add_flag = 0;
    int list_flag = 0;
    int view_flag = 0;
    int view_id = 0;
    int remove_flag = 0;
    int remove_id = 0;
    int update_flag = 0;
    int threshold_value = 0;
    int filter_flag = 0;
    int filter_start_index = 0;
    int remove_district_flag = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strcpy(role, argv[i + 1]);
        }

        if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strcpy(user, argv[i + 1]);
        }

        if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            strcpy(district, argv[i + 1]);
            add_flag = 1;
        }

        if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) {
            strcpy(district, argv[i + 1]);
            list_flag = 1;
        }
	if (strcmp(argv[i], "--view") == 0 && i + 2 < argc) {
    strcpy(district, argv[i + 1]);
    view_id = atoi(argv[i + 2]);
    view_flag = 1;
}
	if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) {
    strcpy(district, argv[i + 1]);
    remove_id = atoi(argv[i + 2]);
    remove_flag = 1;
}
	if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) {
    strcpy(district, argv[i + 1]);
    threshold_value = atoi(argv[i + 2]);
    update_flag = 1;
}
	if (strcmp(argv[i], "--filter") == 0 && i + 2 < argc) {
    strcpy(district, argv[i + 1]);
    filter_start_index = i + 2;
    filter_flag = 1;
    break;

}
	    if (strcmp(argv[i], "--remove_district") == 0 && i + 1 < argc) {
    strcpy(district, argv[i + 1]);
    remove_district_flag = 1;
}
    }

    if (add_flag) {
        create_district(district);
        add_report(district, user, role);
	create_symlink(district);

        char action[100] = "added report in ";
        strcat(action, district);
        log_action(district, role, user, action);
    } else if (list_flag) {
        list_reports(district, role);
    }else if (view_flag) {
    view_report(district, role, view_id);
}
    else if (remove_flag) {
    remove_report(district, role, remove_id);
}
    else if (update_flag) {
    update_threshold(district, role, threshold_value);
}
    else if (filter_flag) {
    filter_reports(district, role, argc - filter_start_index, &argv[filter_start_index]);
}
    else if (remove_district_flag) {
    remove_district(district, role);
}
    else {
        printf("No valid command provided.\n");
    }

    return 0;
}
