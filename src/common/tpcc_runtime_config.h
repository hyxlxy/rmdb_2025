#pragma once
#include <atomic>

// 运行时一致性与指标配置
struct TPCCRuntimeConfig {
    // 允许 d_next_o_id 与 max(o_id)+1 的偏差（0 表示严格一致）
    static std::atomic<int> d_next_oid_tolerance;
    // 是否启用一致性校验日志
    static std::atomic<bool> enable_consistency_logging;

    // 运行时校验是否强制执行（false 时仅记录/跳过，不抛异常）
    static std::atomic<bool> enforce_runtime_validation;
    // 是否在 district 更新时强制修正 d_next_o_id（默认关闭，遵循 SQL 值）
    static std::atomic<bool> enforce_dnext_fix;

    // 指标开关：开启后仅周期性汇总输出，不逐行打印
    static std::atomic<bool> metrics_enabled;
    // 每多少次事件输出一次汇总（如 2000）
    static std::atomic<int> metrics_emit_every_ops;
    // 事务不完整告警的采样千分比（如 5 表示千分之五抽样详细日志）
    static std::atomic<int> anomaly_sample_per_mille;
};

