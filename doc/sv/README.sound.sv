===========================================================================
 Ljudst�d
===========================================================================

Servern skickar en lista �ver f�rstahands- och andrahandsljudm�rken
till klienten f�r vissa h�ndelser. F�rstahandslm�rkena �r de som
�ndringsupps�ttningen i fr�ga f�redrar. Klienten beh�ver inte ha dessa
ljud. Andrahandsm�rkena ska h�nvisa till standardljud som alla
installationer av Freeciv ska ha.

M�rkena �nv�nds f�r att ge ett enkelt s�tt att byta ljud. En
beskrivningsfil �nv�nds f�r att ange ljudfiler till m�rkena. Ett byte
av beskrivningsfil �ndrar s�ledes ljuden. Det g�rs med
kommandoradsargument enligt f�ljande:

  civclient --Sound minaljud.spec

D� �nv�nds de ljudfiler som anges i filen "minaljud.spec". Man m�ste
se till att ljudfilerna finns d�r det st�r att de ska finnas genom att
kopiera eller l�nka dem dit, eller genom att �ndra i
beskrivningsfilen. Alla s�kv�gar �r som standard relativa till
katalogen "data/". Ljudupps�ttningar kan laddas ner i tar-format fr�n
Freecivs webserver. Man packar upp dem med kommandot
"tar zxvf stdsoundsX.tar.gz" (eller med WinZip f�r Windows) och l�gger
filerna i ovann�mnda datakatalog.

CVS-versionen inneh�ller varken ljudfiler eller beskrivningsfil. Man
kan h�mta ljudupps�ttningar (ljudfiler med beskrivningsfil) fr�n
<ftp://ftp.freeciv.org/freeciv/contrib/sounds/>. P� denna adress kan
man �ven hitta extra ljudfiler f�r att �ndra en befintlig
ljudupps�ttning eller skapa en ny.

================================
 Insticksprogram
================================

Ljudutmatningen i klienten g�rs av insticksprogram. Vilka
insticksprogram som �r tillg�ngliga avg�rs av de bibliotek som hittas
p� v�rdsystemet. Man kan v�lja vilket insticksprogram som ska anv�ndas
med kommandoradsargument:

  civclient --Plugin sdl

Man kan v�lja "none" f�r att tysta klienten. Freeciv st�djer f�r
n�rvarande f�ljande insticksprogram:
  - dummy (none)
  - Esound (esd)
  - SDL with SDL_mixer library (sdl)

F�r att l�gga till st�d f�r ett nytt insticksprogram �ndrar man dessa
filer (d�r "n�got" �r namnet p� det nya insticksprogrammet):
	configure.in			/* l�gg till nytt prov */
	acconfig.h			/* l�gg till ny konfigurationsmetavariabel */
	client/audio.c			/* l�nka in nytt insticksprogram */
	client/Makefile.am		/* l�gg till filerna nedan */
	client/audio_whatever.c		/* insticksprogram */
	client/audio_whatever.h		/* insticksprograms huvud */

================================
 M�rken
================================

Det fins 2 slags ljudm�rken:
 - angivna i regelupps�ttningarna
 - angivna i programkoden

Emedan det f�rsta kan v�ljas fritt, kan det andra inte �ndras.

Ljudm�rkena som h�r till v�rldsunder och stadsf�rb�ttringar,
enhetsf�rflyttningar och strid m�ste anges i regelupps�ttningarna.
Freeciv skickar �ver dessa ljudm�rken till klienten, d�r de �vers�tts
till filnamn med hj�lp av beskrivningsfilen. Varje beskrivningsfil ska
ha allm�nna ljudm�rken f�r v�rldsunder ("w_generic"),
stadsf�rb�ttringar ("b_generic"), enhetsf�rflyttningar ("m_generic")
och strid ("f_generic").

Ljudm�rken som h�r till vissa h�ndelser kommer fr�n freecivkoden och
kan inte �ndras utifr�n. Beskrivningsfilen tillordnar �ven ljudfiler
f�r dessa m�rken. M�rkenas namn �r uppr�kningsnamn (se
common/events.h) best�ende av gemena bokst�ver. S�ledes blir
"E_POLLUTION" m�rket "e_pollution". Det finns ingen allm�nt m�rke och
ej heller n�gra andrahandsm�rken.

Det finns f�r n�rvarande endast ett musikstycke; inledningsstycket.
Det spelas tills spelet b�rjar. M�rket f�r detta musikstycke �r
"music_start".

================================
 ATT G�RA
================================

Det finns n�gra saker som kan g�ras f�r att f�rb�ttra Freecivs
ljudst�d:
  * l�gg till fler insticksprogram (gstreamer, arts, windows, ...)
  * l�gg till ett ljudm�rke f�r varje forskningsframsteg
  * l�gg till st�d f�r ogg-filer
  * l�gg till fler h�ndelsem�rken
  * hitta eller skapa b�ttre ljud och f�rb�ttra beskrivningsfilen

================================
 �vrigt
================================

Ljudskapare; v�nligen namnge ljudfilerna p� ett intelligent s�tt.
Bifoga en informationsfil med namnet README. Den m�ste inneh�lla
licensvillkoren. Om ljuden �r fria fr�n upphovsr�ttsanspr�k ska det
framg�.

Skapare av �ndringsupps�ttnigar; v�nligen g�r andrahandsm�rken som
h�nvisar till standardm�rken s� att de som inte har laddat ner den
senaste och b�sta ljudupps�ttningen �nd� kan njuta av spelet.
