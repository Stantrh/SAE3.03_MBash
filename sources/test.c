//commande pour compiler : gcc -o mbash mbash.c -lreadline -lhistory

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <wordexp.h>
#include <time.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <math.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TAILLEHISTORIQUE 100 // Pour le nombre de commandes que l'historique pourra contenir
#define MAXLI 2048

char* verifierPS1EtRetournerValeur(char *commande);
int changerPrompt(char *nouveauPrompt);

char commande[MAXLI];
pid_t pidParent; // Pid du processus qui exécute globalement l'application
int codeRetourGlobal = 0; // Code de retour qui change selon les commandes de l'utilisateur et leur succès/echec


// On crée une structure qui contiendra le nombre de commandes et les commandes elles-mêmes
typedef struct{
    char commandes[TAILLEHISTORIQUE][MAXLI]; // Contient les commandes de l'utilisateur
    int count; // Nombre de commandes
} Historique;

// On initialise l'historique pour pouvoir après l'utiliser dans mbash
Historique historique;

/**
 * Méthode simple pour effacer tout ce qu'il y a dans la console
 **/
void clearConsole() {
    printf("\033[H\033[J");
}

/**
 * Grâce à cette méthode on peut ajouter la commande que l'utilisateur vient d'entrer dans la structure qui contient les commandes
*/
void ajouterHistorique(Historique *historique, const char *cmd){
    if(historique->count < TAILLEHISTORIQUE){ // Si l'historique n'est pas complètement rempli
        strcpy(historique->commandes[historique->count], cmd);
        historique->count++;
    }else{ // Sinon il est plein et il faut décaler les commandes pour supprimer la toute première
        for(int i = 0; i < TAILLEHISTORIQUE - 1; i++){
            strcpy(historique->commandes[i], historique->commandes[i+1]);
        }
        strcpy(historique->commandes[TAILLEHISTORIQUE - 1], cmd);
    }
}

/**
 * Affiche sous forme de liste l'historique
*/
void afficherHistorique(const Historique *historique){
    for(int i = 0; i < historique->count; i++){
        printf("%d: %s\n", i + 1, historique->commandes[i]);
    }
}



// reussite : 0
// échec : -1
// méthode qui permet de changer le répertoire courant
int cd(char *nouveauDossier) {
    // si l'on veut se déplacer dans le home (~) alors il faut associer le ~ au home (applelé expansion)
    if (nouveauDossier == NULL || nouveauDossier[0] == '~') {
        // Si le chemin commence par ~, on l'expande manuellement
        const char *home = getenv("HOME");
        if (home != NULL) {
            char cheminComplet[PATH_MAX]; // PATH_MAX c'est une constante qui représente la taille max d'un chemin de fichier (au cas où)
            // On doit faire la vérification car on peut pas modifier nouveauDossier dans un if sinon modification effective que dans le if
            // Donc s'il est null, alors on change la manière de faire le sprintf
            if (nouveauDossier == NULL) {
                snprintf(cheminComplet, sizeof(cheminComplet), "%s", home);
            } else {
                snprintf(cheminComplet, sizeof(cheminComplet), "%s%s", home, nouveauDossier + 1); // On met dans cheminComplet  home avec le reste du chemin
            }
            int res = chdir(cheminComplet);
            if (res == -1) {
                perror("chdir");
            }
            return res;
        } else {
            fprintf(stderr, "Erreur : Impossible de récupérer le répertoire home\n");
            return -1;
        }
    } else {
        // Si ce n'est pas ~, on change simplement de répertoire
        int res = chdir(nouveauDossier);
        if (res == -1) {
            perror("chdir");
        }
        return res;
    }
}

// Permet d'indiquer si on continue la boucle ou pas (nécessite 2 CTRL + C pour arrêter tout le programme)
int continuerBoucle = 1;

// Avec ça on peut gérer l'interruption soit de la boucle soit du programme complet
void gestionnaireSignal(int signal) {
    if (signal == SIGINT) {
        if (continuerBoucle) {
            continuerBoucle = 0; // Permet d'arrêter la boucle
        } else {
            exit(EXIT_SUCCESS);
        }
    }
}

int numBackgroundJobs = 1;

void mbash(char* recuperer) {
    // On associe à SIGINT la fonction qui gère les CTRL + C
    signal(SIGINT, gestionnaireSignal);


    // printf("Chemin de l'exécutable : %s%s", recuperer, "\n");

    // si on rentre dans le if c'est que l'utilisateur veut changer la variable PS1
    if(strcmp(verifierPS1EtRetournerValeur(commande), "pasUneCommandePourChangerLePS1") == 0){
        // On crée le tableau d'arguments pour execve (chemin, arguments, et variables d'environnement) 
        char* args[MAXLI];
        char* token = strtok(commande, " "); // On sépare la commande en parties séparées par les espaces
        int i = 0;


        while (token != NULL) { // Puis chaque partie séparée par un espace correspond à un argument
            args[i++] = token;
            token = strtok(NULL, " ");
        }


        args[i] = NULL; // Dernier élément du tableau doit être NULLS



        // On regarde si le dernier argument est un & et si oui alors on exécutera la commande en arrière plan
        int executeEnArrierePlan = 0;
        if ((i > 0 && strcmp(args[i - 1], "&") == 0) && (recuperer != NULL)) {
            printf("EXECUTER EN ARRRIERE PLAN \n");
            executeEnArrierePlan = 1;
            // args[i - 1] = NULL; // Supprime "&" de la liste des arguments
        }



        char* env[] = {NULL};

        // Affichage de chaque élément du tableau args
        // printf("Contenu de args :\n");
        // for (int j = 0; args[j] != NULL; ++j) {
        //    printf("args[%d] = %s\n", j, args[j]);
        // }

        if(recuperer == NULL){ // Si la commande which n'a rien retourné alors Sil faut qu'on vérifie le nom de la commande pour voir si elle a été reprogrammée par nos soins
            // Par convention et exigence de execve, le premier élément d'args doit toujours contenir le nom de la commande.
            // Donc on a le nom de la commande
            // Faire un switch est impossible car permet seulement de comparer un caractère à la fois ou des entiers etc... mais pas des chaînes directement
            if(strcmp(args[0], "cd") == 0){
                int res = cd(args[1]);
                codeRetourGlobal = res;
                if (res == -1) {
                    codeRetourGlobal = 1;
                }
                
            }else if(strcmp(args[0], "history") == 0){
                afficherHistorique(&historique);
                codeRetourGlobal = 0;
            }else if (strcmp(args[0], "cdd") == 0) {
                const char *imagePath = "lol.gif";

                while (continuerBoucle) {  // On continue la boucle tant que continuerBoucle vaut 1 
                    // Code Chafa pour générer l'image
                    char chafaCommand[100];
                    sprintf(chafaCommand, "chafa %s", imagePath);

                    // Exécution de la commande avec lecture
                    FILE *chafaOutput = popen(chafaCommand, "r");
                    if (chafaOutput == NULL) {
                        perror("Erreur lors de l'exécution de Chafa");
                        exit(EXIT_FAILURE);
                    }

                    // Et on redirige ça dans le terminal
                    char buffer[256];
                    while (fgets(buffer, sizeof(buffer), chafaOutput) != NULL) {
                        printf("%s", buffer);
                    }

                    // Fermeture du fichier de sortie de Chafa
                    pclose(chafaOutput);
                    codeRetourGlobal = 0;
                }
            }else if(strcmp(args[0], "cls") == 0){
                clearConsole();
                codeRetourGlobal = 0;
            }else if (strcmp(args[0], "$?") == 0) {
                printf("%d\n", codeRetourGlobal);
            } else if (strcmp(args[0], "$$") == 0) {
                printf("%d\n", pidParent);
                codeRetourGlobal = 0;
            }else if(strcmp(args[0], "help")== 0) {
                
            }else{
                codeRetourGlobal = 127;
                printf("\033[0;31m");
                printf("%s", "Commande à reprogrammer car contenue directement dans Bash ou alors inexistante\n");
            }
            
        }else{ // si récuperer != null
            // On crée un nouveau processus
            pid_t pid = fork();

            // Si c'est le processus fils qui exécute le code
            if(pid == 0){
                // Si la commande est destinée à s'exécuter en arrière-plan, redirige la sortie standard et d'erreur vers /dev/null
                if (executeEnArrierePlan) {
                    freopen("/dev/null", "w", stdout);
                    freopen("/dev/null", "w", stderr);
                }

                // On vérifie si ça contient $$ ou $?
                for (int j = 0; args[j] != NULL; ++j) {
                    if (strcmp(args[j], "$$") == 0) {
                        // On remplace $$ par le pid du parent 
                        char pidStr[20];
                        snprintf(pidStr, sizeof(pidStr), "%d", pidParent);
                        args[j] = strdup(pidStr);
                    } else if (strcmp(args[j], "$?") == 0) {
                        // On remplace $? par le dernier code de retour
                        char codeRetourStr[20];
                        snprintf(codeRetourStr, sizeof(codeRetourStr), "%d", codeRetourGlobal);
                        args[j] = strdup(codeRetourStr);
                    }
                }


                // On enregistre le code de retour de la commande avec execve
                int retour = execve(recuperer, args, env);
                codeRetourGlobal = retour;
                if(retour == -1){ // On catch une erreur et on l'affiche
                    perror("execve");
                    exit(EXIT_FAILURE); // On termine le processus fils
                }
            }else if(pid > 0){
                // Si la commande est destinée à s'exécuter en arrière plan on ne met pas de waitpid, sinon on en met un
                if (executeEnArrierePlan) {
                    printf("[%d] %d\n", numBackgroundJobs++, pid); // Affiche le PID du processus en arrière-plan
                } else {
                    waitpid(pid, &codeRetourGlobal, 0);
                }
            } else { // Si le fork n'a pas fonctionné on met l'erreur, (pas assez de mémoire par exemple)
                perror("fork");
                exit(EXIT_FAILURE); // On termine le processus actuel
            }

        }
    
    }else{
        char* valeurPS1 = malloc(strlen(commande) + 1);
        valeurPS1 = verifierPS1EtRetournerValeur(commande);
        changerPrompt(valeurPS1);
    }
}



/**
 * Méthode qui renvoie la commande permettant de récupérer le chemin d'un programme
 * Pour l'instant elle utilise /usr/bin/which mais on essayera de la refaire plus tard
 **/
char* recupererCheminCmd() {
    FILE *fp; // On en a besoin pour popen
    char path[MAXLI]; // Contiendra le chemin de la commande avec le which pour trouver l'endroit d'exécution de la commande passée

    char* chemin = (char*)malloc(MAXLI * 2 + 1);

     
    sprintf(chemin, "which %s", commande); // sprintf ça stocke dans un buffer pré aloué, donc là ça va stocker dans chemin la commande totale du which

    //printf("VALEUR DU SPRINTF : %s%s", chemin, "\n");

    // Ensuite ce que popen fait c'est qu'il exécute la commande which + nom commande et lit la sortie de cette commande
    fp = popen(chemin, "r");
    if (fp == NULL) { // Si ca vaut null y a probablement eu une erreur : La fonction popen() renvoie NULL si l'appel système fork(2) ou pipe(2) échoue, ou s'il n'a pas assez de mémoire. manpagesfree
        perror("popen");
        exit(EXIT_FAILURE);
    }

    // Ensuite on lit la sortie de l'exécution de la ligne de commande qui est contenue dans le flux fp
    if (fgets(path, MAXLI, fp) != NULL) { // Si c'est pas vide alors on mettra dans path la valeur retournée
        // On supprime le saut de ligne à la fin de la commande pour éviter les erreurs
        path[strcspn(path, "\n")] = '\0';
        
        strcpy(chemin, path);

    } else { // Sinon ça veut dire que ça n'a rien retourné, donc soit que l'utilisateur a entré une commande native à Bash (genre cd etc...) soit une commande qui n'existe pas
        // Pour l'instant on ne traite pas la différence entre les deux cas, on met l'automate d'abord.
        chemin = NULL;
    }

    pclose(fp); // Puis on ferme le flux une fois les opérations terminées
    return chemin;
}

// Méthode qui permet de changer le prompt du terminal
int changerPrompt(char *nouveauPrompt){
    //on change la variable d'environement qui correspond au PS1
    int res = setenv("PS1", nouveauPrompt, 1);
    return res;
}

char* recupererResultatComande(char* commande){
    //on créé un fichier pour stocker le résultat de pwd
    FILE *fp = popen(commande, "r");
    
    //tableau qui va stocker le résultat de la commande
    char res[1024];
    if (fgets(res, sizeof(res), fp) == NULL) {
        perror("fgets dans recupererResultatComande");
        exit(EXIT_FAILURE);
    }

    //on enlève le \n de la fin de la ligne pour ne pas retourner à la ligne
    res[strcspn(res, "\n")] = '\0';

    //on ferme le fichier, on en a plus besoin
    pclose(fp);

    return strdup(res);
}

char* informationsTemps(char unite){
    // variable qui va contenir le nombre de secondes écoulées depuis le 1er janvier 1970
    time_t tempsActuel;
    //structure qui permet de stocker des informations sur le temps
    struct tm *tempsInfo;

    //on récupère le nombre de secondes écoulées depuis le 1er janvier 1970
    time(&tempsActuel);
    //on les convertit pour obtenir des valeurs plus lisibles
    tempsInfo = localtime(&tempsActuel);

    //création du prompt
    char temps[40];

    if(unite == 'd'){//date de la forme : mer. janv. 17
        strftime(temps, sizeof(temps), "%a. %b. %d", tempsInfo);
        //on ajoute la date au prompt
        return strdup(temps);
    }else if(unite == 't'){ //heure au format HH:MM:SS
        sprintf(temps, "%02d:%02d:%02d", tempsInfo->tm_hour, tempsInfo->tm_min, tempsInfo->tm_sec);
        return strdup(temps);
    }
    
    return "error";
}
char* recupererPromptCourant() {
    // On utilise strdup pour créer une copie de la chaîne PS1
    char* prompt = strdup(getenv("PS1"));
    if (!prompt) {
        perror("Erreur lors de la récupération du prompt");
        exit(EXIT_FAILURE);
    }

    // Taille maximale du prompt
    char promptCourant[256]; // Utilisation d'une taille statique pour simplifier

    // Initialisation de promptCourant
    promptCourant[0] = '\0';
    // //taille max du prompt
    // char* promptCourant = malloc(256);
    for (int i = 0; prompt[i] != '\0'; i++) {
        if (prompt[i] == '\\') {
            i++;
            if (prompt[i] != '\0') {
                switch (prompt[i]) {
                    case 'w':
                        strcat(promptCourant, recupererResultatComande("pwd"));
                        break;
                    case 'u':
                        strcat(promptCourant, recupererResultatComande("whoami"));
                        break;
                    case 't':
                        strcat(promptCourant, informationsTemps('t'));
                        break;
                    case 'd':
                        strcat(promptCourant, informationsTemps('d'));
                        break;
                    case '\\':
                        strcat(promptCourant, "\\");
                        break;
                    default:
                        // Pour les caractères inconnus, on les ajoute simplement au promptCourant
                        strncat(promptCourant, &prompt[i], 1);
                        break;
                }
            } else if (prompt[i] == '\0') {
                strcpy(promptCourant, ">");
            }
        } else {
            // On ajoute le caractère actuel au promptCourant
            strncat(promptCourant, &prompt[i], 1);
        }
    }

    // On libère la mémoire allouée pour prompt, car nous avons maintenant une copie
    free(prompt);

    // On ajoute "$" à la fin du prompt
    strcat(promptCourant, " $ ");

    // On alloue de la mémoire pour le résultat final et on le retourne
    return strdup(promptCourant);
}

// méthode qui permet de vérifier si une commande PS1 est valide et qui retourne la valeur du PS1
char* verifierPS1EtRetournerValeur(char *commande) {
    #define S_DEPART                    1
    #define S_LETTRE_P                  2
    #define S_LETTRE_S                  3
    #define S_CHIFFRE_1                 4
    #define S_EGAL                      5
    #define S_LETTRE_APRES_EGAL         6
    #define S_DEBUT_DBQUOTE             7
    #define S_LETTRE_APRES_DBQUOTE      8
    #define S_FIN_DBQUOTE               9
    #define S_FINI                      10
    #define S_ERREUR                    11

    // état de départ
    int state = S_DEPART;
    // compteur pour parcourir tous les caractères de la commande
    int indice = 0;

    // valeur du PS1
    char *ps1 = strdup("");

    // on parcourt toute la commande qui est à vérifier, jusqu'à ce qu'on arrive à la fin ou qu'on tombe sur une erreur 
    while(state < S_FINI){
        // on récupère le premier caractère de la commande
        char caractereCourant = commande[indice];
        // on incrémente le compteur pour la vérification du prochain caractère de la commande
        indice += 1;
        // pour tous les caractères de la commande, on vérifie s'ils correspondent bien à une commande valide
        switch (state){
            case S_DEPART:
                switch (caractereCourant){
                    case 'P':
                        state = S_LETTRE_P;
                        break;
                    case ' ':
                        state = S_DEPART;
                    default:
                        state = S_ERREUR;
                        break;
                }
                break;
            case S_LETTRE_P:
                switch(caractereCourant){
                    case 'S':
                        state = S_LETTRE_S;
                        break;
                    default: 
                        state = S_ERREUR;
                        break;
                }
                break;
            case S_LETTRE_S:
                switch(caractereCourant){
                    case '1':
                        state = S_CHIFFRE_1;
                        break;
                    default:
                        state = S_ERREUR;
                        break;
                }
                break;
            case S_CHIFFRE_1:
                switch(caractereCourant){
                    case '=':
                        state = S_EGAL;
                        break;
                    default:
                        state = S_ERREUR;
                        break;
                }
                break;
            case S_EGAL:
                switch(caractereCourant){
                    case '"':
                        state = S_DEBUT_DBQUOTE;
                        break;
                    default:
                        strncat(ps1, &caractereCourant, 1);
                        state = S_LETTRE_APRES_EGAL;
                        break;
                }
                break;
            case S_LETTRE_APRES_EGAL:
                switch(caractereCourant){
                    case '\0':
                        state = S_FINI;
                        break;
                    default:
                        strncat(ps1, &caractereCourant, 1);
                        state = S_LETTRE_APRES_EGAL;
                        break;
                }
                break;
            case S_DEBUT_DBQUOTE:
                switch(caractereCourant){
                    case '"':
                        state = S_FINI;
                        break;
                    case '\0':
                        state = S_ERREUR;
                        break;
                    default:
                        strncat(ps1, &caractereCourant, 1);
                        state = S_LETTRE_APRES_DBQUOTE;
                        break;
                }
                break;
            case S_LETTRE_APRES_DBQUOTE:
                switch(caractereCourant){
                    case '"':
                        state = S_FINI;
                        break;
                    case '\0':
                        state = S_ERREUR;
                        break;
                    default:
                        strncat(ps1, &caractereCourant, 1);
                        state = S_LETTRE_APRES_DBQUOTE;
                        break;
                }
                break;
        }
    }

    // Vous n'avez pas besoin de realloc ici, strdup alloue déjà suffisamment d'espace.
    if(state == S_ERREUR){
        free(ps1); // Libérez la mémoire allouée pour ps1 en cas d'erreur
        return "pasUneCommandePourChangerLePS1"; // 1 correspond à une erreur 
    }

    // réussite
    return ps1;
}



int main(int argc, char** argv) {
    historique.count = 0;

    pidParent = getpid();

    clearConsole();
    changerPrompt("\\u");


    printf(R"EOF(


                     __  ___    ___           __    _  __
                    /  |/  /___/ _ )___ ____ / /   | |/_/
                   / /|_/ /___/ _  / _ `(_-</ _ \  >  <  
                  /_/  /_/   /____/\_,_/___/_//_/ /_/|_| v0.1 
      
                  Par PIERROT Nathan et TROHA Stanislas 
                                                    


)EOF");

    while (1) {

        char* prompt = recupererPromptCourant();
        
        // on alloue de la mémoire pour le nouveau prompt
        char *promptAvecCouleur = malloc(strlen(prompt) + 20);

        //on ajoute de la couleur au prompt et on remet la couleur blanche pour la commande tapée par l'utilisateur
        strcpy(promptAvecCouleur, "\033[0;35m");
        strcat(promptAvecCouleur, prompt);
        strcat(promptAvecCouleur, "\033[0;37m");

        char *input = readline(promptAvecCouleur);

        if (!input) {
            // Gestion de la fin du fichier ou de l'erreur de lecture
            break;
        }

        if (input[0] != '\0') {
            // Ajouter la ligne à l'historique seulement si elle n'est pas vide
            add_history(input);
            ajouterHistorique(&historique, input);
            strcpy(commande, input);
        }else{
            continue;
        }

        if(strcmp(input, "exit") == 0){
            printf("\033[0;32m");
            printf("Merci d'avoir utilisé Mbash\n");
            exit(EXIT_SUCCESS);
        }


        mbash(recupererCheminCmd());



        free(input);

        free(prompt);


    
    }

    return 0;
}