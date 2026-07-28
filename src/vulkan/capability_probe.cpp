#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(TETCAGE_HAS_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace {

#if defined(TETCAGE_HAS_VULKAN)
std::string json_escape(const std::string &value) {
  std::ostringstream output;
  for (const char character : value) {
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20U) {
        output << "\\u00";
        constexpr char digits[] = "0123456789abcdef";
        output << digits[(static_cast<unsigned char>(character) >> 4U) & 0xfU]
               << digits[static_cast<unsigned char>(character) & 0xfU];
      } else {
        output << character;
      }
    }
  }
  return output.str();
}

bool has_extension(const std::vector<VkExtensionProperties> &extensions, const char *name) {
  return std::any_of(extensions.begin(), extensions.end(), [name](const auto &extension) {
    return std::string(extension.extensionName) == name;
  });
}

std::vector<VkExtensionProperties> device_extensions(VkPhysicalDevice device) {
  std::uint32_t count = 0;
  if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS) {
    return {};
  }
  std::vector<VkExtensionProperties> extensions(count);
  if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) !=
      VK_SUCCESS) {
    return {};
  }
  extensions.resize(count);
  return extensions;
}

void print_bool_field(const char *name, bool value, bool comma = true) {
  std::cout << "      \"" << name << "\": " << (value ? "true" : "false");
  if (comma) {
    std::cout << ',';
  }
  std::cout << '\n';
}

#endif

} // namespace

int main() {
#if !defined(TETCAGE_HAS_VULKAN)
  std::cout << "{\n"
            << "  \"schema_version\": 1,\n"
            << "  \"probe_kind\": \"vulkan\",\n"
            << "  \"status\": \"unverified\",\n"
            << "  \"sdk_available_at_build\": false,\n"
            << "  \"reason\": \"No Vulkan loader and development headers were found at configure "
               "time.\",\n"
            << "  \"devices\": []\n"
            << "}\n";
  return 0;
#else
  std::uint32_t api_version = VK_API_VERSION_1_0;
  if (vkEnumerateInstanceVersion != nullptr) {
    static_cast<void>(vkEnumerateInstanceVersion(&api_version));
  }
  VkApplicationInfo application_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  application_info.pApplicationName = "tetcage_vulkan_probe";
  application_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
  application_info.pEngineName = "none";
  application_info.engineVersion = 0;
  application_info.apiVersion = std::min(api_version, VK_API_VERSION_1_3);
  VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  create_info.pApplicationInfo = &application_info;
  VkInstance instance = VK_NULL_HANDLE;
  const VkResult create_result = vkCreateInstance(&create_info, nullptr, &instance);
  if (create_result != VK_SUCCESS) {
    std::cout << "{\n"
              << "  \"schema_version\": 1,\n"
              << "  \"probe_kind\": \"vulkan\",\n"
              << "  \"status\": \"error\",\n"
              << "  \"sdk_available_at_build\": true,\n"
              << "  \"vk_result\": " << static_cast<int>(create_result) << ",\n"
              << "  \"devices\": []\n"
              << "}\n";
    return 2;
  }

  std::uint32_t device_count = 0;
  VkResult enumerate_result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  std::vector<VkPhysicalDevice> devices(device_count);
  if (enumerate_result == VK_SUCCESS && device_count > 0U) {
    enumerate_result = vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    devices.resize(device_count);
  }
  std::cout << "{\n"
            << "  \"schema_version\": 1,\n"
            << "  \"probe_kind\": \"vulkan\",\n"
            << "  \"status\": \""
            << ((enumerate_result == VK_SUCCESS && !devices.empty()) ? "measured" : "unverified")
            << "\",\n"
            << "  \"sdk_available_at_build\": true,\n"
            << "  \"loader_api_version\": \"" << VK_API_VERSION_MAJOR(api_version) << '.'
            << VK_API_VERSION_MINOR(api_version) << '.' << VK_API_VERSION_PATCH(api_version)
            << "\",\n"
            << "  \"devices\": [\n";
  for (std::size_t index = 0; index < devices.size(); ++index) {
    const auto extensions = device_extensions(devices[index]);
    const bool has_acceleration_structure =
        has_extension(extensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    const bool has_ray_pipeline =
        has_extension(extensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    const bool has_ray_query = has_extension(extensions, VK_KHR_RAY_QUERY_EXTENSION_NAME);

    VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_properties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    if (has_acceleration_structure) {
      properties.pNext = &acceleration_properties;
    }
    vkGetPhysicalDeviceProperties2(devices[index], &properties);

    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_address{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_pipeline{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &buffer_address;
    buffer_address.pNext = has_acceleration_structure ? &acceleration_features : nullptr;
    acceleration_features.pNext = has_ray_pipeline ? &ray_pipeline : nullptr;
    ray_pipeline.pNext = has_ray_query ? &ray_query : nullptr;
    vkGetPhysicalDeviceFeatures2(devices[index], &features);

    std::cout << "    {\n"
              << "      \"name\": \"" << json_escape(properties.properties.deviceName) << "\",\n"
              << "      \"vendor_id\": " << properties.properties.vendorID << ",\n"
              << "      \"device_id\": " << properties.properties.deviceID << ",\n"
              << "      \"driver_version_raw\": " << properties.properties.driverVersion << ",\n"
              << "      \"api_version\": \""
              << VK_API_VERSION_MAJOR(properties.properties.apiVersion) << '.'
              << VK_API_VERSION_MINOR(properties.properties.apiVersion) << '.'
              << VK_API_VERSION_PATCH(properties.properties.apiVersion) << "\",\n";
    print_bool_field("VK_KHR_acceleration_structure", has_acceleration_structure);
    print_bool_field("VK_KHR_ray_tracing_pipeline", has_ray_pipeline);
    print_bool_field("VK_KHR_ray_query", has_ray_query);
    print_bool_field("VK_KHR_deferred_host_operations",
                     has_extension(extensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME));
    print_bool_field("buffer_device_address", buffer_address.bufferDeviceAddress == VK_TRUE);
    print_bool_field("acceleration_structure_feature",
                     acceleration_features.accelerationStructure == VK_TRUE);
    print_bool_field("ray_tracing_pipeline_feature", ray_pipeline.rayTracingPipeline == VK_TRUE);
    print_bool_field("ray_query_feature", ray_query.rayQuery == VK_TRUE);
    print_bool_field("VK_NV_partitioned_acceleration_structure",
                     has_extension(extensions, "VK_NV_partitioned_acceleration_structure"));
    print_bool_field("VK_NV_cluster_acceleration_structure",
                     has_extension(extensions, "VK_NV_cluster_acceleration_structure"));
    if (has_acceleration_structure) {
      std::cout << "      \"max_instance_count\": " << acceleration_properties.maxInstanceCount
                << ",\n"
                << "      \"max_geometry_count\": " << acceleration_properties.maxGeometryCount
                << ",\n"
                << "      \"max_primitive_count\": " << acceleration_properties.maxPrimitiveCount
                << ",\n"
                << "      \"min_scratch_offset_alignment\": "
                << acceleration_properties.minAccelerationStructureScratchOffsetAlignment << '\n';
    } else {
      std::cout << "      \"max_instance_count\": null,\n"
                << "      \"max_geometry_count\": null,\n"
                << "      \"max_primitive_count\": null,\n"
                << "      \"min_scratch_offset_alignment\": null\n";
    }
    std::cout << "    }" << (index + 1U == devices.size() ? "\n" : ",\n");
  }
  std::cout << "  ]\n"
            << "}\n";
  vkDestroyInstance(instance, nullptr);
  return enumerate_result == VK_SUCCESS ? 0 : 2;
#endif
}
