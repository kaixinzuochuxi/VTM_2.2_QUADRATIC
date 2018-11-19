/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2018, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     ContextModelling.h
 *  \brief    Classes providing probability descriptions and contexts (header)
 */

#ifndef __CONTEXTMODELLING__
#define __CONTEXTMODELLING__


#include "CommonDef.h"
#include "Contexts.h"
#include "Slice.h"
#include "Unit.h"
#include "UnitPartitioner.h"

#include <bitset>


struct CoeffCodingContext
{
public:
#if HEVC_USE_SIGN_HIDING
  CoeffCodingContext( const TransformUnit& tu, ComponentID component, bool signHide);
#else
  CoeffCodingContext( const TransformUnit& tu, ComponentID component );
#endif
public:
  void  initSubblock     ( int SubsetId, bool sigGroupFlag = false );
public:
  void  resetSigGroup   ()                      { m_sigCoeffGroupFlag.reset( m_subSetPos ); }
  void  setSigGroup     ()                      { m_sigCoeffGroupFlag.set( m_subSetPos ); }
  void  setScanPosLast  ( int       posLast )   { m_scanPosLast = posLast; }
public:
  ComponentID     compID          ()                        const { return m_compID; }
  int             subSetId        ()                        const { return m_subSetId; }
  int             subSetPos       ()                        const { return m_subSetPos; }
  int             cgPosY          ()                        const { return m_subSetPosY; }
  int             cgPosX          ()                        const { return m_subSetPosX; }
  unsigned        width           ()                        const { return m_width; }
  unsigned        height          ()                        const { return m_height; }
  unsigned        log2CGSize      ()                        const { return m_log2CGSize; }
  unsigned        log2BlockWidth  ()                        const { return m_log2BlockWidth; }
  unsigned        log2BlockHeight ()                        const { return m_log2BlockHeight; }
  unsigned        log2BlockSize   ()                        const { return m_log2BlockSize; }
  bool            extPrec         ()                        const { return m_extendedPrecision; }
  int             maxLog2TrDRange ()                        const { return m_maxLog2TrDynamicRange; }
  unsigned        maxNumCoeff     ()                        const { return m_maxNumCoeff; }
  int             scanPosLast     ()                        const { return m_scanPosLast; }
  int             minSubPos       ()                        const { return m_minSubPos; }
  int             maxSubPos       ()                        const { return m_maxSubPos; }
  bool            isLast          ()                        const { return ( ( m_scanPosLast >> m_log2CGSize ) == m_subSetId ); }
  bool            isNotFirst      ()                        const { return ( m_subSetId != 0 ); }
  bool            isSigGroup      ( int       scanPosCG )   const { return m_sigCoeffGroupFlag[ m_scanCG[ scanPosCG ] ]; }
  bool            isSigGroup      ()                        const { return m_sigCoeffGroupFlag[ m_subSetPos ]; }
#if HEVC_USE_SIGN_HIDING
  bool            signHiding      ()                        const { return m_signHiding; }
  bool            hideSign        ( int       posFirst,
                                    int       posLast   )   const { return ( m_signHiding && ( posLast - posFirst >= SBH_THRESHOLD ) ); }
#endif
  CoeffScanType   scanType        ()                        const { return m_scanType; }
  unsigned        blockPos        ( int       scanPos   )   const { return m_scan[ scanPos ]; }
  unsigned        posX            ( int       scanPos   )   const { return m_scanPosX[ scanPos ]; }
  unsigned        posY            ( int       scanPos   )   const { return m_scanPosY[ scanPos ]; }
  unsigned        maxLastPosX     ()                        const { return m_maxLastPosX; }
  unsigned        maxLastPosY     ()                        const { return m_maxLastPosY; }
  unsigned        lastXCtxId      ( unsigned  posLastX  )   const { return m_CtxSetLastX( m_lastOffsetX + ( posLastX >> m_lastShiftX ) ); }
  unsigned        lastYCtxId      ( unsigned  posLastY  )   const { return m_CtxSetLastY( m_lastOffsetY + ( posLastY >> m_lastShiftY ) ); }
  unsigned        sigGroupCtxId   ()                        const { return m_sigGroupCtxId; }

  unsigned sigCtxIdAbs( int scanPos, const TCoeff* coeff, const int state )
  {
    const uint32_t    posY      = m_scanPosY[ scanPos ];
    const uint32_t    posX      = m_scanPosX[ scanPos ];
    const TCoeff* pData     = coeff + posX + posY * m_width;
    const int     diag      = posX + posY;
    int           numPos    = 0;
    int           sumAbs    = 0;
#define UPDATE(x) {int a=abs(x);sumAbs+=std::min(4-(a&1),a);numPos+=!!a;}
    if( posX < m_width-1 )
    {
      UPDATE( pData[1] );
      if( posX < m_width-2 )
      {
        UPDATE( pData[2] );
      }
      if( posY < m_height-1 )
      {
        UPDATE( pData[m_width+1] );
      }
    }
    if( posY < m_height-1 )
    {
      UPDATE( pData[m_width] );
      if( posY < m_height-2 )
      {
        UPDATE( pData[m_width<<1] );
      }
    }
#undef UPDATE
    int ctxOfs = std::min( sumAbs, 5 ) + ( diag < 2 ? 6 : 0 );
    if( m_chType == CHANNEL_TYPE_LUMA )
    {
      ctxOfs += diag < 5 ? 6 : 0;
    }
    m_tmplCpDiag = diag;
    m_tmplCpSum1 = sumAbs - numPos;
    return m_sigFlagCtxSet[std::max( 0, state-1 )]( ctxOfs );
  }

  uint8_t ctxOffsetAbs()
  {
    int offset = 0;
    if( m_tmplCpDiag != -1 )
    {
      offset  = std::min( m_tmplCpSum1, 4 ) + 1;
      offset += ( !m_tmplCpDiag ? ( m_chType == CHANNEL_TYPE_LUMA ? 15 : 5 ) : m_chType == CHANNEL_TYPE_LUMA ? m_tmplCpDiag < 3 ? 10 : ( m_tmplCpDiag < 10 ? 5 : 0 ) : 0 );
    }
    return uint8_t(offset);
  }

  unsigned parityCtxIdAbs   ( uint8_t offset )  const { return m_parFlagCtxSet   ( offset ); }
  unsigned greater1CtxIdAbs ( uint8_t offset )  const { return m_gtxFlagCtxSet[1]( offset ); }
  unsigned greater2CtxIdAbs ( uint8_t offset )  const { return m_gtxFlagCtxSet[0]( offset ); }

  unsigned GoRiceParAbs( int scanPos, const TCoeff* coeff ) const
  {
#define UPDATE(x) sum+=abs(x)-!!x
    const uint32_t    posY      = m_scanPosY[ scanPos ];
    const uint32_t    posX      = m_scanPosX[ scanPos ];
    const TCoeff* pData     = coeff + posX + posY * m_width;
    int           sum       = 0;
    if( posX < m_width-1 )
    {
      UPDATE( pData[1] );
      if( posX < m_width-2 )
      {
        UPDATE( pData[2] );
      }
      if( posY < m_height-1 )
      {
        UPDATE( pData[m_width+1] );
      }
    }
    if( posY < m_height-1 )
    {
      UPDATE( pData[m_width] );
      if( posY < m_height-2 )
      {
        UPDATE( pData[m_width<<1] );
      }
    }
#undef UPDATE
    int     r = g_auiGoRicePars[ std::min( sum, 31 ) ];
    return  r;
  }


  unsigned        emtNumSigCoeff()                          const { return m_emtNumSigCoeff; }
  void            setEmtNumSigCoeff( unsigned val )               { m_emtNumSigCoeff = val; }

private:
  // constant
  const ComponentID         m_compID;
  const ChannelType         m_chType;
  const unsigned            m_width;
  const unsigned            m_height;
  const unsigned            m_log2CGWidth;
  const unsigned            m_log2CGHeight;
  const unsigned            m_log2CGSize;
  const unsigned            m_widthInGroups;
  const unsigned            m_heightInGroups;
  const unsigned            m_log2BlockWidth;
  const unsigned            m_log2BlockHeight;
  const unsigned            m_log2BlockSize;
  const unsigned            m_maxNumCoeff;
#if HEVC_USE_SIGN_HIDING
  const bool                m_signHiding;
#endif
  const bool                m_extendedPrecision;
  const int                 m_maxLog2TrDynamicRange;
  CoeffScanType             m_scanType;
  const unsigned*           m_scan;
  const unsigned*           m_scanPosX;
  const unsigned*           m_scanPosY;
  const unsigned*           m_scanCG;
  const CtxSet              m_CtxSetLastX;
  const CtxSet              m_CtxSetLastY;
  const unsigned            m_maxLastPosX;
  const unsigned            m_maxLastPosY;
  const int                 m_lastOffsetX;
  const int                 m_lastOffsetY;
  const int                 m_lastShiftX;
  const int                 m_lastShiftY;
  const bool                m_TrafoBypass;
  // modified
  int                       m_scanPosLast;
  int                       m_subSetId;
  int                       m_subSetPos;
  int                       m_subSetPosX;
  int                       m_subSetPosY;
  int                       m_minSubPos;
  int                       m_maxSubPos;
  unsigned                  m_sigGroupCtxId;
  int                       m_tmplCpSum1;
  int                       m_tmplCpDiag;
  CtxSet                    m_sigFlagCtxSet[3];
  CtxSet                    m_parFlagCtxSet;
  CtxSet                    m_gtxFlagCtxSet[2];
  std::bitset<MLS_GRP_NUM>  m_sigCoeffGroupFlag;
  unsigned                  m_emtNumSigCoeff;
};


class CUCtx
{
public:
  CUCtx()              : isDQPCoded(false), isChromaQpAdjCoded(false),
                         numNonZeroCoeffNonTs(0) {}
  CUCtx(int _qp)       : isDQPCoded(false), isChromaQpAdjCoded(false),
                         numNonZeroCoeffNonTs(0), qp(_qp) {}
  ~CUCtx() {}
public:
  bool      isDQPCoded;
  bool      isChromaQpAdjCoded;
  uint32_t      numNonZeroCoeffNonTs;
  int8_t     qp;                   // used as a previous(last) QP and for QP prediction
};

class MergeCtx
{
public:
  MergeCtx() : numValidMergeCand( 0 ), hasMergedCandList( false ) { for( unsigned i = 0; i < MRG_MAX_NUM_CANDS; i++ ) mrgTypeNeighbours[i] = MRG_TYPE_DEFAULT_N; }
  ~MergeCtx() {}
public:
  MvField       mvFieldNeighbours [ MRG_MAX_NUM_CANDS << 1 ]; // double length for mv of both lists
  unsigned char interDirNeighbours[ MRG_MAX_NUM_CANDS      ];
  MergeType     mrgTypeNeighbours [ MRG_MAX_NUM_CANDS      ];
  int           numValidMergeCand;
  bool          hasMergedCandList;

  MotionBuf     subPuMvpMiBuf;
  MotionBuf     subPuMvpExtMiBuf;
  void setMergeInfo( PredictionUnit& pu, int candIdx );
};


namespace DeriveCtx
{
unsigned CtxCUsplit   ( const CodingStructure& cs, Partitioner& partitioner );
unsigned CtxBTsplit   ( const CodingStructure& cs, Partitioner& partitioner );
#if ENABLE_BMS
unsigned CtxQtCbf     ( const ComponentID compID, const unsigned trDepth, const bool prevCbCbf );
#else
unsigned CtxQtCbf     ( const ComponentID compID, const bool prevCbCbf );
#endif
unsigned CtxInterDir  ( const PredictionUnit& pu );
unsigned CtxSkipFlag  ( const CodingUnit& cu );
unsigned CtxIMVFlag   ( const CodingUnit& cu );
unsigned CtxAffineFlag( const CodingUnit& cu );
}

#endif // __CONTEXTMODELLING__
