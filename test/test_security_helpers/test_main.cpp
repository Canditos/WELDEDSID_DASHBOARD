#include <unity.h>
#include <string.h>

#include "../../src/security/SecurityHelpers.h"
#include "../../src/common/Constants.h"

void test_default_config_has_three_enabled_roles() {
    SecurityConfig config;
    SecurityHelpers::applyDefaultSecurityConfig(config);

    TEST_ASSERT_TRUE(config.users[0].enabled);
    TEST_ASSERT_TRUE(config.users[1].enabled);
    TEST_ASSERT_TRUE(config.users[2].enabled);
    TEST_ASSERT_EQUAL_STRING(Config::ADMIN_USER, config.users[0].username);
    TEST_ASSERT_EQUAL_STRING(Config::OPERATOR_USER, config.users[1].username);
    TEST_ASSERT_EQUAL_STRING(Config::VIEWER_USER, config.users[2].username);
}

void test_role_order_enforces_expected_permissions() {
    TEST_ASSERT_TRUE(SecurityHelpers::roleAtLeast(UserRole::ADMIN, UserRole::OPERATOR));
    TEST_ASSERT_TRUE(SecurityHelpers::roleAtLeast(UserRole::OPERATOR, UserRole::VIEWER));
    TEST_ASSERT_FALSE(SecurityHelpers::roleAtLeast(UserRole::VIEWER, UserRole::OPERATOR));
}

void test_validation_requires_admin_and_unique_usernames() {
    SecurityConfig config;
    SecurityHelpers::applyDefaultSecurityConfig(config);
    config.users[0].enabled = false;

    char error[96];
    TEST_ASSERT_FALSE(SecurityHelpers::validateSecurityConfig(config, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("At least one admin user must remain enabled", error);

    SecurityHelpers::applyDefaultSecurityConfig(config);
    strncpy(config.users[1].username, config.users[0].username, sizeof(config.users[1].username) - 1);
    config.users[1].username[sizeof(config.users[1].username) - 1] = '\0';
    TEST_ASSERT_FALSE(SecurityHelpers::validateSecurityConfig(config, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("Usernames must be unique", error);
}

void test_validation_rejects_short_passwords_for_enabled_users() {
    SecurityConfig config;
    SecurityHelpers::applyDefaultSecurityConfig(config);
    strncpy(config.users[2].password, "short", sizeof(config.users[2].password) - 1);
    config.users[2].password[sizeof(config.users[2].password) - 1] = '\0';

    char error[96];
    TEST_ASSERT_FALSE(SecurityHelpers::validateSecurityConfig(config, error, sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("All enabled users must have passwords with at least 8 characters", error);
}

void test_default_credential_detection_matches_expected_roles() {
    SecurityConfig config;
    SecurityHelpers::applyDefaultSecurityConfig(config);

    TEST_ASSERT_TRUE(SecurityHelpers::isDefaultCredential(config.users[0]));
    TEST_ASSERT_TRUE(SecurityHelpers::isDefaultCredential(config.users[1]));
    TEST_ASSERT_TRUE(SecurityHelpers::isDefaultCredential(config.users[2]));

    strncpy(config.users[1].password, "custompass", sizeof(config.users[1].password) - 1);
    config.users[1].password[sizeof(config.users[1].password) - 1] = '\0';
    TEST_ASSERT_FALSE(SecurityHelpers::isDefaultCredential(config.users[1]));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_default_config_has_three_enabled_roles);
    RUN_TEST(test_role_order_enforces_expected_permissions);
    RUN_TEST(test_validation_requires_admin_and_unique_usernames);
    RUN_TEST(test_validation_rejects_short_passwords_for_enabled_users);
    RUN_TEST(test_default_credential_detection_matches_expected_roles);
    return UNITY_END();
}
