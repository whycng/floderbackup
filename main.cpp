#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <optional>
#include <map>
#include <set>
#include <windows.h>
// #include <openssl/sha.h>  // 需安装 OpenSSL

namespace fs = std::filesystem;

//参数配置
const std::string source_folder = "E:\\Proj\\vsCodeProj\\code\\cppProj\\projBackup\\file01";
const std::string backup_base_folder = "E:\\Proj\\vsCodeProj\\code\\cppProj\\projBackup\\file02";
const std::string backup_store_folder = "E:\\Proj\\vsCodeProj\\code\\cppProj\\projBackup\\_store";//store_folder
// const int interval_minutes = 5;//间隔5分钟备份一次文件夹
const int interval_seconds = 10;//间隔10s检查一次文件夹
const int max_backup_count = 10;//最大备份数量 10份文件夹（时间命名）

// 文件追踪表
std::unordered_map<std::string, fs::file_time_type> file_states;
 
// 计算文件 SHA256
// std::string calculate_file_hash(const fs::path& path) {
//     std::ifstream file(path, std::ios::binary);
//     if (!file) return "";

//     SHA256_CTX ctx;
//     SHA256_Init(&ctx);

//     char buffer[8192];
//     while (file.read(buffer, sizeof(buffer))) {
//         SHA256_Update(&ctx, buffer, file.gcount());
//     }
//     SHA256_Update(&ctx, buffer, file.gcount());

//     unsigned char hash[SHA256_DIGEST_LENGTH];
//     SHA256_Final(hash, &ctx);

//     std::ostringstream oss;
//     for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
//         oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
//     }
//     return oss.str();
// }

std::string simple_hash_path(const fs::path& path) {
    std::hash<std::string> hasher;
    size_t hash_val = hasher(path.string());
    std::stringstream ss;
    ss << std::hex << hash_val;
    return ss.str();
}


// 检查文件是否被修改
bool is_file_modified(const fs::path& old_path, const fs::path& new_path) {
    if (!fs::exists(old_path)) return true;
    auto old_time = fs::last_write_time(old_path);
    auto new_time = fs::last_write_time(new_path);
    return new_time != old_time;
}

// 获取前一个备份文件夹
std::optional<fs::path> get_last_backup(const fs::path& backup_dir) {
    std::vector<fs::path> backups;
    for (auto& entry : fs::directory_iterator(backup_dir)) {
        if (fs::is_directory(entry)) {
            backups.push_back(entry.path());
        }
    }
    if (backups.empty()) return std::nullopt;

    std::sort(backups.begin(), backups.end());
    return backups.back();
}


//获取当前时间戳
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

// 构建文件列表（扁平）
std::vector<fs::path> get_all_files(const fs::path& root) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (fs::is_regular_file(entry)) {
            files.push_back(entry.path());
        }
    }
    return files;
}

// 创建软链接
void create_symlink_safely(const fs::path& target, const fs::path& link) {
    try {
        if (fs::exists(link)) fs::remove(link);
        fs::create_symlink(target, link);
    } catch (const std::exception& e) {
        std::cerr << "[!] 创建软链失败: " << link << " -> " << target << " | " << e.what() << "\n";
    }
}

// 拷贝或软链备份文件
void backup_file(const fs::path& file, const fs::path& backup_folder) {
    auto relative_path = fs::relative(file, source_folder);
    std::string file_hash = simple_hash_path(file);
    // std::string file_hash = calculate_file_hash(file);

    fs::path store_path = fs::path(backup_store_folder) / file_hash.substr(0, 2) / file_hash;
    fs::create_directories(store_path.parent_path());

    if (!fs::exists(store_path)) {
        try {
            fs::copy_file(file, store_path);
        } catch (const std::exception& e) {
            std::cerr << "[!] 拷贝文件到 store 失败: " << e.what() << "\n";
            return;
        }
    }

    fs::path backup_target_path = backup_folder / relative_path;
    fs::create_directories(backup_target_path.parent_path());
    create_symlink_safely(store_path, backup_target_path);
}

// 拷贝单个文件（确保父目录存在）
void copy_file_safely(const fs::path& src, const fs::path& dst) {
    fs::create_directories(dst.parent_path());
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
}

// 遍历源文件夹，判断是否需要备份（新文件或被修改）
std::vector<fs::path> get_files_to_backup() {
    std::vector<fs::path> changed_files;

    for (const auto& entry : fs::recursive_directory_iterator(source_folder)) {
        if (!fs::is_regular_file(entry)) continue;

        const auto& path = entry.path();
        auto last_write_time = fs::last_write_time(path);
        std::string rel_path = fs::relative(path, source_folder).string();

        if (file_states.count(rel_path) == 0 || file_states[rel_path] != last_write_time) {
            changed_files.push_back(path);
            file_states[rel_path] = last_write_time;
        }
    }

    return changed_files;
}

// 清理旧备份文件夹，保留最新 N 个
void clean_old_backups() {
    std::vector<fs::directory_entry> backups;
 
    for (const auto& entry : fs::directory_iterator(backup_base_folder)) {
        if (fs::is_directory(entry)) {
            backups.push_back(entry);
        }
    }

    // 按名称排序（因为文件夹名是时间戳）
    std::sort(backups.begin(), backups.end(), [](const auto& a, const auto& b) {
        return a.path().filename().string() < b.path().filename().string();
    });

    while (backups.size() > max_backup_count) {
        std::cout << u8"[−] 删除旧备份：" << backups.front().path() << "\n";
        fs::remove_all(backups.front().path());
        backups.erase(backups.begin());
    }
}

// void copy_folder(const fs::path& from, const fs::path& to) {
//     fs::create_directories(to);
//     for (const auto& entry : fs::recursive_directory_iterator(from)) {
//         const auto& path = entry.path();
//         auto rel_path = fs::relative(path, from);
//         auto target_path = to / rel_path;

//         if (fs::is_directory(path)) {
//             fs::create_directories(target_path);
//         } else if (fs::is_regular_file(path)) {
//             fs::copy_file(path, target_path, fs::copy_options::overwrite_existing);
//         }
//     }
// }

//主备份任务
void backup_task() {
    std::map<fs::path, std::pair<std::uintmax_t, std::filesystem::file_time_type>> current_snapshot;

    // 1. 构建当前快照：路径 -> (文件大小, 修改时间)
    for (const auto& entry : fs::recursive_directory_iterator(source_folder)) {
        if (fs::is_regular_file(entry)) {
            auto size = fs::file_size(entry);
            auto mtime = fs::last_write_time(entry);
            current_snapshot[entry.path()] = { size, mtime };
        }
    }

    // 2. 检查 _store 中已有文件（用于软链）
    fs::create_directories(backup_store_folder);
    fs::path backup_dir = fs::path(backup_base_folder) / current_timestamp();
    fs::create_directories(backup_dir);

    int copy_count = 0, link_count = 0;

    for (const auto& [path, meta] : current_snapshot) {
        auto rel_path = fs::relative(path, source_folder);
        auto hash_name = simple_hash_path(rel_path);
        fs::path stored_file = fs::path(backup_store_folder) / hash_name;

        // 判断是否需要更新存储
        bool need_update = true;
        if (fs::exists(stored_file)) {
            auto store_mtime = fs::last_write_time(stored_file);
            auto store_size = fs::file_size(stored_file);
            if (store_size == meta.first && store_mtime == meta.second) {
                need_update = false;
            }
        }

        // 若需要，更新_store
        if (need_update) {
            fs::copy_file(path, stored_file, fs::copy_options::overwrite_existing);
            fs::last_write_time(stored_file, meta.second);
            copy_count++;
        }

        // 在新备份文件夹中建立软链
        fs::path target_path = backup_dir / rel_path;
        fs::create_directories(target_path.parent_path());
        create_symlink(stored_file, target_path);
        link_count++;
    }

    std::cout << u8"[✓] 本次备份完成: " << backup_dir << "\n";
    std::cout << u8"    文件总数: " << current_snapshot.size()
              << u8"，拷贝: " << copy_count << u8"，软链: " << link_count << "\n";

    clean_old_backups();
}

// void backup_task() {
//     auto all_files = get_all_files(source_folder);
//     if (all_files.empty()) {
//         std::cout << "[ ] 源目录为空，无需备份。\n";
//         return;
//     }

//     std::string timestamp = current_timestamp();
//     fs::path backup_folder = fs::path(backup_base_folder) / timestamp;
//     fs::create_directories(backup_folder);

//     std::cout << "[+] 备份开始，总文件数: " << all_files.size() << "\n";
//     for (const auto& file : all_files) {
//         backup_file(file, backup_folder);
//     }

//     std::cout << "[✓] 本次备份完成: " << backup_folder << "\n";
// }

// 清理旧备份
// void clean_old_backups() {
//     std::vector<fs::path> backups;
//     for (const auto& entry : fs::directory_iterator(backup_base_folder)) {
//         if (fs::is_directory(entry)) {
//             backups.push_back(entry.path());
//         }
//     }

//     std::sort(backups.begin(), backups.end());
//     while (backups.size() > max_backup_count) {
//         try {
//             fs::remove_all(backups.front());
//             std::cout << "[-] 删除旧备份: " << backups.front() << "\n";
//         } catch (...) {}
//         backups.erase(backups.begin());
//     }
// }

// void backup_task() {
//     auto changed_files = get_files_to_backup();
//     std::set<fs::path> changed_set(changed_files.begin(), changed_files.end());

//     std::string timestamp = current_timestamp();
//     fs::path backup_folder = fs::path(backup_base_folder) / timestamp;
//     fs::create_directories(backup_folder);

//     auto last_backup_opt = get_last_backup(backup_base_folder);
//     fs::path last_backup = last_backup_opt.value_or("");  // 可能为空

//     std::cout << u8"[+] 扫描源目录构建快照...\n";

//     size_t copied = 0;
//     size_t linked = 0;
//     size_t total = 0;

//     for (auto& p : fs::recursive_directory_iterator(source_folder)) {
//         if (!fs::is_regular_file(p)) continue;

//         auto rel_path = fs::relative(p.path(), source_folder);
//         auto target_path = backup_folder / rel_path;
//         fs::create_directories(target_path.parent_path());

//         if (changed_set.find(p.path()) != changed_set.end()) {
//             try {
//                 copy_file_safely(p.path(), target_path);
//                 copied++;
//             } catch (const std::exception& e) {
//                 std::cerr << u8"[x] 拷贝失败: " << p.path() << " => " << e.what() << "\n";
//             }
//         } else if (!last_backup.empty()) {
//             fs::path link_target = fs::relative(last_backup / rel_path, target_path.parent_path());

//             try {
//                 std::error_code ec;
//                 fs::create_symlink(link_target, target_path, ec);
//                 if (ec) {
//                     std::cerr << u8"[!] 创建软链失败: " << target_path << " => " << ec.message() << "\n";
//                 } else {
//                     linked++;
//                 }
//             } catch (const std::exception& e) {
//                 std::cerr << u8"[x] 创建软链异常: " << e.what() << "\n";
//             }
//         }

//         total++;
//     }

//     std::cout << u8"[✓] 本次备份完成: \"" << backup_folder << "\"\n";
//     std::cout << u8"    文件总数: " << total
//               << u8"，拷贝: " << copied
//               << u8"，软链: " << linked << "\n";

//     clean_old_backups();
// }

// void backup_task() {
//     std::string timestamp = current_timestamp();
//     fs::path backup_folder = fs::path(backup_base_folder) / timestamp;
//     fs::create_directories(backup_folder);

//     std::optional<fs::path> last_backup = get_last_backup(backup_base_folder);

//     std::cout << u8"[+] 扫描源目录构建快照...\n";

//     size_t file_count = 0, copied = 0, linked = 0;

//     for (const auto& entry : fs::recursive_directory_iterator(source_folder)) {
//         if (!fs::is_regular_file(entry)) continue;

//         fs::path rel_path = fs::relative(entry.path(), source_folder);
//         fs::path target_path = backup_folder / rel_path;
//         fs::create_directories(target_path.parent_path());

//         bool should_copy = true;

//         if (last_backup) {
//             fs::path prev_file = *last_backup / rel_path;
//             if (fs::exists(prev_file) && !is_file_modified(prev_file, entry.path())) {
//                 try {
//                     fs::create_symlink(prev_file, target_path); // 软链接未变文件
//                     should_copy = false;
//                     ++linked;
//                 } catch (const std::exception& e) {
//                     std::cerr << u8"[x] 创建软链接失败: " << target_path << " => " << e.what() << "\n";
//                 }
//             }
//         }

//         if (should_copy) {
//             try {
//                 fs::copy_file(entry.path(), target_path, fs::copy_options::overwrite_existing);
//                 ++copied;
//             } catch (const std::exception& e) {
//                 std::cerr << u8"[x] 拷贝失败: " << entry.path() << " => " << e.what() << "\n";
//             }
//         }

//         ++file_count;
//     }

//     std::cout << u8"[✓] 本次备份完成: " << backup_folder << "\n";
//     std::cout << u8"    文件总数: " << file_count
//               << u8"，拷贝: " << copied
//               << u8"，软链: " << linked << "\n";

//     clean_old_backups();
// }

// void backup_task() {
//     auto changed_files = get_files_to_backup();
//     if (changed_files.empty()) {
//         std::cout << u8"[ ] 无需备份，未发现更改文件。\n";
//         return;
//     }

//     std::string timestamp = current_timestamp();
//     fs::path backup_folder = fs::path(backup_base_folder) / timestamp;
//     fs::create_directories(backup_folder);

//     std::cout << u8"[+] 备份开始，变更文件数: " << changed_files.size() << "\n";
//     for (const auto& file : changed_files) {
//         auto rel_path = fs::relative(file, source_folder);
//         fs::path target = backup_folder / rel_path;
//         try {
//             copy_file_safely(file, target);
//         } catch (const std::exception& e) {
//             std::cerr << u8"[x] 拷贝失败: " << file << " => " << e.what() << "\n";
//         }
//     }

//     std::cout << u8"[✓] 本次备份完成: " << backup_folder << "\n";
//     clean_old_backups();
// }
// void backup_task() {
//     std::string timestamp = current_timestamp();
//     fs::path target = fs::path(backup_base_folder) / timestamp;
//     std::cout << u8"[+] 备份开始: " << target << std::endl;

//     try {
//         copy_folder(source_folder, target);
//         std::cout << u8"[✓] 备份完成\n";
//     } catch (const std::exception& e) {
//         std::cerr << u8"[x] 备份失败: " << e.what() << std::endl;
//     }
// }

int main() {
    // 设置控制台为UTF-8编码
    SetConsoleOutputCP(CP_UTF8);
    
     std::cout << u8"[*] 智能备份程序启动，每 " << interval_seconds << u8" 秒检查一次变更，最多保留 "
              << max_backup_count << u8" 个备份。\n";

    while (true) {
        backup_task();
        clean_old_backups();
        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
    }
              
    // while (true) {
    //     backup_task();
    //     std::this_thread::sleep_for(std::chrono::minutes(interval_minutes));
    // }

    return 0;
}
