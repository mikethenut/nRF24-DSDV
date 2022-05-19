# BSO Projekt

Navodila (Tema 14): Izdelajte graf sosedov in usmerjevalni algoritem za prenos podatkov z uporabo modula nRF24L01.



## Struktura

Ker je cilj izdelati usmerjevalni algoritem, je potrebno postaviti tri stvari:

- Vsaka naprava se mora znati identificirati (določiti svoj id).
- Naprave morajo dobiti lokalno ali globalno sliko omrežja.
- Na podlagi teh informacij morajo določiti način pošiljanja podatkov do katerekoli druge naprave v omrežju.

**Trenutna ideja**: Uporaba algoritma **DVR** in njegove nadgradnje **DSDV**. Vozlišče najprej določi svoje sosede in tabelo sosedov posreduje ostalim. Na podlagi tega potem zgradi usmerjevalno tabelo. Predhodni postopek določanja svojega id-ja algoritem ne specificira.

**Opombe**: Poročilo naj bi obsegalo 10 strani. Verjetno bova do takšnega velikega obsega prišla le z obrazložitvijo vsake odločitve med izdelovanjem projekta. Drugače rečeno, za vsak del kode bo potrebno opisati ozadje problema, pogoste rešitve, razlog za izbrano rešitev, ter njene podrobnosti. Posledično lahko v poročilu opiševa več algoritmov, s katerimi potem primerjava izbranega. Zaradi pomanjkanja časa in dostopa do naprav verjetno ne bova imela časa preizkušati algoritmov, kar pa v navodilih piše da je predvideno. Iskati morava tudi druge načine "preverjanja" algoritma. V program lahko npr. spiševa funkcije, ki "prejmejo" fake podatke in vidiva, kaj naprava stori na podlagi tega.



## Viri

- https://github.com/thllwg/efficient-dsdv
- https://github.com/lukeflima/DSDV
- https://github.com/elliamlee/newDSDV



## Poročilo + predstavitev

Overleaf links:

- [poročilo](https://www.overleaf.com/6596639163jwhqjdrcwqfy)
- [predstavitev](https://www.overleaf.com/8587598478djbvvvrvsdsh).