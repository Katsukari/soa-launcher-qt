#include "core/network/SwiftNetwork.h"

#include <openssl/evp.h>

#include <stddef.h>

bool soa_verify_ed25519(const uint8_t* public_key,
                        const uint64_t public_key_size,
                        const uint8_t* message,
                        const uint64_t message_size,
                        const uint8_t* signature,
                        const uint64_t signature_size)
{
    if (!public_key || public_key_size != 32 || !message || !signature || signature_size != 64
        || message_size > SIZE_MAX)
        return false;

    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, public_key,
                                                 (size_t)public_key_size);
    if (!key)
        return false;

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    const bool valid = context
        && EVP_DigestVerifyInit(context, NULL, NULL, NULL, key) == 1
        && EVP_DigestVerify(context, signature, (size_t)signature_size, message,
                            (size_t)message_size) == 1;

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return valid;
}
