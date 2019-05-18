// Includes C
#include <stdio.h>

// Includes NDS
#include <nds.h>

// Includes propios
#include "graphics.h"
#include "common.h"

// Consolas de texto
PrintConsole topConsole;
PrintConsole bottomConsole;
int bitmapLayerTop = -1;
int bitmapLayerBottom = -1;
static ConsoleFont consoleFont;
static Keyboard *teclado = NULL;

// Callback cuando se pulsa una tecla en el teclado
static void keyPressed(int c){
	if(c > 0) iprintf("%c", c);
}

// Inicializa el motor grafico de la DS
void iniciarGfx(){
	
	// Carga la fuente de texto
	u32 palSize;
	consoleFont.gfx = loadFile("/proyectoGestion/fonts/fuente.fnt", NULL);
	consoleFont.pal = loadFile("/proyectoGestion/fonts/fuente.pal", &palSize);
	consoleFont.numChars = 256;
	consoleFont.numColors = palSize >> 1;
	consoleFont.bpp = 8;
	consoleFont.asciiOffset = 0;
	consoleFont.convertSingleColor = false;
	
	// Inicializa la pantalla superior
	// Bancos 4-7: Bitmap 8 bits (64KB)
	// Banco 3: Consola de texto (16KB)
	// Banco 2: ultimos KB para el mapa de la consola de texto
	videoSetMode(MODE_5_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankE(VRAM_E_LCD);
	bgExtPaletteEnable();
	consoleInit(&topConsole, 0, BgType_Text8bpp, BgSize_T_256x256, 22, 3, true, false);
	consoleSetFont(&topConsole, &consoleFont);														// Por defecto lo copia en la paleta comun
	dmaCopyBuffer(&VRAM_E_EXT_PALETTE[0][0], consoleFont.pal, 512);									// Copia la paleta en VRAM (capa 0, slot 0)
	vramSetBankE(VRAM_E_BG_EXT_PALETTE);
	bitmapLayerTop = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 4, 0);
	bgWrapOn(bitmapLayerTop);
	oamInit(&oamMain, SpriteMapping_Bmp_1D_128, false);
	
	// Inicializa la pantalla inferior
	// Bancos 4-7: Bitmap 8 bits (64KB)
	// Banco 3: Consola de texto (16KB)
	// Banco 0-2: Teclado (40KB) + mapas del teclado y la consola de texto en los 8KB restantes
	// 4KB mapa teclado + 2KB mapa consola de texto = 6KB (nos sobran 2KB)
	videoSetModeSub(MODE_5_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_SUB_SPRITE);
	vramSetBankH(VRAM_H_LCD);
	bgExtPaletteEnableSub();
	consoleInit(&bottomConsole, 0, BgType_Text8bpp, BgSize_T_256x256, 22, 3, false, false);
	consoleSetFont(&bottomConsole, &consoleFont);
	dmaCopyBuffer(&VRAM_H_EXT_PALETTE[0][0], consoleFont.pal, 512);
	teclado = keyboardInit(NULL, 1, BgType_Text4bpp, BgSize_T_256x512, 20, 0, false, true);			// Ojo que el teclado ocupa 40KB (3 bancos de tiles)
	teclado->OnKeyPressed = keyPressed;
	vramSetBankH(VRAM_H_SUB_BG_EXT_PALETTE);
	bitmapLayerBottom = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 4, 0);
	bgWrapOn(bitmapLayerBottom);
	oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);
	
	// Por defecto, escribe en la consola de arriba
	consoleSelect(&topConsole);
}

// Carga una imagen de 8 bits y muestrala en pantalla
void loadBitmap8(const char *file, u8 screen){
	
	// Carga los archivos
	u32 gfxSize, palSize;
	char path[256];
	sprintf(path, "%s.img", file);
	void *gfxData = loadFile(path, &gfxSize);
	sprintf(path, "%s.pal", file);
	u16 *palData = loadFile(path, &palSize);
	
	// Copia la imagen a la VRAM
	if(screen){						// Pantalla inferior
		dmaCopyBuffer(bgGetGfxPtr(bitmapLayerBottom), gfxData, gfxSize);
		dmaCopyBuffer(BG_PALETTE_SUB, palData, palSize);
		REG_DISPCNT_SUB |= DISPLAY_BG3_ACTIVE;
		REG_BG3X_SUB = 0;
		REG_BG3Y_SUB = 0;
	} else {						// Pantalla superior
		dmaCopyBuffer(bgGetGfxPtr(bitmapLayerTop), gfxData, gfxSize);
		dmaCopyBuffer(BG_PALETTE, palData, palSize);
		REG_DISPCNT |= DISPLAY_BG3_ACTIVE;
		REG_BG3X = 0;
		REG_BG3Y = 0;
	}
	
	// Libera los datos temporales
	free(gfxData);
	free(palData);
}

// Carga un sprite de 16 bits y muestralo en pantalla
sprite *loadSprite(const char *file, u8 screen, u8 index, SpriteSize size){
	
	// Carga el sprite en RAM
	u32 gfxSize;
	void *gfxData = loadFile(file, &gfxSize);
	
	// Crea la estructura del sprite
	sprite *spr = (sprite*) allocate(sizeof(sprite));
	
	// Carga el sprite en VRAM
	if(screen){
		spr->gfx = oamAllocateGfx(&oamSub, size, SpriteColorFormat_Bmp);
	} else {
		spr->gfx = oamAllocateGfx(&oamMain, size, SpriteColorFormat_Bmp);
	}
	dmaCopyBuffer(spr->gfx, gfxData, gfxSize);
	
	// Guarda los datos
	spr->screen = screen;
	spr->size = size;
	spr->id = index;
	
	// Libera el sprite de RAM
	free(gfxData);
	
	// Devuelve el sprite cargado
	return spr;
}

// Libera un sprite cargado anteriormente
void freeSprite(sprite *spr){
	
	// Comprueba que tenemos un sprite
	if(spr == NULL) return;
	
	// Selecciona la OAM
	OamState *oam = &oamMain;
	if(spr->screen) oam = &oamSub;
	
	// Esconde el sprite
	oamClearSprite(oam, spr->id);
	swiWaitForVBlank();
	oamUpdate(oam);
	
	// Libera los datos
	if(spr->gfx && !spr->cloned) oamFreeGfx(oam, spr->gfx);
	free(spr);
}

// Actualiza un sprite en la OAM
void updateSprite(sprite *spr){
	
	// Comprueba que tenemos un sprite y no es un clon
	if(spr == NULL) return;
	
	// Selecciona la OAM
	OamState *oam = &oamMain;
	if(spr->screen) oam = &oamSub;
	
	// Actualiza el sprite en la OAM seleccionada
	oamSet(oam, spr->id, spr->x, spr->y, 0, 31, spr->size, SpriteColorFormat_Bmp, spr->gfx, -1, true, false, false, false, false);
}

// Comprueba si un sprite ha sido tocado
u8 spriteTouched(sprite *spr, u8 width, u8 height){
	if(spr == NULL) return 0;
	return ((keyDown &KEY_TOUCH) && (touchXY.px > spr->x) && (touchXY.px < (spr->x + width)) && 
									(touchXY.py > spr->y) && (touchXY.py < (spr->y + height)));
}

// Clona un sprite
sprite *cloneSprite(sprite *sprsrc, u8 index){
	sprite *spr = (sprite*) allocate(sizeof(sprite));
	spr->gfx = sprsrc->gfx;
	spr->x = sprsrc->x;
	spr->y = sprsrc->y;
	spr->screen = sprsrc->screen;
	spr->id = index;
	spr->size = sprsrc->size;
	spr->cloned = true;
	return spr;
}

// Carga un archivo en RAM
void *loadFile(const char *file, u32 *fileSize){
	
	// Abre el archivo
	FILE *f = fopen(file, "rb");
	if(f == NULL) fileNotFoundError(file);
	
	// Obten el tamaño del archivo
	fseek(f, 0, SEEK_END);
	u32 size = ftell(f);
	rewind(f);
	
	// Lee el archivo
	void *data = allocate(size);
	fread(data, size, 1, f);
	
	// Cierra el archivo
	fclose(f);
	
	// Guarda el tamaño del archivo, si es necesario
	if(fileSize) *fileSize = size;
	
	// Devuelve los datos cargados
	return data;
}

// Carga un archivo en una region de memoria
void loadFileTo(const char *file, void *addr){
	
	// Carga el archivo en RAM
	u32 size;
	void *data = loadFile(file, &size);
	
	// Copia los datos por DMA al destino
	dmaCopyBuffer(addr, data, size);
	
	// Libera los datos temporales
	free(data);
}

// Copia datos por el canal DMA (de forma sincrona)
// Funcion basada en el siguiente articulo:
// http://www.coranac.com/2009/05/dma-vs-arm9-fight/
void dmaCopyBuffer(void* dest, const void* source, u32 size){

	// Datos de origen y destino
	u32 src = (u32)source;
	u32 dst = (u32)dest;

	// Verifica si los datos estan alineados
	if ((src | dst) & 1) {

		// No lo estan, copia normal
		memcpy(dest, source, size);

	} else {

		// Espera a que el canal 3 este libre
		while (dmaBusy(3));

		// Manda el cache a la memoria
		DC_FlushRange(source, size);

		// Dependiendo de la alineacion de datos, selecciona el metodo de copia
		if ((src | dst | size) & 3) {
			 dmaCopyHalfWords(3, source, dest, size);		// 16 bits
		} else {
			dmaCopyWords(3, source, dest, size);			// 32 bits
		}

		// Evita que el destino sea almacenado en cache
		DC_InvalidateRange(dest, size);
	}
}
