#include "LibVulkan.h"

#include <dlfcn.h>
#include <iostream>
#include <memory>

LibVulkan::LibVulkan()
    : handle(::dlopen("libvulkan.so", RTLD_NOW | RTLD_GLOBAL)) {
  if (handle == nullptr) {
    std::cerr << "ERROR: Unable to dlopen libvulkan.so because " << ::dlerror()
              << std::endl;
    std::exit(EXIT_FAILURE);
  }
  ::dlerror();  // clear
  vkGetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dlsym(handle.get(), "vkGetInstanceProcAddr");
  char* error = ::dlerror();
  if (error != NULL) {
    std::cerr << "ERROR: Unable to dlsym vkGetInstanceProcAddr from "
                 "libulkan.so because "
              << error << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

void LibVulkan::Deleter::operator()(void* handle) {
  if (::dlclose(handle) != 0) {
    std::cerr << "ERROR: Unable to dlclose libvulkan.so because " << ::dlerror()
              << std::endl;
    std::exit(EXIT_FAILURE);
  }
}
