Gramtropy: a grammar-based password generator
=============================================

https://github.com/sipa/gramtropy

What is Gramtropy
-----------------

Gramtropy is a tool that can generate passwords or passphrases taken uniformly
from a set specified by an unambiguous
[Context-free Grammar](https://en.wikipedia.org/wiki/Context-free_grammar).

It aims to solve the problem of generated passwords that are pronouncable
according to arbitrary rules, while simultaneously guaranteeing a given
[security level](https://en.wikipedia.org/wiki/Security_level) (in bits).

Building
--------

You need the C++11 compiler from GCC 4.7 or later. It likely works with other
C++11 compilers, but I haven't tested it.

Run `make`. Two binaries should be produced, `gramc` and `gram`. The first is a
compiler that takes a grammar file and a security level, and produces a translation
file. The second interprets a translation file to generate passphrases and more.

Usage
-----

Create a file `simple.gram` with the following contents:

```
vowel = "a"|"e"|"i"|"o"|"u";
consonant = "b"|"d"|"f"|"g"|"h"|"k"|"l"|"m"|"n"|"p"|"r"|"s|"t"|"v"|"w"|"z";
convow = consonant vowel;
main = convow+;
```

This grammar specifies what vowels and consonants are, and that valid phrases
(`main`) consist of one or more consonant-vowel pairs.

You can convert it to a translation file using

```
$ ./gramc simple.gram simple.gtp
Using length range 22..22
Result: 2E90EDD00000000000 combinations (69.5412 bits)
```

which converts it into a translation file `simple.gtr`. By default, a security
level of 64 bits is used. The compiler determined that using just phrases of
length 22 was sufficient to reach over 2<sup>69</sup> combinations, satisfying
the 64 bit security level requirement.

Now you can use the `simple.gtp` file to generate a password:

```
$ ./gram simple.gtp
godudamuhasomifohikuwi
```

If you want a different security level, specify it using `-b`:

```
$ ./gramc -b 128 simple.gram simple.gtp
Using length range 42..42
Result: 1B1AE4D6E2EF5000000000000000000000 combinations (132.76 bits)
```

Example grammars
----------------

Demonstation:
* [Simple demo](grammars/demo.gram): Explains the syntax with a slightly more
  complicated version of the grammar above.

Pronouncable passwords:
* [English](grammars/pronouncable.gram)
* [Dutch](grammars/dutch.gram)

Passphrases:
* [Version 1](grammars/silly.gram)
* [Version 2](grammars/english.gram)

Less serious:
* [Breezy](grammars/breezy.gram)
* [Fail mail](grammars/failmail.gram)
