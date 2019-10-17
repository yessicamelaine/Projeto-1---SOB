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

static void __exit teste_exit(void){
}
static int test_shash(void){
	
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
	char *ptext="Texto";
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

	kfree(out_hex);
	kfree(out);
	kfree(adesc);

	/* */
	return 0;
}

module_init(test_shash);
module_exit(teste_exit);

