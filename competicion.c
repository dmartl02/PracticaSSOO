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

	//pthread_t caja; /*hilo del box*/
	int id;
	int correcto;
	int indumentaria;
	int nulo;
	int ocupada;/
	int descansa; /*Cada 2 atletas*/
	int agua;
};