/**
 * msg.h — gadget-to-gadget messages over the LAN. See D046, D047.
 *
 * v1.25.0 turned the flat inbox into CONVERSATIONS: a history of in/out
 * entries per peer, a contact book that learns addresses on its own, and an
 * unread count the clock face shows. Transport is unchanged: HTTP into the
 * async server, mDNS-SD discovery, worker task for anything that blocks.
 *
 * Contacts are learned three ways, so replying never requires a scan:
 *   - a scan finds peers advertising _gadget-msg._tcp
 *   - SENDING to a typed name/IP remembers it
 *   - RECEIVING remembers the sender's name against the connection's source
 *     IP — the address arrives with the message, no lookup needed
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MSG_TEXT_MAX   96
#define MSG_FROM_MAX   23
#define MSG_HISTORY_N  32
#define MSG_PEERS_N    10

struct MsgPeer  { char name[MSG_FROM_MAX + 1]; char ip[16]; };
struct MsgEntry { char peer[MSG_FROM_MAX + 1]; char text[MSG_TEXT_MAX + 1];
                  uint32_t at_ms; bool outgoing; };

void        msg_begin(void);
const char *msg_name(void);

/* ---- discovery + contacts (name -> ip, learned from every direction) ---- */
void msg_request_scan(void);
bool msg_busy(void);
int  msg_contact_count(void);
bool msg_contact(int i, MsgPeer *out);
/* Accepts a known contact name OR a literal IP; false when unresolvable. */
bool msg_resolve(const char *name_or_ip, char *ip_out, size_t cap);

/* ---- sending (worker task; poll msg_last_send_result) ---- */
void msg_request_send(const char *ip, const char *text);
int  msg_last_send_result(void);
/* Record an outgoing message in the history + remember the contact. */
void msg_note_sent(const char *peer, const char *ip, const char *text);

/* ---- history (flat, newest first; filter by peer for a thread) ---- */
uint32_t msg_rev(void);
int      msg_history_count(void);
bool     msg_history(int i, MsgEntry *out);
void     msg_store(const char *from, const char *from_ip, const char *text);

/* ---- unread, for the clock badge ---- */
int  msg_unread(void);
void msg_mark_read(void);
