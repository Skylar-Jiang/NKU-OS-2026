/* LAB5 2411788: integrated user-mode COW demonstration. */
#include <ulib.h>
#include <stdio.h>
static volatile unsigned char cow_data[4096] __attribute__((aligned(4096))) = {0x44};
int main(void) {
    int pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        assert(cow_data[0] == 0x44);
        cow_data[0] = 0x55;
        assert(cow_data[0] == 0x55);
        cprintf("cowproof: child data=0x%x\n", cow_data[0]);
        exit(0);
    }
    assert(waitpid(pid, NULL) == 0);
    assert(cow_data[0] == 0x44);
    cprintf("cowproof: parent data=0x%x (unchanged)\n", cow_data[0]);
    cprintf("cowproof pass.\n");
    return 0;
}
