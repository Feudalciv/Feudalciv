Klient/servermodell
===================

Varje klientspelare har ett attributblock och servern tillhandah�ller
�ven ett s�dant attributblock f�r varje spelare. Alla attributblock
som servern tillhandah�ller skrivs till sparningsfilen. Klienten och
servern samordnar sina block. Servern skickar sitt block till klienten
vid spelets b�rjan eller �terladdning. Klienten skickar ett uppdaterat
block till servern vid slutet av varje omg�ng. Eftersom den st�rsta
paketstorleken �r 4k och attributblock kan ha godtycklig storlek
(dock begr�nsat till 64k i denna inledande version) s� kan inte
attributblocken skickas i ett enda paket. S� attributblocken delas upp
i attributsjok som �ter sammanfogas hos mottagaren. Ingen del av
servern k�nner till n�gon inre ordning hos attributblocket. F�r
servern �r ett attributblock endast ett block av betydelsel�sa data.

Anv�ndargr�nssnitt
==================

Eftersom ett attributblock inte �r ett bra anv�ndargr�nssnitt s� kan
anv�ndaren komma �t attributen genom
kartl�ggnings-/ordliste-/haskarte-/hashtabellsgr�nssnitt.
Denna hashtabell serialiseras till attributblocket och omv�nt.
Hashtabellens nyckel best�r av: (den verkliga) nyckeln, x, y och id.
(Den verkliga) nyckeln �r ett heltal som definierar anv�ndningen och
formatet f�r detta attribut. Hashtabellens v�rden kan ha godtycklig
l�ngd. Den inere ordningen hos ett v�rde �r ok�nt f�r
attributhanteringen.

F�r enklare �tkomst finns det omslagsfunktioner f�r de vanliga typerna
enhet, stad, spelare och ruta. S� det finns enkla s�tt att koppla
godtyckliga data till en enhet, stad, spelare (sj�lv eller annan)
eller en ruta.
