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
#ifndef TINCAN_VNET_DESCRIPTOR_H_
#define TINCAN_VNET_DESCRIPTOR_H_
#include "tincan_base.h"
#include "turn_descriptor.h"
namespace tincan
{
struct VnetDescriptor
{
  std::string uid;
  std::string name;
  std::string mac;
  std::string vip4;
  uint32_t prefix4;
  uint32_t mtu4;
  std::string description;
  std::string stun_addr;
  std::string turn_addr;
  std::string turn_user;
  std::string turn_pass;
  bool l2tunnel_enabled;
  bool l3tunnel_enabled;
};
struct OverlayDescriptor
{
  std::string uid;
  //string type;
  //string name;
  //string mac;
  //string vlink_id;
  //string description;
  std::vector<std::string> stun_servers;
  std::vector<TurnDescriptor> turn_descs;
  bool enable_ip_mapping;
  bool disable_encryption;
};
} // namespace tincan
#endif // TINCAN_VNET_DESCRIPTOR_H_
