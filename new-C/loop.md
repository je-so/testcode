
### Schleifen

Wiederholung einer vorher bestimmten Anzahl von Durchläufen:

```C
for (int i; i < 10; ++i) {
   ...
}
```

Wiederholung einer durch eine Bedingung abhängige Anzahl von Durchläufen:

```C
// === while

{  int i = 0; /* lokale Initialisierungsblock, genau wie bei for */ 
   char c;
}  while ( (c=str[i]) != 0;  ++i, ...) {
                          // ^ <next>-Instruktionen wie bei for
   ...
}

// === do while

{  int i = 0; /* lokale Initialisierungsblock, genau wie bei for */ 
   char c = str[i];
}  do {
                          
   ...
} while ( c != 0;  ++i, ...);
                   // ^ <next>-Instruktionen wie bei for wird **vor** der Bedingung ausgewertet 
```

### Lokale Initialisierungsblock

Lokale Initialsierungsblöcke dürfen vor den reservierten Kommandos **for, if, do, while** stehen.


```C
{  char c;
}  if ( (c == *str) == 'a' ) {
      ... // Verwendung von C
   } else if (c == 'b') {
      ... // Verwendung von C
   } else {
      ... // Verwendung von C
   }
}
```



