Linux System Monitoring Tool -- Concurrency & Signals Assignment 3  
By: Adam Skinner  
Date: April 9th, 2024  

This code has been tested only on Linux operating systems.  

## How to compile
#### Using provided makefile (ensure `makefile`, `B09A3.c`, `stats_functions.c`, `stats_functions.h` are in current working directory):
`make a3`

## Positional arguments
`[samples]`: Changes the number of samples to the number specified as `[samples]`. Default value: 10.  
`[tdelay]`: Changes the delay between samples to the number specified as `[tdelay]` (in seconds). Default value: 1.  

## Flags (Optional)
`--system`: Prints everything but the users section.  
`--user`: Prints everything but the Memory & CPU usage sections.  
`--graphics`: Prints graphical output for the Memory & CPU usage section to easily track change over time.  
`--sequential`: Prints information sequentially without needing to "refresh" the screen (useful if you would like to redirect the output into a file). Memory info is only printed for the current iteration (including graphics), while CPU graphics contain information from previous iterations (if `--graphics` is selected).  
`--samples=X`: Changes the number of samples to the number specified as `X`. Takes precedence over the positional argument.  
`--tdelay=X`: Changes the delay between samples to the number specified as `X`. Takes precedence over the positional argument.  

## Sample usage 
	./a3 [samples] [tdelay] [--system] [--user] [--graphics] [--sequential] 
		 [--samples=X] [--tdelay=X]

## Documentation
### Important Constants
`STRLEN` defines the maximum length of a line, which is used throughout the program. Note that under normal conditions, any line length should not exceed `STRLEN`.   
`CONV_FAC` defines the conversion factor for converting from bytes to gigabytes.  

### Functions
`int main(int argc, char **argv)`  
Reads command line arguments (using for loops & getopt), initializes & declares variables, creates pipes, launches child processes for gathering data, deals with coordinating printing from each child process & using ANSI escape sequences to refresh the screen after each iteration.  

`void printStringArray(char arr[][STRLEN], int length)`  
Prints a string array with string lengths of `STRLEN` line-by-line with total array length of `length`.  

`float myRound(float f)`  
Returns a rounded version of `f` to 2 decimal places.  

`void appendChars(char *str, char c, int num)`  
Appends char `c` to string `str` `num` times.  

`void long_sleep(int micros)`  
Causes the program to sleep for `micros` microseconds for sleeping less than one second.  

`void updateCPUGraphics(char *str, float usage)`  
Adds graphical information to string `str` corresponding to the amount of CPU usage (as a percentage) from `usage`. Note: number of bars equivalent to (rounded) percentage + 3.  

`void updateMemoryGraphics(char *str, float mem_before, float mem_after)`  
Adds graphical information to the string `str` that already has numerical memory usage. The appearances of the graphics are calculated from the values of `mem_before` & `mem_after`.

`void printUsage(struct rusage *usage_self, struct rusage *usage_children)`  
Prints the program's memory usage in KB (including child processes).
`usage_self` is a pointer to the rusage struct where this data is stored for the parent process, and `usage_children` is the same for the child processes.  

`cpu_u getCPUUsage_first()`  
Uses the `/proc/stat` file to gather first instance of CPU usage as specified in the assignment handout. Returns a `cpu_u` struct which contains two fields `usage` & `total` corresponding to _U_<sub>1</sub> & _T_<sub>1</sub> from the assignment handout.  

`float getCPUUsage_second(cpu_u first_usage)`  
Gathers CPU data a second time from `/proc/stat` and returns the CPU usage (as a percentage) using the data in `first_usage` using the formula specified in the assignment handout.  

`void handleMem(int *fd_read, char memArr[][STRLEN], int graphics, int samples, int delay)`  
Gathers memory usage data & sends it to the parent process. This function is intended to run in a child process, hence the `exit(0)` system call at the end. The file descriptor `fd_read` is used to send the memory data to the parent process. It is assumed that the file descriptor has already been opened using `pipe()`. This function iteratively gathers memory data & stores it in `memArr` at the current index of the iteration, then sends this information to the parent process to be printed. `graphics` is used to indicate whether or not graphical information will be added to the line in `memArr`. `samples` is used to denote how many iterations to perform, and `delay` denotes the time to sleep in between iterations.

`void printUsers(int *fd_read, int *fd_write, int samples)`  
Uses the utmp library to gather currently connected users and print them in a human-readable format. This function is intended to run in a child process, hence the `exit(0)` system call at the end. Two file descriptors, `fd_read` & `fd_write` are used to coordinate printing with the parent process, since printing is done in this function (unlike the other two main functions). `samples` indicates how many times the main for loop in the function will iterate.  

`void handleCPU(int *fd_read, int samples, int delay)`  
Gathers CPU usage from helper functions & sends it to the parent process. This function is intended to run in a child process, hence the `exit(0)` system call at the end. The file descriptor `fd_read` is used to send the CPU usage data to the parent process. It is assumed that the file descriptor has already been opened using `pipe()`. `samples` is used to denote how many iterations of the main for loop to perform. `delay` indicates the delay between iterations

`void printSystemInfo()`  
Prints system information using the utsname library and total uptime from the /proc/uptime file converted into a human-readable format.  

`int runHandleCPU(int *fd_read, int samples, int delay)`  
Creates a child process and runs `handleCPU()` (using same parameters) from that child process. Returns the pid of the newly created child process.  

`int runPrintUsers(int *fd_users, int *fd_users_write, int samples)`  
Creates a child process and runs `printUsers()` (using same parameters) from that child process. Returns the pid of the newly created child process.  

`int runHandleMem(int *fd_read, char memArr[][STRLEN], int graphics, int samples, int delay)`  
Creates a child process and runs `handleMem()` (using same parameters) from that child process. Returns the pid of the newly created child process.

`void register_handler(sig_a* pids)`  
Function to register the signal handler for `SIGINT` using `sigaction`. `pids` is a pointer to a `sig_a` struct which contains 3 fields corresponding to the pids of each child process (and -1 if it does not exist), used for terminating them if the user decides to quit.

`void handler(int code, sig_a *pids)`  
Signal handler for `SIGINT`. Asks the user if they want to exit the program, and then terminates or continues depending on the response. `pids` is used to get the child process PIDs & terminate them if required.

## Challenges & how they were solved
- To get the program to work in a concurrent fashion, the main file (`B09A3.c`) contained functions starting with `run` that would call `fork()` to create a child process for each operation (gathering memory, user, and CPU data). These functions would be called sequentially before the main for loop in `main()` (depending on the flags). The child processes will then concurrently gather the corresponding data, and they all use a for loop which iterates the same number of times as the one in the parent process & sleeps the same amount of time in between each iteration to ensure sufficient time passes before gathering the next set of data. Note that the function to print users uses two file descriptors instead of sleeping so the parent can indicate when to print while the child hangs then the child indicates when it is done printing back to the parent so the parent can continue. The memory & CPU usage are printed by the parent process, where it hangs with `read()` waiting for data from the children before printing. Since all the processes run in parallel and terminate around the same time, pathological states are avoided upon termination of the program, as new processes are not being created on each iteration.

- In my implementation, the child processes are created before the main for loop in `main()` for optimal efficiency. 

- One challenge for gathering CPU data was figuring out how to make a delay during the first iteration without incurring additional time (over the 1% threshold). To accomplish this, during the first iteration I gathered the first instance of CPU data, then slept for _half_ the time of `tdelay` (converted to microseconds & using `usleep()` in case this value was not an integer, hence the `long_sleep()` function). After sleeping for _half_ of `tdelay`, I gathered the second instance of CPU data and performed the calculation. Then, after everything printed, the process slept for the other _half_ of `tdelay`. This ensured the delay between samples remained consistent. For each subsequent iteration, the first instance of CPU data is gathered before sleeping for `tdelay` seconds and the second is gathered after sleeping, resulting in a `tdelay` delay between CPU samples as recommended.  
- Another challenge was determining how to pass the child PIDs to the signal handler in the parent process to ensure they would be properly terminated if the user decided to terminate the program. To accomplish this, I defined a custom handler function (with an extra parameter for a pointer to a `sig_a` struct) that had a static variable which stored the pointer to the `sig_a` struct. This variable was initialized at the beginning of the program, where I called the handler function with a "dummy signal" and passed the pointer of the already-initialized dynamically-allocated `sig_a` struct, which initialized the static variable. When registering the signal handler, I type-casted my custom handler function to the proper signature into the `sa_handler` argument.