#define ADDR_APP_PROGRAM 0x800C800
typedef void (*pFunction)(void);

void enter_to_application()
{
    HAL_RCC_DeInit();
    HAL_DeInit();

    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk |
                    SCB_SHCSR_BUSFAULTENA_Msk |
                    SCB_SHCSR_MEMFAULTENA_Msk);

    __set_MSP(*(__IO uint32_t *)ADDR_APP_PROGRAM);

    pFunction app_entry = (pFunction)(*(__IO uint32_t *)(ADDR_APP_PROGRAM + 4));
    app_entry();
}

#define START_BYTE 0xAA
#define ACK 0x79
#define NACK 0x1F
#define BLOCK_SIZE 256

#define START_PAGE 50
#define END_PAGE 127

void flash_erase_range_by_page()
{
    for (uint32_t page = START_PAGE; page <= END_PAGE; page++)
    {
        uint32_t addr = 0x08000000 + page * 1024;
        flash_erase(addr);
    }
}

uint16_t simpleCRC(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        crc += data[i];
    }
    return crc;
}

void send_byte(uint8_t byte)
{
    HAL_UART_Transmit(&huart1, &byte, 1, HAL_MAX_DELAY);
}

uint8_t receive_byte(uint8_t *byte, uint32_t timeout)
{
    return HAL_UART_Receive(&huart1, byte, 1, timeout);
}

uint8_t header[3];
uint8_t data[BLOCK_SIZE];
uint8_t crc_buf[2];
uint16_t crc_calc;
uint16_t crc_recv;
void bootloader_loop()
{
    uint32_t app_address = ADDR_APP_PROGRAM;
    uint16_t len;
    for (int b = 0; b < 3; b++)
    {
        header[b] = 0;
    }
    flash_unlock();
    flash_erase_range_by_page();

    while (1)
    {
        if (receive_byte(&header[0], 5000) != HAL_OK || header[0] != START_BYTE)
        {
            continue;
        }

        if (HAL_UART_Receive(&huart1, &header[1], 2, 1000) != HAL_OK)
        {
            send_byte(NACK);
            continue;
        }

        len = (header[1] << 8) | header[2];
        if (len > BLOCK_SIZE)
        {
            send_byte(NACK);
            continue;
        }

        if (len == 0)
        {
            send_byte(ACK);
            break;
        }

        if (HAL_UART_Receive(&huart1, data, len, 1000) != HAL_OK)
        {
            send_byte(NACK);
            continue;
        }

        if (HAL_UART_Receive(&huart1, crc_buf, 2, 500) != HAL_OK)
        {
            send_byte(NACK);
            continue;
        }

        crc_recv = (crc_buf[0] << 8) | crc_buf[1];
        crc_calc = simpleCRC(data, len);

        if (crc_recv != crc_calc)
        {
            send_byte(NACK);
            continue;
        }

        flash_write(app_address, data, len);
        app_address += len;

        send_byte(ACK);
    }

    flash_lock();
}
