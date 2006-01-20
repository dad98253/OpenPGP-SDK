/** \file
 */

#include "util.h"
#include "packet.h"

#ifndef OPS_CRYPTO_H
#define OPS_CRYPTO_H

#define OPS_MAX_HASH	64

typedef void ops_hash_init_t(ops_hash_t *hash);
typedef void ops_hash_add_t(ops_hash_t *hash,const unsigned char *data,
			unsigned length);
typedef unsigned ops_hash_finish_t(ops_hash_t *hash,unsigned char *out);

/** _ops_hash_t */
struct _ops_hash_t
    {
    ops_hash_algorithm_t algorithm;
    const char *name;
    ops_hash_init_t *init;
    ops_hash_add_t *add;
    ops_hash_finish_t *finish;
    void *data;
    };

void ops_crypto_init(void);
void ops_crypto_finish(void);
void ops_hash_md5(ops_hash_t *hash);
void ops_hash_sha1(ops_hash_t *hash);
void ops_hash_any(ops_hash_t *hash,ops_hash_algorithm_t alg);
ops_hash_algorithm_t ops_hash_algorithm_from_text(const char *hash);
const char *ops_text_from_hash(ops_hash_t *hash);
unsigned ops_hash_size(ops_hash_algorithm_t alg);

void ops_hash_add_int(ops_hash_t *hash,unsigned n,unsigned length);

ops_boolean_t ops_dsa_verify(const unsigned char *hash,size_t hash_length,
			     const ops_dsa_signature_t *sig,
			     const ops_dsa_public_key_t *dsa);
int ops_rsa_public_decrypt(unsigned char *out,const unsigned char *in,
			   size_t length,const ops_rsa_public_key_t *rsa);
int ops_rsa_private_encrypt(unsigned char *out,const unsigned char *in,
			    size_t length,const ops_rsa_secret_key_t *srsa,
			    const ops_rsa_public_key_t *rsa);

unsigned ops_block_size(ops_symmetric_algorithm_t alg);

#endif
