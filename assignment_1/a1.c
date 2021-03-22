#include <stdio.h>
#include <string.h>

#define OP_VARIANT "variant"
#define OP_LIST "list"

int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0){
            printf("41938\n");
        }else if(strcmp(argv[1],OP_LIST) == 0) {
            //todo
        }
    }
    return 0;
}