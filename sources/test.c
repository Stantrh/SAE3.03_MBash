#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <wordexp.h>


#define TAILLEHISTORIQUE 100 // Pour le nombre de commandes que l'historique pourra contenir
#define MAXLI 2048

char commande[MAXLI];



// On crée une structure qui contiendra le nombre de commandes et les commandes elles-mêmes
typedef struct{
    char commandes[TAILLEHISTORIQUE][MAXLI];
    int count;
} Historique;

// On initialise l'historique pour pouvoir après l'utiliser dans mbash
Historique historique;


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

void afficherHistorique(const Historique *historique){
    for(int i = 0; i < historique->count; i++){
        printf("%d: %s\n", i + 1, historique->commandes[i]);
    }
}



// reussite : 0
// échec : -1
// méthode qui permet de changer le répertoire courant
int cd(char *nouveauDossier){
    // si l'on veut se déplacer dans le home (~) alors il faut associer le ~ au home (applelé expansion)
    if(nouveauDossier[0]=='~'){
        //variable temporaire utilisée pour l'assiciation du home
        wordexp_t tmp;
        if (wordexp(nouveauDossier, &tmp, 0) == 0) {
            // variable contenant le résultat de l'expansion, le chemin du home se trouva dans la première case du tableau
            chdir(tmp.we_wordv[0]);
            // on libère la mémoire de la variable temporaire et de tout ce qu'elle peut contenir
            wordfree(&tmp);
        }
    }
    // on change de répertoire
    int res = chdir(nouveauDossier);
    if(res == -1){
        perror("chdir");
    }
    return res;
}

void mbash(char* recuperer) {
    // On crée un nouveau processus
    pid_t pid = fork();


    // Si le pid actuel est le pid du processus fils qui vient d'être créé alors le code qui va suivre c'est le processus fils qui va l'exécuter
    if (pid == 0) {

        //printf("Chemin de l'exécutable : %s%s", recuperer, "\n");

        // On crée le tableau d'arguments pour execve (chemin, arguments, et variables d'environnement) 
        char* args[MAXLI];
        char* token = strtok(commande, " "); // On sépare la commande en parties séparées par les espaces
        int i = 0;

        while (token != NULL) { // Puis chaque partie séparée par un espace correspond à un argument
            args[i++] = token;
            token = strtok(NULL, " ");
        }

        args[i] = NULL; // Dernier élément du tableau doit être NULLS
        char* env[] = {NULL};

        // Affichage de chaque élément du tableau args
        //printf("Contenu de args :\n");
        //for (int j = 0; args[j] != NULL; ++j) {
        //    printf("args[%d] = %s\n", j, args[j]);
        //}

        if(recuperer == NULL){ // Si la commande which n'a rien retourné alors il faut qu'on vérifie le nom de la commande pour voir si elle a été reprogrammée par nos soins
             // Par convention et exigence de execve, le premier élément d'args doit toujours contenir le nom de la commande.
                // Donc on a le nom de la commande
            // Faire un switch est impossible car permet seulement de comparer un caractère à la fois ou des entiers etc... mais pas des chaînes directement
            if(strcmp(args[0], "cd") == 0){

                if (args[1] != NULL) {
                    int res = cd(args[1]);
                    if (res == -1) {
                        fprintf(stderr, "Erreur lors de l'exécution de la commande cd\n");
                    }
                } else {
                    fprintf(stderr, "Erreur : Argument manquant pour cd\n");
                }
            }else if(strcmp(args[0], "history") == 0){
                afficherHistorique(&historique);
            }else{
                printf("%s", "Commande à reprogrammer car contenue directement dans Bash ou alors inexistante\n");
            }
            
        }else{
                // On enregistre le code de retour de la commande avec execve
            int retour = execve(recuperer, args, env);
            if(retour == -1){ // On catch une erreur et on l'affiche
                perror("execve");
                exit(EXIT_FAILURE); // On termine le processus fils
            }
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


int main(int argc, char** argv) {
    historique.count = 0;


    printf(R"EOF(


               __  ___    ___           __    _  __
              /  |/  /___/ _ )___ ____ / /   | |/_/
             / /|_/ /___/ _  / _ `(_-</ _ \  >  <  
            /_/  /_/   /____/\_,_/___/_//_/ /_/|_| v0.1 

            Par PIERROT Nathan et TROHA Stanislas 
                                                    


)EOF");

    while (1) {
        printf("\nCMD : ");

        if (fgets(commande, MAXLI, stdin) != NULL) {
            commande[strcspn(commande, "\n")] = '\0';

            // Si l'utilisateur a entré quelque chose de vide (du style entrée)
            if (strlen(commande) == 0) {
                continue; // On recommence la boucle pour lui redemander
            }

            ajouterHistorique(&historique, commande);

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


