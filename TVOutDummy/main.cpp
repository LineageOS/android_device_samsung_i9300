#include "SecTVOutService.h"
#include <cstdlib>

int main() {
    android::SecTVOutService::instantiate();

    // let it run for some time (1 sec)
    usleep(1000000);

    return 0;
}
