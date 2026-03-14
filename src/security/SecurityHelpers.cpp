#include "SecurityHelpers.h"

#include <string.h>
#include "../common/Constants.h"

namespace {
void copyCString(char* dest, size_t destSize, const char* src) {
    if (destSize == 0) return;
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

void setDefaultUser(SecurityUser& user, UserRole role, const char* username, const char* password) {
    memset(&user, 0, sizeof(SecurityUser));
    user.role = role;
    user.enabled = true;
    copyCString(user.username, sizeof(user.username), username);
    copyCString(user.password, sizeof(user.password), password);
}

bool isDuplicateUsername(const SecurityConfig& config, size_t currentIndex, const char* username) {
    for (size_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        if (i == currentIndex) continue;
        const SecurityUser& other = config.users[i];
        if (!other.enabled) continue;
        if (strcmp(other.username, username) == 0) {
            return true;
        }
    }
    return false;
}
}

namespace SecurityHelpers {
const char* roleToString(UserRole role) {
    switch (role) {
        case UserRole::VIEWER: return "viewer";
        case UserRole::OPERATOR: return "operator";
        case UserRole::ADMIN: return "admin";
        default: return "unknown";
    }
}

bool roleAtLeast(UserRole actual, UserRole required) {
    return static_cast<int>(actual) >= static_cast<int>(required);
}

void applyDefaultSecurityConfig(SecurityConfig& config) {
    memset(&config, 0, sizeof(SecurityConfig));
    setDefaultUser(config.users[0], UserRole::ADMIN, Config::ADMIN_USER, Config::ADMIN_PASS);
    setDefaultUser(config.users[1], UserRole::OPERATOR, Config::OPERATOR_USER, Config::OPERATOR_PASS);
    setDefaultUser(config.users[2], UserRole::VIEWER, Config::VIEWER_USER, Config::VIEWER_PASS);
}

bool isUserConfigured(const SecurityUser& user) {
    return user.enabled && user.username[0] != '\0' && user.password[0] != '\0';
}

bool isDefaultCredential(const SecurityUser& user) {
    switch (user.role) {
        case UserRole::ADMIN:
            return strcmp(user.username, Config::ADMIN_USER) == 0 && strcmp(user.password, Config::ADMIN_PASS) == 0;
        case UserRole::OPERATOR:
            return strcmp(user.username, Config::OPERATOR_USER) == 0 && strcmp(user.password, Config::OPERATOR_PASS) == 0;
        case UserRole::VIEWER:
            return strcmp(user.username, Config::VIEWER_USER) == 0 && strcmp(user.password, Config::VIEWER_PASS) == 0;
        default:
            return false;
    }
}

bool validateSecurityConfig(const SecurityConfig& config, char* error, size_t errorSize) {
    if (error != nullptr && errorSize > 0) {
        error[0] = '\0';
    }

    bool hasAdmin = false;
    for (size_t i = 0; i < Config::SECURITY_USER_COUNT; ++i) {
        const SecurityUser& user = config.users[i];
        if (user.role == UserRole::ADMIN && user.enabled) {
            hasAdmin = true;
        }

        if (!user.enabled) continue;

        if (user.username[0] == '\0') {
            copyCString(error, errorSize, "Enabled users must have a username");
            return false;
        }

        if (strlen(user.password) < 8) {
            copyCString(error, errorSize, "All enabled users must have passwords with at least 8 characters");
            return false;
        }

        if (isDuplicateUsername(config, i, user.username)) {
            copyCString(error, errorSize, "Usernames must be unique");
            return false;
        }
    }

    if (!hasAdmin) {
        copyCString(error, errorSize, "At least one admin user must remain enabled");
        return false;
    }

    return true;
}
}
