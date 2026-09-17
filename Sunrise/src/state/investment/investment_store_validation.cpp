#include "../entitlements/validation.h"
#include "store_internal.h"

namespace sunrise::state::investment::store {

/** Startup refuses an incomplete or corrupt save without replacing it with defaults. */
bool validate() noexcept {
    Transaction transaction;
    if (!transaction.ready()) {
        return false;
    }
    Statement integrity("PRAGMA quick_check");
    std::string_view verdict;
    if (integrity.step() != SQLITE_ROW || !integrity.text(0, verdict) || verdict != "ok"
        || integrity.step() != SQLITE_DONE) {
        return false;
    }
    Statement references("PRAGMA foreign_key_check");
    if (references.step() != SQLITE_DONE) {
        return false;
    }
    AccountState account;
    Family5State family;
    entitlements::Table ownership;
    if (!read_account(account) || !read_family5(family) || !read_entitlements(ownership)
        || !entitlements::valid(ownership)) {
        return false;
    }
    for (std::size_t character = 0; character < kCharacterCapacity; ++character) {
        unlocks::Table banks;
        if (!read_unlocks(banks, static_cast<int>(character))) {
            return false;
        }
    }
    return transaction.commit();
}

} // namespace sunrise::state::investment::store
