TARGET:= $(patsubst %.c,%.o,$(wildcard *.c))

#DEFS += -DOSCL_UNUSED_ARG=""

#CFLAGS += $(DEFS)

#$(warning "$(TARGET)-------------")


%.o:%.c
	gcc -c -O2 $(CFLAGS) $< -o $@

all: $(TARGET)
	gcc $(CFLAGS) -g -o testbin $(TARGET)


.PHONY: clean
clean:
	@echo "---------cleaing--------------"
	$(RM) $(TARGET)

