#include "core/network/SwiftNetwork.h"

#include <limits.h>
#include <stddef.h>

#include <openssl/evp.h>

bool soa_verify_ed25519(const uint8_t* public_key,
                        const uint64_t public_key_size,
                        const uint8_t* message,
                        const uint64_t message_size,
                        const uint8_t* signature,
                        const uint64_t signature_size)
{
    if (public_key == NULL || public_key_size != 32 ||
        signature == NULL || signature_size != 64 ||
        (message == NULL && message_size != 0) || message_size > SIZE_MAX)
    {
        return false;
    }

    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, NULL, public_key, (size_t)public_key_size);
    if (key == NULL)
    {
        return false;
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == NULL)
    {
        EVP_PKEY_free(key);
        return false;
    }

    const int initialized = EVP_DigestVerifyInit(context, NULL, NULL, NULL, key);
    const int verified = initialized == 1
        ? EVP_DigestVerify(context,
                           signature,
                           (size_t)signature_size,
                           message,
                           (size_t)message_size)
        : 0;

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return verified == 1;
}
