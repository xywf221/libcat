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

TEST(cat_work, base)
{
    SKIP_IF_USE_VALGRIND();
    bool done = false;

    static int buckets[10] = { };
    cat_msec_t s = cat_time_msec();

    for (int n = 0; n < 10; n++) {
        co([&, n] {
            ASSERT_TRUE(work(CAT_WORK_KIND_SLOW_IO, [n] {
                usleep((n + 1) * 5000); /* 5ms ~ 50ms */
                buckets[n] = n;
            }, 500)); /* wait max 500ms */
        });
    }

    s = cat_time_msec() - s;
    EXPECT_LE(s, 10); /* <= 10ms := no-blocking */
    for (int c = 0; c < 5; c++) {
        cat_time_msleep(100);
        done = ([&] {
            for (int n = 0; n < 10; n++) {
                if(buckets[n] != n) {
                    return false;
                }
            }
            return true;
        })();
        if (done) {
            break;
        }
    }
    ASSERT_TRUE(done);
}

TEST(cat_work, timeout)
{
    ASSERT_FALSE(work(CAT_WORK_KIND_SLOW_IO, [] {
        usleep(10 * 1000);
    }, 1));
    ASSERT_EQ(cat_get_last_error_code(), CAT_ETIMEDOUT);
}

TEST(cat_work, cancel)
{
    cat_coroutine_t *coroutine = co([] {
        bool ret = work(CAT_WORK_KIND_SLOW_IO, [] {
            usleep(1000); // 1ms
        }, TEST_IO_TIMEOUT);
        ASSERT_FALSE(ret);
        ASSERT_EQ(cat_get_last_error_code(), CAT_ECANCELED);
    });
    cat_coroutine_resume(coroutine, nullptr, nullptr);
}
