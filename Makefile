htu21-y += htu21_sensor.o

obj-m += htu21.o

all:
	make -C $(KSRC) M=$(PWD) modules
clean:
	make -C $(KSRC) M=$(PWD) clean
	rm -f *.o *~

