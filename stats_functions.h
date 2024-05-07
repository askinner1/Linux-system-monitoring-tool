#include <stdio.h>
#include <stdlib.h>

#define STRLEN 256
#define CONV_FAC 1073741824

typedef struct sig_args {
    int pid1;
    int pid2;
    int pid3;
} sig_a;

typedef struct CPU_usage {
	float usage;
	float total;
} cpu_u;

void long_sleep(int micros);

float myRound(float f);

void appendChars(char *str, char c, int num);

void updateCPUGraphics(char *str, float usage);

void printStringArray(char arr[][STRLEN], int length);

void updateMemoryGraphics(char *str, float mem_before, float mem_after);

cpu_u getCPUUsage_first();

float getCPUUsage_second(cpu_u first_usage);

void handleMem(int *fd_read, char memArr[][STRLEN], int graphics, int samples, int delay);

void printUsers(int *fd_read, int *fd_write, int samples);

void handleCPU(int *fd_read, int samples, int delay);

