#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string json_escape(NSString *value) {
  if (value == nil) {
    return "";
  }
  const char *utf8 = [value UTF8String];
  if (utf8 == nullptr) {
    return "";
  }
  std::ostringstream output;
  for (const char *cursor = utf8; *cursor != '\0'; ++cursor) {
    switch (*cursor) {
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
      output << *cursor;
    }
  }
  return output.str();
}

const char *json_bool(BOOL value) { return value ? "true" : "false"; }

struct SizeQuery {
  std::uint64_t instance_count{};
  bool indirect{};
  bool extended_limits{};
  bool accepted{};
  std::uint64_t acceleration_structure_bytes{};
  std::uint64_t build_scratch_bytes{};
  std::string error;
};

SizeQuery query_instance_descriptor(id<MTLDevice> device, std::uint64_t count, bool indirect,
                                    bool extended_limits) {
  SizeQuery result{};
  result.instance_count = count;
  result.indirect = indirect;
  result.extended_limits = extended_limits;
  @try {
    MTLAccelerationStructureDescriptor *descriptor = nil;
    if (indirect) {
      if (@available(macOS 14.0, *)) {
        auto *indirect_descriptor = [MTLIndirectInstanceAccelerationStructureDescriptor descriptor];
        indirect_descriptor.maxInstanceCount = static_cast<NSUInteger>(count);
        descriptor = indirect_descriptor;
      } else {
        result.error = "indirect descriptor API unavailable";
        return result;
      }
    } else {
      auto *direct_descriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
      direct_descriptor.instanceCount = static_cast<NSUInteger>(count);
      descriptor = direct_descriptor;
    }
    if (extended_limits) {
      if (@available(macOS 12.0, *)) {
        descriptor.usage = descriptor.usage | MTLAccelerationStructureUsageExtendedLimits;
      } else {
        result.error = "extended-limits usage API unavailable";
        return result;
      }
    }
    const MTLAccelerationStructureSizes sizes =
        [device accelerationStructureSizesWithDescriptor:descriptor];
    result.accepted = sizes.accelerationStructureSize > 0U;
    result.acceleration_structure_bytes = sizes.accelerationStructureSize;
    result.build_scratch_bytes = sizes.buildScratchBufferSize;
  } @catch (NSException *exception) {
    result.error = json_escape([exception reason]);
  }
  return result;
}

void print_size_queries(id<MTLDevice> device, bool indirect, bool extended_limits) {
  constexpr std::uint64_t counts[] = {1ULL, 100'000ULL, 1'000'000ULL, 2'800'000ULL, 16'000'000ULL};
  std::cout << "[\n";
  for (std::size_t index = 0; index < std::size(counts); ++index) {
    const auto query = query_instance_descriptor(device, counts[index], indirect, extended_limits);
    std::cout << "          {\"instance_count\": " << query.instance_count
              << ", \"accepted\": " << (query.accepted ? "true" : "false")
              << ", \"acceleration_structure_bytes\": ";
    if (query.accepted) {
      std::cout << query.acceleration_structure_bytes;
    } else {
      std::cout << "null";
    }
    std::cout << ", \"build_scratch_bytes\": ";
    if (query.accepted) {
      std::cout << query.build_scratch_bytes;
    } else {
      std::cout << "null";
    }
    std::cout << ", \"error\": ";
    if (query.error.empty()) {
      std::cout << "null";
    } else {
      std::cout << '"' << query.error << '"';
    }
    std::cout << '}' << (index + 1U == std::size(counts) ? "\n" : ",\n");
  }
  std::cout << "        ]";
}

} // namespace

int main() {
  @autoreleasepool {
    NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
    if (devices.count == 0U) {
      id<MTLDevice> default_device = MTLCreateSystemDefaultDevice();
      if (default_device != nil) {
        devices = @[ default_device ];
      }
    }
    std::cout << "{\n"
              << "  \"schema_version\": 1,\n"
              << "  \"probe_kind\": \"metal\",\n"
              << "  \"status\": \"" << (devices.count > 0U ? "measured" : "unverified") << "\",\n"
              << "  \"os\": \""
              << json_escape([[NSProcessInfo processInfo] operatingSystemVersionString]) << "\",\n"
              << "  \"device_count\": " << devices.count << ",\n"
              << "  \"limitations\": [\n"
              << "    \"Metal exposes no direct numeric maximum-instance or AS nesting-depth "
                 "property; descriptor size queries below are real driver calls but do not prove "
                 "a successful build at that count.\",\n"
              << "    \"Metal API availability is not treated as device support; family and "
                 "supportsRaytracing values come from each MTLDevice.\"\n"
              << "  ],\n"
              << "  \"devices\": [\n";
    for (NSUInteger index = 0; index < devices.count; ++index) {
      id<MTLDevice> device = devices[index];
      BOOL supports_raytracing = NO;
      BOOL supports_render_raytracing = NO;
      BOOL supports_function_pointers = NO;
      if (@available(macOS 11.0, *)) {
        supports_raytracing = device.supportsRaytracing;
        supports_function_pointers = device.supportsFunctionPointers;
      }
      if (@available(macOS 12.0, *)) {
        supports_render_raytracing = device.supportsRaytracingFromRender;
      }
      BOOL metal3 = NO;
      BOOL metal4 = NO;
      if (@available(macOS 13.0, *)) {
        metal3 = [device supportsFamily:MTLGPUFamilyMetal3];
      }
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
      if (@available(macOS 26.0, *)) {
        metal4 = [device supportsFamily:MTLGPUFamilyMetal4];
      }
#endif
      BOOL indirect_descriptor_api = NO;
      BOOL extended_limits_api = NO;
      if (@available(macOS 14.0, *)) {
        indirect_descriptor_api = YES;
      }
      if (@available(macOS 12.0, *)) {
        extended_limits_api = YES;
      }
      std::cout << "    {\n"
                << "      \"name\": \"" << json_escape(device.name) << "\",\n"
                << "      \"registry_id\": " << device.registryID << ",\n"
                << "      \"unified_memory\": " << json_bool(device.hasUnifiedMemory) << ",\n"
                << "      \"recommended_max_working_set_bytes\": "
                << device.recommendedMaxWorkingSetSize << ",\n"
                << "      \"max_buffer_length\": " << device.maxBufferLength << ",\n"
                << "      \"supports_raytracing\": " << json_bool(supports_raytracing) << ",\n"
                << "      \"supports_raytracing_from_render\": "
                << json_bool(supports_render_raytracing) << ",\n"
                << "      \"supports_function_pointers\": " << json_bool(supports_function_pointers)
                << ",\n"
                << "      \"supports_metal3_family\": " << json_bool(metal3) << ",\n"
                << "      \"supports_metal4_family\": " << json_bool(metal4) << ",\n"
                << "      \"indirect_instance_descriptor_api_available\": "
                << json_bool(indirect_descriptor_api) << ",\n"
                << "      \"extended_limits_usage_api_available\": "
                << json_bool(extended_limits_api) << ",\n"
                << "      \"reported_max_instance_count\": null,\n"
                << "      \"reported_max_as_nesting_depth\": null,\n"
                << "      \"descriptor_queries\": {\n"
                << "        \"direct_standard\": ";
      print_size_queries(device, false, false);
      std::cout << ",\n        \"direct_extended\": ";
      print_size_queries(device, false, true);
      std::cout << ",\n        \"indirect_standard\": ";
      print_size_queries(device, true, false);
      std::cout << ",\n        \"indirect_extended\": ";
      print_size_queries(device, true, true);
      std::cout << "\n      }\n"
                << "    }" << (index + 1U == devices.count ? "\n" : ",\n");
    }
    std::cout << "  ]\n"
              << "}\n";
    return devices.count > 0U ? 0 : 2;
  }
}
