
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef SYNC

#define SYNC
#define MAX_FILENAME_LENGTH 256


typedef enum {
    READ_MODE,   // Mode lecture
    WRITE_MODE,  // Mode écriture
    APPEND_MODE, // Mode ajout à la fin du fichier
} FileMode;


// Structure pour représenter un fichier ouvert par le serveur
typedef struct {
    char filename[MAX_FILENAME_LENGTH]; // Nom du fichier
    int num_lecteur; // Numéro de lecteur
    pthread_mutex_t mutex; // Mutex pour la synchronisation
} ServerFile;


// Structure pour le tableau dynamique de ServerFile
typedef struct {
    ServerFile** array; // Tableau de ServerFile
    size_t size; // Nombre d'éléments actuellement dans le tableau
} ServerFileArray;



/**
 * \brief Crée un nouvel objet ServerFile avec le nom de fichier spécifié.
 * 
 * Alloue la mémoire pour un nouvel objet ServerFile, initialise ses champs
 * avec le nom de fichier fourni, initialise le nombre de lecteurs à 0 et
 * initialise le mutex pour la synchronisation.
 * 
 * \param filename Le nom du fichier pour le nouvel objet ServerFile.
 * \return Un pointeur vers le nouvel objet ServerFile créé, ou NULL en cas d'erreur.
 */

ServerFile* create_ServerFile(const char* filename);



/**
 * \brief Ajoute un ServerFile au tableau dynamique de ServerFileArray.
 * 
 * \param serverFile Le ServerFile à ajouter.
 * \param serverFileArray Le tableau dynamique de ServerFile.
 * \return 0 en cas de succès, -1 en cas d'erreur.
 */
int add_ServerFile(ServerFile* serverFile, ServerFileArray* serverFileArray);



/**
 * \brief Recherche un fichier dans le tableau dynamique de ServerFileArray.
 * 
 * \param filename Le nom du fichier à rechercher.
 * \param serverFileArray Le tableau dynamique de ServerFile.
 * \return Un pointeur vers le ServerFile trouvé, ou NULL s'il n'est pas trouvé.
 */
ServerFile* search_ServerFile(const char* filename, ServerFileArray* serverFileArray);




/**
 * \brief Supprime un fichier du tableau dynamique de ServerFileArray.
 * 
 * \param filename Le nom du fichier à supprimer.
 * \param serverFileArray Le tableau dynamique de ServerFile.
 * \return 0 en cas de succès, -1 en cas d'erreur.
 */
int remove_ServerFile(const char* filename, ServerFileArray* serverFileArray);




/**
 * \brief Initialise un tableau dynamique de ServerFileArray.
 * 
 * \param serverFileArray Le tableau dynamique de ServerFile à initialiser.
 */
void initialize_serverFileArray(ServerFileArray* serverFileArray);





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
int start_file_session(const char* filename, int mode, ServerFileArray* serverFileArray);




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
int stop_file_session(const char* filename, int mode, ServerFileArray* serverFileArray);



#endif