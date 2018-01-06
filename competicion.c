//PRÁCTICA FINAL
//SISTEMAS OPERATIVOS
//
//ÁLVARO CELADA CELADA
//DAVID MARTÍNEZ LÓPEZ
//MARIO HERMIDA LÓPEZ

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>


#define NUMATLETAS 10
#define NUMTARIMAS 2

//Funciones que emplearemos
void writeLogMessage (char *id,char *msg);
int generaAleatorio(int max, int min);
void *accionesTarima(void* manejadora);
void *accionesAtleta(void* manejadora);



//Variables globales
int numeroAtleta;
int competicion;
int contador;
int ganador;
int colaAtletas[10];
int puntuacionGanador;
char id[10];
char msg[100];


//Structs
struct atletas {
	int indispuesto; 
	int id; 
	int ha_Competido;
	int tarima_Asignada; 
	int necesita_Beber;
	pthread_t atletaHilo;
	int puntuacion;
};

//Almacenamos todos los atletas
struct atletas *punteroAtletas; 


struct tarimas {
	int id;
	int correcto;
	int indumentaria;
	int nulo;
	int ocupada;/
	int descansa; //Cada 2 atletas
	int agua;
};

//Almacenamos todas las tarimas
struct tarimas *punteroTarimas


//Semaforos
pthread_mutex_t controladorColaTarima; //No hay dos en la misma posicion
pthread_mutex_t controladorTarima; //No hay dos en la misma tarima
pthread_mutex_t controladorLog; //No hay dos escribiendo en el fichero
pthread_mutex_t pulso;
pthread_mutex_t controladorJuez;






void* accionesTarima(void* manejadora) {
	/*	1. Primer atleta el maximo tiempo, si no de otra tarima
		2. Cambiar flag
		3. Actuacón atleta
		4. Logear hora inicio levantamiento
		5. Dormir tiempo de levantamiento
		6. Agua? y cambiar flag
		7. Logear hora fin levantamiento
		8. Logear puntuacion
		9. Cambiar flag ha_Competido
		10. Contador atletas +1
		11. Comprobar descanso jueces
		12. Paso 1
	*/

}


void* accionesAtleta(void* manejadora) {
	/*	1. Logear hora y tarima
		2. Problemas atleta
			a) No levanta, fin de hilo y libera espacio cola
			b) Dormir 3 segs y volver a 2
		3. Si coompite esperamos a que termine
		4. Logear hora de fin (y puntuación?)
		5. Final hilo
	*/


}




void writeLogMessage(char *id,char *msg) {
	/*Calculamos la hora actual*/
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow,19,"%d %m %y %H: %M: %S",tlocal);
	
	/*Escribimos en el log*/
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
		
}




int generaAleatorio(int max, int min) {
	srand(time(NULL));
	int numeroAleatorio = rand()%((max+1)-min)+min;
	return numeroAleatorio;
