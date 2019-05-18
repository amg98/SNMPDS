#ifndef _TFTP_H_
#define _TFTP_H_

// Includes NDS
#include <nds.h>

// Cabeceras de funciones
int TFTP_SendFile(char *filename, void *data, u32 size);

#endif
