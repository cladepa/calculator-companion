#include <stdio.h>
#include "../src/https_client.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <url> <json-body> <ca_cert_path>\n", argv[0]);
        return 2;
    }

    int status = 0;
    char err[256] = {0};
    int rc = https_post(argv[1], argv[2], argv[3], &status, err, sizeof(err));
    if (rc == 0) {
        printf("OK status=%d\n", status);
        return 0;
    }
    printf("FAIL err=%s\n", err);
    return 1;
}
