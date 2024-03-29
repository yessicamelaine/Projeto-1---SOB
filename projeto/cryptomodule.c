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

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/moduleparam.h>
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>        // Required for the copy to user function
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <crypto/rng.h>


#define  DEVICE_NAME "crypto"     ///< The device will appear at /dev/ebbchar using this value
#define  CLASS_NAME  "ebb"        ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Yessica Melaine Castillo RA 13054895");    ///< The author -- visible when you use modinfo
MODULE_AUTHOR("Janaina sanches RA 07270085");    ///< The author -- visible when you use modinfo
MODULE_AUTHOR("Gabriela Diamante RA 11183399");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("http://www.derekmolloy.ie/");  ///< The description -- see modinfo
MODULE_DESCRIPTION("pelo autor Derek Molloy");  ///< The description -- see modinfo
MODULE_DESCRIPTION("Baseado no A simple Linux char driver for the BBB");  ///< The description -- see modinfo
MODULE_DESCRIPTION("Sistemas Operacionas B Projeto 1");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  ebbcharClass  = NULL; ///< The device-driver class struct pointer
static struct device* ebbcharDevice = NULL; ///< The device-driver device struct pointer
static struct mutex crypto_mutex;

size_t key_size,iv_size,key_bin_size, iv_bin_size;
static char *key;
static char *iv;
module_param(key, charp, 0000);
MODULE_PARM_DESC(key, "key");
module_param(iv, charp, 0000);
MODULE_PARM_DESC(iv, "iv");
static char *key_bin;
static char *iv_bin;

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static int test_shash( char *ptext, char **res);
static int test_skcipher(char op, char *intext,int len, char **res); 


/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init ebbchar_init(void){

   printk(KERN_INFO "EBBChar: Initializing the EBBChar LKM\n");
   printk(KERN_INFO "EBBChar: key=%s iv=%s\n",key,iv);
   
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

   printk(KERN_INFO "EBBChar: key=%s iv=%s\n",key_bin,iv_bin);

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "EBBChar failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "EBBChar: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ebbcharClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(ebbcharClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "EBBChar: device class registered correctly\n");

   // Register the device driver
   ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ebbcharDevice)){               // Clean up if there is an error
      class_destroy(ebbcharClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(ebbcharDevice);
   }
   printk(KERN_INFO "EBBChar: device class created correctly\n"); // Made it! device was initialized
   
   mutex_init(&crypto_mutex);
   
   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit ebbchar_exit(void){
   kfree(key_bin);
   kfree(iv_bin);

   device_destroy(ebbcharClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(ebbcharClass);                          // unregister the device class
   class_destroy(ebbcharClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "EBBChar: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;

   mutex_lock(&crypto_mutex);

   printk(KERN_INFO "EBBChar: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){            // if true then have success
      printk(KERN_INFO "EBBChar: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "EBBChar: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
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
	   pr_info("EBBChar: (%s)(%s)\n",buffer,buffer_bin);
	   switch(op){
		case 'h': 
			test_shash(buffer_bin,&res);
			sprintf(message, "Resumo criptografico de (%s)=(%s)",buffer_bin,res);
			kfree(res);
			break;
		case 'c': 
		case 'd': 
			test_skcipher(op, buffer_bin,buffer_bin_len, &res); 
			sprintf(message, "Res (%s)=(%s)",buffer_bin,res);
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
   printk(KERN_INFO "EBBChar: Received %zu characters from the user\n", len);
   return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "EBBChar: Device successfully closed\n");

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

	pr_info("psize=%d,digestsize=%d\n",psize,digestsize);
	pr_info("A (%s)=(%s)=%lu\n",ptext,out,strlen(out));
	pr_info("B (%s)=(%s)=%lu\n",ptext,out_hex,strlen(out_hex));

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
	char ttext[17]="";
	//int ttext_size=16;
	//char hex[33]="d8b59848c7670c94b29b54d2379e2e7a";
	//char hex[33]="d79b53fb2d1bfa526ad8720a523b2fbe";
	//char hex[33]="9aba660ac568b29f3259df662047f4e6";
	char hex[33]="";
	//hex2bin(ptext,hex,16);

	DECLARE_CRYPTO_WAIT(wait);

	strcpy(ptext,intext);
	ptext[strlen(intext)]='\0';

	strcpy(key,key_bin);
	key[strlen(key_bin)]='\0';

	strcpy(iv,iv_bin);
	iv[strlen(iv_bin)]='\0';
	

	pr_info("ptext=(%s)%ld\n",ptext,strlen(ptext));

	skcipher  = crypto_alloc_skcipher("cbc(aes)", 0, 0);
	if (IS_ERR(skcipher)) {
		pr_info("could not allocate skcipher handle\n");
		return PTR_ERR(skcipher);
	}

	req  = skcipher_request_alloc(skcipher , GFP_KERNEL);
	if (!req) {
		pr_info("could not allocate skcipher request\n");
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
	} else {
		crypto_wait_req(crypto_skcipher_decrypt(req), &wait);
	}

	bin2hex(hex,ctext,16);
   	*res=kmalloc(33,0);
	bin2hex(*res,ctext,16);
	*res[33]='\0';

	pr_info("-----------------\n");
	pr_info("1(%s)\n",ptext);
	pr_info("2(%s)%ld\n",ctext,strlen(ctext));
	pr_info("3(%s)%ld\n",ttext,strlen(ttext));
	pr_info("hex=(%s)%ld\n",hex,strlen(hex));

	pr_info("-----------------\n");
	pr_info("teste04(%s)%ld\n",ctext,strlen(ctext));
	pr_info("-----------------\n");

	skcipher_request_free(req);
	crypto_free_skcipher(skcipher);

	return 0;

}


/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(ebbchar_init);
module_exit(ebbchar_exit);
