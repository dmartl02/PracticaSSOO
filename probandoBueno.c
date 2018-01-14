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
pthread_mutex_t entradaCompeticion;  //protege los accesos al array punteroAtletas
pthread_mutex_t semaforoLog;  //protege los accesos al fichero de log
pthread_mutex_t semaforoCola;
int contadorAtletas; 
int finalizaCompeticion;  
int atletasTarima1;  //NO PUEDE SER GLOBAL
int atletasTarima2;  //NO PUEDE SER GLOBAL
int colaTarimas[ATLETAS];
int mejoresPuntuaciones[3];
int mejoresIds[3];
FILE *logFile;
char *logFileName ="registroTiempos.log";

//FUNCIONES
void nuevoCompetidor(int sig);
void inicializaVariablesAtleta(int posicionPuntero, int id, int indispuesto, int tarima);
void *AccionesAtleta(void *arg);
void *AccionesTarima(void *arg);
void comprobarPodio(int puntuacion, int id);
void writeLogMessageThreadSafe(char *id,char *msg);
void finalizarCompeticion(int sig);
int generarAleatorio(int min, int max);

//STRUCTS
struct atleta {
	pthread_t atletaHilo;
	int id;  //numero de atletas (de 1 a ATLETAS)
	int ha_Competido;
	int tarima_Asignada;
	int necesita_Beber;
};

struct atleta *punteroAtletas;

int main() {
	char id[10];  //identificador para escribir en el log
	char msg[100];  //mensaje que se escribe en el log
	pthread_t tarima1, tarima2;
	int idTarima1, idTarima2;

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

	//Aleatorizacion de la semilla para generar numeros aleatorios
	srand(time(NULL));
	
	//Inicializamos los recursos
	if (pthread_mutex_init(&entradaCompeticion, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoLog, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoCola, NULL) != 0) exit(-1);

	idTarima1 = 1;
	idTarima2 = 2;
	contadorAtletas = 0;  //posicion de los atletas (de 0 a ATLETAS-1)
	finalizaCompeticion = 0;
	atletasTarima1 = 0;  //CAMBIAR ESTA VARIABLE
	atletasTarima2 = 0;  //CAMBIAR ESTA VARIABLE

	int i;
	for (i = 0; i < ATLETAS; i++) {  //marcamos con ceros las posiciones libres
		colaTarimas[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		mejoresPuntuaciones[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		mejoresIds[i] = 0;
	}
	
	//Reservamos memoria para punteroAtletas 
	punteroAtletas = (struct atleta*)malloc(ATLETAS*sizeof(struct atleta)); 

	//Creamos los 2 threads de tarimas
	pthread_create(&tarima1, NULL, AccionesTarima, (void *)&idTarima1);
	pthread_create(&tarima2, NULL, AccionesTarima, (void *)&idTarima2);

	//Creamos el archivo para los log en caso de que no exista, si existe lo sobreescribimos
	logFile = fopen(logFileName, "w");
	if(logFile == NULL){
		char *err = strerror(errno);
		printf("%s", err);
		fclose(logFile);
	}
	writeLogMessageThreadSafe("Comienzo","Competicion de powerlifting");

	//El programa espera hasta recibir una signal, en cuanto reciba SIGINT se finaliza el programa
	while(finalizaCompeticion != 1) {
		pause();
	}

	return 0;
}

//Al recibir la signal SIGUSR1 o SIGUSR2
void nuevoCompetidor(int sig){
	if(signal(SIGUSR1,nuevoCompetidor)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidor)==SIG_ERR){
		exit(-1);
	}

	if (contadorAtletas < ATLETAS) {

		pthread_mutex_lock(&entradaCompeticion);

		//Inicializamos variables de atleta
		punteroAtletas[contadorAtletas].id = contadorAtletas+1;
		punteroAtletas[contadorAtletas].ha_Competido = 0;
		if (sig == 10) {  //SIGUSR1
			punteroAtletas[contadorAtletas].tarima_Asignada = 1;
		} 
		if (sig == 12) {  //SIGUSR2
			punteroAtletas[contadorAtletas].tarima_Asignada = 2;
		}
		punteroAtletas[contadorAtletas].necesita_Beber = 0;

		//Creamos el hilo de atleta
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);

		contadorAtletas++;

		pthread_mutex_unlock(&entradaCompeticion);
	}
}

void *AccionesAtleta(void *arg) {
	char id[10];
	char msg[100];
	int idAtleta = *(int *)arg;  //numero del atleta (de 1 a ATLETAS)
	int i, num;

	//Escribimos en el log que ha entrado un nuevo atleta y cual es su tarima
	sprintf(id, "atleta_%d", idAtleta);
	sprintf(msg, "Entra en la competicion, su tarima asignada es %d", punteroAtletas[idAtleta-1].tarima_Asignada);
	writeLogMessageThreadSafe(id, msg);

	//Asignamos a cada atleta su posicion en la cola
	pthread_mutex_lock(&semaforoCola);
	colaTarimas[idAtleta-1] = idAtleta;
	pthread_mutex_unlock(&semaforoCola);

	//Comportamiento del atleta
	while(punteroAtletas[idAtleta-1].ha_Competido == 0) {  //Comprobamos el pulso del atleta cada 3 segundos
		num = generarAleatorio(1, 100);

		if (num <= 15) {
			punteroAtletas[idAtleta-1].ha_Competido = 1;

			sprintf(id, "atleta_%d", idAtleta);
			sprintf(msg, "Se retira por deshidratacion y falta de electrolitos");
			writeLogMessageThreadSafe(id, msg);

			pthread_cancel(punteroAtletas[idAtleta-1].atletaHilo);
		}
		sleep(3);
	}
}

void *AccionesTarima(void *arg) {
	char id[10];
	char msg[100];
    int idTarima = *(int *)arg;
    int atletasAtendidos;
	int atletaActual;  //0 para el primer atleta
    int eventoAleatorio, duermeAleatorio, puntuacionAleatorio, aguaAleatorio;
    int i, tarimaOcupada = 0;

	while(tarimaOcupada == 0) {
		for (i = 0; i < ATLETAS; i++) {
			if (colaTarimas[i] != 0 && punteroAtletas[i].tarima_Asignada == idTarima && punteroAtletas[i].ha_Competido == 0) {  //Si la tarima asignada al atleta coincide con la tarima actual 

				//Calculamos lo que sucede con el atleta
	            eventoAleatorio = generarAleatorio(1, 100);

				punteroAtletas[i].ha_Competido = 1;

				//Inicio levantamiento
	            sprintf(id, "atleta_%d", colaTarimas[i]);
	            sprintf(msg, "Inicio del levantamiento");
	            writeLogMessageThreadSafe(id, msg);

		        if (eventoAleatorio <= 80) {  //Movimiento valido
					puntuacionAleatorio = generarAleatorio(60, 300);
			
	                duermeAleatorio = generarAleatorio(2, 6);
	                sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
	                sprintf(id, "atleta_%d", colaTarimas[i]);
	                sprintf(msg, "Fin del levantamiento, movimiento valido, puntuacion = %d, tiempo = %d", puntuacionAleatorio, duermeAleatorio);
	                writeLogMessageThreadSafe(id, msg);

					//Vemos si la puntuacion es digna de podio
					comprobarPodio(puntuacionAleatorio, i);

					atletasAtendidos++;

					pthread_cancel(punteroAtletas[i].atletaHilo);

	            } else if (eventoAleatorio > 80 && eventoAleatorio <= 90 ) {  //Nulo por indumentaria
					puntuacionAleatorio = 0;

	                duermeAleatorio = generarAleatorio(1, 4);
	                sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
	                sprintf(id, "atleta_%d", colaTarimas[i]);
	                sprintf(msg, "Fin del levantamiento, movimiento nulo por indumentaria, puntuacion = 0, tiempo = %d", duermeAleatorio);
	                writeLogMessageThreadSafe(id, msg);

					//Vemos si la puntuacion es digna de podio
					comprobarPodio(puntuacionAleatorio, i);

					atletasAtendidos++;

					pthread_cancel(punteroAtletas[i].atletaHilo);

	            } else {  //Nulo por falta de fuerza
					puntuacionAleatorio = 0;

	                duermeAleatorio = generarAleatorio(6, 10);
	                sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
	                sprintf(id, "atleta_%d", colaTarimas[i]);
	                sprintf(msg, "Fin del levantamiento, movimiento nulo por falta de fuerza, puntuacion = 0, tiempo = %d", duermeAleatorio);
	                writeLogMessageThreadSafe(id, msg);

					//Vemos si la puntuacion es digna de podio
					comprobarPodio(puntuacionAleatorio, i);

					atletasAtendidos++;

					pthread_cancel(punteroAtletas[i].atletaHilo);
				
	            }

				atletasTarima1++;

				punteroAtletas[i].ha_Competido = 2;

	            if (atletasAtendidos == 4) {  //Miramos si le toca descansar a los jueces
	                sprintf(id, "juez_%d", idTarima);
	                sprintf(msg, "Inicio del descanso");
	                writeLogMessageThreadSafe(id, msg);

	                sleep(10);

	                sprintf(id, "juez_%d", idTarima);
	                sprintf(msg, "Fin del descanso");
	                writeLogMessageThreadSafe(id, msg);

	                atletasAtendidos = 0;
	            }

			} else if (colaTarimas[i] != 0 && punteroAtletas[i].ha_Competido == 0) {

				//Calculamos que sucede con el atleta
	            eventoAleatorio = generarAleatorio(1, 100);

				punteroAtletas[i].ha_Competido = 1;

				//Inicio levantamiento
	            sprintf(id, "atleta_%d", colaTarimas[i]);
	            sprintf(msg, "Inicio del levantamiento");
	            writeLogMessageThreadSafe(id, msg);

		        if (eventoAleatorio <= 80) {  //Movimiento valido
					puntuacionAleatorio = generarAleatorio(60, 300);
			
	                duermeAleatorio = generarAleatorio(2, 6);
	                sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
	                sprintf(id, "atleta_%d", colaTarimas[i]);
	                sprintf(msg, "Fin del levantamiento, movimiento valido, puntuacion = %d, tiempo = %d", puntuacionAleatorio, duermeAleatorio);
	                writeLogMessageThreadSafe(id, msg);

					//Vemos si la puntuacion es digna de podio
					comprobarPodio(puntuacionAleatorio, i);

					atletasAtendidos++;

					pthread_cancel(punteroAtletas[i].atletaHilo);

	            } else if (eventoAleatorio > 80 && eventoAleatorio <= 90 ) {  //Nulo por indumentaria
					puntuacionAleatorio = 0;

	                duermeAleatorio = generarAleatorio(1, 4);
	                sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
	                sprintf(id, "atleta_%d", colaTarimas[i]);
	                sprintf(msg, "Fin del levantamiento, movimiento nulo por indumentaria, puntuacion = 0, tiempo = %d", duermeAleatorio);
	                writeLogMessageThreadSafe(id, msg);

					//Vemos si la puntuacion es digna de podio
					comprobarPodio(puntuacionAleatorio, i);

					atletasAtendidos++;

					pthread_cancel(punteroAtletas[i].atletaHilo);

	            } else {  //Nulo por falta de fuerza
					puntuacionAleatorio = 0;
	                duermeAleatorio = generarAleatorio(6, 10);
	                sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
	                sprintf(id, "atleta_%d", colaTarimas[i]);
	                sprintf(msg, "Fin del levantamiento, movimiento nulo por falta de fuerza, puntuacion = 0, tiempo = %d", duermeAleatorio);
	                writeLogMessageThreadSafe(id, msg);

					//Vemos si la puntuacion es digna de podio
					comprobarPodio(puntuacionAleatorio, i);

					atletasAtendidos++;

					pthread_cancel(punteroAtletas[i].atletaHilo);
				
	            }

				atletasTarima2++;

				punteroAtletas[i].ha_Competido = 2;

	            if (atletasAtendidos == 4) {  //Miramos si le toca descansar a los jueces
	                sprintf(id, "juez_%d", idTarima);
	                sprintf(msg, "Inicio del descanso");
	                writeLogMessageThreadSafe(id, msg);

	                sleep(10);

	                sprintf(id, "juez_%d", idTarima);
	                sprintf(msg, "Fin del descanso");
	                writeLogMessageThreadSafe(id, msg);

	                atletasAtendidos = 0;
	            }
			}
		}	
	}
}

void comprobarPodio(int puntuacion, int id) {
	int i, auxiliarPuntuacion, auxiliarId;
	for (i = 2; i >= 0; i--) {
		if(puntuacion >= mejoresPuntuaciones[i]) {
			if (i == 2) {
				auxiliarPuntuacion = mejoresPuntuaciones[2];
				mejoresPuntuaciones[2] = puntuacion;
				mejoresPuntuaciones[0] = mejoresPuntuaciones[1];
				mejoresPuntuaciones[1] = auxiliarPuntuacion;
				auxiliarId = mejoresIds[2];
				mejoresIds[2] = id+1;
				mejoresIds[0] = mejoresIds[1];
				mejoresIds[1] = auxiliarId;
				break;
			} else if (i == 1) {
				auxiliarPuntuacion = mejoresPuntuaciones[1];
				mejoresPuntuaciones[1] = puntuacion;
				mejoresPuntuaciones[0] = auxiliarPuntuacion;
				auxiliarId = mejoresIds[1];
				mejoresIds[1] = id+1;
				mejoresIds[0] = auxiliarId;
				break;
			} else {
				mejoresPuntuaciones[0] = puntuacion;
				mejoresIds[0] = id+1;
				break;
			}
		}
	}
}

//Metodo para escribir en el log
void writeLogMessageThreadSafe(char *id, char *msg){
	pthread_mutex_lock(&semaforoLog);
	//Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
	
	//Escribimos en el log
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	//Para seguir el logfile por pantalla
	printf("###logFile:[%s] %s: %s\n", stnow, id, msg); 
	fclose(logFile);	
	pthread_mutex_unlock(&semaforoLog);
}

//Cuando recibimos la signal SIGINT debemos finalizar el programa
void finalizarCompeticion(int sig) {
	char id[10];
	char msg[100];
	finalizaCompeticion = 1;
	sprintf(msg, "Han accedido %d atletas a tarima1 y %d a tarima2", atletasTarima1, atletasTarima2);
	writeLogMessageThreadSafe("informacion", msg);
	if (contadorAtletas == 0) {	
		writeLogMessageThreadSafe("informacion", "Ningun atleta ha participado en la competicion");
	} else if (contadorAtletas == 1) {
		sprintf(id, "atleta_%d", mejoresIds[2]);
		sprintf(msg, "Primer puesto con puntuacion = %d", mejoresPuntuaciones[2]);
		writeLogMessageThreadSafe(id, msg);		
	} else if (contadorAtletas == 2) {
		sprintf(id, "atleta_%d", mejoresIds[2]);
		sprintf(msg, "Primer puesto con puntuacion = %d", mejoresPuntuaciones[2]);
		writeLogMessageThreadSafe(id, msg);
		sprintf(id, "atleta_%d", mejoresIds[1]);
		sprintf(msg, "Segundo puesto con puntuacion = %d", mejoresPuntuaciones[1]);
		writeLogMessageThreadSafe(id, msg);
	} else {
		sprintf(id, "atleta_%d", mejoresIds[2]);
		sprintf(msg, "Primer puesto con puntuacion = %d", mejoresPuntuaciones[2]);
		writeLogMessageThreadSafe(id, msg);
		sprintf(id, "atleta_%d", mejoresIds[1]);
		sprintf(msg, "Segundo puesto con puntuacion = %d", mejoresPuntuaciones[1]);
		writeLogMessageThreadSafe(id, msg);
		sprintf(id, "atleta_%d", mejoresIds[0]);
		sprintf(msg, "Tercer puesto con puntuacion = %d", mejoresPuntuaciones[0]);
		writeLogMessageThreadSafe(id, msg);
	}
	writeLogMessageThreadSafe("Final", "La competicion ha finalizado");
}

//Metodo para generar numero aleatorios
int generarAleatorio(int min, int max){
	//srand(time(NULL));
	int aleatorio = rand()%((max+1)-min)+min;
	return aleatorio;
}
