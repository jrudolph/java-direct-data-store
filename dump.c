#include "Test.h"
#include <stdio.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "dump.h"
#include "reloc.h"

struct oopDesc {
  void *header;
  klassOop  klass;
  char data[];
};

struct KlassVtbl {
  void *reserved[20];
  int(*oop_size)(pKlass*,oop);    // 0x14
  void *reserved2[96];
  char*(*internal_name)(pKlass*); // 117
};

struct klassOopDesc {
  struct oopDesc oop;
  pKlass vtbl;
  int  layout_helper;
  int  super_check_offset;
};

struct oopArrayOopDesc {
  struct oopDesc oop;
  long length;
  char data[];
};

struct ClassInfo {
  struct ClassInfo *next;
  long name_length;
  char name[/*name_length*/];
  // char data[];
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
  pKlass *kl = &o->klass->vtbl;
  int ret = (*kl)->oop_size(kl, o);
  //printf("SizeOf: %d SizeOf2: %d\n", ret, sizeOf2(o));
  return ret;
}

char *internal_name(klassOop klass)
{
  pKlass *kl = &klass->vtbl;
  return (*kl)->internal_name(kl);
}

klassOop *unwrap_java_class(jclass clazz)
{
  oop *javaKlassOop = clazz;
  return ((void*)*javaKlassOop) + sizeof(struct oopDesc);
}

klassOop klass_by_name(JNIEnv *env, char *name)
{
  return *unwrap_java_class((*env)->FindClass(env, name));
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

typedef void(*Iterator)(oop,oop,int,long);

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
  it(root, 0, 0, arg);
  
  void **end = ((void*)root) + size;
  
  void **cur = root->data;
  while (cur < end) {
    printf("Now at %lx\n", cur);
  
    if (is_oop(*cur)) {
      it(*cur, root, (void*)cur - (void*)root, arg);
      iterate_over_fields(*cur, arg, it);
    }
    cur += 1;
  }
}

void iterate_over_oop_array(oopArrayOop array, void *arg, Iterator it)
{
  struct ArrayInfo *info = &array->oop.klass->layout_helper;
  dump(array, 100);
  printf("Static ArrayInfo: tag=0x%x offset=%d type=%d element_size=%d\n",
    info->tag,
    info->offset,
    info->type,
    info->element_size);
    
  int size = array->length;
  printf("Array length: %d\n", size);
  
  int i;
  for (i = 0; i < size; i++) {
    oop *ele = array->data + (i << info->element_size);
    printf("Element %d 0x%lx data=0x%lx\n", i, *ele, array->data);
    it(*ele, array, (void*)ele - (void*)array, arg);
  }  
}

struct RelocationState {
  HashTable relocated_oops;
  HashTable relocated_classes;
  void *oops_pos;
  void *class_pos;
};
typedef struct RelocationState *pState;

void* align(void *p)
{
  void *aligned = ((long)p & 0xfffffffffffffff8);
  if (p == aligned)
    return p;
  else
    return aligned + 8;
}

oop relocateKlass(klassOop kl, pState p)
{
  oop res = find(p->relocated_classes, kl);
  
  if (!res) {
    struct ClassInfo *next = p->class_pos;
    
    char *name = internal_name(kl);
    int length = strlen(name);
    next->name_length = length;
    memcpy(next->name, name, length);

    res = &next->name[length];
    int oop_size = sizeOf(kl);
    memcpy(res, kl, oop_size);
    
    next->next = align((void*)next + sizeof(struct ClassInfo) + length + oop_size);
    p->class_pos = next->next;
    
    put(p->relocated_classes, kl, res);
    
    printf("Relocating class 0x%lx to 0x%lx %s name_length: %d size: %d\n", kl, res, name, length, oop_size);
  }
  else {
    printf("Already relocated class at 0x%lx to 0x%lx %s\n", kl, res, internal_name(kl));
  }
  return res;
}

oop relocate(oop o, pState relocator)
{
  oop reloc = find(relocator->relocated_oops, o);
  if (!reloc) { // not yet relocated
    int size = sizeOf(o) << 3;
    reloc = relocator->oops_pos;
    memcpy(reloc, o, size);
    relocator->oops_pos += size;
    
    reloc->klass = relocateKlass(reloc->klass, relocator);
    
    printf("Relocating 0x%lx to 0x%lx\n", o, reloc);    
    put(relocator->relocated_oops, o, reloc);
  }
  else {
    printf("Already relocated: 0x%lx to 0x%lx\n", o, reloc);
  }
  return reloc;
}

void print_oop(oop o,void *l)
{
  printf("--- Found object: 0x%lx size=%d classSize=%d class=%s\n", 
    o,
    sizeOf(o),
    sizeOf(o->klass),
    internal_name(o->klass));
}

void print_and_relocate_oop(oop o, oop parent, int offset, pState p)
{
  print_oop(o, p);
    
  oop new = relocate(o, p);
  if (parent) {
    // plugin the holes in the parent
    oop *newHole = ((void*)find(p->relocated_oops, parent)) + offset;
    *newHole = new;
  }
}

#define FILE_SIZE 50000

JNIEXPORT void JNICALL Java_Test_analyze
  (JNIEnv *env, jclass clazz, jobject o)
{
  int f = open("bla", O_RDWR);
  printf("after creat page: %ld file: %d\n", FILE_SIZE, f);
  Formatp data = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
  data->base = data;

  struct RelocationState state;
  state.relocated_oops = create_hash_table();
  state.relocated_classes = create_hash_table();
  state.oops_pos = data->data;
  state.class_pos = data->header;
  
  printf("Starting to relocate to 0x%lx\n", state.oops_pos);

  oop *myO = o;
  iterate_over_fields(*myO, &state, print_and_relocate_oop);
  msync(data, FILE_SIZE, MS_SYNC);
  
  free_hash_table(state.relocated_oops);
}
