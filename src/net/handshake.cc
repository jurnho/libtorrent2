#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "general.h"
#include "torrent/exceptions.h"
#include "download_main.h"
#include "handshake.h"

#include <unistd.h>
#include <netinet/in.h>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

extern std::list<std::string> caughtExceptions;

Handshake::Handshakes Handshake::m_handshakes;

// Incoming connections.
Handshake::Handshake(int fdesc, const std::string dns, unsigned short port)  :
  SocketBase(fdesc),
  m_peer("", dns, port),
  m_download(NULL),
  m_state(READ_HEADER),
  m_incoming(true),
  m_buf(new char[256+48]),
  m_pos(0)
{
  insert_read();
  insert_except();
}

// Outgoing connections.
Handshake::Handshake(int fdesc, const PeerInfo& p, DownloadState* d) :
  SocketBase(fdesc),
  m_peer(p),
  m_download(d),
  m_state(CONNECTING),
  m_incoming(false),
  m_buf(new char[256+48]),
  m_pos(0)
{
  insert_write();
  insert_except();

  m_buf[0] = 19;
  std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
  std::memset(&m_buf[20], 0, 8);
  std::memcpy(&m_buf[28], m_download->hash().c_str(), 20);
  std::memcpy(&m_buf[48], m_download->me().id().c_str(), 20);
}

Handshake::~Handshake() {
  removeConnection(this);

  delete [] m_buf;

  if (m_fd >= 0)
    ::close(m_fd);
}

void Handshake::connect(int fdesc, const std::string dns, unsigned short port) {
  if (fdesc < 0)
    //return;
    throw internal_error("PeerhHandshake received a negative fd, bug or feature?");

  std::stringstream s;

  s << "Incoming connection " << dns << ':' << port;

  caughtExceptions.push_front(s.str());

  set_socket_nonblock(fdesc);

  // TODO: add checks so we don't do multiple connections.
  addConnection(new Handshake(fdesc, dns, port));
}

bool Handshake::connect(const PeerInfo& p, DownloadState* d) {
  if (p.dns().length() == 0 ||
      p.port() == 0)
    throw internal_error("Tried to connect with invalid peer information");

  try {
    
    sockaddr_in sa;
    make_sockaddr(p.dns(), p.port(), sa);

    addConnection(new Handshake(make_socket(sa), p, d));

    return true;

  } catch (network_error& e) {
    return false;
  }
}

void Handshake::read() {
  DownloadMain* d;

  try {

  switch (m_state) {
  case READ_HEADER:
    if (!recv1())
      return;

    if (m_incoming) {
      if ((d = DownloadMain::getDownload(m_infoHash)) != NULL) {
	m_download = &d->state();

	remove_read();
	insert_write();
	
	m_buf[0] = 19;
	std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
	std::memset(&m_buf[20], 0, 8);
	std::memcpy(&m_buf[28], m_download->hash().c_str(), 20);
	std::memcpy(&m_buf[48], m_download->me().id().c_str(), 20);
	
	m_state = WRITE_HEADER;
	m_pos = 0;
	
	return;

      } else {
	throw communication_error("Peer connection for unknown file hash");
      }

    } else {
      if (m_infoHash != m_download->hash())
	throw communication_error("Peer returned bad file hash");

      m_state = READ_ID;
      m_pos = 0;
    }

  case READ_ID:
    if (!recv2())
      return;

    if (m_peer.id() == m_download->me().id())
      throw communication_error("Connected to client with the same id");

    m_download->addConnection(m_fd, m_peer);
    m_fd = -1;
      
    delete this;
    return;

  default:
    throw internal_error("Handshake::read() called on object in wrong state");
  }

  } catch (network_error& e) {
    //caughtExceptions.push_front("Handshake: " + std::string(e.what()));

    delete this;
  }
}

void Handshake::write() {
  int error;

  try {

  switch (m_state) {
  case CONNECTING:
    error = get_socket_error(m_fd);

    if (error != 0) {
      throw network_error("Could not connect to client");
    }

    m_state = WRITE_HEADER;

  case WRITE_HEADER:
    if (!write_buf(m_buf + m_pos, 68, m_pos))
      return;

    remove_write();
    insert_read();

    m_pos = 0;
    m_state = m_incoming ? READ_ID : READ_HEADER;

    return;

  default:
    throw internal_error("Handshake::write() called on object in wrong state");
  }

  } catch (network_error& e) {
    //caughtExceptions.push_front("Handshake: " + std::string(e.what()));

    delete this;
  }
}

void Handshake::except() {
  delete this;
}

bool Handshake::recv1() {
  if (m_pos == 0 &&
      !read_buf(m_buf, 1, m_pos))
    return false;

  int l = (unsigned char)m_buf[0];

  if (!read_buf(m_buf + m_pos, l + 29, m_pos))
    return false;

  m_peer.ref_protocol() = std::string(&m_buf[1], l);
  m_peer.ref_options()  = std::string(&m_buf[1+l], 8);
  m_infoHash            = std::string(&m_buf[9+l], 20);

  if (m_peer.protocol() != "BitTorrent protocol") {
    throw communication_error("Peer returned wrong protocol identifier");
  } else {
    return true;
  }
}

bool Handshake::recv2() {
  if (!read_buf(m_buf + m_pos, 20, m_pos))
    return false;

  m_peer.ref_id() = std::string(m_buf, 20);

  return true;
}  

void Handshake::addConnection(Handshake* p) {
  if (p == NULL)
    throw internal_error("Tried to add bad Handshake to handshake cue");

  if (m_handshakes.size() > 2000)
    throw internal_error("Handshake queue is bigger than 2000");

  m_handshakes.push_back(p);
}

void Handshake::removeConnection(Handshake* p) {
  Handshakes::iterator itr = std::find(m_handshakes.begin(), m_handshakes.end(), p);

  if (itr == m_handshakes.end())
    throw internal_error("Could not remove connection from torrent, not found");

  m_handshakes.erase(itr);
}

bool Handshake::isConnecting(const std::string& id) {
  return std::find_if(m_handshakes.begin(), m_handshakes.end(),
		      eq(ref(id), on<const std::string&>(call_member(&Handshake::peer),
							 call_member(&PeerInfo::id))))
    != m_handshakes.end();
}

}