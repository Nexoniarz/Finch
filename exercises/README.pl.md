# Ćwiczenia z Fincha

Ucz się Fincha w praktyce: każdy plik to małe zadanie z testami, które je sprawdzają.

```sh
exercises/next.sh          # pierwsze nierozwiązane ćwiczenie i co mówią jego testy
exercises/next.sh all      # ile już masz
finch test exercises/03_if_else.fch     # jedno ćwiczenie
```

Otwórz ćwiczenie, przeczytaj kilka linii na górze, zmieniaj kod oznaczony `TODO`, aż testy przejdą, i uruchom
`exercises/next.sh` jeszcze raz. Nie zmieniaj bloków `test`: to one sprawdzają. Gdy utkniesz, w `exercises/solutions/`
jest jedno rozwiązanie każdego (często są też inne). Komentarze w ćwiczeniach są po angielsku; ściąga po polsku to
`docs/pl/sciaga.md`.

| | Temat |
|---|---|
| 01–04 | wartości, funkcje, `if`, pętle |
| 05–07 | listy, tekst, struktury i metody |
| 08–11 | `mut`, mapy, błędy (`!`, `or`, `try`), wartości, których może nie być (`T?`) |
| 12–15 | `enum` i `match`, funkcje jako wartości, interfejsy, typy generyczne |
| 16–18 | asembler (x86-64): rejestry, kilka wartości, pętla |
