
#ifndef C_once_B4C75AF9_A27F_4744_A6DC_A2F259B3E6DA
#define C_once_B4C75AF9_A27F_4744_A6DC_A2F259B3E6DA

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _BF_CONTEXT
{
	unsigned int P[16 + 2];
	unsigned int S[4][256];
} BF_CONTEXT;

void Blowfish_Encrypt8(const BF_CONTEXT* self, void* block8);
void Blowfish_Decrypt8(const BF_CONTEXT* self, void* block8);
void Blowfish_Init(BF_CONTEXT* self, const void* key, size_t key_len);

#ifdef __cplusplus
}
#endif

#endif /* C_once_B4C75AF9_A27F_4744_A6DC_A2F259B3E6DA */
