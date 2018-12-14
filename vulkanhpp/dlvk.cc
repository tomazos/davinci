#include "LibVulkan.h"

#include <iostream>
#include <optional>
#include <vector>

namespace spk {
namespace detail {
PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion;
PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
PFN_vkCreateInstance CreateInstance;

void set_functions(LibVulkan& libvulkan) {
  EnumerateInstanceVersion = LIBVULKAN_GET_INSTANCE_PROC_ADDR(
      libvulkan, VK_NULL_HANDLE, vkEnumerateInstanceVersion);
  EnumerateInstanceExtensionProperties = LIBVULKAN_GET_INSTANCE_PROC_ADDR(
      libvulkan, VK_NULL_HANDLE, vkEnumerateInstanceExtensionProperties);
  EnumerateInstanceLayerProperties = LIBVULKAN_GET_INSTANCE_PROC_ADDR(
      libvulkan, VK_NULL_HANDLE, vkEnumerateInstanceLayerProperties);
  CreateInstance = LIBVULKAN_GET_INSTANCE_PROC_ADDR(libvulkan, VK_NULL_HANDLE,
                                                    vkCreateInstance);
}
};  // namespace detail

uint32_t enumerate_instance_version() {
  uint32_t apiVersion;
  detail::EnumerateInstanceVersion(&apiVersion);
  return apiVersion;
}

struct layer_properties {
  std::string layer_name;
  uint32_t spec_version;
  uint32_t implementation_version;
  std::string description;
};

std::vector<layer_properties> enumerate_instance_layer_properties() {
  uint32_t propertyCount;
  detail::EnumerateInstanceLayerProperties(&propertyCount, nullptr);
  std::vector<VkLayerProperties> properties(propertyCount);
  detail::EnumerateInstanceLayerProperties(&propertyCount, properties.data());
  std::vector<layer_properties> result(propertyCount);

  for (size_t i = 0; i < propertyCount; ++i) {
    const VkLayerProperties& a = properties[i];
    layer_properties& b = result[i];
    b.layer_name = a.layerName;
    b.spec_version = a.specVersion;
    b.implementation_version = a.implementationVersion;
    b.description = a.description;
  }
  return result;
}

struct extension_properties {
  std::string extension_name;
  uint32_t spec_version;
};

std::vector<extension_properties> enumerate_instance_extension_properties(
    std::optional<std::string> layer_name = std::nullopt) {
  const char* pLayerName = (layer_name ? layer_name->c_str() : nullptr);
  uint32_t propertyCount;
  detail::EnumerateInstanceExtensionProperties(pLayerName, &propertyCount,
                                               nullptr);
  std::vector<VkExtensionProperties> properties(propertyCount);
  detail::EnumerateInstanceExtensionProperties(pLayerName, &propertyCount,
                                               properties.data());
  std::vector<extension_properties> result(propertyCount);

  for (size_t i = 0; i < propertyCount; ++i) {
    const VkExtensionProperties& a = properties[i];
    extension_properties& b = result[i];
    b.extension_name = a.extensionName;
    b.spec_version = a.specVersion;
  }
  return result;
};

class instance {
 public:
  instance() {}

 private:
  VkInstance handle;
};

};  // namespace spk

int main() {
  LibVulkan libvulkan;
  spk::detail::set_functions(libvulkan);
  std::cout << spk::enumerate_instance_version() << std::endl;
  for (spk::layer_properties layer_properties :
       spk::enumerate_instance_layer_properties()) {
    std::cout << layer_properties.layer_name << ": "
              << layer_properties.description << std::endl;
    for (spk::extension_properties extension_properties :
         spk::enumerate_instance_extension_properties(
             layer_properties.layer_name))
      std::cout << extension_properties.extension_name << std::endl;
  }
  std::cout << std::endl;
  for (spk::extension_properties extension_properties :
       spk::enumerate_instance_extension_properties())
    std::cout << extension_properties.extension_name << std::endl;
}
