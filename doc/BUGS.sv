==========
PROGRAMFEL
==========

Freeciv 1.11.4 �r en "driftss�ker" utg�va och anses var tillr�ckligt
fri fr�n programfel f�r dagligt bruk. Om man �nd� hittar ett
programfel skulle vi vilja f� reda p� det s� att vi kan r�tta
det. Denna fil listar k�nda programfel i denna utg�va och ger
information om att anm�la programfel.

Listan inneh�ller endast de tydligaste programfelen. F�r en
fullst�ndig lista, se:

    http://www.freeciv.org/cgi-bin/bugs

K�NDA PROGRAMFEL:
=================

 - Gtk+-klienten har ibland ett problem med at fokuset fastnar p�
   tjattraden. D� fungerar pilknapparna inte l�ngre f�r att flytta
   enheter i huvudanblicken. Ett s�tt att komma tillbaka till
   huvudanblicken �r att anv�nda <Tab>-knappen f�r att flytta fram
   fokuset till n�gon annan sk�rmbest�ndsdel. D� fungerar pilknapparna
   igen. (Att klicka i huvudanblicken �r ocks� ett s�tt att upph�va
   problemet.)

 - De enkla datorstyrda fienderna �r inte tillr�ckligt enkla f�r
   nyb�rjare. Om de datorstyrda spelarna besegrar dig tidigt i spelet,
   prova att s�tta sververvalm�jligheten "generator" till 2 eller 3,
   det vill s�ga skriv "set generator 2" eller "set generator 3" innan
   spelet har satts ig�ng.

 - De sv�ra datorstyrda fienderna �r inte tillr�ckligt sv�ra f�r
   erfarna spelare. De g�r fortfarande dumma saker, till exempel s�
   f�redrar de att l�mna st�der i upplopp i st�llet f�r att sv�lta ner
   dem.

 - Ibland �r det f�r m�nga framsteg i "m�l"-menyn i
   forskningsredog�relsen s� att menyn str�cker sig utanf�r sk�rmens
   underkant s� att man inte kan v�lja alla framsteg. 

 - Servern visar inte hela namnet p� v�rdar med mycket l�nga namn.

 - V�rldsundret "Stora Muren", vilket verkar som en stadsmur i varje
   stad, f�rhindrar byggandet av riktiga stadsmurar som inte
   f�rsvinner n�r v�rldsundret blir f�r�ldrat.

ANM�LA PROGRAMFEL
=================

(Om det �r ett fel i en �vers�ttning ska det anm�las till �vers�ttaren
f�r spr�ket i fr�ga. Se <http://www.freeciv.org/l10n.phtml> f�r namn
p� och epostadresser till �vers�ttarna.)

S� h�r g�r man:

- Ser efter att det inte �r n�got av programfelen i listan ovan! :-)

- Tittar p� <http://www.freeciv.org> och f�rs�krar sig om att man har
  den nyaste versionen. (Vi kanske redan har r�ttat felet.)

  Man kanske vill prova en utvecklarversion fr�n CVS-upplaget. De kan
  h�mtas fr�n:

        http://www.freeciv.org/latest.html

- Tittar p� Freecivs FAQ p� <http://www.freeciv.org/faq> f�r att se om
  n�got s�tt att upph�va problemet har offentliggjorts.

- Tittar p� Freecivs system f�r sp�rning av programfel vid:

        http://www.freeciv.org/cgi-bin/bugs

  f�r att se om programfelet redan har anm�lts.

- Anm�l programfelet!

   Man kan �nm�la ett programfel genom att skicka epost till
    <bugs@freeciv.freeciv.org> eller p� v�ven vid
    <http://www.freeciv.org/cgi-bin/bugs>.

   Om man vill skicka f�rslag till freecivutvecklarna utan att g�ra en
   programfelsanm�lan kan man skicka epost till
   <freeciv-dev@freeciv.org>, freecivutvecklarnas s�ndlista.

   Vad man ska n�mna i sin programfelsanm�lan:

   - Beskrivning av problemet och i f�rekommande fall det felmeddeland
     man f�r.

   - Vilken klient man anv�nder (Gtk+ eller Xaw).

   - Namn och versionsnummer:

       - Det operativsystem som man anv�nder. Kommandot "uname -a" kan
         vara anv�ndbart.

       - Versionsnumret f�r Freeciv.

       - Om man anv�nder Gtk+-klienten, versionsnumren (om man k�nner
         till dem) f�r sina Gtk+-, glib- och imlibbibliotek.

       - Om man anv�nder Xaw-klienten, versionsnumren (om man k�nner
         till dem) f�r X-biblioteken, Xpm-biblioteket och
         Xaw-biblioteket och i synnerhet om det �r en variant s�som
         Xaw3d, Xaw95 eller Nextaw.

       - Om man kompilerar fr�n k�llkod, namnet och versionsnumret f�r
         kompilatorn.

       - Om man installerar fr�n ett f�rdigkompilerat paket, dess
         namn, vilken distribution det �r f�r och varifr�n man h�mtat
         det.

   - Om Freeciv "dumpar core", kan vi efterfr�ga en "stacksp�rning",
     vilken ges vid anv�ndning av en avlusare. F�r detta beh�vs
     "core"-filen, s� var god beh�ll den ett tag.

YTTERLIGARE INFORMATION:
========================

F�r mer information se Freecivs plats p� v�rldsv�ven:

        http://www.freeciv.org/
