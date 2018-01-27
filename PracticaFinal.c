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
pthread_mutex_t semaforoCola;  //protege los accesos a la cola
pthread_mutex_t semaforoFuente;  //protege los accesos a la fuente
int contadorAtletas; 
int finalizaCompeticion;  
int idAtletaEsperandoBeber;  //guardamos el primer atleta al que se le manda beber
int atletasAtendidosTarimas[TARIMAS];  //guardamos los atletas que han sido atendidos por cada tarima
int colaTarimas[ATLETAS];  //almacenamos los atletas en la cola unica
int mejoresPuntuaciones[3];
int mejoresIds[3];
int atletasTotalesActuales;
int atletasCompitiendo;
FILE *logFile;
char *logFileName ="registroTiempos.log";

//FUNCIONES
void nuevoCompetidor(int sig);
void inicializaVariablesAtleta(int posicionPuntero, int id, int indispuesto, int tarima);
void *AccionesAtleta(void *arg);
void *AccionesTarima(void *arg);
void comprobarPodio(int puntuacion, int id);
void writeLogMessage(char *id,char *msg);
void finalizarCompeticion(int sig);
void atletasActuales(int sig);
int generarAleatorio(int min, int max);

//STRUCTS
struct atleta {
	pthread_t atletaHilo;
	int id;  //numero de atletas (de 1 a ATLETAS)
	int ha_Competido;
	int tarima_Asignada;
	int necesita_Beber;
};

/*Valores de ha_Competido
	0 -> no ha competido
	1 -> ha subido a la tarima pero NO levantamiento
	2 -> ya ha hecho el levantamiento
*/

struct atleta *punteroAtletas;

int main() {
	char id[10];  //identificador para escribir en el log
	char msg[100];  //mensaje que se escribe en el log
	pthread_t tarima1, tarima2;
	int idTarima1, idTarima2;

	int atletasCompitiendo=0;
	int atletasTotalesActuales=0;

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
	if(signal(SIGPIPE, atletasActuales)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	//Aleatorizacion de la semilla para generar numeros aleatorios
	srand(time(NULL));
	
	//Inicializamos los recursos
	if (pthread_mutex_init(&entradaCompeticion, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoLog, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoCola, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&semaforoFuente, NULL) != 0) exit(-1);

	idTarima1 = 1;
	idTarima2 = 2;
	contadorAtletas = 0;  //posicion de los atletas (de 0 a ATLETAS-1)
	finalizaCompeticion = 0;
	idAtletaEsperandoBeber = 0;

	int i;
	for (i = 0; i < TARIMAS; i++) {
		atletasAtendidosTarimas[i] = 0;
	}
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
	writeLogMessage("Comienzo","Competicion de powerlifting");

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
	
	atletasTotalesActuales++; //incrementamos en 1 el numero de atletas cada vez que se crea uno nuevo

	pthread_mutex_lock(&entradaCompeticion);
	if (contadorAtletas < ATLETAS) {

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
	}
	pthread_mutex_unlock(&entradaCompeticion);
}

void *AccionesAtleta(void *arg) {
	char id[10];
	char msg[100];
	int idAtleta = *(int *)arg;  //numero del atleta (de 1 a ATLETAS)
	int i, num;

	//Escribimos en el log que ha entrado un nuevo atleta y cual es su tarima
	pthread_mutex_lock(&semaforoLog);
	sprintf(id, "atleta_%d", idAtleta);
	sprintf(msg, "Entra en la competicion, su tarima asignada es %d", punteroAtletas[idAtleta-1].tarima_Asignada);
	writeLogMessage(id, msg);
	pthread_mutex_unlock(&semaforoLog);

	//Asignamos a cada atleta su posicion en la cola
	pthread_mutex_lock(&semaforoCola);
	colaTarimas[idAtleta-1] = idAtleta;
	pthread_mutex_unlock(&semaforoCola);

	//Comportamiento del atleta
	while(punteroAtletas[idAtleta-1].ha_Competido == 0) {  //Comprobamos el pulso del atleta cada 3 segundos
		num = generarAleatorio(1, 100);

		if (num <= 15) {
			punteroAtletas[idAtleta-1].ha_Competido = 1;

			pthread_mutex_lock(&semaforoLog);
			sprintf(id, "atleta_%d", idAtleta);
			sprintf(msg, "Se retira por deshidratacion y falta de electrolitos");
			writeLogMessage(id, msg);
			pthread_mutex_unlock(&semaforoLog);
	
			pthread_cancel(punteroAtletas[idAtleta-1].atletaHilo);
		}
		sleep(3);
	}

	while(punteroAtletas[idAtleta-1].ha_Competido == 1) {  //Esperamos a que los atletas compitan
		sleep(1);
	}
	
	if (punteroAtletas[idAtleta-1].necesita_Beber == 1) {  //si el juez le manda a beber agua
		while (idAtletaEsperandoBeber == idAtleta) { //soy el atleta que espera
			//esperamos ayuda...
			sleep(1);
		} 
		//ya llego el ayudante
		pthread_mutex_lock(&semaforoLog);
		sprintf(id, "atleta_%d", idAtleta);
		sprintf(msg, "Ya he bebido agua");
		writeLogMessage(id, msg);
		pthread_mutex_unlock(&semaforoLog);
		
		pthread_cancel(punteroAtletas[idAtleta-1].atletaHilo);
	}
}

void *AccionesTarima(void *arg) {
	char id[10];
	char msg[100];
    int idTarima = *(int *)arg;
    int eventoAleatorio, duermeAleatorio, puntuacionAleatorio, aguaAleatorio;
    int i = -1, j = -1;

	while(1) {
		pthread_mutex_lock(&semaforoCola);
		
		//buscamos en la cola de nuestra tarima
		j = 0;
		while(j < ATLETAS && (colaTarimas[j] == 0 || colaTarimas[j] != 0 && punteroAtletas[colaTarimas[j]-1].ha_Competido != 0 || colaTarimas[j] != 0 && punteroAtletas[colaTarimas[j]-1].tarima_Asignada != idTarima)) {
			j++;
		}
		if (j >= ATLETAS || colaTarimas[j] == 0 || punteroAtletas[colaTarimas[j]-1].ha_Competido!=0 || colaTarimas[j] != 0 && punteroAtletas[colaTarimas[j]-1].tarima_Asignada!=idTarima) { //no hay atletas esperando en nuestra cola, buscamos en otras
			//buscamos en la cola de cualquier tarima
			j = 0;
			while(j < ATLETAS && (colaTarimas[j] == 0 || colaTarimas[j] != 0 && punteroAtletas[colaTarimas[j]-1].ha_Competido!=0)) {
				j++;
			}
			if (j >= ATLETAS || colaTarimas[j] == 0 || punteroAtletas[colaTarimas[j]-1].ha_Competido!=0) { //no hay atletas esperando
				j=-1;
			}
		}

		//en este punto, o bien j es el atleta a tratar, o es -1, y toca esperar a que lleguen nuevos atletas

		
			
		if (j==-1) {
			//esperar a que lleguen nuevos atletas
			pthread_mutex_unlock(&semaforoCola);
			sleep(1);
		} else { //proceder con accionesTarima
			i = colaTarimas[j]-1;

			atletasCompitiendo++;//Atletas en la tarima se incrementa

			punteroAtletas[i].ha_Competido = 1;
			pthread_mutex_unlock(&semaforoCola);

			//Calculamos lo que sucede con el atleta
			eventoAleatorio = generarAleatorio(1, 100);

			//Inicio levantamiento
			pthread_mutex_lock(&semaforoLog);
			sprintf(id, "Tarima_%d", idTarima);
			sprintf(msg, "atleta_%d inicio del levantamiento", colaTarimas[i]);
			writeLogMessage(id, msg);
			pthread_mutex_unlock(&semaforoLog);

			if (eventoAleatorio <= 80) {  //Movimiento valido
				puntuacionAleatorio = generarAleatorio(60, 300);
		
				duermeAleatorio = generarAleatorio(2, 6);
				sleep(duermeAleatorio);

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "Tarima_%d", idTarima);
				sprintf(msg, "atleta_%d fin del levantamiento, movimiento valido, puntuacion = %d, tiempo = %d", colaTarimas[i], puntuacionAleatorio, duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

			} else if (eventoAleatorio > 80 && eventoAleatorio <= 90 ) {  //Nulo por indumentaria
				puntuacionAleatorio = 0;

				duermeAleatorio = generarAleatorio(1, 4);
				sleep(duermeAleatorio);

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "atleta_%d", colaTarimas[i]);
				sprintf(msg, "Fin del levantamiento, movimiento nulo por indumentaria, puntuacion = 0, tiempo = %d", duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

			} else {  //Nulo por falta de fuerza
				puntuacionAleatorio = 0;

				duermeAleatorio = generarAleatorio(6, 10);
				sleep(duermeAleatorio);

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "atleta_%d", colaTarimas[i]);
				sprintf(msg, "Fin del levantamiento, movimiento nulo por falta de fuerza, puntuacion = 0, tiempo = %d", duermeAleatorio);
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);
			}

			//Vemos si la puntuacion es digna de podio
			comprobarPodio(puntuacionAleatorio, i);

			atletasAtendidosTarimas[idTarima-1]++;

			atletasCompitiendo--;//Atletas en la tarima se decrementa

			punteroAtletas[i].ha_Competido = 2;

			//Vemos si tiene que ir a beber agua
			aguaAleatorio = generarAleatorio(1, 10);
			if (aguaAleatorio == 1) {  //al atleta le toca beber agua
				punteroAtletas[i].necesita_Beber = 1;

				pthread_mutex_lock(&semaforoFuente);
				if (idAtletaEsperandoBeber == 0) {  //si es el primer atleta, espera ayudante
					idAtletaEsperandoBeber = i+1;  //guardamos su id

					pthread_mutex_lock(&semaforoLog);
					sprintf(id, "atleta_%d", colaTarimas[i]);
					sprintf(msg, "Voy a beber agua");
					writeLogMessage(id, msg);
					pthread_mutex_unlock(&semaforoLog);
				} else {  //soy el ayudante
					idAtletaEsperandoBeber = 0;  //volver a dejar vacia la fuente
					
					pthread_mutex_lock(&semaforoLog);
					sprintf(id, "atleta_%d", colaTarimas[i]);
					sprintf(msg, "Ayudo a beber agua");
					writeLogMessage(id, msg);
					pthread_mutex_unlock(&semaforoLog);
				}
				pthread_mutex_unlock(&semaforoFuente);
			}

			

			//Miramos si le toca descansar a los jueces
			if (atletasAtendidosTarimas[idTarima-1] % 4 == 0) {

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "juez_%d", idTarima);
				sprintf(msg, "Inicio del descanso");
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);

				sleep(10);

				pthread_mutex_lock(&semaforoLog);
				sprintf(id, "juez_%d", idTarima);
				sprintf(msg, "Fin del descanso");
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&semaforoLog);
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

//Metodo de la seña SIGPIPE
void atletasActuales(int sig){
	char id[10];
	char msg[100];

	if(signal(SIGPIPE ,atletasActuales)==SIG_ERR){
		exit(-1);
	}

	pthread_mutex_lock(&semaforoLog);
	sprintf(id, "%d atletas", atletasTotalesActuales);
	sprintf(msg, "han entrado a la competición hasta el momento.");
	writeLogMessage(id, msg);
	pthread_mutex_unlock(&semaforoLog);
	pthread_mutex_lock(&semaforoLog);
	sprintf(id, "%d atletas", atletasCompitiendo);
	sprintf(msg, "estan siendo atendidos en este momento.");
	writeLogMessage(id, msg);
	pthread_mutex_unlock(&semaforoLog);

	
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
	//Para seguir el logfile por pantalla
	printf("###logFile:[%s] %s: %s\n", stnow, id, msg); 
	fclose(logFile);	
}

//Cuando recibimos la signal SIGINT debemos finalizar el programa
void finalizarCompeticion(int sig) {
	char id[10];
	char msg[100];

	pthread_mutex_lock(&semaforoLog);
	//Vemos si queda algun atleta en la fuente
	if (idAtletaEsperandoBeber != 0) {
		sprintf(id, "atleta_%d", idAtletaEsperandoBeber);
		sprintf(msg, "Se acaba la competicion me voy de la fuente");
		writeLogMessage(id, msg);
		
		pthread_cancel(punteroAtletas[idAtletaEsperandoBeber-1].atletaHilo);
	}

	sprintf(msg, "Han accedido %d atletas a tarima1 y %d a tarima2", atletasAtendidosTarimas[0], atletasAtendidosTarimas[1]);
	writeLogMessage("informacion", msg);
	if (contadorAtletas == 0) {
		writeLogMessage("informacion", "Ningun atleta ha participado en la competicion");
	} else {
		if (contadorAtletas >= 1) {
			sprintf(id, "atleta_%d", mejoresIds[2]);
			sprintf(msg, "Primer puesto con puntuacion = %d", mejoresPuntuaciones[2]);
			writeLogMessage(id, msg);		
		} 
		if (contadorAtletas >= 2) {
			sprintf(id, "atleta_%d", mejoresIds[1]);
			sprintf(msg, "Segundo puesto con puntuacion = %d", mejoresPuntuaciones[1]);
			writeLogMessage(id, msg);
		}
		if (contadorAtletas >= 3) {
			sprintf(id, "atleta_%d", mejoresIds[0]);
			sprintf(msg, "Tercer puesto con puntuacion = %d", mejoresPuntuaciones[0]);
			writeLogMessage(id, msg);
		}
	}
	writeLogMessage("Final", "La competicion ha finalizado");
	pthread_mutex_unlock(&semaforoLog);

	finalizaCompeticion = 1;
}

//Metodo para generar numero aleatorios
int generarAleatorio(int min, int max){
	int aleatorio = rand()%((max+1)-min)+min;
	return aleatorio;
}

