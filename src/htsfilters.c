/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: httrack.c subroutines:                                 */
/*       filters ("regexp")                                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

// *.gif                  match all gif files 
// *[file]/*[file].exe    match all exe files with one folder structure
// *[A-Z,a-z,0-9,/,?]     match letters, nums, / and ?
// *[A-Z,a-z,0-9,/,?]

// *[>10,<100].gif        match all gif files larger than 10KB and smaller than 100KB
// *[file,>10,<100].gif   FORBIDDEN: you must not mix size test and pattern test

#include "htsfilters.h"

/* specific definitions */
#include "htsbase.h"
#include "htslib.h"
#include <ctype.h>
/* END specific definitions */

// à partir d'un tableau de {"+*.toto","-*.zip","+*.tata"} définit si nom est autorisé
// optionnel: taille à contrôller (ou numéro, etc) en pointeur
//            (en de détection de *size, la taille limite est écrite par dessus *size)
// exemple: +-*.gif*[<5] == supprimer GIF si <5KB

// from an array of {"+ *. foo", "- *. zip", "+ *. tata"} defines if name is allowed
// optional: size to control (or number, etc.) in pointer
// (in detection of * size, the size limit is written on top * size)
// example: + - *. gif * [<5] == remove GIF if <5KB

// joker = wildcard
int fa_strjoker(int type, char **filters, int nfil, const char *nom, LLint * size,
                int *size_flag, int *depth) {
  int verdict = 0;              // on sait pas
  int i;
  LLint sizelimit = 0;

  if (size)
    sizelimit = *size;
  for(i = 0; i < nfil; i++) {
    LLint sz;
    int filteroffs = 1;

    if (strncmp(filters[i] + filteroffs, "mime:", 5) == 0) {
      if (type == 0)            // regular filters
        continue;
      filteroffs += 5;          // +mime:text/html
    } else {                    // mime filters
      if (type != 0)
        continue;
    }
    if (size)
      sz = *size;
    if (strjoker(nom, filters[i] + filteroffs, &sz, size_flag)) {       // reconnu # recognised
      if (size)
        if (sz != *size)
          sizelimit = sz;
      if (filters[i][0] == '+')
        verdict = 1;            // autorisé # authorized
      else
        verdict = -1;           // interdit # not allowed
      if (depth)
        *depth = i;
    }
  }
  if (size)
    *size = sizelimit;
  return verdict;
}

// supercomparateur joker (tm)
// compare a et b (b=avec joker dedans), case insensitive [voir CI] # compare a and b (b = with joker in), insensitive box [see CI]
// renvoi l'adresse de la première lettre de la chaine # return the address of the first letter of the chain
// (càd *[..]toto.. renvoi adresse de toto dans la chaine) # (ie * [..] foo .. return address of foo in the chain)
// accepte les délires du genre www.*.*/ * / * truc*.* # accept delusions like www. *. * / * / * stuff *. *
// cet algo est 'un peu' récursif mais ne consomme pas trop de tm # this algo is 'a bit' recursive but does not consume too much tm
// * = toute lettre # * = any letter
// --?-- : spécifique à HTTrack et aux ? # --?-- : specific to HTTrack and?
HTS_INLINE const char *strjoker(const char *chaine, const char *joker, LLint * size,
                          int *size_flag) {
  //int err=0;
  if (strnotempty(joker) == 0) {        // fin de chaine joker # end of wildcard
    if (strnotempty(chaine) == 0)       // fin aussi pour la chaine: ok # end also for the chain: ok
      return chaine;
    else if (chaine[0] == '?')
      return chaine;            // --?-- pour les index.html?Choix=2 # --?-- for index.html?Choix=2
    else
      return NULL;              // non trouvé # Not found
  }
  // on va progresser en suivant les 'mots' contenus dans le joker # we will progress by following the 'words' contained in the joker
  // un mot peut être un * ou bien toute autre séquence de lettres # a word can be a * or any other sequence of letters

  if (strcmp(joker, "*") == 0) {        // ok, rien après # ok, nothing after
    return chaine;
  }
  // 1er cas: jokers * ou jokers multiples *[..] # case 1: wildcards * or multiple wildcards *[..]
  if (joker[0] == '*') {        // comparer joker+reste (*toto/..) # compare wildcard+remainder (*toto/..)
    int jmp;                    // nombre de caractères pour le prochain mot dans joker # number of characters for the next word in joker
    int cut = 0;                // interdire tout caractère superflu # prohibit any superfluity
    char pass[256];
    char LEFT = '[', RIGHT = ']';
    int unique = 0;

    switch (joker[1]) {
    case '[':
      LEFT = '[';
      RIGHT = ']';
      unique = 0;
      break;
    case '(':
      LEFT = '(';
      RIGHT = ')';
      unique = 1;
      break;
    }

    if ((joker[1] == LEFT) && (joker[2] != LEFT)) {     // multijoker (tm) # multi wild card (tm)
      int i;

      for(i = 0; i < 256; i++)
        pass[i] = 0;

      // noms réservés # reserved names
      if ((strfield(joker + 2, "file")) || (strfield(joker + 2, "name"))) {
        for(i = 0; i < 256; i++)
          pass[i] = 1;
        pass[(int) '?'] = 0;
        //pass[(int) ';'] = 0;
        pass[(int) '/'] = 0;
        i = 2;
        {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len))
            i++;
        }
      } else if (strfield(joker + 2, "path")) {
        for(i = 0; i < 256; i++)
          pass[i] = 1;
        pass[(int) '?'] = 0;
        //pass[(int) ';'] = 0;
        i = 2;
        {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len))
            i++;
        }
      } else if (strfield(joker + 2, "param")) {
        if (chaine[0] == '?') { // il y a un paramètre juste là # there is a parameter right there
          for(i = 0; i < 256; i++)
            pass[i] = 1;
        }                       // sinon synonyme de 'rien' # otherwise synonymous with 'nothing'
        i = 2;
        {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len))
            i++;
        }
      } else {
        // décode les directives comme *[A-Z,âêîôû,0-9] # decodes the directives as *[A-Z,âêîôû,0-9]
        i = 2;
        if (joker[i] == RIGHT) {        // *[] signifie "plus rien après" # *[] means "nothing after"
          cut = 1;              // caractère supplémentaire interdit # additional character forbidden
        } else {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len)) {
            if ((joker[i] == '<') || (joker[i] == '>')) {       // *[<10]
              int lsize = 0;
              int lverdict;

              i++;
              if (sscanf(joker + i, "%d", &lsize) == 1) {
                if (size) {
                  if (*size >= 0) {
                    if (size_flag)
                      *size_flag = 1;   /* a joué # played */
                    if (joker[i - 1] == '<')
                      lverdict = (*size < lsize);
                    else
                      lverdict = (*size > lsize);
                    if (!lverdict) {
                      return NULL;      // ne correspond pas # does not match
                    } else {
                      *size = lsize;
                      return chaine;    // ok
                    }
                  } else
                    return NULL;        // ne correspond pas # does not match
                } else
                  return NULL;  // ne correspond pas (test impossible) # does not match (impossible test)
                // jump
                while(isdigit((unsigned char) joker[i]))
                  i++;
              }
            } else if (joker[i + 1] == '-') {   // 2 car, ex: *[A-Z]
              if ((int) (unsigned char) joker[i + 2] >
                  (int) (unsigned char) joker[i]) {
                int j;

                for(j = (int) (unsigned char) joker[i];
                    j <= (int) (unsigned char) joker[i + 2]; j++)
                  pass[j] = 1;

              }
              // else err=1;
              i += 3;
            } else {            // 1 car, ex: *[ ]
              if (joker[i + 2] == '\\' && joker[i + 3] != 0) {  // escaped char, such as *[\[] or *[\]]
                i++;
              }
              pass[(int) (unsigned char) joker[i]] = 1;
              i++;
            }
            if ((joker[i] == ',') || (joker[i] == ';'))
              i++;
          }
        }
      }
      // à sauter dans joker # jump into wildcard
      jmp = i;
      if (joker[i])
        jmp++;

      //
    } else {                    // tout autoriser # authorize everything
      //
      int i;

      for(i = 0; i < 256; i++)
        pass[i] = 1;            // tout autoriser # authorize everything
      jmp = 1;
      ////if (joker[2]==LEFT) jmp=3;        // permet de recher *<crochet ouvrant> # allows searching *<opening hook>
    }

    {
      int i, max;
      const char *adr;

      // la chaine doit se terminer exactement # the string must end exactly
      if (cut) {
        if (strnotempty(chaine))
          return NULL;          // perdu
        else
          return chaine;        // ok
      }
      // comparaison en boucle, c'est ca qui consomme huhu.. # loop comparison, that's what consumes huhu ..
      // le tableau pass[256] indique les caractères ASCII autorisés # the pass array [256] indicates the ASCII characters allowed

      // tester sans le joker (pas ()+ mais ()*) # test without the wildcard (not () + but () *)
      if (!unique) {
        if ((adr = strjoker(chaine, joker + jmp, size, size_flag))) {
          return adr;
        }
      }
      // tester
      i = 0;
      if (!unique)
        max = (int) strlen(chaine);
      else                      /* *(a) only match a (not aaaaa) */
        max = 1;
      while(i < (int) max) {
        if (pass[(int) (unsigned char) chaine[i]]) {    // caractère autorisé # authorized char
          if ((adr = strjoker(chaine + i + 1, joker + jmp, size, size_flag))) {
            return adr;
          }
          i++;
        } else
          i = max + 2;          // sortir # exit
      }

      // tester chaîne vide # test empty string
      if (i != max + 2)         // avant c'est ok # before it's ok
        if ((adr = strjoker(chaine + max, joker + jmp, size, size_flag)))
          return adr;

      return NULL;              // perdu # lost
    }

  } else {                      // comparer mot+reste (toto*..) # compare word + rest (foo * ..)
    if (strnotempty(chaine)) {
      int jmp = 0, ok = 1;

      // comparer début de joker et début de chaine # compare start of joker and start of chain
      while((joker[jmp] != '*') && (joker[jmp]) && (ok)) {
        // CI : remplacer streql par une comparaison != # CI: replace streql with a comparison! =
        if (!streql(chaine[jmp], joker[jmp])) {
          ok = 0;               // quitter
        }
        jmp++;
      }

      // comparaison ok?
      if (ok) {
        // continuer la comparaison.
        if (strjoker(chaine + jmp, joker + jmp, size, size_flag))
          return chaine;        // retourner 1e lettre
      }

    }                           // strlen(a)
    return NULL;
  }                             // * ou mot # * or word

  return NULL;
}

// recherche multiple
// exemple: find dans un texte de strcpybuff(*[A-Z,a-z],"*[0-9]"); va rechercher la première occurence
// d'un strcpy sur une variable ayant un nom en lettres et copiant une chaine de chiffres
// ATTENTION!! Eviter les jokers en début, où gare au temps machine!

// multiple search
// // example: find in a strcpybuff text (* [A-Z, a-z], "* [0-9]"); will look for the first occurrence
// // a strcpy on a variable with a name in letters and copying a string of digits
// // WARNING!! Avoid wildcards early, where station machine time!
const char *strjokerfind(const char *chaine, const char *joker) {
  const char *adr;

  while(*chaine) {
    if ((adr = strjoker(chaine, joker, NULL, NULL))) {  // ok trouvé
      return adr;
    }
    chaine++;
  }
  return NULL;
}
