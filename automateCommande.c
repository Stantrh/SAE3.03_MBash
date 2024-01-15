#include <stdio.h>
#include <ctype.h>

// méthode qui permet de vérifier si une commande est valide.
// Ne prend pas en compte les compte les commandes avec des chiffres
public int verifierCommande(char *commande){

    // expression régulière de l'automate : 
    // \s*[a-zA-Z]+\s*(-{1}[a-zA-Z]+\s*)*

    #define S_DEPART                1
    #define S_LETTRE                2
    #define S_ESPACE                3
    #define S_TIRET                 4
    #define S_LETTRE_APRES_TIRET    5
    #define S_FINI                  6
    #define S_ERREUR                7

    // état de départ
    state = S_DEPART;
    // compteur pour parcourir tous les caractères de la commande
    int indice = 0;

    //on parcourt toute la commande qui est à vérifier, jusqu'a ce qu'on arrive à la fin ou qu'on tombe sur une erreur 
    while(state < S_FINI){
        //on récupère le premier caractère de la commande
        caractereCourant = commande[indice];
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
        }
    }
    if(state == S_ERREUR){
        return 1; // 1 correspond à une erreur 
    }

    //réussite
    return 0
}



