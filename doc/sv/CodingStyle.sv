===========================================================================
 Freecivs Stilv�gledning
===========================================================================

Om man vill koda Freeciv och f� sina patchar antagna s� hj�lper det
att f�lja n�gra enkla stilregler. Ja, n�gra av dessa �r en aning
petiga, men krig utk�mpas �ver de f�nigaste saker...

- Freeciv �r skrivet i C (f�rutom BEOS-klienten). Detta inneb�r att
  C++-drag som "//"-kommentarer och variabeldeklarationer som inte �r
  i b�rjan av block �r f�rbjudna.

- Anv�nd K&R-indragsstil med indrag 2 (vid tveksamhet, anv�nd
  "indent -kr -i2 -l77"). Men man ska inte �ndra stilen p� kodstycken
  som man inte �ndrar p� annat s�tt eller skapar. H�r �r de viktigaste
  egenskaperna:
    - den st�rsta till�tna radl�ngden �r 77 tecken
    - blanksteg infogas f�re och efter operatorer ("int i, j, k;" i
      st�llet f�r "int i,j,k;" och "if (foo <= bar) c = a + b;" i
      st�llet f�r "if(foo<=bar) c=a+b;")
    - funktionsklammern skall st� i den f�rsta kolonnen:
        int foo()
        {
          return 0;
        }
      i st�llet f�r
        int foo() {
          return 0;
        }
    - tabavst�ndet skall vara 8

- En tom rad ska s�ttas mellan 2 �tskilda kodstycken.

================================
 Kommentarer
================================

- Varje funktion ska ha ett kommentarhuvud. Det ska vara ovanf�r
  funktionens implementering, inte prototypen:

/*************************************************************************
 funktionsbeskrivningen ska vara h�r
 all anv�ndbar information s�som vad som anropar funktionen med mera
 skapa _inte_ en ny funktion utan n�gon form av kommentar
*************************************************************************/

int the_function_starts_here(int value) 
{
  ...
}

- Enradskommentarer: Kommentaren ska vara r�tt indragen och st�
  ovanf�r koden som kommenteas:

  int x;

  /* Detta �r en enradskommentar */
  x = 3;

- Flerradskommentarer: Stj�rnor ska st� framf�r kommentarraden:

  /* Detta �r en
   * flerradskommentar 
   * bla bla bla */

- Kommentarer i deklarationer. Om man beh�ver kommentera en
  variabeldeklaration s� ska det g�ras s� h�r:

  struct foo {
    int bar;                    /* bar anv�nds f�r ....
                                 * att ..... ska */
    int blah;                   /* blah anv�nds f�r .... */
  };

- Kommentarer i villkorssatser: Om man beh�ver en kommentar f�r att
  visa programfl�det s� ska den st� nedanf�r if eller else:

  if(is_barbarian(pplayer)) {
    x++;
  } else {
    /* Om inte barbar ... */
    x--;
  }

- Kommentarer till �vers�ttare st�r f�re str�ngen som �r m�rkt med
  N_(), _() eller Q_() och har f�rstavelsen "TRANS:". Dessa
  kommentarer kopieras till �vers�ttningsfilerna. De ska anv�ndas
  n�rhelst man tycker att �vers�ttarna kan beh�va lite mer
  information:

    /* TRANS: �vers�tt inte "commandname". */
    printf(_("commandname <arg> [-o <optarg>]"));

================================
 Deklarera variabler
================================

- Variabler kan tilldelas ett v�rde vid initialiseringen:

int foo(struct unit *punit)
{
  int x = punit->x;
  int foo = x;
  char *blah;

  ...
}

- Efter variabeldeklarationerna ska det vara en tom rad f�re resten av
  funktionskroppen.

- Sammanfoga deklarationer: Variabler beh�ver inte dekrareras p� var
  sin rad. De ska dock endast sammanfogas om de har liknande
  anv�ndning.

int foo(struct city *pcity)
{
  int i, j, k;
  int total, cost;
  int build = pcity->shield_stock;
}

================================
 Klamrar
================================

- Ytterligare klamrar p� upprepningar: L�gg m�rke till att
  *_iterate_end; ska st� p� samma rad som slutklammern:

  unit_list_iterate(pcity->units_supported, punit) {
    kill(punit);
  } unit_list_iterate_end;

- I switchsatser skall klamrar bara s�ttas d�r de beh�vs, till exempel
  f�r att skydda variabler.

- Klammrar skall anv�ndas efter villkorssatser:

  if (x == 3) {
    return;
  }

 och 

  if (x == 3) {
    return 1;
  } else {
    return 0;
  }

 inte

  if (x == 3)
    return 1;  /* D�LIGT! */

================================
 Annat
================================

- Om ett tomt block beh�vs s� ska man s�tta en f�rklarnde kommentar i
  det:

  while(*i++) {
    /* nothing */
  }

- Ordna inkluderingsfiler konsekvent: Alla systeminkluderingsfiler ska
  st� f�rst, inom <> och i bokstavsordning. Sedan ska alla
  freecivinkluderingsfiler st� inom "" och ordnade efter katalog
  (common, server, ...) och sedan i bokstavsordning. Det ska vara en
  tom rad mellan styckena. Detta hj�lper till att undvika dubbla
  inkluderingar.

- Om man anv�nder systemberoende funktioner s� ska man inte l�gga till
  #ifdef __CRAY__ eller liknande. Man ska i st�llet skriva ett prov
  f�r funktionen i b�de configure.in och configure.ac och anv�nda ett
  meningsfullt makronamn i k�llkoden.

- Globala funktioner ska alltid ha prototyper i l�mpliga
  inkluderingsfiler. Lokala funktioner ska alltid deklareras som
  statiska. F�r att uppt�cka detta och n�gra andra problem skall man
  anv�nda f�ljande varningsargument f�r gcc; "-Wall -Wpointer-arith
  -Wcast-align -Wmissing-prototypes -Wmissing-declarations
  -Wstrict-prototypes -Wnested-externs".

- Om man skickar patchar s� ska man anv�nda "diff -u" (eller "diff -r
  -u" eller "cvs diff -u"). Se
  <http://www.freeciv.org/contribute.html> f�r ytterligare
  upplysningar. Man ska �ven ge patcharna beskrivande namn (till
  exempel "fix-foo-0.diff", men inte "freeciv.diff").

- N�r man k�r "diff" f�r en patch s� ska man se till att utesluta
  on�diga filer genom att anv�nda kommandoradsargumentet "-X" f�r
  programmet "diff":

    % diff -ruN -Xdiff_ignore freeciv_c freeciv > /tmp/fix-foo-0.diff

  En f�reslagen "diff_ignore"-fil f�ljer med Freeciv.

===========================================================================
