/*
 * This code is (mostly) a copy of the fine bsdiff / bspatch
 * utilities, so the original license follows.
 */

/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "binary_data.hxx"
#include <algorithm>
#include <stdexcept>
#include <boost/format.hpp>
#include <sys/types.h>
#include <bzlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;
using namespace Archive::Backend;

namespace {

void split(off_t *I,off_t *V,off_t start,off_t len,off_t h)
{
	off_t i,j,k,x,tmp,jj,kk;

	if(len<16) {
		for(k=start;k<start+len;k+=j) {
			j=1;x=V[I[k]+h];
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {
					x=V[I[k+i]+h];
					j=0;
				};
				if(V[I[k+i]+h]==x) {
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
					j++;
				};
			};
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			if(j==1) I[k]=-1;
		};
		return;
	};

	x=V[I[start+len/2]+h];
	jj=0;kk=0;
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;kk+=jj;

	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	if(jj>start) split(I,V,start,jj-start,h);

	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	if(jj==kk-1) I[jj]=-1;

	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

void qsufsort(off_t *I,off_t *V,const u_char *old,off_t oldsize)
{
	off_t buckets[256];
	off_t i,h,len;

	for(i=0;i<256;i++) buckets[i]=0;
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	I[0]=oldsize;
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				len-=I[i];
				i-=I[i];
			} else {
				if(len) I[i-len]=-len;
				len=V[I[i]]+1-i;
				split(I,V,i,len,h);
				i+=len;
				len=0;
			};
		};
		if(len) I[i-len]=-len;
	};

	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

off_t matchlen(const u_char *olds,off_t oldsize,const u_char *news,off_t newsize)
{
	off_t i;

	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(olds[i]!=news[i]) break;

	return i;
}

off_t search(off_t *I,const u_char *olds,off_t oldsize,
		const u_char *news,off_t newsize,off_t st,off_t en,off_t *pos)
{
	off_t x,y;

	if(en-st<2) {
		x=matchlen(olds+I[st],oldsize-I[st],news,newsize);
		y=matchlen(olds+I[en],oldsize-I[en],news,newsize);

		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	x=st+(en-st)/2;
	if(memcmp(olds+I[x],news,min(oldsize-I[x],newsize))<0) {
		return search(I,olds,oldsize,news,newsize,x,en,pos);
	} else {
		return search(I,olds,oldsize,news,newsize,st,x,pos);
	};
}

void offtout(off_t x,u_char *buf)
{
	off_t y;

	if(x<0) y=-x; else y=x;

		buf[0]=y%256;y-=buf[0];
	y=y/256;buf[1]=y%256;y-=buf[1];
	y=y/256;buf[2]=y%256;y-=buf[2];
	y=y/256;buf[3]=y%256;y-=buf[3];
	y=y/256;buf[4]=y%256;y-=buf[4];
	y=y/256;buf[5]=y%256;y-=buf[5];
	y=y/256;buf[6]=y%256;y-=buf[6];
	y=y/256;buf[7]=y%256;

	if(x<0) buf[7]|=0x80;
}

off_t offtin(u_char *buf)
{
	off_t y;

	y=buf[7]&0x7F;
	y=y*256;y+=buf[6];
	y=y*256;y+=buf[5];
	y=y*256;y+=buf[4];
	y=y*256;y+=buf[3];
	y=y*256;y+=buf[2];
	y=y*256;y+=buf[1];
	y=y*256;y+=buf[0];

	if(buf[7]&0x80) y=-y;

	return y;
}

inline int CheckAndThrow(int op, int accept1, int accept2, const char* file, int line)
{
    if (op != accept1 && op != accept2) throw runtime_error((boost::format("bzip: %1% in %2% at %3%") % op % file % line).str());
    return op;
}

#define CHECK_BZ(op, accept) if (op != accept) throw runtime_error((boost::format("bzip: %1% in %2% at %3%") % op % __FILE__ % __LINE__).str());
#define CHECKV_BZ(op, accept1, accept2) CheckAndThrow(op, accept1, accept2, __FILE__, __LINE__)

} // anonymous namespace

vector<unsigned char> BinaryData::CreatePatch(const vector<unsigned char>& oldData, const vector<unsigned char>& newData)
{
	const u_char *olds;
    const u_char *news;
	off_t oldsize,newsize;
	off_t *I,*V;
	off_t scan,pos,len;
	off_t lastscan,lastpos,lastoffset;
	off_t oldscore,scsc;
	off_t s,Sf,lenf,Sb,lenb;
	off_t overlap,Ss,lens;
	off_t i;
	off_t dblen,eblen;
	u_char *db,*eb;
	u_char buf[8];
	u_char header[32];
    
    olds = &oldData[0];
    oldsize = oldData.size();
    news = &newData[0];
    newsize = newData.size();

    vector<unsigned char> patch(sizeof(header));
    
	if(((I=(off_t*)malloc((oldsize+1)*sizeof(off_t)))==NULL) ||
		((V=(off_t*)malloc((oldsize+1)*sizeof(off_t)))==NULL)) return patch;

	qsufsort(I,V,olds,oldsize);

	free(V);

	if(((db=(u_char*)malloc(newsize+1))==NULL) ||
		((eb=(u_char*)malloc(newsize+1))==NULL)) return patch;
	
    dblen=0;
	eblen=0;

	/* Create the patch */
    
    bz_stream ctrl;
    memset(&ctrl, 0, sizeof(bz_stream));
    u_char output[512];
    
	/* Header is
		0	8	 "BSDIFF40"
		8	8	length of bzip2ed ctrl block
		16	8	length of bzip2ed diff block
		24	8	length of new file */
	/* File is
		0	32	Header
		32	??	Bzip2ed ctrl block
		??	??	Bzip2ed diff block
		??	??	Bzip2ed extra block */
    
    memcpy(header,"BSDIFF40",8);
	offtout(0, header + 8);
	offtout(0, header + 16);
	offtout(newsize, header + 24);
    
	/* Compute the differences, writing ctrl as we go */
    CHECK_BZ(BZ2_bzCompressInit(&ctrl, 9, 0, 30), BZ_OK);
    ctrl.next_out = (char*)output;
    ctrl.avail_out = 512;
	scan=0;len=0;
	lastscan=0;lastpos=0;lastoffset=0;
	while(scan<newsize) {
		oldscore=0;

		for(scsc=scan+=len;scan<newsize;scan++) {
			len=search(I,olds,oldsize,news+scan,newsize-scan,
					0,oldsize,&pos);

			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<oldsize) &&
				(olds[scsc+lastoffset] == news[scsc]))
				oldscore++;

			if(((len==oldscore) && (len!=0)) || 
				(len>oldscore+8)) break;

			if((scan+lastoffset<oldsize) &&
				(olds[scan+lastoffset] == news[scan]))
				oldscore--;
		};

		if((len!=oldscore) || (scan==newsize)) {
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<oldsize);) {
				if(olds[lastpos+i]==news[lastscan+i]) s++;
				i++;
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			lenb=0;
			if(scan<newsize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(olds[pos-i]==news[scan-i]) s++;
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);
				s=0;Ss=0;lens=0;
				for(i=0;i<overlap;i++) {
					if(news[lastscan+lenf-overlap+i]==
					   olds[lastpos+lenf-overlap+i]) s++;
					if(news[scan-lenb+i]==
					   olds[pos-lenb+i]) s--;
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				lenf+=lens-overlap;
				lenb-=lens;
			};

			for(i=0;i<lenf;i++)
				db[dblen+i]=news[lastscan+i]-olds[lastpos+i];
			for(i=0;i<(scan-lenb)-(lastscan+lenf);i++)
				eb[eblen+i]=news[lastscan+lenf+i];

			dblen+=lenf;
			eblen+=(scan-lenb)-(lastscan+lenf);

			offtout(lenf,buf);
            ctrl.next_in = (char*)buf;
            ctrl.avail_in = 8;
            CHECK_BZ(BZ2_bzCompress(&ctrl, BZ_RUN), BZ_RUN_OK);

			offtout((scan-lenb)-(lastscan+lenf),buf);
            ctrl.next_in = (char*)buf;
            ctrl.avail_in = 8;
            CHECK_BZ(BZ2_bzCompress(&ctrl, BZ_RUN), BZ_RUN_OK);

			offtout((pos-lenb)-(lastpos+lenf),buf);
            ctrl.next_in = (char*)buf;
            ctrl.avail_in = 8;
            CHECK_BZ(BZ2_bzCompress(&ctrl, BZ_RUN), BZ_RUN_OK);

			lastscan=scan-lenb;
			lastpos=pos-lenb;
			lastoffset=pos-scan;
            
            if (ctrl.avail_out) {
                for (unsigned index = 0; index < 512 - ctrl.avail_out; ++index) patch.push_back(output[index]);
                ctrl.next_out = (char*)output;
                ctrl.avail_out = 512;
            }
		};
	};
    
    while (CHECKV_BZ(BZ2_bzCompress(&ctrl, BZ_FINISH), BZ_FINISH_OK, BZ_STREAM_END) != BZ_STREAM_END) {
        ctrl.next_out = (char*)output;
        ctrl.avail_out = 512;
    }
	for (unsigned index = 0; index < 512 - ctrl.avail_out; ++index) patch.push_back(output[index]);

	offtout(ctrl.total_out_lo32, header + 8);
	CHECK_BZ(BZ2_bzCompressEnd(&ctrl), BZ_OK);

	/* Write compressed diff data */
    CHECK_BZ(BZ2_bzCompressInit(&ctrl, 9, 0, 30), BZ_OK);
    ctrl.next_out = (char*)output;
    ctrl.avail_out = 512;
    
    ctrl.next_in = (char*)db;
    ctrl.avail_in = dblen;
	
    CHECK_BZ(BZ2_bzCompress(&ctrl, BZ_RUN), BZ_RUN_OK);
    while (CHECKV_BZ(BZ2_bzCompress(&ctrl, BZ_FINISH), BZ_FINISH_OK, BZ_STREAM_END) != BZ_STREAM_END) {
        ctrl.next_out = (char*)output;
        ctrl.avail_out = 512;
    }
	for (unsigned index = 0; index < 512 - ctrl.avail_out; ++index) patch.push_back(output[index]);

	offtout(ctrl.total_out_lo32, header + 16);
	CHECK_BZ(BZ2_bzCompressEnd(&ctrl), BZ_OK);

	/* Write compressed extra data */
    CHECK_BZ(BZ2_bzCompressInit(&ctrl, 9, 0, 30), BZ_OK);
    ctrl.next_out = (char*)output;
    ctrl.avail_out = 512;
    
    ctrl.next_in = (char*)eb;
    ctrl.avail_in = eblen;
	
    CHECK_BZ(BZ2_bzCompress(&ctrl, BZ_RUN), BZ_RUN_OK);
    while (CHECKV_BZ(BZ2_bzCompress(&ctrl, BZ_FINISH), BZ_FINISH_OK, BZ_STREAM_END) != BZ_STREAM_END) {
        ctrl.next_out = (char*)output;
        ctrl.avail_out = 512;
    }
	for (unsigned index = 0; index < 512 - ctrl.avail_out; ++index) patch.push_back(output[index]);
	
	CHECK_BZ(BZ2_bzCompressEnd(&ctrl), BZ_OK);

    memcpy(&patch[0], header, 32);
	free(db);
	free(eb);
	free(I);

	return patch;
}

vector<unsigned char> BinaryData::ApplyPatch(const vector<unsigned char>& data, const vector<unsigned char>& patch)
{
	signed oldsize,newsize;
	signed bzctrllen,bzdatalen;
	u_char header[32],buf[8];
	const u_char *olds;
    u_char *news;
	off_t oldpos,newpos;
	off_t ctrl[3];
	off_t i;
    
    if (patch.size() <= sizeof(header)) throw runtime_error("invalid patch");
    olds = &data[0];
    oldsize = data.size();
    
    memcpy(header, &patch[0], sizeof(header));
	if (memcmp(header, "BSDIFF40", 8) != 0) throw runtime_error("invalid patch");
	
    bzctrllen=offtin(header+8);
	bzdatalen=offtin(header+16);
	newsize=offtin(header+24);
	if((bzctrllen<0) || (bzdatalen<0) || (newsize<0)) throw runtime_error("invalid patch");
    
    vector<unsigned char> result(newsize);
    
    bz_stream ctrlz;
    bz_stream dataz;
    bz_stream extra;
    
    memset(&ctrlz, 0, sizeof(bz_stream));
    memset(&dataz, 0, sizeof(bz_stream));
    memset(&extra, 0, sizeof(bz_stream));
    
    CHECK_BZ(BZ2_bzDecompressInit(&ctrlz, 0, 0), BZ_OK);
    CHECK_BZ(BZ2_bzDecompressInit(&dataz, 0, 0), BZ_OK);
    CHECK_BZ(BZ2_bzDecompressInit(&extra, 0, 0), BZ_OK);
    
	news=(u_char*)&result[0];
    
    ctrlz.avail_in = bzctrllen;
    ctrlz.next_in = (char*)&patch[32];
    
    dataz.avail_in = bzdatalen;
    dataz.next_in = (char*)&patch[32 + bzctrllen];
    
    extra.avail_in = patch.size() - (32 + bzctrllen + bzdatalen);
    extra.next_in = (char*)&patch[32 + bzctrllen + bzdatalen];

	oldpos=0;newpos=0;
	while(newpos<newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
            ctrlz.next_out = (char*)buf;
            ctrlz.avail_out = 8;
			CHECKV_BZ(BZ2_bzDecompress(&ctrlz), BZ_OK, BZ_STREAM_END);
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
		if(newpos+ctrl[0]>newsize) throw runtime_error("invalid patch");

		/* Read diff string */
        dataz.next_out = (char*)news + newpos;
        dataz.avail_out = ctrl[0];
		CHECKV_BZ(BZ2_bzDecompress(&dataz), BZ_OK, BZ_STREAM_END);

		/* Add old data to diff string */
		for(i=0;i<ctrl[0];i++)
			if((oldpos+i>=0) && (oldpos+i<oldsize))
				news[newpos+i]+=olds[oldpos+i];

		/* Adjust pointers */
		newpos+=ctrl[0];
		oldpos+=ctrl[0];

		/* Sanity-check */
		if(newpos+ctrl[1]>newsize) throw runtime_error("invalid patch");

		/* Read extra string */
        extra.next_out = (char*)news + newpos;
        extra.avail_out = ctrl[1];
		CHECKV_BZ(BZ2_bzDecompress(&extra), BZ_OK, BZ_STREAM_END);

		/* Adjust pointers */
		newpos+=ctrl[1];
		oldpos+=ctrl[2];
    }
    
    BZ2_bzDecompressEnd(&extra);
    BZ2_bzDecompressEnd(&dataz);
    BZ2_bzDecompressEnd(&ctrlz);
    
    return result;
}
