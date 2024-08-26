#include "sync.h"
#define SYNC


// Prototypes des fonctions

/**
 * \brief Crée un objet ServerFile pour un fichier donné.
 * 
 * \param filename Le nom du fichier pour lequel créer l'objet ServerFile.
 * \return Un pointeur vers le ServerFile créé, ou NULL en cas d'erreur.
 */
ServerFile* create_ServerFile(const char* filename) {
    ServerFile* serverFile = malloc(sizeof(ServerFile));
    if (serverFile == NULL) {
        // Gérer l'échec de l'allocation mémoire
        return NULL;
    }

    strcpy(serverFile->filename, filename);
    serverFile->num_lecteur = 0; // Initialise le nombre de lecteurs à 0
    pthread_mutex_init(&serverFile->mutex, NULL); // Initialise le mutex
    return serverFile;
}



/**
 * \brief Ajoute un ServerFile au tableau dynamique de ServerFileArray.
 * 
 * \param serverFile Le ServerFile à ajouter.
 * \param serverFileArray Le tableau dynamique de ServerFile.
 * \return 0 en cas de succès, -1 en cas d'erreur.
 */
int add_ServerFile(ServerFile* serverFile, ServerFileArray* serverFileArray) {
    // Augmenter la taille du tableau dynamique
    if (serverFileArray->size <= 0){
        serverFileArray->array = (ServerFile**)malloc(sizeof(ServerFile*));
    } else {
         serverFileArray->array = realloc(serverFileArray->array, (serverFileArray->size + 1) * sizeof(ServerFile*));
    }
   
    if (serverFileArray->array == NULL) {
        // Gérer l'échec de l'allocation mémoire
        perror("Erreur lors de l'allocation mémoire");
        return -1;
    }
    // Ajouter le nouveau fichier à la fin du tableau
    serverFileArray->array[serverFileArray->size] = serverFile;
    // Augmenter le compteur de taille du tableau
    serverFileArray->size++;
    return 0;
}



/**
 * \brief Recherche un fichier dans le tableau dynamique de ServerFileArray.
 * 
 * \param filename Le nom du fichier à rechercher.
 * \param serverFileArray Le tableau dynamique de ServerFile.
 * \return Un pointeur vers le ServerFile trouvé, ou NULL s'il n'est pas trouvé.
 */
ServerFile* search_ServerFile(const char* filename, ServerFileArray* serverFileArray) {
    // Parcourir tous les fichiers dans le tableau
    for (size_t i = 0; i < serverFileArray->size; ++i) {
        // Comparer le nom du fichier avec le nom du fichier recherché
        if (strcmp(serverFileArray->array[i]->filename, filename) == 0) {
            // Retourner le pointeur vers le fichier trouvé
            return serverFileArray->array[i];
        }
    }
    // Si le fichier n'est pas trouvé, retourner NULL
    return NULL;
}







/**
 * \brief Supprime un fichier du tableau dynamique de ServerFileArray.
 * 
 * \param filename Le nom du fichier à supprimer.
 * \param serverFileArray Le tableau dynamique de ServerFile.
 * \return 0 en cas de succès, -1 en cas d'erreur.
 */
int remove_ServerFile(const char* filename, ServerFileArray* serverFileArray) {
    // Parcourir tous les fichiers dans le tableau
    for (size_t i = 0; i < serverFileArray->size; ++i) {
        // Comparer le nom du fichier avec le nom du fichier recherché
        if (strcmp(serverFileArray->array[i]->filename, filename) == 0) {
            // Libérer la mémoire occupée par le mutex
            pthread_mutex_destroy(&serverFileArray->array[i]->mutex);
            // Libérer la mémoire occupée par le fichier
            free(serverFileArray->array[i]);
            
            // Déplacer tous les éléments suivants d'un indice vers la gauche
            for (size_t j = i; j < serverFileArray->size - 1; ++j) {
                serverFileArray->array[j] = serverFileArray->array[j + 1];
            }
            // Diminuer la taille du tableau dynamique
            serverFileArray->size--;
            // Terminer la fonction après avoir trouvé et supprimé le fichier
            return 0;
        }
    }
    // Si le fichier n'est pas trouvé, afficher un message d'erreur
    printf("Erreur : Le fichier '%s' n'a pas été trouvé dans la liste des fichiers.\n", filename);
    return -1;
}


/**
 * \brief Initialise un tableau dynamique de ServerFileArray.
 * 
 * \param serverFileArray Le tableau dynamique de ServerFile à initialiser.
 */
void initialize_serverFileArray(ServerFileArray* serverFileArray) {
    serverFileArray->array = NULL;
    serverFileArray->size = 0;
}



/**
 * \brief Démarre une session de fichier avec le client.
 * 
 * Cette fonction démarre une session de fichier avec le client spécifié en utilisant le nom de fichier
 * fourni et le mode spécifié (lecture ou écriture). Si le fichier n'existe pas sur le serveur, il est créé.
 * Pour le mode de lecture, le nombre de lecteurs du fichier est incrémenté. Pour le mode d'écriture, 
 * un verrou mutex est tenté d'être obtenu pour assurer l'exclusivité d'accès au fichier. En cas d'erreur,
 * la fonction retourne -1.
 * 
 * \param filename Le nom du fichier pour la session.
 * \param mode Le mode de la session de fichier (READ_MODE ou WRITE_MODE).
 * \param serverFileArray Le tableau dynamique des fichiers serveur.
 * \return 0 en cas de succès, -1 en cas d'erreur.
 */
int start_file_session(const char* filename, int mode, ServerFileArray* serverFileArray) {
    // Chercher le fichier dans la liste des fichiers serveur
    ServerFile* serverFile = search_ServerFile(filename, serverFileArray);

    if (serverFile == NULL) {
        // Si le fichier n'est pas trouvé, le créer
        serverFile = create_ServerFile(filename);
        if (serverFile == NULL) {
            // Gérer l'échec de la création du fichier
            return -1;
        }
        // Ajouter le nouveau fichier à la liste des fichiers serveur
        add_ServerFile(serverFile, serverFileArray);
    }

    // Verifier le mode
    if (mode == READ_MODE) {
        // Si le mode est lecture, incrémenter le nombre de lecteurs
        
        if (serverFile->num_lecteur == 0){
            if (pthread_mutex_trylock(&(serverFile->mutex)) != 0) {
                // Si le verrouillage du mutex échoue, retourner -1 (erreur)
                return -1;
            }
        }

        serverFile->num_lecteur++;
        return 0; // Succès

    } else if (mode == WRITE_MODE) {
        // Si le mode est écriture, tenter de verrouiller le mutex
        if (serverFile->num_lecteur > 0 || pthread_mutex_trylock(&(serverFile->mutex)) != 0) {
            // Si le verrouillage du mutex échoue, retourner -1 (erreur)
            // printf("verro echec");
            return -1;
        } else {
            return 0; // Succès
        }
    } else {
        // Mode invalide, retourner -1 (erreur)
        return -1;
    }
}




/**
 * \brief Arrête une session de fichier avec le client.
 * 
 * Cette fonction arrête une session de fichier avec le client spécifié pour le fichier donné.
 * Elle décrémente le nombre de lecteurs du fichier si le mode est lecture, déverrouille le mutex si le mode
 * est écriture et supprime le fichier de la liste des fichiers serveur. En cas d'erreur, la fonction retourne -1.
 * 
 * \param filename Le nom du fichier pour la session.
 * \param mode Le mode de la session de fichier (READ_MODE ou WRITE_MODE).
 * \param serverFileArray Le tableau dynamique des fichiers serveur.
 * \return 0 en cas de succès, -1 en cas d'erreur.
 */
int stop_file_session(const char* filename, int mode, ServerFileArray* serverFileArray) {
    // Chercher le fichier dans la liste des fichiers serveur
    ServerFile* serverFile = search_ServerFile(filename, serverFileArray);

    if (serverFile == NULL) {
        // Si le fichier n'est pas trouvé, retourner une erreur
        return -1;
    }

    // Vérifier le mode
    if (mode == READ_MODE) {
        serverFile->num_lecteur--; // décrémenter le nombre de lecteurs
        if (serverFile->num_lecteur == 0) { // Si le nombre de lecteurs atteint zéro, déverrouiller le mutex
            pthread_mutex_unlock(&serverFile->mutex);
            remove_ServerFile(filename,serverFileArray); 
        }
        // printf("fichier supprimé \n");
        return 0;

    } else if (mode == WRITE_MODE) {
        pthread_mutex_unlock(&serverFile->mutex); // déverrouiller le mutex
        remove_ServerFile(filename,serverFileArray); // supprimer le fichier
        return 0; // Succès
    } else {
        // Mode invalide, retourner une erreur
        return -1;
    }
    
}


