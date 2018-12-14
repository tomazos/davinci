#pragma once

#include <memory>
#define VULKAN_NO_PROTOTYPES
#include <vulkan/vulkan.h>

class LibVulkan {
 public:
  LibVulkan();

  template<typename PFN>
  PFN GetInstanceProcAddr(VkInstance instance, const char* pName);

 private:
  struct Deleter {
    void operator()(void* handle);
  };
  std::unique_ptr<void, Deleter> handle;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
};

#define LIBVULKAN_GET_INSTANCE_PROC_ADDR(libvulkan, instance, command) libvulkan.GetInstanceProcAddr<PFN_##command>(instance, #command)

template<typename PFN>
PFN LibVulkan::GetInstanceProcAddr(VkInstance instance, const char* pName) {
  return (PFN) vkGetInstanceProcAddr(instance, pName);
}
