#include "Test.h"
#include <stdio.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

JNIEXPORT jobject JNICALL Java_Test_get
(JNIEnv *env, jclass cl)
{
  printf("Wurstbombe\n");
  return 0;
}

struct oopDesc;
struct klassOopDesc;
typedef struct oopDesc* oop;
typedef struct klassOopDesc* klassOop;

struct oopDesc {
  void *header;
  klassOop  klass;
};

struct klassOopDesc {
  struct oopDesc oop;
  void **vtable;
  int  layout_helper;
  int  super_check_offset;
};

struct Format {
  long base;
  char header[1000];
  char data[];
};

typedef struct Format *Formatp;

void dump(void *buffer, int size)
{
  int f = open("output", O_WRONLY|O_TRUNC|O_CREAT);
  write(f, buffer, size);
  close(f);
}

int sizeOf(oop o) 
{
  klassOop k = o->klass;
  int(*method)(void*,oop) = k->vtable[0xa0];
  return method(&k->layout_helper, o);
}

klassOop *unwrap_java_class(jclass clazz)
{
  oop *javaKlassOop = clazz;
  return ((void*)*javaKlassOop) + sizeof(struct oopDesc);
}


JNIEXPORT jobject JNICALL Java_Test_persist
  (JNIEnv *env, jclass cl, jobject o)
{
  oop *objectOop = o;
  oop *javaKlassOop = (*env)->GetObjectClass(env, o);
  oop *klassOop = unwrap_java_class(javaKlassOop);
  //dump((*javaKlassOop), 100);
  //int s = (*javaKlassOop)->klass->layout_helper;
  //printf("OOP: klass: 0x%lx size: %d 0->klass: %lx realKlass: %lx\n", *javaKlassOop, s, (*objectOop)->klass, *klassOop);
  
  //printf("Objectinfo: %lx %d %d\n", (*objectOop)->klass->vtable, sizeOf(*objectOop), (*objectOop)->klass->layout_helper);
  //printf("Klassinfo: %lx %d %d\n", (*klassOop)->klass->vtable, sizeOf(*klassOop), (*klassOop)->klass->layout_helper);

  long page = sysconf(_SC_PAGE_SIZE);

  int f = open("bla", O_RDWR);
  printf("after creat page: %ld file: %d\n", page, f);
  Formatp data = mmap(NULL, page, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
  printf("after mmap\n");
  data->base = data;
  memcpy(data->header, *klassOop, 500);
  memcpy(data->data, *((void**)o), 50);
  msync(data, page, MS_SYNC);
  //munmap(buffer, page);
  //close(f);
  oop newOop = data->data;
  newOop->klass = data->header;

  void** ret = (void**) (*env)->NewLocalRef(env, data->data);
  *ret = data->data;
  return o;
}

JNIEXPORT jobject JNICALL Java_Test_load
  (JNIEnv *env, jclass recv, jclass inClass)
{
  long page = sysconf(_SC_PAGE_SIZE);

  int f = open("bla", O_RDONLY);
  long pos;
  int res = read(f, &pos, sizeof(long));
  printf("Read returned %d\n", res);
  Formatp data = mmap(pos, page, PROT_READ|PROT_WRITE, MAP_PRIVATE, f, 0);
  printf("after mmap buffer: %lx\n", data);
  
  memcpy(data->header, *unwrap_java_class(inClass), 500);
  
  void** ret = (void**) (*env)->NewLocalRef(env, data->data);
  *ret = data->data;
  return ret;
}
