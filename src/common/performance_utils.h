
#pragma once

#include <string>
#include <string_view>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cstring>
#include <stdexcept>
#include <cctype>

/**
 * @brief 性能优化工具类
 * 提供高性能的I/O和字符串操作
 */
class PerformanceUtils
{
public:
    /**
     * @brief 使用pread代替read，避免lseek调用
     * @param fd 文件描述符
     * @param buf 缓冲区
     * @param count 读取字节数
     * @param offset 文件偏移量
     * @return 实际读取的字节数
     */
    static ssize_t fast_pread(int fd, void *buf, size_t count, off_t offset)
    {
#ifdef TPCC_PERFORMANCE_MODE
        // 使用pread避免lseek系统调用
        return pread(fd, buf, count, offset);
#else
        // 原始方式：lseek + read
        if (lseek(fd, offset, SEEK_SET) == -1)
        {
            return -1;
        }
        return read(fd, buf, count);
#endif
    }

    /**
     * @brief 使用pwrite代替write，避免lseek调用
     * @param fd 文件描述符
     * @param buf 缓冲区
     * @param count 写入字节数
     * @param offset 文件偏移量
     * @return 实际写入的字节数
     */
    static ssize_t fast_pwrite(int fd, const void *buf, size_t count, off_t offset)
    {
#ifdef TPCC_PERFORMANCE_MODE
        // 使用pwrite避免lseek系统调用
        return pwrite(fd, buf, count, offset);
#else
        // 原始方式：lseek + write
        if (lseek(fd, offset, SEEK_SET) == -1)
        {
            return -1;
        }
        return write(fd, buf, count);
#endif
    }

    /**
     * @brief 高性能字符串比较
     * @param sv1 字符串视图1
     * @param sv2 字符串视图2
     * @return 比较结果
     */
    static int fast_string_compare(std::string_view sv1, std::string_view sv2)
    {
#ifdef TPCC_PERFORMANCE_MODE
        // 使用string_view避免字符串拷贝
        return sv1.compare(sv2);
#else
        // 原始字符串比较
        std::string s1(sv1);
        std::string s2(sv2);
        return s1.compare(s2);
#endif
    }

    /**
     * @brief 快速字符串查找
     * @param haystack 被搜索的字符串
     * @param needle 要查找的子串
     * @return 找到的位置，未找到返回string_view::npos
     */
    static size_t fast_string_find(std::string_view haystack, std::string_view needle)
    {
#ifdef TPCC_PERFORMANCE_MODE
        return haystack.find(needle);
#else
        std::string h(haystack);
        std::string n(needle);
        return h.find(n);
#endif
    }

    /**
     * @brief 快速字符串转换为数值
     * @param sv 字符串视图
     * @return 转换后的整数
     */
    static int fast_string_to_int(std::string_view sv) {
        if (sv.empty()) return 0;

        // 手动解析数字，避免库函数开销
        int result = 0;
        bool negative = false;
        size_t i = 0;

        // 处理符号
        if (sv[0] == '-') {
            negative = true;
            i = 1;
        } else if (sv[0] == '+') {
            i = 1;
        }

        // 解析数字
        for (; i < sv.size() && std::isdigit(sv[i]); ++i) {
            result = result * 10 + (sv[i] - '0');
        }

        return negative ? -result : result;
    }

    /**
     * @brief 快速内存拷贝
     * @param dest 目标地址
     * @param src 源地址
     * @param n 拷贝字节数
     */
    static void fast_memcpy(void *dest, const void *src, size_t n)
    {
#ifdef TPCC_PERFORMANCE_MODE
        // 对于小块内存，使用内联汇编或编译器优化
        if (n <= 64)
        {
            __builtin_memcpy(dest, src, n);
        }
        else
        {
            memcpy(dest, src, n);
        }
#else
        memcpy(dest, src, n);
#endif
    }

    /**
     * @brief 快速内存比较
     * @param s1 内存块1
     * @param s2 内存块2
     * @param n 比较字节数
     * @return 比较结果
     */
    static int fast_memcmp(const void *s1, const void *s2, size_t n)
    {
#ifdef TPCC_PERFORMANCE_MODE
        // 对于小块内存，使用优化的比较
        if (n <= 64)
        {
            return __builtin_memcmp(s1, s2, n);
        }
        else
        {
            return memcmp(s1, s2, n);
        }
#else
        return memcmp(s1, s2, n);
#endif
    }

    /**
     * @brief 预取内存到缓存
     * @param addr 内存地址
     * @param rw 读写提示（0=读，1=写）
     * @param locality 局部性提示（0-3，3=最高局部性）
     */
    static void prefetch_memory(const void *addr, int rw = 0, int locality = 3)
    {
#ifdef TPCC_PERFORMANCE_MODE
        __builtin_prefetch(addr, rw, locality);
#else
        // 空操作
        (void)addr;
        (void)rw;
        (void)locality;
#endif
    }

    /**
     * @brief 批量预取多个内存位置
     * @param addrs 内存地址数组
     * @param count 地址数量
     */
    static void batch_prefetch(const void **addrs, size_t count)
    {
#ifdef TPCC_PERFORMANCE_MODE
        for (size_t i = 0; i < count; ++i)
        {
            __builtin_prefetch(addrs[i], 0, 3);
        }
#else
        (void)addrs;
        (void)count;
#endif
    }

    /**
     * @brief 内存屏障，确保内存操作顺序
     */
    static void memory_barrier()
    {
#ifdef TPCC_PERFORMANCE_MODE
        __sync_synchronize();
#else
// 空操作
#endif
    }

    /**
     * @brief 编译器屏障，防止编译器重排
     */
    static void compiler_barrier()
    {
#ifdef TPCC_PERFORMANCE_MODE
        asm volatile("" ::: "memory");
#else
// 空操作
#endif
    }
};

/**
 * @brief 高性能字符串包装器
 * 在性能模式下使用string_view，否则使用string
 */
#ifdef TPCC_PERFORMANCE_MODE
using FastString = std::string_view;
#else
using FastString = std::string;
#endif

/**
 * @brief 创建FastString的辅助函数
 */
inline FastString make_fast_string(const char *str)
{
#ifdef TPCC_PERFORMANCE_MODE
    return std::string_view(str);
#else
    return std::string(str);
#endif
}

inline FastString make_fast_string(const std::string &str)
{
#ifdef TPCC_PERFORMANCE_MODE
    return std::string_view(str);
#else
    return str;
#endif
}
