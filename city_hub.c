#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#define PID_FILE ".monitor_pid"

static volatile sig_atomic_t running = 1;

void handle_sigusr1(int sig) {
    (void)sig;
    const char *msg = "Monitor: new report added (SIGUSR1 received)\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_sigint(int sig) {
    (void)sig;
    const char *msg = "Monitor: SIGINT received, shutting down\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    running = 0;
}

int main(void) {
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error creating .monitor_pid");
        return 1;
    }

    char pid_buffer[32];
    int len = snprintf(pid_buffer, sizeof(pid_buffer), "%ld\n", (long)getpid());
    if (write(fd, pid_buffer, len) != len) {
        perror("Error writing .monitor_pid");
        close(fd);
        unlink(PID_FILE);
        return 1;
    }
    close(fd);

    printf("Monitor started. PID = %ld\n", (long)getpid());
    fflush(stdout);

    struct sigaction sa_usr1;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);

    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("sigaction SIGUSR1");
        unlink(PID_FILE);
        return 1;
    }

    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT");
        unlink(PID_FILE);
        return 1;
    }

    while (running) {
        pause();
    }

    unlink(PID_FILE);
    return 0;
}
