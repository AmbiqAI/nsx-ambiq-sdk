/* Production SPI framing and writer; only HAL/board operations are mocked. */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void SPI_write_byte(uint8_t);
uint8_t SPI_read_byte(void);
#include "device_tx_source.c"

static unsigned credit, requested, sent, headers, transfers, enables, disables;
static unsigned enable_status, header_status, data_status, selects, deselects;
static bool selected, ready=true, bitbang;
static uint8_t payload[260], received[260];

uint32_t am_hal_gpio_input_read(unsigned pin) { assert(pin==AM_BSP_GPIO_EM9305_INT); return ready; }
void am_hal_gpio_state_write(unsigned pin, unsigned state) {
    assert(pin==AM_BSP_GPIO_EM9305_CS);
    selected=state==AM_HAL_GPIO_OUTPUT_CLEAR;
    if(selected) selects++; else deselects++;
}
void am_util_delay_us(uint32_t us) { (void)us; }
void am_util_debug_printf(const char *s, ...) { (void)s; }
uint32_t am_devices_em9305_bus_enable(void *h) { assert(h); enables++; return enable_status; }
uint32_t am_devices_em9305_bus_disable(void *h) { assert(h); disables++; return 0; }
bool am_devices_em9305_spi_bitbang_enabled(void) { return bitbang; }
void SPI_write_byte(uint8_t b) {
    assert(selected && spiTxInProgress);
    if(headers==transfers) { assert(b==0x42); headers++; }
    else { assert(sent<requested); received[sent++]=b; if(sent==requested || sent%credit==0) transfers++; }
}
uint8_t SPI_read_byte(void) { return credit; }
uint32_t am_hal_iom_spi_blocking_fullduplex(void *h, am_hal_iom_transfer_t *t) {
    (void)h;
    assert(selected && spiTxInProgress && t->eDirection==AM_HAL_IOM_FULLDUPLEX);
    assert(t->pui32TxBuffer && t->pui32RxBuffer && !t->bContinue);
    assert((uintptr_t)t->pui32TxBuffer%4==0 && (uintptr_t)t->pui32RxBuffer%4==0);
    if(headers==transfers) {
        assert(t->ui32NumBytes==2);
        assert(((uint8_t *)t->pui32TxBuffer)[0]==0x42 && ((uint8_t *)t->pui32TxBuffer)[1]==0);
        ((uint8_t *)t->pui32RxBuffer)[0]=0xc0;
        ((uint8_t *)t->pui32RxBuffer)[1]=(uint8_t)credit;
        headers++;
        /* Zero credit retries headers; it never starts a payload transfer. */
        if(!credit) transfers++;
        return header_status;
    }
    unsigned expected=credit<requested-sent ? credit : requested-sent;
    assert(t->ui32NumBytes==expected && sent+expected<=sizeof(received));
    memcpy(received+sent,t->pui32TxBuffer,expected);
    /* Real full duplex writes the complete requested RX length. */
    memset(t->pui32RxBuffer,0xa5,t->ui32NumBytes);
    sent+=expected; transfers++;
    return data_status;
}
int main(int argc, char **argv) {
    assert(argc==4); requested=(unsigned)atoi(argv[1]); credit=(unsigned)atoi(argv[2]);
    const char *mode=argv[3];
    assert(requested<=sizeof(payload) && credit<=255);
    for(unsigned i=0;i<sizeof(payload);i++) payload[i]=(uint8_t)(i*19+7);
    if(!strcmp(mode,"enable-error")) enable_status=42;
    else if(!strcmp(mode,"header-error")) header_status=1;
    else if(!strcmp(mode,"data-error")) data_status=1;
    else if(!strcmp(mode,"not-ready")) ready=false;
    else if(!strcmp(mode,"bitbang")) bitbang=true;
    else assert(!strcmp(mode,"ok"));
    am_devices_em9305_t device={0};
    unsigned result=am_devices_em9305_block_write(&device,payload,requested);
    assert(enables==1 && disables==(enable_status ? 0u : 1u));
    assert(!selected && !spiTxInProgress);
    for(unsigned i=0;i<sizeof(payload);i++) assert(payload[i]==(uint8_t)(i*19+7));
    if(enable_status) assert(result==enable_status && !headers && !sent);
    else if(requested>EM9305_BUFFER_SIZE) assert(result==AM_DEVICES_EM9305_DATA_LENGTH_ERROR && !headers && !sent);
    else if(!requested) assert(!result && !headers && !sent);
    else if(header_status || !ready || !credit) {
        assert(result==AM_DEVICES_EM9305_RX_FULL && !sent);
        if(!credit) assert(headers==EM9305_STS_CHK_CNT_MAX);
        if(header_status) assert(headers==1);
        if(!ready) assert(!headers);
    }
    else {
        assert(result==(data_status ? AM_DEVICES_EM9305_DATA_TRANSFER_ERROR : 0));
        assert(sent==requested && memcmp(received,payload,requested)==0);
        assert(headers==(requested+credit-1)/credit && transfers==headers);
        assert(selects==headers && deselects==headers);
    }
    puts("PASS");
}
