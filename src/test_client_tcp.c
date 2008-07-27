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
 * \file test_client_tcp.c
 * \brief TCP TURN client example.
 * \author Sebastien Vincent
 * \date 2008
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#include "protocol.h"
#include "turn.h"
#include "util_crypto.h"
#include "util_sys.h"

/**
 * \brief Entry point of the program.
 * \param argc number of argument
 * \param argv array of argument
 * \return EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc, char** argv)
{
  struct iovec iov[50];
  size_t index = 0;
  ssize_t nb = -1;
  index = 0;
  uint8_t id[12];
  struct turn_msg_hdr* hdr = NULL;
  struct turn_attr_hdr* attr = NULL;
  struct turn_attr_hdr* attr2 = NULL;
  struct sockaddr_storage server_addr;
  socklen_t server_addr_size = 0; 
  struct sockaddr_storage peer_addr;
  struct addrinfo hints;
  struct addrinfo* res = NULL;
  int sock = -1;
  unsigned char md_buf[16]; /* MD5 */
  struct turn_message message;
  uint16_t tabu[16];
  size_t tabu_size = sizeof(tabu) / sizeof(uint16_t);
  uint8_t nonce[513];
  char buf[1500];
  ssize_t nb3 = -1;
  size_t n_len = 0;
  uint8_t token[8];
  char peer_port[8];

  if(argc != 5)
  {
    printf("Usage %s client_address server_address peer_address peer_port\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* get address for server_addr */
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  hints.ai_flags = 0;

  if(getaddrinfo(argv[2], "3478", &hints, &res) != 0)
  {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }
  memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
  server_addr_size = res->ai_addrlen;
  freeaddrinfo(res);

  /* get address for peer_addr */
  snprintf(peer_port, sizeof(peer_port), "%s", argv[4]);
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol =  IPPROTO_TCP;
  hints.ai_flags = 0;

  if(getaddrinfo(argv[3], peer_port, &hints, &res) != 0)
  {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }
  memcpy(&peer_addr, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  nb = turn_generate_transaction_id(id);

  sock = socket_create(IPPROTO_TCP, argv[1] ? argv[1] : "127.0.0.1", 0);

  if(sock == -1)
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  if(connect(sock, (struct sockaddr*)&server_addr, server_addr_size) == -1)
  {
    perror("connect");
    close(sock);
    exit(EXIT_FAILURE);
  }

  /* Allocate request */
  hdr = turn_msg_allocate_request_create(0, id, &iov[index]);
  index++;

  printf("Send allocate request\n");
  nb = turn_tcp_send(sock, iov, index);

  if(nb == -1)
  {
    perror("sendto\n");
    close(sock);
    exit(EXIT_FAILURE);
  }

  memset(&message, 0x00, sizeof(message));

  nb3 = recv(sock, buf, 1024, 0);
  nb = turn_parse_message(buf, nb3, &message, tabu, &tabu_size);

  if(nb == -1)
  {
    printf("Error parsing or connection closed by the server\n");
    close(sock);
    exit(EXIT_FAILURE);
  }

  memcpy(nonce, message.nonce->turn_attr_nonce, ntohs(message.nonce->turn_attr_len));
  nonce[ntohs(message.nonce->turn_attr_len)] = 0x00;
  n_len = ntohs(message.nonce->turn_attr_len);

  /* NONCE */
  attr = turn_attr_nonce_create(nonce, n_len, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REALM */
  attr = turn_attr_realm_create("domain.org", strlen("domain.org"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* USERNAME */
  attr = turn_attr_username_create("ping6", strlen("ping6"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* LIFETIME */
  attr = turn_attr_lifetime_create(0x00000005, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* SOFTWARE */
  attr = turn_attr_software_create("Client TURN 0.1 test", strlen("Client TURN 0.1 test"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REQUESTED-TRANSPORT */
  attr = turn_attr_requested_transport_create(IPPROTO_UDP, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REQUESTED-PROPS */
  attr = turn_attr_requested_props_create(0xC0000000, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REQUESTED-ADDRESS-TYPE */
  attr = turn_attr_requested_address_type_create(peer_addr.ss_family == AF_INET ? STUN_ATTR_FAMILY_IPV4 : STUN_ATTR_FAMILY_IPV6, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* MESSAGE-INTEGRITY */
  attr = turn_attr_message_integrity_create(NULL, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;

  nb = index; /* number of element before MESSAGE-INTEGRITY */
  index++;

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* after convert STUN/TURN message length to big endian we can calculate HMAC-SHA1 */
  /* index -1 because we do not take count MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"ping6:domain.org:password", strlen("ping6:domain.org:password"));
  turn_calculate_integrity_hmac_iov(iov, index - 1, md_buf, sizeof(md_buf), ((struct turn_attr_message_integrity*)attr)->turn_attr_hmac);
  attr2 = attr;

#if 1
  /* FINGERPRINT */
  attr = turn_attr_fingerprint_create(0, &iov[index]);
  hdr->turn_msg_len = ntohs(hdr->turn_msg_len) + iov[index].iov_len;
  index++; 

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* calculate fingerprint */
  /* index -1, we do not take count FINGER attribute */
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc = htonl(turn_calculate_fingerprint(iov, index - 1));
#endif

  printf("Send allocate request\n");
  nb = turn_tcp_send(sock, iov, index);

  nb = recv(sock, buf, sizeof(buf), 0);

  nb3 = 0;
  nb = turn_parse_message(buf, (size_t)nb, &message, NULL, (size_t*)&nb3);

  if(STUN_IS_ERROR_RESP(ntohs(message.msg->turn_msg_type)))
  {
    perror("Allocate error received, stop here!");
    iovec_free_data(iov, index);
    close(sock);
    exit(EXIT_FAILURE);
  }

  if(message.fingerprint)
  {
    /* verify if CRC is valid */
    uint32_t crc = 0;
    size_t total_len = ntohs(message.msg->turn_msg_len) + 20;

    crc = crc32_generate((const unsigned char*)buf, total_len - sizeof(struct turn_attr_fingerprint), 0);

    nb = htonl(crc) == message.fingerprint->turn_attr_crc;
  }

  if(message.message_integrity)
  {
    size_t total_len = ntohs(message.msg->turn_msg_len) + 20;
    uint8_t hash[20];

    if(message.fingerprint)
    {
      /* if the message contains a FINGERPRINT attribute, we adjust the size */
      size_t len_save = message.msg->turn_msg_len;

      message.msg->turn_msg_len = ntohs(message.msg->turn_msg_len) - sizeof(struct turn_attr_fingerprint);

      message.msg->turn_msg_len = htons(message.msg->turn_msg_len);
      turn_calculate_integrity_hmac((const unsigned char*)buf, total_len - sizeof(struct turn_attr_fingerprint) - sizeof(struct turn_attr_message_integrity), md_buf, sizeof(md_buf), hash);

      /* restore length */
      message.msg->turn_msg_len = len_save;
    }
    else
    {
      turn_calculate_integrity_hmac((const unsigned char*)buf, total_len -  sizeof(struct turn_attr_message_integrity), md_buf, sizeof(md_buf), hash);
    }

    nb = memcmp(hash, message.message_integrity->turn_attr_hmac, 20);
  }

  if(message.reservation_token)
  {
    memcpy(token, message.reservation_token->turn_attr_token, 8);
  }

  iovec_free_data(iov, index);
  index = 0;

  /* Refresh request */
  hdr = turn_msg_refresh_request_create(0, id, &iov[index]);
  index++;

  /* NONCE */
  attr = turn_attr_nonce_create(nonce, n_len, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REALM */
  attr = turn_attr_realm_create("domain.org", strlen("domain.org"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* USERNAME */
  attr = turn_attr_username_create("ping6", strlen("ping6"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* LIFETIME */
  attr = turn_attr_lifetime_create(0x00000006, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* SOFTWARE */
  attr = turn_attr_software_create("Client TURN 0.1 test", strlen("Client TURN 0.1 test"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REQUESTED-PROPS */
  attr = turn_attr_requested_props_create(0x00000000, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* MESSAGE-INTEGRITY */
  attr = turn_attr_message_integrity_create(NULL, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;

  nb = index; /* number of element before MESSAGE-INTEGRITY */
  index++;

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* after convert STUN/TURN message length to big endian we can calculate HMAC-SHA1 */
  /* index -1 because we do not take count MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"ping6:domain.org:password", strlen("ping6:domain.org:password"));
  turn_calculate_integrity_hmac_iov(iov, index - 1, md_buf, sizeof(md_buf), ((struct turn_attr_message_integrity*)attr)->turn_attr_hmac);
  attr2 = attr;

#if 1 
  /* FINGERPRINT */
  attr = turn_attr_fingerprint_create(0, &iov[index]);
  hdr->turn_msg_len = ntohs(hdr->turn_msg_len) + iov[index].iov_len;
  index++; 

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* calculate fingerprint */
  /* index -1, we do not take count FINGER attribute */
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc = htonl(turn_calculate_fingerprint(iov, index - 1));
#endif

  printf("Send refresh request\n");
  nb = turn_tcp_send(sock, iov, index);

  iovec_free_data(iov, index);
  index = 0;

  nb = recv(sock, buf, sizeof(buf), 0);

  /* ChannelBind request */
  hdr = turn_msg_channelbind_request_create(0, id, &iov[index]);
  index++;

  /* NONCE */
  attr = turn_attr_nonce_create(nonce, n_len, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REALM */
  attr = turn_attr_realm_create("domain.org", strlen("domain.org"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* USERNAME */
  attr = turn_attr_username_create("ping6", strlen("ping6"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* CHANNEL-NUMBER */
  attr = turn_attr_channel_number_create(0x4009, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* SOFTWARE */
  attr = turn_attr_software_create("Client TURN 0.1 test", strlen("Client TURN 0.1 test"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* PEER-ADDRESS */
  attr = turn_attr_peer_address_create((struct sockaddr*)&peer_addr, STUN_MAGIC_COOKIE, id, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* MESSAGE-INTEGRITY */
  attr = turn_attr_message_integrity_create(NULL, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;

  nb = index; /* number of element before MESSAGE-INTEGRITY */
  index++;

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* after convert STUN/TURN message length to big endian we can calculate HMAC-SHA1 */
  /* index -1 because we do not take count MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"ping6:domain.org:password", strlen("ping6:domain.org:password"));
  turn_calculate_integrity_hmac_iov(iov, index - 1, md_buf, sizeof(md_buf), ((struct turn_attr_message_integrity*)attr)->turn_attr_hmac);
  attr2 = attr;

#if 1
  /* FINGERPRINT */
  attr = turn_attr_fingerprint_create(0, &iov[index]);
  hdr->turn_msg_len = ntohs(hdr->turn_msg_len) + iov[index].iov_len;
  index++; 

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* calculate fingerprint */
  /* index -1, we do not take count FINGER attribute */
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc = htonl(turn_calculate_fingerprint(iov, index - 1));
#endif

  printf("Send ChannelBind request\n");
  nb = turn_tcp_send(sock, iov, index);

  iovec_free_data(iov, index);
  index = 0;

  nb = recv(sock, buf, sizeof(buf), 0);

  hdr = turn_msg_send_indication_create(0, id, &iov[index]);
  index++;

  attr = turn_attr_peer_address_create((struct sockaddr*)&peer_addr, STUN_MAGIC_COOKIE, id, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  attr = turn_attr_data_create("Hello", 5, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  printf("Send Send indication request\n");
  nb = turn_tcp_send(sock, iov, index);

  iovec_free_data(iov, index);
  index = 0;
  sleep(1);

  /* ChannelData */
  {
    char* str = "hello";
    uint32_t padding = 0;
    struct turn_channel_data channel_data;
    channel_data.turn_channel_number = htons(0x4009);
    channel_data.turn_channel_len = htons(5);

    iov[index].iov_base = &channel_data;
    iov[index].iov_len = sizeof(struct turn_channel_data);
    index++;

    iov[index].iov_base = str;
    iov[index].iov_len = strlen(str);
    index++;
    if(strlen(str) % 4)
    {
      printf("add padding\n");
      iov[index].iov_base = &padding;
      iov[index].iov_len = 4 - (strlen(str) % 4);
      index++;
    }

    printf("ChannelData\n");
    turn_tcp_send(sock, iov, index);
  }

  sleep(1);
  close(sock);

  sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  index = 0;

  /* Allocate request */
  hdr = turn_msg_allocate_request_create(0, id, &iov[index]);
  index++;

  memset(&message, 0x00, sizeof(message));

  /* NONCE */
  attr = turn_attr_nonce_create(nonce, n_len, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REALM */
  attr = turn_attr_realm_create("domain.org", strlen("domain.org"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* USERNAME */
  attr = turn_attr_username_create("ping6", strlen("ping6"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* LIFETIME */
  attr = turn_attr_lifetime_create(0x00000005, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* SOFTWARE */
  attr = turn_attr_software_create("Client TURN 0.1 test", strlen("Client TURN 0.1 test"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REQUESTED-TRANSPORT */
  attr = turn_attr_requested_transport_create(IPPROTO_UDP, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REQUESTED-PROPS */
  attr = turn_attr_requested_props_create(0x00000000, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

#if 1
  /* RESERVATION-TOKEN */
  attr = turn_attr_reservation_token_create(token, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;
#endif

  /* REQUESTED-ADDRESS-TYPE */
  attr = turn_attr_requested_address_type_create(STUN_ATTR_FAMILY_IPV4, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* MESSAGE-INTEGRITY */
  attr = turn_attr_message_integrity_create(NULL, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;

  nb = index; /* number of element before MESSAGE-INTEGRITY */
  index++;

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* after convert STUN/TURN message length to big endian we can calculate HMAC-SHA1 */
  /* index -1 because we do not take count MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"ping6:domain.org:password", strlen("ping6:domain.org:password"));
  turn_calculate_integrity_hmac_iov(iov, index - 1, md_buf, sizeof(md_buf), ((struct turn_attr_message_integrity*)attr)->turn_attr_hmac);
  attr2 = attr;

#if 1
  /* FINGERPRINT */
  attr = turn_attr_fingerprint_create(0, &iov[index]);
  hdr->turn_msg_len = ntohs(hdr->turn_msg_len) + iov[index].iov_len;
  index++; 

  /* convert to big endian */
  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  /* calculate fingerprint */
  /* index -1, we do not take count FINGER attribute */
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc = htonl(turn_calculate_fingerprint(iov, index - 1));
#endif

  printf("Send allocate request\n");
  nb = turn_tcp_send(sock, iov, index);

  iovec_free_data(iov, index);
  close(sock);

  return EXIT_SUCCESS;
}
