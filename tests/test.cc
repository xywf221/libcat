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
  | Author: Twosee <twose@qq.com>                                            |
  | Author: codinghuang <2812240764@qq.com>                                  |
  +--------------------------------------------------------------------------+
 */

#include "test.h"

namespace testing
{
    cat_timeout_t CONFIG_IO_TIMEOUT = (!is_valgrind() ? 10 : 60) * 1000;
    uint32_t CONFIG_MAX_REQUESTS = !is_valgrind() ? 256 : 64;
    uint32_t CONFIG_MAX_CONCURRENCY = !is_valgrind() ? 128 : 8;
}

class BootstrapEnvironment : public testing::Environment
{
public:
    virtual void SetUp()
    {
        ASSERT_TRUE(cat_init_all());

        cat_socket_set_global_timeout(dns, TEST_IO_TIMEOUT);
        cat_socket_set_global_timeout(accept, -1);
        cat_socket_set_global_timeout(connect, TEST_IO_TIMEOUT);
        cat_socket_set_global_timeout(read, TEST_IO_TIMEOUT);
        cat_socket_set_global_timeout(write, TEST_IO_TIMEOUT);

        ASSERT_TRUE(cat_run(CAT_RUN_EASY));

        ASSERT_NE(cat_coroutine_get_current(), nullptr);
        ASSERT_TRUE(cat_event_is_running());
    }

    virtual void TearDown()
    {
        cat_stop();
        ASSERT_EQ(cat_coroutine_get_active_count() , 1);
        cat_shutdown_all();
    }
};

int main(int argc, char *argv[])
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ::testing::AddGlobalTestEnvironment(new BootstrapEnvironment);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(test, defer)
{
    bool foo = false;
    DEFER(ASSERT_TRUE(foo));
    foo = true;
}
