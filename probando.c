//LIBRERIAS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

//VARIABLES GLOBALES
pthread_mutex_t semaforoNuevoCompetidor;
int numeroAtletasCompeticion;
int contadorAtletas;
int finalizaCompeticion;

//FUNCIONES
void nuevoCompetidor(int sig);
void inicializaVariablesAtleta(int posicionPuntero, int id, int indispuesto);
void *AccionesAtleta(void *arg);
void finalizarCompeticion(int sig);

//STRUCTS
struct atleta {
	pthread_t atletaHilo;
	int id;
	int indispuesto;
	int tarimaAsignada;
};

struct atleta *punteroAtletas;

int main() {
	//Registramos las signal
	if(signal(SIGUSR1, nuevoCompetidor)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGUSR2, nuevoCompetidor)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGINT, finalizarCompeticion)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	//Inicializamos los recursos
	numeroAtletasCompeticion = 0;
	contadorAtletas = 0;
	finalizaCompeticion = 0;

	//Reservamos memoria para el punteroAtletas
	punteroAtletas = (struct atleta*)malloc(10*sizeof(struct atleta)); 
	
	//El programa espera hasta recibir una signal, en cuanto reciba SIGINT se finaliza el programa
	while(finalizaCompeticion != 1) {
		pause();
	}
	return 0;
}

void nuevoCompetidor(int a){
	if(signal(SIGUSR1,nuevoCompetidor)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidor)==SIG_ERR){
		exit(-1);
	}
	numeroAtletasCompeticion++;
	if (numeroAtletasCompeticion < 11) {
		inicializaVariablesAtleta(contadorAtletas, contadorAtletas+1, 0);
		printf("Voy a proceder a crear el hilo %d\n", numeroAtletasCompeticion);
		//PROBLEMA AL CREAR EL HILO
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);
		contadorAtletas++;
		printf("Se acaba de crear el hilo %d\n", numeroAtletasCompeticion);
	}
}

void inicializaVariablesAtleta(int posicionPuntero, int id, int indispuesto){
	punteroAtletas[posicionPuntero].id = id;
	punteroAtletas[posicionPuntero].indispuesto = indispuesto;
}

void *AccionesAtleta(void *arg) {  //el parametro que le pasamos por argumento es el id que lo utilizaremos mas tarde para escribir en el log
	printf("El hilo se ha creado correctamente.\n");
	printf("Mi id de atleta es %d.\n", (int *)punteroAtletas[arg].id);
}

void finalizarCompeticion(int sig) {
	printf("La competición ha finalizado.\n");
	finalizaCompeticion = 1;
}
