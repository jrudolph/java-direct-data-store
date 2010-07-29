#include "Test.h"
#include <stdio.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct oopDesc;
struct klassOopDesc;
struct oopArrayOopDesc;
typedef struct oopDesc* oop;
typedef struct klassOopDesc* klassOop;
typedef struct oopArrayOopDesc* oopArrayOop;

struct oopDesc {
  void *header;
  klassOop  klass;
  char data[];
};

struct klassOopDesc {
  struct oopDesc oop;
  void **vtable;
  int  layout_helper;
  int  super_check_offset;
};

struct oopArrayOopDesc {
  struct oopDesc oop;
  long length;
  char data[];
};

struct Format {
  long base;
  char header[1000];
  char data[];
};

typedef struct Format *Formatp;

void assert(int cond, char *msg)
{
  if (!cond) printf("Assertion failed: %s\n", msg);
}

int size_of_instance(oop o)
{
  int size = o->klass->layout_helper;
  assert(size != 0, "Not an instance");
  return size;
}

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
  printf("Read returned %d\n", sizeof(void*));
  Formatp data = mmap(pos, page, PROT_READ|PROT_WRITE, MAP_PRIVATE, f, 0);
  printf("after mmap buffer: %lx\n", data);
  
  memcpy(data->header, *unwrap_java_class(inClass), 500);
  
  void** ret = (void**) (*env)->NewLocalRef(env, data->data);
  *ret = data->data;
  return ret;
}

typedef void(*Iterator)(oop,long);

int is_oop(void *ptr)
{
  return ptr > 0x7f0000000000;
}

struct ArrayInfo {
  unsigned char element_size;
  unsigned char type;
  unsigned char offset;
  unsigned char tag;  
};
#define ARRAY_TAG_MASK 0xff000000
#define OOPS_ARRAY 0x80000000
// see klass.hpp for explanation of layout_helper field
int is_oop_array(oop o)
{
  return (size_of_instance(o) & ARRAY_TAG_MASK) == OOPS_ARRAY;
}

void iterate_over_fields(oop root, void *arg, Iterator it)
{
  printf("Iterating over 0x%lx\n", root);

  int size = size_of_instance(root);
  if (size == 0) {
    printf("Not an instance: 0x%lx", root);
    return;
  }
  else if (is_oop_array(root))
  {
    printf("Found array: 0x%lx\n", root);
    iterate_over_oop_array(root, arg, it);
    return;
  }
  it(root, arg);
  
  void **end = ((void*)root) + size;
  
  void **cur = root->data;
  while (cur < end) {
    printf("Now at %lx\n", cur);
  
    if (is_oop(*cur)) {
      it(*cur, arg);
      iterate_over_fields(*cur, arg, it);
    }
    cur += 1;
  }
}

void iterate_over_oop_array(oopArrayOop array, void *arg, Iterator it)
{
  struct ArrayInfo *info = &array->oop.klass->layout_helper;
  dump(array, 100);
  printf("ArrayInfo: tag=0x%x offset=%d type=%d element_size=%d\n",
    info->tag,
    info->offset,
    info->type,
    info->element_size);
  
  int size = array->length;
  int i;
  for (i = 0; i < size; i++) {
    oop *ele = array->data + (i << info->element_size);
    printf("Element %d 0x%lx data=0x%lx\n", i, *ele, array->data);
    it(*ele, arg);
  }  
}

void print_oop(oop o,void *l)
{
  printf("--- Found object: 0x%lx\n", o);
}

JNIEXPORT void JNICALL Java_Test_analyze
  (JNIEnv *env, jclass clazz, jobject o)
{
  oop *myO = o;
  iterate_over_fields(*myO, 0, print_oop);
}
