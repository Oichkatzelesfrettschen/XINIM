#include <unistd.h>
#include <string.h>

int main(void) {
    char buf[256];
    if (getcwd(buf, sizeof(buf)) == 0) {
        write(2, "pwd: getcwd failed\n", 19);
        return 1;
    }
    write(1, buf, strlen(buf));
    write(1, "\n", 1);
    return 0;
}
