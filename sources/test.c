#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAXLI 2048

char commande[MAXLI];

void mbash(char* recuperer);
char* recupererCheminCmd();



int main(int argc, char** argv) {

    printf(R"EOF(


               __  ___    ___           __    _  __
              /  |/  /___/ _ )___ ____ / /   | |/_/
             / /|_/ /___/ _  / _ `(_-</ _ \  >  <  
            /_/  /_/   /____/\_,_/___/_//_/ /_/|_| v0.1 

            Par PIERROT Nathan et TROHA Stanislas 
                                                    


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
        int succes = verifierCommande(path);
        printf("Succes de la commande : %d%s", succes, "\n");

    } else { // Sinon ça veut dire que ça n'a rien retourné, donc soit que l'utilisateur a entré une commande native à Bash (genre cd etc...) soit une commande qui n'existe pas
        // Pour l'instant on ne traite pas la différence entre les deux cas, on met l'automate d'abord.
        printf("%s", "Commande à reprogrammer car contenue directement dans Bash ou alors inexistante\n");
    }

    pclose(fp); // Puis on ferme le flux une fois les opérations terminées
    return chemin;
}


// méthode qui permet de vérifier si une commande est valide.
// Ne prend pas en compte les compte les commandes avec des chiffres
// retours possibles :
//   - 0 : succès
//   - 1 : échec
//   - 2 : commande se terminant par un &, il faut alors la lancer en arrière plan
int verifierCommande(char *commande){

    // expression régulière de l'automate : 
    // \s*[a-zA-Z]+\s*(-{1}[a-zA-Z]+\s*)*

    #define S_DEPART                1
    #define S_LETTRE                2
    #define S_ESPACE                3
    #define S_TIRET                 4
    #define S_LETTRE_APRES_TIRET    5
    #define S_ET_COMMERCIAL         6
    #define S_FINI                  7
    #define S_ERREUR                8

    // état de départ
    int state = S_DEPART;
    // compteur pour parcourir tous les caractères de la commande
    int indice = 0;

    //on parcourt toute la commande qui est à vérifier, jusqu'a ce qu'on arrive à la fin ou qu'on tombe sur une erreur 
    while(state < S_FINI){
        //on récupère le premier caractère de la commande
        char caractereCourant = commande[indice];
        //on incrémente le compteur pour la vérification du prochain caractère de la commande
        indice +=1;
        // pour tout les caractères de la commande, on vérifie si ils corespondent bien à une commande valide
        switch (state){
            // pour le premier caractère, il peut être soit un espace soit une lettre
            case S_DEPART:
                switch (caractereCourant){
                    case ' ': //si c'est un espace
                        state = S_DEPART;
                        break;
                    case '\0': // normalement pas possible d'en arriver là
                        state = S_ERREUR;
                        break;
                    default: // si ce n'est pas un espace : c'est une lettre ou autre chose
                        if(isalpha(caractereCourant)){ // si c'est une lettre, on continue
                            state = S_LETTRE;
                        }else{ // si ce n'est pas une lettre alors erreur
                            state = S_ERREUR;
                        }
                        break;
                }
                break;
            case S_LETTRE: // pour les premières lettre de la commande
                switch(caractereCourant){
                    case ' ':
                        state = S_ESPACE;
                        break;
                    case '\0': // \0 correspond au caractère de fin du tableau (et donc de la commande)
                        state = S_FINI;
                        break;
                    case '&':
                        state = S_ET_COMMERCIAL;
                        break;
                    default:
                        if(isalpha(caractereCourant)){
                            printf("%s", "COUCOUCOUCOU");
                            state = S_LETTRE;
                        }else{
                            state = S_ERREUR;
                        }
                        break;
                }
                break;
            case S_ESPACE: // si on tombe sur un espace à l'intérieur de la commande
                // il peut être suivi de : 
                switch(caractereCourant){
                    case ' ': // un autre espace
                        state = S_ESPACE;
                        break;
                    case '-': // un tiret 
                        state = S_TIRET;
                        break;
                    case '\0': // la fin de la commande
                        state = S_FINI;
                        break;
                    case '&':
                        state = S_ET_COMMERCIAL;
                        break;
                    default:
                        state = S_ERREUR;
                        break;
                }
                break;
            case S_TIRET: // si on arrive sur un tiret, le caractère suivant peut être uniquement une lettre 
                if(isalpha(caractereCourant)){
                    state = S_LETTRE_APRES_TIRET;
                }else{
                    state = S_ERREUR;
                }
                break;
            case S_LETTRE_APRES_TIRET:
                switch(caractereCourant){
                    case ' ':
                        state = S_ESPACE;
                        break;
                    case '\0':
                        state = S_FINI;
                        break;
                    default:
                        if(isalpha(caractereCourant)){
                            state = S_LETTRE_APRES_TIRET;
                        }else{
                            state = S_ERREUR;
                        }
                        break;
                }
                break;
            case S_ET_COMMERCIAL: // après un & on ne peut trouver que des espaces
                switch(caractereCourant){
                    case ' ': // si on a un espace alors on retourne dans cette partie de l'automate jusqu'à arriver à la fin de la commande ou jusqu'à ce qu'on tombe sur un caractère 
                        state = S_ET_COMMERCIAL;
                        break;
                    case '\0':
                        state = S_FINI;
                        break;
                    default: // si on tombe sur autre chose qu'un espace alors il y a une erreur
                        state = S_ERREUR;
                        break;
                }
        }
    }
    if(state == S_ET_COMMERCIAL) return 2; // correspond à une commande à lancer en arrière plan

    if(state == S_ERREUR) return 1; // 1 correspond à une erreur 

    return 0; //réussite
}