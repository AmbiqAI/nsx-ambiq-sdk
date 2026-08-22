/**
 * @brief Common staged power-profile dump helpers.
 *
 * The register rows are platform-specific and live in
 * src/<arch>/nsx_power_profile.c; the print plumbing below is shared by
 * every SoC family.
 */

#include "nsx_core.h"
#include "nsx_power.h"
#include "nsx_power_internal.h"

static void nsx_power_profile_print_json(
    const nsx_power_profile_reg_t *regs,
    size_t count,
    uint32_t snapshot_index) {
    nsx_printf("{\n");
    nsx_printf("  \"snapshotIndex\": %lu,\n", (unsigned long)snapshot_index);
    nsx_printf("  \"registers\": [\n");

    for (size_t index = 0; index < count; ++index) {
        const nsx_power_profile_reg_t *reg = &regs[index];
        const char *comma = (index + 1u < count) ? "," : "";

        nsx_printf("    {\"name\": \"%s\", \"value\": \"0x%08lX\", \"description\": \"%s\"}%s\n",
            reg->name,
            (unsigned long)(*reg->addr),
            reg->description,
            comma);
    }

    nsx_printf("  ]\n");
    nsx_printf("}\n");
}

static void nsx_power_profile_print_csv(
    const nsx_power_profile_reg_t *regs,
    size_t count) {
    nsx_printf("REGNAME,VALUE,DESCRIPTION\n");
    for (size_t index = 0; index < count; ++index) {
        const nsx_power_profile_reg_t *reg = &regs[index];
        nsx_printf("%s,0x%08lX,%s\n",
            reg->name,
            (unsigned long)(*reg->addr),
            reg->description);
    }
}

uint32_t nsx_power_profile_dump(
    uint32_t snapshot_index,
    nsx_power_profile_format_t format) {
    if ((format != NSX_POWER_PROFILE_FORMAT_JSON) &&
        (format != NSX_POWER_PROFILE_FORMAT_CSV)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    size_t count = 0;
    const nsx_power_profile_reg_t *regs = nsx_power_profile_platform_regs(&count);

    if ((regs == NULL) || (count == 0u)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    if (format == NSX_POWER_PROFILE_FORMAT_CSV) {
        nsx_power_profile_print_csv(regs, count);
        return NSX_STATUS_SUCCESS;
    }

    nsx_power_profile_print_json(regs, count, snapshot_index);
    return NSX_STATUS_SUCCESS;
}
