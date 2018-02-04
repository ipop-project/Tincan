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
#include "tunnel.h"
#include "tincan_exception.h"
#include "tincan_control.h"

namespace tincan
{
Tunnel::Tunnel(
  unique_ptr<OverlayDescriptor> descriptor,
  shared_ptr<IpopControllerLink> ctrl_handle) :
  Overlay(move(descriptor), ctrl_handle)
{}

shared_ptr<VirtualLink>
Tunnel::CreateVlink(
  unique_ptr<VlinkDescriptor> vlink_desc,
  unique_ptr<PeerDescriptor> peer_desc)
{
  if(vlink_)
  {
    vlink_->PeerCandidates(peer_desc->cas);
    vlink_->StartConnections();
    LOG(LS_INFO) << "Added remote CAS to vlink w/ peer "
      << vlink_->PeerInfo().uid;
  }
  else
  {
    cricket::IceRole ir = cricket::ICEROLE_CONTROLLED;
    if(local_fingerprint_->ToString() < peer_desc->fingerprint)
      ir = cricket::ICEROLE_CONTROLLING;
    string roles[] = { "CONTROLLED", "CONTROLLING" };
    LOG(LS_INFO) << "Creating " << roles[ir] << " vlink w/ peer "
      << peer_desc->uid;
    vlink_ = Overlay::CreateVlink(move(vlink_desc), move(peer_desc), ir);
  }
  return vlink_;
}

void Tunnel::QueryInfo(
  Json::Value & olay_info)
{
  olay_info[TincanControl::OverlayId] = descriptor_->uid;
  olay_info[TincanControl::FPR] = Fingerprint();
  olay_info[TincanControl::TapName] = tap_desc_->name;
  olay_info[TincanControl::MAC] = MacAddress();
  olay_info[TincanControl::VIP4] = tap_desc_->ip4;
  olay_info["IP4PrefixLen"] = tap_desc_->prefix4;
  olay_info[TincanControl::MTU4] = tap_desc_->mtu4;
  olay_info["LinkIds"] = Json::Value(Json::arrayValue);
  if(vlink_)
  {
    olay_info["LinkIds"].append(vlink_->Id());
  }
}

void Tunnel::QueryLinkCas(
  const string & vlink_id,
  Json::Value & cas_info)
{
  if(vlink_)
  {
    if(vlink_->IceRole() == cricket::ICEROLE_CONTROLLING)
      cas_info[TincanControl::IceRole] = TincanControl::Controlling.c_str();
    else if(vlink_->IceRole() == cricket::ICEROLE_CONTROLLED)
      cas_info[TincanControl::IceRole] = TincanControl::Controlled.c_str();

    cas_info[TincanControl::CAS] = vlink_->Candidates();
  }
}

void Tunnel::QueryLinkIds(vector<string>& link_ids)
{
  if(vlink_)
    link_ids.push_back(vlink_->Id());
}

void Tunnel::QueryLinkInfo(
  const string & vlink_id,
  Json::Value & vlink_info)
{
  if(vlink_)
  {
    vlink_info[TincanControl::LinkId] = vlink_->Id();
    if(vlink_->IsReady())
    {
      if(vlink_->IceRole() == cricket::ICEROLE_CONTROLLING)
        vlink_info[TincanControl::IceRole] = TincanControl::Controlling;
      else
        vlink_info[TincanControl::IceRole] = TincanControl::Controlled;
      LinkInfoMsgData md;
      md.vl = vlink_;
      net_worker_.Post(RTC_FROM_HERE, this, MSGID_QUERY_NODE_INFO, &md);
      md.msg_event.Wait(Event::kForever);
      vlink_info[TincanControl::Stats].swap(md.info);
      vlink_info[TincanControl::Status] = "online";
    }
    else
    {
      vlink_info[TincanControl::Status] = "offline";
      vlink_info[TincanControl::Stats] = Json::Value(Json::arrayValue);
    }
  }
  else
  {
    vlink_info[TincanControl::Status] = "unknown";
    vlink_info[TincanControl::Stats] = Json::Value(Json::arrayValue);
  }

}

void Tunnel::SendIcc(
  const string & vlink_id,
  const string & data)
{
  if(!vlink_ || vlink_->Id() != vlink_id)
    throw TCEXCEPT("No vlink exists by the specified id");
  unique_ptr<IccMessage> icc = make_unique<IccMessage>();
  icc->Message((uint8_t*)data.c_str(), (uint16_t)data.length());
  unique_ptr<TransmitMsgData> md = make_unique<TransmitMsgData>();
  md->frm = move(icc);
  md->vl = vlink_;
  net_worker_.Post(RTC_FROM_HERE, this, MSGID_SEND_ICC, md.release());

}

void Tunnel::Shutdown()
{
  Overlay::Shutdown();
  if (vlink_)
    vlink_->Disconnect();
}

void
Tunnel::StartIo()
{
  tdev_->Up();
  Overlay::StartIo();
}

void Tunnel::RemoveLink(
  const string & vlink_id)
{
  if(vlink_->Id() != vlink_id)
    throw TCEXCEPT("The specified VLink ID does not match this Tunnel");
  vlink_->Disconnect();
  vlink_.reset();
}

void
Tunnel::UpdateRouteTable(
  const Json::Value & rt_descr)
{}

/*
When the overlay is a tunnel, the only operations are sending ICCs and normal IO
*/
void Tunnel::VlinkReadComplete(
  uint8_t * data,
  uint32_t data_len,
  VirtualLink & vlink)
{
  unique_ptr<TapFrame> frame = make_unique<TapFrame>(data, data_len);
  TapFrameProperties fp(*frame);
  if(fp.IsDtfMsg())
  {
    //frame->Dump("Frame from vlink");
    frame->BufferToTransfer(frame->Payload()); //write frame payload to TAP
    frame->BytesToTransfer(frame->PayloadLength());
    frame->SetWriteOp();
    tdev_->Write(*frame.release());
  }
  else if(fp.IsIccMsg())
  { // this is an ICC message, deliver to the ipop-controller
    unique_ptr<TincanControl> ctrl = make_unique<TincanControl>();
    ctrl->SetControlType(TincanControl::CTTincanRequest);
    Json::Value & req = ctrl->GetRequest();
    req[TincanControl::Command] = TincanControl::ICC;
    req[TincanControl::OverlayId] = descriptor_->uid;
    req[TincanControl::LinkId] = vlink.Id();
    req[TincanControl::Data] = string((char*)frame->Payload(),
      frame->PayloadLength());
    //LOG(TC_DBG) << " Delivering ICC to ctrl, data=\n"
    //<< req[TincanControl::Data].asString();
    ctrl_link_->Deliver(move(ctrl));
  }
  else
  {
    LOG(LS_ERROR) << "Unknown frame type received!";
    frame->Dump("Invalid header");
  }
}

void Tunnel::TapReadComplete(
  AsyncIo * aio_rd)
{
  TapFrame * frame = static_cast<TapFrame*>(aio_rd->context_);
  if(!aio_rd->good_)
  {
    frame->Initialize();
    frame->BufferToTransfer(frame->Payload());
    frame->BytesToTransfer(frame->PayloadCapacity());
    if(0 != tdev_->Read(*frame))
      delete frame;
    return;
  }
  frame->PayloadLength(frame->BytesTransferred());
  frame->BufferToTransfer(frame->Begin()); //write frame header + PL to vlink
  frame->BytesToTransfer(frame->Length());
  frame->Header(kDtfMagic);
  TransmitMsgData *md = new TransmitMsgData;
  md->frm.reset(frame);
  md->vl = vlink_;
  net_worker_.Post(RTC_FROM_HERE, this, MSGID_TRANSMIT, md);
}

void Tunnel::TapWriteComplete(
  AsyncIo * aio_wr)
{
  //TapFrame * frame = static_cast<TapFrame*>(aio_wr->context_);
  delete static_cast<TapFrame*>(aio_wr->context_);
}

} // end namespace tincan
