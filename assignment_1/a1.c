#include <stdio.h>
#include <string.h>

#include "list_op.h"

#define OP_VARIANT "variant"
#define OP_LIST "list"
#define OP_PARSE "parse"


int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], OP_VARIANT) == 0){
            printf("41938\n");
        }else if(strcmp(argv[1],OP_LIST) == 0) {
            perform_op_list(argc,argv);
        }
    }
    return 0;
}
