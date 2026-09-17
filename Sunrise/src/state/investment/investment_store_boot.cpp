#include <Windows.h>

#include "../../../resources/resource.h"
#include "../../core/filesystem/path.h"
#include "store_internal.h"

namespace sunrise::state::investment::store {
namespace {

/** Resource views borrow bytes from the loaded DLL. */
bool resource(void* module, int identifier, std::string_view& output) noexcept {
    const auto loadedModule = static_cast<HMODULE>(module);
    const HRSRC found = FindResourceW(loadedModule, MAKEINTRESOURCEW(identifier), RT_RCDATA);
    if (found == nullptr) {
        return false;
    }
    const DWORD size = SizeofResource(loadedModule, found);
    const HGLOBAL loaded = LoadResource(loadedModule, found);
    const auto* bytes =
        loaded != nullptr ? static_cast<const char*>(LockResource(loaded)) : nullptr;
    if (bytes == nullptr || size == 0) {
        return false;
    }
    output = {bytes, size};
    return true;
}

} // namespace

/** Schema and seed data stay in DLL resources; only the persistent database is materialized. */
bool initialize(void* module) noexcept {
    core::path::Buffer directory;
    if (!core::path::artifact_directory(module, directory)
        || !core::path::append(directory, L"\\data")) {
        return false;
    }
    if (!CreateDirectoryW(directory.chars.data(), nullptr)
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    if (!core::path::append(directory, L"\\investment.sqlite3")) {
        return false;
    }
    const int bytes = WideCharToMultiByte(CP_UTF8,
                                          0,
                                          directory.chars.data(),
                                          static_cast<int>(directory.length),
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);
    if (bytes <= 0) {
        return false;
    }
    std::string filename(static_cast<std::size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            0,
                            directory.chars.data(),
                            static_cast<int>(directory.length),
                            filename.data(),
                            bytes,
                            nullptr,
                            nullptr)
        != bytes) {
        return false;
    }
    std::string_view schema;
    std::string_view defaults;
    std::string_view settingsSchema;
    std::string_view settingsDefaults;
    if (!resource(module, IDR_INVESTMENT_SCHEMA, schema)
        || !resource(module, IDR_INVESTMENT_DEFAULTS, defaults)
        || !resource(module, IDR_ACCOUNT_SETTINGS_SCHEMA, settingsSchema)
        || !resource(module, IDR_ACCOUNT_SETTINGS_DEFAULTS, settingsDefaults)
        || !open(filename, schema, defaults, settingsSchema, settingsDefaults)) {
        return false;
    }
    return true;
}

} // namespace sunrise::state::investment::store
