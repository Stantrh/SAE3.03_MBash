#include <stdio.h>
#include <ctype.h>

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
                    default:
                        if(isalpha(caractereCourant)){
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




