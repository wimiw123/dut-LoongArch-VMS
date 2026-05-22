#include "trap.h"

int main(void) {
    int a = 7;
    int b = 3;
    int c;

    if (a > b) {
        c = 1;
    } else {
        c = 2;
    }

    check(c == 1);
    return 0;
}
