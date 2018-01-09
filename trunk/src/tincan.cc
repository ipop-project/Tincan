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
  ctrl_listener_(nullptr),

  ctrl_link_(nullptr),

  exit_event_(false, false)
{}

Tincan::~Tincan()
{
  delete ctrl_listener_;
}

//void Tincan::UpdateRoute(
//  const string & tap_name,
//  const string & dest_mac,
//  const string & path_mac)
//{
  //VirtualNetwork & vn = VnetFromName(tap_name);
  //MacAddressType mac_dest, mac_path;
  //size_t cnt = StringToByteArray(dest_mac, mac_dest.begin(), mac_dest.end());
  //if(cnt != 6)
  //  throw TCEXCEPT("UpdateRoute failed, destination MAC address was NOT successfully converted.");
  //cnt = StringToByteArray(path_mac, mac_path.begin(), mac_path.end());
  //if(cnt != 6)
  //  throw TCEXCEPT("UpdateRoute failed, path MAC address was NOT successfully converted.");
  //vn.UpdateRoute(mac_dest, mac_path);
//}

//void
//Tincan::ConnectTunnel(
//  const Json::Value & link_desc)
//{
//}

void 
Tincan::SetIpopControllerLink(
  shared_ptr<IpopControllerLink> ctrl_handle)
{
  ctrl_link_ = ctrl_handle;
}

void Tincan::CreateOverlay(
  const Json::Value & olay_desc)
{
  unique_ptr<OverlayDescriptor> ol_desc(new OverlayDescriptor);
  ol_desc->uid = olay_desc["OverlayId"].asString();
  if(GetOverlay(ol_desc->uid))
    throw TCEXCEPT("The specified overlay identifier already exisits");
  ol_desc->stun_addr = olay_desc["StunAddress"].asString();
  ol_desc->turn_addr = olay_desc["TurnAddress"].asString();
  ol_desc->turn_pass = olay_desc["TurnPass"].asString();
  ol_desc->turn_user = olay_desc["TurnUser"].asString();
  ol_desc->enable_ip_mapping = olay_desc["EnableIPMapping"].asBool();
  unique_ptr<Overlay> olay;
  if(olay_desc["Type"].asString() == "VNET")
  {
    olay = make_unique<VirtualNetwork>(move(ol_desc), ctrl_link_);
  }
  else if(olay_desc["Type"].asString() == "TUNNEL")
  {
    olay = make_unique<Tunnel>(move(ol_desc), ctrl_link_);
  }
  else
    throw TCEXCEPT("Invalid Overlay type specified");
  unique_ptr<TapDescriptor> tap_desc = make_unique<TapDescriptor>();
  tap_desc->name = olay_desc["TapName"].asString();
  tap_desc->ip4 = olay_desc["IP4"].asString();
  tap_desc->prefix4 = olay_desc["PrefixLen4"].asUInt();
  tap_desc->mtu4 = olay_desc["MTU4"].asUInt();

  olay->Configure(move(tap_desc));
  olay->Start();
  lock_guard<mutex> lg(ovlays_mutex_);
  ovlays_.push_back(move(olay));

  return;
}

//void
//Tincan::CreateTunnel(
//  const Json::Value & tnl_desc)
//{
//  lock_guard<mutex> lg(ovlays_mutex_);
//  unique_ptr<OverlayDescriptor> ol_desc(new OverlayDescriptor);
//
//  ol_desc->name = tnl_desc["OverlayName"].asString();
//  ol_desc->uid = tnl_desc["OverlayId"].asString();
//  ol_desc->l2tunnel_enabled = tnl_desc["L2TunnelEnabled"].asBool();
//  ol_desc->l3tunnel_enabled = tnl_desc["L3TunnelEnabled"].asBool();
//  ol_desc->stun_addr = tnl_desc["StunAddress"].asString();
//  ol_desc->turn_addr = tnl_desc["TurnAddress"].asString();
//  ol_desc->turn_pass = tnl_desc["TurnPass"].asString();
//  ol_desc->turn_user = tnl_desc["TurnUser"].asString();
//
//  auto tnl = make_unique<Tunnel>(move(ol_desc), ctrl_link_);
//  unique_ptr<TapDescriptor> tap_desc = make_unique<TapDescriptor>();
//  tap_desc->name = tnl_desc[TincanControl::TapName].asString();
//  tap_desc->ip4 = tnl_desc["LocalVirtIP4"].asString();
//  tap_desc->prefix4 = tnl_desc["LocalPrefix4"].asUInt();
//  tap_desc->mtu4 = tnl_desc["MTU4"].asUInt();
//
//  tnl->Configure(move(tap_desc));
//  tnl->Start();
//  ovlays_.push_back(move(tnl));
//}

void
Tincan::CreateVlink(
  const Json::Value & link_desc,
  TincanControl & ctrl)
{
  unique_ptr<PeerDescriptor> peer_desc = make_unique<PeerDescriptor>();
  peer_desc->uid =
    link_desc[TincanControl::PeerInfo][TincanControl::UID].asString();
  peer_desc->vip4 =
    link_desc[TincanControl::PeerInfo][TincanControl::VIP4].asString();
  peer_desc->cas =
    link_desc[TincanControl::PeerInfo][TincanControl::CAS].asString();
  peer_desc->fingerprint =
    link_desc[TincanControl::PeerInfo][TincanControl::Fingerprint].asString();
  peer_desc->mac_address =
    link_desc[TincanControl::PeerInfo][TincanControl::MAC].asString();
  string tap_name = link_desc[TincanControl::TapName].asString();
  unique_ptr<VlinkDescriptor> vl_desc = make_unique<VlinkDescriptor>();
  vl_desc->uid = link_desc["OverlayId"].asString();;
  vl_desc->sec_enabled = link_desc[TincanControl::EncryptionEnabled].asBool();

  Overlay & ol = OverlayFromId(vl_desc->uid);
  shared_ptr<VirtualLink> vlink =
    ol.CreateVlink(move(vl_desc), move(peer_desc));
  //if(vlink->Candidates().empty())
  {
    std::lock_guard<std::mutex> lg(inprogess_controls_mutex_);
    inprogess_controls_.push_back(make_unique<TincanControl>(move(ctrl)));
    vlink->SignalLocalCasReady.connect(this, &Tincan::OnLocalCasUpdated);
  }
}

void
Tincan::InjectFrame(
  const Json::Value & frame_desc)
{
  //const string & tap_name = frame_desc[TincanControl::TapName].asString();
  //VirtualNetwork & vn = VnetFromName(tap_name);
  //vn.InjectFame(frame_desc[TincanControl::Data].asString());
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
  const Json::Value & link_desc,
  Json::Value & link_info)
{
  string olid = link_desc["OverlayId"].asString();
  string vlid = link_desc["LinkId"].asString();
  if(olid.empty() || vlid.empty())
    throw TCEXCEPT("Required paramater missing.");
  Overlay & ol = OverlayFromId(olid);
  ol.QueryLinkInfo(vlid, link_info);
}

void
Tincan::QueryOverlayInfo(
  const Json::Value & olay_desc,
  Json::Value & olay_info)
{
  Overlay & ol = OverlayFromId(olay_desc["OverlayId"].asString());
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
    }
  }
  LOG(LS_WARNING) << "No such virtual network exists " << olid;
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
  //const string & tap_name = icc_desc[TincanControl::TapName].asString();
  //VirtualNetwork & vn = VnetFromName(tap_name);
  //const string & mac = icc_desc[TincanControl::RecipientMac].asString();
  //const string & data = icc_desc[TincanControl::Data].asString();
  //vn.SendIcc(mac, data);
}

void
Tincan::SetIgnoredNetworkInterfaces(
  const Json::Value & olay_desc,
  vector<string>& ignored_list)
{
  Overlay & ol = OverlayFromId(olay_desc["OverlayId"].asString());
  ol.IgnoredNetworkInterfaces(ignored_list);
}

void
Tincan::OnLocalCasUpdated(
  string lcas)
{
  if(lcas.empty())
  {
    lcas = "No local candidates available on this vlink";
    LOG(LS_WARNING) << lcas;
  }
  //this seemingly round-about code is to avoid locking the Deliver() call or
  //setting up the excepton handler necessary for using the mutex directly.
  bool to_deliver = false;
  list<unique_ptr<TincanControl>>::iterator itr;
  unique_ptr<TincanControl> ctrl;
  {
    std::lock_guard<std::mutex> lg(inprogess_controls_mutex_);
    itr = inprogess_controls_.begin();
    for(; itr != inprogess_controls_.end(); itr++)
    {
      if((*itr)->GetCommand() == TincanControl::CreateTunnel.c_str())
      {
        to_deliver = true;
        ctrl = move(*itr);
        inprogess_controls_.erase(itr);
        break;
      }
    }
  }
  if(to_deliver)
  {
    ctrl->SetResponse(lcas, true);
    LOG(LS_INFO) << "Sending updated CAS to Ctlr: " << ctrl->StyledString();
    ctrl_link_->Deliver(move(ctrl));
  }
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
  ctrl_listener_ = new ControlListener(move(ctrl_dispatch));
  ctl_thread_.Start(ctrl_listener_);
  exit_event_.Wait(Event::kForever);
}

//void Tincan::GetLocalNodeInfo(
//  VirtualNetwork & vnet,
//  Json::Value & node_info)
//{
  //OverlayDescriptor & lcfg = vnet.Descriptor();
  //node_info[TincanControl::Type] = "local";
  //node_info[TincanControl::UID] = lcfg.uid;
  ////node_info[TincanControl::VIP4] = lcfg.vip4;
  //node_info[TincanControl::VnetDescription] = lcfg.description;
  //node_info[TincanControl::MAC] = vnet.MacAddress();
  //node_info[TincanControl::Fingerprint] = vnet.Fingerprint();
  //node_info[TincanControl::TapName] = vnet.Name();
//}

Overlay *
Tincan::GetOverlay(
  const string & oid)
{
  lock_guard<mutex> lg(ovlays_mutex_);
  for(auto const & vnet : ovlays_) {
    if(vnet->Descriptor().uid.compare(oid) == 0)
      return vnet.get();
  }
  return nullptr;
}

Overlay &
Tincan::OverlayFromId(
  const string & oid)
{
  lock_guard<mutex> lg(ovlays_mutex_);
  for(auto const & vnet : ovlays_)
  {
    if(vnet->Descriptor().uid.compare(oid) == 0)//list of vnets will be small enough where a linear search is best
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
