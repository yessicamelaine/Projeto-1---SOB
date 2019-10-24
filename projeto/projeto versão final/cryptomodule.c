//Sistemas Operacionas B Projeto 1
//Baseado no A simple Linux char driver for the BBB
//pelo autor Derek Molloy
//http://www.derekmolloy.ie/
//Gabriela Diamante RA 11183399
//Janaina sanches RA 07270085
//Yessica Melaine Castillo RA 13054895

/**
 * @file   ebbchar.c
 * @author Derek Molloy
 * @date   7 April 2015
 * @version 0.1
 * @brief   An introductory character driver to support the second article of my series on
 * Linux loadable kernel module (LKM) development. This module maps to /dev/ebbchar and
 * comes with a helper C program that can be run in Linux user space to communicate with
 * this the LKM.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
 */

#include <linux/init.h>  //Macros usadas para marcar funções. ex: __init __exit
#include <linux/module.h> // Cabeçalho principal para carregar módulos no kernel
#include <linux/moduleparam.h> // incluido para inserir parâmetros
#include <linux/device.h>  //Cabeçalho para dar suporte ao Modelo de Driver do kernel

#include <linux/kernel.h>      // Contém tipos, macros, funções para o kernel
#include <linux/fs.h>          // Cabeçalho para suporte ao sistema de arquivos Linux
#include <linux/uaccess.h>     // Required for the copy to user function
#include <linux/mutex.h>       // incluido para inserir MUTEX = LOCK
#include <linux/slab.h>

#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <crypto/rng.h>

//O dispositivo aparecerá em / dev / crypto usando este valor
#define  DEVICE_NAME "crypto" 
//A classe de dispositivo - este é um driver de dispositivo de caracteres.
#define  CLASS_NAME  "ebb" 

//O tipo de licença - isso afeta a funcionalidade disponível
MODULE_LICENSE("GPL");   
// O autor - visível quando você usa o modinfo
MODULE_AUTHOR("Yessica Melaine Castillo RA 13054895");
MODULE_AUTHOR("Janaina sanches RA 07270085");    
MODULE_AUTHOR("Gabriela Diamante RA 11183399");
// A descricao -- ve no modinfo  
MODULE_DESCRIPTION("http://www.derekmolloy.ie/");  
MODULE_DESCRIPTION("pelo autor Derek Molloy");  
MODULE_DESCRIPTION("Baseado no A simple Linux char driver for the BBB");  
MODULE_DESCRIPTION("Sistemas Operacionas B Projeto 1");  
MODULE_VERSION("0.1");           

static int    majorNumber;  // Armazena o número do dispositivo - determinado
//Memória para a string que é passada do espaço do usuário.
static char   message[256] = {0}; 
static short  size_of_message;// Usado para lembrar o tamanho da string armazenada.
static int    numberOpens = 0;// Conta o número de vezes que o dispositivo é aberto.
//ponteiro struct da classe do driver de dispositivo.
static struct class*  ebbcharClass  = NULL; 
//ponteiro de estrutura do dispositivo do driver de dispositivo
static struct device* ebbcharDevice = NULL; 
static struct mutex crypto_mutex; //estrutura do mutex

// declaracao dos parametros
size_t key_size,iv_size,key_bin_size, iv_bin_size;
static char *key;
static char *iv;
module_param(key, charp, 0000);
MODULE_PARM_DESC(key, "key");
module_param(iv, charp, 0000);
MODULE_PARM_DESC(iv, "iv");
static char *key_bin;
static char *iv_bin;

// As funções de protótipo para o driver de caractere 
//- devem vir antes da definição da estrutura.
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static int test_shash( char *ptext, char **res);
static int test_skcipher(char op, char *intext,int len, char **res); 


/** @brief Os dispositivos são representados como estrutura de arquivo no kernel. 
* A estrutura *file_operations de /linux/fs.h lista as funções de retorno de chamada * que você deseja associar às suas operações de arquivo usando uma estrutura de 
* sintaxe C99. Os dispositivos char geralmente implementam chamadas de abertura, 
* leitura, gravação e liberação.
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** @brief A função de inicialização do módulo
* A palavra-chave estática restringe a visibilidade da função dentro desse arquivo C. * A macro __init significa que, para um driver interno (não um módulo), a função é 
* usada apenas no momento da inicialização e pode ser descartada e a memória *liberada após esse ponto.
* @return retorna 0 se for sucesso.
*/
static int __init ebbchar_init(void){

   printk(KERN_INFO "cryptomodule: Inicializando o modulo cryptomodule \n");
   printk(KERN_INFO "cryptomodule: key=%s iv=%s\n",key,iv);
   
   key_size = strlen( key );
   iv_size = strlen( iv );

   key_bin_size = key_size / 2  ;
   iv_bin_size = iv_size / 2 ;

   key_bin=kmalloc(key_bin_size+1,0);
   iv_bin=kmalloc(iv_bin_size+1,0); 

   hex2bin(key_bin,key,key_bin_size);
   hex2bin(iv_bin,iv,iv_bin_size);

   key_bin[key_bin_size]='\0';
   iv_bin[iv_bin_size]='\0';

   printk(KERN_INFO "cryptomodule: key=%s iv=%s\n",key_bin,iv_bin);

   // tenta alocar dinamicamente um número principal para o dispositivo. 
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "O cryptomodule falhou ao registrar um número principal\n");
      return majorNumber;
   }
   printk(KERN_INFO "cryptomodule: registrado corretamente com número principal %d\n", majorNumber);

   // Registra a classe dispositivo
   ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ebbcharClass)){       // Limpe se houver um erro.
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Falhou em criar a classe dispositivo \n");
      return PTR_ERR(ebbcharClass);
      // Maneira correta de retornar um erro em um ponteiro
   }
   printk(KERN_INFO "cryptomodule: classe dispositivo registrado corretamente\n");

   // Registra o  driver do dispositivo
   ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ebbcharDevice)){               // Limpe se houver um erro
      //Código repetido, mas a alternativa são instruções goto
      class_destroy(ebbcharClass);
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Falha ao criar o dispositivo\n");
      return PTR_ERR(ebbcharDevice);
   }
   printk(KERN_INFO "cryptomodule: classe de dispositivo criada corretamente\n");
    // dispositivo foi inicializado
   
   mutex_init(&crypto_mutex); //inicializa mutex
   
   return 0;
}

/** @brief A função de limpeza do módulo
 *  Semelhante à função de inicialização, é estático. A macro __exit notifica que, se * esse código for usado para um driver interno (não um módulo), essa função não será * necessária.
 */

static void __exit ebbchar_exit(void){
   kfree(key_bin);
   kfree(iv_bin);

   device_destroy(ebbcharClass, MKDEV(majorNumber, 0));     // remove o dispositivo
   class_unregister(ebbcharClass);  // cancelar o registro da classe de dispositivo
   class_destroy(ebbcharClass);     // remove a  classe do dispositivo
   unregister_chrdev(majorNumber, DEVICE_NAME);
   // cancelar o registro o numero principal
   printk(KERN_INFO "cryptomodule: Adeus do modulo!\n");
}

/** @brief A função de abertura do dispositivo é chamada sempre que 
*o dispositivo é aberto. 
*  Isso apenas incrementará o contador numberOpens nesse caso.
 *  @param inodep Um ponteiro para um objeto inode (definido em linux / fs.h)
 *  @param filep Um ponteiro para um objeto de arquivo (definido em linux / fs.h)
 */

static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   //quem chegar aqui e conseguir executar, passa para o proximo comando
   // ou seja, eh onde faz o bloqueio de acesso
   mutex_lock(&crypto_mutex);

   printk(KERN_INFO "cryptomodule: Dispositivo foi aberto %d vez(es)\n", numberOpens);
   return 0;
}

/** @brief Essa função é chamada sempre que o dispositivo está sendo lido no espaço *do usuário, ou seja, os dados estão sendo enviados do dispositivo para o usuário. 
*Nesse caso, usa a função copy_to_user () para enviar a string do buffer para o *usuário e captura todos os erros.
* @param filep Um ponteiro para um objeto de arquivo (definido em linux / fs.h)
* @param buffer O ponteiro para o buffer no qual esta função grava os dados
* @param len O comprimento do b
* @param offset O deslocamento, se necessário
*/
//Ou seja, pega a informação dada pelo usuario

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user tem o formato ( * to, *from, size) e retorna 0 no successo
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){            // se for verdade entao successo
      printk(KERN_INFO "cryptomodule: Enviado %d caracteres para o usuario\n", size_of_message);
      return (size_of_message=0);  //limpe a posição até o início e retorne 0
   }
   else {
      printk(KERN_INFO "cryptomodule: Falha ao enviar % d caracteres ao usuario\n", error_count);
      return -EFAULT; // Falha - retorne uma mensagem de endereço incorreto (-14)

   }
}

/** @brief Essa função é chamada sempre que o dispositivo está sendo gravado no *espaço do usuário, ou seja, os dados são enviados ao dispositivo pelo usuário. 
*Os dados são copiados para a matriz de mensagens [] neste modulo usando a função sprintf () junto com o comprimento da *string.
* @param filep Um ponteiro para um objeto de arquivo
* buffer @param O buffer contém a string a ser gravada no dispositivo
* @param len O comprimento da matriz de dados que está sendo passada no buffer const char
  * @param offset O deslocamento, se necessário
  */
//escreve no buffer

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){

   char *buffer_bin;
   char *res;
   int buffer_bin_len;
   char op;
   if( len > 2 ){
	   op=buffer[0];
	   buffer_bin_len=(len-2)/2;
	   buffer_bin=kmalloc(buffer_bin_len+1,0);
	   buffer_bin[0]='\0';
	   hex2bin(buffer_bin,&buffer[2],len-2);
	   buffer_bin[buffer_bin_len]='\0';
	   switch(op){
		case 'h': 
			test_shash(buffer_bin,&res);
			sprintf(message, "Resumo criptografico de \"%s\"=\"%s\"",buffer_bin,res);
			kfree(res);
			break;
		case 'c': 
		case 'd': 
			test_skcipher(op, buffer_bin,buffer_bin_len, &res);
			sprintf(message, "Operacao \"%s\"=\"%s\"",buffer,res);
                        kfree(res);
                        break;
		default: 
			sprintf(message, "Operacao nao implementada %c", op);
			break;
	   }
   } else {
	   sprintf(message, "String vazia");
   }



   //sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "cryptomodule: Received %zu characters from the user\n", len);
   return len;
}

/** @brief A função de liberação do dispositivo que é chamada sempre que
* o dispositivo é fechado / liberado pelo programa userspace
  * @param inodep Um ponteiro para um objeto inode (definido em linux / fs.h)
  * @param filep Um ponteiro para um objeto de arquivo (definido em linux / fs.h)
  */

static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "cryptomodule: Device successfully closed\n");
   // um processo abriu e fez a leitura e escrita e quando fechou o arquivo,
   // desbloqueia o acesso do processo que esta em espera	
   mutex_unlock(&crypto_mutex);

   return 0;
}


static int test_shash( char *ptext, char **res){
	
	u8 *driver="sha1-generic";
	const char *adriver="sha1";
	struct shash_desc *adesc = NULL;
	struct crypto_shash *atfm = NULL;
	struct crypto_ahash *aatfm = NULL;
	struct ahash_request *areq;
	int error;
	int digestsize;
	//char *ptext="The quick brown fox jumps over the lazy dog";
	//char *ptext="TXT";
	char psize=0;
	u8 *out;
	u8 *out_hex;
	int type=0;
	int mask=0;
	psize=strlen(ptext);
	atfm = crypto_alloc_shash(adriver, 0, 0);
	adesc = kmalloc(sizeof(struct shash_desc)+crypto_shash_descsize(atfm), GFP_KERNEL);
	aatfm = crypto_alloc_ahash(driver, type, mask);
	areq = ahash_request_alloc(aatfm, GFP_KERNEL);
	aatfm = crypto_ahash_reqtfm(areq);
	adesc->tfm = atfm;
	digestsize = crypto_ahash_digestsize(aatfm);
	out=kmalloc( digestsize + 1, GFP_KERNEL);
	out_hex=kmalloc( (digestsize*2)+1, GFP_KERNEL);
	/* */
	error = crypto_shash_digest(adesc, ptext, psize, out);
	out[digestsize]='\0';
	bin2hex(out_hex,out,digestsize);
	out_hex[digestsize*2]='\0';

	crypto_free_ahash(aatfm);
	ahash_request_free(areq);
	//crypto_free_ahash(aatfm);
	crypto_free_shash(atfm);
	
	*res=kmalloc( (digestsize*2)+1, GFP_KERNEL);
	strcpy(*res,out_hex);

	kfree(out_hex);
	kfree(out);
	kfree(adesc);

	/* */
	return 0;
}



static int test_skcipher(char op, char *intext,int intext_size, char **res) 
{

	struct crypto_skcipher *skcipher=NULL;
	struct skcipher_request *req=NULL;
	struct crypto_skcipher *tfm;
	struct scatterlist src, dst;
	//unsigned int maxdatasize=32;
	unsigned int maxkeysize;
	unsigned int ivsize;
	int ret = -EFAULT;
	char key[]="1234567890123456";
	int key_size=16;
	char iv[] ="1234567890123456";
	int iv_size=16;
	//char ptext[17]="1234567890123456";
	char ptext[17]="";
	int ptext_size=16;

	char ctext[17]="";
	int ctext_size=16;
	char hex[33]="";

	DECLARE_CRYPTO_WAIT(wait);

	strcpy(ptext,intext);
	ptext[strlen(intext)]='\0';

	strcpy(key,key_bin);
	key[strlen(key_bin)]='\0';

	strcpy(iv,iv_bin);
	iv[strlen(iv_bin)]='\0';
	


	skcipher  = crypto_alloc_skcipher("cbc(aes)", 0, 0);
	if (IS_ERR(skcipher)) {
		pr_info("nao foi possível alocar o manipulador skcipher\n");
		return PTR_ERR(skcipher);
	}

	req  = skcipher_request_alloc(skcipher , GFP_KERNEL);
	if (!req) {
		pr_info("nao foi possível alocar a solicitacao skcipher\n");
		ret = -ENOMEM;
		//goto out;
	}
	tfm  = crypto_skcipher_reqtfm(req );
	maxkeysize = tfm->keysize;
	ivsize = crypto_skcipher_ivsize(tfm);
	ret = crypto_skcipher_setkey(tfm , key, key_size);
	//goto out;
	sg_init_one(&src, ptext, ptext_size);	
	sg_init_one(&dst, ctext, ctext_size);	

	skcipher_request_set_callback(req, 0, crypto_req_done, &wait);
	skcipher_request_set_crypt(req, &src, &dst, iv_size, iv);
	if( op == 'c' ){
		crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
		bin2hex(hex,ctext,16);
		*res=kmalloc(33,0);
		bin2hex(*res,ctext,16);
		
		//*res[32]='\0';
	} else {
		crypto_wait_req(crypto_skcipher_decrypt(req), &wait);
		bin2hex(hex,ctext,16);
		*res=kmalloc(33,0);
		bin2hex(*res,ctext,16);
		
	}


	skcipher_request_free(req);
	crypto_free_skcipher(skcipher);

	return 0;

}



/** @brief Um módulo deve usar as macros module_init () module_exit () 
* do linux / init.h, que identifique a função de inicialização 
* no momento da inserção e a função de limpeza (como listado acima)
*/

module_init(ebbchar_init);
module_exit(ebbchar_exit);
