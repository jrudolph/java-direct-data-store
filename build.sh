javac Test.java Person.java &&
javah Test &&
gcc -I/usr/lib/jvm/default-java/include -I/usr/lib/jvm/default-java/include/linux -shared -fPIC -o libtest.so -g Test.c &&
LD_LIBRARY_PATH=. java Test
