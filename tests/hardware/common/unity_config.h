#ifndef NSX_HW_UNITY_CONFIG_H
#define NSX_HW_UNITY_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif
void nsx_test_putchar(int ch);
#ifdef __cplusplus
}
#endif
#define UNITY_OUTPUT_CHAR(ch) nsx_test_putchar(ch)
#define UNITY_EXCLUDE_FLOAT
#define UNITY_EXCLUDE_DOUBLE
#endif
