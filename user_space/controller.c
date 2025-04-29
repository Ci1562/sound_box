#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>

#define DEVICE_PATH "/dev/led"

void voice_btn(int fd, struct pollfd fds, char *buf, int pid){
	int button, led;
	int voice_running = 1;
	while(voice_running){
		int ret = poll(&fds, 1, 500);
		if (ret > 0 && (fds.revents & POLLIN)){
			memset(buf, 0, sizeof(buf));
			read(fd, buf, sizeof(buf));
			
			if ((sscanf(buf, "button:%d\nled:%d\n", &button, &led) == 2) && button == 1){
			kill(pid, SIGTERM);
			waitpid(pid, NULL, 0);
			system("mpc stop");
			system("mpc clear");
			system("killall -q ssd1306_API");
			write(fd, "cancel", strlen("cancel"));
			voice_running = 0;
			break;
			}
		}

		int status;
		pid_t voice_result = waitpid(pid, &status, WNOHANG);
		if (voice_result == pid) {
			write(fd, "reset", strlen("reset"));
			voice_running = 0;
			break;
		}
	}

	waitpid(pid, NULL, 0);
	sleep(2);
	system("mpc play");
}

int main(){
	int fd;
	char buf[64] = {0};
	int button, led;
	int pid;
	struct pollfd fds;

	fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0){
		perror("Failed to open");
		return 1;
	}

	fds.fd = fd;
	fds.events = POLLIN;
	printf("Listening...\n");

	while(1){
		int ret = poll(&fds, 1, -1);
		if (ret < 0){
			perror("poll");
			break;
		}

		if (fds.revents & POLLIN) {
			//read
			memset(buf, 0, sizeof(buf));
			read(fd, buf, sizeof(buf));

			if (sscanf(buf, "button:%d\nled:%d\n", &button, &led) == 2){
				//write
				char led_cmd[16];
				snprintf(led_cmd, sizeof(led_cmd), "led=%d", led);

				if (write(fd, led_cmd, strlen(led_cmd)) < 0){
					perror("Failed to write");
					close(fd);
					return 1;
				}
			} else {
				fprintf(stderr, "Failed to parse read data\n");
			}
		}

		switch (led){
			case 1:
				system("python3 /home/cii/max98357a.py");
				break;
			case 2:
				system("mpc pause");
				pid = fork();
				switch (pid){
					case -1:
						perror("Fork failed");
						return -1;
					case 0:
						system("/home/cii/snd2txt/ssd1306_API/ssd1306_API");
						exit(0);
					default:
						voice_btn(fd, fds, buf, pid);
						break;
				}
				break;
			default:

				break;
		}

	}

	close(fd);
	return 0;
}

