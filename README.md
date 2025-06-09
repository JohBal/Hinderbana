# Hinderbana

Bibliotek:
- Adafruit_SSD1306@2.5.14
- TinyStepper_28BYJ_48@1.0.0
- Servo@1.2.2

Underhållsfunktioner:
I icke-aktivt läge:
  Kan de två översta knapparna tryckas ner samtidigt för att hissen ska flytta upp
  Kan de två nedersta knapparna tryckas ner samtidigt för att hissen ska flytta ner
  <s>Uppdateras basljusnivån automatisk varje sekund ifall ljusförhållanderna skulle förändras (se dock till att ljussensorn inte får för mycket smuts över sig)</s>
I fall servomotorerna fastnar kan alla knappar tryckas ner för att servomotorerna ska flytta till uppåt och sedan neråt position

Hur koden fungerar:
Får aktiv signal
Flyttar hissen upp
Aktiverar servomotorerna
Väntar tills bollen rullat av
Flyttar hissen ner
Väntar tills timer når 40 s eller ljussensorn får hög signal
Låser servomoterna

Nuvarande problem:
Utåt sladdarna är mycket korta
Hissen kan lätt fastna
Sladdarna bakom knappanordningen kan blockera hissen
OLED skärmen har slutat fungera
Det översta hindret måste flyttas ner
(Ev.) Den översta banan måste omplaceras
