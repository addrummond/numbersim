CC := gcc
CFLAGS := -I./ $(CFLAGS)
OBJS := numbersim.o parser.o pcg_basic.o

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	$(CC) -MM $(CFLAGS) $*.c > $*.d

-include $(OBJS:.o=d)

.PHONY: clean
clean:
	find ./ -name '*.o' -exec rm -f {} \;

numbersim: $(OBJS) numbersim
	$(CC) $(LDFLAGS) $(OBJS) -o numbersim
