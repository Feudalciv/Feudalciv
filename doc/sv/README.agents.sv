�versikt
========

Ett ombud �r ett kodstycke som ansvarar f�r ett visst omr�de. Ett
ombud f�r ett uppdrag av sin anv�ndare, samt en upps�ttning objekt som
ombudet kan styra over (till exempel tillverkningen i en stad, en stad
i helhet, en enhet, en upps�ttning enheter eller alla rikets enheter).
Anv�ndare kan vara en m�nskig spelare eller en annan del av
programkoden, till exempel ett annat ombud. Det sker inget ytterligare
samspel mellan anv�ndare och ombud efter att ombudet f�tt sin
uppdragsbeskrivning.

Exempel p� ombud:
 - ett ombud som ansvarar f�r att f�rflytta en enhet fr�n A till B
 - ett ombud som ansvarar f�r att ge st�rsta m�jliga mattillverkning i
   en stad
 - ett ombud som ansvarar f�r tillverkningen i en stad
 - ett ombud som ansvarar f�r f�rsvaret av en stad
 - ett ombud som ansvarar f�r en stad
 - ett ombud som ansvarar f�r alla st�der

Ett ombud kan anv�nda andra ombud f�r att uppn� sina m�l. S�dana
beroenden bildar en rangordning av ombud. Ombud har en rang i denna
rangordning. Ombud med h�gre rang �r mer invecklade. Ett ombud av rang
n kan endast anv�nda ombud av rang (n - 1) eller l�gre. Rang 0
inneb�r handlingar som utf�rs av servern och �r odelbara (inte kan
simuleras av klienten).

Genom en s�dan definition beh�ver ett ombut inte vara implementerad i C
och beh�ver inte heller anv�nda client/agents/agents.[ch].

K�rnan i ett ombud utg�rs av 2 delar: en del som fattar beslut och en
del som verkst�ller beslut. Den f�rsta delens beslut ska g�ras
tillg�ngliga. Ett ombud som saknar den verkst�llande delen kallas
r�dgivare.

Ett ombud ska f�rutom k�rnan tillhandah�lla ett grafiskt gr�nssnitt.

Implementering
==============

Den mottagna uppdragsbeskrivningen och alla beslut som har fattats kan
sparas i attribut. Ett ombud ska inte g�ra n�gra antaganden. Detta
inneb�r: INGA MAGISKA TAL. Allt ska vara inst�llbart av anv�ndaren.

Anv�nd clients/agents/agents.[ch] f�r att informeras om vissa
h�ndelser. Tveka inte att l�gga till fler �terrop. Anv�nd
client/agents/agents:wait_for_requests i st�llet f�r
client/civclient:wait_till_request_got_processed.
