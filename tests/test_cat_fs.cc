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
  |         codinghuang <2812240764@qq.com>                                  |
  |         dixyes <dixyes@gmail.com>                                        |
  +--------------------------------------------------------------------------+
 */

#include "cat_fs.h"
#include "test.h"
#include "uv.h"
#include <cstdio>

std::string path_join(std::string a, std::string b){
    if("" == a){
        return b;
    }
    if("" == b){
        return a;
    }
    std::string aa = a;
#ifdef CAT_OS_WIN
    // see https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    if(4 /* "\\\\?\\" */ + a.length() + 1/* '\\' */ + b.length() > 32767){
        // too long
        return "";
    }
    if(a.length() + 1/* '\\' */ + b.length() > MAX_PATH){
        aa = std::string("\\\\?\\") + a;
    }
#endif
    if(0 == aa.compare(aa.length()-1, 1, std::string(TEST_PATH_SEP))){
        return aa+b;
    }
    return aa+std::string(TEST_PATH_SEP)+b;
}

bool no_tmp(){
    static int accessible = access(TEST_TMP_PATH, R_OK | W_OK);
    return 0 != accessible;
}

TEST(cat_fs, path_join)
{
    ASSERT_EQ(std::string("aaa"), path_join("", "aaa")); 
    ASSERT_EQ(std::string("tmp" TEST_PATH_SEP "aaa"), path_join("tmp", "aaa")); 
    ASSERT_EQ(std::string("tmp" TEST_PATH_SEP "aaa"), path_join("tmp" TEST_PATH_SEP, "aaa")); 
}

TEST(cat_fs, open_close_read_write)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    char buf[CAT_BUFFER_DEFAULT_SIZE], red[CAT_BUFFER_DEFAULT_SIZE];
    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_ocrw");
    const char * fn = fnstr.c_str();
    int fd = -1;
    ASSERT_GT(fd = cat_fs_open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0600), 0);
    DEFER({
        cat_fs_unlink(fn);
    });

    ASSERT_LT(cat_fs_read(fd, buf, 5), 0);
    cat_srand(buf, CAT_BUFFER_DEFAULT_SIZE);

    for(int i = 0; i<1; i++){
        ASSERT_EQ(cat_fs_write(fd, buf, CAT_BUFFER_DEFAULT_SIZE), CAT_BUFFER_DEFAULT_SIZE);
    }

    ASSERT_EQ(0, cat_fs_close(fd));

    //printf("%s\n",cat_get_last_error_message());
    ASSERT_GT(fd = cat_fs_open(fn, O_RDONLY, 0600), 0);
    DEFER(cat_fs_close(fd));
    ASSERT_LT(cat_fs_write(fd, "hello", 5), 0);
    
    for(int i = 0; i<128; i++){
        ASSERT_EQ(cat_fs_read(fd, red, CAT_BUFFER_DEFAULT_SIZE), CAT_BUFFER_DEFAULT_SIZE);
        ASSERT_EQ(std::string(buf, CAT_BUFFER_DEFAULT_SIZE), std::string(red, CAT_BUFFER_DEFAULT_SIZE));
    }
}

#define LSEEK_BUFSIZ 32768
TEST(cat_fs, stat_lstat_fstat)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    umask(0);

    char buf[LSEEK_BUFSIZ];
    cat_srand(buf, LSEEK_BUFSIZ);

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_stats");
    const char * fn = fnstr.c_str();
    int fd = -1;
    cat_fs_unlink(fn);
    ASSERT_GT(fd = cat_fs_open(fn, O_RDWR | O_CREAT | O_TRUNC, 0666), 0);
    ASSERT_EQ(cat_fs_write(fd, buf, LSEEK_BUFSIZ), LSEEK_BUFSIZ);
    ASSERT_EQ(cat_fs_close(fd), 0);
    DEFER({
        ASSERT_EQ(cat_fs_unlink(fn), 0);
    });

    cat_stat_t statbuf = {0};
    ASSERT_EQ(cat_fs_stat(fn, &statbuf), 0);
    ASSERT_EQ(statbuf.st_mode & 0777, 0666);
    ASSERT_EQ(statbuf.st_size, LSEEK_BUFSIZ);
    ASSERT_EQ(cat_fs_lstat(fn, &statbuf), 0);
    ASSERT_EQ(statbuf.st_mode & 0777, 0666);
    ASSERT_EQ(statbuf.st_size, LSEEK_BUFSIZ);
}

TEST(cat_fs, link)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_target");
    const char * fn = fnstr.c_str();
    std::string fn2str = path_join(TEST_TMP_PATH, "cat_tests_link");
    const char * fn2 = fn2str.c_str();
    
    DEFER({
        cat_fs_unlink(fn);
        cat_fs_unlink(fn2);
    });

    cat_stat_t statbuf;

    cat_fs_unlink(fn);
    cat_fs_unlink(fn2);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0600)), 0);
    
    if(cat_fs_link(fn, fn2) != 0){
        GTEST_SKIP_("File system may not support hard link");
    }
    ASSERT_EQ(cat_fs_stat(fn2, &statbuf), 0);
    ASSERT_GT(statbuf.st_nlink, 1);
    uint64_t inode = statbuf.st_ino;
    ASSERT_EQ(cat_fs_stat(fn, &statbuf), 0);
    ASSERT_GT(statbuf.st_nlink, 1);
    ASSERT_EQ(inode, statbuf.st_ino);
}

TEST(cat_fs, symlink){
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_target");
    const char * fn = fnstr.c_str();
    std::string fn2str = path_join(TEST_TMP_PATH, "cat_tests_link");
    const char * fn2 = fn2str.c_str();
    
    DEFER({
        cat_fs_unlink(fn);
        cat_fs_unlink(fn2);
    });

    cat_stat_t statbuf;

    cat_fs_unlink(fn);
    cat_fs_unlink(fn2);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0600)), 0);
    if(cat_fs_symlink(fn, fn2, 0) != 0){
        GTEST_SKIP_("File system may not support symbol link");
    }
    ASSERT_EQ(cat_fs_stat(fn2, &statbuf), 0);
    uint64_t inode = statbuf.st_ino;
    ASSERT_EQ(cat_fs_lstat(fn2, &statbuf), 0);
    ASSERT_NE(inode, statbuf.st_ino);
    ASSERT_EQ(statbuf.st_mode & S_IFLNK, S_IFLNK);
}

#ifdef CAT_OS_WIN
TEST(cat_fs, symlink_windows){
    SKIP_IF_(no_tmp(), "Temp dir not writable");
    GTEST_SKIP_("Test not yet implemented");
}
#endif

TEST(cat_fs, lseek)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    char buf[LSEEK_BUFSIZ], red[LSEEK_BUFSIZ];
    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_seek");
    const char * fn = fnstr.c_str();
    int fd = cat_fs_open(fn, O_RDWR | O_CREAT | O_TRUNC, 0600);
    ASSERT_GT(fd, 0);
    DEFER({
        ASSERT_EQ(cat_fs_close(fd), 0);
        ASSERT_EQ(cat_fs_unlink(fn), 0);
    });
    
    cat_srand(buf, LSEEK_BUFSIZ);

    int ret;
    ASSERT_EQ(cat_fs_write(fd, buf, LSEEK_BUFSIZ), LSEEK_BUFSIZ);
    ASSERT_EQ(cat_fs_lseek(fd, 0, SEEK_SET), 0);
    ASSERT_EQ(cat_fs_lseek(fd, 10, SEEK_SET), 10);
    ASSERT_GT(ret = cat_fs_lseek(fd, 0, SEEK_END), 0);
    ASSERT_EQ(cat_fs_lseek(fd, -10, SEEK_END), ret - 10);
    ASSERT_EQ(cat_fs_lseek(fd, 0, SEEK_CUR), ret - 10);
    ASSERT_EQ(cat_fs_lseek(fd, -10, SEEK_CUR), ret - 20);
    ASSERT_EQ(cat_fs_lseek(fd, 0, SEEK_SET), 0);
    ASSERT_EQ(cat_fs_read(fd, red, LSEEK_BUFSIZ), LSEEK_BUFSIZ);
    ASSERT_EQ(std::string(buf, LSEEK_BUFSIZ), std::string(red, LSEEK_BUFSIZ));
}

TEST(cat_fs, access)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_access");
    const char * fn = fnstr.c_str();
    int fd = -1;
    DEFER({
        cat_fs_unlink(fn);
    });
    cat_fs_unlink(fn);
    ASSERT_GT(fd = cat_fs_open(fn, O_RDONLY | O_CREAT, 0400), 0);
    ASSERT_EQ(cat_fs_access(fn, R_OK), 0);
    ASSERT_LT(cat_fs_access(fn, X_OK|W_OK), 0);
    ASSERT_EQ(cat_fs_close(fd), 0);

    cat_fs_unlink(fn);
    ASSERT_GT(fd = cat_fs_open(fn, O_RDWR | O_CREAT, 0700), 0);
    ASSERT_EQ(cat_fs_access(fn, W_OK|X_OK|R_OK), 0);
    ASSERT_EQ(cat_fs_close(fd), 0);
}

TEST(cat_fs, mkdir_rmdir)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_dir");
    const char * fn = fnstr.c_str();
    std::string fn2str = path_join(fnstr, "cat_tests_dir2");
    const char * fn2 = fn2str.c_str();
    DEFER({
        cat_fs_rmdir(fn2);
        cat_fs_rmdir(fn);
    });
    cat_fs_rmdir(fn2);
    cat_fs_rmdir(fn);
    ASSERT_LT(cat_fs_mkdir(fn2, 0777), 0);
    ASSERT_EQ(cat_get_last_error_code(), CAT_ENOENT);
    ASSERT_EQ(cat_fs_mkdir(fn, 0777), 0);
    ASSERT_LT(cat_fs_mkdir(fn, 0777), 0);
    ASSERT_EQ(cat_get_last_error_code(), CAT_EEXIST);
    ASSERT_EQ(cat_fs_mkdir(fn2, 0777), 0);
    ASSERT_LT(cat_fs_rmdir(fn), 0);
    ASSERT_EQ(cat_get_last_error_code(), CAT_ENOTEMPTY);
    ASSERT_EQ(cat_fs_rmdir(fn2), 0);
    ASSERT_EQ(cat_fs_rmdir(fn), 0);
    ASSERT_LT(cat_fs_rmdir(fn2), 0);
    ASSERT_EQ(cat_get_last_error_code(), CAT_ENOENT);
}

TEST(cat_fs, opendir_readdir_readdirs_closedir)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_readdir");
    const char * fn = fnstr.c_str();
    std::string fndstr = path_join(fnstr, "cat_tests_readdir_dir1");
    const char * fnd = fndstr.c_str();
    std::string fnd2str = path_join(fnstr, "cat_tests_readdir_dir2");
    const char * fnd2 = fnd2str.c_str();
    std::string fnf3str = path_join(fnstr, "cat_tests_readdir_file3");
    const char * fnf3 = fnf3str.c_str();
    DEFER({
        cat_fs_rmdir(fnd);
        cat_fs_rmdir(fnd2);
        cat_fs_unlink(fnf3);
        cat_fs_rmdir(fn);
    });
    cat_fs_rmdir(fnd);
    cat_fs_rmdir(fnd2);
    cat_fs_unlink(fnf3);
    cat_fs_rmdir(fn);

    ASSERT_EQ(cat_fs_mkdir(fn, 0777), 0);
    ASSERT_EQ(cat_fs_mkdir(fnd, 0777), 0);
    ASSERT_EQ(cat_fs_mkdir(fnd2, 0777), 0);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fnf3, O_RDONLY | O_CREAT | O_TRUNC, 0400)), 0);

    cat_dir_t * dir;
    ASSERT_NE(dir = cat_fs_opendir(fn), nullptr);

    cat_dirent_t *dirent1 = NULL, *dirent2 = NULL, *dirent3 = NULL;
    DEFER({
        if(dirent1){
            free((void*)dirent1->name);
            free((void*)dirent1);
        }
        if(dirent2){
            free((void*)dirent2->name);
            free((void*)dirent2);
        }
        if(dirent3){
            free((void*)dirent3->name);
            free((void*)dirent3);
        }
    });

    ASSERT_NE(dirent1 = cat_fs_readdir(dir), nullptr);
    ASSERT_EQ(std::string(dirent1->name, sizeof("cat_tests_readdir_")-1), std::string("cat_tests_readdir_"));
    ASSERT_NE(dirent2 = cat_fs_readdir(dir), nullptr);
    ASSERT_EQ(std::string(dirent2->name, sizeof("cat_tests_readdir_")-1), std::string("cat_tests_readdir_"));
    ASSERT_NE(dirent3 = cat_fs_readdir(dir), nullptr);
    ASSERT_EQ(std::string(dirent3->name, sizeof("cat_tests_readdir_")-1), std::string("cat_tests_readdir_"));
    ASSERT_NE(std::string(dirent1->name), std::string(dirent2->name));
    ASSERT_NE(std::string(dirent1->name), std::string(dirent3->name));
    ASSERT_NE(std::string(dirent2->name), std::string(dirent3->name));
    //printf("%s\n", dirent1->name);
    //printf("%s\n", dirent2->name);
    //printf("%s\n", dirent3->name);
    free((void*)dirent1->name);
    free((void*)dirent1);
    ASSERT_EQ(dirent1 = cat_fs_readdir(dir), nullptr);
    ASSERT_EQ(cat_fs_closedir(dir), 0);

    cat_dirent_t *dirents1 = (cat_dirent_t *)malloc(sizeof(*dirents1));
    memset(dirents1, 0, sizeof(*dirents1));
    cat_dirent_t *dirents2 = (cat_dirent_t *)malloc(sizeof(*dirents2)*2);
    memset(dirents2, 0, sizeof(*dirents2)*2);
    DEFER({
        if(dirents1){
            if(dirents1[0].name){
                free((void*)dirents1[0].name);
            }
            free(dirents1);
        }
        if(dirents2){
            if(dirents2[0].name){
                free((void*)dirents2[0].name);
            }
            if(dirents2[1].name){
                free((void*)dirents2[1].name);
            }
            free(dirents2);
        }
    });

    ASSERT_NE(dir = cat_fs_opendir(fn), nullptr);
    ASSERT_EQ(cat_fs_readdirs(dir, dirents2, 2), 2);
    ASSERT_EQ(std::string(dirents2[0].name, sizeof("cat_tests_readdir_")-1), std::string("cat_tests_readdir_"));
    ASSERT_EQ(std::string(dirents2[1].name, sizeof("cat_tests_readdir_")-1), std::string("cat_tests_readdir_"));
    ASSERT_EQ(cat_fs_readdirs(dir, dirents1, 1), 1);
    ASSERT_EQ(std::string(dirents1[0].name, sizeof("cat_tests_readdir_")-1), std::string("cat_tests_readdir_"));
    ASSERT_NE(std::string(dirents1[0].name), std::string(dirents2[0].name));
    ASSERT_NE(std::string(dirents1[0].name), std::string(dirents2[1].name));
    ASSERT_NE(std::string(dirents2[0].name), std::string(dirents2[1].name));
    free((void*)dirents2[0].name);
    free((void*)dirents2[1].name);
    memset(dirents2, 0, sizeof(*dirents2)*2);
    ASSERT_EQ(cat_fs_readdirs(dir, dirents2, 2), 0);
    ASSERT_EQ(cat_fs_closedir(dir), 0);
}

static int cat_test_nofile_filter(const cat_dirent_t * x){
    return x->type != CAT_DIRENT_FILE;
}

static int cat_test_reverse_compare(const cat_dirent_t * a, const cat_dirent_t * b){
    //printf("a: %p %s\n", a->name, a->name);
    //printf("b: %p %s\n", b->name, b->name);
    return -strcoll(a->name, b->name);
}

TEST(cat_fs, scandir)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_dir");
    const char * fn = fnstr.c_str();
    std::string fndstr = path_join(fnstr, "cat_tests_dir_dir1");
    const char * fnd = fndstr.c_str();
    std::string fnd2str = path_join(fnstr, "cat_tests_dir_dir2");
    const char * fnd2 = fnd2str.c_str();
    std::string fnf3str = path_join(fnstr, "cat_tests_dir_file3");
    const char * fnf3 = fnf3str.c_str();
    DEFER({
        cat_fs_rmdir(fnd);
        cat_fs_rmdir(fnd2);
        cat_fs_unlink(fnf3);
        cat_fs_rmdir(fn);
    });
    cat_fs_rmdir(fnd);
    cat_fs_rmdir(fnd2);
    cat_fs_unlink(fnf3);
    cat_fs_rmdir(fn);

    ASSERT_EQ(cat_fs_mkdir(fn, 0777), 0);
    ASSERT_EQ(cat_fs_mkdir(fnd, 0777), 0);
    ASSERT_EQ(cat_fs_mkdir(fnd2, 0777), 0);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fnf3, O_RDONLY | O_CREAT | O_TRUNC, 0400)), 0);

    int ret = -1;
    cat_dirent_t * namelist;
    ASSERT_EQ(ret = cat_fs_scandir(fn, &namelist, cat_test_nofile_filter, cat_test_reverse_compare), 2);
    DEFER({
        for(int i = 0 ;i < ret; i++){
            free((void*)namelist[i].name);
        }
        free(namelist);
    });
    ASSERT_EQ(std::string("cat_tests_dir_dir2"), std::string(namelist[0].name));
    ASSERT_EQ(std::string("cat_tests_dir_dir1"), std::string(namelist[1].name));
    
}

TEST(cat_fs, rename)
{
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string dstr = path_join(TEST_TMP_PATH, "cat_tests_dir");
    const char * d = dstr.c_str();
    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_rename1");
    const char * fn = fnstr.c_str();
    std::string fn2str = path_join(TEST_TMP_PATH, "cat_tests_rename2");
    const char * fn2 = fn2str.c_str();
    std::string fn3str = path_join(dstr, "cat_tests_rename3");
    const char * fn3 = fn3str.c_str();
    DEFER({
        cat_fs_unlink(fn3);
        cat_fs_unlink(fn2);
        cat_fs_unlink(fn);
        cat_fs_rmdir(d);
    });
    cat_fs_unlink(fn3);
    cat_fs_unlink(fn2);
    cat_fs_unlink(fn);
    cat_fs_rmdir(d);

    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_WRONLY | O_CREAT, 0321)), 0);
    ASSERT_EQ(cat_fs_mkdir(d, 0777), 0);
    ASSERT_EQ(cat_fs_rename(fn, fn2), 0);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_WRONLY | O_CREAT, 0321)), 0);
    ASSERT_EQ(cat_fs_rename(fn, fn2), 0);
    ASSERT_LT(cat_fs_rename(fn, fn3), 0);
    ASSERT_EQ(cat_get_last_error_code(), CAT_ENOENT);
    ASSERT_EQ(cat_fs_rename(fn2, fn3), 0);
}

TEST(cat_fs, readlink){
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_target");
    const char * fn = fnstr.c_str();
    std::string fn2str = path_join(TEST_TMP_PATH, "cat_tests_link");
    const char * fn2 = fn2str.c_str();
    
    DEFER({
        cat_fs_unlink(fn);
        cat_fs_unlink(fn2);
    });

    cat_fs_unlink(fn);
    cat_fs_unlink(fn2);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0600)), 0);
    if(cat_fs_symlink(fn, fn2, 0) != 0){
        GTEST_SKIP_("File system may not support symbol link");
    }
    char buf[4096] = {0};
    ASSERT_EQ(cat_fs_readlink(fn2, buf, 1), 1);
    ASSERT_NE(buf[0], 0);
    ASSERT_EQ(buf[1], 0);

    ASSERT_GT(cat_fs_readlink(fn2, buf, 4096), 0);
    ASSERT_EQ(std::string(buf), fn);
}

TEST(cat_fs, realpath){
    SKIP_IF_(no_tmp(), "Temp dir not writable");

    std::string dstr = path_join(TEST_TMP_PATH, "cat_tests_dir");
    const char * d = dstr.c_str();
    std::string fnstr = path_join(dstr, "cat_tests_target");
    const char * fn = fnstr.c_str();
    std::string fn2str = path_join(dstr, "cat_tests_link");
    const char * fn2 = fn2str.c_str();

    std::string pathstr = path_join(TEST_TMP_PATH, "cat_tests_dir" TEST_PATH_SEP ".." TEST_PATH_SEP "cat_tests_dir" TEST_PATH_SEP "cat_tests_link");
    const char * testpath = pathstr.c_str();
    
    DEFER({
        cat_fs_unlink(fn);
        cat_fs_unlink(fn2);
        cat_fs_rmdir(d);
    });

    cat_fs_unlink(fn);
    cat_fs_unlink(fn2);
    cat_fs_rmdir(d);
    ASSERT_EQ(cat_fs_mkdir(d, 0777), 0);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0600)), 0);
    if(cat_fs_symlink(fn, fn2, 0) != 0){
        GTEST_SKIP_("File system may not support symbol link");
    }
    char buf[4096] = {0};
    char * ret = nullptr;
    ASSERT_NE(ret = cat_fs_realpath(testpath, nullptr), nullptr);
    free(ret);
    ASSERT_EQ(ret = cat_fs_realpath(testpath, buf), buf);
    std::string teststr = std::string(buf);
    ASSERT_EQ(ret = cat_fs_realpath(fn, buf), buf);
    std::string origstr = std::string(buf);
    ASSERT_EQ(teststr, origstr);
}

TEST(cat_fs, chmod_fchmod){
    SKIP_IF_(no_tmp(), "Temp dir not writable");
    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_chmod");
    const char * fn = fnstr.c_str();
    cat_fs_unlink(fn);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0200)), 0);
    DEFER({cat_fs_unlink(fn);});

    ASSERT_EQ(cat_fs_chmod(fn, 0600), 0);
    ASSERT_EQ(cat_fs_chmod(fn, 0400), 0);
    int fd = -1;
    ASSERT_GT(fd = cat_fs_open(fn, O_RDONLY), 0);
    ASSERT_EQ(cat_fs_fchmod(fd, 0600), 0);
    ASSERT_EQ(cat_fs_fchmod(fd, 0200), 0);
}

TEST(cat_fs, chown_fchown_lchown){
    SKIP_IF_(no_tmp(), "Temp dir not writable");
    std::string fnstr = path_join(TEST_TMP_PATH, "cat_tests_chown");
    const char * fn = fnstr.c_str();
    std::string fn2str = path_join(TEST_TMP_PATH, "cat_tests_chownlink");
    const char * fn2 = fn2str.c_str();
    cat_fs_unlink(fn);
    cat_fs_unlink(fn2);
    ASSERT_EQ(cat_fs_close(cat_fs_open(fn, O_RDONLY | O_CREAT | O_TRUNC, 0400)), 0);
    ASSERT_EQ(cat_fs_link(fn, fn2), 0);
    DEFER({
        cat_fs_unlink(fn);
        cat_fs_unlink(fn2);
    });

    ASSERT_EQ(cat_fs_chown(fn, -1, -1), 0);
    ASSERT_EQ(cat_fs_chown(fn2, -1, -1), 0);
    ASSERT_EQ(cat_fs_lchown(fn2, -1, -1), 0);
    int fd = -1;
    ASSERT_GT(fd = cat_fs_open(fn, O_RDONLY), 0);
    ASSERT_EQ(cat_fs_fchown(fd, -1, -1), 0);
#ifndef CAT_OS_WIN
    ASSERT_LT(cat_fs_chown("/", 0, 0), 0);
    ASSERT_EQ(cat_get_last_error_code(), CAT_EPERM);
    ASSERT_LT(cat_fs_lchown("/", 0, 0), 0);
    ASSERT_EQ(cat_get_last_error_code(), CAT_EPERM);
#endif
}
