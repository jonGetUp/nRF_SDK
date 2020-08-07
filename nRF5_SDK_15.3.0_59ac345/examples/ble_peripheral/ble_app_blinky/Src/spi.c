#include "spi.h"
#include "ble_config.h"

static const nrf_drv_spis_t spis = NRF_DRV_SPIS_INSTANCE(SPIS_INSTANCE);/**< SPIS instance. */

 uint8_t       m_tx_buf[SPI_BUFFER_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};   /**< TX buffer.*/
 uint8_t       m_rx_buf[sizeof(m_tx_buf)];  /**< RX buffer. 1 byte more than the expected message*///2+1];//
static const uint8_t m_length = sizeof(m_tx_buf); /**< Transfer length. */

//Characteristics
uint16_t batVolt = 0;
uint32_t serialNumber = 0;

static volatile bool spis_xfer_done; /**< Flag used to indicate that SPIS instance completed the transfer. */



/**
 * Function for configuring the spi peripheral as slave
 */
void spi_init(void)//static 
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

/**
 * @brief SPIS user event handler.
 *
 * @param event
 */
void spis_event_handler(nrf_drv_spis_event_t event)
{
    if (event.evt_type == NRF_DRV_SPIS_XFER_DONE)
    {
        //spis transfer done -> set flag
        spis_xfer_done = true;
        NRF_LOG_INFO(" Transfer completed. Received function code: %x",m_rx_buf[0]);
        //m_rx_buffer cleared when readed
        uint8_t functionCode = m_rx_buf[0];
        //first sended byte is the functionCode
        switch(functionCode)
        {
          case 0x00:
              //do nothing, master read the slave txBuffer
              break;
          case 0x7F:
              //never used, nRF only write PIC, no read
//            //tell the SPI master wich variable to return
//            spis_reset_tx_buffer();
//            m_tx_buf[0] = 0x7F; //Acknowledge, send function code and data
//            m_tx_buf[1] = 0x01; //data size
//            m_tx_buf[2] = functionCode; //desired function code
//            //txBuffer loaded and ready to be readed -> pulse Interrupt request to the master
//            
            break;
          case FC_BATVOLT:
            //ble profil value format is little endian
            batVolt = (((uint16_t)m_rx_buf[2])<<8)& 0xFF00;         //read 8 LSB
            batVolt = batVolt + (((uint16_t)m_rx_buf[3]) & 0x00FF); //read 8 MSB
            ble_lbs_batVolt_characteristic_update(m_conn_handle, &m_lbs, &batVolt);  //call the characteristic update function
            break;
          case FC_SERIAL_NUMBER:
            //write the ble characteristic in little endian
            serialNumber = (((uint32_t)m_rx_buf[2])<<24) +  //LSB
                           (((uint32_t)m_rx_buf[3])<<16) +
                           (((uint32_t)m_rx_buf[4])<<8)  +
                           ((uint32_t)m_rx_buf[5]);         //MSB
            ble_lbs_characteristic_1_update(m_conn_handle, &m_lbs, &serialNumber);
            break;          
          default:
            //Is MSB set?
            if((functionCode & 0x80) == 0x80)
            {
              NRF_LOG_INFO("SPI: ERROR");
            }else
            {
              //Function code doesn't exist
              NRF_LOG_INFO("SPI: ILLEGAL FUNCTION");
              //Return exception code
//              spis_reset_tx_buffer();
//              m_tx_buf[0] = functionCode + 0x80;
//              m_tx_buf[1] = ILLEGAL_FUNCTION;
//              
            }
            break;
        }
    }
}

/**
 * @Brief Function call in the while(1), handle the SPI communications
 */
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