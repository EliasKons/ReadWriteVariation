CC = gcc

CODE = main.c child.c

OBJS = main.o child.o

EXEC = main

ARGS = text.txt 10 5 3

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) -lpthread

clean:
	find . -type f -name '*.txt' -not -name 'text.txt' -delete
	rm -f $(OBJS) $(EXEC) 

run: $(EXEC)
	./$(EXEC) $(ARGS)

valgrind: $(EXEC)
	valgrind ./$(EXEC) $(ARGS) --trace-children=yes