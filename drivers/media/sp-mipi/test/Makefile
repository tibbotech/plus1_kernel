CROSS = arm-none-linux-gnueabi-

OUT := mipicsi

mipicsi: mipicsi.o
	$(CROSS)gcc -o $@ $^ -static
	$(CROSS)strip $@

%.o: %.c
	$(CROSS)gcc -c -o $@ $<

clean:
	@rm -f *.o $(OUT)

