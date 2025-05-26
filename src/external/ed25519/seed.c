#include "ed25519.h"

#ifndef ED25519_NO_SEED

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <stdio.h>
#endif

int ed25519_create_seed(unsigned char *seed) {
#ifdef _WIN32
    HCRYPTPROV prov;

    if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))  {
        return 1;
    }

    if (!CryptGenRandom(prov, 32, seed))  {
        CryptReleaseContext(prov, 0);
        return 1;
    }

    CryptReleaseContext(prov, 0);
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) {
        // If /dev/urandom fails, try /dev/random
        f = fopen("/dev/random", "rb");
        if (!f) {
            // If both fail, use a fallback method
            for (int i = 0; i < 32; i++) {
                seed[i] = rand() % 256;
            }
            return 0;
        }
    }

    size_t bytes_read = fread(seed, 1, 32, f);
    fclose(f);
    
    if (bytes_read != 32) {
        // If we couldn't read enough bytes, fill the rest with rand()
        for (size_t i = bytes_read; i < 32; i++) {
            seed[i] = rand() % 256;
        }
    }
#endif

    return 0;
}

#endif
