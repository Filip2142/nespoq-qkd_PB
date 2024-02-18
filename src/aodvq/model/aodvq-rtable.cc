/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on
 *      NS-2 AODVQ model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 *
 *      AODVQ-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AODVQ-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */

#include "aodvq-rtable.h"
#include <algorithm>
#include <iomanip>
#include "ns3/simulator.h"
#include "ns3/log.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AodvqRoutingTable");

namespace aodvq {

/*
 The Routing Table
 */

RoutingTableEntry::RoutingTableEntry (Ptr<NetDevice> dev, Ipv4Address dst, bool vSeqNo, uint32_t seqNo,Ipv4InterfaceAddress iface, uint16_t hops, Ipv4Address nextHop, Time lifetime, uint32_t L_summary)
  : m_ackTimer (Timer::CANCEL_ON_DESTROY),
    m_validSeqNo (vSeqNo),
    m_seqNo (seqNo),
    m_hops (hops),
    m_lifeTime (lifetime + Simulator::Now ()),
    m_iface (iface),
    m_flag (VALID),
    m_reqCount (0),
    m_blackListState (false),
    m_blackListTimeout (Simulator::Now ()),
    m_L_summary (L_summary)
{
  m_ipv4Route = Create<Ipv4Route> ();
  m_ipv4Route->SetDestination (dst);
  m_ipv4Route->SetGateway (nextHop);
  m_ipv4Route->SetSource (m_iface.GetLocal ());
  m_ipv4Route->SetOutputDevice (dev);
}

RoutingTableEntry::~RoutingTableEntry ()
{
}

bool
RoutingTableEntry::InsertPrecursor (Ipv4Address id)
{
  NS_LOG_FUNCTION (this << id);
  if (!LookupPrecursor (id))
    {
      m_precursorList.push_back (id);
      return true;
    }
  else
    {
      return false;
    }
}

bool
RoutingTableEntry::LookupPrecursor (Ipv4Address id)
{
  NS_LOG_FUNCTION (this << id);
  for (std::vector<Ipv4Address>::const_iterator i = m_precursorList.begin (); i
       != m_precursorList.end (); ++i)
    {
      if (*i == id)
        {
          NS_LOG_LOGIC ("Precursor " << id << " found");
          return true;
        }
    }
  NS_LOG_LOGIC ("Precursor " << id << " not found");
  return false;
}

bool
RoutingTableEntry::DeletePrecursor (Ipv4Address id)
{
  NS_LOG_FUNCTION (this << id);
  std::vector<Ipv4Address>::iterator i = std::remove (m_precursorList.begin (),
                                                      m_precursorList.end (), id);
  if (i == m_precursorList.end ())
    {
      NS_LOG_LOGIC ("Precursor " << id << " not found");
      return false;
    }
  else
    {
      NS_LOG_LOGIC ("Precursor " << id << " found");
      m_precursorList.erase (i, m_precursorList.end ());
    }
  return true;
}

void
RoutingTableEntry::DeleteAllPrecursors ()
{
  NS_LOG_FUNCTION (this);
  m_precursorList.clear ();
}

bool
RoutingTableEntry::IsPrecursorListEmpty () const
{
  return m_precursorList.empty ();
}

void
RoutingTableEntry::GetPrecursors (std::vector<Ipv4Address> & prec) const
{
  NS_LOG_FUNCTION (this);
  if (IsPrecursorListEmpty ())
    {
      return;
    }
  for (std::vector<Ipv4Address>::const_iterator i = m_precursorList.begin (); i
       != m_precursorList.end (); ++i)
    {
      bool result = true;
      for (std::vector<Ipv4Address>::const_iterator j = prec.begin (); j
           != prec.end (); ++j)
        {
          if (*j == *i)
            {
              result = false;
            }
        }
      if (result)
        {
          prec.push_back (*i);
        }
    }
}

void
RoutingTableEntry::Invalidate (Time badLinkLifetime)
{
  NS_LOG_FUNCTION (this << badLinkLifetime.GetSeconds ());
  if (m_flag == INVALID)
    {
      return;
    }
  m_flag = INVALID;
  m_reqCount = 0;
  m_lifeTime = badLinkLifetime + Simulator::Now ();
}

void
RoutingTableEntry::Print (Ptr<OutputStreamWrapper> stream) const
{
  std::ostream* os = stream->GetStream ();
  *os << m_ipv4Route->GetDestination () << "\t" << m_ipv4Route->GetGateway ()
      << "\t" << m_iface.GetLocal () << "\t";
  switch (m_flag)
    {
    case VALID:
      {
        *os << "UP";
        break;
      }
    case INVALID:
      {
        *os << "DOWN";
        break;
      }
    case IN_SEARCH:
      {
        *os << "IN_SEARCH";
        break;
      }
    }
  *os << "\t";
  *os << std::setiosflags (std::ios::fixed) <<
  std::setiosflags (std::ios::left) << std::setprecision (2) <<
  std::setw (14) << (m_lifeTime - Simulator::Now ()).GetSeconds ();
  *os << "\t" << m_hops << "\t\t" << m_L_summary << "\n";
}

/*
 The Routing Table
 */

RoutingTable::RoutingTable (Time t)
  : m_badLinkLifetime (t)
{
}

bool
RoutingTable::LookupRoute (Ipv4Address id, std::vector<RoutingTableEntry> & rt)
{
  NS_LOG_FUNCTION (this << id);
  Purge ();
  if (m_ipv4AddressEntry.empty ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found; m_ipv4AddressEntry is empty");
      return false;
    }
  std::map<Ipv4Address, std::vector<RoutingTableEntry>>::const_iterator i =
    m_ipv4AddressEntry.find (id);
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  rt = i->second;
  NS_LOG_LOGIC ("Route to " << id << " found");
  return true;
}

bool
RoutingTable::LookupRoute1 (Ipv4Address id, RoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this << id);
  Purge ();
  if (m_ipv4AddressEntry.empty ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found; m_ipv4AddressEntry is empty");
      return false;
    }
  std::map<Ipv4Address, std::vector<RoutingTableEntry>>::iterator i =
    m_ipv4AddressEntry.find (id);
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  std::vector<RoutingTableEntry> routes;
  routes= i->second;
  for (std::vector<RoutingTableEntry>::iterator i = routes.begin(); i != routes.end(); ++i)
  {
    
    rt = (*i);
      
      return true;

    
  }
  return false;
}


bool
RoutingTable::LookupValidRoute (Ipv4Address id, std::vector<RoutingTableEntry> & rt)
{
  NS_LOG_FUNCTION (this << id);
  if (!LookupRoute (id, rt))
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  std::vector<RoutingTableEntry> validrt;
  for (std::vector<RoutingTableEntry>::iterator i = rt.begin(); i != rt.end(); ++i)
    {
      if(i->GetFlag () == VALID){
      NS_LOG_LOGIC ("Route to " << id << " flag is " << ((i->GetFlag () == VALID) ? "valid" : "not valid"));
      validrt.push_back(*i);
      }
    }
  if(!validrt.empty())
  {
      rt = validrt;
      return true;
  }
  return false;
}

bool
RoutingTable::LookupValidRoute1 (Ipv4Address id, RoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this << id);
  std::vector<RoutingTableEntry> rt1;
  if (!LookupRoute (id, rt1))
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  for (std::vector<RoutingTableEntry>::iterator i = rt1.begin(); i != rt1.end(); ++i)
    {
      if(i->GetFlag () == VALID){
      NS_LOG_LOGIC ("Route to " << id << " flag is " << ((i->GetFlag () == VALID) ? "valid" : "not valid"));
      rt = *i;
      return true;
      }
    }
  return false;
  
}





bool
RoutingTable::DeleteRoute (Ipv4Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  Purge ();
  if (m_ipv4AddressEntry.erase (dst) != 0)
    {
      NS_LOG_LOGIC ("Route deletion to " << dst << " successful");
      return true;
    }
  NS_LOG_LOGIC ("Route deletion to " << dst << " not successful");
  return false;
}

bool
RoutingTable::AddRoute (RoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this);
  Purge ();
  if (rt.GetFlag () != IN_SEARCH)
    {
      rt.SetRreqCnt (0);
    }
  /* std::pair<std::map<Ipv4Address, RoutingTableEntry>::iterator, bool> result =
    m_ipv4AddressEntry.insert (std::make_pair (rt.GetDestination (), rt)); */
    //std::pair<std::map<Ipv4Address,std::vector<RoutingTableEntry>>::iterator, bool> result;
    
    std::map<Ipv4Address, std::vector<RoutingTableEntry>>::iterator i = m_ipv4AddressEntry.find(rt.GetDestination());
    std::vector<RoutingTableEntry> routes;
    
    
   if(m_ipv4AddressEntry.empty() || i == m_ipv4AddressEntry.end ())
   {
     routes= {rt};
     //result = m_ipv4AddressEntry.insert (std::make_pair (rt.GetDestination (), routes));
     m_ipv4AddressEntry.insert(std::make_pair(rt.GetDestination(), routes));
     NS_LOG_DEBUG("New entry to routing table destination" << rt.GetDestination ());
     return true;
   }
   else
   {

      routes = i->second; 
      routes.push_back(rt);
      //result = m_ipv4AddressEntry.insert (std::make_pair (rt.GetDestination (), routes));
      m_ipv4AddressEntry.erase(i);
      m_ipv4AddressEntry.insert(std::make_pair(rt.GetDestination(), routes));
      NS_LOG_DEBUG("New entry to routing table" << rt.GetDestination ());
      return true;
   }
   
   

  
  return false;
}

bool
RoutingTable::Update (RoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this);
  std::vector<RoutingTableEntry> routes;
  
  std::map<Ipv4Address, std::vector<RoutingTableEntry>>::iterator i =
    m_ipv4AddressEntry.find (rt.GetDestination ());
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " fails; not found");
      return false;
    }
  routes = i->second;
  if (routes.size() == 1)
    {
      NS_LOG_DEBUG("Clearing of vector");
      routes.clear();
      routes.push_back(rt);
      i->second = routes;
      return true;
    } 


  std::vector<RoutingTableEntry> routes1;
    for (std::vector<RoutingTableEntry>::iterator i = routes.begin(); i != routes.end();++i)
    {
        if (i->GetNextHop() == rt.GetNextHop())
          {
            if (i->GetFlag () != IN_SEARCH)
            {
              NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " set RreqCnt to 0");
              rt.SetRreqCnt(0);
              routes1.push_back(rt);
    
            }
          }
        else
        {
          routes1.push_back(*i);
        }
    }
  
  i->second = routes1;
  return  true;
}

bool
RoutingTable::SetEntryState (Ipv4Address id, RouteFlags state)
{
  NS_LOG_FUNCTION (this);
  
  std::vector<RoutingTableEntry> routes;
  
  std::map<Ipv4Address, std::vector<RoutingTableEntry>>::iterator i =
  m_ipv4AddressEntry.find (id);
  
  
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route set entry state to " << id << " fails; not found");
      return false;
    }
  routes = i->second;
  for (std::vector<RoutingTableEntry>::iterator i = routes.begin(); i != routes.end(); ++i)
  {
    i->SetFlag (state);
    i->SetRreqCnt (0);
    NS_LOG_LOGIC ("Route set entry state to " << id << ": new state is " << state);
  
  }
  return true;
}

void
RoutingTable::GetListOfDestinationWithNextHop (Ipv4Address nextHop, std::map<Ipv4Address, uint32_t> & unreachable )
{
  NS_LOG_FUNCTION (this);
  Purge ();
  unreachable.clear ();

  for (std::map<Ipv4Address, std::vector<RoutingTableEntry>>::const_iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); ++i)
         {
           std::vector<RoutingTableEntry> routes = i->second;
           for (std::vector<RoutingTableEntry>::iterator i = routes.begin(); i != routes.end(); ++i)
           {
             if(i->GetNextHop() == nextHop){
               NS_LOG_LOGIC ("Unreachable insert " << i->GetDestination() << " " << i->GetSeqNo ());
               unreachable.insert (std::make_pair (i->GetDestination(), i->GetSeqNo ()));
             }
           }
         }
  
  
  
  
  
}

void
RoutingTable::InvalidateRoutesWithDst (const std::map<Ipv4Address, uint32_t> & unreachable)
{
  NS_LOG_FUNCTION (this);
  Purge ();
   for (std::map<Ipv4Address, std::vector<RoutingTableEntry>>::const_iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); ++i)
    {
      for (std::map<Ipv4Address, uint32_t>::const_iterator j =
             unreachable.begin (); j != unreachable.end (); ++j)
        {
          if (i->first == j->first){
                std::vector<RoutingTableEntry> routes = i->second;
                for (std::vector<RoutingTableEntry>::iterator k = routes.begin(); k != routes.end(); ++k)
                {
                if (k->GetFlag () == VALID)
                  {
                    NS_LOG_DEBUG("Size of route 3 " << routes.size());
                    NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
                    k->Invalidate (m_badLinkLifetime);
                  }
                }
          }
        }
    }
}

void
RoutingTable::DeleteAllRoutesFromInterface (Ipv4InterfaceAddress iface)
{
  NS_LOG_FUNCTION (this);
  if (m_ipv4AddressEntry.empty ())
    {
      return;
    }
  
  for (std::map<Ipv4Address, std::vector<RoutingTableEntry>>::const_iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); ++i)
    {
        std::vector<RoutingTableEntry> routes = i->second;
        for (std::vector<RoutingTableEntry>::iterator j = routes.begin(); j != routes.end();)
          {
            if (j->GetInterface () == iface)
            {
              std::vector<RoutingTableEntry>::iterator tmp = j;
              ++j;
              routes.erase (tmp);
            }
            else
            {
              ++j;
            } 
    
    
          }
    }
 
 
 
 
 
 /*  for (std::map<Ipv4Address, RoutingTableEntry>::iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); )
    {
      if (i->second.GetInterface () == iface)
        {
          std::map<Ipv4Address, RoutingTableEntry>::iterator tmp = i;
          ++i;
          m_ipv4AddressEntry.erase (tmp);
        }
      else
        {
          ++i;
        }
    } */
}

void
RoutingTable::Purge ()
{
   NS_LOG_FUNCTION (this);
  if (m_ipv4AddressEntry.empty ())
    {
      return;
    }


  for (std::map<Ipv4Address, std::vector<RoutingTableEntry>>::const_iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); ++i)
    {
        std::vector<RoutingTableEntry> routes = i->second;
        for (std::vector<RoutingTableEntry>::iterator j = routes.begin(); j != routes.end();)
          {
            if (j->GetLifeTime () < Seconds (0))
            {
              if (j->GetFlag () == INVALID)
              {
                std::vector<RoutingTableEntry>::iterator tmp = j;
                ++j;
                routes.erase (tmp);
              }
              else if (j->GetFlag () == VALID)
              {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              j->Invalidate (m_badLinkLifetime);
              ++j;
              }
              else
              {
              ++j;
              }
            }   
            else
            {
            ++j;
            }

          }
    }


  /* for (std::map<Ipv4Address, RoutingTableEntry>::iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); )
    {
      if (i->second.GetLifeTime () < Seconds (0))
        {
          if (i->second.GetFlag () == INVALID)
            {
              std::map<Ipv4Address, RoutingTableEntry>::iterator tmp = i;
              ++i;
              m_ipv4AddressEntry.erase (tmp);
            }
          else if (i->second.GetFlag () == VALID)
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              i->second.Invalidate (m_badLinkLifetime);
              ++i;
            }
          else
            {
              ++i;
            }
        }
      else
        {
          ++i;
        }
    } */
}

void
RoutingTable::Purge (std::map<Ipv4Address, std::vector<RoutingTableEntry>> &table) const
{
  NS_LOG_FUNCTION (this);
  if (table.empty ())
    {
      return;
    }
  for (std::map<Ipv4Address, std::vector<RoutingTableEntry>>::iterator i =
         table.begin (); i != table.end (); ++i)
    {
      std::vector<RoutingTableEntry> routes = i->second;
      for (std::vector<RoutingTableEntry>::iterator j = routes.begin(); j != routes.end();)
      {
      if (j->GetLifeTime () < Seconds (0))
        {
          if (j->GetFlag () == INVALID)
            {
              std::vector<RoutingTableEntry>::iterator tmp = j;
              ++j;
              routes.erase (tmp);
            }
          else if (j->GetFlag () == VALID)
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              j->Invalidate (m_badLinkLifetime);
              ++j;
            }
          else
            {
              ++j;
            }
        }
      else
        {
          ++j;
        }
      }
    }
}

bool
RoutingTable::MarkLinkAsUnidirectional (Ipv4Address neighbor, Time blacklistTimeout)
{
  NS_LOG_FUNCTION (this << neighbor << blacklistTimeout.GetSeconds ());
  std::map<Ipv4Address, std::vector<RoutingTableEntry>>::iterator i =
    m_ipv4AddressEntry.find (neighbor);
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Mark link unidirectional to  " << neighbor << " fails; not found");
      return false;
    }
  std::vector<RoutingTableEntry> routes = i->second;
    for (std::vector<RoutingTableEntry>::iterator j = routes.begin(); j != routes.end();)
    {
  
      j->SetUnidirectional (true);
      j->SetBlacklistTimeout (blacklistTimeout);
      j->SetRreqCnt (0);
      NS_LOG_LOGIC ("Set link to " << neighbor << " to unidirectional");
    }
    i->second = routes;
      
      return true;
    
}

void
RoutingTable::Print (Ptr<OutputStreamWrapper> stream) const
{
  std::map<Ipv4Address, std::vector<RoutingTableEntry>> table = m_ipv4AddressEntry;
  Purge (table);
  *stream->GetStream () << "\nAODVQ Routing table\n"
                        << "Destination\tGateway\t\tInterface\tFlag\tExpire\t\tHops\t\tL_route\n";
  for (std::map<Ipv4Address, std::vector<RoutingTableEntry>>::const_iterator i =
        table.begin (); i != table.end (); ++i)
  {
    std::vector<RoutingTableEntry> routes = i->second;
    for (std::vector<RoutingTableEntry>::iterator j = routes.begin(); j != routes.end(); ++j)
    {
      j->Print(stream);
    }
  }
  
  
  
  
  
  
  
  
  /* std::map<Ipv4Address, RoutingTableEntry> table = m_ipv4AddressEntry;
  Purge (table);
  *stream->GetStream () << "\nAODVQ Routing table\n"
                        << "Destination\tGateway\t\tInterface\tFlag\tExpire\t\tHops\tL-summary\n";
  for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i =
         table.begin (); i != table.end (); ++i)
    {
      i->second.Print (stream);
    }
  *stream->GetStream () << "\n"; */
}

}
}
