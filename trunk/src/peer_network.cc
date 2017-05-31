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
#include "peer_network.h"
#include "tincan_exception.h"
namespace tincan
{
PeerNetwork::PeerNetwork(
  const string & name) :
  name_(name),
  scavenge_interval(120000)
{}

PeerNetwork::~PeerNetwork()
{}
/*
Adds a new adjacent node to the peer network. This is used when a new vlink is
created. The tunnel is assumed to be precreated and with at least one vlink obj.
*/
void PeerNetwork::Add(shared_ptr<Tunnel> tnl)
{
  tnl->is_valid_ = true;
  MacAddressType mac = tnl->Id();
  //size_t cnt = StringToByteArray(
  //  tnl->Vlink()->PeerInfo().mac_address, mac.begin(), mac.end());
  //if(cnt != 6)
  //{
  //  string emsg = "Converting the MAC to binary failed, the input string is: ";
  //  emsg.append(tnl->Vlink()->PeerInfo().mac_address);
  //  throw TCEXCEPT(emsg.c_str());
  //}
  {
    lock_guard<mutex> lg(mac_map_mtx_);
    //if(mac_map_.count(mac) == 1)
    //{
    //  LOG_F(LS_INFO) << "Entry " << tnl->Vlink()->PeerInfo().mac_address <<
    //    " already exists in peer net. It will be updated.";
    //}
    mac_map_[mac] = tnl;
  }
  //LOG(TC_DBG) << "Added node " << tnl->Vlink()->PeerInfo().mac_address;
}

void PeerNetwork::UpdateRoute(
  MacAddressType & dest,
  MacAddressType & route)
{
  lock_guard<mutex> lg(mac_map_mtx_);
  if(dest == route || mac_map_.count(route) == 0 ||
    (mac_map_.count(route) && !mac_map_.at(route)->is_valid_))
  {
    stringstream oss;
    oss << "Attempt to add INVALID route! DEST=" <<
      ByteArrayToString(dest.begin(), dest.end()) << " ROUTE=" <<
      ByteArrayToString(route.begin(), route.end());
    throw TCEXCEPT(oss.str().c_str());
  }
  mac_routes_[dest].tnl = mac_map_.at(route);
  mac_routes_[dest].accessed = steady_clock::now();
  LOG(TC_DBG) << "Updated route to node=" <<
    ByteArrayToString(dest.begin(), dest.end()) << " through node=" << 
    ByteArrayToString(route.begin(), route.end()) << " vlink obj=" << 
    mac_routes_[dest].tnl->Vlink().get();
}
/*
Used when a vlink is removed and the peer is no longer adjacent. All routes that
use this path must be removed as well.
*/
void
PeerNetwork::Remove(
  //const string & peer_uid)
  MacAddressType mac)
{
  try
  {
    lock_guard<mutex> lg(mac_map_mtx_);
    shared_ptr<Tunnel> tnl = mac_map_.at(mac);
    LOG(TC_DBG) << "Removing node " << tnl->Vlink()->PeerInfo().mac_address
      << " tnl use count=" << tnl.use_count() << " vlink obj=" <<
      tnl->Vlink().get();
    tnl->is_valid_ = false;
    mac_map_.erase(mac); //remove the MAC for the adjacent node
    //when tnl goes out of scope ref count is decr, if it is 0 it's deleted 
  }
  catch(exception & e)
  {
    LOG_F(LS_WARNING) << e.what();
  }
  catch(...)
  {
    ostringstream oss;
    oss << "The Peer Network Remove operation failed. MAC " <<
      ByteArrayToString(mac.begin(), mac.end()) <<
      " was not found in peer network: " << name_;
    LOG_F(LS_WARNING) << oss.str().c_str();
  }
}

shared_ptr<Tunnel>
PeerNetwork::GetTunnel(
  const string & mac)
{
  MacAddressType mac_arr;
  StringToByteArray(mac, mac_arr.begin(), mac_arr.end());
  return GetTunnel(mac_arr);
}

shared_ptr<Tunnel>
PeerNetwork::GetTunnel(
  const MacAddressType& mac)
{
  lock_guard<mutex> lgm(mac_map_mtx_);
  return mac_map_.at(mac);
}

//shared_ptr<Tunnel>
//PeerNetwork::GetOrCreateTunnel(
//  const MacAddressType& mac)
//{
//  lock_guard<mutex> lgm(mac_map_mtx_);
//  if(!mac_map_[mac])
//  {
//    mac_map_[mac] = make_shared<Tunnel>();
//  }
//  return mac_map_[mac];
//}

shared_ptr<Tunnel>
PeerNetwork::GetRoute(
  const MacAddressType& mac)
{
  lock_guard<mutex> lgm(mac_map_mtx_);
  HubEx & hux = mac_routes_.at(mac);
  hux.accessed = steady_clock::now();
  return hux.tnl;
}

bool
PeerNetwork::IsAdjacent(
  const string & mac)
{
  MacAddressType mac_arr;
  StringToByteArray(mac, mac_arr.begin(), mac_arr.end());
  return IsAdjacent(mac_arr);
}

bool
PeerNetwork::IsAdjacent(
  const MacAddressType& mac)
{
  lock_guard<mutex> lgm(mac_map_mtx_);
  return (mac_map_.count(mac) == 1);
}

bool
PeerNetwork::IsRouteExists(
  const MacAddressType& mac)
{
  bool rv = false;
  lock_guard<mutex> lgm(mac_map_mtx_);
  if(mac_routes_.count(mac) == 1)
  {
    if(mac_routes_.at(mac).tnl->is_valid_)
      rv = true;
    else
      mac_routes_.erase(mac);
  }
  return rv;
}

void
PeerNetwork::Run(Thread* thread)
{
  steady_clock::time_point accessed;
  while(thread->ProcessMessages((int)scavenge_interval.count()))
  {
    accessed = steady_clock::now();
    list<MacAddressType> ml;
    lock_guard<mutex> lgm(mac_map_mtx_);
    for(auto & i : mac_routes_)
    {
      if(!i.second.tnl->is_valid_)
        ml.push_back(i.first);
      else
      {
        std::chrono::duration<double, milli> elapsed = steady_clock::now() - i.second.accessed;
        if(elapsed > 3 * scavenge_interval)
          ml.push_back(i.first);
      }
    }
    for(auto & mac : ml)
    {
      LOG(TC_DBG) << "Scavenging route to " << ByteArrayToString(mac.begin(), mac.end());
      mac_routes_.erase(mac);
    }
    if(LOG_CHECK_LEVEL(LS_VERBOSE))
    {
      LOG_F(LS_VERBOSE) << "PeerNetwork scavenge took " <<
        (steady_clock::now() - accessed).count() << " nanosecs.";
    }
  }
}
} // namespace tincan
