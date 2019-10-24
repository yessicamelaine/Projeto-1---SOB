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

#define BUFFER_LENGTH 256               // O comprimento do buffer 
static char receive[BUFFER_LENGTH];     // O buffer de recebimento do modulo

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
   printf("Iniciando o exemplo de codigo de teste do dispositivo...\n");
   fd = open("/dev/crypto", O_RDWR); 
   //Abra o dispositivo com acesso de leitura / gravação
   if (fd < 0){
      perror("Falha ao abrir o dispositivo...");
      return errno;
   }
   printf("Digite uma sequencia curta para enviar ao modulo do kernel:\n");
   //scanf("%[^\n]%*c", stringToSend); // Read in a string (with spaces)
   printf("Gravando mensagem no dispositivo [%s].\n", argv[1]);
   ret = write(fd, argv[1], strlen(argv[1])); // Envie a string para o modulo
   if (ret < 0){
      perror("Falha ao gravar a mensagem no dispositivo.");
      return errno;
   }

   printf("Lendo do dispositivo...\n");
   ret = read(fd, receive, BUFFER_LENGTH);  // Le a resposta do modulo
   if (ret < 0){
      perror("Falha ao ler a mensagem do dispositivo.");
      return errno;
   }
   printf("A mensagem recebida eh: [%s]\n", receive);
   printf("Fim do programa\n");
   return 0;
}
