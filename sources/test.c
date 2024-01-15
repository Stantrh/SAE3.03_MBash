#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXLI 2048

char commande[MAXLI];

void mbash(char* recuperer);
char* recupererCheminCmd();



int main(int argc, char** argv) {

    printf(R"EOF(


               __  ___    ___           __     _  __
              /  |/  /___/ _ )___ ____ / /    | |/_/
             / /|_/ /___/ _  / _ `(_-</ _ \  _>  <  
            /_/  /_/   /____/\_,_/___/_//_/ /_/|_|  
                                                    


)EOF");

    while (1) {
        printf("CMD : ");

        if (fgets(commande, MAXLI, stdin) != NULL) {
            commande[strcspn(commande, "\n")] = '\0';

            // Si l'utilisateur a entré quelque chose de vide (du style entrée)
            if (strlen(commande) == 0) {
                continue; // On recommence la boucle pour lui redemander
            }

            char* recuperer = recupererCheminCmd();
            mbash(recuperer);
            free(recuperer);
        } else {
            perror("fgets");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}

void mbash(char* recuperer) {
    // On crée un nouveau processus
    pid_t pid = fork();



    // Si le pid actuel est le pid du processus fils qui vient d'être créé alors le code qui va suivre c'est le processus fils qui va l'exécuter
    if (pid == 0) {
        printf("Chemin de l'exécutable : %s%s", recuperer, "\n");
        // On crée le tableau d'arguments pour execve (chemin, arguments, et variables d'environnement) 
        char* args[] = {recuperer, NULL};
        char* env[] = {NULL};

        // On enregistre le code de retour de la commande avec execve
        int retour = execve(recuperer, args, env);
        if(retour == -1){ // On catch une erreur et on l'affiche
            perror("execve");
            exit(EXIT_FAILURE); // On termine le processus fils
        }
    } else if (pid > 0) { // Si le pid est supérieur à 0 alors c'est encore le processus parent
        waitpid(pid, NULL, 0); // Donc il faut attendre que son processus fils se finisse
    } else { // Si le fork n'a pas fonctionné on met l'erreur, (pas assez de mémoire par exemple)
        perror("fork");
        exit(EXIT_FAILURE); // On termine le processus actuel
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

    // 
    sprintf(chemin, "which %s", commande); // sprintf ça stocke dans un buffer pré aloué, donc là ça va stocker dans chemin la commande totale du which

    printf("VALEUR DU SPRINTF : %s%s", chemin, "\n");

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
        printf("%s", "Commande à reprogrammer car contenue directement dans Bash ou alors inexistante\n");
    }

    pclose(fp); // Puis on ferme le flux une fois les opérations terminées
    return chemin;
}
