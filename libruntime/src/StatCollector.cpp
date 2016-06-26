/** StatCollector Implementation -*- C++ -*-
 * @file
 * @section License
 *
 * This file is part of Galois.  Galois is a framework to exploit
 * amorphous data-parallelism in irregular programs.
 *
 * Galois is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Galois is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Galois.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * @section Copyright
 *
 * Copyright (C) 2016, The University of Texas at Austin. All rights
 * reserved.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */

#include "Galois/Runtime/StatCollector.h"

#include <cmath>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <string>
#include <vector>
#include <sstream>

namespace Galois {
namespace Runtime {
extern unsigned activeThreads;
} } //end namespaces

using namespace Galois;
using namespace Galois::Runtime;

const std::string* Galois::Runtime::StatCollector::getSymbol(const std::string& str) const {
  auto ii = symbols.find(str);
  if (ii == symbols.cend())
    return nullptr;
  return &*ii;
}

const std::string* Galois::Runtime::StatCollector::getOrInsertSymbol(const std::string& str) {
  auto ii = symbols.insert(str);
  return &*ii.first;
}

unsigned Galois::Runtime::StatCollector::getInstanceNum(const std::string& str) const {
  auto s = getSymbol(str);
  if (!s)
    return 0;
  auto ii = std::lower_bound(loopInstances.begin(), loopInstances.end(), s, [] (const StringPair<unsigned>& s1, const std::string* s2) { return s1.first < s2; } );
  if (ii == loopInstances.end() || s != ii->first)
    return 0;
  return ii->second;
}

void Galois::Runtime::StatCollector::addInstanceNum(const std::string& str) {
  auto s = getOrInsertSymbol(str);
  auto ii = std::lower_bound(loopInstances.begin(), loopInstances.end(), s, [] (const StringPair<unsigned>& s1, const std::string* s2) { return s1.first < s2; } );
  if (ii == loopInstances.end() || s != ii->first) {
    loopInstances.emplace_back(s, 0);
    std::sort(loopInstances.begin(), loopInstances.end(), [] (const StringPair<unsigned>& s1, const StringPair<unsigned>& s2) { return s1.first < s2.first; } );
  } else {
    ++ii->second;
  }
}

Galois::Runtime::StatCollector::RecordTy::RecordTy(const std::string* loop, const std::string* category, unsigned instance, size_t value) :loop(loop), category(category),instance(instance), mode(0), valueInt(value) {}

Galois::Runtime::StatCollector::RecordTy::RecordTy(const std::string* loop, const std::string* category, unsigned instance, double value) :loop(loop), category(category),instance(instance), mode(1), valueDouble(value) {}

Galois::Runtime::StatCollector::RecordTy::RecordTy(const std::string* loop, const std::string* category, unsigned instance, const std::string& value) :loop(loop), category(category),instance(instance), mode(2), valueStr(value) {}

Galois::Runtime::StatCollector::RecordTy::~RecordTy() {
  using string_type = std::string;
  if (mode == 2)
    valueStr.~string_type();
}

Galois::Runtime::StatCollector::RecordTy::RecordTy(const RecordTy& r) : loop(r.loop), category(r.category), instance(r.instance), mode(r.mode) {
  switch(mode) {
  case 0: valueInt    = r.valueInt;    break;
  case 1: valueDouble = r.valueDouble; break;
  case 2: valueStr    = r.valueStr;    break;
  }
}      

void Galois::Runtime::StatCollector::RecordTy::print(std::ostream& out) const {
  switch(mode) {
  case 0: out << valueInt;    break;
  case 1: out << valueDouble; break;
  case 2: out << valueStr;    break;
  }
}

template<typename T>
void Galois::Runtime::StatCollector::RecordList::insertStat(const std::string* loop, const std::string* category, unsigned instance, const T& val) {
  MAKE_LOCK_GUARD(lock);
  stats.push_back(RecordTy(loop, category, instance, val));
}

void Galois::Runtime::StatCollector::addToStat(const std::string& loop, const std::string& category, size_t value, unsigned TID) {
  if (TID == Substrate::ThreadPool::getTID())
    Stats.getLocal()->insertStat(getOrInsertSymbol(loop), getOrInsertSymbol(category), getInstanceNum(loop), value);
  else
    Stats.getRemote(TID)->insertStat(getOrInsertSymbol(loop), getOrInsertSymbol(category), getInstanceNum(loop), value);

}

void Galois::Runtime::StatCollector::addToStat(const std::string& loop, const std::string& category, double value, unsigned TID) {
  if (TID == Substrate::ThreadPool::getTID())
    Stats.getLocal()->insertStat(getOrInsertSymbol(loop), getOrInsertSymbol(category), getInstanceNum(loop), value);
  else
    Stats.getRemote(TID)->insertStat(getOrInsertSymbol(loop), getOrInsertSymbol(category), getInstanceNum(loop), value);
}

void Galois::Runtime::StatCollector::addToStat(const std::string& loop, const std::string& category, const std::string& value, unsigned TID) {
  if (TID == Substrate::ThreadPool::getTID())
    Stats.getLocal()->insertStat(getOrInsertSymbol(loop), getOrInsertSymbol(category), getInstanceNum(loop), value);
  else
    Stats.getRemote(TID)->insertStat(getOrInsertSymbol(loop), getOrInsertSymbol(category), getInstanceNum(loop), value);
}

uint32_t Galois::Runtime::StatCollector::num_recv_expected;

//assumne called serially
void Galois::Runtime::StatCollector::printStatsForR(std::ostream& out, bool json) {
  if (json)
    out << "[\n";
  else
    out << "HOST,LOOP,INSTANCE,CATEGORY,THREAD,VAL\n";

  auto& net = Galois::Runtime::getSystemNetworkInterface();
  for (unsigned x = 0; x < Stats.size(); ++x) {
    auto rStat = Stats.getRemote(x);
    MAKE_LOCK_GUARD(rStat->lock);
    for (auto& r : rStat->stats) {
      if (json)
        out << "{\"HOST\" : " << net.ID << " , \"LOOP\" : " << *r.loop << " , \"INSTANCE\" : " << r.instance << " , \"CATEGORY\" : " << *r.category << " , \"THREAD\" : " << x << " , \"VALUE\" : ";
      else
        out << net.ID << "," << *r.loop << "," << r.instance << "," << *r.category << "," << x << ",";
      r.print(out);
      out << (json ? "}\n" : "\n");
    }
  }
  if (json)
    out << "]\n";
}

//Assume called serially
//still assumes int values
void Galois::Runtime::StatCollector::printStats(std::ostream& out) {
  std::map<std::tuple<const std::string*, unsigned, const std::string*>, std::vector<size_t> > LKs;

  unsigned maxThreadID = 0;
  //Find all loops and keys
  for (unsigned x = 0; x < Stats.size(); ++x) {
    auto rStat = Stats.getRemote(x);
    std::lock_guard<Substrate::SimpleLock> lg(rStat->lock);
    for (auto& r : rStat->stats) {
      maxThreadID = x;
      auto& v = LKs[std::make_tuple(r.loop, r.instance, r.category)];
      if (v.size() <= x)
        v.resize(x+1);
      v[x] += r.valueInt;
    }
  }

  auto& net = Galois::Runtime::getSystemNetworkInterface();
  //print header
  out << "STATTYPE,LOOP,INSTANCE,CATEGORY,n,sum";
  for (unsigned x = 0; x <= maxThreadID; ++x)
    out << ",T" << x;
  out << "\n";
  //print all values
  for (auto ii = LKs.begin(), ee = LKs.end(); ii != ee; ++ii) {
    std::vector<unsigned long>& Values = ii->second;
    out << "STAT," << net.ID << ","
        << std::get<0>(ii->first)->c_str() << ","
        << std::get<1>(ii->first) << ","
        << std::get<2>(ii->first)->c_str() << ","
        << maxThreadID + 1 <<  ","
        << std::accumulate(Values.begin(), Values.end(), static_cast<unsigned long>(0));
    for (unsigned x = 0; x <= maxThreadID; ++x)
      out << "," <<  (x < Values.size() ? Values.at(x) : 0);
    out << "\n";
  }
}

//Assume called serially
//still assumes int values
void Galois::Runtime::StatCollector::printDistStats_landingPad(Galois::Runtime::RecvBuffer& buf){
  std::string recv_str;
  uint32_t from_ID;
  Galois::Runtime::gDeserialize(buf, from_ID, recv_str);
  Substrate::gPrint(recv_str);
  --num_recv_expected;
}

void Galois::Runtime::StatCollector::printDistStats(std::ostream& out) {
  std::map<std::tuple<const std::string*, unsigned, const std::string*>, std::vector<size_t> > LKs;

  unsigned maxThreadID = 0;
  //Find all loops and keys
  for (unsigned x = 0; x < Stats.size(); ++x) {
    auto rStat = Stats.getRemote(x);
    std::lock_guard<Substrate::SimpleLock> lg(rStat->lock);
    for (auto& r : rStat->stats) {
      maxThreadID = x;
      auto& v = LKs[std::make_tuple(r.loop, r.instance, r.category)];
      if (v.size() <= x)
        v.resize(x+1);
      v[x] += r.valueInt;
    }
  }

  auto& net = Galois::Runtime::getSystemNetworkInterface();
  num_recv_expected = net.Num - 1;
  //print header
  std::stringstream ss;
  ss << "STATTYPE,LOOP,INSTANCE,CATEGORY,n,sum";
  for (unsigned x = 0; x <= maxThreadID; ++x)
    ss << ",T" << x;
  ss << "\n";
  //print all values
  for (auto ii = LKs.begin(), ee = LKs.end(); ii != ee; ++ii) {
    std::vector<unsigned long>& Values = ii->second;
    ss << "STAT," << net.ID << ","
        << std::get<0>(ii->first)->c_str() << ","
        << std::get<1>(ii->first) << ","
        << std::get<2>(ii->first)->c_str() << ","
        << maxThreadID + 1 <<  ","
        << std::accumulate(Values.begin(), Values.end(), static_cast<unsigned long>(0));
    for (unsigned x = 0; x <= maxThreadID; ++x)
      ss << "," <<  (x < Values.size() ? Values.at(x) : 0);
    ss << "\n";
  }

  if(net.ID == 0){
    Substrate::gPrint(ss.str());

    while(num_recv_expected){
      net.handleReceives();
    }
  }
  else{
    //send to host 0 to print.
    Galois::Runtime::SendBuffer b;
    gSerialize(b, net.ID, ss.str());
    net.send(0, printDistStats_landingPad, b);
    net.flush();
  }

  ss.str(std::string());
  ss.clear();
  //out << ss.str();
}


void Galois::Runtime::StatCollector::beginLoopInstance(const std::string& str) {
  addInstanceNum(str);
}


