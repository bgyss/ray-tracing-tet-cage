#!/usr/bin/env bash
set -euo pipefail

has_command() {
  command -v "$1" >/dev/null 2>&1
}

vulkan_available=false
cuda_available=false
nvidia_available=false
if has_command vulkaninfo; then
  vulkan_available=true
fi
if has_command nvcc; then
  cuda_available=true
fi
if has_command nvidia-smi; then
  nvidia_available=true
fi

ready=false
status="blocked"
decision="remove_from_production_path"
reason="A matched NVIDIA Vulkan/CUDA device and both compute implementations are required before interop can be measured."
if [[ "$vulkan_available" == true && "$cuda_available" == true && "$nvidia_available" == true ]]; then
  ready=true
  status="ready_for_experiment"
  decision="retain_or_remove_after_end_to_end_measurement"
  reason="Tooling and a device report are available; UUID matching, external synchronization, and equivalent-kernel measurements are still required."
fi

cat <<JSON
{
  "schema_version": 1,
  "evidence_class": "host_capability_gate",
  "hypothesis": "CUDA may reduce GPU instance-descriptor generation time, but external-memory and semaphore ownership could erase that benefit.",
  "vulkaninfo_present": $vulkan_available,
  "nvcc_present": $cuda_available,
  "nvidia_smi_present": $nvidia_available,
  "matched_device_uuid": false,
  "external_memory_bridge_measured": false,
  "external_semaphore_bridge_measured": false,
  "status": "$status",
  "ready_for_experiment": $ready,
  "decision": "$decision",
  "reason": "$reason",
  "next_step": "On a real NVIDIA host, record physical-device UUIDs, implement equivalent Vulkan/CUDA kernels, include ownership and synchronization in end-to-end timings, and retain CUDA only for a material benefit or required ecosystem integration."
}
JSON
