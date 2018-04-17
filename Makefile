KDIR=/lib/modules/$(shell uname -r)/build

obj-m += fmspic.o

all:
	make -C $(KDIR) M=$(PWD)

modules: fmspic.o

insert:
	-(lsmod | grep -q fmspic) && sudo rmmod fmspic
	sudo insmod ./fmspic.ko

remove:
	sudo rmmod fmspic

SERPORT=/dev/ttyUSB0

attach:
	@echo "running inputattach in foreground, press Ctrl-C to detach..."
	sudo inputattach --baud 9600 --always -zhen $(SERPORT)


clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *~

