----------------------------------------------------------------------
Freecivs bilder och bildbeskrivningsfiler
----------------------------------------------------------------------

Att anv�nda bilder:
-------------------

F�r att anv�nda andra bilder �n standardbilderna med Freeciv ger man
"--tiles" som kommandoradsargument till freecivklienten. F�r att till
exempel and�nva "Engels"-bilderna k�r man ageten med:

  civclient --tiles engels

Vad Freeciv g�r d� �r att leta efter en fil med namnet
"engels.tilespec" n�gonstans i Freecivs datas�kv�g. (Se filen INSTALL
f�r information om Freecivs datas�kv�g.) Denna beskrivningsfil
inneh�ller information om vilka bildfiler som ska anv�ndas och vad de
inneh�ller.

Det �r allt som man beh�ver veta f�r att anv�nda andra
bildupps�ttningar som f�ljer med Freeciv eller sprids f�r sig av
tredje part. Resten av denna fil beskriver (dock inte fullst�ndigt)
inneh�llet i beskrivningsfilen och andra filer i sammanhanget. Detta
�r t�nkt som utvecklarhandledning och f�r folk som vill
skapa/sammanst�lla alternativa bildupps�ttningar och
�ndringsupps�ttningar f�r Freeciv.

----------------------------------------------------------------------
�versikt:
---------

Syftet med beskrivningsfilen "tilespec" och andra beskrivningsfiler i
sammanhanget �r att beskriva hur de olika bilderna �r lagrade i
bildfilerna s� att denna information inte beh�ver vara h�rdkodad i
Freeciv. D�rf�r �r det enkelt att erbjuda ytterligare bilder som
till�gg.

Det �r tv� lager i beskrivningsfilerna:

Filen med det �vre lagret har till exempel namnet "trident.tilespec".
Filens grundnamn, i detta fall "trident", motsvarar det
kommandoradsargument som skrivs efter "--tiles" p� freecivklientens
kommandorad, s�som beskrivet ovan.

Den filen inneh�ller allm�n information om hela bildupps�ttningen och
en lista �ver filer som ger information om de ensilda bildfilerna.
Dessa filer m�ste finnas n�gonstans i datas�kv�gen, men inte
n�dv�ndigtvis p� samma st�lle som det �vre lagrets beskrivningsfil.
L�gg m�rke till att de h�nvisade filernas antal och inneh�ll �r helt
anpasningsbart.

Ett undantag �r att inledningsbilderna m�ste vara i enskilda filer,
s�som det beskrivs i beskrivningsfilen, ty Freeciv s�rbehandlar dessa:
de tas bort ur arbetsminnet n�r spelet b�rjar och l�ses in igen om det
beh�vs.

----------------------------------------------------------------------
Enskilda beskrivningsfiler:
---------------------------

Varje beskrivningsfil beskriver en bildfil (f�r n�rvarande endast i
xpm-format) s�som det anges i beskrivningsfilen. Bildfilen m�ste
finnas i Freecivs datas�kv�g, men inte n�dv�ndigtvis n�ra
beskrivningsfilen. (D�rf�r man man ha flera beskrivningsfiler som
anv�nder samma bildfil p� olika s�tt.)

Huvudinformationen som beskrivs i beskrivningsfilen st�r i stycken som
heter [grid_*], d�r * �r n�got godtyckligt m�rke (men entydigt inom
varje fil). Ett rutn�t motsvarar en vanlig tabell av rutor. I
allm�nhet kan man ha flera rutn�t i varje fil, men
standardbildupps�ttningarna har vanligtvis bara ett i varje fil.
(Flera rutn�t i samma fil vore anv�ndbart f�r att ha olika
rutstorlekar i samma fil.) Varje rutn�t anger ett utg�ngsl�ge (�vre
v�nstra) och rutavst�nd, b�da i bildpunkter, och h�nvisar sedan till
ensilda rutor i rutn�tet med hj�lp av rad och kolumn. Rader och
kolumner r�knas med (0, 0) som �vre v�nstra.

Enskilda rutor ges ett m�rke som �r en str�ng som det h�nvisas
till i koden eller fr�n regelupps�ttningsfilerna. Ett rutn�t kan vara
glest, med tomma rutor (deras koordinater n�mns helt enkelt inte), och
en enda ruta kan ha flera m�rken (f�r att anv�nda samma bild f�r flera
syften i spelet), ange helt enkelt en lista med kommateckenskilda
str�ngar.

Om ett givet m�rke f�rekommer flera g�nger i en beskrivningsfil s�
anv�nds den sista f�rekomsten. (Allts� i den ordning som filerna �r
n�mnda i det �vre lagrets beskrivningsfil och ordningen i varje
ensild fil.) Detta till�ter att valda bilder ers�tts genom att ange en
ers�ttningsbeskrivningsfil n�ra slutet av fillistan i det �vre lagrets
beskrivningsfil lagret utan att �ndra tidigare filer i listan.

----------------------------------------------------------------------
M�rkesf�rstavelser:
-------------------

F�r att h�lla ordning p� m�rkena finns det ett grovt f�rstavelsesystem
som anv�nds f�r vanliga m�rken:

  f.	        nationsflaggor
  r.	        v�g/j�rnv�g
  s.	        allm�nt liten
  u.	        enhetsbilder
  t.	        grundlandskapsslag (med _n0s0e0w0 to _n1s1e1w1)
  ts.	        s�rskilda tillg�ngar i landskapet
  tx.	        ytterligare landskapsegenskaper
  gov.	      regeringsformer
  unit.	      ytterligare enhetsinformation: tr�ffpunkter, stack,
              sysslor (g� till, bef�st med mera)
  upkeep.     enhetsunderh�ll och lycklighet
  city.	      stadsinformation (stad, storlek, tillverkning i ruta,
              upplopp, upptagen)
  cd.	        standardv�rden f�r stad
  citizen.    stadsinv�nare, �ven fackm�n
  explode.    explosionsbilder (k�rnvapen, enheter)
  spaceship.  rymdskeppsdelar
  treaty.     avtalstummar
  user.	      h�rkors (i allm�nhet: anv�ndargr�nssnitt?)

I allm�nhet m�ste bildm�rken som �r h�rdkodade i Freeciv
tillhandah�llas i beskrivningsfilerna, annars v�grar klienten att g�
ig�ng. Bildm�rken som ges av regelupps�ttningarna (�tminstone
standardregelupps�ttningarna) ska ocks� tillhandah�llas, men i
allm�nhet g�r klienten ig�ng �nd�, men d� kan klientens bildvisning
bli alltf�r bristf�llig f�r anv�ndaren. F�r att fungera ordentligt ska
m�rkena h�nvisa till bilder med l�mplig storlek. (Grundstorleken kan
variera, s�som anges i det �vre lagrets beskrivningsfil, men de
ensilda rutbilderna ska st�mma �verens med dessa storlekar eller
anv�ndningen av dessa bilder.)

----------------------------------------------------------------------
