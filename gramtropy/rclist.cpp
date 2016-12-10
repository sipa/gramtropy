#include <stdio.h>
#include "rclist.h"

int main(void) {
    struct test;
    typedef rclist<test> rc;
    struct test {
        int i;
        rc::iterator it;
        test(int i_, const rc::iterator it_) : i(i_), it(it_) {
            printf("Construct %i\n", i);
        }
        ~test() {
            printf("Descruct %i\n", i);
        }
    };
    rc bla;
    auto x2 = bla.emplace_front(2, bla.end());
    auto x3 = bla.emplace_back(3, x2);
    auto x1 = bla.emplace(x2, 1, x2);
    auto x4 = bla.emplace_back(4, x2);
    x2->it = x4;

    printf("All\n");
    for (const auto& x : bla) {
        printf("x: %i\n", x.i);
    }

    x3 = bla.end();
    printf("All - 3\n");
    for (const auto& x : bla) {
        printf("x: %i\n", x.i);
    }
    x4 = bla.end();
    x1 = bla.end();
    x2 = bla.end();
    rclist<test>::const_fixed_iterator ix2 = x2;
    printf("None\n");
    for (const auto& x : bla) {
        printf("x: %i\n", x.i);
    }
    return 0;
}
