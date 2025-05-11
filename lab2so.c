#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/mman.h>

// Funciones de utilidad
void limpiar_pantalla() {
    printf("\033[2J\033[H");  // Código ANSI para limpiar pantalla



}

void presionar_para_continuar() {
    printf("\nPresiona Enter para continuar...");
    getchar();
}




// Estructura para representar una carta
typedef struct {
    char valor[3]; // 2-10, J, Q, K, A
    char palo;     // C (corazones), D (diamantes), T (tréboles), P (picas)
    int puntos;    // Valor numérico
} Carta;

// Estructura para representar un mazo de cartas
typedef struct {
    Carta cartas[52];
    int indice;
} Mazo;

// Estructura para representar una mano de cartas
typedef struct {
    Carta cartas[21]; // Máximo de 21 cartas (teóricamente)
    int num_cartas;
} Mano;

// Estructura para el estado del juego compartido
typedef struct {
    int puntuaciones[5]; // Puntuaciones de los 4 jugadores + croupier
    Carta carta_boca_arriba_croupier;
    Carta carta_boca_abajo_croupier;
    int total_mano_croupier;
    int ronda_actual;
    int total_rondas;
    bool ronda_terminada;
    bool juego_terminado;
    bool turno_jugador_completado[4];
    int resultados_ronda[4]; // 0=perdió, 1=ganó
} EstadoJuego;

// Nombres de palos y valores
const char* VALORES[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"};
const char PALOS[] = {'C', 'D', 'T', 'P'};
const char* NOMBRES_PALOS[] = {"Corazones", "Diamantes", "Tréboles", "Picas"};
// Definir nombres de jugadores considerando que el orden comienza a la izquierda del croupier
const char* NOMBRES_JUGADORES[] = {"Jugador 2", "Jugador 1", "Jugador 4", "Jugador 3", "Croupier"};

// Funciones para recuadros y separadores decorativos
void imprimir_separador() {
    printf("\n╔════════════════════════════════════════════════════════════════════╗\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
}

void imprimir_titulo_seccion(const char* titulo) {
    int longitud = strlen(titulo);
    int espacios = (68 - longitud) / 2;
    
    printf("\n╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║");
    for (int i = 0; i < espacios; i++) {
        printf(" ");
    }
    printf("%s", titulo);
    for (int i = 0; i < espacios - (longitud % 2 == 0 ? 0 : 1); i++) {
        printf(" ");
    }
    printf("║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
}

void imprimir_caja_inicio(const char* titulo) {
    int longitud = strlen(titulo);
    int espacios = (68 - longitud) / 2;
    
    printf("\n╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║");
    for (int i = 0; i < espacios; i++) {
        printf(" ");
    }
    printf("%s", titulo);
    for (int i = 0; i < espacios - (longitud % 2 == 0 ? 0 : 1); i++) {
        printf(" ");
    }
    printf("║\n");
    printf("╠════════════════════════════════════════════════════════════════════╣\n");
}

void imprimir_caja_fin() {
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
}

void imprimir_contenido_caja(const char* texto) {
    printf("║ %-68s ║\n", texto);
}

// Inicializar el mazo
void inicializar_mazo(Mazo* mazo) {
    int idx = 0;
    for (int i = 0; i < 13; i++) {
        for (int j = 0; j < 4; j++) {
            strcpy(mazo->cartas[idx].valor, VALORES[i]);
            mazo->cartas[idx].palo = PALOS[j];
            
            // Asignar puntos
            if (i >= 9 && i <= 11) { // J, Q, K
                mazo->cartas[idx].puntos = 10;
            } else if (i == 12) { // As
                mazo->cartas[idx].puntos = 11; // Por defecto 11, puede cambiar a 1
            } else { // Cartas numéricas
                mazo->cartas[idx].puntos = i + 2;
            }
            
            idx++;
        }
    }
    mazo->indice = 0;
}

// Barajar el mazo
void barajar_mazo(Mazo* mazo) {
    srand(time(NULL));
    for (int i = 51; i > 0; i--) {
        int j = rand() % (i + 1);
        Carta temp = mazo->cartas[i];
        mazo->cartas[i] = mazo->cartas[j];
        mazo->cartas[j] = temp;
    }
}

// Repartir una carta del mazo
Carta repartir_carta(Mazo* mazo) {
    if (mazo->indice >= 52) {
        printf("Error: No hay más cartas en el mazo\n");
        exit(1);
    }
    return mazo->cartas[mazo->indice++];
}

// Mostrar una carta
void mostrar_carta(Carta carta) {
    const char* nombre_palo;
    for (int i = 0; i < 4; i++) {
        if (PALOS[i] == carta.palo) {
            nombre_palo = NOMBRES_PALOS[i];
            break;
        }
    }
    printf("%s de %s", carta.valor, nombre_palo);
}

// Calcular el valor de una mano
int calcular_valor_mano(Mano* mano) {
    int valor = 0;
    int num_ases = 0;
    
    for (int i = 0; i < mano->num_cartas; i++) {
        valor += mano->cartas[i].puntos;
        if (strcmp(mano->cartas[i].valor, "A") == 0) {
            num_ases++;
        }
    }
    
    // Ajustar el valor de los ases si la mano se pasa de 21
    while (valor > 21 && num_ases > 0) {
        valor -= 10;  // Cambia el valor del as de 11 a 1
        num_ases--;
    }
    
    return valor;
}

// Verificar si una mano es Blackjack (21 con 2 cartas)
bool es_blackjack(Mano* mano) {
    return mano->num_cartas == 2 && calcular_valor_mano(mano) == 21;
}

// Función principal del juego
int main() {
    srand(time(NULL));
    
    // Configuración inicial
    int total_rondas = 0;
    limpiar_pantalla();
    imprimir_caja_inicio("BLACKJACK");
    imprimir_contenido_caja("Bienvenido al juego de Blackjack con bots");
    imprimir_contenido_caja("Jugarás como el Croupier contra 4 jugadores bots");
    imprimir_caja_fin();
    
    printf("\nIngrese el número de rondas a jugar (predeterminado: 5): ");
    char input[10];
    fgets(input, sizeof(input), stdin);
    if (input[0] != '\n') {
        total_rondas = atoi(input);
    }
    if (total_rondas <= 0) {
        total_rondas = 5;
    }
    
    // Crear memoria compartida para el estado del juego
    EstadoJuego* estado = mmap(NULL, sizeof(EstadoJuego), 
                            PROT_READ | PROT_WRITE, 
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if (estado == MAP_FAILED) {
        perror("Error al crear memoria compartida");
        exit(1);
    }
    
    // Inicializar estado del juego
    memset(estado, 0, sizeof(EstadoJuego));
    estado->total_rondas = total_rondas;
    estado->ronda_actual = 1;
    
    // Crear los procesos para los jugadores
    pid_t pids[4];
    for (int i = 0; i < 4; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("Error al crear proceso hijo");
            exit(1);
        } else if (pids[i] == 0) {
            // Código del proceso hijo (jugador)
            int id_jugador = i;
            
            while (!estado->juego_terminado) {
                // Esperar a que comience la ronda
                while (estado->ronda_actual <= estado->total_rondas && !estado->ronda_terminada && !estado->turno_jugador_completado[id_jugador]) {
                    usleep(100000); // Esperar 100ms
                    
                    // Si es nuestro turno, jugar
                    if (!estado->turno_jugador_completado[id_jugador]) {
                        Mazo mazo_local;
                        Mano mano_local;
                        mano_local.num_cartas = 2; // Ya tenemos 2 cartas iniciales
                        
                        // Leer desde stdin la información del juego
                        char buffer[1024];
                        bool pedir_carta = false;
                        bool plantarse = false;
                        int valor_mano = 0;
                        
                        // Leer la mano actual
                        if (fgets(buffer, sizeof(buffer), stdin)) {
                            // Parsear la mano
                            sscanf(buffer, "%d", &valor_mano);
                            
                            // Decidir si pedir carta o plantarse según las reglas
                            if (valor_mano <= 11) {
                                pedir_carta = true;
                            } else if (valor_mano >= 12 && valor_mano <= 18) {
                                pedir_carta = (rand() % 100 < 50); // 50% probabilidad
                            } else {
                                plantarse = true;
                            }
                            
                            // Enviar la decisión al padre
                            if (pedir_carta) {
                                printf("PEDIR_________00000__000\n");
                                fflush(stdout);
                            } else {
                                printf("PLANTARSE\n");
                                fflush(stdout);
                                estado->turno_jugador_completado[id_jugador] = true;
                            }
                        }
                        
                        usleep(500000); // Esperar 500ms
                    }
                }
                
                // Esperar a que termine la ronda
                while (!estado->ronda_terminada) {
                    usleep(100000); // Esperar 100ms
                }
                
                // Resetear para la siguiente ronda
                estado->turno_jugador_completado[id_jugador] = false;
                
                // Esperar a que comience la siguiente ronda
                while (estado->ronda_terminada) {
                    usleep(100000); // Esperar 100ms
                }
            }
            
            // Enviar puntuación final
            printf("%s: %d puntos\n", NOMBRES_JUGADORES[id_jugador], estado->puntuaciones[id_jugador]);
            exit(0);
        }
    }
    
    // Código del proceso padre (croupier)
    int puntuacion_croupier = 0;
    
    for (int ronda = 1; ronda <= total_rondas; ronda++) {
        limpiar_pantalla();
        imprimir_caja_inicio("RONDA");
        char texto_ronda[50];
        sprintf(texto_ronda, "Ronda %d de %d", ronda, total_rondas);
        imprimir_contenido_caja(texto_ronda);
        imprimir_caja_fin();
        
        // Inicializar estado de la ronda
        memset(estado->turno_jugador_completado, 0, sizeof(estado->turno_jugador_completado));
        memset(estado->resultados_ronda, 0, sizeof(estado->resultados_ronda));
        estado->ronda_actual = ronda;
        estado->ronda_terminada = false;
        
        // Crear y barajar el mazo
        Mazo mazo;
        inicializar_mazo(&mazo);
        barajar_mazo(&mazo);
        
        // Crear las manos
        Mano manos[5]; // 4 jugadores + croupier
        for (int i = 0; i < 5; i++) {
            manos[i].num_cartas = 0;
        }
        
        // Repartir las cartas iniciales (2 a cada jugador y al croupier)
        printf("Repartiendo cartas iniciales...\n\n");
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 5; i++) {
                Carta carta = repartir_carta(&mazo);
                manos[i].cartas[manos[i].num_cartas++] = carta;
                
                // Guardar las cartas del croupier en el estado
                if (i == 4) { // Croupier
                    if (j == 0) {
                        estado->carta_boca_abajo_croupier = carta;
                    } else {
                        estado->carta_boca_arriba_croupier = carta;
                    }
                }
                
                // Mostrar las cartas repartidas (excepto la carta boca abajo del croupier)
                if (!(i == 4 && j == 0)) { // Si no es la carta boca abajo del croupier
                    printf("%s recibe: ", NOMBRES_JUGADORES[i]);
                    mostrar_carta(carta);
                    printf("\n");
                }
            }
        }
        
        // Mostrar estado inicial
        printf("\nEstado inicial (una carta boca arriba y una boca abajo):\n");
        for (int i = 0; i < 4; i++) {
            printf("%s: ", NOMBRES_JUGADORES[i]);
            printf("Carta boca abajo | ");
            mostrar_carta(manos[i].cartas[1]);
            printf(" (Total visible: %d)\n", manos[i].cartas[1].puntos);
        }
        printf("%s: Carta boca abajo | ", NOMBRES_JUGADORES[4]);
        mostrar_carta(estado->carta_boca_arriba_croupier);
        printf("\n");
        
        // Turno de cada jugador (siguiendo el orden correcto: comienza a la izquierda del croupier)
        printf("\nOrden de juego: J3-J2-J1-CROUPIER-J4\n");
        for (int i = 0; i < 4; i++) {
            printf("\n=== Turno de %s ===\n", NOMBRES_JUGADORES[i]);
            
            while (true) {
                int valor_mano = calcular_valor_mano(&manos[i]);
                printf("Mano actual: ");
                for (int j = 0; j < manos[i].num_cartas; j++) {
                    mostrar_carta(manos[i].cartas[j]);
                    if (j < manos[i].num_cartas - 1) {
                        printf(", ");
                    }
                }
                printf(" (Total: %d)\n", valor_mano);
                
                // Decidir acción según las reglas
                bool pedir_carta = false;
                
                if (valor_mano <= 11) {
                    pedir_carta = true;
                    printf("Decisión automática: PEDIR\n");
                } else if (valor_mano >= 12 && valor_mano <= 18) {
                    pedir_carta = (rand() % 100 < 50); // 50% probabilidad
                    printf("Decisión automática: %s\n", pedir_carta ? "PEDIR" : "PLANTARSE");
                } else {
                    pedir_carta = false;
                    printf("Decisión automática: PLANTARSE\n");
                }
                
                if (pedir_carta) {
                    Carta carta = repartir_carta(&mazo);
                    manos[i].cartas[manos[i].num_cartas++] = carta;
                    printf("%s recibe: ", NOMBRES_JUGADORES[i]);
                    mostrar_carta(carta);
                    printf("\n");
                    
                    // Verificar si se pasó de 21
                    if (calcular_valor_mano(&manos[i]) > 21) {
                        printf("%s se ha pasado de 21.\n", NOMBRES_JUGADORES[i]);
                        break;
                    }
                } else {
                    printf("%s se planta.\n", NOMBRES_JUGADORES[i]);
                    break;
                }
            }
            
            estado->turno_jugador_completado[i] = true;
            presionar_para_continuar();
        }
        
        // Turno del croupier (controlado por el usuario)
        printf("\n=== Turno del Croupier ===\n");
        printf("Revelando carta boca abajo: ");
        mostrar_carta(estado->carta_boca_abajo_croupier);
        printf("\n");
        
        printf("Mano actual: ");
        mostrar_carta(estado->carta_boca_abajo_croupier);
        printf(", ");
        mostrar_carta(estado->carta_boca_arriba_croupier);
        
        estado->total_mano_croupier = calcular_valor_mano(&manos[4]);
        printf(" (Total: %d)\n", estado->total_mano_croupier);
        
        while (true) {
            printf("\n¿Qué deseas hacer?\n");
            printf("1. Pedir carta\n");
            printf("2. Plantarse\n");
            printf("Opción: ");
            
            char opcion[10];
            fgets(opcion, sizeof(opcion), stdin);
            
            if (opcion[0] == '1') {
                Carta carta = repartir_carta(&mazo);
                manos[4].cartas[manos[4].num_cartas++] = carta;
                printf("Croupier recibe: ");
                mostrar_carta(carta);
                printf("\n");
                
                estado->total_mano_croupier = calcular_valor_mano(&manos[4]);
                printf("Mano actual (Total: %d)\n", estado->total_mano_croupier);
                
                // Verificar si se pasó de 21
                if (estado->total_mano_croupier > 21) {
                    printf("Croupier se ha pasado de 21.\n");
                    break;
                }
            } else if (opcion[0] == '2') {
                printf("Croupier se planta.\n");
                break;
            } else {
                printf("Opción inválida. Intenta de nuevo.\n");
            }
        }
        
        // Determinar ganadores
        printf("\n=== Resultados de la Ronda %d ===\n", ronda);
        int jugadores_ganados = 0;
        
        for (int i = 0; i < 4; i++) {
            int valor_jugador = calcular_valor_mano(&manos[i]);
            int valor_croupier = estado->total_mano_croupier;
            bool blackjack_jugador = es_blackjack(&manos[i]);
            bool blackjack_croupier = es_blackjack(&manos[4]);
            
            printf("%s: ", NOMBRES_JUGADORES[i]);
            
            // Determinar si el jugador gana
            if (valor_jugador > 21) {
                printf("Pierde (Se pasó de 21)\n");
                estado->resultados_ronda[i] = 0;
            } else if (blackjack_jugador && !blackjack_croupier) {
                printf("¡GANA con Blackjack!\n");
                estado->resultados_ronda[i] = 1;
                estado->puntuaciones[i]++;
                jugadores_ganados++;
            } else if (valor_croupier > 21) {
                printf("¡GANA! (Croupier se pasó)\n");
                estado->resultados_ronda[i] = 1;
                estado->puntuaciones[i]++;
                jugadores_ganados++;
            } else if (valor_jugador > valor_croupier) {
                printf("¡GANA!\n");
                estado->resultados_ronda[i] = 1;
                estado->puntuaciones[i]++;
                jugadores_ganados++;
            } else if (valor_jugador == valor_croupier) {
                printf("Empate (Push) - Se considera victoria pero no suma puntos\n");
                estado->resultados_ronda[i] = 1;
                jugadores_ganados++;
            } else {
                printf("Pierde\n");
                estado->resultados_ronda[i] = 0;
            }
        }
        
        // Asignar puntos al croupier
        if (jugadores_ganados == 0) {
            printf("\nCroupier gana 2 puntos (venció a todos los jugadores)\n");
            estado->puntuaciones[4] += 2;
        } else if (jugadores_ganados <= 2) {
            printf("\nCroupier gana 1 punto (venció a la mayoría)\n");
            estado->puntuaciones[4] += 1;
        } else {
            printf("\nCroupier no gana puntos esta ronda\n");
        }
        
        // Mostrar puntuaciones actuales
        printf("\nPuntuaciones actuales:\n");
        for (int i = 0; i < 5; i++) {
            printf("%s: %d puntos\n", NOMBRES_JUGADORES[i], estado->puntuaciones[i]);
        }
        
        estado->ronda_terminada = true;
        
        if (ronda < total_rondas) {
            presionar_para_continuar();
            estado->ronda_terminada = false;
        }
    }
    
    // Determinar ganador final
    limpiar_pantalla();
    printf("=== FIN DEL JUEGO - RESULTADOS FINALES ===\n\n");
    
    int max_puntos = -1;
    int ganadores[5];
    int num_ganadores = 0;
    
    // Imprimir puntuaciones finales y encontrar el máximo
    printf("Puntuaciones finales:\n");
    for (int i = 0; i < 5; i++) {
        printf("%s: %d puntos\n", NOMBRES_JUGADORES[i], estado->puntuaciones[i]);
        
        if (estado->puntuaciones[i] > max_puntos) {
            max_puntos = estado->puntuaciones[i];
            num_ganadores = 0;
            ganadores[num_ganadores++] = i;
        } else if (estado->puntuaciones[i] == max_puntos) {
            ganadores[num_ganadores++] = i;
        }
    }
    
    // Anunciar ganadores
    printf("\n¡El(los) ganador(es) es(son):\n");
    for (int i = 0; i < num_ganadores; i++) {
        printf("- %s con %d puntos\n", NOMBRES_JUGADORES[ganadores[i]], max_puntos);
    }
    
    // Marcar el juego como terminado para que los procesos hijos salgan
    estado->juego_terminado = true;
    
    // Esperar a que terminen los procesos hijos
    for (int i = 0; i < 4; i++) {
        waitpid(pids[i], NULL, 0);
    }
    
    // Liberar memoria compartida
    munmap(estado, sizeof(EstadoJuego));
    
    return 0;
}
