#ifndef _GRAMTROPY_GENERATOR_
#define _GRAMTROPY_GENERATOR_

#ifdef __cplusplus
extern "C" {
#endif

void* generator_create(const char* grammar, int bits, char* status);
int generator_generate(void* gen, char* str, int length);
void generator_destroy(void* gen);

#ifdef __cplusplus
}
#endif

#endif
