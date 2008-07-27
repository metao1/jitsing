/*
 *  TurnServer - TURN server implementation.
 *  Copyright (C) 2008 Sebastien Vincent <vincent@lsiit.u-strasbg.fr>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *    
 *  In addition, as a special exception, the copyright holders give
 *  permission to link the code of portions of this program with the
 *  OpenSSL library under certain conditions as described in each
 *  individual source file, and distribute linked combinations
 *  including the two.
 *  You must obey the GNU General Public License in all respects
 *  for all of the code used other than OpenSSL.  If you modify
 *  file(s) with this exception, you may extend this exception to your
 *  version of the file(s), but you are not obligated to do so.  If you
 *  do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source
 *  files in the program, then also delete it here.
 */

/**
 * \file allocation.h
 * \brief Allocation between TURN client and external(s) client(s).
 * \author Sebastien Vincent
 */

#ifndef ALLOCATION_H
#define ALLOCATION_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "list.h"

/**
 * \struct allocation_token
 * \brief Allocation token.
 */
struct allocation_token
{
  uint8_t id[8]; /**< Token ID */
  int sock; /**< The opened socket */
  timer_t expire_timer; /**< Expire timer */
  struct list_head list; /**< For list management */
  struct list_head list2; /**< For list management (expired list) */
};

/**
 * \struct allocation_tuple
 * \brief Allocation tuple
 */
struct allocation_tuple
{
  int transport_protocol; /**< Transport protocol */
  struct sockaddr_storage client_addr; /**< Client address */
  struct sockaddr_storage server_addr; /**< Server address */
};

/**
 * \struct allocation_permission
 * \brief Network address permission.
 */
struct allocation_permission
{
  int family; /**< Address family */
  uint8_t peer_addr[16]; /**< Peer address */
  int permission; /**< Permission state of the peer (1 = relay packet, 0 = drop packet from it) */
  timer_t expire_timer; /**< Expire timer */
  struct list_head list; /**< For list management */
  struct list_head list2; /**< For list management (expired list) */
};

/**
 * \struct allocation_channel
 * \brief Allocation channel.
 */
struct allocation_channel
{
  int family; /**< Address family */
  uint8_t peer_addr[16]; /**< Peer address */
  uint16_t peer_port; /**< Peer port */
  uint16_t channel_number; /**< Channel bound to this peer */
  timer_t expire_timer; /**< Expire timer */
  struct list_head list; /**< For list management */
  struct list_head list2; /**< For list management (expired list) */
};

/**
 * \struct allocation_desc
 * \brief Allocation descriptor.
 */
struct allocation_desc
{
  char* username; /**< Username obtained by the TURN client */
  int relayed_transport_protocol; /**< Relayed transport protocol used */
  struct sockaddr_storage relayed_addr; /**< Relayed transport address */
  struct allocation_tuple tuple; /**< 5-tuple */
  struct list_head peers_channels; /**< List of channel to peer bindings */
  struct list_head peers_permissions; /**< List of peers permissions */
  int relayed_sock; /**< Socket for the allocated transport address */
  int tuple_sock; /**< Socket for the connection between the TURN server and the TURN client */
  uint8_t transaction_id[12]; /**< Transaction ID of the Allocate Request */
  timer_t expire_timer; /**< Expire timer */
  int preserving_flag; /**< Allocation preserving flag */
  int expired; /**< Expired flag, indicates that the descriptor will be removed  lately */
  struct list_head list; /**< For list management */
  struct list_head list2; /**< For list management (expired list) */
};

/**
 * \brief Create a new allocation descriptor.
 * \param id transaction ID of the Allocate request
 * \param transport_protocol transport protocol (i.e. TCP, UDP, ...)
 * \param username one-time login
 * \param relayed_addr relayed address and port
 * \param server_addr server network address and port
 * \param client_addr client network address and port
 * \param addr_size sizeof address
 * \param preserving_flag preserving flag
 * \param lifetime expire of the allocation
 * \return pointer on struct allocation_desc, or NULL if problem
 */
struct allocation_desc* allocation_desc_new(const uint8_t* id, uint8_t transport_protocol, const char* username, const struct sockaddr* relayed_addr, const struct sockaddr* server_addr, const struct sockaddr* client_addr, socklen_t addr_size, int preserving_flag, uint32_t lifetime);

/**
 * \brief Free a allocation descriptor.
 * \param desc pointer on pointer allocated by allocation_desc_new
 */
void allocation_desc_free(struct allocation_desc** desc);

/**
 * \brief Set timer.
 * \param desc allocation descriptor
 * \param lifetime lifetime timer
 */
void allocation_desc_set_timer(struct allocation_desc* desc, uint32_t lifetime);

/**
 * \brief Set the last minutes timer to prevent allocate the same relayed address.
 * \param desc allocation descriptor
 * \param lifetime lifetime timer
 */
void allocation_desc_set_last_timer(struct allocation_desc* desc, uint32_t lifetime);

/**
 * \brief Find if a peer (network address only) has a permissions installed.
 * \param desc allocation descriptor
 * \param family address family (IPv4 or IPv6)
 * \param peer_addr network address
 * \return pointer on allocation_permission or NULL if not found
 */
struct allocation_permission* allocation_desc_find_permission(struct allocation_desc* desc, int family, const char* peer_addr);

/**
 * \brief Find if a peer (network address only) has a permissions installed.
 * \param desc allocation descriptor
 * \param addr network address
 * \return pointer on allocation_permission or NULL if not found
 */
struct allocation_permission* allocation_desc_find_permission_sockaddr(struct allocation_desc* desc, const struct sockaddr* addr);

/**
 * \brief Add a permission for a peer.
 * \param desc allocation descriptor
 * \param lifetime lifetime of the permission
 * \param family address family (IPv4 or IPv6)
 * \param peer_addr network address
 * \return 0 if success, -1 otherwise
 */
int allocation_desc_add_permission(struct allocation_desc* desc, uint32_t lifetime, int family, const char* peer_addr);

/**
 * \brief Find if a peer (transport address) has a channel bound.
 * \param desc allocation descriptor
 * \param family address family (IPv4 or IPv6)
 * \param peer_addr network address
 * \param peer_port peer port
 * \return the channel if the peer has already a channel bound, 0 otherwise
 */
uint32_t allocation_desc_find_channel(struct allocation_desc* desc, int family, const char* peer_addr, uint16_t peer_port);

/**
 * \brief Find if a channel number has a peer (transport address).
 * \param desc allocation descriptor
 * \param channel channel number
 * \return pointer on allocation_channel if found, NULL otherwise
 */
struct allocation_channel* allocation_desc_find_channel_number(struct allocation_desc* desc, uint16_t channel);

/**
 * \brief Add a channel to a peer (transport address).
 * \param desc allocation descriptor
 * \param channel channel number
 * \param lifetime lifetime of the channel
 * \param family address family (IPv4 or IPv6)
 * \param peer_addr network address
 * \param peer_port peer port
 * \return 0 if success, -1 otherwise
 */
int allocation_desc_add_channel(struct allocation_desc* desc, uint16_t channel, uint32_t lifetime, int family, const char* peer_addr, uint16_t peer_port);

/**
 * \brief Reset the timer of the channel.
 * \param channel allocation channel
 * \param lifetime lifetime
 */
void allocation_channel_set_timer(struct allocation_channel* channel, uint32_t lifetime);

/**
 * \brief Reset the timer of the permission.
 * \param permission allocation permission
 * \param lifetime lifetime
 */
void allocation_permission_set_timer(struct allocation_permission* permission, uint32_t lifetime);

/**
 * \brief Free a list of allocation.
 * \param list list of allocation
 */
void allocation_list_free(struct list_head* list);

/**
 * \brief Add an allocation to a list.
 * \param list list of allocation
 * \param desc allocation descriptor to add
 */
void allocation_list_add(struct list_head* list, struct allocation_desc* desc);

/**
 * \brief Remove and free an allocation from a list.
 * \param list list of allocation
 * \param desc allocation to remove
 */
void allocation_list_remove(struct list_head* list, struct allocation_desc* desc);

/**
 * \brief Find in the list a element that match ID.
 * \param list list of allocations
 * \param id transaction ID
 * \return pointer on allocation_desc or NULL if not found
 */
struct allocation_desc* allocation_list_find_id(struct list_head* list, const uint8_t* id);

/**
 * \brief Find in the list a element that match username.
 * \param list list of allocations
 * \param username username
 * \return pointer on allocation_desc or NULL if not found
 */
struct allocation_desc* allocation_list_find_username(struct list_head* list, const char* username);

/**
 * \brief Find in the list a element that match the 5-tuple.
 * \param list list of allocations
 * \param transport_protocol transport protocol
 * \param server_addr server address and port
 * \param client_addr client address and port
 * \param addr_size sizeof addr
 * \return pointer on allocation_desc or NULL if not found
 */
struct allocation_desc* allocation_list_find_tuple(struct list_head* list, int transport_protocol, const struct sockaddr* server_addr, const struct sockaddr* client_addr, socklen_t addr_size);

/**
 * \brief Find in the list a element that match the relayed address.
 * \param list list of allocations
 * \param relayed_addr relayed address and port
 * \param addr_size sizeof addr
 * \return pointer on allocation_desc or NULL if not found
 */
struct allocation_desc* allocation_list_find_relayed(struct list_head* list, const struct sockaddr* relayed_addr, socklen_t addr_size);

/**
 * \brief Create a new token.
 * \param id token ID (MUST be 64 bit length)
 * \param sock opened socket
 * \param lifetime lifetime
 * \return pointer on allocation_token or NULL if problem
 */
struct allocation_token* allocation_token_new(uint8_t* id, int sock, uint32_t lifetime);

/**
 * \brief Free a token.
 * \param token pointer on pointer allocated by allocation_token_new
 */
void allocation_token_free(struct allocation_token** token);

/**
 * \brief Set timer.
 * \param token allocation descriptor
 * \param lifetime lifetime timer
 */
void allocation_token_set_timer(struct allocation_token* token, uint32_t lifetime);

/**
 * \brief Add a token to a list.
 * \param list list of tokens
 * \param token token to add
 */
void allocation_token_list_add(struct list_head* list, struct allocation_token* token);

/**
 * \brief Find a specified token.
 * \param list list of tokens
 * \param id token ID (64 bit)
 * \return pointer on allocation_token or NULL if not found
 */
struct allocation_token* allocation_token_list_find(struct list_head* list, uint8_t* id);

/**
 * \brief Free a token list.
 * \param list list of tokens
 */
void allocation_token_list_free(struct list_head* list);

/**
 * \brief Remove and free a token from a list.
 * \param list list of allocation
 * \param desc allocation to remove
 */
void allocation_token_list_remove(struct list_head* list, struct allocation_token* desc);

#endif /* ALLOCATION_H */
