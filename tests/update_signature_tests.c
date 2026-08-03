#include "core/network/SwiftNetwork.h"

#include <stdint.h>
#include <stdio.h>

static uint8_t nibble(const char value)
{
    if (value >= '0' && value <= '9')
        return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f')
        return (uint8_t)(value - 'a' + 10);
    return 0xff;
}

static int decode(const char* hex, uint8_t* output, const size_t size)
{
    for (size_t index = 0; index < size; ++index)
    {
        const uint8_t high = nibble(hex[index * 2]);
        const uint8_t low = nibble(hex[index * 2 + 1]);
        if (high == 0xff || low == 0xff)
            return 0;
        output[index] = (uint8_t)((high << 4) | low);
    }
    return 1;
}

int main(void)
{
    uint8_t public_key[32];
    uint8_t signature[64];
    const uint8_t message[] = {0x72};
    const char* public_key_hex =
        "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c";
    const char* signature_hex =
        "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
        "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00";

    if (!decode(public_key_hex, public_key, sizeof(public_key))
        || !decode(signature_hex, signature, sizeof(signature)))
        return 1;
    if (!soa_verify_ed25519(public_key, sizeof(public_key), message, sizeof(message), signature,
                            sizeof(signature)))
        return 1;

    signature[0] ^= 1;
    if (soa_verify_ed25519(public_key, sizeof(public_key), message, sizeof(message), signature,
                           sizeof(signature)))
        return 1;
    return 0;
}
