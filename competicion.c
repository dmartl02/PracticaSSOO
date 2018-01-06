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

/*Funciones que emplearemos*/
void writeLogMessage (char *id,char *msg);
int generaAleatorio(int max, int min)


/*Variables globales*/
pthread_t juez;
int numeroAtleta;
int competicion;
int contador;
int ganador;
int colaAtletas[10];
int puntuacionGanador;
char id[10];
char msg[100];




/*Structs*/
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
	int descansa; /*Cada 2 atletas*/
	int agua;
};

//Almacenamos todas las tarimas
struct tarimas *punteroTarimas












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
