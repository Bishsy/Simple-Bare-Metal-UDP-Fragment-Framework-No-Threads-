/************************************************************
 * baremetal_udp_frag.c
 *
 * Bare-metal UDP-style Fragmentation + Reliability Engine
 * Target: Arria 10 SoC (No OS, No Threads)
 *
 * Features:
 *  - Application-level fragmentation
 *  - ACK handling
 *  - Retransmission (timeout based)
 *  - Polling architecture
 *  - No malloc
 *  - Deterministic execution
 *
 * Replace low-level ethernet_* functions with real driver.
 ************************************************************/

#include <stdint.h>
#include <string.h>

/************************************************************
 * CONFIGURATION
 ************************************************************/
#define MAX_DATA_SIZE      2000
#define MAX_FRAGMENTS      32
#define FRAME_SIZE         600      /* Change dynamically if needed */
#define HEADER_OVERHEAD    40       /* Approx IP + UDP header */
#define TIMEOUT_TICKS      5000     /* Retransmission timeout */
#define ACK_TYPE           1
#define DATA_TYPE          0

/************************************************************
 * SYSTEM TICK (Implement using hardware timer)
 ************************************************************/
volatile uint32_t system_ticks = 0;

/* Call this from timer interrupt every 1 ms */
void sys_tick_handler(void)
{
    system_ticks++;
}

/************************************************************
 * FRAGMENT STRUCTURE
 ************************************************************/
typedef struct
{
    uint16_t frag_id;
    uint16_t size;
    uint8_t  acked;
} fragment_t;

/************************************************************
 * GLOBAL TX STATE
 ************************************************************/
static uint8_t tx_data[MAX_DATA_SIZE];
static fragment_t tx_frags[MAX_FRAGMENTS];

static uint16_t tx_total_frags = 0;
static uint8_t  tx_active = 0;
static uint32_t tx_start_tick = 0;

/************************************************************
 * RX STATE
 ************************************************************/
static uint8_t rx_buffer[MAX_DATA_SIZE];
static uint8_t rx_frag_received[MAX_FRAGMENTS];
static uint16_t rx_expected_frags = 0;
static uint16_t rx_received_count = 0;

/************************************************************
 * LOW LEVEL DRIVER INTERFACE (Replace These)
 ************************************************************/

/* Send fragment over Ethernet */
void ethernet_send_fragment(uint8_t type,
                            uint16_t frag_id,
                            uint8_t *data,
                            uint16_t size)
{
    /* TODO: Replace with real Ethernet TX */
    (void)type;
    (void)frag_id;
    (void)data;
    (void)size;
}

/* Check if frame received */
uint8_t ethernet_frame_available(void)
{
    /* TODO: Replace with real RX check */
    return 0;
}

/* Read received frame */
void ethernet_read_frame(uint8_t *type,
                         uint16_t *frag_id,
                         uint8_t *payload,
                         uint16_t *size)
{
    /* TODO: Replace with real RX read */
    (void)type;
    (void)frag_id;
    (void)payload;
    (void)size;
}

/************************************************************
 * START TRANSMISSION
 ************************************************************/
void tx_start(uint16_t data_size)
{
    if (data_size > MAX_DATA_SIZE)
        return;

    uint16_t max_payload = FRAME_SIZE - HEADER_OVERHEAD;
    uint16_t offset = 0;
    uint16_t frag_id = 0;

    tx_total_frags =
        (data_size + max_payload - 1) / max_payload;

    while (offset < data_size && frag_id < MAX_FRAGMENTS)
    {
        uint16_t chunk =
            (data_size - offset > max_payload) ?
            max_payload : (data_size - offset);

        tx_frags[frag_id].frag_id = frag_id;
        tx_frags[frag_id].size = chunk;
        tx_frags[frag_id].acked = 0;

        offset += chunk;
        frag_id++;
    }

    tx_active = 1;
    tx_start_tick = system_ticks;
}

/************************************************************
 * TX TASK (Call in main loop)
 ************************************************************/
void tx_task(void)
{
    if (!tx_active)
        return;

    uint16_t offset = 0;

    for (uint16_t i = 0; i < tx_total_frags; i++)
    {
        if (!tx_frags[i].acked)
        {
            ethernet_send_fragment(
                DATA_TYPE,
                tx_frags[i].frag_id,
                &tx_data[offset],
                tx_frags[i].size);
        }

        offset += tx_frags[i].size;
    }
}

/************************************************************
 * PROCESS ACK
 ************************************************************/
void process_ack(uint16_t frag_id)
{
    if (frag_id < tx_total_frags)
    {
        tx_frags[frag_id].acked = 1;
    }
}

/************************************************************
 * RETRANSMISSION TASK
 ************************************************************/
void retransmission_task(void)
{
    if (!tx_active)
        return;

    if ((system_ticks - tx_start_tick) > TIMEOUT_TICKS)
    {
        tx_start_tick = system_ticks;

        for (uint16_t i = 0; i < tx_total_frags; i++)
        {
            if (!tx_frags[i].acked)
            {
                ethernet_send_fragment(
                    DATA_TYPE,
                    tx_frags[i].frag_id,
                    NULL,
                    tx_frags[i].size);
            }
        }
    }

    /* Check if transmission complete */
    uint8_t complete = 1;
    for (uint16_t i = 0; i < tx_total_frags; i++)
    {
        if (!tx_frags[i].acked)
        {
            complete = 0;
            break;
        }
    }

    if (complete)
    {
        tx_active = 0;
    }
}

/************************************************************
 * RX PROCESSING
 ************************************************************/
void process_data_fragment(uint16_t frag_id,
                           uint8_t *payload,
                           uint16_t size)
{
    if (frag_id >= MAX_FRAGMENTS)
        return;

    if (!rx_frag_received[frag_id])
    {
        memcpy(&rx_buffer[frag_id * size],
               payload,
               size);

        rx_frag_received[frag_id] = 1;
        rx_received_count++;
    }

    /* Send ACK */
    ethernet_send_fragment(
        ACK_TYPE,
        frag_id,
        0,
        0);

    if (rx_received_count == rx_expected_frags)
    {
        /* Full packet received */
        rx_received_count = 0;
        memset(rx_frag_received, 0,
               sizeof(rx_frag_received));
    }
}

/************************************************************
 * RX POLL TASK
 ************************************************************/
void ethernet_poll_rx(void)
{
    if (!ethernet_frame_available())
        return;

    uint8_t type;
    uint16_t frag_id;
    uint16_t size;
    uint8_t payload[FRAME_SIZE];

    ethernet_read_frame(&type,
                        &frag_id,
                        payload,
                        &size);

    if (type == ACK_TYPE)
    {
        process_ack(frag_id);
    }
    else
    {
        process_data_fragment(frag_id,
                              payload,
                              size);
    }
}

/************************************************************
 * INITIALIZATION
 ************************************************************/
void hardware_init(void)
{
    /* Initialize timer + Ethernet here */
}

/************************************************************
 * MAIN
 ************************************************************/
int main(void)
{
    hardware_init();

    /* Fill TX buffer with test data */
    for (uint16_t i = 0; i < MAX_DATA_SIZE; i++)
    {
        tx_data[i] = (uint8_t)(i & 0xFF);
    }

    /* Start sending 2000 bytes */
    tx_start(2000);

    while (1)
    {
        ethernet_poll_rx();
        tx_task();
        retransmission_task();
    }

    return 0;
}
