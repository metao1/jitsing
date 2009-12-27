/*
 *  TurnServer - TURN server implementation.
 *  Copyright (C) 2008-2009 Sebastien Vincent <sebastien.vincent@turnserver.org>
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
 * \file test_client_dtls.c
 * \brief TLS over UDP TURN client example.
 * \author Sebastien Vincent
 * \date 2009
 */

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

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
  struct sockaddr_storage peer_addr;
  struct sockaddr_storage peer_addr2;
  int sock = -1;
  unsigned char md_buf[16]; /* MD5 */
  struct turn_message message;
  uint16_t tabu[16];
  size_t tabu_size = sizeof(tabu) / sizeof(uint16_t);
  uint8_t nonce[513];
  char buf[8192];
  char buf2[8192];
  ssize_t nb2 = -1;
  size_t n_len = 0;
  struct tls_peer* speer = NULL;
  socklen_t server_addr_size = 0; 
  char peer_port[8];
  struct addrinfo hints;
  struct addrinfo* res = NULL;
  struct sockaddr_storage daddr;
  socklen_t daddr_size = sizeof(struct sockaddr_storage);

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

  if(getaddrinfo(argv[2], "5349", &hints, &res) != 0)
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
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol =  IPPROTO_UDP;
  hints.ai_flags = 0;

  if(getaddrinfo(argv[3], peer_port, &hints, &res) != 0)
  {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }
  memcpy(&peer_addr, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  /* get address for peer_addr2 */
  snprintf(peer_port, sizeof(peer_port), "%s", argv[4]);
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol =  IPPROTO_UDP;
  hints.ai_flags = 0;

  if(getaddrinfo("10.1.0.3", peer_port, &hints, &res) != 0)
  {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }
  memcpy(&peer_addr2, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  /* libssl initialization */
  SSL_library_init();
  /* OpenSSL_add_all_algorithms(); */
  SSL_load_error_strings();
  ERR_load_crypto_strings();

  nb = turn_generate_transaction_id(id);

  speer = tls_peer_new(IPPROTO_UDP, argv[1], 0, "./ca.crt", "client1.crt", "./client1.key");

  if(!speer)
  {
    perror("speer");
    /* cleanup SSL lib */
    EVP_cleanup();
    ERR_remove_state(0);
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
    exit(EXIT_FAILURE);
  }

  sock = speer->sock;

  if(connect(speer->sock, (struct sockaddr*)&server_addr, server_addr_size) == -1)
  {
    perror("connect");
    /* cleanup SSL lib */
    EVP_cleanup();
    ERR_remove_state(0);
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
    exit(EXIT_FAILURE);
  }
  printf("connected\n");

  if(tls_peer_do_handshake(speer, (struct sockaddr*)&server_addr, server_addr_size) == -1)
  {
    printf("Handshake failed!\n");
    tls_peer_free(&speer);
    /* cleanup SSL lib */
    EVP_cleanup();
    ERR_remove_state(0);
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
    exit(EXIT_FAILURE);
  }
  else
  {
    printf("Handshake completed!\n");
  }

  /* Allocate request */
  index = 0;
  hdr = turn_msg_allocate_request_create(0, id, &iov[index]);
  index++;

  printf("Send allocate request\n");

  nb = turn_tls_send(speer, (struct sockaddr*)&server_addr, server_addr_size, sizeof(*hdr), iov, index);

  if(nb == -1)
  {
    perror("sendto\n");
    tls_peer_free(&speer);
    /* cleanup SSL lib */
    EVP_cleanup();
    ERR_remove_state(0);
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
    exit(EXIT_FAILURE);
  }
  memset(&message, 0x00, sizeof(message));

  printf("wait message\n");
  nb2 = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&daddr, &daddr_size);

  getpeername(sock, (struct sockaddr*)&server_addr, &server_addr_size);

  nb2 = tls_peer_udp_read(speer, buf, nb2, buf2, sizeof(buf2), (struct sockaddr*)&server_addr, server_addr_size);
  if(nb2 == -1)
  {
    perror("tls_peer_udp_read");
    tls_peer_free(&speer);
    /* cleanup SSL lib */
    EVP_cleanup();
    ERR_remove_state(0);
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
    exit(EXIT_FAILURE);
  }

  nb = turn_parse_message(buf2, nb2, &message, tabu, &tabu_size);
  printf("parsing done\n");

  memcpy(nonce, message.nonce->turn_attr_nonce, ntohs(message.nonce->turn_attr_len));
  nonce[ntohs(message.nonce->turn_attr_len)] = 0x00;
  n_len = ntohs(message.nonce->turn_attr_len);

  /* NONCE */
  attr = turn_attr_nonce_create(nonce, ntohs(message.nonce->turn_attr_len), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REALM */
  attr = turn_attr_realm_create("domain.org", strlen("domain.org"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* USERNAME */
  attr = turn_attr_username_create("toto", strlen("toto"), &iov[index]);
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

  /* EVEN-PORT */
  attr = turn_attr_even_port_create(0x00, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* REQUESTED-ADDRESS-FAMILY */
  attr = turn_attr_requested_address_family_create(peer_addr.ss_family == AF_INET ? STUN_ATTR_FAMILY_IPV4 : STUN_ATTR_FAMILY_IPV6, &iov[index]);
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
  /* index -1 because we do not take into account MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"toto:domain.org:password", strlen("toto:domain.org:password"));
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
  /* index -1, we do not take into account FINGERPRINT attribute */
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc = htonl(turn_calculate_fingerprint(iov, index - 1));
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc ^= htonl(STUN_FINGERPRINT_XOR_VALUE);
#endif

  printf("Send allocate request\n");
  nb = turn_tls_send(speer, (struct sockaddr*)&server_addr, server_addr_size, ntohs(hdr->turn_msg_len) + sizeof(*hdr), iov, index);

  iovec_free_data(iov, index);

  nb = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&daddr, &daddr_size);
  nb = tls_peer_udp_read(speer, buf, nb, buf2, sizeof(buf2), (struct sockaddr*)&server_addr, server_addr_size);

  nb2 = 0;
  nb = turn_parse_message(buf, (size_t)nb, &message, NULL, (size_t*)&nb2);

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
      turn_calculate_integrity_hmac((const unsigned char*)buf, total_len - sizeof(struct turn_attr_fingerprint) - sizeof(struct turn_attr_message_integrity),
                                    md_buf, sizeof(md_buf), hash);

      /* restore length */
      message.msg->turn_msg_len = len_save;
    }
    else
    {
      turn_calculate_integrity_hmac((const unsigned char*)buf, total_len -  sizeof(struct turn_attr_message_integrity), md_buf, sizeof(md_buf), hash);
    }

    nb = memcmp(hash, message.message_integrity->turn_attr_hmac, 20);
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
  attr = turn_attr_username_create("toto", strlen("toto"), &iov[index]);
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

  /* EVEN-PORT */
  attr = turn_attr_even_port_create(0x00, &iov[index]);
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
  /* index -1 because we do not take into account MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"toto:domain.org:password", strlen("toto:domain.org:password"));
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
  /* index -1, we do not take into account FINGERPRINT attribute */
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc = htonl(turn_calculate_fingerprint(iov, index - 1));
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc ^= htonl(STUN_FINGERPRINT_XOR_VALUE);
#endif

  printf("Send refresh request\n");
  nb = turn_tls_send(speer, (struct sockaddr*)&server_addr, server_addr_size, ntohs(hdr->turn_msg_len) + sizeof(*hdr), iov, index);

  iovec_free_data(iov, index);
  index = 0;

  nb = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&daddr, &daddr_size);
  nb2 = tls_peer_udp_read(speer, buf, nb, buf2, sizeof(buf2), (struct sockaddr*)&server_addr, server_addr_size);

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
  attr = turn_attr_username_create("toto", strlen("toto"), &iov[index]);
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

  /* XOR-PEER-ADDRESS */
  /*  peer_addr.sin_addr.s_addr = inet_addr("10.168.0.1"); */
  /*  peer_addr.sin_port = htons(445); */
  attr = turn_attr_xor_peer_address_create((struct sockaddr*)&peer_addr, STUN_MAGIC_COOKIE, id, &iov[index]);
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
  /* index -1 because we do not take into account MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"toto:domain.org:password", strlen("toto:domain.org:password"));
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
  /* index -1, we do not take into account FINGERPRINT attribute */
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc = htonl(turn_calculate_fingerprint(iov, index - 1));
  ((struct turn_attr_fingerprint*)attr)->turn_attr_crc ^= htonl(STUN_FINGERPRINT_XOR_VALUE);
#endif

#if 1
  printf("Send ChannelBind request\n");
  nb = turn_tls_send(speer, (struct sockaddr*)&server_addr, server_addr_size, ntohs(hdr->turn_msg_len) + sizeof(*hdr), iov, index);
  nb = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&daddr, &daddr_size);
  nb2 = tls_peer_udp_read(speer, buf, nb, buf2, sizeof(buf2), (struct sockaddr*)&server_addr, server_addr_size);
#endif

  iovec_free_data(iov, index);
  index = 0;

  /* CreatePermission */
  hdr = turn_msg_createpermission_request_create(0, id, &iov[index]);
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
  attr = turn_attr_username_create("toto", strlen("toto"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* SOFTWARE */
  attr = turn_attr_software_create("Client TURN 0.1 test", strlen("Client TURN 0.1 test"), &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* XOR-PEER-ADDRESS */
  attr = turn_attr_xor_peer_address_create((struct sockaddr*)&peer_addr, STUN_MAGIC_COOKIE, id, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  /* XOR-PEER-ADDRESS */
  attr = turn_attr_xor_peer_address_create((struct sockaddr*)&peer_addr2, STUN_MAGIC_COOKIE, id, &iov[index]);
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
  /* index -1 because we do not take into account MESSAGE-INTEGRITY attribute */
  md5_generate(md_buf, (unsigned char*)"toto:domain.org:password", strlen("toto:domain.org:password"));
  turn_calculate_integrity_hmac_iov(iov, index - 1, md_buf, sizeof(md_buf), ((struct turn_attr_message_integrity*)attr)->turn_attr_hmac);
  attr2 = attr;

#if 1
  printf("Send CreatePermission request\n");
  nb = turn_tls_send(speer, (struct sockaddr*)&server_addr, server_addr_size, ntohs(hdr->turn_msg_len) + sizeof(*hdr), iov, index);
  nb = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&daddr, &daddr_size);
  nb2 = tls_peer_udp_read(speer, buf, nb, buf2, sizeof(buf2), (struct sockaddr*)&server_addr, server_addr_size);
#endif
  iovec_free_data(iov, index);
  index = 0;

  hdr = turn_msg_send_indication_create(0, id, &iov[index]);
  index++;

  attr = turn_attr_xor_peer_address_create((struct sockaddr*)&peer_addr, STUN_MAGIC_COOKIE, id, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  attr = turn_attr_data_create("Hello", 5, &iov[index]);
  hdr->turn_msg_len += iov[index].iov_len;
  index++;

  hdr->turn_msg_len = htons(hdr->turn_msg_len);

  printf("Send Send indication request\n");
  nb = turn_tls_send(speer, (struct sockaddr*)&server_addr, server_addr_size, ntohs(hdr->turn_msg_len) + sizeof(*hdr), iov, index);

  iovec_free_data(iov, index);
  index = 0;
  sleep(1);

#if 0
  printf("Receive\n");
  nb2 = recvfrom(speer->sock, buf, sizeof(buf), 0, (struct sockaddr*)&daddr, &daddr_size);
  printf("Decode\n");
  nb2 = tls_peer_udp_read(speer, buf, nb2, buf2, sizeof(buf2), (struct sockaddr*)&server_addr, server_addr_size);
  nb = turn_parse_message(buf2, nb2, &message, tabu, &tabu_size);

  if(nb == -1)
  {
    printf("turn_parse_message failed\n");
  }

  if(message.data)
  {
    char received[8192];
    printf("Received %u bytes\n", ntohs(message.data->turn_attr_len));
    memcpy(received, message.data->turn_attr_data, ntohs(message.data->turn_attr_len));
    received[ntohs(message.data->turn_attr_len)] = 0x00;
    printf("I receive %s\n", received);
  }
#endif

  /* ChannelData */
  {
    char* str = "hello";
    uint32_t padding = 0;
    size_t data_len = 0;
    struct turn_channel_data channel_data;
    channel_data.turn_channel_number = htons(0x4009);
    channel_data.turn_channel_len = htons(5);

    data_len = sizeof(struct turn_channel_data) + strlen(str);

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
      data_len += iov[index].iov_len;
      index++;
    }

    printf("ChannelData len = %zu\n", data_len);
    nb = turn_tls_send(speer, (struct sockaddr*)&server_addr, server_addr_size, data_len, iov, index);

#if 1
    nb2 = recvfrom(speer->sock, buf, sizeof(buf), 0, (struct sockaddr*)&daddr, &daddr_size);
    nb2 = tls_peer_udp_read(speer, buf, nb2, buf2, sizeof(buf2), (struct sockaddr*)&server_addr, server_addr_size);

    if(nb2 > 0)
    {
      char received[8192];
      struct turn_channel_data* dt = (struct turn_channel_data*)buf2;
      printf("Received %u bytes\n", ntohs(dt->turn_channel_len));
      memcpy(received, dt->turn_channel_data, ntohs(dt->turn_channel_len));
      received[ntohs(dt->turn_channel_len)] = 0x00;
      printf("I receive %s (channel data)\n", received);
    }
#endif
  }

  sleep(1);
  tls_peer_free(&speer);

  /* cleanup SSL lib */
  EVP_cleanup();
  ERR_remove_state(0);
  ERR_free_strings();
  CRYPTO_cleanup_all_ex_data();

  return EXIT_SUCCESS;
}

