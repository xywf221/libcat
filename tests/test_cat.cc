/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
  +--------------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");          |
  | you may not use this file except in compliance with the License.         |
  | You may obtain a copy of the License at                                  |
  | http://www.apache.org/licenses/LICENSE-2.0                               |
  | Unless required by applicable law or agreed to in writing, software      |
  | distributed under the License is distributed on an "AS IS" BASIS,        |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
  | See the License for the specific language governing permissions and      |
  | limitations under the License. See accompanying LICENSE file.            |
  +--------------------------------------------------------------------------+
  | Author: Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

#include "test.h"

TEST(cat, string)
{
    char *name = cat_sprintf(
        "%s v%d.%d.%d%s%s",
        "libcat", CAT_MAJOR_VERSION, CAT_MINOR_VERSION, CAT_RELEASE_VERSION, CAT_STRLEN(CAT_EXTRA_VERSION) > 0 ? "-" : "", CAT_EXTRA_VERSION
    );
    ASSERT_STREQ(name, "libcat v" CAT_VERSION);
    cat_free(name);
}

TEST(cat_sys, sleep)
{
    ASSERT_EQ(cat_sys_sleep(0), 0);
}

TEST(cat_sys, usleep)
{
    ASSERT_EQ(cat_sys_usleep(1), 0);
}

TEST(cat_sys, nanosleep)
{
    cat_timespec req = { 0, 1000 };
    ASSERT_EQ(cat_sys_nanosleep(&req, NULL), 0);
}

TEST(cat_sys, nanosleep_failed)
{
    cat_timespec req = { 0, INT_MAX };
    ASSERT_EQ(cat_sys_nanosleep(&req, NULL), -1);
    ASSERT_EQ(cat_translate_sys_error(cat_sys_errno), CAT_EINVAL);
}

TEST(cat, runtime_init_with_error_log_stdout)
{
    FILE *original_error_log = cat_get_error_log();
    DEFER(cat_set_error_log(original_error_log));

    cat_env_set("CAT_ERROR_LOG", "stdout");
    cat_runtime_shutdown();
    cat_runtime_init();
    testing::internal::CaptureStdout();
    cat_warn(CORE, "Now error log is stdout");
    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_NE(output.find("Now error log is stdout"), std::string::npos);
}

TEST(cat, runtime_init_with_error_log_stderr)
{
    FILE *original_error_log = cat_get_error_log();
    DEFER(cat_set_error_log(original_error_log));

    cat_env_set("CAT_ERROR_LOG", "stderr");
    cat_runtime_shutdown();
    cat_runtime_init();
    testing::internal::CaptureStderr();
    cat_warn(CORE, "Now error log is stderr");
    std::string output = testing::internal::GetCapturedStderr();
    ASSERT_NE(output.find("Now error log is stderr"), std::string::npos);
}
