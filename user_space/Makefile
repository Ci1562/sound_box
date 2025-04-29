CC:=gcc

%.o: %.c
	$(CC) -c -o $@ $<

controller: controller.o
#test: test.o
	$(CC) -o $@ $<

clean:
	rm -rf controller.o controller
#	rm -rf test.c test

