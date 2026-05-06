#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define PID_FILE ".monitor_pid"

void handle_sigusr1(int sig) {
    const char *msg = "Signal received: New report added\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_sigint(int sig) {
    const char *msg = "Monitor shutting down (SIGINT received)\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    unlink(PID_FILE); // delete file

    exit(0);
}

int main() {
    // 1. Write PID to file
    FILE *f = fopen(PID_FILE, "w");
    if (!f) {
        perror("Error creating .monitor_pid");
        return 1;
    }

    fprintf(f, "%d\n", getpid());
    fclose(f);

    printf("Monitor started. PID = %d\n", getpid());

    // 2. Set up signal handlers
    struct sigaction sa_usr1, sa_int;

    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    // 3. Infinite wait loop
    while (1) {
        pause(); // wait for signals
    }

    return 0;
}
