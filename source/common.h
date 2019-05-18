#ifndef _COMMON_H_
#define _COMMON_H_

// Includes NDS
#include <nds.h>

// Tipos de datos
typedef void (*estado_callback)(void);

// Variables externas
extern estado_callback estado;
extern touchPosition touchXY;
extern u32 keyDown, keyHeld, keyUp;

// Cabeceras de funciones
void *allocate(u32 size);
void fileNotFoundError(const char *filename);
void cambiarEstado(estado_callback nuevo_estado, estado_callback carga, estado_callback elimina);
void fadeIn(u8 screen);
void fadeOut(u8 screen);

#endif
