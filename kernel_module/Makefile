obj-m += hello.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

test: test.c
	gcc -g -o $@ $< -lpthread

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	sudo insmod hello.ko

uninstall:
	sudo rmmod hello.ko
	
info:
	@echo "hello.ko" && lsmod | grep hello || true
	echo "major devices(minor device is not included)"
	@cat /proc/devices | grep reverse || echo "no target devices"
	@dmesg | tail -10

