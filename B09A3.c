/* CSCB09 Assignment 3
Author: Adam Skinner
Date: 04/04/2024 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "stats_functions.h"

int handler(int code, sig_a *pids) {
	static sig_a* saved = NULL;
	if (saved == NULL) saved = pids;

	if (code == SIGINT) {
		// Ask user if they want to quit
		char response;
		printf("Are you sure you want to quit? (y/n) ");
		scanf(" %c", &response);

		// Clear input buffer
	    int c;
	    while ((c = getchar()) != '\n' && c != EOF) {}

		if (response == 'y') { // If yes, end program & children
			if(pids->pid1 != -1) kill(pids->pid1, SIGTERM);
			if(pids->pid2 != -1) kill(pids->pid2, SIGTERM);
			if(pids->pid3 != -1) kill(pids->pid3, SIGTERM);
			free(pids);
			exit(0);
		} else { // If no, clear line and continue
			printf("\033[2K\r");
		}
	}
	return 0;
}

// Registers the signal handler for the parent process
void register_handler(sig_a* pids) {
	struct sigaction newact;

	newact.sa_handler = (void (*)(int)) handler;
	newact.sa_flags = 0;
	sigemptyset(&newact.sa_mask);
	if (sigaction(SIGINT, &newact, NULL) == -1) perror("sigaction");
	handler(SIGTSTP, pids); // This assigns the static "saved" variable in the handler to child PIDs
							// i.e, this acts as a "dummy signal" to initialize the static variable
}

void printSystemInfo() {
	struct utsname system;
	struct tm info;
	if (uname(&system) < 0) perror("uname");
	printf("---------------------------------------\n");
	printf("### System Information ###\n");
	printf(" System Name = %s\n", system.sysname);
	printf(" Machine Name = %s\n", system.nodename);
	printf(" Version = %s\n", system.version);
	printf(" Release = %s\n", system.release);
	printf(" Architecture = %s\n", system.machine);

	FILE* uptime = fopen("/proc/uptime", "r"); // Open /proc/uptime
	if (uptime == NULL) {
		perror("fopen");
		return;
	}

	// Convert uptime stored in /proc/uptime into a human-readable format using time library
	float seconds;
	fscanf(uptime, "%f", &seconds);
	int days = (int)(seconds/(60 * 60 * 24));
	int tot_hours = (int)(seconds/(60 * 60));
	seconds -= days * (60 * 60 * 24);
	int hours = (int)(seconds/(60 * 60));
	seconds -= hours * (60 * 60);
	int minutes = (int)(seconds / 60);
	seconds -= minutes * 60;
	int int_seconds = ((int)(seconds*100 + 0.5))/100;
	fclose(uptime);

	info.tm_sec = int_seconds;
	info.tm_min = minutes;
	info.tm_hour = hours;

	// Display uptime in days, hours, minutes, seconds & hours, minutes, seconds
	char long_time[10];
	strftime(long_time, 10, "%H:%M:%S", &info);
	char short_time[7];
	strftime(short_time, 7, "%M:%S", &info);

	printf(" System running since last reboot: %d days %s (%d:%s)\n", days, long_time, tot_hours, short_time);
	printf("---------------------------------------\n");
}

int runHandleCPU(int *fd_read, int samples, int delay) {
	int r = fork();
	if (r < 0) {
		perror("fork");
		exit(1);
	}

	if (r == 0) {
		signal(SIGINT, SIG_IGN);
		handleCPU(fd_read, samples, delay);
	}
	return r;
}

int runPrintUsers(int *fd_users, int *fd_users_write, int samples) {
	int r = fork();
	if (r < 0) {
		perror("fork");
		exit(1);
	}

	if (r == 0) {
		signal(SIGINT, SIG_IGN);
		printUsers(fd_users, fd_users_write, samples);
	}
	return r;
}

int runHandleMem(int *fd_read, char memArr[][STRLEN], int graphics, int samples, int delay) {
	int r = fork();
	if (r < 0) {
		perror("fork");
		exit(1);
	}

	if (r == 0) {
		signal(SIGINT, SIG_IGN);
		handleMem(fd_read, memArr, graphics, samples, delay);
	}
	return r;
}

void printUsage(struct rusage *usage_self, struct rusage *usage_children) {
	printf(" Memory usage: %ld KB\n", usage_self->ru_maxrss + usage_children->ru_maxrss);
	printf("---------------------------------------\n");
}

int main(int argc, char **argv) {
	int samples = 10;
	int delay = 1;
	int samples_changed = 0;
	long n_cores;

	int systemOnly = 0;
	int userOnly = 0;
	int graphics = 0;
	int sequential = 0;

	float cpu_usage;

	int users_pid = -1;
	int mem_pid = -1;
	int cpu_pid = -1;
	int indicator = 1;

	signal(SIGTSTP, SIG_IGN); // Ignore Ctrl + Z

	// Code to handle positional arguments
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' && !samples_changed) {
			samples = strtol(argv[i], NULL, 10); // If the argument is not a flag, then first is the nbr of samples
			samples_changed = 1;
		} else if (argv[i][0] != '-'){
			delay = strtol(argv[i], NULL, 10); // Next pos. argument must be tdelay
		}
	}

	// Code to handle flags (using getopt_long)
	struct option opts[] = 
	{
		{"system", 0, 0, 's'},
		{"user", 0, 0, 'u'},
		{"graphics", 0, 0, 'g'},
		{"sequential", 0, 0, 'q'},
		{"samples", required_argument, 0, 'm'},
		{"tdelay", required_argument, 0, 'd'},
		{0,0,0,0}
	};
	int opt_val = 0;
	int opt_index = 0;

	while ((opt_val = getopt_long(argc, argv, "sugqm:d::", opts, &opt_index)) != -1) {
		switch (opt_val)
		{
		case 's':
			systemOnly = 1;
			break;
		case 'u':
			userOnly = 1;
			break;
		case 'g':
			graphics = 1;
			break;
		case 'q':
			sequential = 1;
			break;
		case 'm':
			samples = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case '?':
			break;
		}
	}
	
	if (systemOnly && userOnly) {
		systemOnly = 0;
		userOnly = 0; // Correct the user input if both flags set to 1
	}

	// System usage struct declaration
	struct rusage usage_self, usage_children;
	int half = (delay * 1000000) / 2; // Half the delay in microseconds (for usleep())

	// Create array of strings to store and display used memory
	char memArr[samples][STRLEN]; // All strings should be less than STRLEN characters
	char cpuGraphics[samples][STRLEN]; // Stores CPU graphical information

	for (int i = 0; i < samples; i++) {
		memArr[i][0] = '\0';
		if (graphics) { // If graphics are selected, create string array to display CPU usage
			cpuGraphics[i][0] = '\0';
		}
	}

	// File descriptors for inter-process communication for memory usage
	int fd_read[2];

	if (pipe(fd_read) < 0) {
		perror("pipe");
		exit(1);
	}

	// File descriptors for users process
	int fd_users[2];
	int fd_users_write[2];

	if (pipe(fd_users) < 0 || pipe(fd_users_write) < 0) {
		perror("pipe");
		exit(1);
	}

	// File descriptors for CPU process
	int fd_read_cpu[2];

	if (pipe(fd_read_cpu) < 0) {
		perror("pipe");
		exit(1);
	}

	// Start the child processes
	if (!userOnly) {
		mem_pid = runHandleMem(fd_read, memArr, graphics, samples, delay);
	}

	if (!systemOnly) {
		users_pid = runPrintUsers(fd_users_write, fd_users, samples);
	}

	if (!userOnly) {
		cpu_pid = runHandleCPU(fd_read_cpu, samples, delay);
		read(fd_read_cpu[0], &n_cores, sizeof(long)); // Get the # of cores
	}

	// Close unused ends of pipe for parent process
	close(fd_read[1]);
	close(fd_users_write[0]);
	close(fd_users[1]);
	close(fd_read_cpu[1]);

	// Get child PIDs and pass them to the signal handler registration function
	sig_a *c_pids = (sig_a*) calloc(1, sizeof(sig_a));
	c_pids->pid1 = mem_pid;
	c_pids->pid2 = users_pid;
	c_pids->pid3 = cpu_pid;
	register_handler(c_pids);

	// Main program loop
	printf("Nbr of samples: %d -- every %d secs\n", samples, delay);
	for (int i = 0; i < samples; i++) {
		if (!sequential && i != 0) {
			printf("Nbr of samples: %d -- every %d secs\n", samples, delay);
		} else if (sequential) {
			printf(">>> iteration %d\n", i);
		}

		// Get program memory used incl. children
		if (getrusage(RUSAGE_SELF, &usage_self) < 0) perror("rusage");
		if (getrusage(RUSAGE_CHILDREN, &usage_children) < 0) perror("rusage");
		printUsage(&usage_self, &usage_children);

		if (!userOnly) {
			// Memory done, print
			read(fd_read[0], &memArr[i], STRLEN);
			printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");
			printStringArray(memArr, samples);
			printf("---------------------------------------\n");
		}

		if (!systemOnly) {
			// Let users print then hang until complete
			if (write(fd_users_write[1], &indicator, sizeof(int)) <= 0) perror("write");
			read(fd_users[0], &indicator, sizeof(int));
		}

		if (!userOnly){
			// CPU done, print
			read(fd_read_cpu[0], &cpu_usage, sizeof(float));
			printf("Number of cores: %ld\n", n_cores);
			printf(" total cpu use = %.2f%%\n", cpu_usage);
			if (graphics) { // Update CPU graphics (if selected)
				printf("\033[0J");
				updateCPUGraphics(cpuGraphics[i], cpu_usage);
				printStringArray(cpuGraphics, samples);
			}
		}


		fflush(stdout); // Ensure everything is printed to the terminal before sleep() is called
		if (i == 0 && !userOnly) {
			long_sleep(half);
		} else {
			sleep(delay);
		}
		
		if (!sequential && i != samples - 1) { // Clear screen if sequential isn't selected and isn't last iteration
			printf("\033[H\033[J");
		}

		if (sequential && i != samples - 1) { // Print new line and "clear" previous memory usage so there's only one per iteration if sequential
			memArr[i][0] = '\0';
			printf("\n");
		}
	}

	close(fd_read[0]);
	close(fd_users[0]);
	close(fd_users_write[1]);
	close(fd_read_cpu[0]);

	printSystemInfo();

	return 0;
}