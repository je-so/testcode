Types
=====

(function-, data-, class-, ... types)

1. We need type extensions (categories, fragments) 
   * so that language types could be extended.
2. IO category
   * Needed for marking types as invalidated input: (zip,street)'invalidated-input.
   * Or for marking types as I/O actions
3. action category
   * Marking types as part of (trans-)actions which have side effects
3. memory category
   * Marking types as allocated on what heap? Session, request, transactional heap...
5. state category
   * Marking types as opened,closed,(un-)initialized or partly-initialized,
   * or in init-phase where invariants are not valid ...
   * Types during init-phase should be able to develop like an embryo
6. units for physical quantity
   * A number type should be extendable by 'km, or 'µs and so on.
   * Support for overloading like:
     `setTimeout(number'ns)`, `setTimeout(number'µs)`
     or more generic `setTimeout(number'@unit) { setTimeout(number as int'ns); }`
   
Multi Dimensional
=================

Sometimes functions need to return more than one data channel.
The most prominent is the error channel.

But we - as programmers - need only a single deterministic value. We need to collapse 
*multidimensional* information back to a single value.

```
var value = (function (tuple) { if (tuple.error) return defaultValue; else tuple.result }) (parse(strValue));
```

Flow of error and valid data information are two dimensions. There are more...
The *Time* to measure the max. duration of an operation (and to coordinate concurrent behaviour) is another one.

And - as you know - if you intermix dimensions in any non orthogonal way you get into trouble cause of side effects lurking in one dimension to effect another one cause of non  orthogonal combination of behaviour.

