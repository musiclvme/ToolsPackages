TARGET = test.o def.o


all: $(TARGET)
	gcc -g -o testbin $(TARGET)

test.o:test.c
	gcc -g -c test.c

def.o:def.c
	gcc -g -c def.c 

.PHONY: clean
clean:
	$(RM) $(TARGET)
