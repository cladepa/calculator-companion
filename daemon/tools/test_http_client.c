#include <stdio.h>
#include "../src/http_client.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <url> <json-body>\n", argv[0]);
        return 2;
    }

    int status = 0;
    char err[256] = {0};
    int rc = http_post(argv[1], argv[2], &status, err, sizeof(err));
    if (rc == 0) {
        printf("OK status=%d\n", status);
        return 0;
    }
    printf("FAIL err=%s\n", err);
    return 1;
}
