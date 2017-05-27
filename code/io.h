#pragma once
#include <sys/types.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum { NONE, GET, PUT, DELETE, LIST } verb;


ssize_t read_all_from_socket(int socket, char *buffer, size_t count);
ssize_t write_all_to_socket(int socket, const char *buffer, size_t count);

void read_all_noerr(int socket, char *buffer, size_t count);
void write_all_noerr(int socket, const char *buffer, size_t count);

ssize_t read_all_until_newline(int socket, char *buffer, size_t count);
