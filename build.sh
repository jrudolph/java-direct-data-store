javac Test.java Person.java &&
javah Test &&
gcc -I/usr/lib/jvm/default-java/include \
  -I/usr/lib/jvm/default-java/include/linux \
  -lssl \
  -shared -fPIC -o libtest.so -g dump.c reloc.c &&
LD_LIBRARY_PATH=. java Test
