TARGET = g726.o g726_tests.o bitstream.o test_utils.o


all: $(TARGET)
	gcc -g -o testbin $(TARGET) -lm -lsndfile

g726.o:g726.c
	gcc -g -c g726.c

g726_tests.o:g726_tests.c
	gcc -g -c g726_tests.c 

test_utils.o:test_utils.c
	gcc -g -c test_utils.c

.PHONY: clean
clean:
	$(RM) $(TARGET)


