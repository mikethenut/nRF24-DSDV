# BSO Projekt

Navodila (Tema 14): Izdelajte graf sosedov in usmerjevalni algoritem za prenos podatkov z uporabo modula nRF24L01.

## Struktura

Ker je cilj izdelati usmerjevalni algoritem, je potrebno postaviti tri stvari:

- Vsaka naprava se mora znati identificirati (določiti svoj id).
- Naprave morajo dobiti lokalno ali globalno sliko omrežja.
- Na podlagi teh informacij morajo določiti način pošiljanja podatkov do katerekoli druge naprave v omrežju.

**Trenutna ideja**: Uporaba algoritma **DSDV**, kjer vozlišče najprej določi svoje sosede in to informacijo posreduje ostalim. Na podlagi izmenjanih podatkov potem zgradi oz. posodobi usmerjevalno tabelo. Predhodni postopek določanja lastnega id-ja algoritem ne specificira.

**Opombe**: Poročilo naj bi obsegalo 10 strani. Obrazloži je treba vsako odločitev med izdelovanjem projekta, t.j. ozadje problema, pogoste rešitve, razlog za izbrano rešitev, ter njene podrobnosti. Najti je treba tudi druge načine "preverjanja" algoritma,  npr. uporaba funkcij, ki "prejmejo" fake podatke in spremljanje delovanja naprave.

## Viri
- https://nrf24.github.io/RF24/index.html
- https://github.com/joshua-jerred/DSDV
- https://github.com/liudongdong1/DSDV
- https://github.com/thllwg/efficient-dsdv
- https://github.com/lukeflima/DSDV
- https://github.com/elliamlee/newDSDV

## Poročilo + predstavitev
Overleaf links:

- [poročilo](https://www.overleaf.com/6596639163jwhqjdrcwqfy)
- [predstavitev](https://www.overleaf.com/8587598478djbvvvrvsdsh).
