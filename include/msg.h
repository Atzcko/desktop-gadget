/**
 * msg.h — gadget-to-gadget messages over the LAN. See D046.
 *
 * Transport is HTTP into the async server every device already runs, and
 * discovery is mDNS-SD: each device advertises _gadget-msg._tcp with its
 * name (Settings ▸ BLE ▸ name — the device's one identity) in a TXT record.
 * Chosen over ESP-NOW (MAC-only identity, AP-channel-locked, no cross-AP)
 * and MQTT (an external broker, which the scope boundary forbids).
 *
 * Scans and sends BLOCK for seconds, so they run on a worker task; the UI
 * requests and polls. Only msg_store() is called from the web server's task,
 * and it takes a lock shared with the readers.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MSG_TEXT_MAX   96
#define MSG_FROM_MAX   23
#define MSG_INBOX_N    16
#define MSG_PEERS_N     8

struct MsgPeer  { char name[MSG_FROM_MAX + 1]; char ip[16]; };
struct MsgEntry { char from[MSG_FROM_MAX + 1]; char text[MSG_TEXT_MAX + 1];
                  uint32_t at_ms; };

void        msg_begin(void);
const char *msg_name(void);                  /* this device's identity      */

void msg_request_scan(void);                 /* worker: mDNS browse ~3 s    */
bool msg_busy(void);                         /* a scan or send in flight    */
int  msg_peer_count(void);
bool msg_peer(int i, MsgPeer *out);

void msg_request_send(const char *ip, const char *text);
int  msg_last_send_result(void);             /* -1 pending, else HTTP code  */

/* Inbox: newest first. rev increments on every store, so the UI can poll. */
uint32_t msg_inbox_rev(void);
int      msg_inbox_count(void);
bool     msg_inbox(int i, MsgEntry *out);
void     msg_store(const char *from, const char *text);   /* any task */
