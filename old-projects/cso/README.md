# This Repository will be deleted

I will begin to work on the "dynamic data model" (see [js-projekt](https://github.com/je-so/js-projekt))
which replaces the functionality covered in this project. 

# cso

An easy C module to serialize objects into a simple binary format.

Use it to store data or to send it over a network.
It supports either an object of type struct (CSO_DICT) or array (CSO_ARRAY)
which contains elements of type integer of different sizes, double, struct or array.

If you want to know more about the interface, take a look at [src/cso.h](src/cso.h).

Type <code> > make test</code> to run the unit test.

The unit test is located in the implementation file.
See function "int unittest_cso(void)" at the bottom of [src/cso.c](src/cso.c).
The unit test is included only if CONFIG_UNITTEST is defined.
The test driver is located in [src/test.c](src/test.c).

## Documentation

Use the comments in [src/cso.h](src/cso.h) as documentation. Also a short description of the used binary format is included.

## Dependencies

CSO should run on platforms which support a compatible C99 compiler.
The module uses no additional libraries but depends on nonstandard endian functions be32toh, le32toh, ...
to convert integer values into the byte order of the host.
Replace them with your own functions if your platform does not support &lt;endian.h&gt;.

The module is only tested on a 32 bit Linux system.

### License

This software is licensed under [MIT license](http://www.opensource.org/licenses/mit-license.php).
