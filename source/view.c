// Includes C
#include <stdio.h>

// Includes NDS
#include <nds.h>

// Includes propios
#include "view.h"
#include "graphics.h"
#include "common.h"
#include "menu.h"
#include "snmp.h"
#include "config.h"
#include "tftp.h"
#include "interfaces.h"

// Defines del microfono
#define MIC_SAMPLERATE 8000
#define MAX_AUDIO_LENGTH 10
#define SOUND_BUFFER_SIZE (MIC_SAMPLERATE * 2 * MAX_AUDIO_LENGTH)
#define MIC_BUFFER_SIZE (MIC_SAMPLERATE * 2 / 30)			// Para que se llene cada frame por el ARM7

// Estructura de la cabecera del formato WAV
// http://soundfile.sapp.org/doc/WaveFormat/
typedef struct{
	char ChunkID[4];
	u32 ChunkSize;
	char Format[4];
	char Subchunk1ID[4];
	u32 Subchunk1Size;
	u16 AudioFormat;
	u16 NumChannels;
	u32 SampleRate;
	u32 ByteRate;
	u16 BlockAlign;
	u16 BitsPerSample;
	char Subchunk2ID[4];
	u32 Subchunk2Size;
}WAV_Header_t;
static WAV_Header_t wav_header = {
	"RIFF", 0, "WAVE", "fmt ", 16, 1, 1, MIC_SAMPLERATE, MIC_SAMPLERATE * 1 * 16/8, 1 * 16/8, 16, "data", 0
};

// Variables locales
static u32 mic_length = 0;
static u16 *mic_buffer = NULL;
static u16 *sound_buffer = NULL;

// Handler del microfono
void micHandler(void *data, int length){
	
	// Comprueba que el buffer existe y no esta lleno
	if(sound_buffer == NULL || mic_length > SOUND_BUFFER_SIZE) return;
	
	// Copia los datos al buffer de audio
	DC_InvalidateRange(data, length);
	dmaCopy(data, ((u8*)sound_buffer) + mic_length + sizeof(wav_header), length);
	
	// Actualiza el contador de bytes leidos
	mic_length += length;
}

// Envia los traps por TFTP
static void enviarTraps(){
	
	// Crea los buffers para el microfono
	sound_buffer = (u16*) allocate (sizeof(wav_header) + SOUND_BUFFER_SIZE);
	mic_buffer = (u16*) allocate (MIC_BUFFER_SIZE);
	
	// Graba el mensaje
	consoleClear();
	iprintf("\x1b[19;2HManten A para grabar");
	mic_length = 0;
	
	// Espera a que pulse A
	u8 salir = 0;
	int audioID;
	while(!salir){
		
		// Si pulsa A, comienza a grabar
		if(keysDown() &KEY_A){
			soundMicRecord(mic_buffer, MIC_BUFFER_SIZE, MicFormat_12Bit, MIC_SAMPLERATE, micHandler);
		}
		
		// Si sueltas A, deja de grabar
		if(keysUp() &KEY_A){
			soundMicOff();
			soundEnable();
			audioID = soundPlaySample(sound_buffer, SoundFormat_16Bit, mic_length, MIC_SAMPLERATE, 127, 64, false, 0);
			salir = 1;
		}
		
		scanKeys();
		swiWaitForVBlank();
	}
	
	// Rellena la cabecera WAV
	wav_header.Subchunk2Size = mic_length;
	wav_header.ChunkSize = 36 + wav_header.Subchunk2Size;
	dmaCopy((void*)&wav_header, sound_buffer, sizeof(wav_header));
	
	// Espera los ciclos necesarios hasta que termine de reproducirse
	u32 ciclos = (mic_length / (MIC_SAMPLERATE * 2) + 1) * 60;
	u32 i;
	for(i = 0; i < ciclos; i++){
		swiWaitForVBlank();
	}
	soundKill(audioID);			// Para el audio y liberalo
	
	// Envia el audio al servidor TFTP
	consoleClear();
	iprintf("\x1b[19;2HEnviando audio a %s", ip_tftp);
	int result = TFTP_SendFile("audio.wav", sound_buffer, mic_length + sizeof(wav_header));
	if(result < 0){
		consoleClear();
		iprintf("\x1b[19;2HError enviando el audio (%d)", result);
		free(sound_buffer);
		free(mic_buffer);
		return;
	}
	
	// Libera los datos del audio
	free(sound_buffer);
	free(mic_buffer);
	
	// Muestra un mensaje de "Enviando..."
	consoleClear();
	iprintf("\x1b[19;2HEnviando traps a %s", ip_tftp);
	
	// Abre el archivo de traps
	FILE *f = fopen(trapFile, "rb");
	if(f == NULL){
		consoleClear();
		iprintf("\x1b[19;2HNo se encuentra el archivo");
		return;
	}
			
	// Cargalo en RAM
	fseek(f, 0, SEEK_END);
	u32 size = ftell(f);
	rewind(f);
	void *data = allocate (size);
	fread(data, size, 1, f);
	fclose(f);
	
	// Envia el archivo por TFTP
	result = TFTP_SendFile("traps.html", data, size);
	if(result == 0){
		consoleClear();
		iprintf("\x1b[19;2HSe ha enviado correctamente");
	} else {
		consoleClear();
		iprintf("\x1b[19;2HError enviando traps (%d)", result);
	}
	
	// Libera los datos usados
	free(data);
}

// Carga la pantalla de visualizacion
void cargarView(){
	
	// Carga las imagenes necesarias
	loadBitmap8("proyectoGestion/images/view", 0);
	loadBitmap8("proyectoGestion/images/view_sub", 1);
	
	// Selecciona la consola de texto
	consoleSelect(&bottomConsole);
	iprintf("\x1b[19;5HSelecciona una opcion");
}

// Estado de la pantalla de visualizacion
void estado_view(){
	
	// Si tocas la pantalla
	if(keyDown &KEY_TOUCH){
		
		// Si tocas "Borrar traps", borra el archivo de los traps
		if(touchXY.px >= 142 && touchXY.px <= 241 && touchXY.py >= 22 && touchXY.py <= 59){
			consoleClear();
			if(remove(trapFile) == 0){
				iprintf("\x1b[19;2HFichero borrado exitosamente");
			} else {
				iprintf("\x1b[19;2HNo se pudo borrar el archivo");
			}
		}
		
		// Si tocas "Mandar traps", manda el archivo de traps por TFTP
		else if(touchXY.px >= 142 && touchXY.px <= 243 && touchXY.py >= 70 && touchXY.py <= 106){
			enviarTraps();
		}
		
		// Si tocas "Visualizar tabla de interfaces", ve a esa pantalla
		else if(touchXY.px >= 14 && touchXY.px <= 127 && touchXY.py >= 22 && touchXY.py <= 111){
			cambiarEstado(interfaces_printTable, cargarInterfaces, eliminarView);
		}
	}
	
	// Si pulsas B, vuelve al menu
	else if(keyDown &KEY_B){
		cambiarEstado(estado_menu, cargarMenu, eliminarView);
	}
}

// Elimina la pantalla de visualizacion
void eliminarView(){
	
	// Esconde los fondos
	bgHide(bitmapLayerTop);
	bgHide(bitmapLayerBottom);
	
	// Limpia la consola
	consoleClear();
}
