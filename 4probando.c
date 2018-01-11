//LIBRERIAS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

//CONSTANTES
#define ATLETAS 10
#define TARIMAS 2

//VARIABLES GLOBALES
pthread_mutex_t entradaCompeticion;
pthread_mutex_t escritura;
pthread_mutex_t entradaTarima;
int numeroAtletasCompeticion;
int contadorAtletas;
int finalizaCompeticion;
int colaTarimas[10];
int podio[3];
char id[10];
char msg[100];
FILE *logFile;
char *logFileName ="registroTiempos.log";

//FUNCIONES
void nuevoCompetidorATarima1(int sig);
void nuevoCompetidorATarima2(int sig);
void inicializaVariablesAtleta(int posicionPuntero, int id, int indispuesto, int tarima);
void *AccionesAtleta(void *arg);
void *AccionesTarima(void *arg);
void writeLogMessage(char *id,char *msg);
void finalizarCompeticion(int sig);
int generarAleatorio(int min, int max);

//STRUCTS
struct atleta {
	pthread_t atletaHilo;
	int id;
	int ha_Competido;
	int tarima_Asignada;
	int necesita_Beber;
	int puntuacion;
};

struct atleta *punteroAtletas;

struct tarima {
	pthread_t tarimaHilo;
	int id;
	int ocupada;
};

struct tarima *punteroTarimas;

int main() {
	//Registramos las signal
	if(signal(SIGUSR1, nuevoCompetidorATarima1)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGUSR2, nuevoCompetidorATarima2)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGINT, finalizarCompeticion)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	//Inicializamos los recursos
	if (pthread_mutex_init(&entradaCompeticion, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&escritura, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&entradaTarima, NULL) != 0) exit(-1);
	numeroAtletasCompeticion = 0;
	contadorAtletas = 0;
	finalizaCompeticion = 0;
	
	//Reservamos memoria para punteroAtletas y para punteroTarima
	punteroAtletas = (struct atleta*)malloc(ATLETAS*sizeof(struct atleta)); 
	punteroTarimas = (struct tarima*)malloc(TARIMAS*sizeof(struct tarima));

	//Creamos los 2 threads de tarimas
	int i;
	for (i = 0; i < TARIMAS; i++) {
		punteroTarimas[i].id = i+1;
		punteroTarimas[i].ocupada = 0;
		pthread_create(&punteroTarimas[i].tarimaHilo, NULL, AccionesTarima, (void *)&punteroTarimas[i].id);
	}

	//Creamos el archivo para los log en caso de que no exista, si existe lo sobreescribimos
	logFile = fopen(logFileName, "w");
	if(logFile == NULL){
		char *err = strerror(errno);
		printf("%s", err);
		fclose(logFile);
	}
	writeLogMessage("Comienzo","Competicion de powerlifting");

	//El programa espera hasta recibir una signal, en cuanto reciba SIGINT se finaliza el programa
	while(finalizaCompeticion != 1) {
		pause();
	}
	printf("El número de atletas en la competición es %d.\n", contadorAtletas);
	return 0;
}

//Al recibir la signal SIGUSR1
void nuevoCompetidorATarima1(int a){
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	pthread_mutex_lock(&entradaCompeticion);
	if (numeroAtletasCompeticion < ATLETAS) {
		numeroAtletasCompeticion++;
		//Inicializamos variables de atleta
		punteroAtletas[contadorAtletas].id = contadorAtletas+1;
		punteroAtletas[contadorAtletas].ha_Competido = 0;
		punteroAtletas[contadorAtletas].tarima_Asignada = 1;
		punteroAtletas[contadorAtletas].necesita_Beber = 0;
		punteroAtletas[contadorAtletas].puntuacion = 0;
		//Creamos el hilo
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);
		contadorAtletas++;
	}
	pthread_mutex_unlock(&entradaCompeticion);
}

//Al recibir la signal SIGUSR2
void nuevoCompetidorATarima2(int a){
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}
	pthread_mutex_lock(&entradaCompeticion);
	if (numeroAtletasCompeticion < ATLETAS) {
		numeroAtletasCompeticion++;
		//Inicializamos variables de atleta
		punteroAtletas[contadorAtletas].id = contadorAtletas+1;
		punteroAtletas[contadorAtletas].ha_Competido = 0;
		punteroAtletas[contadorAtletas].tarima_Asignada = 2;
		punteroAtletas[contadorAtletas].necesita_Beber = 0;
		punteroAtletas[contadorAtletas].puntuacion = 0;
		//Creamos el hilo
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);
		contadorAtletas++;
	}
	pthread_mutex_unlock(&entradaCompeticion);
}

//Metodo para decir que hace el atleta al ser creado
void *AccionesAtleta(void *arg) {
	int idAtleta = *(int *)arg;
	//Guardamos los atletas en la cola
	
	//Escribimos en el log que ha entrado un nuevo atleta
	pthread_mutex_lock(&escritura);
	sprintf(id, "atleta_%d", idAtleta);
	sprintf(msg, "Entra en la competicion, su tarima asignada es %d", punteroAtletas[idAtleta-1].tarima_Asignada);
	writeLogMessage(id, msg);
	pthread_mutex_unlock(&escritura);
	//Comportamiento del atleta
	int num;
	while(1) {//En esperas activas como esta, realiar un sleep() para no petar el procesador
			num = generarAleatorio(1, 100);
			if (num < 16) {
				printf("El thread ha finalizado, numero %d, mi id de hilo es %d.\n", num, idAtleta);
				numeroAtletasCompeticion--;
				pthread_mutex_lock(&escritura);
				sprintf(id, "atleta_%d", idAtleta);
				sprintf(msg, "Se retira por deshidratacion y falta de electrolitos");
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&escritura);
				pthread_exit(NULL);
			}
			sleep(3);
			/*if (punteroAtletas[*(int *)arg].ha_Competido == 1) {
				pthread_wait();
			}*/
		}
}

void *AccionesTarima(void *arg) {
	int idTarima = *(int *)arg;
	int atletaActual;
	//Buscamos el atleta para competir
	while(1) {
		pthread_mutex_lock(&entradaTarima);
		sleep(3);
		pthread_mutex_unlock(&entradaTarima);
	}
	//Cambiamos el flag
	punteroTarimas[idTarima].ocupada = 1;
	
	//Levantamiento del atleta
	
	//Guardamos en el log la hora del levantamiento

	//Dormimos a)80% mov valido (2 a 6 s)  b)10% nulo indumentaria (1 a 4 s) c)10% nulo falta de fuerza (6 a 10 s)
	
	//
}

//Metodo para escribir en el log
void writeLogMessage(char *id, char *msg){
	//Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
	
	//Escribimos en el log
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);	
}

//Cuando recibimos la signal SIGINT debemos finalizar el programa
void finalizarCompeticion(int sig) {
	printf("La competicion ha finalizado.\n");
	writeLogMessage("Final", "La competicion ha finalizado");
	finalizaCompeticion = 1;
}

//Metodo para generar numero aleatorios
int generarAleatorio(int min, int max){
	srand(time(NULL));
	int aleatorio = rand()%((max+1)-min)+min;
	return aleatorio;
}
