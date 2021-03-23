//
// Created by keresztes on 3/23/21.
//

#ifndef ASSIGNMENT_1_LIST_OP_H
#define ASSIGNMENT_1_LIST_OP_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_PATH_SIZE 300
#define MAX_NAME_SIZE 30
#define MAX_NR_ELEMENTS 100

#define SUCCESS 0
#define INVALID_DIRECTORY_PATH -1
#define INVALID_ARGUMENTS -2
#define MISSING_DIR_PATH -3

struct parameters{
    bool path;
    bool recursive;
    bool suffix;
    bool permission;
};
int list_directory_tree(char * dir_path, char ** dir_elements, int * elem_count, char * suffix, char * permission, struct parameters detected);
void perform_op_list(int nr_parameters, char ** parameters);
unsigned convert_permission_format(const char * permission);

#endif //ASSIGNMENT_1_LIST_OP_H
