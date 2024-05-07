CC = gcc
FLAGS = -Wall -Werror
CFLAGS = $(FLAGS)

a3: B09A3.o stats_functions.o
	$(CC) $(FLAGS) $^ -o $@

B09A3.o: B09A3.c
	$(CC) $(FLAGS) $^ -c -o $@

stats_functions.o: stats_functions.c
	$(CC) $(FLAGS) $^ -c -o $@

clean:
	rm -f *.o

help:
	@echo "a3: compile program"
	@echo "B09A3.o: generate object code file for B09A3.c"
	@echo "stats_functions.o: generate object code file for stats_functions.c"
	@echo "clean: remove object code files"