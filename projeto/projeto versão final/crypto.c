//Sistemas Operacionas B Projeto 1
//Baseado no A simple Linux char driver for the BBB
//pelo autor Derek Molloy
//http://www.derekmolloy.ie/
//Gabriela Diamante RA 11183399
//Janaina sanches RA 07270085
//Yessica Melaine Castillo RA 13054895

/**
 * @file   testebbchar.c
 * @author Derek Molloy
 * @date   7 April 2015
 * @version 0.1
 * @brief  A Linux user space program that communicates with the ebbchar.c LKM. It passes a
 * string to the LKM and reads the response from the LKM. For this example to work the device
 * must be called /dev/ebbchar.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
*/
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
static char receive[BUFFER_LENGTH];     ///< The receive buffer from the LKM

int main(int argc, char *argv[]){
   int ret, fd;
   char stringToSend[BUFFER_LENGTH];
   if( argc != 2 ){
	   printf(" comando errado\n");
	   printf(" Cifragem execute   |# sudo./crypto \"c 5465737465313233\" \n" );
	   printf(" Decifragem execute |# sudo./crypto \"d 4B4B909AE6FA1FC50809BC8260430757\" \n"  );
	   printf(" Calculo Hash       |# sudo./crypto \"h 5465737465313233\" \n");
	   return -1;

   }
   printf("Starting device test code example...\n");
   fd = open("/dev/crypto", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
   printf("Type in a short string to send to the kernel module:\n");
   //scanf("%[^\n]%*c", stringToSend);                // Read in a string (with spaces)
   printf("Writing message to the device [%s].\n", argv[1]);
   ret = write(fd, argv[1], strlen(argv[1])); // Send the string to the LKM
   if (ret < 0){
      perror("Failed to write the message to the device.");
      return errno;
   }

   printf("Press ENTER to read back from the device...\n");
   getchar();

   printf("Reading from the device...\n");
   ret = read(fd, receive, BUFFER_LENGTH);        // Read the response from the LKM
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return errno;
   }
   printf("The received message is: [%s]\n", receive);
   printf("End of the program\n");
   return 0;
}
