Stadsf�rvaltning
================

Stadsf�rvaltningen �r ett ganska nytt verktyg som byggts in i
gtk-klienten i version 1.12.1 av Freeciv. Den �r till f�r att hj�lpa
till att hantera st�der, det vill s�ga f�rdela arbetarna p� de olika
arbetsuppgifterna s� att st�derna blir s� l�nsamma som m�jligt. Man
kan s�tta p� och st�nga av stadsf�rvaltningen n�r som helst f�r vilken
stad som helst, men det uppst�r problem om man blandar
stadsf�rvaltning med eget styre i grupper av st�der som �verlappar
varandra (se nedan). Stadsf�rvaltningen kr�ver serverversion 1.12.1
eller nyare.

Stadsf�rvaltningen hj�rta �r en optimerande algoritm som f�rs�ker
anv�nda stadens arbetare p� ett s�tt som uppfyller anv�ndarens m�l s�
mycket som m�jligt. Det fanns redan f�re stadsf�rvaltningen ett slags
optimering; n�r man �ppnde en stadsdialog och klickade p� mittrutan i
kartan (stadsk�rnan) s� sattes arbetarna ut p� ett s�tt som
huvudsakligen maximerade forskningen, men struntar i ordning/upplopp.

Stadsf�rvaltningen g�r mycket mer �n denna enkla optimering. F�r det
f�rsta s� utf�r den ber�kningen p� nytt varje g�ng n�got f�r�ndras i
staden, till expempel: staden v�xer eller krymper, krigsenheter g�r in
eller ut, rutor f�r bevattning eller gruva eller bes�tts av fiender.
F�r det andra s� kan stadsf�rvaltningen g�ra alla slags optimeringar,
till exempel tillverkning (sk�ldar), guld, forskning eller �verfl�d.
F�r det tredje s� ger den spelaren utf�rligt inflytande �ver detta,
med m�jlighet att s�tta begr�nsningar f�r varje slag av utbyte fr�n
staden. Det senare innefattar firande, vilket g�r det mycket enkelt
att l�ta sina st�der v�xa, �ven i sv�ra tider. F�r det fj�rde och
antagligen viktigaste i krigstider s� h�ller den stadsborna n�jda s�
att det inte blir upplopp.


  Anv�ndning
==============

Man kan st�lla in stadsf�rvaltningen f�r en stad genom att �ppna
stadsf�nstret och klicka p� stadsf�rvaltningsfliken. P� v�nster sida
kan man v�lja en f�rinst�llning f�r ett visst m�l. P� h�ger sida kan
man ange mer invecklade m�l genom att anv�nda rullningslisterna. Man
kan v�lja en f�rinst�llning f�rst och sedan �ndra det. N�r man har
skapat en ny inst�llning kan man l�gga till det som en f�rinst�llning
med ett namn. Detta �r inte n�dv�ndigt men mycket anv�ndbart eftersom
man kan se och till och med �ndra en stads inst�llning fr�n
stadsredog�relsen om inst�llningen har ett namn.
Man ska komma i h�g att spara inst�llningarna (i "Spel"-menyn) n�r man
har skapat en ny f�rinst�llning.

Rullningslisterna �r av 2 slag: de h�gra rullningslisterna �r
faktorer varmed man kan v�rdera olika enheter j�mf�rt med varandra,
till exempel hur mycket sk�ldar �r v�rda j�mf�rt med allt annat. De
v�nstra rullningslisterna �r begr�nsningar. Man kan beordra staden att
inte f�rlora man genom att s�tta �verskottsbegr�nsningen f�r mat till
0, man kan till�ta staden att f�rlora guld genom att s�tta
�verskottsbegr�nsningen f�r guld till -3, och man kan beordra staden
att tillverka minst 5 sk�ldar genom att s�tta �verskottsbegr�nsningen
f�r sk�ldar till 5. Den kraftfullaste begr�nsningen �r dock
firarbegr�nsningen, vilken g�r att staden genast firar (det har
vanligtvis sin verkan omg�ngen efter man �ndrar det).

Det �r uppenbart att stadsf�rvaltningen inte kan uppfylla alla dessa
begr�nsningar i alla l�gen. N�r den inte kan det avg�r den med
meddelandet "Kan inte utf�ra uppdraget i Stockholm. L�mnar uppdraget."
D� kan man v�lja mellan at sk�ta staden sj�lv (vilket har vissa
nackdelar, se nedan) eller �ppna staden och �ndra
�verskottsbegr�nsningarna s� att de kan uppfyllas.

N�r man har gjort inst�llningarna f�r en stad m�ste man klicka p�
"Styr stad" f�r att l�mna �ver staden till stadsf�rvaltningen. Om
knappen inte �r klickbar s� styrs staden redan av stadsf�rvaltningen
eller s� �r m�len ouppn�eliga. I det senare fallet ser man streck i
st�llet f�r tal i vinstblocket. Om man n�gonsin vill st�nga av
stadsf�rvaltningen avsiktligt s� trycker p� p� Sl�pp stad.


  Fortskridet anv�ndande
==========================

Det finns �nnu inte mycket erfarenhet av att anv�nda stadsf�rvaltning,
men n�gra vanliga p�pekanden kan vara hj�lpsamma. Vanligtvis beror
st�dernas m�l p� vilket skede i spelet man befinner sig, om man vill
sprida sig vitt, v�xa snabbt, forska eller kriga. Man b�r s�tta en h�g
faktor f�r forskning n�r man ska forska och en h�g faktor f�r sk�ldar n�r
man ska bygga enheter. Den h�gsta tillg�ngliga faktorn �r 25. Det
inneb�r att om sk�ldfaktor �r 25 och de andra �r 1, s� f�redrar
stadsf�rfaltningen en enda sk�ld �ver 25 guld (eller handel). Detta �r
ganska mycket eftersom man �ven kan k�pa enheter f�r guld. Det inneb�r
�ven att stadsf�rvaltningen �r likgiltig om tillverkning av guld,
forskning, �verfl�d och mat; men n�r man krigar f�redrar man
vanligtvis guld eller �verfl�d. S� det �r antagligen bra att s�tta en
andra (eller till och med tredje) faktor f�r stadens vinst, allts�
guldfaktor 5. Det g�r fortfarande att 1 sk�ld f�redras framf�r 5 guld
(och 1 guld �ver 5 mat eller n�gonting annat).

Begr�nsningar �r inte anv�ndbara i alla l�gen. Om man vill ha en h�g
guldvinst �r det antagligen b�ttre att s�tta guldfaktorn till 25 �n
att s�tta en �verskottsgr�ns p� 5 guld eller liknande ty en stor stad
kan ge mer guld �n en liten och man skulle d�rmed beh�va s�tta olika
�verskottsgr�nser f�r guld i olika st�der.

Om en stads sk�ld�verskott �r under 0 s� kan den inte
underh�lla alla sina enheter l�ngre. Man f�rlorar d� vissa av
enheterna. Om mat�verskottet �r under 0 s� sv�lter staden s� sm�ningom
och krymper (n�r s�deslagret �r tomt). Detta m� vara avsiktligt, men
om staden underh�ller bos�ttare s� f�rloras de innan staden krymper.
Begr�nsningar kan �ven varna.

Vilka begr�nsningar som kan uppfyllas beror mycket p� rikets
forsknings-, skatte- och �verfl�dssatser. Ett guld�verskott p� >= 0 �r
allts� l�ttare att uppfylla med en h�gre skattesats �n med en l�gre.
Man ska alltid �verv�ga att �ndra dessa satser n�r man t�nker �ndra
stadsf�rvaltningsdirektiven f�r de flesta av sina st�der.

R�d: F�r att undivka att oavsiktligt avs�tta stadsf�rvaltningar n�r
man �ndrar satser �r det b�st att g�ra det fr�n skattedialogen i
st�llet f�r fr�n satsvisaren i huvudf�nstret.


  Nackdelar
============

Stadsf�rvaltningen �r ett kraftfullt verktyg som inte bara avlastar
spelaren fr�n finstyrnignen av st�derna utan �ven ger b�ttre l�nsamhet
�n vad de flesta spelarna brukar �stadkomma sj�lva.

Det finns dock n�gra nackdelar. N�r man har tillsatt
stadsf�rvaltningen s� l�gger den beslag p� varje bra ruta som den kan
komma �t. S� det blir v�ldigt sv�rt att styra �verlappande stader
sj�lv. Om man vill ha b�de st�der med stadsf�rvaltning och st�der som
man f�rvaltar sj�lv s� b�r de inte �verlappa varandra.

Det finns flera l�gen d� stadsf�rvaltningen tillf�lligt inte kan uppn�
sina m�l, till exempel n�r man f�rflyttar ett skepp fr�n en stad till
en annan eller n�r ie fientlig enhet g�r genom ens land.
Stadsf�rvaltningen avg�r d� och man m�ste �tertills�tta den f�r hand.
Ett allm�nt f�rfarande f�r att f�rhindra detta �r att s�tta
begr�nsningarna s� l�gt som m�jligt (-20). Naturligtvis m�ste man vara
f�rsiktig med mat- och sk�ld�verskotten.

Emedan stadsf�rvaltningen arbetar utm�rkt f�r enskilda st�der s�
sl�pper den aldrig en ruta till f�rm�n f�r n�gon annan stad.
Stadsf�rvaltningarna f�r de olika st�derna kommer p� tur i en
slumpm�ssig ordning. D�rf�r blir den sammanlagda vinsten f�r en grupp
�verlappande st�der inte alltid optimal.


  Inst�llningsfil
===================

Klienten l�ter anv�ndaren ladda och spara f�rinst�llningar f�r
stadsf�rvaltningen. V�lj "Spara inst�llningar" fr�n "Spel"-menyn f�r
att spara allm�nna inst�llningar, meddelandeinst�llningar och �ven
stadsf�rvaltningsinst�llningar.

Formatet f�r inst�llningsfilen (vanligtvis ~/.civclientrc) �r f�ljande
(i fall att man vill �ndra f�rinst�llningarna f�r hand, till exempel
med en textredigerare).

Under �verskriften [cma] fins en "number_of_presets". Detta ska s�ttas
till det antal f�rinst�llningar som finns i inst�llningsfilen. Om man
l�gger till eller tar bort f�rinst�llningar s� m�ste man �ndra detta
tal till r�tt v�rde.

D�refter kommer en tabell med f�rinst�llningarna. H�r �r huvudet:

preset={ "name","minsurp0","factor0","minsurp1","factor1","minsurp2",
"factor2","minsurp3","factor3","minsurp4","factor4","minsurp5",
"factor5","reqhappy","factortarget","happyfactor"

s� f�rinst�llningarnas ordning ska vara f�ljande:

f�rinst�llningens namn, minsta �verskott 0, faktor 0, ... , kr�v att
stad ska vara lycklig, vad m�let ska vara [0, 1], lycklighetsfaktor.

F�r n�rvarande finns det 6 �verskott och faktorer. De �r:
0 = mat, 1 = sk�ldar, 2 = handel, 3 = guld, 4 = �verfl�d,
5 = forskning

F�r tillf�llet �r inte "factortarget" �ndringsbar med klienten, se
"client/agents/citizen_management.h" f�r mer information.

Tabellen ska avslutas med '}'. H�r �r 21 f�rinst�llningar om f�r de
som inte kan komma p� sina sj�lva:

"max mat",0,10,0,1,0,1,0,1,0,1,0,1,0,0,1
"max sk�ldar",0,1,0,10,0,1,0,1,0,1,0,1,0,0,1
"max handel",0,1,0,1,0,10,0,1,0,1,0,1,0,0,1
"max skatt",0,1,0,1,0,1,0,10,0,1,0,1,0,0,1
"max �verfl�d",0,1,0,1,0,1,0,1,0,10,0,1,0,0,1
"max forskning",0,1,0,1,0,1,0,1,0,1,0,10,0,0,1
"+2 mat",2,1,0,1,0,1,0,1,0,1,0,1,0,0,1
"+2 sk�ldar",0,1,2,1,0,1,0,1,0,1,0,1,0,0,1
"+2 handel",0,1,0,1,2,1,0,1,0,1,0,1,0,0,1
"+2 guld",0,1,0,1,0,1,2,1,0,1,0,1,0,0,1
"+2 �verfl�d",0,1,0,1,0,1,0,1,2,1,0,1,0,0,1
"+2 forskning",0,1,0,1,0,1,0,1,0,1,2,1,0,0,1
"max mat ingen guldbegr�nsning",0,10,0,1,0,1,-20,1,0,1,0,1,0,0,1
"max sk�ldar ingen guldbegr�nsning",0,1,0,10,0,1,-20,1,0,1,0,1,0,0,1
"max handel ingen guldbegr�nsning",0,1,0,1,0,10,-20,1,0,1,0,1,0,0,1
"max guld ingen guldbegr�nsning",0,1,0,1,0,1,-20,10,0,1,0,1,0,0,1
"max �verfl�d ingen guldbegr�nsning",0,1,0,1,0,1,-20,1,0,10,0,1,0,0,1
"max science ingen guldbegr�nsning",0,1,0,1,0,1,-20,1,0,1,0,10,0,0,1
"max mat+sk�ldar ingen guldbegr�nsning",0,10,0,10,0,1,-20,1,0,1,0,1,0,0,1
"max mat+sk�ldar+handel",0,10,0,10,0,10,0,1,0,1,0,1,0,0,1
"max allt",0,1,0,1,0,1,0,1,0,1,0,1,0,0,1

h�r �r 6 till som har lagt till i efterhand:

"+1 mat, max sk�ldar ingen guldbegr�nsning",1,1,0,10,0,1,-20,1,0,1,0,1,0,0,1
"+2 mat, max sk�ldar ingen guldbegr�nsning",2,1,0,10,0,1,-20,1,0,1,0,1,0,0,1
"+3 mat, max sk�ldar ingen guldbegr�nsning",3,1,0,10,0,1,-20,1,0,1,0,1,0,0,1
"+4 mat, max sk�ldar ingen guldbegr�nsning",4,1,0,10,0,1,-20,1,0,1,0,1,0,0,1
"+5 mat, max sk�ldar ingen guldbegr�nsning",5,1,0,10,0,1,-20,1,0,1,0,1,0,0,1
"+6 mat, max sk�ldar ingen guldbegr�nsning",6,1,0,10,0,1,-20,1,0,1,0,1,0,0,1

and even more, some with multiple goals:

"forskning till varje pris",0,1,0,5,-20,1,-20,1,-20,1,-20,25,0,0,1
"firande och tillv�xt",1,1,0,25,-20,1,-20,12,-20,1,-20,1,1,0,1
"tillv�xt till varje pris",1,25,0,5,-20,1,-20,1,-20,1,-20,5,0,0,1
"forskning och n�gra sk�ldar",0,1,0,8,0,1,-3,1,0,1,0,25,0,0,1
"sk�ldar och lite guld",0,1,0,25,0,1,-3,3,0,1,0,1,0,0,1
"m�nga sk�ldar och lite guld",0,1,0,25,0,1,0,9,0,1,0,1,0,0,1
"sk�ldar och lite forskning",0,1,0,25,0,1,-2,1,0,1,0,8,0,0,1
"fira och v�x genast",1,1,0,25,-20,1,-20,1,-20,1,-20,8,1,0,1

senast uppdaterad 2002-01-09, �versatt 2002-07-04
