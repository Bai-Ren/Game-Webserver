#pragma once

typedef enum {
	HEADER, GET, POST
} method;

typedef struct headerField {
	char *key;
	char *value;
	struct headerField *next;
} headerFields;

typedef struct {
	method m;
	headerFields *fields;
	char *filename;
	int	cfd;     //client socket fd
	int pfd[2];  //file descriptors for game process
} status;