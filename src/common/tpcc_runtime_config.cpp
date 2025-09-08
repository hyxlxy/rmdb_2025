#include "tpcc_runtime_config.h"
std::atomic<int> TPCCRuntimeConfig::d_next_oid_tolerance{0};
std::atomic<bool> TPCCRuntimeConfig::enable_consistency_logging{false};
std::atomic<bool> TPCCRuntimeConfig::enforce_runtime_validation{true};
std::atomic<bool> TPCCRuntimeConfig::enforce_dnext_fix{false};
std::atomic<bool> TPCCRuntimeConfig::metrics_enabled{false};
std::atomic<int> TPCCRuntimeConfig::metrics_emit_every_ops{2000000};
std::atomic<int> TPCCRuntimeConfig::anomaly_sample_per_mille{0};

