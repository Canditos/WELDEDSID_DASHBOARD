#pragma once

#include <stddef.h>
#include "../common/Types.h"

namespace SecurityHelpers {
    const char* roleToString(UserRole role);
    bool roleAtLeast(UserRole actual, UserRole required);
    void applyDefaultSecurityConfig(SecurityConfig& config);
    bool isUserConfigured(const SecurityUser& user);
    bool isDefaultCredential(const SecurityUser& user);
    bool validateSecurityConfig(const SecurityConfig& config, char* error, size_t errorSize);
}
