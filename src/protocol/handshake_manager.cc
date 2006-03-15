// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <rak/socket_address.h>

#include "net/manager.h"
#include "torrent/exceptions.h"
#include "download/download_info.h"

#include "peer_info.h"
#include "handshake.h"
#include "handshake_manager.h"

namespace torrent {

inline void
HandshakeManager::delete_handshake(Handshake* h) {
  socketManager.dec_socket_count();

  h->clear();
  h->get_fd().close();
  h->get_fd().clear();

  delete h;
}

HandshakeManager::size_type
HandshakeManager::size_info(DownloadInfo* info) const {
  return std::count_if(Base::begin(), Base::end(), rak::equal(info, std::mem_fun(&Handshake::download_info)));
}

void
HandshakeManager::clear() {
  std::for_each(Base::begin(), Base::end(), std::bind1st(std::mem_fun(&HandshakeManager::delete_handshake), this));
  Base::clear();
}

void
HandshakeManager::erase(Handshake* handshake) {
  iterator itr = std::find(Base::begin(), Base::end(), handshake);

  if (itr == Base::end())
    throw internal_error("HandshakeManager::erase(...) could not find handshake.");

  Base::erase(itr);
}

struct handshake_manager_equal : std::binary_function<const rak::socket_address*, const Handshake*, bool> {
  bool operator () (const rak::socket_address* sa1, const Handshake* p2) const {
    return *sa1 == *p2->peer_info()->socket_address();
  }
};

bool
HandshakeManager::find(const rak::socket_address& sa) {
  return std::find_if(Base::begin(), Base::end(), std::bind1st(handshake_manager_equal(), &sa)) != Base::end();
}

void
HandshakeManager::erase_info(DownloadInfo* info) {
  iterator split = std::partition(Base::begin(), Base::end(), rak::not_equal(info, std::mem_fun(&Handshake::download_info)));

  std::for_each(split, Base::end(), std::bind1st(std::mem_fun(&HandshakeManager::delete_handshake), this));
  Base::erase(split, Base::end());
}

void
HandshakeManager::add_incoming(SocketFd fd, const rak::socket_address& sa) {
  if (!socketManager.can_connect(sa.c_sockaddr()) || !fd.set_nonblock()) {
    fd.close();
    return;
  }

  socketManager.inc_socket_count();

  Handshake* h = new Handshake(fd, this);
  h->initialize_incoming(sa);

  Base::push_back(h);
}
  
void
HandshakeManager::add_outgoing(const rak::socket_address& sa, DownloadInfo* info) {
  if (!socketManager.can_connect(sa.c_sockaddr()))
    return;

  SocketFd fd;

  if (!fd.open_stream())
    return;

  const rak::socket_address* bindAddress = rak::socket_address::cast_from(socketManager.bind_address());

  if (!fd.set_nonblock() ||
      (bindAddress->is_bindable() && !fd.bind(*bindAddress)) ||
      !fd.connect(sa)) {
    fd.close();
    return;
  }

  socketManager.inc_socket_count();

  Handshake* h = new Handshake(fd, this);
  h->initialize_outgoing(sa, info);

  Base::push_back(h);
}

void
HandshakeManager::receive_succeeded(Handshake* h) {
  erase(h);
  h->clear();

//   h->download_info()->signal_network_log().emit("Successful handshake: " + h->peer_info()->get_address());

  m_slotConnected(h->get_fd(), h->download_info(), *h->peer_info());

  h->set_fd(SocketFd());
  delete h;
}

void
HandshakeManager::receive_failed(Handshake* h) {
  erase(h);

//   if (h->download_info() != NULL)
//     h->download_info()->signal_network_log().emit("Failed handshake: " + h->peer_info()->get_address());

  delete_handshake(h);
}

}
