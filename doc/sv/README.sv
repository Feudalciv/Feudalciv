====================
Freeciv Version 1.12
====================

V�lkommen till Freeciv!

Detta arkiv inneh�ller Freeciv, en fri Civilizationklon, huvudsakligen
f�r X under Unix. Den har st�d f�r flerspelarspel lokalt eller �ver
n�tverk, samt utmanande datorstyrda spelare.

Freeciv siktar p� att ha regler som huvudsakligen st�mmer �verens med
Civilisation II [tm] utgivet av Sid Meier och Microprose [tm]. Vissa
regler �r annorlunda f�r att vi tycker att det �r b�ttre s�. Det finns
m�nga inst�llbara parametrar f�r att anpassa sina spel.

Freeciv har skapats helt oberoende av Civilization; man beh�ver inte
�ga Civilization f�r att spela Freeciv.

Fast�n vi �nnu inte har st�d f�r ljud och datorspelarna inte kan
f�rhandla �r reglerna i det n�rmaste fullst�ndiga och v�rt
flerspelarst�d �r utm�rkt.

Detta �r den svenska �vers�ttningen av filen "../README". Eftersom
denna �vers�ttning har f�rre l�sare �n originalet �r det st�rre risk
att den inneh�ller felaktigheter. Det kan finnas ouppt�ckta
fel�vers�ttningar, rester av gammal information som tagits bort i
originalet men ej i �vers�ttningen, samt till�gg i originalfilen
som �nnu inte kommit med i �vers�ttningen. J�mf�r d�rf�r med
originalet om tveksamhet uppst�r. Vid felaktigheter, kontakta
�vers�ttaren: Erik Sigra <sigra@home.se>.


N�tplats:
=========

Freecivs n�tplats �r:

  http://www.freeciv.org/

D�r kan man h�mta de senaste freecivnyheterna, -utg�vorna och
-uppdateringarna, hitta information om Freecivs s�ndlistor samt se
metaservern som visar information om spel som spelas runt om i
v�rlden.

Licens:
=======

Freeciv ges ut under GNU General Public License. Det betyder i korthet
att man f�r kopiera detta program (�ven k�llkoden) fritt, men se filen
"../COPYING" f�r fullst�ndiga villkor.

Kompilera och installera:
=========================

Var god l�s filen INSTALL.sv noga f�r anvisningar kompilering och
installering av Freeciv.


P�b�rja ett nytt spel:
======================

Freeciv �r 2 program, en server och en klient. N�r ett spel �r i g�ng
k�rs ett serverprogram och s� m�nga klientprogram som det finns
m�nskliga spelare. Serverprogrammet beh�ver inte X, men det g�r
klientprogrammen.

  ANM�RKNING:
  F�ljande exempel antar att Freeciv har installerats p� systemet och
  att katalogen som inneh�ller programmen "civclient" och "civserver"
  finns i variabeln PATH. Om Freeciv inte �r installerat kan man
  anv�nda programmen "civ" och "ser" som finns i freecivkatalogen. De
  anv�nds p� samma s�tt som "civclient" och "civserver".

F�r att kunna spela Freeciv beh�ver man starta servern, klienterna och
datorspelarna, samt ge servern startkommandot. H�r �r stegen:

Server:

  F�r att starta servern:

  |  % civserver

  Eller f�r en lista �ver kommandoradsargument:

  |  % civserver --help

  N�r servern har startats visas en prompt:

  |  F�r inledande hj�lp, skriv 'help'.
  |  >

  och man kan se denna information genom att anv�nda hj�lpkommandot:

  | > help
  |  V�lkommen - detta �r den inledande hj�lptexten f�r
  |  freecivservern.
  |
  |  2 Viktiga serverbegrepp �r kommandon och valm�jligheter.
  |  Kommandon, s�som "help", anv�nds f�r att v�xelverka med servern.
  |  Vissa kommandon tar ett eller flera argument, �tskilda av
  |  blanksteg.  I m�nga fall kan kommandon och kommandoargument
  |  f�rkortas. Valm�jligheter �r inst�llningar som styr servern medan
  |  den �r i g�ng. 
  |
  |  F�r att ta reda p� hur man f�r mer information om kommandon och
  |  valm�jligheter, anv�nd "help help".
  |
  |  F�r den ot�liga �r kommandona f�r att komma i g�ng:
  |    show   -  se nuvarande valm�jligheter
  |    set    -  s�tt valm�jligheter
  |    start  -  s�tt i g�ng spelet n�r spelare har anslutit sig
  |    save   -  spara nuvarande spel
  |    quit   -  avsluta
  |  >

  Man kan anv�nda kommandot "set" f�r att �ndra n�gon av
  servervalm�jligheterna. Man kan f� en lista med alla
  servervalm�jligheter med kommandot "show" och utf�rliga
  beskrivningar av varje servervalm�jlighet med kommandot "help
  <servervalm�jlighetsnamn>".

  Till exempel:

  |  > help xsize
  |  Valm�jlighet: xsize  -  Kartbredd i rutor
  |  Status: �ndringsbar
  |  V�rde: 80, Minsta m�jliga: 40, Standard: 80, H�gsta m�jliga: 200

  Och:

  |  > set xsize 100
  |  > set ysize 80

  Detta g�r kartan dubbelt s� stor som standardstorleken 80x50.

Klient:

  Nu ska alla m�nskliga spelare ansluta genom att k�ra
  freecivklienten:

  |  % civclient

  Detta antar att servern k�r p� samma maskin. Om inte kan man
  antingen ange det p� kommandoraden med parametern "--server" eller
  skriva in det i den f�rsta dialogrutan som visas i klientprogrammet.

  Antag till exempel att servern k�rs p� en annan maskin kallad
  "baldur". D� ansluter spelare med kommandot:

  |  % civclient --server baldur

  Om man �r den enda m�nskliga spelaren beh�ver endast en klient
  anv�ndas. P� vanligt Unixvis kan man k�ra klienten i bakgrunden
  genom att l�gga till en ampersand:

  |  % civclient &

  En annan valm�jlighet �r "--tiles" som anv�nds f�r att k�ra klienten med en
  annan upps�ttning rutbilder f�r landskap, enheter med mera. Utg�van
  inneh�ller flera olika upps�ttningar rutbilder. Denna utg�va
  inneh�ller 3 upps�ttningar rutbilder:
  - hires: En isometrisk rutbildsupps�ttning som liknar den i
           Civilization 2. (Fungerar inte i XAW-klienten.)
  - trident: En Civilization-1-liknande rutbildsupps�ttning
             rutstorleken 30x30 bildpunkter.
  - engels: En Civilization-1-liknande rutbildsupps�ttning
            rutstorleken 30x30 bildpunkter.
  Det finns varianter av trident och engels som heter trident_shields
  och engels_shields.
  I denna utg�va �r hires f�rval i GTK- och Amikaklienterna och
  trident �r f�rval i XAW-klienten. (I vissa tidigare utg�vor var
  engels f�rval.)
  Rutbildsupps�ttningarna har ganska olika stilar; man kan ha
  personliga uppfattningar om dem. "_shields"-varianterna anv�nder
  sk�ldformade flaggor, vilket �r mindre och diskretare. Trident har
  mindre rutor vilket kan vara bra om man har liten sk�rm. Prova dem
  alla och best�m dig vilken du vill anv�nda.

  K�r klienten med f�ljande kommando f�r att anv�nda trident:

  |  % civclient --tiles trident

  Andra upps�ttningar kan finnas p� Freecivs FTP-server.


  Klienter kan ges till�telse att utf�ra serverkommandon. Skriv
  f�ljande vid serverprompten f�r att endast ge dem till�telse att
  endast ge informationskommandon:

  |  > cmdlevel info

  Klienter kan nu anv�nda "/help", "/list", "/show settlers" med mera.

Datorstyrda spelare:

  Det finns 2 s�tt att skapa datorstyrda spelare. Det f�rst �r att
  ange antalet spelare med servervalm�jligheten "aifill":

  |  > set aifill 7

  Efter att ha anv�nt serverkommandot "start" f�r att s�tta i g�ng
  spelet, kommer de spelare som inte �r m�nskliga att bli datorstyrda.
  I exempelt ovan skulle 5 datorstyrda spelare ha skapats om det hade
  funnits 2 m�nskliga spelare.

  Det andra s�ttet �r att skapa en datorspelare med serverkommandot
  "create":

  |  > create Widukind

  Detta skapar den datorstyrda spelaren Widukind.

  Datorstyrda spelare tilldelas folkstammar efter att alla m�nskliga
  spelare har valt folkstammar, men man kan v�lja en s�rskild folkstam
  f�r en datorstyrd spelare genom att anv�nda ett namn som �r namnet
  p� en ledare f�r den folkstammen. Man kan till exempel spela mot
  ryssarna med f�ljande kommando:

  |  > create Stalin

  Om ingen av de m�nskliga spelarna v�ljer att spela med ryssarna
  kommer denna datorstyrda spelare att g�ra det.

Server:

  N�r alla har anslutit (anv�nd kommandot "list" f�r att se vilka som
  �r anslutna), s�tt i g�ng spelet med kommandot "start":

  |  > start

  Sedan �r spelet i g�ng!


Tillk�nnage spelet:
===================

Om man vill ha andra motst�ndare �n lokala v�nner och datorstyrda
spelare kan man bes�ka Freecivs metaserver:

  http://meta.freeciv.org/metaserver/

Det �r en lista �ver freecivservrar. F�r att f� sin server att anm�la
sig d�r k�r man civserver med kommandoradsargumentet "--meta" eller
"-m".

Varningar:

 1) P� grund av nya funktioner �r olika versioner av server och klient
    ofta of�renliga. Inte alla of�renligheter kan uppt�ckas och
    meddelas p� ett l�mpligt s�tt. Ett exempel �r en klient av version
    1.8.0 (eller senare) som ansluter till en server av version 1.7.2:
    klienten verkar ansluta men f�r aldrig n�gra meddelanden fr�n
    servern.

 2) Om metaserverknappen i anslutningsdialogen inte fungerar, unders�k
    om internetanslutningen kr�ver en WWW-proxy, och se till att
    Freeciv anv�nder den genom att st�lla in variabeln $http_proxy. Om
    proxyn till exempel �r proxy.minanslutning.se port 8888, s�tt
    $http_proxy till http://proxy.minanslutning.se:8888/ innan
    klienten startas.

 3) Ibland finns det inga spel p� metaservern. Detta kan delvis bero
    p� att metaservern har bytt v�rdar och att tidigare
    freecivversioner inte k�nner till den nya adressen. Det kan �ven
    bero p� att spel inte l�ngre anm�ls till metaservern som standard,
    utan att man m�ste anv�nda kommandoradsargumentet "--meta". Om man
    vill spela ett spel p� metaservern och det inte finns n�gra d�r
    kan man skapa ett.


Under spelets g�ng:
===================

Spelet kan sparas med serverkommandot "save":

  |  > save mittspel.sav

(Om servern �r kompilerad med packningsst�d och servervalm�jligheten
"compress" �r satt till n�gnting annat �n 0 packas filen och kallas
mittspel.sav.gz.)

Freecivklienten fungerar i stort sett s� som man kan f�rv�nta sig av
ett civilizationspel med flerspelarst�d. De m�nskliga spelarna g�r
sina drag samtidigt. De datorstyrda spelarna g�r sina drag n�r de
m�nskliga spelarna har avslutat sina omg�ngar. Det finns en tidsgr�ns
som �r satt till 0 sekunder (ingen tidsgr�ns) som standard. Detta
v�rde kan �ndras med serverkommandot "set".

Titta p� hj�lpen i klientprogrammet. Alla 3 musknapparna anv�nds och
�r dokumenterade i hj�lpen.

Spelare kan trycka p� returnknappen eller klicka p� "Avsluta
omg�ng"-knappen f�r att avsluta sin omg�ng.

Anv�nd spelardialogen f�r att se vilka som har avslutat sin omg�ng och
vilka man v�ntar p�.

Anv�nd inmatningsraden vid f�nstrets underkant f�r att skicka
meddelanden till andra spelare.

Man kan skicka ett meddelande till en enskild spelare (till exempel
"einar"):

  |  einar: flytta p� pansarvagnen NU!

Servern kan gissa sig till namn om man skriver dem ofullst�ndigt. Om
man till exempel skriver "ein:" hittar den spelaren med namn som
st�mmer delvis med namnet man skrev.

P� nyare servrar (version 1.8.1 eller vissa utvecklingsversioner av
1.8.0) eller nyare kan man ge serverkommandon p� klientens
inmatningsrad:

  |  /list
  |  /set settlers 4
  |  /save mittspel.sav

Serverhandhavaren till�ter kanske bara informationskommandon eftersom
det �r en s�kerhetsrisk att l�ta spelare anv�nda alla serverkommandon,
till exempel:

  |  /save /etc/passwd

Naturligtvis ska freecivservern inte k�ras med fullst�ndiga
r�ttigheter p� grund av denna risk.

Om man just har b�rjat spela Freeicv och vill ha hj�lp med strategin
kan man titta i filen "HOWTOPLAY.sv".

Se freecivhandboken p� f�ljande adress f�r mycket mer information om
klienten, servern och spelet:

  http://www.freeciv.org/manual/manual.html


Avsluta spelet:
===============

Det finns 3 s�tt att avsluta spelet:

1) Vara den enda �terst�ende spelaren.
2) N� slut�ret.
3) Bygga ett rymdskepp och s�nda i v�g det s� att det n�r Alfa
   Kentauri.

En utv�rderingstabell visas i samtliga fall. Anm�rkning:
Serverhandhavaren kan s�tta slut�ret n�r spelet �r i g�ng genom att
�ndra servervalm�jligheten "end-year". Detta �r anv�ndbart n�r det �r
uppenbart vem som kommer att segra men man inte vill spela sig igenom
uppst�dningen.


�ppna spel:
===========

Man kan �ppna ett sparat spel genom att k�ra servern med
kommandoradsargumentet "-f":

  |  % civserver -f mittspel2001.sav

eller om filen �r pacakd:

  |  % civserver -f mittspel2001.sav.gz

Sedan kan spelarna �teransluta:

  |  % civclient -n Bismarck

L�gg m�rke till att spelarnamnet anges med kommandoradsargumentet
"-n". Det �r viktigt att spelaren anv�nder sama namn som den anv�nde
f�rrut, annars sl�pps de inte in.

Spelet kan s�ttas i g�ng igen med serverkommandot "start".


Lokalt spr�kst�d:
=================

Freeciv st�der flera lokala spr�k.

Man kan v�lja vilket lokalt spr�k man vill anv�nda genom att ange en
"locale". Varje locale har ett standardnamn (till exempel "de" f�r
tyska). Om man har installerat Freeciv kan man v�lja locale genom att
s�tta variablen LANG till denna locales standardnamn innan man k�r
civserver och civclient. F�r att till exempel k�ra Freeciv p� tyska
g�r man s� h�r:

  export LANG; LANG=de    (i Bourneskalet (sh))

eller

  setenv LANG de          (i C-skalet (csh))

(Man kan g�ra detta i sin "~/.profile" eller "~/.login".)

Loggmeddelanden:
================

B�de klienten och servern skriver loggmeddelanden. Dessa �r av 5 olika
slag, d�dliga, fel-, normala, m�ngordiga och avlusningsmeddelanden.

Som standard skrivs d�dliga, fel- och normala meddelanden till
standard output. Man man skicka loggmeddelanden till en fil i st�llet
med kommandoradsargumentet "--log <filnamn>" eller "-l filnamn".

Man kan �ndra loggl�get med kommandoradsargumentet "--debug <l�ge>"
eller "-d <l�ge>" (eller "-de <l�ge>" f�r Xawklienten eftersom "-d" �r
flertydigt mellan "-debug" och "-display"), d�r <l�ge> �r 0, 1, 2
eller 3. 0 betyder att endast d�dligameddelanden visas, 1 betyder att
d�dliga och felmeddelanden visas, 2 betyder att d�dliga, fel- och
normala meddelanden visas (standard). 3 betyder att d�dliga, fel-,
normala och m�ngordiga meddellanden visas.

Om man kompilerar med DEBUG definierad (ett enkelt s�tt att g�ra detta
�r att konfigurera med "--enable-debug") , kan man f�
avlusningsmeddelanden genom att s�tta loggl�get till 4. Det �r
dessutom m�jligt att styra avlusningsmeddelanden (men inte andra
meddelanden) med avseende p� fil. Anv�nd d� "--debug 4:str1:str2" (s�
m�nga str�ngar man vill) och alla filnamn som �verensst�mmer med dessa
str�ngar som understr�ng har avlusningsloggning p�slaget. Alla andra
avlusningsmeddelanden st�ngs av. Anv�nd "--debug 4:str1,undre,�vre"
f�r att styra rader. Endast meddelanden mellan undre raden och �vre
raden kommer att visas. Endast 1 upps�ttning gr�nser kan anges f�r en
fil.

Exempel:

  |  % civserver -l mitt.log -d 3

Detta skickar alla loggmeddelanden, innefattande m�ngordiga
meddelanden, fr�n servern till filen "mitt.log".

Exempel:

  |  % civclient --debug 0

Detta d�ljer alla loggmeddelanden utom d�dliga meddelanden.

Exempel:

  | % civserver -d 4:log:civserver,120,500:autoattack

Detta visar alla d�dliga, fel-, normala och m�ngordiga meddelanden f�r
servern, samt avlusningsmeddelanden f�r vissa angivna delar. L�gg
m�rke till att "log" st�mmer �verens med b�de "gamelog.c" och "log.c".
F�r "civserver.c" visas endast avlusningsmeddelanden mellan raderna
120 och 500. Detta exempel fungerar endast om servern har kompilerats
med DEBUG.


Programfel:
===========

Vi vill g�rna bli underr�ttade om programfel s� att vi kan �tg�rda dem. Se
filen BUGS.sv f�r en lista �ver k�nda programfel i denna utg�va samt
information om att anm�la programfel.


S�ndlistor:
===========

Vi har 6 s�ndlistor:

  freeciv          Allm�nt samtal.
  freeciv-announce Kung�relser av allm�nt intresse.
                   Denna lista kan endast l�sas och sprider mycket f�
                   meddelanden. Man kan allts� inte skicka brev till
                   listan utan bara ta emot. Kung�relser som skickas
                   hit skickas �ven till s�ndlistan freeciv.
  freeciv-dev      Freecivutveckling.
  freeciv-data     Utveckling av inneh�ll i "data"-katalogen
                   (regelupps�ttningar, rutbildsupps�ttningar med
                   mera).
  freeciv-java     Utveckling av en javaklient (Java �r ett annat
                   programmeringsspr�k �n det som Freeciv �r skrivet
                   i).
  freeciv-cvs      Kung�relser om �ndringar i CVS-f�rvaret.
                   Denna lista kan endast l�sas och sprider
                   automatiska meddelanden. Man kan allts� inte skicka
                   brev till listan utan endast ta emot.

Alla listor �r �ppna f�r allm�nheten och alla �r v�lkomna att anm�la sig.

Man f�ljer dessa anvisningar f�r att anm�la sig eller avanm�la sig:

  1. E-posta till <listar@freeciv.org>.
  2. L�mna �mnesraden tom.
  3. Skriv n�gra av f�ljande rader i brevet:
      F�r att anm�la:
        subscribe freeciv
        subscribe freeciv-announce
        subscribe freeciv-dev
        subscribe freeciv-data
        subscribe freeciv-java
        subscribe freeciv-cvs
      F�r att avanm�la:
        unsubscribe freeciv
        unsubscribe freeciv-announce
        unsubscribe freeciv-dev
        unsubscribe freeciv-data
        unsubscribe freeciv-java
        unsubscribe freeciv-cvs

Anv�nd f�ljande adresser f�r att skicka brev till s�ndlistorna:
  <freeciv@freeciv.org>.
  <freeciv-dev@freeciv.org>.
  <freeciv-data@freeciv.org>.
  <freeciv-java@freeciv.org>.


Nya utg�vor:
============

Se emellan�t efter p� Freecivs n�tplats om det finns en ny utg�va.


Slutligen:
==========

Ha det kul och lycka till!

                                   --  Freecivlaget
