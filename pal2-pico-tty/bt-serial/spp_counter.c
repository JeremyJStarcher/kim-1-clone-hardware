/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */
#include <stddef.h> /* defines size_t */
#include "pal-io.h"

static uint16_t mtu;

#define SEND_TIMEOUT_MS 750 // 500
#define SEND_THRESHOLD_CHARS (mtu - 1)

static uint32_t send_elapsed_ms = 0;
static uint32_t l2cap_elapsed_ms = 0;

#define BTSTACK_FILE__ "spp_counter.c"

#define ECHO_BUF_SIZE 512
static char echo_buf[ECHO_BUF_SIZE];
static size_t echo_len = 0;

#define KEEP_ALIVE_CHAR (0x02)

// *****************************************************************************
/* EXAMPLE_START(spp_counter): SPP Server - Heartbeat Counter over RFCOMM
 *
 * @text The Serial port profile (SPP) is widely used as it provides a serial
 * port over Bluetooth. The SPP counter example demonstrates how to setup an SPP
 * service, and provide a periodic timer over RFCOMM.
 *
 * @text Note: To test, please run the spp_counter example, and then pair from
 * a remote device, and open the Virtual Serial Port.
 */
// *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#include "config.h"
#include "ring.h"
#include "debug.h"

// #define debug_printf(...) printf(__VA_ARGS__)
#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 100

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t rfcomm_channel_id;
static uint8_t spp_service_buffer[150];
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t last_con_handle = HCI_CON_HANDLE_INVALID;

/* @section SPP Service Setup
 *s
 * @text To provide an SPP service, the L2CAP, RFCOMM, and SDP protocol layers
 * are required. After setting up an RFCOMM service with channel nubmer
 * RFCOMM_SERVER_CHANNEL, an SDP record is created and registered with the SDP server.
 * Example code for SPP service setup is
 * provided in Listing SPPSetup. The SDP record created by function
 * spp_create_sdp_record consists of a basic SPP definition that uses the provided
 * RFCOMM channel ID and service name. For more details, please have a look at it
 * in \path{src/sdp_util.c}.
 * The SDP record is created on the fly in RAM and is deterministic.
 * To preserve valuable RAM, the result could be stored as constant data inside the ROM.
 */

static void spp_service_setup(void)
{

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    rfcomm_init();
    rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff); // reserved channel, mtu limited by l2cap

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    sdp_register_service(spp_service_buffer);
    debug_printf("SDP service record size: %u\n", de_get_len(spp_service_buffer));
}

/* @section Periodic Timer Setup
 *
 * @text The heartbeat handler increases the real counter every second,
 * and sends a text string with the counter value, as shown in Listing PeriodicCounter.
 */

static btstack_timer_source_t heartbeat;

static void heartbeat_handler(struct btstack_timer_source *ts)
{
    // 1) Count how many bytes we’ve queued
    size_t chars_to_send = ring_count(&tx_ring);

    // 2) Did the user just press a key?
    bool user_pressed_key = user_pressed_key_get();

    // 3) Only if we’re actually connected…
    if (rfcomm_channel_id)
    {
        //  a) immediate send on user-key
        if (user_pressed_key)
        {
            rfcomm_request_can_send_now_event(rfcomm_channel_id);
            send_elapsed_ms = 0;
            user_pressed_key_set(false);
        }
        //  b) or send if threshold exceeded
        else if (chars_to_send >= SEND_THRESHOLD_CHARS || send_elapsed_ms >= SEND_TIMEOUT_MS)
        {
            // Keep alive, avoid timeouts.
            if (chars_to_send == 0)
            {
                ring_push(&tx_ring, KEEP_ALIVE_CHAR);
            }
            rfcomm_request_can_send_now_event(rfcomm_channel_id);
            send_elapsed_ms = 0;
        }
    }

    // L2CAP ping to keep link active
    // L2CAP ping only every 5 s of inactivity
    if (last_con_handle != HCI_CON_HANDLE_INVALID && l2cap_elapsed_ms >= 5000u)
    {
        l2cap_send_echo_request(last_con_handle, NULL, 0);
        l2cap_elapsed_ms = 0;
    }

    // 4) advance our elapsed clock by the timer interval…
    send_elapsed_ms += HEARTBEAT_PERIOD_MS;
    l2cap_elapsed_ms += HEARTBEAT_PERIOD_MS;

    // 5) re-arm the timer for the next tick
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}

static void one_shot_timer_setup(void)
{
    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);
}

/* @section Bluetooth Logic
 * @text The Bluetooth logic is implemented within the
 * packet handler, see Listing SppServerPacketHandler. In this example,
 * the following events are passed sequentially:
 * - BTSTACK_EVENT_STATE,
 * - HCI_EVENT_PIN_CODE_REQUEST (Standard pairing) or
 * - HCI_EVENT_USER_CONFIRMATION_REQUEST (Secure Simple Pairing),
 * - RFCOMM_EVENT_INCOMING_CONNECTION,
 * - RFCOMM_EVENT_CHANNEL_OPENED,
 * - RFCOMM_EVETN_CAN_SEND_NOW, and
 * - RFCOMM_EVENT_CHANNEL_CLOSED
 */

/* @text Upon receiving HCI_EVENT_PIN_CODE_REQUEST event, we need to handle
 * authentication. Here, we use a fixed PIN code "0000".
 *
 * When HCI_EVENT_USER_CONFIRMATION_REQUEST is received, the user will be
 * asked to accept the pairing request. If the IO capability is set to
 * SSP_IO_CAPABILITY_DISPLAY_YES_NO, the request will be automatically accepted.
 *
 * The RFCOMM_EVENT_INCOMING_CONNECTION event indicates an incoming connection.
 * Here, the connection is accepted. More logic is need, if you want to handle connections
 * from multiple clients. The incoming RFCOMM connection event contains the RFCOMM
 * channel number used during the SPP setup phase and the newly assigned RFCOMM
 * channel ID that is used by all BTstack commands and events.
 *
 * If RFCOMM_EVENT_CHANNEL_OPENED event returns status greater then 0,
 * then the channel establishment has failed (rare case, e.g., client crashes).
 * On successful connection, the RFCOMM channel ID and MTU for this
 * channel are made available to the heartbeat counter. After opening the RFCOMM channel,
 * the communication between client and the application
 * takes place. In this example, the timer handler increases the real counter every
 * second.
 *
 * RFCOMM_EVENT_CAN_SEND_NOW indicates that it's possible to send an RFCOMM packet
 * on the rfcomm_cid that is include
 */

static void add_to_ring(volatile ring_t *rx_ring, char *line, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        char ch = line[i];
        ring_push(rx_ring, ch);
    }
}

static void send_from_ring(uint16_t cid, volatile ring_t *tx)
{
    if (!rfcomm_can_send_packet_now(cid))
    {
        return;
    }

    size_t avail = ring_count(tx);
    if (!avail)
        return;

    size_t len = MIN(avail, ECHO_BUF_SIZE - 1);
    for (size_t i = 0; i < len; i++)
    {
        ring_pop(tx, &echo_buf[i]);
    }

    rfcomm_send(cid, (uint8_t *)echo_buf, len);

    // schedule next send if still data left
    if (ring_count(tx))
    {
        rfcomm_request_can_send_now_event(cid);
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);

    bd_addr_t event_addr;
    uint8_t rfcomm_channel_nr;
    int i;

    switch (packet_type)
    {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet))
        {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING)
            {
                uint16_t con_handle = rfcomm_event_channel_opened_get_con_handle(packet);
                hci_send_cmd(&hci_write_link_policy_settings, con_handle, 0x0000); // disable sniff, hold, park

                gap_discoverable_control(1);
                gap_connectable_control(1);

                one_shot_timer_setup();
            }
            break;
        }

        switch (hci_event_packet_get_type(packet))
        {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf(" Disconnect reason(0x08 and 0x13 are common) %02x\n", packet[5]);
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            // inform about pin code request
            debug_printf("Pin code request - using '0000'\n");
            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            gap_pin_code_response(event_addr, "0000");
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            // ssp: inform about user confirmation request
            debug_printf("SSP User Confirmation Request with numeric value '%06" PRIu32 "'\n", little_endian_read_32(packet, 8));
            debug_printf("SSP User Confirmation Auto accept\n");
            break;

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
            rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
            rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);

            last_con_handle = rfcomm_event_channel_opened_get_con_handle(packet);
            // DOCUMENT
            hci_send_cmd(&hci_write_link_policy_settings, last_con_handle, 0x0001);
            hci_send_cmd(&hci_write_link_supervision_timeout, last_con_handle, 0xEA60);

            rfcomm_request_can_send_now_event(rfcomm_channel_id);
            debug_printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
            rfcomm_accept_connection(rfcomm_channel_id);
            break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
            if (rfcomm_event_channel_opened_get_status(packet))
            {
                debug_printf("RFCOMM channel open failed, status %u\n", rfcomm_event_channel_opened_get_status(packet));
            }
            else
            {
                rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);

                // Starts sending right away without waiting for a keystroke from the receiver.
                ring_push(&tx_ring, KEEP_ALIVE_CHAR);
                ring_push(&tx_ring, '\r');
                ring_push(&tx_ring, '\r');
                ring_push(&tx_ring, '\r');

                rfcomm_request_can_send_now_event(rfcomm_channel_id); // kick-start TX
                debug_printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
                system_config.rfcomm_channel_id = rfcomm_channel_id;
                system_config.bt_connected_state = CONNECTION_STATE_NEW_CONNECTION;
            }
            break;
        case RFCOMM_EVENT_CAN_SEND_NOW:
            send_from_ring(rfcomm_channel_id, &tx_ring);
            break;

        case RFCOMM_EVENT_CHANNEL_CLOSED:
            debug_printf("RFCOMM channel closed\n");
            rfcomm_channel_id = 0;
            last_con_handle = HCI_CON_HANDLE_INVALID;
            system_config.bt_connected_state = CONNECTION_STATE_NEW_DISCONNECT;

            // re-enable both discoverable (inquiry) and connectable (page) scans
            gap_discoverable_control(1);
            gap_connectable_control(1);
            break;

        default:
            break;
        }
        break;

    case RFCOMM_DATA_PACKET:
        add_to_ring(&rx_ring, packet, size);
        rfcomm_request_can_send_now_event(rfcomm_channel_id);
        break;

    default:
        break;
    }
}

int btstack_main(int argc, const char *argv[]);
int btstack_main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    spp_service_setup();

    // This will prompt for the device # and work with it
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    // gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    gap_set_local_name("KIM1-1-BT 00:00:00:00:00:00");

    // 0x020104 = Major class: 0x02 (PHONE), Minor: 0x01 (CELLULAR), Service: 0x04 (OBJECT_TRANSFER)
    gap_set_class_of_device(0x020104);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();

    return 0;
}
