#include "my-channel-manager.h"

#include "ns3/assert.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/tag.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/double.h"
#include "mpdu-aggregator.h"
#include "msdu-aggregator.h"



namespace ns3 {


NS_LOG_COMPONENT_DEFINE ("ChannelBondingManager");

ChannelBondingManager::ChannelBondingManager()
: ch_map(ChannelMapping())
{
	need_rts_cts = false;
	max_width = 160;
	primary_ch = 36;
	request_width = 160;

	alloc_last_primary_hdr = false;

	received_channel[36] = false; received_channel[40] = false; received_channel[44] = false; received_channel[48] = false;
	received_channel[52] = false; received_channel[56] = false; received_channel[60] = false; received_channel[64] = false;

}
ChannelBondingManager::~ChannelBondingManager()
{
	NS_LOG_FUNCTION (this);

}

/* static */
TypeId
ChannelBondingManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ChannelBondingManager")
    .SetParent<Object> ()
    .SetGroupName ("Wifi")
    .AddConstructor<ChannelBondingManager> ()
  ;
  return tid;
}

void ChannelBondingManager::SetMyMac(Ptr<MacLow> mac)
{
	m_mac = mac;
}

std::map<uint16_t, std::pair<uint16_t,uint32_t> >  ChannelBondingManager::ChannelMapping()
{
	 std::map<uint16_t, std::pair<uint16_t,uint32_t> >  result;                            //make channel map
	//20Hz
	 result[36] = std::make_pair(40, 20);                                                 // 36   40   44   48    52    56    60    64
	 result[40] = std::make_pair(36, 20);                                                 //  \  /      \   /       \   /       \  /
	 result[44] = std::make_pair(48, 20);                                                 //   38        46          54          62
	 result[48] = std::make_pair(44, 20);                                                 //       \   /                 \   /
	 result[52] = std::make_pair(56, 20);                                                 //        42                     58
	 result[56] = std::make_pair(52, 20);                                                 //                  \   /
	 result[60] = std::make_pair(64, 20);                                                 //                    50
	 result[64] = std::make_pair(60, 20);

	//40Hz
	 result[38] = std::make_pair(46, 40);
	 result[46] = std::make_pair(38, 40);
	 result[54] = std::make_pair(62, 40);
	 result[62] = std::make_pair(54, 40);

	//80Hz
	 result[42] = std::make_pair(58, 80);
	 result[58] = std::make_pair(42, 80);

	//160Hz
	 result[50] = std::make_pair(50, 160);
	 return result;
}

void ChannelBondingManager::SetChannelOption(uint16_t primary_ch,uint32_t max_width){
	this->max_width = max_width;                        //set max width and primary_ch
	this->primary_ch = primary_ch;
}

void ChannelBondingManager::MakePhys(const WifiPhyHelper &phy, Ptr<WifiPhy> primary, uint16_t ch_num, uint32_t channel_width, enum WifiPhyStandard standard){
   SetChannelOption(ch_num, channel_width);                          //make each phys


   Ptr<NetDevice> device = primary->GetDevice();
   Ptr<Node> node = device->GetNode();


   for(int i =0;i<8;++i){
	   if(ch_numbers[i] == ch_num)
	   {
		   m_phys[ch_num] = primary;
	   }

	   else
	   {
		   m_phys[ch_numbers[i]] = phy.Create (node, device);
		   m_phys[ch_numbers[i]]->ConfigureStandard (standard);
	   }

	   m_phys[ch_numbers[i]]->EnableChannelBonding(true);
	   m_phys[ch_numbers[i]]->SetChannelNumber(ch_numbers[i]);
   }
}

void ChannelBondingManager::ResetPhys(){
	for(int i =0;i<8;++i){                                                                    //reset phys
		m_phys[ch_numbers[i]]->SetReceiveOkCallback (MakeNullCallback<void, Ptr<Packet>, double, WifiTxVector, enum WifiPreamble> ());
		m_phys[ch_numbers[i]]->SetReceiveErrorCallback (MakeNullCallback<void, Ptr<Packet>, double> ());
		m_phys[ch_numbers[i]] = 0;
	}
}

void ChannelBondingManager::ClearReceiveRecord(){
	for(int i =0;i<8;++i){                               //clear received record
		received_channel[ch_numbers[i]] = false;
		last_received_packet[ch_numbers[i]] = 0;
	}

}


uint16_t ChannelBondingManager::CheckChBonding(uint16_t primary)
{
	if (primary == 0)                                      //find suitable bonding channel (idle condition / no consider Rts-Cts width
		return 0;

	std::pair<uint16_t,uint32_t> ch_info;

	uint32_t usable_ch = primary;

	while(true)
	{
		ch_info = ch_map.at(usable_ch);

		if( ch_info.second == max_width)
			break;

		else if( CheckAllSubChannelIdle( ch_info.first ))
		{
			usable_ch = (usable_ch + ch_info.first) / 2;
		}

		else
			break;
	}

	return usable_ch;
}

bool ChannelBondingManager::CheckAllSubChannelIdle(uint16_t ch_num){
	std::pair<uint16_t,uint32_t> ch_info;                     //find all subchannels are idle
	ch_info = ch_map.at(ch_num);

	uint32_t ch_width = ch_info.second;
	uint16_t side_num = ch_width / 20;

	if(ch_width == 20){
		if(m_phys[ch_num]->IsStateIdle ())
			return true;

		else
			return false;
	}

	else
	{
		if(CheckAllSubChannelIdle(ch_num - side_num) && CheckAllSubChannelIdle (ch_num + side_num) )
			return true;

		else
			return false;
	}
}

uint16_t ChannelBondingManager::GetUsableBondingChannel(uint16_t primary)                //get bonding channel in rts-cts environment
{
	std::pair<uint16_t,uint32_t> ch_info;

	ch_info = ch_map.at(primary);


	uint16_t usable_ch = primary;

	if(!need_rts_cts)
	{
		CheckChannelBeforeSend();
		return request_ch;
	}

	if(!CheckAllSubChannelReceived(primary))
		return 0;

	while(true)
	{
		ch_info = ch_map.at(usable_ch);
		if( ch_info.second >= max_width)
			break;

		else if( CheckAllSubChannelReceived( ch_info.first ))
		{
			usable_ch = (usable_ch + ch_info.first) / 2;
		}

		else
			break;
	}

	return usable_ch;
}

bool ChannelBondingManager::CheckAllSubChannelReceived(uint16_t ch_num)                   //check all sub channel receive rts-cts
{
	std::pair<uint16_t,uint32_t> ch_info;
	ch_info = ch_map.at(ch_num);

	uint32_t ch_width = ch_info.second;
	uint16_t side_num = ch_width / 20;

	if(ch_width == 20){
		if(received_channel[ch_num])
		{
			WifiMacHeader hdr1,hdr2;
			if(received_channel[primary_ch])
			{
				last_received_packet[ch_num]->PeekHeader(hdr1);
				last_received_packet[primary_ch]->PeekHeader(hdr2);
				if(hdr1.GetAddr1() == hdr2.GetAddr1() &&
					hdr1.GetType() == hdr2.GetType()
				   )
					return true;

				else
					return false;
			}
			else
				return false;
		}
		else
			return false;
	}

	else
	{
		if(CheckAllSubChannelReceived(ch_num - side_num) && CheckAllSubChannelReceived (ch_num + side_num) )
			return true;

		else
			return false;
	}

}
uint16_t ChannelBondingManager::GetPrimaryCh()
{
	return primary_ch;
}
uint32_t ChannelBondingManager::GetMaxWidth()
{
	return max_width;
}

uint32_t ChannelBondingManager::GetRequestWidth()
{
	return request_width;
}

void ChannelBondingManager::SetPhysCallback()
{

	for(int i =0;i<8;++i){
		m_phys[ch_numbers[i]]->SetReceiveErrorCallback (MakeCallback (&ChannelBondingManager::ReceiveError, this));
	}

	m_phys[ch_numbers[0]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive36Channel, this));
	m_phys[ch_numbers[1]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive40Channel, this));
	m_phys[ch_numbers[2]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive44Channel, this));
	m_phys[ch_numbers[3]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive48Channel, this));
	m_phys[ch_numbers[4]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive52Channel, this));
	m_phys[ch_numbers[5]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive56Channel, this));
	m_phys[ch_numbers[6]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive60Channel, this));
	m_phys[ch_numbers[7]]->SetReceiveOkCallback (MakeCallback (&ChannelBondingManager::Receive64Channel, this));
}

void ChannelBondingManager::Receive36Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,36);
}

void ChannelBondingManager::Receive40Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,40);
}

void ChannelBondingManager::Receive44Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,44);
}

void ChannelBondingManager::Receive48Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,48);
}

void ChannelBondingManager::Receive52Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,52);
}

void ChannelBondingManager::Receive56Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,56);
}

void ChannelBondingManager::Receive60Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,60);
}

void ChannelBondingManager::Receive64Channel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	ReceiveSubChannel(Packet,rxSnr,txVector,preamble,64);
}

void ChannelBondingManager::ReceiveSubChannel (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble, uint16_t ch_num)
{
	bool isampdu = false;                              //manage 1 subchannel receive packet
	WifiMacHeader hdr;
	AmpduTag ampdu;
	AmpduSubframeHeader ampduhdr;

	if(Packet->PeekPacketTag(ampdu))
	{
		isampdu = true;
		Packet->RemoveHeader(ampduhdr);
		Packet->PeekHeader(hdr);
		Packet->AddHeader(ampduhdr);
		alloc_last_primary_hdr = false;

		if(ch_num == primary_ch){
			alloc_last_primary_hdr = true;

			last_primary_hdr = hdr;
		}
	}

	else
	{
		isampdu = false;
		Packet->PeekHeader(hdr);

		if(ch_num == primary_ch){
			alloc_last_primary_hdr = true;

			last_primary_hdr = hdr;
		}
	}

	received_channel[ch_num] = true;
	last_received_packet[ch_num] = Packet;


	//start timer
	if(ch_num == primary_ch)
	{
		if(clean_timer.IsRunning())
			clean_timer.Cancel();

		clean_timer = Simulator::Schedule(m_mac->GetSifs(),
												&ChannelBondingManager::ClearReceiveRecord, this
												);
	}

	if(isampdu)
	{
		//need_rts_cts = false;
		if(!need_rts_cts)
		{
			request_width = txVector.GetChannelWidth();
			request_ch = GetChannelWithWidth(request_width);
		}

		if(GetNumberOfReceive() == (request_width/20) ){
			//std::cout<<"ho~\n";
			ManageReceived(Packet, rxSnr, txVector, preamble);
		}

	}

	else
	{
       if(hdr.IsRts() || hdr.IsCts() )
		{
			if(primary_ch == ch_num ||
				hdr.GetAddr1() == m_mac->GetAddress()
				)
			{
				NeedRtsCts(true);
			}

			request_ch = GetUsableBondingChannel(primary_ch);

			if(request_ch == 0)
				request_width = 0;

			else
				request_width = ch_map.at(request_ch).second;


			if (primary_ch == ch_num)  //received primary
			{
				ManageReceived(Packet, rxSnr, txVector, preamble);
			}
		}

		else
		{
			if(ch_num == primary_ch){
				ManageReceived(Packet, rxSnr, txVector, preamble);
			}
		}
	}
}



void ChannelBondingManager::ManageReceived (Ptr<Packet> Packet, double rxSnr, WifiTxVector txVector, WifiPreamble preamble)
{
	bool isampdu = false;                   //marge receive packet and forwardup to mac-low
	AmpduTag ampdu;
	WifiMacHeader hdr;
	AmpduSubframeHeader ampduhdr;


	if(Packet->PeekPacketTag(ampdu))
	{
		Packet->PeekHeader(ampduhdr);
		isampdu = true;
	}
	else
	{
		Packet->PeekHeader(hdr);
		isampdu = false;
	}

	Ptr<ns3::Packet> p;


	/*if(isampdu)
	{
		for(int i=0;i<8;i++)
		{
			if(received_channel[ch_numbers[i]])
			{
				temp_p = last_received_packet[ch_numbers[i]]->Copy();

				if(ch_numbers[i] == primary_ch)
				{
					temp_p->RemoveHeader(ampduhdr);
					temp_p->RemoveHeader(hdr);
				}
				else
				{
					temp_p->RemoveHeader(etc_ampdu);
					temp_p->RemoveHeader(etc);
				}

				if(p == 0)
					p = temp_p;

				else
					p->AddAtEnd(temp_p);
			}
		}
		p->AddHeader(hdr);
		p->AddHeader(ampduhdr);
	}

	else*/
	//{
		p = Packet->Copy();
	//}

	if(isampdu || (!hdr.IsRts() && !hdr.IsCts()))
		ClearReceiveRecord();

	m_mac->DeaggregateAmpduAndReceive(p, rxSnr,txVector, preamble);
}

void ChannelBondingManager::SendPacket (Ptr<const Packet> packet, WifiTxVector txVector, enum WifiPreamble preamble, enum mpduType mpdutype)
{                                               //send packet
	ConvertPacket(packet);                       //split packet

	ClearReceiveRecord();


	//txVector.SetChannelWidth(request_width);

	for(int i=0;i<8;i++)
	{
		if(packet_pieces[ch_numbers[i]] != 0)
		{
			m_phys[ch_numbers[i]]->SendPacket(packet_pieces[ch_numbers[i]], txVector, preamble, mpdutype);
		}
	}


	CleanPacketPieces();

}

void ChannelBondingManager::SendPacket (Ptr<const Packet> packet, WifiTxVector txVector, enum WifiPreamble preamble)
{
	SendPacket (packet, txVector, preamble, NORMAL_MPDU);
}


void ChannelBondingManager::ReceiveError(ns3::Ptr<ns3::Packet> packet, double rxSnr)
{
	m_mac->ReceiveError(packet, rxSnr);
}

void ChannelBondingManager::CheckChannelBeforeSend()   //set bonding channel in no rts-cts environment
{
	request_ch = CheckChBonding(primary_ch);
	request_width = ch_map.at(request_ch).second;
}

Ptr<Packet> ChannelBondingManager::ConvertPacket(Ptr<const Packet> packet)   //split the packet
{
	CleanPacketPieces();

	/*bool isampdu = false;
	AmpduTag ampdu;
	WifiMacHeader hdr;
	AmpduSubframeHeader ampduhdr;
	Ptr<Packet> origin_p = packet->Copy();

	if(origin_p->PeekPacketTag(ampdu))
	{
		isampdu = true;
		origin_p->RemoveHeader(ampduhdr);
		origin_p->RemoveHeader(hdr);
	}

	else
	{
		isampdu = false;
		origin_p->PeekHeader(hdr);
	}


	if(request_ch == 0){  //이상
		if(!need_rts_cts)
			CheckChannelBeforeSend();
		else
			return packet->Copy();
	}


	std::vector<uint16_t> sub_chs = FindSubChannels(request_ch);

	int using_channel = request_width / 20;

	if(isampdu)
	{
		Ptr<Packet> p;

		int size_p = origin_p->GetSize();

		int point = 0;
		int unit = size_p / using_channel;


		for(std::vector<uint16_t>::iterator i = sub_chs.begin() ;
			i != sub_chs.end() ;
			++i )
		{

			if((size_p - point)%using_channel ==0 )
			{
				p = origin_p->CreateFragment(point, unit);
				point += unit;
				--using_channel;
			}

			else
			{
				p = origin_p->CreateFragment(point,unit+1);
				point += (1+unit);
				--using_channel;
			}
			p->AddHeader(hdr);
			p->AddHeader(ampduhdr);
			packet_pieces[*i] = p;
		}
	}

	else*/
	{
		Ptr<Packet> origin_p = packet->Copy();
		std::vector<uint16_t> sub_chs = FindSubChannels(request_ch);
		for(std::vector<uint16_t>::iterator i = sub_chs.begin() ;
			i != sub_chs.end() ;
			++i )
		{
			packet_pieces[*i] = origin_p;
		}
	}


	return packet_pieces[primary_ch];
}

void ChannelBondingManager::CleanPacketPieces()           //clear the storage for splited packet
{
	for(std::map< uint16_t, Ptr<Packet> >::iterator i = packet_pieces.begin();
		i != packet_pieces.end();
		++i)
	{
		i->second = 0;
	}
}

std::vector<uint16_t> ChannelBondingManager::FindSubChannels(uint16_t ch_num)    //find all subchannel composing the bonding channel
{
	ChannelInfo ch_info = ch_map.at(ch_num);
	std::vector<uint16_t> result,temp;

	if(ch_info.second == 20)
	{
		result.push_back(ch_num);
	}

	else
	{
		uint16_t side_num = ch_info.second / 20;
		temp = FindSubChannels(ch_num - side_num);
		result.reserve(result.size() + temp.size());
		result.insert(result.end(),temp.begin(),temp.end());

		temp = FindSubChannels(ch_num + side_num);
		result.reserve(result.size() + temp.size());
		result.insert(result.end(),temp.begin(),temp.end());
	}

	return result;
}

uint16_t ChannelBondingManager::GetChannelWithWidth(uint32_t width)      //find bonding channel number using width
{
	uint16_t result = primary_ch;
	ChannelInfo ch_info = ch_map.at(result);

	if(ch_info.second == 0)
	{
		//error
		return 0;
	}
	while(true)
	{
		ch_info = ch_map.at(result);

		if(ch_info.second == width)
			return result;

		else
			result = (result + ch_info.first)/2;
	}
}

void ChannelBondingManager::NeedRtsCts(bool need){   //set using rts-cts
	need_rts_cts = need;
}

uint8_t ChannelBondingManager::GetNumberOfReceive()       //find number of received packet in each subchannel
{
	uint8_t result = 0;

	if(!received_channel[primary_ch])
		return result;

	else
	{
		Ptr<Packet> p;
		p = last_received_packet[primary_ch]->Copy();
		AmpduTag ampdu;
		WifiMacHeader hdr;
		AmpduSubframeHeader ampduhdr;
		bool primary_mpdu_tf, mpdu_tf;
		Mac48Address primary_sender_addr;
		WifiMacType primary_hdr_type;

		if(p->PeekPacketTag(ampdu))
		{
			p->RemoveHeader(ampduhdr);
			p->RemoveHeader(hdr);
			primary_mpdu_tf = true;
		}

		else
		{
			p->RemoveHeader(hdr);
			primary_mpdu_tf = false;
		}

		p = 0;
		primary_sender_addr = hdr.GetAddr2();
		primary_hdr_type = hdr.GetType();

		for(int i=0;i<8;i++)
		{
			if(received_channel[ch_numbers[i]])
			{
				p = last_received_packet[ch_numbers[i]]->Copy();
				if(p->PeekPacketTag(ampdu))
				{
					p->RemoveHeader(ampduhdr);
					p->RemoveHeader(hdr);
					mpdu_tf = true;
				}

				else
				{
					p->RemoveHeader(hdr);
					mpdu_tf = false;
				}

				p = 0;

				if(primary_mpdu_tf == mpdu_tf)
				{
					if(primary_sender_addr == hdr.GetAddr2() &&
							primary_hdr_type == hdr.GetType())
					{
						++result;
					}

					else
					{
						received_channel[ch_numbers[i]] = false;
						last_received_packet[ch_numbers[i]] = 0;
					}
				}
				else
				{
					received_channel[ch_numbers[i]] = false;
					last_received_packet[ch_numbers[i]] = 0;
				}
			}
		}
		return result;
	}
}

uint32_t ChannelBondingManager::GetConvertedSize(Ptr<const Packet> packet)    //get converted packet size
{


  WifiMacHeader hdr;
  AmpduSubframeHeader ampduhdr;
  Ptr<Packet> origin_p = packet->Copy();


  origin_p->RemoveHeader(ampduhdr);
  origin_p->RemoveHeader(hdr);



  int using_channel = request_width / 20;


  Ptr<Packet> p;

  int size_p = origin_p->GetSize();
  int unit = size_p / using_channel;


  if(size_p % using_channel ==0 )
  {
    p = origin_p->CreateFragment(0, unit);
  }

  else
  {
    p = origin_p->CreateFragment(0,unit+1);
  }
  p->AddHeader(hdr);
  p->AddHeader(ampduhdr);

  return p->GetSize();

}


}
