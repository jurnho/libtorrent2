#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include <torrent/common.h>
#include <torrent/entry.h>
#include <torrent/peer.h>
#include <torrent/tracker.h>

#include <iosfwd>
#include <list>
#include <vector>
#include <sigc++/slot.h>
#include <sigc++/connection.h>

namespace torrent {

typedef std::list<Peer> PList;

class Bencode;
struct DownloadWrapper;

class Download {
public:
  typedef std::vector<uint16_t> SeenVector;

  enum {
    NUMWANT_DISABLED = -1
  };

  Download(DownloadWrapper* d = NULL) : m_ptr(d) {}

  // Not active atm. Opens and prepares/closes the files.
  void                 open();
  void                 close();

  // Only call resume_load() on newly open()'ed downloads. 
  void                 hash_check(bool resume = true);
  void                 hash_save();

  // Start/stop the download. The torrent must be open.
  void                 start();
  void                 stop();

  // Does not check if the download has been removed.
  bool                 is_valid()  { return m_ptr; }

  bool                 is_open();
  bool                 is_active();
  bool                 is_tracker_busy();

  bool                 is_hash_checked();
  bool                 is_hash_checking();

  // Returns "" if the object is not valid.
  std::string          get_name();
  std::string          get_hash();

  // Only set the root directory while the torrent is closed.
  std::string          get_root_dir();
  void                 set_root_dir(const std::string& dir);

  // String to send to the tracker in the "ip" field. An empty string
  // means that nothing gets sent. The client is responsible for setting
  // a vaild ip address.
  std::string          get_ip();
  void                 set_ip(const std::string& ip);

  // Bytes uploaded this session.
  uint64_t             get_bytes_up();
  // Bytes downloaded this session.
  uint64_t             get_bytes_down();
  // Bytes completed.
  uint64_t             get_bytes_done();
  // Size of the torrent.
  uint64_t             get_bytes_total();

  uint32_t             get_chunks_size();
  uint32_t             get_chunks_done();
  uint32_t             get_chunks_total();

  // Bytes per second.
  uint32_t             get_rate_up();
  uint32_t             get_rate_down();
  
  const unsigned char* get_bitfield_data();
  uint32_t             get_bitfield_size();

  uint32_t             get_peers_min();
  uint32_t             get_peers_max();
  uint32_t             get_peers_connected();
  uint32_t             get_peers_not_connected();

  uint32_t             get_uploads_max();
  
  uint64_t             get_tracker_timeout();
  int16_t              get_tracker_numwant();

  void                 set_peers_min(uint32_t v);
  void                 set_peers_max(uint32_t v);

  void                 set_uploads_max(uint32_t v);

  void                 set_tracker_timeout(uint64_t v);
  void                 set_tracker_numwant(int16_t n);

  // Access the trackers in the torrent.
  Tracker              get_tracker(uint32_t index);
  uint32_t             get_tracker_size();

  // Access the files in the torrent.
  Entry                get_entry(uint32_t index);
  uint32_t             get_entry_size();

  const SeenVector&    get_seen();

  // Call this when you want the modifications of the download priorities
  // in the entries to take effect. It is slightly expensive as it rechecks
  // all the peer bitfields to see if we are still interested.
  void                 update_priorities();

  // If you create a peer list, you *must* keep it up to date with the signals
  // peer_{connected,disconnected}. Otherwise you may experience undefined
  // behaviour when using invalid peers in the list.
  void                 peer_list(PList& pList);
  Peer                 peer_find(const std::string& id);

  // Note on signals: If you bind it to a class member function, make sure the
  // class does not get copied as the binding only points to the original
  // memory location.

  typedef sigc::slot0<void>                Slot;

  typedef sigc::slot1<void, Peer>          SlotPeer;
  typedef sigc::slot1<void, Bencode&>      SlotTrackerSucceded;
  typedef sigc::slot1<void, std::string>   SlotTrackerFailed;
  typedef sigc::slot1<void, uint32_t>      SlotChunk;

  sigc::connection    signal_download_done(Slot s);
  sigc::connection    signal_hash_done(Slot s);

  sigc::connection    signal_peer_connected(SlotPeer s);
  sigc::connection    signal_peer_disconnected(SlotPeer s);

  sigc::connection    signal_tracker_succeded(SlotTrackerSucceded s);
  sigc::connection    signal_tracker_failed(SlotTrackerFailed s);

  sigc::connection    signal_chunk_passed(SlotChunk s);
  sigc::connection    signal_chunk_failed(SlotChunk s);

private:
  DownloadWrapper*      m_ptr;
};

}

#endif

