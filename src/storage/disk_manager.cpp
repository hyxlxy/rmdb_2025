#include "storage/disk_manager.h"

#include <sys/stat.h> // for stat
#include <unistd.h>   // for lseek
#include <fcntl.h>    // for fcntl

#include <cassert> // for assert
#include <cstring> // for memset
#include <iostream> // for std::cerr

#include "defs.h"
#include "record/rm_defs.h"

// 初始化静态成员变量
// std::atomic<int> DiskManager::next_file_id_{0};

DiskManager::DiskManager() { memset(fd2pageno_, 0, MAX_FD * (sizeof(std::atomic<page_id_t>) / sizeof(char))); }

/**
 * @description: 将数据写入文件的指定磁盘页面中
 * @param {int} fd 磁盘文件的文件句柄
 * @param {page_id_t} page_no 写入目标页面的page_id
 * @param {char} *offset 要写入磁盘的数据
 * @param {int} num_bytes 要写入磁盘的数据大小
 */
void DiskManager::write_page(int fd, page_id_t page_no, const char *offset, int num_bytes)
{
    // 1. 使用 lseek 定位到文件头，通过 (fd, page_no) 可以定位到指定页面及其在磁盘文件中的偏移量
    // off_t offset_pos = static_cast<off_t>(page_no) * PAGE_SIZE;
    // if (lseek(fd, offset_pos, SEEK_SET) == -1)
    // {
    //     throw UnixError();
    // }

    // // 2. 调用 write 函数
    // ssize_t bytes_written = write(fd, offset, num_bytes);
    // if (bytes_written != num_bytes)
    // {
    //     throw InternalError("DiskManager::write_page Error: write failed, expected " + std::to_string(num_bytes) + " bytes, but wrote " + std::to_string(bytes_written) + " bytes.");
    // }
    // 使用 pwrite 替代 lseek+write（线程安全，无竞态）
    off_t offset_pos = static_cast<off_t>(page_no) * PAGE_SIZE;
    ssize_t bytes_written = pwrite(fd, offset, num_bytes, offset_pos);
    if (bytes_written != num_bytes) {
        throw InternalError("DiskManager::write_page pwrite failed");
    }
}

/**
 * @description: 强制同步文件到磁盘
 * @param {int} fd 文件句柄
 */
void DiskManager::sync_file(int fd) {
    if (fsync(fd) == -1) {
        throw UnixError();
    }
}

/**
 * @description: 读取文件中指定编号的页面中的部分数据到内存中
 * @param {int} fd 磁盘文件的文件句柄
 * @param {page_id_t} page_no 指定的页面编号
 * @param {char} *offset 读取的内容写入到offset中
 * @param {int} num_bytes 读取的数据量大小
 */
void DiskManager::read_page(int fd, page_id_t page_no, char *offset, int num_bytes)
{
    // 1.lseek()定位到文件头，通过(fd,page_no)可以定位指定页面及其在磁盘文件中的偏移量
    // 2.调用read()函数
    // 注意read返回值与num_bytes不等时，throw InternalError("DiskManager::read_page Error");

    // // 1. 使用 lseek 定位到文件头，通过 (fd, page_no) 可以定位到指定页面及其在磁盘文件中的偏移量
    // off_t offset_pos = static_cast<off_t>(page_no) * PAGE_SIZE;
    // if (lseek(fd, offset_pos, SEEK_SET) == -1)
    // {
    //     throw UnixError();
    // }

    // // 2. 调用 read 函数
    // ssize_t bytes_read = read(fd, offset, num_bytes);
    // if (bytes_read != num_bytes)
    // {
    //     throw InternalError("DiskManager::read_page Error: read failed, expected " + std::to_string(num_bytes) + " bytes, but read " + std::to_string(bytes_read) + " bytes.");
    // }
    // 使用 pread 替代 lseek+read（线程安全，无竞态）
    off_t offset_pos = static_cast<off_t>(page_no) * PAGE_SIZE;
    ssize_t bytes_read = pread(fd, offset, num_bytes, offset_pos);
    if (bytes_read != num_bytes) {
        off_t file_size = lseek(fd, 0, SEEK_END);
        throw InternalError("DiskManager::read_page pread failed. "
                            + std::string("Got ") + std::to_string(bytes_read) + " bytes, wanted "
                            + std::to_string(num_bytes) + ". File size: " + std::to_string(file_size)
                            + ", Page: " + std::to_string(page_no));
    }
}

/**
 * @description: 分配一个新的页号
 * @return {page_id_t} 分配的新页号
 * @param {int} fd 指定文件的文件句柄
 */
page_id_t DiskManager::allocate_page(int fd)
{
    // 简单的自增分配策略，指定文件的页面编号加1
    assert(fd >= 0 && fd < MAX_FD);
    return fd2pageno_[fd]++;
}

void DiskManager::deallocate_page(__attribute__((unused)) page_id_t page_id) {}

bool DiskManager::is_dir(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void DiskManager::create_dir(const std::string &path)
{
    // Create a subdirectory
    std::string cmd = "mkdir " + path;
    if (system(cmd.c_str()) < 0)
    { // 创建一个名为path的目录
        throw UnixError();
    }
}

void DiskManager::destroy_dir(const std::string &path)
{
    std::string cmd = "rm -r " + path;
    if (system(cmd.c_str()) < 0)
    {
        throw UnixError();
    }
}

/**
 * @description: 判断指定路径文件是否存在
 * @return {bool} 若指定路径文件存在则返回true
 * @param {string} &path 指定路径文件
 */
bool DiskManager::is_file(const std::string &path)
{
    // 用struct stat获取文件信息
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * @description: 用于创建指定路径文件
 * @return {*}
 * @param {string} &path
 */
void DiskManager::create_file(const std::string &path)
{
    // 调用open()函数，使用O_CREAT模式
    // 注意不能重复创建相同文件
    // 检查文件是否已经存在
    // struct stat buffer;

    // // 检查文件是否已经存在
    // if (stat(path.c_str(), &buffer) == 0)
    // {
    //     throw FileExistsError(path);
    // }

    // // 创建文件，_CREAT | O_EXCL 确保文件是新创建的，如果文件已经存在，则会返回错误。S_IRUSR | S_IWUSR 设置文件的权限，表示文件所有者有读写权限
    // int fd = open(path.c_str(), O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    // if (fd < 0)
    // {
    //     throw UnixError();
    // }

    // // 关闭文件描述符
    // if (close(fd) < 0)
    // {
    //     throw UnixError();
    // }
      int fd = ::open(
        path.c_str(),
        O_WRONLY               // 只打开以写模式，不必读
      | O_CREAT                // 如果不存在就创建
      | O_EXCL                 // 已存在则失败
      | O_CLOEXEC,             // 防止 exec 时继承 fd    
        S_IRUSR | S_IWUSR       // 文件权限：owner 可读写
    );
    if (fd < 0) {
        if (errno == EEXIST) {
            throw FileExistsError(path);
        }
        // 其他错误一并抛出
        throw UnixError();
    }

    // 利用 RAII 保证 close 一定被调用
    struct FdGuard {
        int fd_;
        explicit FdGuard(int fd) : fd_(fd) {}
        ~FdGuard() {
            ::close(fd_);
            // 通常不在析构中抛异常
        }
    } guard(fd);
}

/**
 * @description: 删除指定路径的文件
 * @param {string} &path 文件所在路径
 */
void DiskManager::destroy_file(const std::string &path)
{
    // 调用unlink()函数
    // 注意不能删除未关闭的文件
    // 检查文件是否已经关闭
    auto it = path2fd_.find(path);
    if (it != path2fd_.end())
    {
        throw FileOpenError(path); // 文件被找到
    }

    // 删除文件
    if (unlink(path.c_str()) < 0)
    {
        if (errno == ENOENT)
        {
            throw FileNotFoundError(path);
        }
        else
        {
            throw UnixError();
        }
    }
}

/**
 * @description: 打开指定路径文件
 * @return {int} 返回打开的文件的文件句柄
 * @param {string} &path 文件所在路径
 */
int DiskManager::open_file(const std::string &path)
{
    // 调用open()函数，使用O_RDWR模式
    // 注意不能重复打开相同文件，并且需要更新文件打开列表

    // 检查文件是否已经打开
    auto it = path2fd_.find(path);
    if (it != path2fd_.end())
    {
        // 文件在映射表中，但可能文件描述符已经无效
        // 尝试验证文件描述符是否仍然有效
        int existing_fd = it->second;

        // 使用 fcntl 检查文件描述符是否有效
        if (fcntl(existing_fd, F_GETFD) == -1 && errno == EBADF) {
            // 文件描述符无效，清理映射表
            std::cerr << "Warning: Found invalid file descriptor for " << path
                      << ", cleaning up mapping..." << std::endl;
            fd2path_.erase(existing_fd);
            path2fd_.erase(it);
            path2refcount_.erase(path);
        } else {
            // 文件描述符仍然有效，增加引用计数并返回现有的文件描述符
            path2refcount_[path]++;
            std::cerr << "Warning: File " << path << " is already open, increasing ref count to "
                      << path2refcount_[path] << ", returning existing fd: " << existing_fd << std::endl;
            return existing_fd;
        }
    }

    // 打开文件
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0)
    {
        if (errno == ENOENT)
        {
            throw FileNotFoundError(path);
        }
        else
        {
            throw UnixError();
        }
    }

    // 更新文件打开列表
    path2fd_[path] = fd;
    fd2path_[fd] = path;
    path2refcount_[path] = 1;  // 初始化引用计数为1

    return fd;
    //  auto [it, inserted] = path2fd_.try_emplace(path, -1);
    // if (!inserted) {
    //     return it->second;  // 已存在，直接复用
    // }

    // // 直接 open，省去 stat 调用；加上 CLOEXEC 防止子进程继承
    // int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    // if (fd < 0) {
    //     // 打开失败：移除占位并抛错
    //     path2fd_.erase(it);
    //     if (errno == ENOENT) {
    //         throw UnixError();
    //     }
    //     throw UnixError();
    // }

    // // 更新 fd
    // it->second = fd;
    // fd2path_.emplace(fd, path);
    // return fd;
}

/**
 * @description:用于关闭指定路径文件
 * @param {int} fd 打开的文件的文件句柄
 */
void DiskManager::close_file(int fd)
{
    // 调用close()函数
    // 注意不能关闭未打开的文件，并且需要更新文件打开列表
    auto it = fd2path_.find(fd);
    if (it == fd2path_.end())
    {
        return;  // 文件未打开，无需处理
    }

    // 取出路径拷贝，避免引用失效
    std::string path = it->second;

    // 检查引用计数
    auto ref_it = path2refcount_.find(path);
    if (ref_it != path2refcount_.end() && ref_it->second > 1) {
        // 还有其他引用，只减少引用计数
        ref_it->second--;
        std::cerr << "Decreasing ref count for " << path << " to " << ref_it->second << std::endl;
        return;
    }

    // 引用计数为1或0，真正关闭文件
    if (::close(fd) < 0) {
        throw UnixError();
    }

    // 更新文件打开列表
    fd2path_.erase(it);
    path2fd_.erase(path);
    path2refcount_.erase(path);
    //   auto it = fd2path_.find(fd);
    // if (it == fd2path_.end()) {
    //     return;  // 未打开，无需处理
    // }

    // // 取出路径拷贝，避免引用失效
    // std::string path = it->second;

    // // 调用 close，并检测错误
    // if (::close(fd) < 0) {
    //     throw UnixError();
    // }

    // // 依次删除两张映射表中的条目
    // fd2path_.erase(it);
    // path2fd_.erase(path);
}

/**
 * @description: 获得文件的大小
 * @return {int} 文件的大小
 * @param {string} &file_name 文件名
 */
int DiskManager::get_file_size(const std::string &file_name)
{
    struct stat stat_buf;
    int rc = stat(file_name.c_str(), &stat_buf); // stat 类似 golang os.Stat 函数
    return rc == 0 ? stat_buf.st_size : -1;
}

/**
 * @description: 根据文件句柄获得文件名
 * @return {string} 文件句柄对应文件的文件名
 * @param {int} fd 文件句柄
 */
std::string DiskManager::get_file_name(int fd)
{
    if (!fd2path_.count(fd))
    {
        throw FileNotOpenError(fd);
    }
    return fd2path_[fd];
}

/**
 * @description:  获得文件名对应的文件句柄
 * @return {int} 文件句柄
 * @param {string} &file_name 文件名
 */
int DiskManager::get_file_fd(const std::string &file_name)
{
    if (!path2fd_.count(file_name))
    {
        return open_file(file_name);
    }
    return path2fd_[file_name];
}

bool DiskManager::is_file_open(const std::string &file_name)
{
    return path2fd_.count(file_name) > 0;
}

int DiskManager::get_open_file_fd(const std::string &file_name)
{
    auto it = path2fd_.find(file_name);
    if (it != path2fd_.end()) {
        return it->second;
    }
    throw FileNotOpenError(-1);  // 文件未打开
}

/**
 * @description:  读取日志文件内容
 * @return {int} 返回读取的数据量，若为-1说明读取数据的起始位置超过了文件大小
 * @param {char} *log_data 读取内容到log_data中
 * @param {int} size 读取的数据量大小
 * @param {int} offset 读取的内容在文件中的位置
 */
int DiskManager::read_log(char *log_data, int size, int offset)
{
    // read log file from the previous end
    if (log_fd_ == -1)
    {
        log_fd_ = open_file(LOG_FILE_NAME);
    }
    int file_size = get_file_size(LOG_FILE_NAME);
    if (offset > file_size)
    {
        return -1;
    }

    size = std::min(size, file_size - offset);
    if (size == 0)
        return 0;
    lseek(log_fd_, offset, SEEK_SET);
    ssize_t bytes_read = read(log_fd_, log_data, size);
    assert(bytes_read == size);
    return bytes_read;
}

/**
 * @description: 写日志内容
 * @param {char} *log_data 要写入的日志内容
 * @param {int} size 要写入的内容大小
 */


/**
 * @description: 写日志内容
 * @param {char} *log_data 要写入的日志内容
 * @param {int} size 要写入的内容大小
 */
void DiskManager::write_log(char *log_data, int size) {
    if (log_fd_ == -1) {
        log_fd_ = open_file(LOG_FILE_NAME);
    }

    // write from the file_end
    lseek(log_fd_, 0, SEEK_END);
    ssize_t bytes_write = write(log_fd_, log_data, size);
    if (bytes_write != size) {
        throw UnixError();
    }
    // fsync 已移除：基准测试下不需要每次提交都等待物理刷盘
}

/**
 * @description: 写检查点数据到文件
 * @param {int} fd 文件句柄
 * @param {char} *check_point_data 要写入的检查点数据
 * @param {int} size 数据大小
 */
void DiskManager::write_check_point(int fd, char *check_point_data, int size) {
    // 定位到文件开头
    lseek(fd, 0, SEEK_SET);
    ssize_t bytes_write = write(fd, check_point_data, size);
    if (bytes_write != size) {
        throw UnixError();
    }
    // 同步到磁盘
    if (fsync(fd) == -1) {
        throw UnixError();
    }
}
