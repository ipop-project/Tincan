/*
* ipop-project
* Copyright 2016, University of Florida
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/
#ifndef TINCAN_VIRTUAL_NETWORK_H_
#define TINCAN_VIRTUAL_NETWORK_H_
#include "tincan_base.h"
#include "overlay.h"
namespace tincan
{
class VirtualNetwork :
  public Overlay
{
public:
  enum MSG_ID
  {
    //MSGID_CREATE_LINK,
    //MSGID_START_CONNECTION,
    MSGID_TRANSMIT,
    MSGID_SEND_ICC,
    //MSGID_END_CONNECTION,
    MSGID_QUERY_NODE_INFO,
    MSGID_FWD_FRAME,
    MSGID_FWD_FRAME_RD,
  };
  class TransmitMsgData : public MessageData
  {
  public:
    shared_ptr<VirtualLink> tnl;
    unique_ptr<TapFrame> frm;
  };
  //class VlinkMsgData : public MessageData
  //{
  //public:
  //  shared_ptr<VirtualLink> vl;
  //};
  //class MacMsgData : public MessageData
  //{
  //public:
  //  MacAddressType mac;
  //};
  //class CreateVlinkMsgData : public MessageData
  //{
  //public:
  //  unique_ptr<PeerDescriptor> peer_desc;
  //  unique_ptr<VlinkDescriptor> vlink_desc;
  //  rtc::Event msg_event;
  //  CreateVlinkMsgData() : msg_event(false, false)
  //  {}
  //  ~CreateVlinkMsgData() = default;
  //};
  class LinkStatsMsgData : public MessageData
  {
  public:
    shared_ptr<VirtualLink> vl;
    Json::Value stats;
    rtc::Event msg_event;
    LinkStatsMsgData() : stats(Json::arrayValue), msg_event(false, false)
    {}
    ~LinkStatsMsgData() = default;
  };
  //ctor
   VirtualNetwork(
     unique_ptr<OverlayDescriptor> descriptor,
     shared_ptr<IpopControllerLink> ctrl_handle);

  ~VirtualNetwork();

  shared_ptr<VirtualLink> CreateVlink(
    unique_ptr<VlinkDescriptor> vlink_desc,
    unique_ptr<PeerDescriptor> peer_desc);

  void QueryInfo(
    Json::Value & olay_info);

  void QueryLinkCas(
    const string & vlink_id,
    Json::Value & cas_info);

  void QueryLinkInfo(
    const string & vlink_id,
    Json::Value & vlink_info);

  void Shutdown();

  void SendIcc(
    const string & recipient_mac,
    const string & data);

  void Start();

  void RemoveLink(
    const string & vlink_id);

  void UpdateRoute(
    MacAddressType mac_dest,
    MacAddressType mac_path);
  //
  //FrameHandler implementation
  void VlinkReadComplete(
    uint8_t * data,
    uint32_t data_len,
    VirtualLink & vlink);
  //
  //AsyncIOComplete
  void TapReadComplete(
    AsyncIo * aio_rd);
  void TapWriteComplete(
    AsyncIo * aio_wr);
  //
  //MessageHandler overrides
  void OnMessage(Message* msg) override;
private:
  unique_ptr<PeerNetwork> peer_network_;
  rtc::Thread peer_net_thread_;
};
}  // namespace tincan
#endif  // TINCAN_VIRTUAL_NETWORK_H_
