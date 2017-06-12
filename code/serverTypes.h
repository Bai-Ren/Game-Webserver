#pragma once

typedef enum {
	HEADER, BODY
} method;

typedef struct {
	method m;
	int	cfd;     //client socket fd
	int pfd[2];  //file descriptors for game process
} status;