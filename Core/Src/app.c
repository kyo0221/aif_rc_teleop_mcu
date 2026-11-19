#include "app.h"
#include "main.h"
#include "crsf.h"
#include "teleop.h"

extern FDCAN_HandleTypeDef hfdcan1;
extern UART_HandleTypeDef huart1;

static volatile uint8_t rx_buf[256];
static volatile uint8_t rx_head, rx_tail;
static crsf_parser_t parser;
static teleop_t t;
static uint16_t ch[16];
static uint8_t lq;
static uint32_t last_tx;
static bool can_err;
static bool started;

static const FDCAN_TxHeaderTypeDef tx_header = {
    .Identifier = 0x200,
    .IdType = FDCAN_STANDARD_ID,
    .TxFrameType = FDCAN_DATA_FRAME,
    .DataLength = FDCAN_DLC_BYTES_8,
    .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
    .BitRateSwitch = FDCAN_BRS_OFF,
    .FDFormat = FDCAN_CLASSIC_CAN,
    .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
    .MessageMarker = 0,
};

void app_init(void)
{
    HAL_UARTEx_EnableFifoMode(&huart1);
    SET_BIT(USART1->CR1, USART_CR1_RXNEIE_RXFNEIE);
    HAL_FDCAN_Start(&hfdcan1);
}

void app_uart_irq(void)
{
    uint8_t head = rx_head;
    while (USART1->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t b = (uint8_t)USART1->RDR;
        uint8_t next = (uint8_t)(head + 1);
        if (next != rx_tail) {
            rx_buf[head] = b;
            head = next;
        }
    }
    rx_head = head;
    USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
}

void app_loop(void)
{
    while (rx_tail != rx_head) {
        uint8_t b = rx_buf[rx_tail];
        rx_tail = (uint8_t)(rx_tail + 1);
        crsf_event_t e = crsf_feed(&parser, b, ch, &lq);
        if (e == CRSF_CHANNELS) {
            teleop_on_channels(&t, ch, HAL_GetTick());
        } else if (e == CRSF_LINK) {
            teleop_on_link_quality(&t, lq);
        }
    }

    uint32_t now = HAL_GetTick();
    teleop_tick(&t, now);

    if (now >= 300) {
        started = true;
    }

    if (started && now - last_tx >= 20) {
        FDCAN_ProtocolStatusTypeDef ps;
        last_tx = now;
        HAL_FDCAN_GetProtocolStatus(&hfdcan1, &ps);
        if (ps.BusOff) {
            CLEAR_BIT(hfdcan1.Instance->CCCR, FDCAN_CCCR_INIT);
            can_err = true;
        } else {
            uint8_t data[8];
            teleop_frame(&t, data);
            can_err = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, data) != HAL_OK || ps.ErrorPassive;
        }
    }

    bool blink = (now / 100) % 2;
    HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, (GPIO_PinState)((now / 500) % 2));
    HAL_GPIO_WritePin(MODE_LED_GPIO_Port, MODE_LED_Pin,
                      (GPIO_PinState)(!t.autonomous && (!(t.link && t.sw_auto) || blink)));
    HAL_GPIO_WritePin(FAULT_LED_GPIO_Port, FAULT_LED_Pin,
                      (GPIO_PinState)(can_err ? blink : !t.link));
}
