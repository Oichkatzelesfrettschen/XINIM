#include <stdio.h>
#include <string.h>
#include "params.h"
#include "indcpa.h"

int main() {
    uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES] = {0};
    uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES] = {0};
    uint8_t coins[KYBER_SYMBYTES];
    memset(coins, 0xAA, sizeof(coins));
    indcpa_keypair_derand(pk, sk, coins);
    
    uint8_t msg[KYBER_INDCPA_MSGBYTES];
    memset(msg, 0, sizeof(msg));
    
    uint8_t ct[KYBER_INDCPA_BYTES];
    uint8_t enc_coins[KYBER_SYMBYTES];
    memset(enc_coins, 0xBB, sizeof(enc_coins));
    indcpa_enc(ct, msg, pk, enc_coins);
    
    uint8_t msg2[KYBER_INDCPA_MSGBYTES];
    indcpa_dec(msg2, ct, sk);
    
    printf("Original:  ");
    for (int i = 0; i < 8; i++) printf("%02x ", msg[i]);
    printf("\nDecrypted: ");
    for (int i = 0; i < 8; i++) printf("%02x ", msg2[i]);
    printf("\n");
    
    int diff = 0;
    for (int i = 0; i < KYBER_INDCPA_MSGBYTES; i++)
        if (msg[i] != msg2[i]) diff++;
    printf("Bytes differ: %d / %d\n", diff, KYBER_INDCPA_MSGBYTES);
    
    // Also print pk[0..3] and sk[0..3]
    printf("pk[0..3]: %02x %02x %02x %02x\n", pk[0], pk[1], pk[2], pk[3]);
    printf("sk[0..3]: %02x %02x %02x %02x\n", sk[0], sk[1], sk[2], sk[3]);
    
    return diff;
}
