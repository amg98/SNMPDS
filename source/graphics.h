#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

// Includes NDS
#include <nds.h>

// Defines
#define TOP_SCREEN 0
#define BOTTOM_SCREEN 1
#define VLINE *(vu16*)0x04000006		// Linea actual de dibujado

// Variables externas
extern PrintConsole topConsole;
extern PrintConsole bottomConsole;
extern int bitmapLayerTop;
extern int bitmapLayerBottom;

// Estructura de un sprite
typedef struct{
	void *gfx;
	int x, y;
	u8 screen;
	u8 id;
	SpriteSize size;
	u8 cloned;
}sprite;

// Cabeceras de funciones
void iniciarGfx();
void loadBitmap8(const char *file, u8 screen);
sprite *loadSprite(const char *file, u8 screen, u8 index, SpriteSize size);
sprite *cloneSprite(sprite *sprsrc, u8 index);
void freeSprite(sprite *spr);
void updateSprite(sprite *spr);
void loadFileTo(const char *file, void *addr);
void *loadFile(const char *file, u32 *fileSize);
void dmaCopyBuffer(void* dest, const void* source, u32 size);
u8 spriteTouched(sprite *spr, u8 width, u8 height);

#endif
