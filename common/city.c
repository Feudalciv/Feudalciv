/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "shared.h"
#include "tech.h"

#include "city.h"

static int improvement_upkeep_asmiths(struct city *pcity, int i, int asmiths);

/****************************************************************
All the city improvements:
Use get_improvement_type(id) to access the array.
The improvement_types array is now setup in:
   server/ruleset.c (for the server)
   client/packhand.c (for the client)
*****************************************************************/
struct improvement_type improvement_types[B_LAST];


char *default_roman_city_names[] = {
  "Roma", "Capua", "Syracusae", "Tarentum", "Neapolis",
  "Tingis", "Nova Carthago", "Brundisium", "Ravenna", "Numantia",
  "Utica", "Chartago", "Caesarea", "Messana", "Ierusalem",
  "Trapezus", "Durcotorum", "Perusia", "Leptis Magna", "Mediolanum",
  "Ariminum", "Tarraco", "Narbo Martius", "Pergamum", "Aquileia",
  "Salonae", "Cyrene", "Dyrrachium", "Luni", "Noreia",
  "Panoramus", "Ancyra", "Caralis", "Patavium", "Petra",
  "Pisae", "Ancona", "Palmyra", "Tyrus", "Cortona", 
  "Agrigentum", "Florentia", "Lix", "Sala", "Berytus",
  "Bononia", "Sabrata","Verona", "Attalea", "Sinope",
  "Velia", "Segesta", "Gaza", "Aleria", "Genua",
  "Arretium", "Thapsus", "Ostia", "Altinum", "Tarsus", (char *)0
};

char *default_babylonian_city_names[] = {
  "Babylon", "Lagash", "Nippur", "Ur", "Kish", 
  "Shuruppak", "Kisurra", "Cutha", "Adab", "Umma",
  "Akkad", "Eridu", "Larsa", "Borsippa", "Sippar",
  "Nineveh", "Ashur", "Calach", "Carcar", "Hamat", /* Assyria */
  "Opis", "Cunaxa", "Arbela", "Nisibis", "Carrhae", /* Persian Babylonia */
  "Seleucia", "Ctesiphon", "Sittace", "Orchoe", "Apamea", /* Greek Babylonia */
  (char *)0
};

/* Important German cities in the 17th century */
char *default_german_city_names[] = {
  "Frankfurt", "Mainz", "Trier", "Aachen", "M�nster",
  "Bremen", "L�beck", "Hamburg", "Berlin", "Dresden",
  "Wien", "Fulda", "W�rzburg", "Bamberg", "N�rnberg",
  "M�nchen", "Brixien", "Salzburg", "Passau", "K�ln",
  "Koblenz", "Leipzig", "Regensburg", "Heidelberg", "Verden",
  "Kassel", "Wittenberg", "Wismar", "Ulm", "N�rdlingen",
  "Stralsund", "Breslau", "Marburg", "Schlettstadt", "Augsburg",
  "Mansfeld", "Donauw�rth", "Dessau", "Osnabr�ck", "Hannover",
  "M�hlberg", "Wittstock", "Speyer", "Kleve", "Worms",
  "Landau", "Heilbronn", "Rosheim", "Schmalkalden", "Lutter", 
  "Wiesloch", "Rosslau", "Magdeburg", "Minden", "Sinsheim", 
  "Philippsburg", "Halberstadt", "Hersfeld", "Altenburg", "Klostergrab", 
 (char *)0
};

/* Greek/Latin names */
char *default_egyptian_city_names[] = {
  "Thebae", "Memphis", "Heliopolis", "Syene", "Arsino�",
  "Ammonium", "Alexandria", "Pelusium", "Canopus", "Naucratis",
  "Diospolis",  "Sais", "Acoris", "Cynopolis", "Thinis", 
  "Timonepsis", "Nilopolis", "Chemmis", "Antaeopolis", "Hermonthis",
  "Letopolis", "Aphroditopolis", "Heracleopolis", "Oxyrynchus",
  "Ibiu", "Busiris", "Andropolis", "Sele", "Antiphrae", "Apis", (char *)0
};

char *default_american_city_names[] = {
  "Washington", "New York", "Boston", "Philadelphia", "Los Angeles", 
  "Atlanta", "Chicago", "Buffalo", "St. Louis", "Detroit",
  "New Orleans", "Baltimore", "Denver", "Cincinnati", "Dallas", 
  "Minneapolis", "Houston", "San Antonio", "San Francisco", "Seattle",
  "Portland", "Miami", "Cleveland", "Buffalo", "Pittsburgh",
  "Salt Lake City", "San Diego", "Phoenix", "Kansas City", "Indianapolis",
  "Orlando", "Milwaukee", "Honolulu", "Boise", "Las Vegas", "Springfield",
  "Nashville", "Hartford", "Charlotte", "El Paso", "Anchorage",
  "Spokane", "Tacoma", "Fort Worth", "Austin", "Sacramento", "Oklahoma City",
  "St. Paul", "Jacksonville", "Sioux Falls", "Madison", "Lubbock",
  "Amarillo", "Omaha", "Baton Rouge", "Little Rock", "Tucson",
  "Erie", "Flint",  "Trenton", "Newark", "Arlington", "Richmond", "Reno",
  "Santa Fe", "Tulsa", "Anapolis", "Dover", "Akron", "Urbana-Champaign", 
  (char *)0
};

/* Latinized names */
char *default_greek_city_names[] = {
  "Athenae", "Sparta", "Thebae", "Corinthus", "Byzantium",
  "Miletus", "Ephesus", "Piraeus", "Argos", "Megalopolis",
  "Megara", "Eleusis", "Chalcis", "Eretria", "Thessalonica",
  "Pella", "Edessa", "Dodona", "Nikopolis", "Naupactus",
  "Patrae", "Olympia", "Pylos", "Mantinea", "Tegea",
  "Nauplion", "Mycenae", "Nemea", "Epidauros", "Aulis",
  "Orchomenos", "Marathon", "Delphi", "Thermopylae", "Phaleron",
  "Nicaea", "Heraclea", "Potidaea", "Stagira", "Olynthus", 
  "Colophon", "Phocaea", "Amphissa", "Elateia", "Hyampolis",
  "Atalante", "Oropos", "Lebadeia", "Larissa", "Iolkus",
  "Amphiclaea", "Mesembria", "Cyzicus", "Halicarnassus", "Pergamos",
  "Pydna", "Philadelphea", "Therme", "Amphipolis", "Corcyra",
  "Ithaca", "Thera", "Delos", "Panormos", "Ortygia",
  "Melos", "Lykosoura", "Amyclae", "Clazomenae", "Kynos Kephalae", (char *)0
};

char *default_indian_city_names[] = {
  "Delhi", "Bombay", "Madras", "Bangalore", "Calcutta",
  "Lahore", "Karachi", "Ahmadabad", "Kanpur", "Hyderabad",
  "Dacca", "Lucknow", "Benares", "Chandrigarh", "Rawalpindi",
  "Colombo", "Madurai", "Coimbatore", "Calicut", "Trivandrum",
  "Nagpur", "Chittagong", "Multan", "Amritsar", "Srinagar",
  "Agra", "Bhopal", "Patna", "Surat", "Jaipur", (char *)0
};

/* Soviet names */
char *default_russian_city_names[] = {
  "Moskva", "Leningrad", "Kiev", "Gor'kiy", "Sverdlovsk",
  "Kuybyshev", "Donetsk", "Khar'kov", "Dnepropetrovsk", "Baku",
  "Novosibirsk", "Tashkent", "Minsk", "Riga", "Odessa", 
  "Tbilisi", "Yerevan", "Vyborg", "Perm'", "Ufa", 
  "Saratov", "Stalingrad", "Rostov", "Alma-Ata", "Omsk",
  "Chelyabinsk", "Irkutsk", "Vladivostok", "Kabarovsk", "Krasnoyarsk",
  (char *)0
};

/* We need more Zulu names */
char *default_zulu_city_names[] = {
  "Zimbabwe", "Ulundi", "Bapedi", "Hlobane", "Isandhlwana",
  "Intombe", "Mpondo", "Ngome", "Swazi", "Tugela", "Umtata",
  "Umfolozi", "Ibabanago", "Isipezi", "Amatikulu",
  "Zunguin", (char *)0
};

char *default_french_city_names[] = {
  "Paris", "Lyon", "Marseille", "Lille", "Bordeaux",
  "Nantes", "Rouen", "Reims", "Tours", "Angers", 
  "Toulouse", "Montpellier", "N�mes", "Toulon", "Orl�ans",
  "Chartres", "Avignon", "Grenoble", "Dijon", "Amiens",
  "Cherbourg", "Caen", "Calais", "Limoges", "Clermont-Ferrand",
  "Nancy", "Besan�on", "St. Etienne", "Brest", "Perpignan", (char *)0
};

char *default_aztec_city_names[] = {
  "Michoacan", "Axaca", "Tuxpan", "Metztitlan", "Otumba",
  "Tlacop�n", "Theotihucan�n", "Tezcuco", "Jalapa", "Tlaxcala",
  "Cholula", "Tenochtitl�n", "Yopitzingo", "Teotitl�n", "Theuantepec",
  "Chiauhtia", "Chapultepec", "Coatepec", "Ayotzinco", "Itzapalapa",
  "Iztapam", "Mitxcoac", "Tacubaya", "Tecamac", "Tepezinco",
  "Ticom�n", "Xaltoc�n", "Xicalango", "Zumpanco", (char *)0
};

/* Pin Yin transliteration */
char *default_chinese_city_names[] = {
  "Beijing", "Shanghai", "Guangzhou", "Nanjing", "Tianjin",
  "Xi'an", "Taiyuan", "Shenyang", "Changchun", "Harbin",
  "Fushun", "Wuhan", "Chengdu", "Chongquing", "Kunming",
  "Hong Kong", "Guiyang", "Lanzhou", "Zhengzhou", "Changsha", 
  "Nanchang", "Fuzhou", "Hangzhou", "Wuxi", "Suzhou",
  "Xuzhou", "Jinan", "L�da", "Jilin", "Qiqihar", (char *)0
};


char *default_english_city_names[] = {
  "London", "Birmingham", "Liverpool", "Manchester", "Leeds",
  "Newcastle", "Glasgow", "Belfast", "Edinburgh", "Middlesborough",
  "Sheffield", "Nottingham", "Coventry", "Cardiff", "Bristol",
  "Brighton", "Portsmouth", "Hull", "Leicester", "Plymouth",
  "Sunderland", "Stoke-on-Trent", "Bolton", "Norwich", "Oxford",
  "Swansea", "Dundee", "Cambridge", "Aberdeen", "Blackpool", (char *)0
};

char *default_mongol_city_names[] = {
  /* Chagatai */
  "Almaligh", "Khotan", "Turfan", "Samarkand", "Bokhara",
  "Tashkent", "Kashgar", "Balkh", "Kabul",
  /* Quipciaq (Golden Horde) */
  "Sarai", "Astrachan", "Tana", "Kazan", "Caffa", "Bulgar",
  /* Gran Khan */
  "Karakorum", "Hami", "Dunhuang", "Lhasa", "Tali",
  /* Ilkhan */
  "Tabriz", "Mosul", "Baghdad", "Basra", "Shiraz",
  "Esfahan", "Rayy", "Kerman", "Hormuz", "Daybul", (char *)0
};

char **race_default_city_names[R_LAST]={
  default_roman_city_names,
  default_babylonian_city_names,
  default_german_city_names,
  default_egyptian_city_names,
  default_american_city_names,
  default_greek_city_names,
  default_indian_city_names,
  default_russian_city_names,
  default_zulu_city_names,
  default_french_city_names,
  default_aztec_city_names,
  default_chinese_city_names,
  default_english_city_names,
  default_mongol_city_names
};

/* collection of towns aroud the world */
/* from xconq-7, distributed under the GNU GPL */
/* Copyright (C) 1991-1997 Stanley T. Shebs <shebs@cygnus.com> */
char *misc_city_names[] = {
  /* Soviet Union */
  "Uglegorsk", "Taganrog", "Kuzemino", "Igodovo", 
  "Izhevsk", "Leninskoye", "Zvenigorod", "Faustovo", 
  "Tokma", "Bolotnoje", "Pudino", "Predivinsk", 
  "Gotoputovo", "Stupino", 
  /* Japan */
  "Toyooka", "Kobayashi", "Kamiyahagi", "Fukude", 
  /* China */
  "Dandong", "Xingtai", "Xiaojiagang", "Wushu", 
  "Wutangjie", "Qingfeng", "Dushikou", "Huilong", 
  "Linyi", "Miaoyang", "Xinbo", "Bugt", 
  /* Indochina */
  "Tan-an", "Ban Khlong Kua", "Bo Phloi", "Thot-not",
  "Herbertabad", "Mong Pawn", "Roi Et",
  /* Indonesia */
  "Butong", "Lubukbertubung", "Moutong", "Gimpu", 
  "Waingapu", "Sindangbarang", "Kualakapuas", "Bongka", 
  "Salimbatu", "Bonggaw", "Baing", "Grokgak", 
  /* India */
  "Bap", "Meerut", "Harda", "Garwa", 
  "Digboi", "Kurnool", "Nirmal", "Coondapoor", 
  "Tetulbaria", "Maheshwar", "Paramagudi", "Bhakkar", 
  "Mungaoli", "Shorapur", "Channapatna", "Chilaw", 
  /* Middle East and Central Asia */
  "Bajandalaj", "Cogt-Ovoo", "Un't", "Ich-Uul", 
  "Yazd", "Samarkand", "Mashhad", "Chah Bahar", 
  "Jubbah", "Al-'Awsajiyah", "Kifri", "Kashgar", 
  "Chundzha", "Ushtobe", "Dzaamar", "Wadi Musa", 
  "Bogustan", "Gakuch", 
  /* Africa */
  "Pibor Post", "Umm Digulgulaya", "Umm Shalil", "Buzaymah", 
  "Gedo", "North Horr", "Todenyang", "Madadi", 
  "Ngetera", "Ouadda", "Mazoula", "Tiglit", 
  "Gummi", "Gbarnga", "Burutu", "Bafwabalinga", 
  "Goonda", "Ankoroka", "Vryburg", "Matuba", 
  "Bakouma", "El Idrissia", "Agadir", "Nungwe", 
  "Bunianga", "Ngali", "Nguiroungou", "Otukpa", 
  "Hell-Ville", "Morafenobe", "Tongobory", "Farafangana", 
  "Mungbere", "Haco", "Barbar", "Oulessebougou",
  /* Australia */
  "Nookawarra", "Bunbury", "Buckleboo", "Breeza Plains", 
  "Mistake Creek", "Boolaloo", "Yarloop", "Dubbo", 
  "Bushy Park", "Old Cork", "Cessnock", "Wagga Wagga", 
  "Mungar Junction", "Koolywirtie", "Wonthaggi",
  "Oatlands", "Bindebango", "Alice Springs",
  /* New Guinea */
  "Kwatisore", "Finschhafen", "Yobi", "Rumahtinggih", 
  /* USA */
  /* AL */
  "New Hope", "Hackleburg", 
  /* AK */
  "Kaktovik", "Fort Yukon", 
  /* AZ */
  "Benson", "Gila Bend", "Turkey Flat", "Tuba City",  
  "Wide Ruins", 
  /* AR */
  "Metalton", "Oil Trough", "Hackett",
  /* CA */
  "Burnt Ranch", "Calexico", "Eel Rock", "Gilroy", 
  "Joshua Tree", "Milpitas", "Mormon Bar", "Pumpkin Center", 
  "Death Valley Junction", "Toms Place",
  "Pinole", "Petaluma", 
  "Scotts Valley", "Whiskeytown", "Leucadia", "Lompoc",
  "Granada Hills",
  /* CO */
  "Las Animas", "Silver Plume", 
  /* CT */
  "Upper Stepney", "Moosup", "Danbury",
  /* FL */
  "Yeehaw Junction", "Big Pine Key", 
  "Panacea", "Wewahitchka", "Estiffanulga", 
  /* GA */
  "Dixieion", "Fowlstown", "Dacula", "Americus", 
  /* HW */
  "Laupahoehoe", 
  /* ID */
  "Malad City", "Kootenai", 
  /* IL */
  "Farmer City", "Aroma Park", "Goreville", "Illiopolis",  
  "Mascoutah", "Metamora", "Metropolis", "New Boston",  
  "Pontoon Beach", "Romeoville", "Teutopolis",  
  /* IN */
  "Etan Green", "French Lick", "Loogootee", "Needmore",  
  "Ogden Dunes", "Oolitic", "Star City",  
  /* IA */
  "Coon Rapids", "Correctionville", "Grundy Center",
  "Lost Nation", "Ossian", "Sac City",  
  /* KA */
  "Countryside", "Mankato", "Pretty Prairie",  "Greeley",
  "Grouse Creek",
  /* KY */
  "Big Clifty", "Cloverport", "Druid Hills", "Fancy Farm", 
  "Hardburly", "Hardshell", "Horse Cave", "Pleasureville", 
  "Science Hill", "Sublimity City", "Watergap", 
  /* LA */
  "Bayou Goula", "Cut Off", "Hackberry", "Lutcher", 
  "Waggaman", 
  /* ME */
  "Veazie", "Madawaska", 
  /* MD */
  "Bestgate", "College Park", "Frostburg", "Pocomoke City", 
  "Port Deposit", "Pumphrey", "Tammany Manor",
  "Weems Creek", "Whiskey Bottom", "Hack Point",
  /* MA */
  "Assinippi", "Buzzards Bay", "Dorothy Pond", "Hopkinton", 
  "Housatonic", "Pigeon Cove", "Swampscott", "Gloucester",
  "Hyannis Port", "Ipswich", "Boxford",
  /* MI */
  "Bad Axe", "Brown City", "Cassopolis", "New Buffalo", 
  "Petoskey", "Ishpeming", "Ypsilanti", "Saugatuck", 
  /* Michigan UP (from Sandra Loosemore) */
  "Skanee", "Bruce Crossing", "Baraga", "Germfask", 
  "Assinins", "Tapiola", "Gaastra", "Bete Grise", 
  /* MN */
  "Ada", "Blue Earth", "Brainerd", "Eden Valley",  
  "Lino Lakes", "New Prague", "Sleepy Eye", "Waconia",  
  /* MS */
  "Bogue Chitto", "Buckatunna", "Guntown", "Picayune", 
  "Red Lick", "Senatobia", "Tie Plant", "Yazoo City",  
  /* MO */
  "Bourbon", "Doe Run", "Hayti", "Humansville", 
  "Lutesville", "Moberly", "New Madrid", "Peculiar", 
  "Sappington", "Vandalia",  
  /* MT */
  "Big Sandy", "Hungry Horse", 
  "Kalispell",  "East Missoula",
  /* NE */
  "Hershey", "Loup City", 
  "Minatare", "Wahoo",  "Grainfield",
  /* NV */
  "Winnemucca", "Tonopah", "Jackpot",  
  /* NH */
  "Littleton", "Winnisquam",  
  /* NJ */
  "Cheesequake", "Freewood Acres",
  "Forked River", "Hoboken", "Succasunna",  
  "Maple Shade", "New Egypt", "Parsippany", "Ship Bottom",  
  /* NM */
  "Adobe Acres", "Cloudcroft", "Ruidoso", "Toadlena",  
  "Los Padillos", "Ojo Caliente", 
  /* NY */
  "Angola on the Lake", "Podunk", "Chili Center",
  "Aquebogue", "Muttontown", "Hicksville", 
  "Hoosick Falls", "Nyack",
  "Painted Post", "Peekskill", "Portville",  
  "Ronkonkoma", "Wappingers Falls", 
  "Sparrow Bush", "Swan Lake",
  /* NC */
  "Altamahaw",
  "Biltmore Forest", "Boger City", "Granite Quarry",  
  "High Shoals", "Lake Toxaway",
  "Scotland Neck", "Hiddenite", 
  "Mocksville", "Yadkinville", "Nags Head", 
  "Kill Devil Hills", "Rural Hall",  
  /* ND */
  "Cannon Ball", "Hoople", "Zap",  
  /* OH */
  "Academia", "Arcanum", "Blacklick Estates", "Blue Ball",  
  "Crooksville", "Dry Run", "Flushing", "Gratis",  
  "Lithopolis", "Mingo Junction", "Newton Falls",
  "New Straitsville", "Painesville", "Pepper Pike", 
  "Possum Woods", "Sahara Sands",  
  /* OK */
  "Bowlegs", "Broken Arrow", "Fort Supply", "Drumright", 
  "Dill City", "Okay", "Hooker",  
  /* OR */
  "Condon", "Happy Valley", "Drain", "Junction City", 
  "Molalla", "Philomath", "Tillamook", "Wankers Corner",
  /* PA */
  "Atlasburg", "Beaver Meadows", "Birdsboro", "Daisytown", 
  "Fairless Hills", "Fairchance", "Kutztown", "Erdenheim", 
  "Hyndman", "Pringle", "Scalp Level", "Slickville", 
  "Zelienople", "Sugar Notch", "Toughkenamon", "Throop", 
  "Tire Hill", "Wormleysburg", "Oleopolis",
  /* RI */
  "Woonsocket", "Pawtucket",
  /* SC */
  "Due West", "Ninety Six", 
  "Travelers Rest", "Ware Shoals",  
  /* SD */
  "Deadwood", "Lower Brule", 
  "New Underwood", "Pickstown", 
  "Plankinton", "Tea", "Yankton",  
  /* TN */
  "Berry's Chapel", "Bulls Gap", "Cornersville", "Counce", 
  "Gilt Edge", "Grimsley", "Malesus", "Soddy-Daisy",  
  /* TX */
  "Bastrop", "New Braunfels", "Harlingen", "Dimock", 
  "Devils Elbow", "North Zulch", "Llano", "Fort Recovery", 
  "Arp", "Bovina", "Cut and Shoot", "College Station", 
  "Grurer", "Iraan", "Leming", "Harlingen", 
  "Muleshoe", "Munday", "Kermit", "La Grange", 
  "Ropesville", "Wink", "Yoakum", "Sourlake",  
  /* UT */
  "Delta", "Moab", "Nephi", "Loa", 
  "Moroni", "Orem", "Tooele", "Sigurd", 
  /* VT */
  "Bellows Falls", "Chester Depot", "Winooski",  
  /* VA */
  "Accotink", "Ben Hur", "Ferry Farms", "Disputanta", 
  "Dooms", "Sleepy Hollow", "Max Meadows", "Goochland", 
  "Rural Retreat", "Sandston", "Stanleytown",
  "Willis Wharf", "Stuarts Draft", 
  /* WA */
  "Black Diamond", "Carnation", "Cle Elum", "Cosmopolis", 
  "Darrington", "Enumclaw", "Forks", "Goose Prairie", 
  "Navy Yard City", "La Push", "Soap Lake", "Walla Walla", 
  "Sedro Woolley", "Pe Ell", "Ruston",  
  /* WV */
  "Barrackville", "Pocatalico", "Fort Gay", "Big Chimney", 
  "Nutter Fort", "Hometown", "Nitro", "Triadelphia", 
  "Star City",  
  /* WI */
  "Combined Lock", "Coon Valley", "Black Earth",
  "New Holstein", "Little Chute", "Wisconsin Dells",
  "Random Lake", "Sheboygan", "Nauwatosa",  
  /* WY */
  "East Thermopolis", "Fort Washakie", "Paradise Valley", 
  /* Canada */
  "Sexsmith", "Squamish", "Fort Qu'Appelle", "Flin Flon", 
  "Moose Jaw", "Grand-Mere", "Great Village", "Pugwash", 
  "Chiliwack", "Cranbery Portage",  
  "Moosonee", "Joe Batt's Arm", "St.-Polycarpe",
  "Crabtree Mills", "Copper Cliff", "Uxbridge", 
  "Penetanguishene", "Boger City", "Drumheller", 
  "Port Blandford", "Hamtramck",
  /* USA? */
  "Hackensack", "North Middleboro", "Fannettsburg", 
  "Corkscrew", "Boynton Beach", 
  "Belchertown",
  /* South America */
  "Huatabampo", "Zapotiltic", "Ipiranga", "Perseverancia", 
  "Bilwaskarma", "Aguadulce",
  "Albert Town", "Fuente de Oro", 
  "Pedras de Fogo", "Maxaranguape", "Comodoro Rivadavia",
  "Coribe", "Rossell y Rius", "General Alvear",
  "Ushaia", "Los Antiguos", "Puerto Alegre", "Quevedo", 
  /* Eastern Europe */
  "Kannonkoski", "Uusikaupunki", "Ulfborg", "Wloszczowa", 
  "Drohiczyn", "Vrchlabi", "Oroshaza", "Klagenfurt", 
  "Pisz", "Krokowa", "Partizanske", "Ozd", 
  "Jimbolia", "Peshkopi", "Galaxidhion", "Naxos", 
  /* Iceland */
  "Thingvellir", "Honningsvag", "Vikna", "Jokkmokk",
  /* Scandinavia */
  "Rimbo", "Kukkola", "Viitasaari",
  "Guderup", "Grindsted", "Store Andst", "Odder", 
  "Vrigstad", "Trollhaetten", "Kinsarvik", "Grimstad", 
  /* Ireland */
  "Ballybunion", "Banagher", "Carncastle",
  /* Belgium */
  "Lisp", "Knokke", "Bialy", "Bor",
  "Hel", "Puck",
  /* Germany */
  "Diepholz", "Sangerhausen", "Biedenkopf", 
  "Mosbach", "Butzbach", "Goslar", "Studenka",
  "Slavonice", "Gouda", "Dokkum", "Oss",
  "Bad Bramstedt", "Dinkelsbuehl", "Hoogezand", 
  "Schoensee", "Fuerstenfeldbruck", 
  "Pfaffenhausen", "Namlos", "Bad Hall",
  "Consdorf", "Cloppenburg", "Bad Muskau", "Exing",
  /* France */
  "Bois-d'Arcy",
  "Presles-en-Brie", "Silly-le-Long", "Saint-Witz", 
  "Limoux", "Crozon", "Guilvinec", "Pignans",
  "La Tour-du-Pin", "Roquefort", "Saint-Quentin", 
  /* Italy */
  "Bobbio", "Viareggio", "Siderno", "Cortona", "Poggibonsi",
  /* Spain */
  "Pedrogao Grande", "Villarcayo", "Alosno", "La Bisbal", 
  /* UK */
  "Cold Norton", "Potten End", "Battlesbridge", 
  "Fawkham Green", "Ysbyty Ystwyth", "Bletchley",
  "Llanbrynmair", "St Keverne", "Foxholes", 
  "Whitby", "Sutton-on-Sea", "Tweedmouth", "Wrexham",
  "Kirkwall", "Blair Atholl", "Inchbare", "Blackwaterfoot", 
  "Ramsgate", "Llantwit Major", "Minehead", "Buckfastleigh", 
  "Pocklington", "Robin Hood's Bay", "West Kilbride",
  "Inchnadamph", "North Tolsta", "Oykel Bridge",
  "Pangbourne", "Moreton-in-Marsh", "Wye", "Congresbury",
  NULL
};

/**************************************************************************
  Set the worker on the citymap.  Also sets the worked field in the map.
**************************************************************************/
void set_worker_city(struct city *pcity, int x, int y, 
		     enum city_tile_type type) 
{
  struct tile *ptile=map_get_tile(pcity->x+x-2, pcity->y+y-2);
  if (pcity->city_map[x][y] == C_TILE_WORKER)
    ptile->worked = NULL;
  pcity->city_map[x][y]=type;
/* this function is called far less than is_worked here */
/* and these two ifs are a lot less CPU load then the iterates! */
  if (type == C_TILE_WORKER)
    ptile->worked = pcity;
}

/**************************************************************************
...
**************************************************************************/
enum city_tile_type get_worker_city(struct city *pcity, int x, int y)
{
  if ((x==0 || x==4) && (y == 0 || y == 4)) 
    return C_TILE_UNAVAILABLE;
  return(pcity->city_map[x][y]);
}

/**************************************************************************
...
**************************************************************************/
int is_worker_here(struct city *pcity, int x, int y) 
{
  if (x < 0 || x > 4 || y < 0 || y > 4 || ((x == 0 || x == 4) && (y == 0|| y==4))) {
    return 0;
  }
  return (get_worker_city(pcity,x,y)==C_TILE_WORKER); 
}

/**************************************************************************
  Convert map coordinate into position in city map
**************************************************************************/
int map_to_city_x(struct city *pcity, int x)
{
	int t=map.xsize/2;
	x-=pcity->x;
	if(x > t) x-=map.xsize;
	else if(x < -t) x+=map.xsize;
	return x+2;
}
int map_to_city_y(struct city *pcity, int y)
{
	return y-pcity->y+2;
}

/****************************************************************
...
*****************************************************************/
int wonder_replacement(struct city *pcity, enum improvement_type_id id)
{
  if(is_wonder(id)) return 0;
  switch (id) {
  case B_BARRACKS:
  case B_BARRACKS2:
  case B_BARRACKS3:
    if (city_affected_by_wonder(pcity, B_SUNTZU))
      return 1;
    break;
  case B_GRANARY:
    if (improvement_variant(B_PYRAMIDS)==0
	&& city_affected_by_wonder(pcity, B_PYRAMIDS))
      return 1;
    break;
  case B_CATHEDRAL:
    if (city_affected_by_wonder(pcity, B_MICHELANGELO))
      return 1;
    break;
  case B_CITY:  
    if (city_affected_by_wonder(pcity, B_WALL))
      return 1;
    break;
  case B_HYDRO:
  case B_POWER:
  case B_NUCLEAR:
    if (city_affected_by_wonder(pcity, B_HOOVER))
      return 1;
    break;
  case B_POLICE:
    if (city_affected_by_wonder(pcity, B_WOMENS))
      return 1;
    break;
  case B_RESEARCH:
    if (city_affected_by_wonder(pcity, B_SETI))
      return 1;
    break;
  default:
    break;
  }
  return 0;
}

char *get_imp_name_ex(struct city *pcity, enum improvement_type_id id)
{
  static char buffer[256];
  char state ='w';
  if (wonder_replacement(pcity, id)) {
    sprintf(buffer, "%s(*)", get_improvement_type(id)->name);
    return buffer;
  }
  if (!is_wonder(id)) 
    return get_improvement_name(id);

  if (game.global_wonders[id]) state='B';
  if (wonder_obsolete(id)) state='O';
  sprintf(buffer, "%s(%c)", get_improvement_type(id)->name, state); 
  return buffer;
}

/****************************************************************
...
*****************************************************************/

char *get_improvement_name(enum improvement_type_id id)
{
  return get_improvement_type(id)->name; 
}

/****************************************************************
...
*****************************************************************/

char *city_name_suggestion(struct player *pplayer)
{
  char **nptr;
  int i, j, k;
  static int n_misc=0;
  static char tempname[100];

  if (!n_misc) {
    for (i=0; misc_city_names[i]; i++) {}
    n_misc = i;
  }

  for(nptr=race_default_city_names[pplayer->race]; *nptr; nptr++) {
    if(!game_find_city_by_name(*nptr))
      return *nptr;
  }

  j = myrand(n_misc);
  for (i=0; i<n_misc; i++) {
    k = (i+j) % n_misc;
    if (!game_find_city_by_name(misc_city_names[k])) 
      return misc_city_names[k];
  }

  for (i = 0; i < 1000;i++ ) {
    sprintf(tempname, "city %d", i);
    if (!game_find_city_by_name(tempname)) 
      return tempname;
  }
  return "";
}


/**************************************************************************
...
**************************************************************************/

int improvement_value(enum improvement_type_id id)
{
  return (improvement_types[id].build_cost);
}

/**************************************************************************
...
**************************************************************************/

int is_wonder(enum improvement_type_id id)
{
  return (improvement_types[id].is_wonder);
}

/**************************************************************************
Returns 1 if the improvement_type "exists" in this game, 0 otherwise.
An improvement_type doesn't exist if one of:
- id is out of range;
- the improvement_type has been flagged as removed by setting its
  tech_requirement to A_LAST;
- it is a space part, and the spacerace is not enabled.
Arguably this should be called improvement_type_exists, but that's too long.
**************************************************************************/
int improvement_exists(enum improvement_type_id id)
{
  if (id<0 || id>=B_LAST)
    return 0;

  if ((id==B_SCOMP || id==B_SMODULE || id==B_SSTRUCTURAL)
      && !game.spacerace)
    return 0;

  return (improvement_types[id].tech_requirement!=A_LAST);
}

/**************************************************************************
Does a linear search of improvement_types[].name
Returns B_LAST if none match.
**************************************************************************/
enum improvement_type_id find_improvement_by_name(char *s)
{
  int i;

  for( i=0; i<B_LAST; i++ ) {
    if (strcmp(improvement_types[i].name, s)==0)
      return i;
  }
  return B_LAST;
}

/**************************************************************************
...
**************************************************************************/
int improvement_variant(enum improvement_type_id id)
{
  return improvement_types[id].variant;
}

/**************************************************************************
...
**************************************************************************/
int improvement_obsolete(struct player *pplayer, enum improvement_type_id id) 
{
  if (improvement_types[id].obsolete_by==A_NONE) 
    return 0;
  return (get_invention(pplayer, improvement_types[id].obsolete_by)
	  ==TECH_KNOWN);
}

/**************************************************************************
...
**************************************************************************/
  
int city_buy_cost(struct city *pcity)
{
  int total,cost;
  int build=pcity->shield_stock;

  if (pcity->is_building_unit) {
    total=unit_value(pcity->currently_building);
    if (build>=total)
      return 0;
    cost=(total-build)*2+(total-build)*(total-build)/20; 
  } else {
    total=improvement_value(pcity->currently_building);
    if (build>=total)
      return 0;
    cost=(total-build)*2;
    if(is_wonder(pcity->currently_building))
      cost*=2;
  }
  if(!build)
    cost*=2;
  return cost;
}

/**************************************************************************
...
**************************************************************************/

int wonder_obsolete(enum improvement_type_id id)
{ 
  if (improvement_types[id].obsolete_by==A_NONE)
    return 0;
  return (game.global_advances[improvement_types[id].obsolete_by]);
}

/**************************************************************************
...
**************************************************************************/

struct improvement_type *get_improvement_type(enum improvement_type_id id)
{
  return &improvement_types[id];
}

/**************************************************************************
...
**************************************************************************/

struct player *city_owner(struct city *pcity)
{
  return (&game.players[pcity->owner]);
}

/****************************************************************
...
*****************************************************************/

int could_build_improvement(struct city *pcity, enum improvement_type_id id)
{ /* modularized so the AI can choose the tech it wants -- Syela */
  if (!improvement_exists(id))
    return 0;
  if (city_got_building(pcity, id))
    return 0;
  if ((city_got_building(pcity, B_HYDRO)|| city_got_building(pcity, B_POWER) ||
      city_got_building(pcity, B_NUCLEAR)) && (id==B_POWER || id==B_HYDRO || id==B_NUCLEAR))
    return 0;
  if (id==B_RESEARCH && !city_got_building(pcity, B_UNIVERSITY))
    return 0;
  if (id==B_UNIVERSITY && !city_got_building(pcity, B_LIBRARY))
    return 0;
  if (id==B_STOCK && !city_got_building(pcity, B_BANK))
    return 0;
  if (id == B_SEWER && !city_got_building(pcity, B_AQUEDUCT))
    return 0;
  if (id==B_BANK && !city_got_building(pcity, B_MARKETPLACE))
    return 0;
  if (id==B_MFG && !city_got_building(pcity, B_FACTORY))
    return 0;
  if ((id==B_HARBOUR || id==B_COASTAL || id == B_OFFSHORE || id == B_PORT) && !is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN))
    return 0;
  if ((id == B_HYDRO || id == B_HOOVER)
      && !(map_get_terrain(pcity->x, pcity->y) == T_RIVER)
      && !(map_get_terrain(pcity->x, pcity->y) == T_MOUNTAINS)
      && !is_terrain_near_tile(pcity->x, pcity->y, T_MOUNTAINS)
      && !is_terrain_near_tile(pcity->x, pcity->y, T_RIVER))
    return 0;
  if (id == B_SSTRUCTURAL || id == B_SCOMP || id == B_SMODULE) {
    if (!game.global_wonders[B_APOLLO]) {
      return 0;
    } else {
      struct player *p=city_owner(pcity);
      if (p->spaceship.state >= SSHIP_LAUNCHED)
	return 0;
      if (id == B_SSTRUCTURAL && p->spaceship.structurals >= NUM_SS_STRUCTURALS)
	return 0;
      if (id == B_SCOMP && p->spaceship.components >= NUM_SS_COMPONENTS)
	return 0;
      if (id == B_SMODULE && p->spaceship.modules >= NUM_SS_MODULES)
	return 0;
    }
  }
  if (is_wonder(id)) {
    if (game.global_wonders[id]) return 0;
  } else {
    struct player *pplayer=city_owner(pcity);
    if (improvement_obsolete(pplayer, id)) return 0;
  }
  return !wonder_replacement(pcity, id);
}

int can_build_improvement(struct city *pcity, enum improvement_type_id id)
{
  struct player *p=city_owner(pcity);
  if (!improvement_exists(id))
    return 0;
  if (!player_knows_improvement_tech(p,id))
    return 0;
  return(could_build_improvement(pcity, id));
}

/****************************************************************
Whether given city can build given unit,
ignoring whether unit is obsolete.
*****************************************************************/
int can_build_unit_direct(struct city *pcity, enum unit_type_id id)
{  
  struct player *p=city_owner(pcity);
  if (!unit_type_exists(id))
    return 0;
  if (unit_flag(id, F_NUCLEAR) && !game.global_wonders[B_MANHATTEN])
    return 0;
  if (get_invention(p,unit_types[id].tech_requirement)!=TECH_KNOWN)
    return 0;
  if (!is_terrain_near_tile(pcity->x, pcity->y, T_OCEAN) && is_water_unit(id))
    return 0;
  return 1;
}

/****************************************************************
Whether given city can build given unit;
returns 0 if unit is obsolete.
*****************************************************************/
int can_build_unit(struct city *pcity, enum unit_type_id id)
{  
  if (!can_build_unit_direct(pcity, id))
    return 0;
  if (can_build_unit_direct(pcity, unit_types[id].obsoleted_by))
    return 0;
  return 1;
}

/**************************************************************************
...
**************************************************************************/

int city_population(struct city *pcity)
{
/*
  int i;
  int res=0;
  for (i=1;i<=pcity->size;i++) res+=i;
  return res*10000;
*/
  /*  Sum_{i=1}^{n} i  ==  n*(n+1)/2  */
  return pcity->size * (pcity->size+1) * 5000;
}

/**************************************************************************
...
**************************************************************************/

int city_got_building(struct city *pcity,  enum improvement_type_id id) 
{
  if (!improvement_exists(id))
    return 0;
  else 
    return (pcity->improvements[id]);
}

/**************************************************************************
...
**************************************************************************/
int improvement_upkeep(struct city *pcity, int i) 
{
  if (!improvement_exists(i))
    return 0;
  if (is_wonder(i))
    return 0;
  if (improvement_types[i].shield_upkeep == 1 &&
      city_affected_by_wonder(pcity, B_ASMITHS)) 
    return 0;
  return (improvement_types[i].shield_upkeep);
}

/**************************************************************************
  Caller to pass asmiths = city_affected_by_wonder(pcity, B_ASMITHS)
**************************************************************************/
static int improvement_upkeep_asmiths(struct city *pcity, int i, int asmiths) 
{
  if (!improvement_exists(i))
    return 0;
  if (is_wonder(i))
    return 0;
  if (asmiths && improvement_types[i].shield_upkeep == 1) 
    return 0;
  return (improvement_types[i].shield_upkeep);
}

/**************************************************************************
...
**************************************************************************/

int get_shields_tile(int x, int y, struct city *pcity)
{
  int s=0;
  enum tile_special_type spec_t=map_get_special(pcity->x+x-2, pcity->y+y-2);
  enum tile_terrain_type tile_t=map_get_terrain(pcity->x+x-2, pcity->y+y-2);

  int gov=get_government(pcity->owner);
  if (city_celebrating(pcity))
    gov+=2;
  if (spec_t & S_SPECIAL) 
    s=get_tile_type(tile_t)->shield_special;
  else
    s=get_tile_type(tile_t)->shield;

  if (x == 2 &&  y == 2 && !s) /* Always give a single shield on city square */
    s=1;
  
  if (spec_t & S_MINE) {
    s++;
    if (tile_t == T_HILLS) 
      s+=2;
  }
  if (spec_t & S_RAILROAD)
    s+=(s*game.rail_prod)/100;
  if (city_affected_by_wonder(pcity, B_RICHARDS))
    s++;
  if (city_got_building(pcity, B_OFFSHORE) && tile_t==T_OCEAN)
    s++;
  if (s>2 && gov <=G_DESPOTISM) 
    s--;
  if (spec_t & S_POLLUTION) return s=(s+1)/2; /* The shields here is icky */
  return s;
}

/**************************************************************************
...
**************************************************************************/

int get_trade_tile(int x, int y, struct city *pcity)
{
  enum tile_special_type spec_t=map_get_special(pcity->x+x-2, pcity->y+y-2);
  enum tile_terrain_type tile_t=map_get_terrain(pcity->x+x-2, pcity->y+y-2);
  int t;
  int gov=get_government(pcity->owner);
  if (city_celebrating(pcity)) 
    gov+=2;
 
  if (spec_t & S_SPECIAL) 
    t=get_tile_type(tile_t)->trade_special;
  else
    t=get_tile_type(tile_t)->trade;
    
  if (spec_t & S_ROAD) {
    switch (tile_t) {
    case T_DESERT:
    case T_GRASSLAND:
    case T_PLAINS:
      t++;
    default:
      break;
    }
  }
  if (t) {
    if (spec_t & S_RAILROAD)
      t+=(t*game.rail_trade)/100;

	/* Civ1 specifically documents that Railroad trade increase is before 
     * Democracy/Republic bonus  -AJS */

    if (gov >=G_REPUBLIC)
      t++; 

    if(city_affected_by_wonder(pcity, B_COLLOSSUS)) 
      t++;
    if((spec_t&S_ROAD) && city_got_building(pcity, B_SUPERHIGHWAYS))
      t*=1.5;
 
    if (t>2 && gov <=G_DESPOTISM) 
      t--;
    if (spec_t & S_POLLUTION)
      t=(t+1)/2; /* The trade here is dirty */
  }
  return t;
}

/***************************************************************
Here the exact food production should be calculated. That is
including ALL modifiers. 
Center tile acts as irrigated...
***************************************************************/

int get_food_tile(int x, int y, struct city *pcity)
{
  int f;
  enum tile_special_type spec_t=map_get_special(pcity->x+x-2, pcity->y+y-2);
  enum tile_terrain_type tile_t=map_get_terrain(pcity->x+x-2, pcity->y+y-2);
  struct tile_type *type;
  int gov=get_government(pcity->owner);

  type=get_tile_type(tile_t);

  if (city_celebrating(pcity))
    gov+=2;

  if (spec_t & S_SPECIAL) 
    f=get_tile_type(tile_t)->food_special;
  else
    f=get_tile_type(tile_t)->food;

  if (spec_t & S_IRRIGATION || (x==2 && y==2 && tile_t==type->irrigation_result)) {
    f++;
  }
  if (city_got_building(pcity, B_HARBOUR) && tile_t==T_OCEAN)
    f++;

  if (spec_t & S_RAILROAD)
    f+=(f*game.rail_food)/100;

  if (city_got_building(pcity, B_SUPERMARKET) && (spec_t & S_IRRIGATION))
    f *= 1.5;

  if (f>2 && gov <=G_DESPOTISM) 
    f--;

  if (spec_t & S_POLLUTION)
    f=(f+1)/2;

  return f;
}

/**************************************************************************
...
**************************************************************************/

int can_establish_trade_route(struct city *pc1, struct city *pc2)
{
  int i;
  int r=1;
  if(!pc1 || !pc2 || pc1==pc2 ||
     (pc1->owner==pc2->owner && 
      map_distance(pc1->x, pc1->y, pc2->x, pc2->y)<=8))
    return 0;
  
  for(i=0;i<4;i++) {
    r*=pc1->trade[i];
    if (pc1->trade[i]==pc2->id) return 0;
  }
  if (r) return 0;
  r = 1;
  for (i=0;i<4;i++) 
    r*=pc2->trade[i];
  return (!r);
}


/**************************************************************************
...
**************************************************************************/

int trade_between_cities(struct city *pc1, struct city *pc2)
{
  int bonus=0;

  if (pc2 && pc1) {
    bonus=(pc1->tile_trade+pc2->tile_trade+4)/8;
    if (map_get_continent(pc1->x, pc1->y) == map_get_continent(pc2->x, pc2->y))
      bonus/=2;
	
    if (pc1->owner==pc2->owner)
      bonus/=2;
  }
  return bonus;
}

/**************************************************************************
 Return number of trade route city has
**************************************************************************/
int city_num_trade_routes(struct city *pcity)
{
  int i,n;
  for(i=0,n=0; i<4; i++)
    if(pcity->trade[i]) n++;
  
  return n;
}

/*************************************************************************
Calculate amount of gold remaining in city after paying for buildings
*************************************************************************/

int city_gold_surplus(struct city *pcity)
{
  int asmiths = city_affected_by_wonder(pcity, B_ASMITHS);
  int cost=0;
  int i;
  for (i=0;i<B_LAST;i++) 
    if (city_got_building(pcity, i)) 
      cost+=improvement_upkeep_asmiths(pcity, i, asmiths);
  return pcity->tax_total-cost;
}


/**************************************************************************
 Whether a city has an improvement, or the same effect via a wonder.
 (The improvement_type_id should be an improvement, not a wonder.)
 Note also: city_got_citywalls(), and server/citytools:city_got_barracks()
**************************************************************************/
int city_got_effect(struct city *pcity, enum improvement_type_id id)
{
  return city_got_building(pcity, id) || wonder_replacement(pcity, id);
}


/**************************************************************************
 Whether a city has its own City Walls, or the same effect via a wonder.
**************************************************************************/
int city_got_citywalls(struct city *pcity)
{
  if (city_got_building(pcity, B_CITY))
    return 1;
  return (city_affected_by_wonder(pcity, B_WALL));
}


/**************************************************************************
...
**************************************************************************/
int city_affected_by_wonder(struct city *pcity, enum improvement_type_id id) /*FIX*/
{
  struct city *tmp;
  if (!improvement_exists(id))
    return 0;
  if (!is_wonder(id) || wonder_obsolete(id))
    return 0;
  if (city_got_building(pcity, id))
    return 1;
  
  /* For Manhatten it can be owned by anyone, and it doesn't matter
   * whether it is destroyed or not.
   *
   * (The same goes for Apollo, with respect to building spaceship parts,
   * but not for getting the map effect.  This function only returns true
   * for Apollo for the owner of a non-destroyed Apollo; for building
   * spaceship parts just check (game.global_wonders[id] != 0).
   * (Actually, this function is not currently used for either Manhatten
   * or Apollo.))
   *
   * Otherwise the player who owns the city needs to have it to
   * get the effect.
   */
  if (id==B_MANHATTEN) 
    return (game.global_wonders[id] != 0);
  
  tmp = player_find_city_by_id(get_player(pcity->owner),
			       game.global_wonders[id]);
  if (!tmp)
    return 0;
  switch (id) {
  case B_ASMITHS:
  case B_APOLLO:
  case B_CURE:
  case B_GREAT:
  case B_WALL:
  case B_HANGING:
  case B_ORACLE:
  case B_UNITED:
  case B_WOMENS:
  case B_DARWIN:
  case B_MAGELLAN:
  case B_MICHELANGELO:
  case B_SETI:
  case B_PYRAMIDS:
  case B_LIBERTY:
  case B_SUNTZU:
    return 1;
  case B_ISAAC:
  case B_COPERNICUS:
  case B_SHAKESPEARE:
  case B_COLLOSSUS:
  case B_RICHARDS:
    return 0;
  case B_HOOVER:
  case B_BACH:
    if (improvement_variant(id)==1) {
      return (map_get_continent(tmp->x, tmp->y) ==
	      map_get_continent(pcity->x, pcity->y));
    } else {
      return 1;
    }
  default:
    return 0;
  }
} 
/***************************************************************
...
***************************************************************/

int city_happy(struct city *pcity)
{
  return (pcity->ppl_happy[4]>=(pcity->size+1)/2 && pcity->ppl_unhappy[4]==0);
}

/**************************************************************************
...
**************************************************************************/
int city_unhappy(struct city *pcity)
{
  return (pcity->ppl_happy[4]<pcity->ppl_unhappy[4]);
}

int city_celebrating(struct city *pcity)
{
  return (pcity->size>=5 && pcity->was_happy/* city_happy(pcity)*/);
}

/* The find_city_by_id() code has returned from its trip to server land and
 * lives in common once again.  There are two ways find_city_by_id() works. 
 * If citycachesize!=0, it uses a fast (and potentially large, 256K) lookup
 * table to find the city.  If citycachesize==0, it uses a slow linear
 * searching method.  citycachesize is set by calling initialize_city_cache(),
 * which code that wants to use the fast method should do.  Currently the
 * server uses the cache, and the client doesn't.  */

static struct city **citycache = NULL;
static int citycachesize = 0;

/**************************************************************************
  Initialize the cache.  This enables the fast find_city_by_id() code.  Once
  enabled, the cache has to be kept consistent by calling add_city_to_cache()
  and remove_city_from_cache() as appropriate.
**************************************************************************/
void initialize_city_cache(void)
{
  int i;

  if(citycache) free(citycache);
  citycachesize=128;
  citycache=fc_malloc(sizeof(*citycache) * citycachesize);
  for(i=0;i<citycachesize;i++) citycache[i]=NULL;
}

/**************************************************************************
  Double the size of the cache.  add_city_to_cache() calls this when needed.
**************************************************************************/
static void reallocate_cache(void)
{
  int i;

  freelog(LOG_DEBUG,"Increasing max city id index from %d to %d",
       citycachesize,citycachesize*2);
  citycachesize*=2;
  citycache=fc_realloc(citycache,sizeof(*citycache)*citycachesize);
  for(i=citycachesize/2;i<citycachesize;i++)  citycache[i]=NULL;
}

/**************************************************************************
  Add the city pointer to the lookup table.  If the calling program isn't
  using the cache, it's still safe to call this function.
**************************************************************************/
void add_city_to_cache(struct city *pcity)
{
  if(!citycachesize) return;		/* Not using the cache, return */
  if(pcity->id < citycachesize)  {
    citycache[pcity->id]=pcity;
  } else {
    reallocate_cache();
    add_city_to_cache(pcity);
  }
}

/**************************************************************************
  Remove the city pointer from the lookup table.  If the calling program isn't
  using the cache, it's still safe to call this function.
**************************************************************************/
void remove_city_from_cache(int id)
{
  if(!citycachesize) return;		/* Not using the cache, return */
  if(id >= citycachesize) {
    freelog(LOG_FATAL, "Tried to delete bogus city id, %d",id);
    exit(0);
  }
  citycache[id]=NULL;
}

/**************************************************************************
  The slow method for find_city_by_id(), searches though the city lists in
  linear time.  Used in the client, which doesn't need to lookup city IDs that
  often.
**************************************************************************/
static struct city *find_city_by_id_list(int city_id)
{
  int i;
  struct city *pcity;

  for(i=0; i<game.nplayers; i++)
    if((pcity=city_list_find_id(&game.players[i].cities, city_id)))
      return pcity;

  return 0;
}

/**************************************************************************
  Often used function to get a city pointer from a city ID.  This uses either
  a fast lookup table, or a slow list searching method.
**************************************************************************/
struct city *find_city_by_id(int id)
{
  /* This is sometimes called with id=unit.ai.charge, which is either
   * a unit id or a city id; if its a unit id then that id won't be used
   * for a city (?), so that is ok, except that it is possible that it
   * might exceed citycachesize. --dwp
   */
  if(!citycachesize) return find_city_by_id_list(id);
  if(id<0 || id>=citycachesize) return NULL;
  return citycache[id];
}


/**************************************************************************
...
**************************************************************************/
void city_list_init(struct city_list *This)
{
  genlist_init(&This->list);
}

/**************************************************************************
...
**************************************************************************/
struct city *city_list_get(struct city_list *This, int index)
{
  return (struct city *)genlist_get(&This->list, index);
}


/**************************************************************************
...
**************************************************************************/
struct city *city_list_find_id(struct city_list *This, int id)
{
  if(id) {
    struct genlist_iterator myiter;

    genlist_iterator_init(&myiter, &This->list, 0);
    
    for(; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter))
    if(((struct city *)ITERATOR_PTR(myiter))->id==id)
	return ITERATOR_PTR(myiter);
  }
  return 0;
}

/**************************************************************************
...
**************************************************************************/
struct city *city_list_find_name(struct city_list *This, char *name)
{
  struct genlist_iterator myiter;

  genlist_iterator_init(&myiter, &This->list, 0);

  for(; ITERATOR_PTR(myiter); ITERATOR_NEXT(myiter))
    if(!mystrcasecmp(name, ((struct city *)ITERATOR_PTR(myiter))->name))
      return ITERATOR_PTR(myiter);

  return 0;
}


/**************************************************************************
...
**************************************************************************/
void city_list_insert(struct city_list *This, struct city *pcity)
{
  genlist_insert(&This->list, pcity, 0);
}


/**************************************************************************
...
**************************************************************************/
int city_list_size(struct city_list *This)
{
  return genlist_size(&This->list);
}

/**************************************************************************
...
**************************************************************************/
void city_list_unlink(struct city_list *This, struct city *pcity)
{
  genlist_unlink(&This->list, pcity);
}

/**************************************************************************
...
**************************************************************************/
void city_list_insert_back(struct city_list *This, struct city *pcity)
{
  genlist_insert(&This->list, pcity, -1);
}

/**************************************************************************
Comparison function for qsort for city _pointers_, sorting by city name.
Args are really (struct city**), to sort an array of pointers.
(Compare with old_city_name_compare() in game.c, which use city_id's)
**************************************************************************/
int city_name_compare(const void *p1, const void *p2)
{
  return mystrcasecmp( (*(struct city**)p1)->name,
		       (*(struct city**)p2)->name );
}
