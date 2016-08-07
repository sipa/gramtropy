#include "generator.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv) {
    unsigned long num = argc > 2 ? strtoul(argv[2], NULL, 10) : 1;
    double bits = argc > 1 ? strtod(argv[1], NULL) : 64;

    char* buf = (char*)malloc(1000000);
    size_t r = fread(buf, 1, 1000000, stdin);
    buf[r] = 0;
    void* gen = generator_create(buf, (int)bits);

    while (num--) {
        generator_generate(gen, buf, 1000000);
        printf("%s\n", buf);
    }

    generator_destroy(gen);

    return 0;
}
