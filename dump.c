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
  void *reserved2;
  char*(*external_name)(pKlass*);
  void *reserved3[94];
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
  long klass_length;
  char data[/*klass_length*/];
  // char name[];
  // alignment
};

struct Format {
  long base;
  char header[100000];
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

char *external_name(klassOop klass)
{
  pKlass *kl = &klass->vtbl;
  return (*kl)->external_name(kl);
}

klassOop *unwrap_java_class(jclass clazz)
{
  oop *javaKlassOop = clazz;
  return ((void*)*javaKlassOop) + sizeof(struct oopDesc);
}

klassOop klass_by_name(JNIEnv *env, char *name)
{
  jclass clazz = (*env)->FindClass(env, name);
  assert(clazz != 0, name);
  return *unwrap_java_class(clazz);
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
  
  void **end = ((void*)root) + size;
  
  void **cur = root->data;
  while (cur < end) {
    printf("Now at %lx\n", cur);
  
    if (is_oop(*cur)) {
      it(*cur, root, (void*)cur - (void*)root, arg);
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

char *name_offset(struct ClassInfo *info)
{
  return align(&info->data[info->klass_length]);
}

oop relocateKlass(klassOop kl, pState p)
{
  oop res = find(p->relocated_classes, kl);
  
  if (!res) {
    struct ClassInfo *next = p->class_pos;
    
    res = &next->data;
    int oop_size = sizeOf(kl)<<3;
    printf("Class of kl: %s\n", internal_name(kl->oop.klass));
    memcpy(res, kl, oop_size);
    next->klass_length = oop_size;
    
    char *name = external_name(kl);
    int length = strlen(name);
    
    char *name_ptr = name_offset(next);

    strcpy(name_ptr, name);
        
    next->next = align(name_ptr + length + 1);
    p->class_pos = next->next;
    
    put(p->relocated_classes, kl, res);
    
    printf("Relocating class 0x%lx to 0x%lx %s name_length: %d size: %d\n", kl, res, name, length, oop_size);
  }
  else {
    printf("Already relocated class at 0x%lx to 0x%lx %s\n", kl, res, internal_name(kl));
  }
  return res;
}

void print_and_relocate_oop(oop o, oop parent, int offset, pState p);

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
    
    iterate_over_fields(o, relocator, print_and_relocate_oop);
  }
  else {
    printf("Already relocated: 0x%lx to 0x%lx\n", o, reloc);
  }
  return reloc;
}

void print_oop(oop o,void *l)
{
  printf("--- Found object: 0x%lx size=%d classSize=%d class=%s ext=%s\n", 
    o,
    sizeOf(o),
    sizeOf(o->klass),
    internal_name(o->klass),
    external_name(o->klass));
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

#define FILE_SIZE 500000

JNIEXPORT jobject JNICALL Java_Test_analyze
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
  print_and_relocate_oop(*myO, 0, 0, &state);
  msync(data, FILE_SIZE, MS_SYNC);
  
  free_hash_table(state.relocated_oops);
  free_hash_table(state.relocated_classes);
  
  void** ret = (void**) (*env)->NewLocalRef(env, &data->data);
  *ret = &data->data;
  return ret;
}

JNIEXPORT jobject JNICALL Java_Test_load
  (JNIEnv *env, jclass recv, jclass inClass)
{
  long page = sysconf(_SC_PAGE_SIZE);

  int f = open("bla", O_RDONLY);
  long pos;
  int res = read(f, &pos, sizeof(long));
  printf("Read returned %d\n", sizeof(void*));
  Formatp data = mmap(pos, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, f, 0);
  printf("after mmap buffer: %lx == %lx\n", data, pos);

  // reload class information
  struct ClassInfo *info = &data->header;
  while(info->next) {
    char *name = name_offset(info);
    printf("Searching for class %-30s ... ", name);
    klassOop kl = klass_by_name(env, name);
    printf("Found class our size %3d their size %3d name %s\n", info->klass_length, sizeOf(kl), external_name(kl));
    memcpy(info->data, kl, info->klass_length);
    info = info->next;
  }
  dump(data, 50000);

  void** ret = (void**) (*env)->NewLocalRef(env, &data->data);
  *ret = &data->data;
  return ret;
}
