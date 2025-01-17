#include "spi.h"
#include <assert.h>

static const nrf_drv_spis_t spis = NRF_DRV_SPIS_INSTANCE(SPIS_INSTANCE);/**< SPIS instance. */
static volatile bool spis_xfer_done; /**< Flag used to indicate that SPIS instance completed the transfer. */

uint8_t m_tx_buf[SPI_BUFFER_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};   /**< TX buffer.*/
uint8_t m_rx_buf[sizeof(m_tx_buf)];  /**< RX buffer. 1 byte more than the expected message*///2+1];//
static const uint8_t m_length = sizeof(m_tx_buf); /**< Transfer length. */

//Characteristics
struct BMS_STATE bmsState; 

void spi_init(void)
{
    // Enable the constant latency sub power mode to minimize the time it takes
    // for the SPIS peripheral to become active after the CSN line is asserted
    // (when the CPU is in sleep mode).
    NRF_POWER->TASKS_CONSTLAT = 1;
    
    //SPI configuration
    nrf_drv_spis_config_t spis_config = NRF_DRV_SPIS_DEFAULT_CONFIG;
    spis_config.csn_pin               = APP_SPIS_CS_PIN;
    spis_config.miso_pin              = APP_SPIS_MISO_PIN;
    spis_config.mosi_pin              = APP_SPIS_MOSI_PIN;
    spis_config.sck_pin               = APP_SPIS_SCK_PIN;
    //SPI init with event handler callback function
    APP_ERROR_CHECK(nrf_drv_spis_init(&spis, &spis_config, spis_event_handler));
}

void spis_event_handler(nrf_drv_spis_event_t event)
{
    ret_code_t err_code;
    if (event.evt_type == NRF_DRV_SPIS_XFER_DONE)
    {
        //spis transfer done -> set flag
        spis_xfer_done = true;
        //NRF_LOG_INFO(" Transfer completed. Received function code: %x",m_rx_buf[0]);
        //m_rx_buffer cleared when readed
        uint8_t functionCode = m_rx_buf[0];
        //first sended byte is the functionCode
        switch(functionCode)
        {
          case 0x00:
              //do nothing, master read the slave txBuffer
              NRF_LOG_INFO("<SPI: tx_buf read by Master");
              break;
          case FC_BATVOLT:
              //ble profil value format is little endian
              bmsState.batVolt = (((uint16_t)m_rx_buf[2])<<8)& 0xFF00;                  //read 8 LSB
              bmsState.batVolt += (((uint16_t)m_rx_buf[3]) & 0x00FF); //read 8 MSB
              update_batVolt(&bmsState.batVolt);
              NRF_LOG_INFO(" SPI: Receive %x batVolt: %u",functionCode, bmsState.batVolt);
            break;
          case FC_BATTERY_CURRENT:
              //ble profil value format is little endian
              bmsState.battery_current = (((uint32_t)m_rx_buf[2])<<24) +  //LSB
                                         (((uint32_t)m_rx_buf[3])<<16) +
                                         (((uint32_t)m_rx_buf[4])<<8)  +
                                          ((uint32_t)m_rx_buf[5]);         //MSB
              update_battery_current(&bmsState.battery_current);
              NRF_LOG_INFO(" SPI: Receive %x battery_current: %d",functionCode, bmsState.battery_current);
            break;
          case FC_CHARGER_CURRENT:
              //ble profil value format is little endian
              bmsState.charger_current = (((uint16_t)m_rx_buf[2])<<8)& 0xFF00;  //read 8 LSB
              bmsState.charger_current += (((uint16_t)m_rx_buf[3]) & 0x00FF);   //read 8 MSB
              update_charger_current(&bmsState.charger_current);
              NRF_LOG_INFO(" SPI: Receive %x charger_current: %d",functionCode, bmsState.charger_current);
            break;
          case FC_CURFAULT:
              //ble profil value format is little endian
              bmsState.curFault = m_rx_buf[2];
              update_curFault(&bmsState.curFault);
              NRF_LOG_INFO(" SPI: Receive %x curFault: %u",functionCode, bmsState.curFault);
            break;
          case FC_BALANCEINWORK:
              //ble profil value format is little endian
              bmsState.balanceInWork = m_rx_buf[2];
              update_balanceInWork(&bmsState.balanceInWork);
              NRF_LOG_INFO(" SPI: Receive %x balanceInWork: %u",functionCode, bmsState.balanceInWork);
            break;
          case FC_SMMAIN:
              //ble profil value format is little endian
              bmsState.smMain = m_rx_buf[2];
              update_smMain(&bmsState.smMain);
              NRF_LOG_INFO(" SPI: Receive %x smMain: %u",functionCode, bmsState.smMain);
            break;
          case FC_SERIAL_NUMBER:
              //write the ble characteristic in little endian
              bmsState.pack_serialNumber = (((uint32_t)m_rx_buf[2])<<24) +  //LSB
                                           (((uint32_t)m_rx_buf[3])<<16) +
                                           (((uint32_t)m_rx_buf[4])<<8)  +
                                           ((uint32_t)m_rx_buf[5]);         //MSB
              update_pack_serialNumber(&bmsState.pack_serialNumber);
              NRF_LOG_INFO(" SPI: Receive %x Serial number: %u",functionCode, bmsState.pack_serialNumber);
            break;
          case FC_UNBLOCK_SM:
              bmsState.unblock_sm = m_rx_buf[2];
              update_unblock_sm(&bmsState.unblock_sm);
              NRF_LOG_INFO(" SPI: Receive %x Unblock sm: %u",functionCode, bmsState.unblock_sm);
            break;
          case FC_CHARGER_CURRENT_HIGH:
              //ble profil value format is little endian
              bmsState.charger_current_high = (((uint16_t)m_rx_buf[2])<<8)& 0xFF00;  //read 8 LSB
              bmsState.charger_current_high += (((uint16_t)m_rx_buf[3]) & 0x00FF);   //read 8 MSB
              update_charger_current_high(&bmsState.charger_current_high);
              NRF_LOG_INFO(" SPI: Receive %x charger_current_high: %d",functionCode, bmsState.charger_current_high);
            break;
          case FC_CHARGER_CURRENT_LOW:
              //ble profil value format is little endian
              bmsState.charger_current_low = (((uint16_t)m_rx_buf[2])<<8)& 0xFF00;  //read 8 LSB
              bmsState.charger_current_low += (((uint16_t)m_rx_buf[3]) & 0x00FF);   //read 8 MSB
              update_charger_current_low(&bmsState.charger_current_low);
              NRF_LOG_INFO(" SPI: Receive %x charger_current_low: %d",functionCode, bmsState.charger_current_low);
            break;
          //>>>>>>>>>> Add other
          default:
            //Is MSB set?
            if((functionCode & 0x80) == 0x80)
            {
              NRF_LOG_INFO("SPI: ERROR");
            }else
            {
              //Function code doesn't exist
              NRF_LOG_INFO("SPI: ILLEGAL FUNCTION");
            }
            break;
        }
    }
}

void spis_handle(void)
{
    memset(m_rx_buf, 0, m_length);
    spis_xfer_done = false;

    //Set the SPI TX/RX buffer
    APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));

    //wait until transfer is done (wait that the master give the clock)
    while (!spis_xfer_done)
    {
        __WFE();  //Wait For Event
    }

    NRF_LOG_FLUSH();
}

/*******************************************************************************
 * @brief Reset the tx_Buffer to 0x00, to override all tx bytes
 ******************************************************************************/
void spis_reset_tx_buffer(void)
{
    for(int i=0; i < SPI_BUFFER_SIZE; i++)
    {
        m_tx_buf[i] = 0;
    }
}
