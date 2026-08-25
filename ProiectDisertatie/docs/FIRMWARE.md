# Firmware ESP32 — funcționalitate completă

Documentație tehnică a firmware-ului care controlează cele două capete pan/tilt ale
sistemului de urmărire stereoscopică.

Fișiere acoperite: `src/main.cpp`, `include/config.h`, `platformio.ini`,
`lib/SpeedStepper/`.

---

## Cuprins

| Secțiune | Conținut |
|---|---|
| 1 | Rolul firmware-ului în sistem |
| 2 | Platformă hardware și configurare de build |
| 3 | Arhitectura de execuție — două nuclee, două sarcini |
| 4 | Starea partajată și accesul concurent |
| 5 | Modelul de date al unui motor |
| 6 | Automatul de stări — vedere de ansamblu |
| 7 | Procedura de referențiere (homing) |
| 8 | Bariera de sincronizare |
| 9 | Parcarea și generarea zeroului mecanic |
| 10 | Regulatorul de poziție |
| 11 | Regulatorul de viteză |
| 12 | Comutarea între moduri |
| 13 | Conectarea la rețea și mecanismul de rezervă |
| 14 | Descoperirea stației de control |
| 15 | Recepția comenzilor |
| 16 | Fluxul de telemetrie |
| 17 | Protocolul de comunicație |
| 18 | Mecanismele de siguranță |
| 19 | Diagnosticul pe portul serial |
| 20 | Biblioteca SpeedStepper |
| 21 | Tabelul complet de constante |
| 22 | Limitări cunoscute |

---

## 1. Rolul firmware-ului în sistem

Firmware-ul este stratul de execuție al sistemului. Nu ia nicio decizie de urmărire și nu
efectuează niciun calcul geometric — acestea aparțin în întregime aplicației Qt de pe PC.

Responsabilitățile lui sunt patru:

1. **Referențierea mecanică** — aducerea celor patru axe într-o origine repetabilă la
   fiecare pornire.
2. **Execuția comenzilor de mișcare**, în două regimuri distincte: viteză (pentru
   urmărire) și poziție (pentru testare manuală).
3. **Raportarea telemetriei** — poziția fiecărei axe, la 50 Hz.
4. **Siguranța locală** — limite software de cursă, temporizare a comenzilor expirate,
   protecția contorului de pași.

Împărțirea responsabilităților este deliberată: PC-ul are camerele, puterea de calcul și
algoritmul; microcontrolerul are temporizarea în timp real a pașilor. Nici unul nu poate
face treaba celuilalt.

---

## 2. Platformă hardware și configurare de build

### Ținta

| Parametru | Valoare |
|---|---|
| Placă | ESP32 DOIT DevKit v1 |
| Platformă PlatformIO | espressif32 |
| Framework | Arduino |
| Partiționare | `huge_app.csv` |
| Viteză port serial | 115200 baud |
| Port de monitorizare | identificat prin VID:PID `10C4:EA60` (punte CP210x) |
| Filtre monitor | colorize, time, esp32_exception_decoder |

Partiționarea `huge_app.csv` este necesară pentru că stiva WiFi ocupă mult spațiu de
program; renunță la partiția OTA în favoarea unei singure imagini de aplicație mai mari.

Filtrul `esp32_exception_decoder` traduce automat adresele din urmele de stivă în nume de
funcții și numere de linie, ceea ce face diagnosticarea unei excepții direct citibilă în
monitorul serial.

### Cablarea motoarelor

| Motor | Rol | Pin STEP | Pin DIR | Pin limitator |
|---|---|---|---|---|
| m1Pan | gimbal 1 — panoramare | 26 | 33 | 15 |
| m1Tilt | gimbal 1 — înclinare | 25 | 32 | 4 |
| m2Pan | gimbal 2 — panoramare | 27 | 21 | 19 |
| m2Tilt | gimbal 2 — înclinare | 14 | 22 | 23 |

Două constrângeri de pini sunt deja respectate și nu trebuie desfăcute:

- **GPIO 15 este pin de strapping** și trebuie să citească HIGH la resetare. `INPUT_PULLUP`
  îl menține acolo, dar dacă limitatorul respectiv este apăsat fizic la alimentare, pinul
  este tras la masă și placa poate să nu pornească. Pinii 16 și 17 sunt liberi ca
  alternativă.
- **Pinii DIR sunt ținuți în afara intervalului GPIO 34–39.** Aceștia sunt exclusiv de
  intrare pe ESP32, deci `digitalWrite` pe ei este o operație fără efect, executată în
  tăcere — axa s-ar roti doar într-un singur sens, fără niciun mesaj de eroare.

---

## 3. Arhitectura de execuție — două nuclee, două sarcini

ESP32 are două nuclee. Firmware-ul le folosește pentru a separa complet două activități cu
cerințe temporale incompatibile.

```
Nucleul 0                          Nucleul 1
─────────                          ─────────
netTask (prioritate 2)             stepperTask (prioritate 2)
  · conectare WiFi                   · automate de stări
  · recepție UDP                     · generare impulsuri de pas
  · flux de telemetrie               · diagnostic serial
  · cedează CPU (vTaskDelay)         · NU cedează CPU niciodată
```

Sarcinile sunt create în `setup()` cu `xTaskCreatePinnedToCore`:

| Sarcină | Nucleu | Prioritate | Stivă |
|---|---|---|---|
| netTask | 0 | 2 | 8192 octeți |
| stepperTask | 1 | 2 | 4096 octeți |

Funcția `loop()` din Arduino nu face nimic — doar `vTaskDelay(1000)`.

### De ce această separare

Generarea impulsurilor de pas este o activitate **dură în timp real**: intervalul dintre
doi pași trebuie respectat cu precizie, altfel motorul pierde pași sau vibrează. Stiva
WiFi, în schimb, este imprevizibilă — poate bloca execuția zeci de milisecunde pentru
retransmisii sau scanări de canal.

Dacă ambele ar rula pe același nucleu, fiecare pachet WiFi ar perturba temporizarea pașilor.

Bucla din `stepperTask` **nu cedează niciodată procesorul** — nu conține `vTaskDelay`. Este o buclă de ocupare
completă a nucleului 1, ceea ce este intenționat: `SpeedStepper::run()` trebuie apelat cât
mai des posibil, pentru că el decide la fiecare apel dacă a trecut suficient timp pentru
următorul impuls. Cu cât este apelat mai des, cu atât temporizarea este mai fină.

Consecința este că nucleul 1 este ocupat permanent. Acesta este motivul pentru care sarcina
de rețea are nucleul 0 în exclusivitate.

Suplimentar, `WiFi.setSleep(false)` dezactivează economia de energie a radioului imediat
după conectare. Fără această linie, ESP32 intră periodic în repaus și adaugă latențe de
ordinul sutelor de milisecunde comenzilor de viteză — inacceptabil pentru o buclă de
urmărire închisă prin rețea.

---

## 4. Starea partajată și accesul concurent

Cele două sarcini comunică prin cinci variabile globale:

```cpp
volatile int32_t  g_speed[4];      // deg/s, scrise de rețea, citite de motoare
volatile int32_t  g_target[4];     // grade, scrise de rețea, citite de motoare
volatile bool     g_homed;         // scris de motoare, citit de rețea
volatile uint8_t  g_mode;          // scris de rețea, citit de motoare
volatile uint32_t g_lastSpeedMs;   // momentul ultimei comenzi de viteză
```

**Nu există niciun mutex.** Justificarea, documentată în sursă, este că fiecare dintre
aceste variabile este un cuvânt de 32 de biți aliniat natural în memorie. Pe această
arhitectură, citirea și scrierea unui astfel de cuvânt se fac într-o singură instrucțiune,
deci nu există momentul în care un cititor ar putea observa o valoare pe jumătate scrisă.

Aceasta este o garanție de **atomicitate**, nu de ordonare. Ea este suficientă aici pentru
că fiecare variabilă are un singur scriitor și pentru că nu există invariante care să lege
două variabile între ele. Nu este o licență generală de a renunța la sincronizare.

Calificativul `volatile` împiedică optimizatorul să păstreze valoarea într-un registru
între iterații — fără el, compilatorul ar putea „deduce" corect că bucla de pe nucleul 1 nu
modifică `g_speed` și ar citi valoarea o singură dată.

---

## 5. Modelul de date al unui motor

Fiecare axă este descrisă de trei structuri suprapuse:

```cpp
struct motorConfig {          // constant, definit la compilare
    SpeedStepper* st;         // driverul propriu-zis
    int       endstopPin;
    int32_t   rangeSteps;     // cursa maximă, în pași
    int       dir;            // sensul de căutare a limitatorului
    float     homeSpeed, homeAccel;
    float     moveSpeed, moveAccel;
    int32_t   endstopClearance;
    int8_t    deadband;       // zonă moartă a regulatorului de poziție
    float_t   Kp;
    float     parkDeg;        // unghiul de parcare, în grade de la origine
};

struct motorState {           // variabil în timpul execuției
    bool        homed;
    motor_State state;
};

struct Motor {
    const motorConfig cfg;
    motorState        state;
};
```

Cele patru axe sunt instanțiate într-un tablou `motors[]`, iar `MOTOR_COUNT` este dedus
automat din dimensiunea lui. Ordinea din tablou este **legea sistemului** și se regăsește
identic în protocolul de comunicație și în aplicația Qt:

| Index | Axă |
|---|---|
| 0 | gimbal 1 — panoramare |
| 1 | gimbal 1 — înclinare |
| 2 | gimbal 2 — panoramare |
| 3 | gimbal 2 — înclinare |

Separarea între configurație constantă și stare variabilă permite adăugarea unui al treilea
gimbal prin simpla extindere a tabloului, fără nicio modificare de logică.

---

## 6. Automatul de stări — vedere de ansamblu

Fiecare axă parcurge independent următoarea secvență:

```
   H_APPROACH ──► H_BACKOFF ──► H_DONE ──►│barieră│──► H_PARK ──► T_MOVEMENT
                                                                       ▲
                                                                       │
                                                                       ▼
                                                                   TEST_MODE
```

| Stare | Regim | Descriere |
|---|---|---|
| H_APPROACH | viteză | caută limitatorul de capăt de cursă |
| H_BACKOFF | viteză | se retrage din limitator până la o distanță sigură |
| H_DONE | oprit | comută pe profilul de mișcare și așteaptă celelalte axe |
| H_PARK | poziție | se deplasează în poziția de lucru |
| T_MOVEMENT | viteză | regim de urmărire — primește deg/s de la PC |
| TEST_MODE | poziție | regim manual — primește grade de la potențiometrele din interfață |

Tranzițiile sunt gestionate de două funcții:

- `motorEnter(Motor&, motor_State)` — execută acțiunile **de intrare** în stare
  (reconfigurare de viteze, accelerații, limite) și tipărește tranziția.
- `motorRun(Motor&, target, speed)` — execută comportamentul **continuu** al stării, apelat
  la fiecare iterație a buclei.

Separarea acțiunilor de intrare de comportamentul continuu este ceea ce permite ca
reconfigurarea profilului de mișcare să se facă exact o dată, la tranziție, și nu de mii de
ori pe secundă.

---

## 7. Procedura de referențiere (homing)

Motoarele pas cu pas nu au traductor de poziție. La alimentare, contorul de pași pornește
de la zero indiferent de unde se află fizic axa. Referențierea rezolvă acest lucru
căutând un reper fizic cunoscut.

### Faza 1 — H_APPROACH

La intrare:

```cpp
setMaxSpeed(homeSpeed);                        // 400 pași/s
setAcceleration(homeAccel);                    // 250 pași/s²
setMinusLimit(-MAX_INT32_T);                   // limite deschise complet
setPlusLimit(MAX_INT32_T);
setSpeed(dir * homeSpeed);                     // pornește spre limitator
```

Limitele sunt deschise larg deoarece în acest moment poziția reală este necunoscută — orice
limită ar fi arbitrară și ar putea opri axa înainte de a atinge reperul.

Comportament continuu: se testează limitatorul la fiecare iterație. La declanșare:

```cpp
stopAndSetHome();                              // oprire bruscă + poziția devine 0
setMinusLimit(getCurrentPosition());           // adică 0 — nu mai poate intra în limitator
```

Funcția `endstopHit` compară cu `HIGH`, iar pinii sunt configurați `INPUT_PULLUP`. Logica
implică deci **comutatoare normal închise** legate la masă: în repaus circuitul este închis
și pinul citește LOW; la apăsare circuitul se deschide și rezistența internă trage pinul la
HIGH. Este configurația sigură la defect — un fir rupt se manifestă ca limitator apăsat,
oprind axa, nu ca limitator liber.

### Faza 2 — H_BACKOFF

Axa se retrage în sens invers, la jumătate din viteza de referențiere:

```cpp
setSpeed(-dir * homeSpeed * 0.5f);
```

Retragerea continuă până când poziția depășește `ENDSTOP_CLEARANCE` (160 pași, adică 18°),
moment în care se apelează din nou `stopAndSetHome()`. **Această a doua oprire definește
poziția 0 a axei.**

De ce două zerouri: prima oprire este pe contactul mecanic al comutatorului, care are
histerezis și nu este un punct repetabil la nivel de pas. Retragerea lentă și oprirea la o
distanță fixă de contact produc un reper mult mai stabil, și în plus lasă axa liberă — dacă
originea ar rămâne chiar pe comutator, orice mișcare negativă l-ar reactiva.

La finalul acestei faze se incrementează `homedCount`.

### Faza 3 — H_DONE

```cpp
stop();
setPlusLimit(rangeSteps);                      // limita superioară reală
setMaxSpeed(moveSpeed);                        // 1000 pași/s
setAcceleration(moveAccel);                    // 500 pași/s²
```

Aici se comută de pe profilul lent de referențiere pe profilul rapid de lucru. Limita
negativă rămâne la 0 din faza anterioară, iar cea pozitivă este acum cursa reală a axei
(1777 pași pentru panoramare, 533 pentru înclinare).

Din acest moment axa este **protejată software** la ambele capete.

---

## 8. Bariera de sincronizare

Starea H_DONE nu se auto-tranziționează. Ea așteaptă:

```cpp
if (homedCount == MOTOR_COUNT) {
    motorEnter(m, H_PARK);
}
```

Aceasta este o **barieră**: nicio axă nu începe parcarea până când toate patru nu și-au
găsit limitatorul.

Motivul este mecanic. Capetele se pot atinge între ele sau pot atinge structura dacă se
rotesc simultan în combinații nefavorabile. Cât timp o axă încă mai caută orbește
limitatorul, poziția ei este necunoscută, deci nicio altă axă nu se poate mișca în siguranță.

Există o a doua barieră, mai fină, la ieșirea din parcare:

```cpp
parkedCount++;
if (parkedCount == MOTOR_COUNT) {
    g_homed = true;
}
```

Distincția dintre cele două contoare este semnificativă: o axă este **referențiată** din
momentul în care și-a găsit limitatorul, dar nu este **gata de lucru** până când nu a și
parcat. Aplicația Qt condiționează triangulația de `g_homed`, iar valorile de poziție citite
în timpul parcării nu au nicio semnificație geometrică.

---

## 9. Parcarea și generarea zeroului mecanic

Parcarea aduce fiecare axă din poziția 0 (punctul de retragere din limitator) în poziția de
lucru, definită de `M*_PARK_DEG`.

| Axă | Unghi de parcare | Echivalent în pași |
|---|---|---|
| panoramare | 75° | 667 |
| înclinare | 30° | 267 |

### De ce nu se re-inițializează contorul

Parcarea trece **dincolo** de poziția 0 și **nu** rezetează contorul de pași. Aceasta este o
decizie deliberată: contorul din poziția parcată reprezintă un unghi real față de reperul
mecanic, iar acel număr este exact valoarea pe care aplicația Qt o folosește ca punct de
plecare pentru `zeroSteps`.

Dacă s-ar re-inițializa contorul la parcare, informația despre relația dintre reperul fizic
și poziția de lucru s-ar pierde definitiv.

### Condiția de ieșire — de ce poziția singură nu este suficientă

```cpp
bool atRest = fabsf(getSpeed()) < 1.0f;
if (atRest && iabs32(parkSteps - getCurrentPosition()) <= deadband) { ... }
```

Regulatorul de poziție comandă `viteză = Kp · eroare`, cu Kp = 3. Urmărirea exactă a acestui
profil ar necesita o decelerație de aproximativ 9 ori eroarea — circa 900 pași/s² la o
eroare de 100 pași — față de accelerația configurată de 500 pași/s².

Axa ajunge deci în zona moartă **încă în mișcare**. Dacă ieșirea din stare s-ar face numai
pe criteriul de poziție, axa ar fi predată stării T_MOVEMENT, care ar comanda viteza 0, iar
axa ar rula liber câteva zeci de pași dincolo de țintă.

Consecința practică, observată înainte de corecție: panoramarea se oprea la 755 de pași
într-o rulare și la 694 în următoarea. Cu așteptarea vitezei, **fiecare rulare parchează la
(694, 240, 694, 240)**. Această repetabilitate este singurul motiv pentru care poziția
parcată poate servi drept bază pentru calibrarea geometrică.

### Protecția împotriva unui unghi de parcare invalid

```cpp
int32_t parkSteps = lroundf(parkDegI * stepsPerDegree);
parkSteps = constrain(parkSteps, 0, m.cfg.rangeSteps);
```

Această limitare replică exact limitarea internă a regulatorului de poziție. Dacă
`PARK_DEG` ar fi configurat dincolo de cursa axei, motorul s-ar opri la `rangeSteps` în timp
ce ținta nelimitată ar rămâne mai departe — testul zonei moarte nu ar fi îndeplinit
niciodată, starea H_PARK nu s-ar încheia, iar `g_homed` ar rămâne fals la nesfârșit, fără
niciun mesaj de eroare. Este o blocare tăcută, evitată prin trei linii de cod.

---

## 10. Regulatorul de poziție

Funcția `positionControl(Motor&, int32_t targetDegrees)` este folosită de două stări:
H_PARK și TEST_MODE.

```cpp
targetSteps = lroundf(targetDegrees * stepsPerDegree);
targetSteps = constrain(targetSteps, 0, rangeSteps);

error = targetSteps - getCurrentPosition();

if (|error| <= deadband) {          // 48 pași = 5,4°
    setSpeed(0);
    return;
}

setSpeed(constrain(Kp * error, -moveSpeed, +moveSpeed));
```

Este un **regulator proporțional pur**, fără componentă integrală sau derivativă. Viteza
scade pe măsură ce axa se apropie de țintă, ceea ce produce o oprire lină fără a fi nevoie
de un profil de mișcare planificat.

Caracteristici:

| Aspect | Valoare | Observație |
|---|---|---|
| Zonă moartă | 48 pași (5,4°) | grosieră, dar suficientă pentru poziționare manuală |
| Câștig Kp | 3,0 | la eroare 48 rezultă 144 pași/s; la eroare 100, 300 pași/s |
| Saturație | ±1000 pași/s | plafonul bibliotecii |
| Sensul de rotație | din semnul erorii | nu se folosește `cfg.dir` |

Zona moartă de 5,4° este mare pentru o măsurătoare, dar acest regulator nu participă la
măsurătoare — urmărirea folosește regimul de viteză, cu propria zonă moartă, mult mai
strânsă, calculată pe PC.

---

## 11. Regulatorul de viteză

Starea T_MOVEMENT este regimul de urmărire. Nu conține nicio buclă de reglare — bucla
închisă este realizată integral pe PC, care compară poziția mingii cu centrul cadrului și
trimite direct o viteză.

```cpp
int32_t cmd = speedDegS;

if (millis() - g_lastSpeedMs > SPEED_TIMEOUT_MS) {   // 300 ms
    cmd = 0;
}

float stepsPerSec = cmd * stepsPerDegree;
setSpeed(constrain(stepsPerSec, -moveSpeed, +moveSpeed));
```

### Temporizarea comenzii — de ce este obligatorie

O comandă de viteză este un **ordin permanent**. Spre deosebire de o comandă de poziție,
care se auto-încheie la atingerea țintei, o viteză diferită de zero rămâne valabilă la
infinit.

Dacă aplicația de pe PC se închide, se blochează sau pierde conexiunea WiFi, ultima viteză
comandată ar continua să fie executată până când axa lovește limita software. Cu
`SPEED_TIMEOUT_MS` = 300, o comandă mai veche de 300 ms este interpretată ca zero.

Aceasta este o siguranță de tip *dead man's switch*: sistemul se oprește prin **absența**
comunicației, nu prin prezența unei comenzi de oprire care ar putea, la rândul ei, să se
piardă.

### Conversia unităților

PC-ul trimite grade pe secundă, nu pași pe secundă. Conversia se face aici, folosind
`stepsPerDegree`, ceea ce înseamnă că aplicația Qt nu trebuie să cunoască rezoluția
mecanică. Dacă micropășirea driverelor se schimbă, se modifică o singură constantă în
`config.h` și nimic pe PC.

Nu se aplică `cfg.dir` în această conversie: sensul de rotație provine din semnul comenzii,
exact cum în regulatorul de poziție provine din semnul erorii. Constanta `dir` are un singur
rol în tot firmware-ul — sensul de căutare a limitatorului în timpul referențierii.

### Plafonul de viteză

Valoarea `M*_MOVE_SPEED` = 1000 pași/s nu este arbitrară: biblioteca SpeedStepper limitează
intervalul minim între pași la 1000 µs, deci 1000 pași/s este maximul absolut atingabil.
Aceasta corespunde la 112,5°/s.

În practică, plafonul care lucrează efectiv este cel de pe PC — 30°/s, adică 267 pași/s.
Limita din firmware este o plasă de siguranță, nu limita de lucru.

---

## 12. Comutarea între moduri

La începutul funcției `motorRun`:

```cpp
if (m.state.state == T_MOVEMENT || m.state.state == TEST_MODE) {
    motor_State wanted = (g_mode == MODE_TEST) ? TEST_MODE : T_MOVEMENT;
    if (m.state.state != wanted) {
        motorEnter(m, wanted);
    }
}
```

Trei observații importante:

1. **Comutarea este posibilă doar din stările operaționale.** Condiția exterioară verifică
   explicit că axa se află deja în T_MOVEMENT sau TEST_MODE. O axă aflată în referențiere
   sau parcare ignoră complet comenzile de mod — nu se poate întrerupe o procedură de
   inițializare printr-un pachet de rețea.

2. **Comportamentul este asimetric.** Expresia este
   `(g_mode == MODE_TEST) ? TEST_MODE : T_MOVEMENT`, deci **orice** valoare diferită de 1
   duce în regimul de urmărire. Un octet corupt nu poate bloca sistemul într-un mod
   necunoscut; cazul implicit este cel funcțional.

3. **Tranziția se execută o singură dată**, la schimbare, datorită testului
   `m.state.state != wanted`.

| Valoare `g_mode` | Constantă | Stare rezultată |
|---|---|---|
| 0 | MODE_TRACK | T_MOVEMENT |
| 1 | MODE_TEST | TEST_MODE |
| oricare alta | — | T_MOVEMENT |

---

## 13. Conectarea la rețea și mecanismul de rezervă

Firmware-ul cunoaște două rețele WiFi, definite într-un tablou:

```cpp
struct WiFiCred { const char* ssid; const char* password; const char* remoteIp; };
static const WiFiCred WIFI_NETWORKS[] = { {...}, {...} };
```

Funcția `connectWiFi()` le încearcă în ordine:

```
pentru fiecare rețea:
    WiFi.begin(ssid, parolă)
    așteaptă până la WIFI_TIMEOUT_MS (5000 ms), verificând la fiecare 100 ms
    dacă s-a conectat:
        memorează adresa de destinație
        WiFi.setSleep(false)          ← dezactivează economia de energie
        returnează succes
    WiFi.disconnect()
returnează eșec
```

Dacă ambele eșuează, `netTask` reia întreaga secvență după o pauză de 2 secunde, la
nesiguranță. Sistemul nu pornește niciodată fără rețea, dar nici nu se blochează — continuă
să încerce.

Existența unei a doua rețele este o măsură practică: prima este rețeaua de laborator, a doua
este o rețea mobilă de rezervă, pentru demonstrații în locuri unde prima nu există.

Așteptarea se face cu `vTaskDelay`, nu cu `delay()`, ceea ce cedează procesorul altor
sarcini în loc să-l ocupe într-o buclă goală.

### Reconectarea automată

Bucla principală din `netTask` verifică la fiecare iterație:

```cpp
if (WiFi.status() != WL_CONNECTED) {
    udp.stop();
    streaming = false;              ← fluxul de telemetrie se oprește
    while (!connectWiFi()) { vTaskDelay(2000); }
    udp.begin(UDP_PORT);
}
```

Oprirea fluxului este esențială: după reconectare, adresa IP poate fi diferită, iar
destinatarul precedent nu mai este valid. Transmisia se reia doar după un nou dialog de
identificare inițiat de PC.

**Notă:** variabila `g_streamTarget`, completată în `connectWiFi` din câmpul `remoteIp`, nu
este citită nicăieri. Destinația fluxului provine exclusiv din adresa expeditorului
dialogului de identificare. Este cod rezidual dintr-o versiune anterioară cu adresă fixă.

---

## 14. Descoperirea stației de control

Nici PC-ul, nici ESP32 nu cunosc dinainte adresa celuilalt. Descoperirea se face prin
difuziune:

```
1. PC ──[difuziune "Connected", 9 octeți]──► toată subrețeaua
2. ESP32 recepționează, memorează IP-ul și portul expeditorului
3. ESP32 ──["Connected"]──► direct către expeditor
4. ESP32 activează fluxul de telemetrie către acel expeditor
5. PC memorează adresa ESP32 și trece la comunicație directă
```

Pe partea ESP32:

```cpp
if (n == 9 && memcmp(buf, "Connected", 9) == 0) {
    peerIp     = udp.remoteIP();
    peerPort   = udp.remotePort();
    streaming  = true;
    lastStream = millis();
    // răspuns cu același mesaj
}
```

Retrimiterea mesajului de identificare reinițializează complet asocierea, ceea ce face ca
schimbarea rețelei sau repornirea PC-ului să nu necesite repornirea robotului.

Portul de răspuns este citit din pachet (`udp.remotePort()`), nu presupus, ceea ce
funcționează corect chiar dacă PC-ul folosește un port sursă efemer.

---

## 15. Recepția comenzilor

Bucla de rețea verifică la fiecare iterație dacă există un pachet în așteptare:

```cpp
int sz = udp.parsePacket();
if (sz > 0) {
    int n = udp.read(buf, sizeof(buf));    // buf are 64 de octeți
    ...
}
```

Primul octet este identificatorul de mesaj, interpretat într-o structură `switch`.

### MSG_TARGET (identificator 2)

```cpp
if (n >= 9) {
    memcpy(&pan,  buf + 1, 4);
    memcpy(&tilt, buf + 5, 4);
    g_target[0] = pan;
    g_target[1] = tilt;
}
```

Valorile sunt în **grade** și alimentează regimul TEST_MODE.

Limitare de semnalat: se scriu doar indicii 0 și 1. Indicii 2 și 3 rămân la valoarea de
inițializare, zero, deci în regimul de testare gimbalul 2 este comandat spre poziția 0.

### MSG_MODE (identificator 3)

```cpp
if (n >= 2) { g_mode = buf[1]; }
```

### MSG_SPEED (identificator 4)

```cpp
uint8_t count = buf[1];
if (count > MOTOR_COUNT) { count = MOTOR_COUNT; }     // limitare defensivă
if (n >= 2 + count * 4) {                              // verificare de lungime
    pentru fiecare i < count:
        memcpy(&v, buf + 2 + i * 4, 4);
        g_speed[i] = v;
    g_lastSpeedMs = millis();                          // marcaj de prospețime
}
```

Două verificări defensive:

- Numărul de motoare declarat este limitat la `MOTOR_COUNT`, deci un pachet care pretinde
  opt motoare nu poate scrie în afara tabloului.
- Lungimea reală a pachetului este verificată față de numărul declarat, deci un pachet
  trunchiat este ignorat, nu interpretat parțial.

Marcajul temporal `g_lastSpeedMs` se actualizează **numai** dacă pachetul a fost acceptat
integral. Un pachet respins nu prelungește valabilitatea comenzii anterioare.

### Utilizarea `memcpy`

Extragerea numerelor întregi se face cu `memcpy`, nu prin conversie de pointeri. Motivul
este alinierea: adresa `buf + 1` este impar aliniată, iar o citire directă a unui `int32_t`
de la o adresă nealiniată este comportament nedefinit și poate genera o excepție pe unele
arhitecturi. `memcpy` este, în plus, optimizat de compilator la aceeași instrucțiune atunci
când alinierea permite.

---

## 16. Fluxul de telemetrie

```cpp
if (streaming && (millis() - lastStream) >= UDP_STREAM_RATE_MS) {
    lastStream += UDP_STREAM_RATE_MS;              // acumulare, nu reasignare
    ...
}
```

Rata este de un pachet la 20 ms, adică **50 Hz**.

Detaliu de temporizare: `lastStream += UDP_STREAM_RATE_MS` în loc de
`lastStream = millis()`. Prima variantă păstrează o bază de timp fixă și nu acumulează
derivă, chiar dacă o iterație întârzie. A doua ar adăuga la fiecare ciclu întârzierea
reziduală, iar frecvența reală ar scădea sub 50 Hz.

Construcția pachetului:

```cpp
out[0] = MSG_MOTOR_POS;                            // 0x01
out[1] = MOTOR_COUNT;                              // 0x04
pentru fiecare motor i:
    memcpy(out + 2 + i*4, &positions[i], 4);       // int32, little-endian
out[2 + count*4] = g_homed ? 1 : 0;
```

Rezultă 19 octeți pentru patru motoare.

Includerea indicatorului `g_homed` în același pachet cu pozițiile este intenționată: cele
două informații sunt inseparabile semantic. O poziție fără indicatorul de referențiere nu
poate fi interpretată, iar transmiterea lor separată ar introduce posibilitatea ca PC-ul să
le asocieze greșit.

---

## 17. Protocolul de comunicație

Port UDP 1234 în ambele sensuri. Toate numerele multi-octet sunt **little-endian**.

| ID | Nume | Sens | Lungime | Conținut |
|---|---|---|---|---|
| 1 | MOTOR_POS | ESP → PC | 2 + 4N + 1 | număr motoare, N × int32 poziție, octet referențiere |
| 2 | TARGET | PC → ESP | 9 | int32 panoramare, int32 înclinare (grade) |
| 3 | MODE | PC → ESP | 2 | octet de mod |
| 4 | SPEED | PC → ESP | 2 + 4N | număr motoare, N × int32 (grade/s) |

Structura pachetului MOTOR_POS pentru N = 4:

```
[0]      0x01                identificator
[1]      0x04                număr de motoare
[2..5]   int32 LE            gimbal 1 panoramare, pași
[6..9]   int32 LE            gimbal 1 înclinare, pași
[10..13] int32 LE            gimbal 2 panoramare, pași
[14..17] int32 LE            gimbal 2 înclinare, pași
[18]     0x00 / 0x01         g_homed
```

### Legea protocolului

Valorile identificatorilor sunt definite **de două ori**, în două fișiere care nu se văd
reciproc:

- `ProiectDisertatie/include/config.h`
- `Qt-WindowsApp/backend/Network/UdpClient.h`

Nu există negociere de versiune și nu există sumă de control. O nepotrivire între cele două
nu produce nicio eroare — pur și simplu pachetele sunt ignorate în tăcere. Orice modificare
trebuie făcută simultan în ambele fișiere.

### De ce UDP și nu TCP

Datele transmise sunt **eșantioane periodice ale unei mărimi care se schimbă continuu**.
Un pachet pierdut este înlocuit 20 ms mai târziu de unul mai nou și mai bun.

TCP ar retransmite pachetul pierdut și ar întârzia toate pachetele ulterioare până la
livrarea lui — exact comportamentul opus celui dorit. Într-o buclă de reglare, un eșantion
vechi livrat cu întârziere este mai dăunător decât un eșantion lipsă.

---

## 18. Mecanismele de siguranță

Rezumat al tuturor protecțiilor implementate, cu cauza fiecăreia.

| Mecanism | Implementare | Protejează împotriva |
|---|---|---|
| Limită software negativă | `setMinusLimit(0)` după retragere | rotirii înapoi în limitator; a salvat contorul de pași într-un incident real de deplasare necontrolată |
| Limită software pozitivă | `setPlusLimit(rangeSteps)` în H_DONE | depășirii cursei mecanice |
| Temporizare comandă viteză | `SPEED_TIMEOUT_MS` = 300 ms | rulării la infinit dacă PC-ul dispare |
| Barieră de referențiere | `homedCount == MOTOR_COUNT` | coliziunii între capete în timpul inițializării |
| Barieră de parcare | `parkedCount == MOTOR_COUNT` | publicării unor poziții fără semnificație geometrică |
| Limitare defensivă a numărului de motoare | `if (count > MOTOR_COUNT)` | scrierii în afara tabloului dintr-un pachet malformat |
| Verificare de lungime | `if (n >= 2 + count * 4)` | interpretării unui pachet trunchiat |
| Limitare a țintei de parcare | `constrain(parkSteps, 0, rangeSteps)` | blocării permanente în H_PARK cu `g_homed` fals |
| Blocarea comutării de mod | condiția din `motorRun` | întreruperii referențierii printr-un pachet de rețea |
| Mod implicit sigur | `(g_mode == MODE_TEST) ? ... : T_MOVEMENT` | blocării într-un mod necunoscut la un octet corupt |
| Comutatoare normal închise | logica `endstopHit` | firului rupt interpretat ca limitator liber |
| Dezactivarea economiei de energie | `WiFi.setSleep(false)` | latențelor imprevizibile în bucla de reglare |

Principiul comun: **absența unui semnal trebuie să ducă la oprire, nu la continuare.**

---

## 19. Diagnosticul pe portul serial

Toate mesajele periodice provin din `stepperTask` și sunt limitate ca frecvență, pentru a nu
satura portul serial și pentru a nu perturba temporizarea pașilor.

| Etichetă | Perioadă | Conținut |
|---|---|---|
| `[speed]` | 200 ms | viteza reală a celor patru axe |
| `[dir]` | 500 ms | sensul de rotație al fiecărei axe |
| `[track]` | 500 ms | vitezele comandate și pozițiile curente |
| `[backoff]` | 200 ms | poziție, distanță de siguranță, diferență — doar în H_BACKOFF |
| `[park]` | o dată | poziția finală de parcare, în pași și în grade |
| `[homing]`, `[net]`, `[boot]` | la eveniment | tranziții de stare și evenimente de rețea |

Mesajele folosesc coduri ANSI de culoare, definite în `config.h`, corelate cu severitatea:
cyan pentru tranziții de stare, verde pentru confirmări, roșu pentru diagnosticul retragerii.

Mesajul `[park]` este cel mai valoros dintre toate — el raportează exact numărul de pași la
care s-a oprit fiecare axă, iar acest număr este punctul de plecare pentru calibrarea
geometrică de pe PC.

---

## 20. Biblioteca SpeedStepper

Bibliotecă externă (Forward Computing and Control, derivată din AccelStepper al lui Mike
McCauley), inclusă local în `lib/SpeedStepper/`. Implementează ecuațiile lui David Austin
pentru generarea profilelor de viteză în timp real.

### Diferența față de AccelStepper

AccelStepper este orientat pe **poziție**: îi dai o destinație și el calculează profilul.
SpeedStepper este orientat pe **viteză**: îi dai o viteză țintă și el accelerează spre ea
respectând limita de accelerație.

Această orientare este exact ce cere sistemul. Bucla de urmărire produce o viteză
proporțională cu eroarea de imagine, nu o poziție țintă — nu se știe unde va fi mingea, ci
doar în ce direcție și cât de repede trebuie să se miște capul.

### Metode utilizate din API

| Metodă | Rol în firmware |
|---|---|
| `setSpeed(float)` | comanda principală, în pași/s, cu semn |
| `getSpeed()` | viteza reală — folosită la testul de repaus din parcare |
| `run()` | generează impulsurile; apelată în bucla principală |
| `stop()` | decelerare controlată până la oprire |
| `stopAndSetHome()` | oprire bruscă și inițializare a contorului la 0 |
| `getCurrentPosition()` | contorul de pași — sursa telemetriei |
| `setMinusLimit` / `setPlusLimit` | limitele software de cursă |
| `setMaxSpeed` / `setAcceleration` | comutarea între profilul de referențiere și cel de lucru |
| `isDirForward()` | diagnostic |

### Caracteristici de reținut

- Este **cu buclă deschisă**, prin calcul incremental. Nu există niciun traductor de
  poziție. Contorul de pași este singura sursă de adevăr despre poziție, iar dacă motorul
  pierde pași mecanic, firmware-ul nu are cum să afle.
- Intervalul minim între pași este limitat intern la 1000 µs, deci viteza maximă absolută
  este 1000 pași/s.
- `setSpeed` cu o valoare sub `minSpeed` este interpretată ca zero.
- Deceleratia până la limitele de poziție este calculată automat — axa începe să frâneze
  singură la apropierea de o limită.

Această caracteristică de buclă deschisă este justificarea centrală pentru toate protecțiile
din secțiunea 18: **contorul de pași este singurul reper, deci coruperea lui ar invalida
întreaga măsurătoare** până la o nouă referențiere.

---

## 21. Tabelul complet de constante

### Mecanice

| Constantă | Valoare | Semnificație |
|---|---|---|
| STEPS_PER_REV | 3200 | pași pe rotație (micropășire 1/16) |
| stepsPerDegree | 8,8889 | derivat, 3200/360 |
| M1/M3_RANGE_DEG | 200° | cursa de panoramare |
| M2/M4_RANGE_DEG | 60° | cursa de înclinare |
| M1/M3_RANGE_STEPS | 1777 | derivat |
| M2/M4_RANGE_STEPS | 533 | derivat |
| M1/M3_PARK_DEG | 75° | unghi de parcare, panoramare |
| M2/M4_PARK_DEG | 30° | unghi de parcare, înclinare |
| ENDSTOP_CLEARANCE | 160 pași | 18° de retragere din limitator |

### Mișcare (identice pe toate cele patru axe)

| Constantă | Valoare | Unitate |
|---|---|---|
| HOMING_SPEED | 400 | pași/s (45°/s) |
| HOMING_ACCEL | 250 | pași/s² |
| MOVE_SPEED | 1000 | pași/s (112,5°/s) — plafonul bibliotecii |
| MOVE_ACCEL | 500 | pași/s² |
| MIN_SPEED | 0,5 | pași/s |
| DIR_M1..M4 | −1 | sensul de căutare a limitatorului |
| Kp | 3,0 | regulator de poziție |
| DEADBAND | 48 | pași (5,4°) |

### Temporizări

| Constantă | Valoare | Semnificație |
|---|---|---|
| WIFI_TIMEOUT_MS | 5000 | timp de așteptare per rețea înainte de trecerea la rezervă |
| UDP_STREAM_RATE_MS | 20 | perioada telemetriei (50 Hz) |
| SPEED_TIMEOUT_MS | 300 | vechimea maximă a unei comenzi de viteză |
| CONTROL_PERIOD_US | 1000 | declarat, neutilizat în versiunea curentă |

### Protocol

| Constantă | Valoare |
|---|---|
| UDP_PORT | 1234 |
| SERIAL_BAUD | 115200 |
| MSG_MOTOR_POS | 1 |
| MSG_TARGET | 2 |
| MSG_MODE | 3 |
| MSG_SPEED | 4 |
| MODE_TRACK | 0 |
| MODE_TEST | 1 |

---

## 22. Limitări cunoscute

| Limitare | Locație | Efect |
|---|---|---|
| TARGET scrie doar indicii 0 și 1 | `netTask`, cazul MSG_TARGET | în regimul de testare, gimbalul 2 este comandat spre poziția 0, nu spre poziția de parcare. De verificat pe hardware înainte de o demonstrație în regim manual. |
| Variabilă `static` partajată în H_BACKOFF | cazul H_BACKOFF | temporizatorul mesajului de diagnostic este unul singur pentru toate cele patru axe, deci în fiecare fereastră de 200 ms se afișează o singură axă, nu toate |
| `g_streamTarget` nefolosită | `connectWiFi` | cod rezidual; destinația reală provine din dialogul de identificare |
| `CONTROL_PERIOD_US` nefolosită | `config.h` | constantă declarată fără utilizare |
| Buclă fără cedare pe nucleul 1 | `stepperTask` | ocupare permanentă a nucleului; intenționată pentru precizia temporizării, dar poate declanșa supraveghetorul sarcinii inactive în anumite configurații |
| Credențiale WiFi în clar | `config.h` | două perechi SSID/parolă sunt stocate în text simplu într-un fișier urmărit de sistemul de versionare. Înainte de publicarea depozitului, acestea trebuie mutate într-un antet neurmărit sau în definiții de compilare, iar parolele schimbate. |
| Fără sumă de control pe protocol | ambele capete | un pachet corupt care trece de verificarea de lungime este acceptat ca valid |
| Fără traductor de poziție | arhitectural | pașii pierduți mecanic nu pot fi detectați; singura corecție este o nouă referențiere |
