#include <jni.h>
#include <string>

#include <kiwi_machine_core/kiwi_main.h>

extern "C" JNIEXPORT int SDL_main(int argc, char* argv[]) {
  return KiwiMain(argc, argv);
}