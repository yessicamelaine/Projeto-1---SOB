#Sistemas Operacionas B Projeto 1
#Baseado no A simple Linux char driver for the BBB
#pelo autor Derek Molloy
#http://www.derekmolloy.ie/
#Gabriela Diamante RA 11183399
#Janaina sanches RA 07270085
#Yessica Melaine Castillo RA 13054895

obj-m+=cryptomodule.o


all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) testcryptomodule.c -o testcryptomodule
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm testcryptomodule
