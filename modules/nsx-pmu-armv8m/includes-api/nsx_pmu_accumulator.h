/**
 * @file    nsx_pmu_acc_matrix.h
 * @author Ambiq
 * @brief   Compact 2‑D PMU accumulator: (op × event) → uint32_t
 * @version 1.0.0
 *
 * USAGE (one model, collected across several invocations):
 *
 *     #define OPS      128                         // layers / operators
 *     #define EVENTS   NSX_PMU_MAP_SIZE             // 70 on current M55 targets
 *     static uint32_t backing[OPS * EVENTS];
 *
 *     nsx_pmu_accm_t h = nsx_pmu_accm_create(OPS, EVENTS, backing);
 *
 *     while (!nsx_pmu_accm_complete(h)) {
 *         nsx_pmu_accm_inference_begin(h);
 *         ...
 *         for (uint16_t op = 0; op < OPS_THIS_RUN; ++op) {
 *             nsx_pmu_accm_op_begin(h, op);
 *             run_layer(op);
 *             nsx_pmu_accm_op_end  (h, op);
 *         }
 *         nsx_pmu_accm_inference_end(h);
 *     }
 *
 *     uint32_t *m; nsx_pmu_accm_get(h, &m);     // m[op * EVENTS + event]
 */

 #ifndef NSX_PMU_ACC_MATRIX_H
 #define NSX_PMU_ACC_MATRIX_H

 #include <stdint.h>
 #include <stdbool.h>

 /* ---------- Build‑time limits (override in project‑level flags) ---------- */
 #ifndef NSX_PMU_MAX_ACTIVE_MATRICES
 #define NSX_PMU_MAX_ACTIVE_MATRICES 1            /* simultaneous matrices   */
 #endif

 #ifndef NSX_PMU_MAX_OPS
    // Match TFLM kMaxEvents
    #if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
        #define NSX_PMU_MAX_OPS 4096 /* rows / operators        */
    #else
        #define NSX_PMU_MAX_OPS 2048
    #endif
#endif

 #ifdef __cplusplus
 extern "C" {
 #endif

 /* ---------- Public types ------------------------------------------------- */
 typedef struct { uint8_t id; } nsx_pmu_accm_t;       /* opaque handle        */

 /* ---------- Mandatory external symbols ---------------------------------- */
 #ifndef NSX_PMU_MAP_SIZE                   /* comes from nsx_pmu_map.h        */
 #define NSX_PMU_MAP_SIZE 70
 #endif

 /* ---------- Helper macro ------------------------------------------------- */
 #define NSX_PMU_MATRIX_BYTES(_ops,_evt) ( (_ops) * (_evt) * sizeof(uint32_t) )

 /* ---------- Life‑cycle --------------------------------------------------- */
 nsx_pmu_accm_t nsx_pmu_accm_create(uint16_t ops,
                                  uint16_t events,
                                  void    *backing_buf);

 void nsx_pmu_accm_destroy(nsx_pmu_accm_t h);

 /* ---------- Inference‑level framing ------------------------------------- */
 void nsx_pmu_accm_inference_begin(nsx_pmu_accm_t h);
 void nsx_pmu_accm_inference_end  (nsx_pmu_accm_t h);

 /* ---------- Op / layer framing ------------------------------------------ */
 void nsx_pmu_accm_op_begin(nsx_pmu_accm_t h, uint16_t op_idx);
 void nsx_pmu_accm_op_end  (nsx_pmu_accm_t h, uint16_t op_idx);

 /* ---------- Tag resolution helper (for TFLM) ---------------------------- */
 uint16_t nsx_pmu_accm_resolve_tag(nsx_pmu_accm_t h, const char *tag);
 uint16_t nsx_pmu_accm_find_tag   (nsx_pmu_accm_t h, const char *tag);

 /* ---------- Query -------------------------------------------------------- */
 bool     nsx_pmu_accm_complete(nsx_pmu_accm_t h);
 void     nsx_pmu_accm_get     (nsx_pmu_accm_t h, uint32_t **matrix);
 void     nsx_pmu_accmprint_matrix(nsx_pmu_accm_t h, uint32_t *matrix, uint16_t ops, uint16_t events);

 /* Return a single layer (row) worth of counters into out[].
  * Copies min(events, caller_capacity) entries starting at the given layer.
  */
 void     nsx_pmu_accm_get_layer(nsx_pmu_accm_t h,
                                uint16_t     layer,
                                uint32_t*    out,
                                uint16_t
                                     caller_capacity);
 #ifdef __cplusplus
 }
 #endif
 #endif /* NSX_PMU_ACC_MATRIX_H */
