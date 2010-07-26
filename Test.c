#include "Test.h"
#include <stdio.h>

JNIEXPORT jobject JNICALL Java_Test_get
(JNIEnv *env, jclass cl) {
  printf("Wurstbombe\n");
  return 0;
}
