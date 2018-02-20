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

#include "tincan.h"
#include "tincan_exception.h"
namespace tincan
{
Tincan::Tincan() :
  exit_event_(false, false)
{}

Tincan::~Tincan()
{
}

void 
Tincan::SetIpopControllerLink(
  shared_ptr<IpopControllerLink> ctrl_handle)
{
  ctrl_link_ = ctrl_handle;
}

void Tincan::CreateOverlay(
  const Json::Value & olay_desc,
  Json::Value & olay_info)
{
  unique_ptr<OverlayDescriptor> ol_desc(new OverlayDescriptor);
  ol_desc->uid = olay_desc[TincanControl::OverlayId].asString();
  if(IsOverlayExisit(ol_desc->uid))
    throw TCEXCEPT("The specified overlay identifier already exisits");
  ol_desc->stun_addr = olay_desc["StunAddress"].asString();
  ol_desc->turn_addr = olay_desc["TurnAddress"].asString();
  ol_desc->turn_pass = olay_desc["TurnPass"].asString();
  ol_desc->turn_user = olay_desc["TurnUser"].asString();
  ol_desc->enable_ip_mapping = false;
  unique_ptr<Overlay> olay;
  if(olay_desc[TincanControl::Type].asString() == "VNET")
  {
    olay = make_unique<VirtualNetwork>(move(ol_desc), ctrl_link_);
  }
  else if(olay_desc[TincanControl::Type].asString() == "TUNNEL")
  {
    olay = make_unique<Tunnel>(move(ol_desc), ctrl_link_);
  }
  else
    throw TCEXCEPT("Invalid Overlay type specified");
  unique_ptr<TapDescriptor> tap_desc = make_unique<TapDescriptor>();
  tap_desc->name = olay_desc["TapName"].asString();
  tap_desc->ip4 = olay_desc["IP4"].asString();
  tap_desc->prefix4 = olay_desc["IP4PrefixLen"].asUInt();
  tap_desc->mtu4 = olay_desc[TincanControl::MTU4].asUInt();

  olay->Configure(move(tap_desc));
  olay->Start();
  olay->QueryInfo(olay_info);
  lock_guard<mutex> lg(ovlays_mutex_);
  ovlays_.push_back(move(olay));

  return;
}

void
Tincan::CreateVlink(
  const Json::Value & link_desc,
  const TincanControl & control)
{
  unique_ptr<VlinkDescriptor> vl_desc = make_unique<VlinkDescriptor>();
  vl_desc->uid = link_desc[TincanControl::LinkId].asString();
  unique_ptr<Json::Value> resp = make_unique<Json::Value>(Json::objectValue);
  Json::Value & olay_info = (*resp)[TincanControl::Message];
  string olid = link_desc[TincanControl::OverlayId].asString();
  if(!IsOverlayExisit(olid))
  {
    CreateOverlay(link_desc, olay_info);
  }
  unique_ptr<PeerDescriptor> peer_desc = make_unique<PeerDescriptor>();
  peer_desc->uid =
    link_desc[TincanControl::PeerInfo][TincanControl::UID].asString();
  peer_desc->vip4 =
    link_desc[TincanControl::PeerInfo][TincanControl::VIP4].asString();
  peer_desc->cas =
    link_desc[TincanControl::PeerInfo][TincanControl::CAS].asString();
  peer_desc->fingerprint =
    link_desc[TincanControl::PeerInfo][TincanControl::FPR].asString();
  peer_desc->mac_address =
    link_desc[TincanControl::PeerInfo][TincanControl::MAC].asString();
  string tap_name = link_desc[TincanControl::TapName].asString();

  vl_desc->sec_enabled = link_desc[TincanControl::EncryptionEnabled].asBool();

  Overlay & ol = OverlayFromId(olid);
  shared_ptr<VirtualLink> vlink =
    ol.CreateVlink(move(vl_desc), move(peer_desc));
  unique_ptr<TincanControl> ctrl = make_unique<TincanControl>(control);
  if(vlink->Candidates().empty())
  {
    ctrl->SetResponse(move(resp));
    std::lock_guard<std::mutex> lg(inprogess_controls_mutex_);
    inprogess_controls_[link_desc[TincanControl::LinkId].asString()]
      = move(ctrl);

    vlink->SignalLocalCasReady.connect(this, &Tincan::OnLocalCasUpdated);
  }
  else
  {
    (*resp)["Message"]["CAS"] = vlink->Candidates();
    (*resp)["Success"] = true;
    ctrl->SetResponse(move(resp));
    ctrl_link_->Deliver(move(ctrl));
  }
}

void
Tincan::InjectFrame(
  const Json::Value & frame_desc)
{
  const string & olid = frame_desc[TincanControl::OverlayId].asString();
  Overlay & ol = OverlayFromId(olid);
  ol.InjectFame(frame_desc[TincanControl::Data].asString());
}

void
Tincan::QueryLinkCas(
  const Json::Value & link_desc,
  Json::Value & cas_info)
{
  const string olid = link_desc[TincanControl::OverlayId].asString();
  const string vlid = link_desc[TincanControl::LinkId].asString();
  Overlay & ol = OverlayFromId(olid);
  ol.QueryLinkCas(vlid, cas_info);
}

void
Tincan::QueryLinkStats(
  const Json::Value & overlay_ids,
  Json::Value & stat_info)
{
  for(uint32_t i = 0; i < overlay_ids["OverlayIds"].size(); i++)
  {
    vector<string>link_ids;
    string olid = overlay_ids["OverlayIds"][i].asString();
    Overlay & ol = OverlayFromId(olid);
    ol.QueryLinkIds(link_ids);
    for(auto vlid : link_ids)
    {
      ol.QueryLinkInfo(vlid, stat_info[olid][vlid]);
    }
  }

}

void
Tincan::QueryOverlayInfo(
  const Json::Value & olay_desc,
  Json::Value & olay_info)
{
  Overlay & ol = OverlayFromId(olay_desc[TincanControl::OverlayId].asString());
  ol.QueryInfo(olay_info);
}

void 
Tincan::RemoveOverlay(
  const Json::Value & olay_desc)
{
  const string olid = olay_desc[TincanControl::OverlayId].asString();
  if(olid.empty())
    throw TCEXCEPT("No overlay id specified");
  
  lock_guard<mutex> lg(ovlays_mutex_);
  for(auto ol = ovlays_.begin(); ol != ovlays_.end(); ol++)
  {
    if((*ol)->Descriptor().uid.compare(olid) == 0)
    {
      (*ol)->Shutdown();
      ovlays_.erase(ol);
      return;
    }
  }
  LOG(LS_WARNING) << "RemoveOverlay: No such virtual network exists " << olid;
}

void
Tincan::RemoveVlink(
  const Json::Value & link_desc)
{
  const string olid = link_desc[TincanControl::OverlayId].asString();
  const string vlid = link_desc[TincanControl::LinkId].asString();
  if(olid.empty() || vlid.empty())
    throw TCEXCEPT("Required identifier not specified");

  lock_guard<mutex> lg(ovlays_mutex_);
  for(auto & ol : ovlays_)
  {
    if(ol->Descriptor().uid.compare(olid) == 0)
    {
      ol->RemoveLink(vlid);
    }
  }
}

void
Tincan::SendIcc(
  const Json::Value & icc_desc)
{
  const string olid = icc_desc[TincanControl::OverlayId].asString();
  const string & link_id = icc_desc[TincanControl::LinkId].asString();
  if(icc_desc[TincanControl::Data].isString())
  {
    const string & data = icc_desc[TincanControl::Data].asString();
    Overlay & ol = OverlayFromId(olid);
    ol.SendIcc(link_id, data);
  }
  else
    throw TCEXCEPT("Icc data is not represented as a string");
}

void
Tincan::SetIgnoredNetworkInterfaces(
  const Json::Value & ignore_list)
{
  int count = ignore_list[TincanControl::IgnoredNetInterfaces].size();
  Json::Value network_ignore_list = 
    ignore_list[TincanControl::IgnoredNetInterfaces];
  vector<string> if_list(count);
  for(int i = 0; i < count; i++)
  {
    if_list[i] = network_ignore_list[i].asString();
  }

  Overlay & oly = OverlayFromId(ignore_list[TincanControl::OverlayId].asString());
  oly.IgnoredNetworkInterfaces(if_list);
}

void
Tincan::OnLocalCasUpdated(
  string link_id,
  string lcas)
{
  if(lcas.empty())
  {
    lcas = "No local candidates available on this vlink";
    LOG(LS_WARNING) << lcas;
  }
  bool to_deliver = false;
  unique_ptr<TincanControl> ctrl;
  {
    std::lock_guard<std::mutex> lg(inprogess_controls_mutex_);
    auto itr = inprogess_controls_.begin();
    for(; itr != inprogess_controls_.end(); itr++)
    {
      if(itr->first == link_id)
      {
        to_deliver = true;
        ctrl = move(itr->second);
        Json::Value & resp = ctrl->GetResponse();
        resp["Message"]["CAS"] = lcas;
        resp["Success"] = true;
        inprogess_controls_.erase(itr);
        break;
      }
    }
  }
  if(to_deliver)
  {
    ctrl_link_->Deliver(move(ctrl));
  }
}

void Tincan::UpdateRouteTable(
  const Json::Value & rts_desc)
{
  string olid = rts_desc[TincanControl::OverlayId].asString();
  Overlay & ol = OverlayFromId(olid);
  ol.UpdateRouteTable(rts_desc["Table"]);
}

void
Tincan::Run()
{
  //TODO:Code cleanup
#if defined(_IPOP_WIN)
  self_ = this;
  SetConsoleCtrlHandler(ControlHandler, TRUE);
#endif // _IPOP_WIN

  //Start tincan control to get config from Controller
  unique_ptr<ControlDispatch> ctrl_dispatch(new ControlDispatch);
  ctrl_dispatch->SetDispatchToTincanInf(this);
  ctrl_listener_ = make_shared<ControlListener>(move(ctrl_dispatch));
  ctl_thread_.Start(ctrl_listener_.get());
  exit_event_.Wait(Event::kForever);
}

bool
Tincan::IsOverlayExisit(
  const string & oid)
{
  lock_guard<mutex> lg(ovlays_mutex_);
  for(auto const & vnet : ovlays_) {
    if(vnet->Descriptor().uid.compare(oid) == 0)
      return true;
  }
  return false;
}

Overlay &
Tincan::OverlayFromId(
  const string & oid)
{
  lock_guard<mutex> lg(ovlays_mutex_);
  for(auto const & vnet : ovlays_)
  {
    //list of vnets will be small enough where a linear search is satifactory
    if(vnet->Descriptor().uid.compare(oid) == 0)
      return *vnet.get();
  }
  string msg("No virtual network exists by this name: ");
  msg.append(oid);
  throw TCEXCEPT(msg.c_str());
}
//-----------------------------------------------------------------------------
void Tincan::OnStop() {
  Shutdown();
  exit_event_.Set();
}

void
Tincan::Shutdown()
{
  lock_guard<mutex> lg(ovlays_mutex_);
  ctl_thread_.Quit();
  for(auto const & vnet : ovlays_) {
    vnet->Shutdown();
  }
}

/*
FUNCTION:ControlHandler
PURPOSE: Handles keyboard signals
PARAMETERS: The signal code
RETURN VALUE: Success/Failure
++*/
#if defined(_IPOP_WIN)
BOOL __stdcall Tincan::ControlHandler(DWORD CtrlType) {
  switch(CtrlType) {
  case CTRL_BREAK_EVENT:  // use Ctrl+C or Ctrl+Break to send
  case CTRL_C_EVENT:      // termination signal
    cout << "Stopping tincan... " << std::endl;
    self_->OnStop();
    return(TRUE);
  }
  return(FALSE);
}
#endif // _IPOP_WIN
} // namespace tincan
