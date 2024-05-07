#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <utmp.h>
#include <sys/utsname.h>
#include <math.h>
#include "stats_functions.h"

/* Helpers */
void long_sleep(int micros)
{
	int chunk;
	while (micros > 0) {
		if (micros < 1000000) {
			usleep(micros);
			break;
		}
		chunk = 500000;
		usleep(chunk);
		micros -= chunk;
	}
}

float myRound(float f) {
	if (f >= 0) return (float)((int)(f*100+0.5))/100;
	return (float)((int)(f*100-0.5))/100;
	
}

void appendChars(char *str, char c, int num) {
	char char_str[2] = {c, '\0'};
	for (int i = 0; i < num; i++) {
		strncat(str, char_str, 1);
	}
}

void updateCPUGraphics(char *str, float usage) {
	strcpy(str, "         |||"); // Number of bars equivalent to (rounded) percentage + 3
	appendChars(str, '|', (int) usage);
	snprintf(str + strlen(str), STRLEN, " %.2f", usage); // Add usage % to end of graphic
}

void printStringArray(char arr[][STRLEN], int length) {
	for (int i = 0; i < length; i++) {
		printf("%s\n", arr[i]);
	}
}

void updateMemoryGraphics(char *str, float mem_before, float mem_after) {
	float diff = myRound(mem_after) - myRound(mem_before);
	int size;
	if (diff == 0) {
		strncat(str, "   |o 0.00", 20); // No change in memory usage
	} else { // Add characters to graphics corresponding to the guidelines if difference is positve or negative
		strncat(str, "   |", 20);
		size = abs((int)(100 * diff));
		if (diff >= 0) {
			appendChars(str, '#', size);
			strncat(str, "* ", 4);
		} else {
			appendChars(str, ':', size);
			strncat(str, "@ ", 4);
		}
		snprintf(str + strlen(str), STRLEN, "%.2f", fabs(myRound(diff))); // Add to end of memory string
	}
	snprintf(str + strlen(str), STRLEN, " (%.2f) ", mem_after);
}

cpu_u getCPUUsage_first() {
	cpu_u output;
	output.usage = 0.0;
	output.total = 0.0;
	FILE* cpustats = fopen("/proc/stat", "r"); // Open /proc/stat
	if (cpustats == NULL) {
		perror("fopen");
		return output;
	}

	char str[100]; // Upper bound for length of line in /proc/stat
	fgets(str, 100, cpustats);
	char* token;
	token = strtok(str, " "); // Use strtok to read each value
	token = strtok(NULL, " ");
	fclose(cpustats);

	int i = 0;
	unsigned long total_time = 0;
	unsigned long idle_time = 0;

	while (token != NULL && i <= 6) { // Determine total usage & idle usage
		total_time += strtol(token, NULL, 10);
		if (i == 3) {
			idle_time = strtol(token, NULL, 10);
		}
		token = strtok(NULL, " ");
		i++;
	}

	output.usage = (float) (total_time - idle_time);
	output.total = (float) total_time;
	return output;
}

float getCPUUsage_second(cpu_u first_usage) {
	FILE* cpustats = fopen("/proc/stat", "r"); // Open /proc/stat again for 2nd reading
	if (cpustats == NULL) {
		perror("fopen");
		return 0.0;
	}

	char str[100]; // Upper bound for length of line in /proc/stat
	fgets(str, 100, cpustats);
	fclose(cpustats);

	char* token;
	token = strtok(str, " ");
	token = strtok(NULL, " ");

	int i = 0;
	unsigned long total_time = 0;
	unsigned long idle_time = 0;

	while (token != NULL && i <= 6) {
		total_time += strtol(token, NULL, 10);
		if (i == 3) {
			idle_time = strtol(token, NULL, 10);
		}
		token = strtok(NULL, " ");
		i++;
	}

	float usage_diff = total_time - idle_time;
	// Calculate CPU usage
	float delta_usage = usage_diff - first_usage.usage;
	float delta_total = total_time - first_usage.total;

	float usage = 100 * (delta_usage / delta_total);

	return usage;
}


/* Main functions */
void handleMem(int *fd_read, char memArr[][STRLEN], int graphics, int samples, int delay) {
	float mem_after, virtual_mem_after;
	float mem_before = 0.0;
	struct sysinfo info;

	// Close read end of fd_read
	close(fd_read[0]);
	for (int i = 0; i < samples; i++) {

		// Get memory data
		if (sysinfo(&info) < 0) perror("sysinfo");

		// Calculate memory info
		mem_after = (float)(info.totalram - info.freeram)/CONV_FAC;
		virtual_mem_after = mem_after + (float)(info.totalswap - info.freeswap)/CONV_FAC;

		snprintf(memArr[i], STRLEN, "%.2f GB / %.2f GB  -- %.2f GB / %.2f GB", mem_after, (float)(info.totalram)/CONV_FAC, 
			virtual_mem_after, (float)(info.totalram + info.totalswap)/CONV_FAC);
		if (graphics && i != 0) { // Update graphics if selected
			updateMemoryGraphics(memArr[i], mem_before, virtual_mem_after);
		} else if (graphics) {
			snprintf(memArr[i] + strlen(memArr[i]), STRLEN, "   |o 0.00 (%.2f) ",
				virtual_mem_after); // No change on first iteration
		}

		mem_before = virtual_mem_after;
		if (write(fd_read[1], &memArr[i], STRLEN) <= 0) perror("write");

		sleep(delay);
	}
	exit(0);
}

void printUsers(int *fd_read, int *fd_write, int samples) {
	int indicator;

	// Close unused pipe ends
	close(fd_read[1]);
	close(fd_write[0]);
	struct utmp *utent;
	for (int i = 0; i < samples; i++) {
		if (utmpname(_PATH_UTMP) < 0) perror("utmpname");
		setutent();
		utent = getutent();

		// Hang using read() until indicator sent through pipe, then print
		read(fd_read[0], &indicator, sizeof(int));

		printf("### Sessions/users ###\n");
		while (utent != NULL) {
			if (utent->ut_type == USER_PROCESS) {
				printf(" %s       %s (%s)\n", utent->ut_user, utent->ut_line, utent->ut_host);
			}
			utent = getutent();
		}
		endutent();
		printf("---------------------------------------\n");
		// Parent process can now continue
		if (write(fd_write[1], &indicator, sizeof(int)) <= 0) perror("write");
	}
	exit(0);
}

void handleCPU(int *fd_read, int samples, int delay) {
	float cpu_usage;
	close(fd_read[0]);

	// Send # of cores to parent process
	long n_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (n_cores < 0) perror("sysconf");
	if (write(fd_read[1], &n_cores, sizeof(long)) <= 0) perror("write");

	cpu_u cpu_usage_stats;
	int half = (delay * 1000000) / 2; // Half the delay in microseconds (for usleep())
	for (int i = 0; i < samples; i++) {
		if (i == 0) {
			cpu_usage_stats = getCPUUsage_first();
			long_sleep(half);
		}

		cpu_usage = getCPUUsage_second(cpu_usage_stats);
		if (write(fd_read[1], &cpu_usage, sizeof(float)) <= 0) perror("write");

		cpu_usage_stats = getCPUUsage_first();

		if (i == 0) {
			long_sleep(half);
		} else {
			sleep(delay);
		}
	}
	exit(0);
}

