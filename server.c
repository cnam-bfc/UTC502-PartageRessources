#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <time.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define SEM_RESSOURCES_NAME "/sem_ressources"
#define SHM_RESSOURCES_AVAILABLE_NAME "/shm_ressources_available"
#define SHM_CLIENTS_NAME "/shm_clients"

// Objet permettant de stocker les informations d'un client
typedef struct ClientInfo {
    int client_pid;
    int resources_using;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
} ClientInfo;

// Objet permettant de stocker les informations des clients
typedef struct {
    int clients_count;
    ClientInfo clients[MAX_CLIENTS];
    sem_t semaphore;
} ArrayListClientInfo;

// Variables globales
int resources_amount;

// Sémaphore
sem_t *semaphore_ressources;

// Variable partagée 'ressources disponibles'
int *ressources_available;

// Variable partagée 'clients'
ArrayListClientInfo *clients;

// Méthode permettant d'afficher le message d'erreur d'utilisation du programme
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <resource_amount> <port>\nOR\nUsage: %s <config_file>\n", prog_name, prog_name);
    exit(EXIT_FAILURE);
}

// Méthode permettant de créer un sémaphore et de l'initialiser (sem_open) à partir de son adresse
void creer_semaphore(sem_t **semaphore, const char *name, int value) {
    *semaphore = sem_open(name, O_CREAT, 0666, value);
    if (*semaphore == SEM_FAILED) {
        perror("Erreur lors de la création du sémaphore");
        exit(EXIT_FAILURE);
    }
}

// Méthode permettant de fermer un sémaphore
void fermer_semaphore(sem_t *semaphore, const char *name) {
    // Fermer le sémaphore
    if (sem_close(semaphore) == -1) {
        perror("Erreur lors de la fermeture du sémaphore");
        exit(EXIT_FAILURE);
    }

    // Supprimer le sémaphore
    if (sem_unlink(name) == -1) {
        perror("Erreur lors de la suppression du sémaphore");
        exit(EXIT_FAILURE);
    }
}

// Méthode permettant de créer un segment de mémoire partagée et de l'associer à un espace d'adressage du processus
void creer_segment_memoire_partagee(int *shm_fd, void **shm_region, const char *name, int size) {
    // Création du segment de mémoire partagée
    *shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (*shm_fd < 0) {
        perror("Erreur lors de la création du segment de mémoire partagée");
        exit(EXIT_FAILURE);
    }

    // Définition de la taille du segment de mémoire partagée
    if (ftruncate(*shm_fd, size) == -1) {
        perror("Erreur lors du redimensionnement du segment de mémoire partagée");
        exit(EXIT_FAILURE);
    }

    // Association du segment de mémoire partagée à un espace d'adressage du processus
    *shm_region = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (*shm_region == MAP_FAILED) {
        perror("Erreur lors de l'association du segment de mémoire partagée à l'espace d'adressage du processus");
        exit(EXIT_FAILURE);
    }
}

// Méthode permettant de fermer un segment de mémoire partagée
void fermer_segment_memoire_partagee(int *shm_fd, void **shm_region, const char *name, int size) {
    // Détacher le segment de mémoire partagée de l'espace d'adressage du processus
    if (munmap(*shm_region, size) == -1) {
        perror("Erreur lors de la détachement du segment de mémoire partagée de l'espace d'adressage du processus");
        exit(EXIT_FAILURE);
    }

    // Fermer le segment de mémoire partagée
    if (close(*shm_fd) == -1) {
        perror("Erreur lors de la fermeture du segment de mémoire partagée");
        exit(EXIT_FAILURE);
    }

    // Supprimer le segment de mémoire partagée
    if (shm_unlink(name) == -1) {
        perror("Erreur lors de la suppression du segment de mémoire partagée");
        exit(EXIT_FAILURE);
    }
}

// Méthode permettant d'ajouter un élément à l'array list
void ajouter_client(ArrayListClientInfo *list, ClientInfo client) {
    // Verrouiller le sémaphore
    sem_wait(&list->semaphore);

    // Ajouter le client à la liste des clients
    list->clients[list->clients_count++] = client;
    
    // Déverrouiller le sémaphore
    sem_post(&list->semaphore);
}

// Méthode permettant de retirer un élément de l'array list
void retirer_client(ArrayListClientInfo *list, int client_pid) {
    // Verrouiller le sémaphore
    sem_wait(&list->semaphore);

    // Rechercher le client par son pid, le retirer et décaler les éléments
    // Recherche du client
    int i = 0;
    while (i < list->clients_count && list->clients[i].client_pid != client_pid) {
        i++;
    }
    // Décaler les éléments
    for (int j = i; j < list->clients_count - 1; j++) {
        list->clients[j] = list->clients[j + 1];
    }
    // Supprimer dernier élément
    list->clients[list->clients_count - 1] = (ClientInfo){0};
    // Décrémenter le nombre de clients
    list->clients_count--;

    // Déverrouiller le sémaphore
    sem_post(&list->semaphore);
}

// Méthode permettant de récupérer le pointeur d'un client par son pid
ClientInfo *get_client_by_pid(ArrayListClientInfo *list, int client_pid) {
    // Verrouiller le sémaphore
    sem_wait(&list->semaphore);

    // Rechercher le client par son pid
    for (int i = 0; i < list->clients_count; i++) {
        if (list->clients[i].client_pid == client_pid) {
            // Déverrouiller le sémaphore
            sem_post(&list->semaphore);
            return &list->clients[i];
        }
    }

    // Déverrouiller le sémaphore
    sem_post(&list->semaphore);
    return NULL;
}

// Méthode permettant de fermer une socket
void fermer_socket(int socket) {
    printf("Fermeture de la socket...\n");
    if (close(socket) == -1) {
        perror("Erreur lors de la fermeture de la socket");
        exit(EXIT_FAILURE);
    } else {
        printf("Socket fermée !\n");
    }
}

// Méthode permettant de faire une demande de ressources
bool changer_ressources_client(ClientInfo *clientInfo, int change_amount) {
    // Verrouiller le sémaphore des ressources
    sem_wait(semaphore_ressources);

    // CAS : Demande de ressources
    if (change_amount > 0) {
        // Vérifier si les ressources sont suffisantes
        if (*ressources_available >= change_amount) {
            // Mettre à jour les ressources
            *ressources_available -= change_amount;
            clientInfo->resources_using += change_amount;

            // Déverrouiller le sémaphore des ressources
            sem_post(semaphore_ressources);

            return true;
        } else {
            // Déverrouiller le sémaphore des ressources
            sem_post(semaphore_ressources);

            return false;
        }
    } 

    // CAS : Libération de ressources
    else if (change_amount < 0) {
        // Vérifier si les ressources sont suffisantes
        if (clientInfo->resources_using >= -change_amount) {
            // Mettre à jour les ressources
            *ressources_available -= change_amount;
            clientInfo->resources_using += change_amount;

            // Déverrouiller le sémaphore des ressources
            sem_post(semaphore_ressources);

            return true;
        } else {
            // Déverrouiller le sémaphore des ressources
            sem_post(semaphore_ressources);

            return false;
        }
    }
    // Sinon, ne rien faire
    else {
        // Déverrouiller le sémaphore des ressources
        sem_post(semaphore_ressources);
        return true;
    }
}

// Méthode permettant de libérer les ressources utilisées par un client
void liberer_ressources_client(ClientInfo *clientInfo) {
    // Récupérer les ressources utilisées par le client
    int resources_used = clientInfo->resources_using;

    // Libérer les ressources utilisées par le client
    if (resources_used > 0) {
        changer_ressources_client(clientInfo, -resources_used);
    }
}

// Méthode permettant de fermer une socket client
void fermer_socket_client(int socket, ArrayListClientInfo *list, ClientInfo *clientInfo) {
    // Libérer les ressources utilisées par le client
    liberer_ressources_client(clientInfo);
    // Retirer le client de la liste des clients
    retirer_client(list, clientInfo->client_pid);
    // Fermer la socket client
    fermer_socket(socket);
}

// Méthode permettant de créer une socket serveur et d'écouter les connexions entrantes
int socket_serveur(int port) {
    int server_socket;
    struct sockaddr_in server_addr;

    // Créer une socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Lier la socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la liaison");
        fermer_socket(server_socket);
        exit(EXIT_FAILURE);
    }

    // Écouter les connexions
    if (listen(server_socket, SOMAXCONN) < 0) {
        perror("Échec de l'écoute");
        fermer_socket(server_socket);
        exit(EXIT_FAILURE);
    } else {
        printf("Serveur à l'écoute sur le port %d\n", port);
    }

    return server_socket;
}

// Méthode permettant d'envoyer une réponse au client
void envoyer_reponse(int socket, const char *reponse, ArrayListClientInfo *list, ClientInfo *clientInfo) {
    printf("Envoi de la réponse: \"%s\"...\n", reponse);
    if (send(socket, reponse, strlen(reponse), 0) < 0) {
        perror("Échec de l'envoi");
        fermer_socket_client(socket, list, clientInfo);
        exit(EXIT_FAILURE);
    } else {
        printf("Réponse envoyée !\n");
    }
}

// Méthode permettant de recevoir une commande du client et la retourner
char *recevoir_commande(int socket, ArrayListClientInfo *list, ClientInfo *clientInfo) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    printf("Attente de la commande du client...\n");
    if ((bytes_received = recv(socket, buffer, BUFFER_SIZE, 0)) < 0) {
        perror("Échec de la réception");
        fermer_socket_client(socket, list, clientInfo);
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        printf("Le client a fermé la connexion\n");
        fermer_socket_client(socket, list, clientInfo);
        exit(EXIT_FAILURE);
    } else {
        printf("Commande reçue !\n");
    }

    buffer[bytes_received] = '\0';
    printf("Commande du client: \"%s\"\n", buffer);

    return strdup(buffer);
}

// Méthode permettant de gérer un client
void handle_client(int client_sock, const char *client_ip, int client_port) {
    // Récupérer le pid du client
    int client_pid = getpid();

    // Créer un objet ClientInfo et l'ajouter à la liste des clients
    ClientInfo clientInfoInst;
    clientInfoInst.client_pid = client_pid;
    clientInfoInst.resources_using = 0;
    strcpy(clientInfoInst.client_ip, client_ip);
    clientInfoInst.client_port = client_port;

    // Ajouter le client à la liste des clients
    ajouter_client(clients, clientInfoInst);

    // Récupérer le pointeur du client
    ClientInfo *clientInfo = get_client_by_pid(clients, client_pid);

    for (;;) {
        char *commande = recevoir_commande(client_sock, clients, clientInfo);

        int requested_amount;
        if (sscanf(commande, "REQUEST %d", &requested_amount) == 1) {
            // Demander les ressources
            if (changer_ressources_client(clientInfo, requested_amount)) {
                // Répondre au client OK
                char buffer[BUFFER_SIZE];
                snprintf(buffer, BUFFER_SIZE, "GRANTED %d", requested_amount);
                envoyer_reponse(client_sock, buffer, clients, clientInfo);
            } else {
                // Répondre au client KO
                char buffer[BUFFER_SIZE];
                snprintf(buffer, BUFFER_SIZE, "DENIED %d, REASON: Ressources insuffisantes", requested_amount);
                envoyer_reponse(client_sock, buffer, clients, clientInfo);
            }
        } else if (sscanf(commande, "RELEASE %d", &requested_amount) == 1) {
            // Demander la libération des ressources
            if (changer_ressources_client(clientInfo, -requested_amount)) {
                // Répondre au client OK
                char buffer[BUFFER_SIZE];
                snprintf(buffer, BUFFER_SIZE, "RELEASED %d", requested_amount);
                envoyer_reponse(client_sock, buffer, clients, clientInfo);
            } else {
                // Répondre au client KO
                char buffer[BUFFER_SIZE];
                snprintf(buffer, BUFFER_SIZE, "DENIED %d, REASON: Ressources insuffisantes", requested_amount);
                envoyer_reponse(client_sock, buffer, clients, clientInfo);
            }
        }
    }

    // Fermer la socket client
    fermer_socket_client(client_sock, clients, clientInfo);
    // Terminer le processus fils
    exit(EXIT_SUCCESS);
}

// Méthode permettant d'attendre la connexion d'un client et de la créer/gérer
int accept_client(int server_socket) {
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Attendre une connexion client
    printf("En attente d'une connexion client...\n");

    // Accepter une connexion client
    if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
        perror("Échec de l'acceptation");
        return -1;
    }

    // Afficher les informations du client
    printf("Client connecté: %s:%d sock_id=%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_socket);

    // Fork pour gérer le client
    pid_t pid = fork();
    if (pid < 0) {
        perror("Échec du fork");
        fermer_socket(client_socket);
    } else if (pid == 0) {
        // Fermer la socket serveur car le fils ne gère pas le serveur
        close(server_socket);
        // Le fils gère le client
        handle_client(client_socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    } else {
        // Fermer la socket client car le père ne gère pas le client
        close(client_socket);
    }

    return client_socket;
}

// Méthode permettant de gérer l'affichage du status du serveur
void handle_status() {
    // Le fils gère l'affichage du status
    for (;;) {
        // Afficher le status du serveur toutes les 5 secondes
        // --- STATUS DU SERVEUR (dd/mm/yyyy hh:mm:ss) ---
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        printf(" --- STATUS DU SERVEUR (%02d/%02d/%04d %02d:%02d:%02d) ---\n", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
        printf("Ressources disponibles: %d\n", *ressources_available);
        printf("Clients connectés: %d\n", clients->clients_count);
        // Afficher les informations des clients
        for (int i = 0; i < clients->clients_count; i++) {
            ClientInfo *clientInfo = &clients->clients[i];
            printf("Client %d: %s:%d, ressources utilisées: %d\n", clientInfo->client_pid, clientInfo->client_ip, clientInfo->client_port, clientInfo->resources_using);
        }

        // Attendre 5 secondes
        sleep(5);
    }
}

// Gestionnaire de signal pour SIGCHLD
void sigchld_handler(int signum) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// Méthode permettant de lire un fichier de configuration
void lireFichierConfig(const char *fichier, int *server_port, int *resource_amount) {
    FILE *fichier_config = fopen(fichier, "r");
    if (fichier_config == NULL) {
        perror("Erreur lors de l'ouverture du fichier de configuration");
        exit(EXIT_FAILURE);
    }

    char ligne[BUFFER_SIZE];
    while (fgets(ligne, BUFFER_SIZE, fichier_config) != NULL) {
        char clef[BUFFER_SIZE];
        char valeur[BUFFER_SIZE];
        if (sscanf(ligne, "%[^=]=%[^\n]", clef, valeur) == 2) {
            if (strcmp(clef, "server_port") == 0) {
                *server_port = atoi(valeur);
            } else if (strcmp(clef, "resource_amount") == 0) {
                *resource_amount = atoi(valeur);
            }
        }
    }

    fclose(fichier_config);
}

// Méthode principale
int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 2) {
        usage(argv[0]);
    }
    
    int port;
    
    if (argc == 3) {
        resources_amount = atoi(argv[1]);
        port = atoi(argv[2]);
    } else {
        lireFichierConfig(argv[1], &port, &resources_amount);
    }

    // Descripteur de fichier de la mémoire partagée 'ressources disponibles'
    int shm_fd_ressources_available;
    // Pointeur pour l'association du segment de mémoire partagée 'ressources disponibles' à un espace d'adressage du processus
    void *shm_region_ressources_available;
    // Créer un segment de mémoire partagée pour les ressources disponibles
    creer_segment_memoire_partagee(&shm_fd_ressources_available, &shm_region_ressources_available, SHM_RESSOURCES_AVAILABLE_NAME, sizeof(int));
    // Lier la variable partagée 'ressources disponibles'
    ressources_available = (int *)shm_region_ressources_available;

    // Initialisation des ressources
    *ressources_available = resources_amount;

    // Descripteur de fichier de la mémoire partagée 'clients'
    int shm_fd_clients;
    // Pointeur pour l'association du segment de mémoire partagée 'clients' à un espace d'adressage du processus
    void *shm_region_clients;
    // Créer un segment de mémoire partagée pour les clients
    creer_segment_memoire_partagee(&shm_fd_clients, &shm_region_clients, SHM_CLIENTS_NAME, sizeof(ArrayListClientInfo));
    // Lier la variable partagée 'clients'
    clients = (ArrayListClientInfo *)shm_region_clients;

    // Initialisation des clients
    clients->clients_count = 0;

    // Mise en place du sémaphore des clients
    sem_init(&clients->semaphore, 1, 1); // 1 pour processus multiples

    // Mise en place du sémaphore des ressources
    creer_semaphore(&semaphore_ressources, SEM_RESSOURCES_NAME, 1);

    int server_sock;

    // Créer une socket serveur
    server_sock = socket_serveur(port);

    // Gérer la terminaison des enfants
    signal(SIGCHLD, sigchld_handler);

    // Gérer l'actualisation du status du serveur ici avec un fork
    pid_t pid_status = fork();
    if (pid_status < 0) {
        perror("Échec du fork");
        fermer_socket(server_sock);
        exit(EXIT_FAILURE);
    } else if (pid_status == 0) {
        // Le fils ne gère pas le serveur
        close(server_sock);
        // Le fils gère l'affichage du status
        handle_status();
        exit(EXIT_SUCCESS);
    }

    for (;;) {
        // Attendre une connexion client
        accept_client(server_sock);
    }

    // Nettoyer
    fermer_socket(server_sock);
    fermer_semaphore(semaphore_ressources, SEM_RESSOURCES_NAME);
    sem_destroy(&clients->semaphore);
    fermer_segment_memoire_partagee(&shm_fd_ressources_available, &shm_region_ressources_available, SHM_RESSOURCES_AVAILABLE_NAME, sizeof(int));
    fermer_segment_memoire_partagee(&shm_fd_clients, &shm_region_clients, SHM_CLIENTS_NAME, sizeof(ArrayListClientInfo));
    exit(EXIT_SUCCESS);
}
