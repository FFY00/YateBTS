/**
 * SigConnection.cpp
 * This file is part of the Yate-BTS Project http://www.yatebts.com
 *
 * Signaling socket connection
 *
 * Yet Another Telephony Engine - Base Transceiver Station
 * Copyright (C) 2013-2014 Null Team Impex SRL
 * Copyright (C) 2014 Legba, Inc
 *
 * This software is distributed under multiple licenses;
 * see the COPYING file in the main directory for licensing
 * information for this specific distribution.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "SigConnection.h"
#include "ConnectionMap.h"

#include <Logger.h>
#include <Globals.h>
#include <GSML3CommonElements.h>
#include <GSMLogicalChannel.h>
#include <GSML3RRMessages.h>
#include <GSMTransfer.h>
#include <GSMConfig.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

using namespace Connection;
using namespace GSM;

#define PROTO_VER 1
#define HB_MAXTIME 30000
#define HB_TIMEOUT 60000

static void processPaging(L3MobileIdentity& ident, uint8_t type)
{
    switch (type) {
	case ChanTypeVoice:
	    gBTS.pager().addID(ident,GSM::TCHFType);
	    break;
	case ChanTypeSMS:
	case ChanTypeSS:
	    gBTS.pager().addID(ident,GSM::SDCCHType);
	    break;
	default:
	    gBTS.pager().removeID(ident);
    }
}

static void processPaging(const char* ident, uint8_t type)
{
    if (!::strncmp(ident,"TMSI",4)) {
	char* err = 0;
	unsigned int tmsi = (unsigned int)::strtol(ident + 4,&err,16);
	if (err && !*err) {
	    L3MobileIdentity id(tmsi);
	    processPaging(id,type);
	}
	else
	    LOG(ERR) << "received invalid Paging TMSI " << (ident + 4);
    }
    else if (!::strncmp(ident,"IMSI",4)) {
	L3MobileIdentity id(ident + 4);
	processPaging(id,type);
    }
    else
	LOG(ERR) << "received unknown Paging identity " << ident;
}


bool SigConnection::send(BtsPrimitive prim, unsigned char info)
{
    assert((prim & 0x80) != 0);
    if (!valid())
	return false;
    unsigned char buf[2];
    buf[0] = (unsigned char)prim;
    buf[1] = info;
    return send(buf,2);
}

bool SigConnection::send(BtsPrimitive prim, unsigned char info, unsigned int id)
{
    assert((prim & 0x80) == 0);
    if (!valid())
	return false;
    unsigned char buf[4];
    buf[0] = (unsigned char)prim;
    buf[1] = info;
    buf[2] = (unsigned char)(id >> 8);
    buf[3] = (unsigned char)id;
    return send(buf,4);
}

bool SigConnection::send(BtsPrimitive prim, unsigned char info, unsigned int id, const void* data, size_t len)
{
    assert((prim & 0x80) == 0);
    if (!valid())
	return false;
    unsigned char buf[len + 4];
    buf[0] = (unsigned char)prim;
    buf[1] = info;
    buf[2] = (unsigned char)(id >> 8);
    buf[3] = (unsigned char)id;
    ::memcpy(buf + 4,data,len);
    return send(buf,len + 4);
}

bool SigConnection::send(unsigned char sapi, unsigned int id, const GSM::L3Frame* frame)
{
    assert(frame);
    if (!valid())
	return false;
    unsigned int len = frame->length();
    unsigned char buf[len];
    frame->pack(buf);
    return send(SigL3Message,sapi,id,buf,len);
}

bool SigConnection::send(const void* buffer, size_t len)
{
    if (!GenConnection::send(buffer,len))
	return false;
    LOG(DEBUG) << "sent message primitive " << (unsigned int)((const unsigned char*)buffer)[0] << " length " << len;
    mHbSend.future(HB_MAXTIME);
    return true;
}

void SigConnection::process(BtsPrimitive prim, unsigned char info)
{
    switch (prim) {
	case SigHandshake:
	    // TODO
	    break;
	case SigHeartbeat:
	    // TODO
	    break;
	default:
	    LOG(ERR) << "unexpected primitive " << prim;
    }
}

void SigConnection::process(BtsPrimitive prim, unsigned char info, const unsigned char* data, size_t len)
{
    switch (prim) {
	case SigStartPaging:
	    if (len >= 12)
		processPaging((const char*)data,info);
	    else
		LOG(ERR) << "received short Start Paging of length " << len;
	    break;
	case SigStopPaging:
	    if (len >= 12)
		processPaging((const char*)data,0xff);
	    else
		LOG(ERR) << "received short Stop Paging of length " << len;
	    break;
	default:
	    LOG(ERR) << "unexpected primitive " << prim << " with data";
    }
}

void SigConnection::process(BtsPrimitive prim, unsigned char info, unsigned int id)
{
    switch (prim) {
	case SigConnRelease:
	    if (!gConnMap.unmap(id))
		LOG(ERR) << "received SigConnRelease for unmapped id " << id;
	    break;
	case SigStartMedia:
	    {
		LogicalChannel* ch = gConnMap.find(id);
		if (ch) {
		    TCHFACCHLogicalChannel* tch = dynamic_cast<TCHFACCHLogicalChannel*>(ch);
		    if (tch) {
			GSM::L3ChannelMode mode((GSM::L3ChannelMode::Mode)info);
			gConnMap.mapMedia(id,tch);
			ch->send(GSM::L3ChannelModeModify(ch->channelDescription(),mode));
		    }
		    else {
			TCHFACCHLogicalChannel* tch = gConnMap.findMedia(id);
			if (!tch) {
			    LOG(NOTICE) << "received Start Media without traffic channel on id " << id;
			    send(SigMediaError,ErrInterworking,id);
			    break;
			}
			tch->open();
			tch->setPhy(*ch);
			GSM::L3ChannelMode mode((GSM::L3ChannelMode::Mode)info);
			gConnMap.mapMedia(id,tch);
			ch->send(GSM::L3AssignmentCommand(tch->channelDescription(),mode));
		    }
		}
		else
		    LOG(ERR) << "received Start Media for unmapped id " << id;
	    }
	    break;
	case SigStopMedia:
	    {
		TCHFACCHLogicalChannel* tch = gConnMap.findMedia(id);
		if (!tch)
		    break;
		gConnMap.mapMedia(id,0);
		if (static_cast<LogicalChannel*>(tch) != gConnMap.find(id))
		    tch->send(GSM::RELEASE);
	    }
	    break;
	case SigAllocMedia:
	    {
		TCHFACCHLogicalChannel* tch = gConnMap.findMedia(id);
		if (tch)
		    break;
		LogicalChannel* ch = gConnMap.find(id);
		if (!ch) {
		    LOG(ERR) << "received Alloc Media for unmapped id " << id;
		    break;
		}
		if (dynamic_cast<TCHFACCHLogicalChannel*>(ch))
		    break;
		tch = gBTS.getTCH();
		if (tch)
		    gConnMap.mapMedia(id,tch);
		else {
		    LOG(WARNING) << "congestion, no TCH available for assignment";
		    send(SigMediaError,ErrCongestion,id);
		}
	    }
	    break;
	case SigEstablishSAPI:
	    {
		LogicalChannel* ch = gConnMap.find(id);
		if (ch) {
		    unsigned char sapi = info;
		    if (sapi & 0x80) {
			ch = ch->SACCH();
			sapi &= 0x7f;
		    }
		    if (!ch) {
			LOG(ERR) << "received Establish SAPI for missing SACCH of id " << id;
			break;
		    }
		    if ((sapi > 4) || !ch->debugGetL2(sapi)) {
			LOG(ERR) << "received Establish for invalid SAPI " << (unsigned int)info;
			break;
		    }
		    if (ch->multiframeMode(sapi))
			send(prim,info,id);
		    else
			ch->send(GSM::ESTABLISH,sapi);
		}
		else
		    LOG(ERR) << "received Establish SAPI for unmapped id " << id;
	    }
	    break;
	default:
	    LOG(ERR) << "unexpected primitive " << prim << " with id " << id;
    }
}

void SigConnection::process(BtsPrimitive prim, unsigned char info, unsigned int id, const unsigned char* data, size_t len)
{
    switch (prim) {
	case SigL3Message:
	    {
		LogicalChannel* ch = gConnMap.find(id);
		if (ch) {
		    if (info & 0x80) {
			ch = ch->SACCH();
			info &= 0x7f;
		    }
		    if (!ch) {
			LOG(ERR) << "received L3 frame for missing SACCH of id " << id;
			break;
		    }
		    if ((info < 4) && ch->debugGetL2(info)) {
			L3Frame frame((const char*)data,len);
			ch->send(frame,info);
		    }
		    else
			LOG(ERR) << "received L3 frame for invalid SAPI " << (unsigned int)info;
		}
		else
		    LOG(ERR) << "received L3 frame for unmapped id " << id;
	    }
	    break;
	default:
	    LOG(ERR) << "unexpected primitive " << prim << " with id " << id << " and data";
    }
}

void SigConnection::process(const unsigned char* data, size_t len)
{
    if (len < 2) {
	LOG(ERR) << "received short message of length " << len;
	return;
    }
    LOG(DEBUG) << "received message primitive " << (unsigned int)data[0] << " length " << len;
    mHbRecv.future(HB_TIMEOUT);
    if (data[0] & 0x80) {
	len -= 2;
	if (len)
	    process((BtsPrimitive)data[0],data[1],data + 2,len);
	else
	    process((BtsPrimitive)data[0],data[1]);
    }
    else {
	if (len < 4) {
	    LOG(ERR) << "received short message of length " << len << " for primitive " << (unsigned int)data[0];
	    return;
	}
	unsigned int id = (((unsigned int)data[2]) << 8) | data[3];
	len -= 4;
	if (len)
	    process((BtsPrimitive)data[0],data[1],id,data + 4,len);
	else
	    process((BtsPrimitive)data[0],data[1],id);
    }
}

void SigConnection::started()
{
    mHbRecv.future(HB_TIMEOUT);
    mHbSend.future(HB_MAXTIME);
    send(SigHandshake);
}

void SigConnection::idle()
{
    if (mHbRecv.passed()) {
	LOG(ALERT) << "heartbeat timed out!";
	clear();
	// TODO abort
    }
    if (mHbSend.passed() && !send(SigHeartbeat))
	mHbSend.future(HB_MAXTIME);
}

/* vi: set ts=8 sw=4 sts=4 noet: */
