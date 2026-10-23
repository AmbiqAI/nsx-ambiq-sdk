#ifndef NSX_TEST_RESULT_REPORTER_H
#define NSX_TEST_RESULT_REPORTER_H

#ifdef __cplusplus
extern "C" {
#endif
void nsx_test_begin(const char *suite_file);
__attribute__((noreturn)) void nsx_test_finish(void);
#ifdef __cplusplus
}
#endif

#endif
