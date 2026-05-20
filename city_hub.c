#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void start_monitor()
{
      pid_t hub_mon = fork();

    if (hub_mon < 0)
    {
        perror("fork");
        return;
    }

    if (hub_mon > 0)
    {
        printf("hub_mon created PID=%d\n",
               hub_mon);

        return;
    }

    printf("Inside hub_mon process\n");
    
    int fd[2];
    if (pipe(fd) == -1)
      {
	perror("pipe");
	exit(1);
      }
    pid_t monitor_pid = fork();
    if (monitor_pid == 0)
      {
	dup2(fd[1], STDOUT_FILENO);
	close(fd[0]);
	close(fd[1]);
	execl("./monitor_reports","monitor_reports",NULL);
	perror("execl");
	exit(1);
      }
    close(fd[1]);
    char buffer[256];
    while (1)
      {
	int n = read(fd[0], buffer,sizeof(buffer) - 1);
	if (n <= 0)
	  break;
	buffer[n] = '\0';
	printf("[MONITOR] %s", buffer);
	fflush(stdout);
      }
}

int main()
{
    char line[512];

    while (1)
    {
        printf("city_hub> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "exit") == 0)
            break;

        if (strcmp(line, "start_monitor") == 0)
        {
            start_monitor();
        }
        else
        {
            printf("Unknown command\n");
        }
    }

    return 0;
}
