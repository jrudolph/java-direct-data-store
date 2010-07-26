#include "Test.h"
#include <stdio.h>

JNIEXPORT jobject JNICALL Java_Test_get
(JNIEnv *env, jclass cl)
{
  printf("Wurstbombe\n");
  return 0;
}

JNIEXPORT jobject JNICALL Java_Test_persist
  (JNIEnv *env, jclass cl, jobject o)
{
  void ** ptr = (void**)o;
  void * buffer = malloc(50);
  memcpy(buffer, *ptr, 50);
  
  void** ret = (void**) (*env)->NewLocalRef(env, buffer);
  *ret = buffer;
  return ret;
}
