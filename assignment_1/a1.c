#include <stdio.h>
#include <string.h>

#define OP_VARIANT "variant"
int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0){
            printf("41938\n");
        }
    }
    return 0;
}