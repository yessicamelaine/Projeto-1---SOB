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

#define  DEVICE_NAME "teste04"     ///< The device will appear at /dev/ebbchar using this value
#define  CLASS_NAME  "teste04"        ///< The device class -- this is a character device driver
MODULE_LICENSE("GPL");



/* Initialize and trigger cipher operation */
static int test_skcipher(void)
{
/*
 * static void generate_random_cipher_testvec(struct skcipher_request *req,
					   struct cipher_testvec *vec,
					   unsigned int maxdatasize,
					   char *name, size_t max_namelen)
*/
	struct crypto_skcipher *skcipher=NULL;
	struct skcipher_request *req=NULL;
	struct crypto_skcipher *tfm;
	struct scatterlist src, dst;
	unsigned int maxdatasize=32;
	unsigned int maxkeysize;
	unsigned int ivsize;
	int ret = -EFAULT;
	char key[]="1234567890123456";
	int key_size=16;
	char iv[] ="1234567890123456";
	int iv_size=16;
	char ptext[]="1234567890123456";
	int ptext_size=16;
	char ctext[17]="";
	int ctext_size=16;
	char ttext[17]="";
	int ttext_size=16;
	char hex[33]="d8b59848c7670c94b29b54d2379e2e7a";
	
	DECLARE_CRYPTO_WAIT(wait);

	hex2bin(ptext,hex,16);
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
	crypto_wait_req(crypto_skcipher_encrypt(req), &wait);
	//crypto_wait_req(crypto_skcipher_decrypt(req), &wait);

	bin2hex(hex,ctext,16);

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

static void __exit teste_exit(void){
}

module_init(test_skcipher);
module_exit(teste_exit);

