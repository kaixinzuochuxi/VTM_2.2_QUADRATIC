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

/** \file     RateCtrl.cpp
    \brief    Rate control manager class
*/
#include "RateCtrl.h"
#include "../CommonLib/ChromaFormat.h"

#include <cmath>

#define LAMBDA_PREC                                           1000000

using namespace std;

//sequence level
EncRCSeq::EncRCSeq()
{
  m_totalFrames         = 0;
  m_targetRate          = 0;
  m_frameRate           = 0;
  m_targetBits          = 0;
  m_GOPSize             = 0;
  m_picWidth            = 0;
  m_picHeight           = 0;
  m_LCUWidth            = 0;
  m_LCUHeight           = 0;
  m_numberOfLevel       = 0;
  m_numberOfLCU         = 0;
  m_averageBits         = 0;
  m_bitsRatio           = NULL;
  m_GOPID2Level         = NULL;
  m_picPara             = NULL;
  m_LCUPara             = NULL;
  m_numberOfPixel       = 0;
  m_framesLeft          = 0;
  m_bitsLeft            = 0;
  m_useLCUSeparateModel = false;
  m_adaptiveBit         = 0;
  m_lastLambda          = 0.0;
  m_bitDepth          = 0;
}

EncRCSeq::~EncRCSeq()
{
  destroy();
}

void EncRCSeq::create( int totalFrames, int targetBitrate, int frameRate, int GOPSize, int picWidth, int picHeight, int LCUWidth, int LCUHeight, int numberOfLevel, bool useLCUSeparateModel, int adaptiveBit )
{
  destroy();
  m_totalFrames         = totalFrames;
  m_targetRate          = targetBitrate;
  m_frameRate           = frameRate;
  m_GOPSize             = GOPSize;
  m_picWidth            = picWidth;
  m_picHeight           = picHeight;
  m_LCUWidth            = LCUWidth;
  m_LCUHeight           = LCUHeight;
  m_numberOfLevel       = numberOfLevel;
  m_useLCUSeparateModel = useLCUSeparateModel;

  m_numberOfPixel   = m_picWidth * m_picHeight;
  m_targetBits      = (int64_t)m_totalFrames * (int64_t)m_targetRate / (int64_t)m_frameRate;
  m_seqTargetBpp = (double)m_targetRate / (double)m_frameRate / (double)m_numberOfPixel;


#if USE_R_Lambda_INTRA
  if (m_seqTargetBpp < 0.03)
  {
    m_aupdate = 0.01;
    m_bupdate = 0.005;
  }
  else if (m_seqTargetBpp < 0.08)
  {
    m_aupdate = 0.05;
    m_bupdate = 0.025;
  }
  else if (m_seqTargetBpp < 0.2)
  {
    m_aupdate = 0.1;
    m_bupdate = 0.05;
  }
  else if (m_seqTargetBpp < 0.5)
  {
    m_aupdate = 0.2;
    m_bupdate = 0.1;
  }
  else
  {
    m_aupdate = 0.4;
    m_bupdate = 0.2;
  }
#else
  if ( m_seqTargetBpp < 0.03 )
  {
    m_alphaUpdate = 0.01;
    m_betaUpdate  = 0.005;
  }
  else if ( m_seqTargetBpp < 0.08 )
  {
    m_alphaUpdate = 0.05;
    m_betaUpdate  = 0.025;
  }
  else if ( m_seqTargetBpp < 0.2 )
  {
    m_alphaUpdate = 0.1;
    m_betaUpdate  = 0.05;
  }
  else if ( m_seqTargetBpp < 0.5 )
  {
    m_alphaUpdate = 0.2;
    m_betaUpdate  = 0.1;
  }
  else
  {
    m_alphaUpdate = 0.4;
    m_betaUpdate  = 0.2;
  }
#endif
  m_averageBits     = (int)(m_targetBits / totalFrames);
  int picWidthInBU  = ( m_picWidth  % m_LCUWidth  ) == 0 ? m_picWidth  / m_LCUWidth  : m_picWidth  / m_LCUWidth  + 1;
  int picHeightInBU = ( m_picHeight % m_LCUHeight ) == 0 ? m_picHeight / m_LCUHeight : m_picHeight / m_LCUHeight + 1;
  m_numberOfLCU     = picWidthInBU * picHeightInBU;

  m_bitsRatio   = new int[m_GOPSize];
  for ( int i=0; i<m_GOPSize; i++ )
  {
    m_bitsRatio[i] = 1;
  }

  m_GOPID2Level = new int[m_GOPSize];
  for ( int i=0; i<m_GOPSize; i++ )
  {
    m_GOPID2Level[i] = 1;
  }

  m_picPara = new TRCParameter[m_numberOfLevel];
  for ( int i=0; i<m_numberOfLevel; i++ )
  {
#if USE_R_Lambda_INTRA
    m_picPara[i].m_a = 0.0;
    m_picPara[i].m_b = 0.0;
    m_picPara[i].m_c = 0.0;
    m_picPara[i].m_validPix = -1;
#else
    m_picPara[i].m_alpha = 0.0;
    m_picPara[i].m_beta  = 0.0;
    m_picPara[i].m_validPix = -1;
#endif
  }

  if ( m_useLCUSeparateModel )
  {
    m_LCUPara = new TRCParameter*[m_numberOfLevel];
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      m_LCUPara[i] = new TRCParameter[m_numberOfLCU];
      for ( int j=0; j<m_numberOfLCU; j++)
      {
#if USE_R_Lambda_INTRA
        m_LCUPara[i][j].m_a = 0.0;
        m_LCUPara[i][j].m_b = 0.0;
        m_LCUPara[i][j].m_c = 0.0;
        m_LCUPara[i][j].m_validPix = -1;
#else
        m_LCUPara[i][j].m_alpha = 0.0;
        m_LCUPara[i][j].m_beta = 0.0;
        m_LCUPara[i][j].m_validPix = -1;
#endif
        
      }
    }
  }

  m_framesLeft = m_totalFrames;
  m_bitsLeft   = m_targetBits;
  m_adaptiveBit = adaptiveBit;
  m_lastLambda = 0.0;
}

void EncRCSeq::destroy()
{
  if (m_bitsRatio != NULL)
  {
    delete[] m_bitsRatio;
    m_bitsRatio = NULL;
  }

  if ( m_GOPID2Level != NULL )
  {
    delete[] m_GOPID2Level;
    m_GOPID2Level = NULL;
  }

  if ( m_picPara != NULL )
  {
    delete[] m_picPara;
    m_picPara = NULL;
  }

  if ( m_LCUPara != NULL )
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      delete[] m_LCUPara[i];
    }
    delete[] m_LCUPara;
    m_LCUPara = NULL;
  }
}

void EncRCSeq::initBitsRatio( int bitsRatio[])
{
  for (int i=0; i<m_GOPSize; i++)
  {
    m_bitsRatio[i] = bitsRatio[i];
  }
}

void EncRCSeq::initGOPID2Level( int GOPID2Level[] )
{
  for ( int i=0; i<m_GOPSize; i++ )
  {
    m_GOPID2Level[i] = GOPID2Level[i];
  }
}

void EncRCSeq::initPicPara( TRCParameter* picPara )
{
  CHECK( m_picPara == NULL, "Object does not exist" );

  if ( picPara == NULL )
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
#if USE_R_Lambda_INTRA
      int bitdepth_luma_scale =
        2
        * (m_bitDepth - 8
          - DISTORTION_PRECISION_ADJUSTMENT(m_bitDepth));
      m_picPara[i].m_a = ParaA * pow(2.0, bitdepth_luma_scale);
      m_picPara[i].m_b = ParaB * pow(2.0, bitdepth_luma_scale);
      m_picPara[i].m_c = ParaC * pow(2.0, bitdepth_luma_scale);





#else


      if (i>0)
      {
        int bitdepth_luma_scale =
          2
          * (m_bitDepth - 8
            - DISTORTION_PRECISION_ADJUSTMENT(m_bitDepth));
        m_picPara[i].m_alpha = 3.2003 * pow(2.0, bitdepth_luma_scale);
        m_picPara[i].m_beta = -1.367;
      }
      else
      {
        int bitdepth_luma_scale =
          2
          * (m_bitDepth - 8
            - DISTORTION_PRECISION_ADJUSTMENT(m_bitDepth));
        m_picPara[i].m_alpha = pow(2.0, bitdepth_luma_scale) * ALPHA;
        m_picPara[i].m_beta = BETA2;
      }
#endif
    }
  }
  else
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      m_picPara[i] = picPara[i];
    }
  }
}

void EncRCSeq::initLCUPara( TRCParameter** LCUPara )
{
  if ( m_LCUPara == NULL )
  {
    return;
  }
  if ( LCUPara == NULL )
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      for ( int j=0; j<m_numberOfLCU; j++)
      {
#if USE_R_Lambda_INTRA
        m_LCUPara[i][j].m_a = m_picPara[i].m_a;
        m_LCUPara[i][j].m_b = m_picPara[i].m_b;
        m_LCUPara[i][j].m_c = m_picPara[i].m_c;
#else
        m_LCUPara[i][j].m_alpha = m_picPara[i].m_alpha;
        m_LCUPara[i][j].m_beta  = m_picPara[i].m_beta;
#endif
      }
    }
  }
  else
  {
    for ( int i=0; i<m_numberOfLevel; i++ )
    {
      for ( int j=0; j<m_numberOfLCU; j++)
      {
        m_LCUPara[i][j] = LCUPara[i][j];
      }
    }
  }
}

void EncRCSeq::updateAfterPic ( int bits )
{
  m_bitsLeft -= bits;
  m_framesLeft--;
}

void EncRCSeq::setAllBitRatio( double basicLambda, double* equaCoeffA, double* equaCoeffB )
{
  int* bitsRatio = new int[m_GOPSize];
  for ( int i=0; i<m_GOPSize; i++ )
  {
    bitsRatio[i] = (int)(equaCoeffA[i] * pow(basicLambda, equaCoeffB[i]) * (double)getPicPara(getGOPID2Level(i)).m_validPix);
  }
  initBitsRatio( bitsRatio );
  delete[] bitsRatio;
}

//GOP level
EncRCGOP::EncRCGOP()
{
  m_encRCSeq  = NULL;
  m_picTargetBitInGOP = NULL;
  m_numPic     = 0;
  m_targetBits = 0;
  m_picLeft    = 0;
  m_bitsLeft   = 0;
}

EncRCGOP::~EncRCGOP()
{
  destroy();
}

void EncRCGOP::create( EncRCSeq* encRCSeq, int numPic )
{
  destroy();
  int targetBits = xEstGOPTargetBits( encRCSeq, numPic );

  if ( encRCSeq->getAdaptiveBits() > 0 && encRCSeq->getLastLambda() > 0.1 )
  {
    double targetBpp = (double)targetBits / encRCSeq->getNumPixel();
    double basicLambda = 0.0;
    double* lambdaRatio = new double[encRCSeq->getGOPSize()];
    double* equaCoeffA = new double[encRCSeq->getGOPSize()];
    double* equaCoeffB = new double[encRCSeq->getGOPSize()];

    if ( encRCSeq->getAdaptiveBits() == 1 )   // for GOP size =4, low delay case
    {
      if ( encRCSeq->getLastLambda() < 120.0 )
      {
        lambdaRatio[1] = 0.725 * log( encRCSeq->getLastLambda() ) + 0.5793;
        lambdaRatio[0] = 1.3 * lambdaRatio[1];
        lambdaRatio[2] = 1.3 * lambdaRatio[1];
        lambdaRatio[3] = 1.0;
      }
      else
      {
        lambdaRatio[0] = 5.0;
        lambdaRatio[1] = 4.0;
        lambdaRatio[2] = 5.0;
        lambdaRatio[3] = 1.0;
      }
    }
    else if ( encRCSeq->getAdaptiveBits() == 2 )  // for GOP size = 8, random access case
    {
      if ( encRCSeq->getLastLambda() < 90.0 )
      {
        lambdaRatio[0] = 1.0;
        lambdaRatio[1] = 0.725 * log( encRCSeq->getLastLambda() ) + 0.7963;
        lambdaRatio[2] = 1.3 * lambdaRatio[1];
        lambdaRatio[3] = 3.25 * lambdaRatio[1];
        lambdaRatio[4] = 3.25 * lambdaRatio[1];
        lambdaRatio[5] = 1.3  * lambdaRatio[1];
        lambdaRatio[6] = 3.25 * lambdaRatio[1];
        lambdaRatio[7] = 3.25 * lambdaRatio[1];
      }
      else
      {
        lambdaRatio[0] = 1.0;
        lambdaRatio[1] = 4.0;
        lambdaRatio[2] = 5.0;
        lambdaRatio[3] = 12.3;
        lambdaRatio[4] = 12.3;
        lambdaRatio[5] = 5.0;
        lambdaRatio[6] = 12.3;
        lambdaRatio[7] = 12.3;
      }
    }
    else if (encRCSeq->getAdaptiveBits() == 3)  // for GOP size = 16, random access case
    {
      {
        int bitdepth_luma_scale =
          2
          * (encRCSeq->getbitDepth() - 8
            - DISTORTION_PRECISION_ADJUSTMENT(encRCSeq->getbitDepth()));

        double hierarQp = 4.2005 * log(encRCSeq->getLastLambda() / pow(2.0, bitdepth_luma_scale)) + 13.7122;  //  the qp of POC16
        double qpLev2 = (hierarQp + 0.0) + 0.2016    * (hierarQp + 0.0) - 4.8848;
        double qpLev3 = (hierarQp + 3.0) + 0.22286 * (hierarQp + 3.0) - 5.7476;
        double qpLev4 = (hierarQp + 4.0) + 0.2333    * (hierarQp + 4.0) - 5.9;
        double qpLev5 = (hierarQp + 5.0) + 0.3            * (hierarQp + 5.0) - 7.1444;

        double lambdaLev1 = exp((hierarQp - 13.7122) / 4.2005) *pow(2.0, bitdepth_luma_scale);
        double lambdaLev2 = exp((qpLev2 - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);
        double lambdaLev3 = exp((qpLev3 - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);
        double lambdaLev4 = exp((qpLev4 - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);
        double lambdaLev5 = exp((qpLev5 - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);

        lambdaRatio[0] = 1.0;
        lambdaRatio[1] = lambdaLev2 / lambdaLev1;
        lambdaRatio[2] = lambdaLev3 / lambdaLev1;
        lambdaRatio[3] = lambdaLev4 / lambdaLev1;
        lambdaRatio[4] = lambdaLev5 / lambdaLev1;
        lambdaRatio[5] = lambdaLev5 / lambdaLev1;
        lambdaRatio[6] = lambdaLev4 / lambdaLev1;
        lambdaRatio[7] = lambdaLev5 / lambdaLev1;
        lambdaRatio[8] = lambdaLev5 / lambdaLev1;
        lambdaRatio[9] = lambdaLev3 / lambdaLev1;
        lambdaRatio[10] = lambdaLev4 / lambdaLev1;
        lambdaRatio[11] = lambdaLev5 / lambdaLev1;
        lambdaRatio[12] = lambdaLev5 / lambdaLev1;
        lambdaRatio[13] = lambdaLev4 / lambdaLev1;
        lambdaRatio[14] = lambdaLev5 / lambdaLev1;
        lambdaRatio[15] = lambdaLev5 / lambdaLev1;
      }
    }

    xCalEquaCoeff( encRCSeq, lambdaRatio, equaCoeffA, equaCoeffB, encRCSeq->getGOPSize() );
    basicLambda = xSolveEqua(encRCSeq, targetBpp, equaCoeffA, equaCoeffB, encRCSeq->getGOPSize());
    encRCSeq->setAllBitRatio( basicLambda, equaCoeffA, equaCoeffB );

    delete []lambdaRatio;
    delete []equaCoeffA;
    delete []equaCoeffB;
  }

  m_picTargetBitInGOP = new int[numPic];
  int i;
  int totalPicRatio = 0;
  int currPicRatio = 0;
  for ( i=0; i<numPic; i++ )
  {
    totalPicRatio += encRCSeq->getBitRatio( i );
  }
  for ( i=0; i<numPic; i++ )
  {
    currPicRatio = encRCSeq->getBitRatio( i );
    m_picTargetBitInGOP[i] = (int)( ((double)targetBits) * currPicRatio / totalPicRatio );
  }

  m_encRCSeq    = encRCSeq;
  m_numPic       = numPic;
  m_targetBits   = targetBits;
  m_picLeft      = m_numPic;
  m_bitsLeft     = m_targetBits;
}

void EncRCGOP::xCalEquaCoeff( EncRCSeq* encRCSeq, double* lambdaRatio, double* equaCoeffA, double* equaCoeffB, int GOPSize )
{
  for ( int i=0; i<GOPSize; i++ )
  {
    int frameLevel = encRCSeq->getGOPID2Level(i);
#if USE_R_Lambda_INTRA
    double a = encRCSeq->getPicPara(frameLevel).m_a;
    double b = encRCSeq->getPicPara(frameLevel).m_b;
    double bpp = encRCSeq->getAverageBits();
    //////////////////////////////////////////////////////////////////
    ////////// TODO:RECALCULATE LAMBDA RATIO
    equaCoeffA[i] = -(2*a*log(bpp/3)+b)/lambdaRatio[i];
    equaCoeffB[i] = 1;
#else

    double alpha   = encRCSeq->getPicPara(frameLevel).m_alpha;
    double beta    = encRCSeq->getPicPara(frameLevel).m_beta;
    equaCoeffA[i] = pow( 1.0/alpha, 1.0/beta ) * pow( lambdaRatio[i], 1.0/beta );
    equaCoeffB[i] = 1.0/beta;
#endif
  }
}

double EncRCGOP::xSolveEqua(EncRCSeq* encRCSeq, double targetBpp, double* equaCoeffA, double* equaCoeffB, int GOPSize)
{
  double solution = 100.0;
  double minNumber = 0.1;
  double maxNumber = 10000.0;
  for ( int i=0; i<g_RCIterationNum; i++ )
  {
    double fx = 0.0;
    for ( int j=0; j<GOPSize; j++ )
    {
#if USE_R_Lambda_INTRA
      double tmpBpp = equaCoeffA[j] /solution;
      double actualBpp = tmpBpp * (double)encRCSeq->getPicPara(encRCSeq->getGOPID2Level(j)).m_validPix / (double)encRCSeq->getNumPixel();
      fx += actualBpp;
#else
      double tmpBpp = equaCoeffA[j] * pow(solution, equaCoeffB[j]);
      double actualBpp = tmpBpp * (double)encRCSeq->getPicPara(encRCSeq->getGOPID2Level(j)).m_validPix / (double)encRCSeq->getNumPixel();
      fx += actualBpp;
#endif
    }

    if ( fabs( fx - targetBpp ) < 0.000001 )
    {
      break;
    }

    if ( fx > targetBpp )
    {
      minNumber = solution;
      solution = ( solution + maxNumber ) / 2.0;
    }
    else
    {
      maxNumber = solution;
      solution = ( solution + minNumber ) / 2.0;
    }
  }

  solution = Clip3( 0.1, 10000.0, solution );
  return solution;
}

void EncRCGOP::destroy()
{
  m_encRCSeq = NULL;
  if ( m_picTargetBitInGOP != NULL )
  {
    delete[] m_picTargetBitInGOP;
    m_picTargetBitInGOP = NULL;
  }
}

void EncRCGOP::updateAfterPicture( int bitsCost )
{
  m_bitsLeft -= bitsCost;
  m_picLeft--;
}

int EncRCGOP::xEstGOPTargetBits( EncRCSeq* encRCSeq, int GOPSize )
{
  int realInfluencePicture = min( g_RCSmoothWindowSize, encRCSeq->getFramesLeft() );
  int averageTargetBitsPerPic = (int)( encRCSeq->getTargetBits() / encRCSeq->getTotalFrames() );
  int currentTargetBitsPerPic = (int)( ( encRCSeq->getBitsLeft() - averageTargetBitsPerPic * (encRCSeq->getFramesLeft() - realInfluencePicture) ) / realInfluencePicture );
  int targetBits = currentTargetBitsPerPic * GOPSize;

  if ( targetBits < 200 )
  {
    targetBits = 200;   // at least allocate 200 bits for one GOP
  }

  return targetBits;
}

//picture level
EncRCPic::EncRCPic()
{
  m_encRCSeq = NULL;
  m_encRCGOP = NULL;

  m_frameLevel    = 0;
  m_numberOfPixel = 0;
  m_numberOfLCU   = 0;
  m_targetBits    = 0;
  m_estHeaderBits = 0;
  m_estPicQP      = 0;
  m_estPicLambda  = 0.0;

  m_LCULeft       = 0;
  m_bitsLeft      = 0;
  m_pixelsLeft    = 0;

  m_LCUs         = NULL;
  m_picActualHeaderBits = 0;
  m_picActualBits       = 0;
  m_picQP               = 0;
  m_picLambda           = 0.0;
  m_picMSE              = 0.0;
  m_validPixelsInPic    = 0;
}

EncRCPic::~EncRCPic()
{
  destroy();
}

int EncRCPic::xEstPicTargetBits( EncRCSeq* encRCSeq, EncRCGOP* encRCGOP )
{
  int targetBits        = 0;
  int GOPbitsLeft       = encRCGOP->getBitsLeft();

  int i;
  int currPicPosition = encRCGOP->getNumPic()-encRCGOP->getPicLeft();
  int currPicRatio    = encRCSeq->getBitRatio( currPicPosition );
  int totalPicRatio   = 0;
  for ( i=currPicPosition; i<encRCGOP->getNumPic(); i++ )
  {
    totalPicRatio += encRCSeq->getBitRatio( i );
  }

  targetBits  = int( ((double)GOPbitsLeft) * currPicRatio / totalPicRatio );

  if ( targetBits < 100 )
  {
    targetBits = 100;   // at least allocate 100 bits for one picture
  }

  if ( m_encRCSeq->getFramesLeft() > 16 )
  {
    targetBits = int( g_RCWeightPicRargetBitInBuffer * targetBits + g_RCWeightPicTargetBitInGOP * m_encRCGOP->getTargetBitInGOP( currPicPosition ) );
  }

  return targetBits;
}

int EncRCPic::xEstPicHeaderBits( list<EncRCPic*>& listPreviousPictures, int frameLevel )
{
  int numPreviousPics   = 0;
  int totalPreviousBits = 0;

  list<EncRCPic*>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == frameLevel )
    {
      totalPreviousBits += (*it)->getPicActualHeaderBits();
      numPreviousPics++;
    }
  }

  int estHeaderBits = 0;
  if ( numPreviousPics > 0 )
  {
    estHeaderBits = totalPreviousBits / numPreviousPics;
  }

  return estHeaderBits;
}

#if V0078_ADAPTIVE_LOWER_BOUND
int EncRCPic::xEstPicLowerBound(EncRCSeq* encRCSeq, EncRCGOP* encRCGOP)
{
  int lowerBound = 0;
  int GOPbitsLeft = encRCGOP->getBitsLeft();

  const int nextPicPosition = (encRCGOP->getNumPic() - encRCGOP->getPicLeft() + 1) % encRCGOP->getNumPic();
  const int nextPicRatio = encRCSeq->getBitRatio(nextPicPosition);

  int totalPicRatio = 0;
  for (int i = nextPicPosition; i < encRCGOP->getNumPic(); i++)
  {
    totalPicRatio += encRCSeq->getBitRatio(i);
  }

  if (nextPicPosition == 0)
  {
    GOPbitsLeft = encRCGOP->getTargetBits();
  }
  else
  {
    GOPbitsLeft -= m_targetBits;
  }

  lowerBound = int(((double)GOPbitsLeft) * nextPicRatio / totalPicRatio);

  if (lowerBound < 100)
  {
    lowerBound = 100;   // at least allocate 100 bits for one picture
  }

  if (m_encRCSeq->getFramesLeft() > 16)
  {
    lowerBound = int(g_RCWeightPicRargetBitInBuffer * lowerBound + g_RCWeightPicTargetBitInGOP * m_encRCGOP->getTargetBitInGOP(nextPicPosition));
  }

  return lowerBound;
}
#endif

void EncRCPic::addToPictureLsit( list<EncRCPic*>& listPreviousPictures )
{
  if ( listPreviousPictures.size() > g_RCMaxPicListSize )
  {
    EncRCPic* p = listPreviousPictures.front();
    listPreviousPictures.pop_front();
    p->destroy();
    delete p;
  }

  listPreviousPictures.push_back( this );
}

void EncRCPic::create( EncRCSeq* encRCSeq, EncRCGOP* encRCGOP, int frameLevel, list<EncRCPic*>& listPreviousPictures )
{
  destroy();
  m_encRCSeq = encRCSeq;
  m_encRCGOP = encRCGOP;

  int targetBits    = xEstPicTargetBits( encRCSeq, encRCGOP );
  int estHeaderBits = xEstPicHeaderBits( listPreviousPictures, frameLevel );

  if ( targetBits < estHeaderBits + 100 )
  {
    targetBits = estHeaderBits + 100;   // at least allocate 100 bits for picture data
  }

  m_frameLevel       = frameLevel;
  m_numberOfPixel    = encRCSeq->getNumPixel();
  m_numberOfLCU      = encRCSeq->getNumberOfLCU();
  m_estPicLambda     = 100.0;
  m_targetBits       = targetBits;
  m_estHeaderBits    = estHeaderBits;
  m_bitsLeft         = m_targetBits;
  int picWidth       = encRCSeq->getPicWidth();
  int picHeight      = encRCSeq->getPicHeight();
  int LCUWidth       = encRCSeq->getLCUWidth();
  int LCUHeight      = encRCSeq->getLCUHeight();
  int picWidthInLCU  = ( picWidth  % LCUWidth  ) == 0 ? picWidth  / LCUWidth  : picWidth  / LCUWidth  + 1;
  int picHeightInLCU = ( picHeight % LCUHeight ) == 0 ? picHeight / LCUHeight : picHeight / LCUHeight + 1;
#if V0078_ADAPTIVE_LOWER_BOUND
  m_lowerBound       = xEstPicLowerBound( encRCSeq, encRCGOP );
#endif

  m_LCULeft         = m_numberOfLCU;
  m_bitsLeft       -= m_estHeaderBits;
  m_pixelsLeft      = m_numberOfPixel;

  m_LCUs           = new TRCLCU[m_numberOfLCU];
  int i, j;
  int LCUIdx;
  for ( i=0; i<picWidthInLCU; i++ )
  {
    for ( j=0; j<picHeightInLCU; j++ )
    {
      LCUIdx = j*picWidthInLCU + i;
      m_LCUs[LCUIdx].m_actualBits = 0;
      m_LCUs[LCUIdx].m_actualSSE  = 0.0;
      m_LCUs[LCUIdx].m_actualMSE  = 0.0;
      m_LCUs[LCUIdx].m_QP         = 0;
      m_LCUs[LCUIdx].m_lambda     = 0.0;
      m_LCUs[LCUIdx].m_targetBits = 0;
      m_LCUs[LCUIdx].m_bitWeight  = 1.0;
      int currWidth  = ( (i == picWidthInLCU -1) ? picWidth  - LCUWidth *(picWidthInLCU -1) : LCUWidth  );
      int currHeight = ( (j == picHeightInLCU-1) ? picHeight - LCUHeight*(picHeightInLCU-1) : LCUHeight );
      m_LCUs[LCUIdx].m_numberOfPixel = currWidth * currHeight;
    }
  }
  m_picActualHeaderBits = 0;
  m_picActualBits       = 0;
  m_picQP               = 0;
  m_picLambda           = 0.0;
  m_validPixelsInPic    = 0;
  m_picMSE              = 0.0;
}

void EncRCPic::destroy()
{
  if( m_LCUs != NULL )
  {
    delete[] m_LCUs;
    m_LCUs = NULL;
  }
  m_encRCSeq = NULL;
  m_encRCGOP = NULL;
}

#if USE_R_Lambda_INTRA
double EncRCPic::estimatePicLambda(list<EncRCPic*>& listPreviousPictures, bool isIRAP)
{
  double a = m_encRCSeq->getPicPara(m_frameLevel).m_a;
  double b = m_encRCSeq->getPicPara(m_frameLevel).m_b;
  double bpp = (double)m_targetBits / (double)m_numberOfPixel;

  int lastPicValPix = 0;
  if (listPreviousPictures.size() > 0)
  {
    lastPicValPix = m_encRCSeq->getPicPara(m_frameLevel).m_validPix;
  }
  if (lastPicValPix > 0)
  {
    bpp = (double)m_targetBits / (double)lastPicValPix;
  }

  double estLambda;
  if (isIRAP)
  {
    ///////////////////////////////////////////TODO?????
    estLambda = calculateLambdaIntra(a, b, pow(m_totalCostIntra / (double)m_numberOfPixel, BETA1), bpp);
  }
  else
  {
    estLambda = -2*a*log(bpp/3)/bpp-b/bpp;
  }

  double lastLevelLambda = -1.0;
  double lastPicLambda = -1.0;
  double lastValidLambda = -1.0;
  list<EncRCPic*>::iterator it;
  for (it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++)
  {
    if ((*it)->getFrameLevel() == m_frameLevel)
    {
      lastLevelLambda = (*it)->getPicActualLambda();
    }
    lastPicLambda = (*it)->getPicActualLambda();

    if (lastPicLambda > 0.0)
    {
      lastValidLambda = lastPicLambda;
    }
  }

  if (lastLevelLambda > 0.0)
  {
    lastLevelLambda = Clip3(0.1, 10000.0, lastLevelLambda);
    estLambda = Clip3(lastLevelLambda * pow(2.0, -3.0 / 3.0), lastLevelLambda * pow(2.0, 3.0 / 3.0), estLambda);
  }

  if (lastPicLambda > 0.0)
  {
    lastPicLambda = Clip3(0.1, 2000.0, lastPicLambda);
    estLambda = Clip3(lastPicLambda * pow(2.0, -10.0 / 3.0), lastPicLambda * pow(2.0, 10.0 / 3.0), estLambda);
  }
  else if (lastValidLambda > 0.0)
  {
    lastValidLambda = Clip3(0.1, 2000.0, lastValidLambda);
    estLambda = Clip3(lastValidLambda * pow(2.0, -10.0 / 3.0), lastValidLambda * pow(2.0, 10.0 / 3.0), estLambda);
  }
  else
  {
    estLambda = Clip3(0.1, 10000.0, estLambda);
  }

  if (estLambda < 0.1)
  {
    estLambda = 0.1;
  }

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  m_estPicLambda = estLambda;

  double totalWeight = 0.0;
  // initial BU bit allocation weight
  for (int i = 0; i<m_numberOfLCU; i++)
  {
    double aLCU, bLCU;
    if (m_encRCSeq->getUseLCUSeparateModel())
    {
      aLCU = m_encRCSeq->getLCUPara(m_frameLevel, i).m_a;
      bLCU = m_encRCSeq->getLCUPara(m_frameLevel, i).m_b;
    }
    else
    {
      aLCU = m_encRCSeq->getPicPara(m_frameLevel).m_a;
      bLCU = m_encRCSeq->getPicPara(m_frameLevel).m_b;
    }

    ////////////////////////////////////TODO, REWEIGHT
    //m_LCUs[i].m_bitWeight = m_LCUs[i].m_numberOfPixel * pow(estLambda / aLCU, 1.0 / bLCU);
    //////////     a*log(bpp)/bpp+b/bpp,from lambda to bpp. lnbpp distribution!!!
    ////////// use the average bpp for this frame
    m_LCUs[i].m_bitWeight = m_LCUs[i].m_numberOfPixel * (-2*aLCU*log(bpp/3)- bLCU)/ estLambda;



    if (m_LCUs[i].m_bitWeight < 0.01)
    {
      m_LCUs[i].m_bitWeight = 0.01;
    }
    totalWeight += m_LCUs[i].m_bitWeight;
  }
  for (int i = 0; i<m_numberOfLCU; i++)
  {
    double BUTargetBits = m_targetBits * m_LCUs[i].m_bitWeight / totalWeight;
    m_LCUs[i].m_bitWeight = BUTargetBits;
  }

  return estLambda;
}
#else
double EncRCPic::estimatePicLambda( list<EncRCPic*>& listPreviousPictures, bool isIRAP)
{
  double alpha         = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  double beta          = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
  double bpp       = (double)m_targetBits/(double)m_numberOfPixel;

  int lastPicValPix = 0;
  if (listPreviousPictures.size() > 0)
  {
    lastPicValPix = m_encRCSeq->getPicPara(m_frameLevel).m_validPix;
  }
  if (lastPicValPix > 0)
  {
    bpp = (double)m_targetBits / (double)lastPicValPix;
  }

  double estLambda;
  if (isIRAP)
  {
    estLambda = calculateLambdaIntra(alpha, beta, pow(m_totalCostIntra/(double)m_numberOfPixel, BETA1), bpp);
  }
  else
  {
    estLambda = alpha * pow( bpp, beta );
  }

  double lastLevelLambda = -1.0;
  double lastPicLambda   = -1.0;
  double lastValidLambda = -1.0;
  list<EncRCPic*>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == m_frameLevel )
    {
      lastLevelLambda = (*it)->getPicActualLambda();
    }
    lastPicLambda     = (*it)->getPicActualLambda();

    if ( lastPicLambda > 0.0 )
    {
      lastValidLambda = lastPicLambda;
    }
  }

  if ( lastLevelLambda > 0.0 )
  {
    lastLevelLambda = Clip3( 0.1, 10000.0, lastLevelLambda );
    estLambda = Clip3( lastLevelLambda * pow( 2.0, -3.0/3.0 ), lastLevelLambda * pow( 2.0, 3.0/3.0 ), estLambda );
  }

  if ( lastPicLambda > 0.0 )
  {
    lastPicLambda = Clip3( 0.1, 2000.0, lastPicLambda );
    estLambda = Clip3( lastPicLambda * pow( 2.0, -10.0/3.0 ), lastPicLambda * pow( 2.0, 10.0/3.0 ), estLambda );
  }
  else if ( lastValidLambda > 0.0 )
  {
    lastValidLambda = Clip3( 0.1, 2000.0, lastValidLambda );
    estLambda = Clip3( lastValidLambda * pow(2.0, -10.0/3.0), lastValidLambda * pow(2.0, 10.0/3.0), estLambda );
  }
  else
  {
    estLambda = Clip3( 0.1, 10000.0, estLambda );
  }

  if ( estLambda < 0.1 )
  {
    estLambda = 0.1;
  }

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  m_estPicLambda = estLambda;

  double totalWeight = 0.0;
  // initial BU bit allocation weight
  for ( int i=0; i<m_numberOfLCU; i++ )
  {
    double alphaLCU, betaLCU;
    if ( m_encRCSeq->getUseLCUSeparateModel() )
    {
      alphaLCU = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_alpha;
      betaLCU  = m_encRCSeq->getLCUPara( m_frameLevel, i ).m_beta;
    }
    else
    {
      alphaLCU = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
      betaLCU  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
    }

    m_LCUs[i].m_bitWeight =  m_LCUs[i].m_numberOfPixel * pow( estLambda/alphaLCU, 1.0/betaLCU );

    if ( m_LCUs[i].m_bitWeight < 0.01 )
    {
      m_LCUs[i].m_bitWeight = 0.01;
    }
    totalWeight += m_LCUs[i].m_bitWeight;
  }
  for ( int i=0; i<m_numberOfLCU; i++ )
  {
    double BUTargetBits = m_targetBits * m_LCUs[i].m_bitWeight / totalWeight;
    m_LCUs[i].m_bitWeight = BUTargetBits;
  }

  return estLambda;
}
#endif
int EncRCPic::estimatePicQP( double lambda, list<EncRCPic*>& listPreviousPictures )
{
  int bitdepth_luma_scale =
    2
    * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  int QP = int(4.2005 * log(lambda / pow(2.0, bitdepth_luma_scale)) + 13.7122 + 0.5);

  int lastLevelQP = g_RCInvalidQPValue;
  int lastPicQP   = g_RCInvalidQPValue;
  int lastValidQP = g_RCInvalidQPValue;
  list<EncRCPic*>::iterator it;
  for ( it = listPreviousPictures.begin(); it != listPreviousPictures.end(); it++ )
  {
    if ( (*it)->getFrameLevel() == m_frameLevel )
    {
      lastLevelQP = (*it)->getPicActualQP();
    }
    lastPicQP = (*it)->getPicActualQP();
    if ( lastPicQP > g_RCInvalidQPValue )
    {
      lastValidQP = lastPicQP;
    }
  }

  if ( lastLevelQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastLevelQP - 3, lastLevelQP + 3, QP );
  }

  if( lastPicQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastPicQP - 10, lastPicQP + 10, QP );
  }
  else if( lastValidQP > g_RCInvalidQPValue )
  {
    QP = Clip3( lastValidQP - 10, lastValidQP + 10, QP );
  }

  return QP;
}

double EncRCPic::getLCUTargetBpp(bool isIRAP)
{
  int   LCUIdx    = getLCUCoded();
  double bpp      = -1.0;
  int avgBits     = 0;

  if (isIRAP)
  {
    int noOfLCUsLeft = m_numberOfLCU - LCUIdx + 1;
    int bitrateWindow = min(4,noOfLCUsLeft);
    double MAD      = getLCU(LCUIdx).m_costIntra;

    if (m_remainingCostIntra > 0.1 )
    {
      double weightedBitsLeft = (m_bitsLeft*bitrateWindow+(m_bitsLeft-getLCU(LCUIdx).m_targetBitsLeft)*noOfLCUsLeft)/(double)bitrateWindow;
      avgBits = int( MAD*weightedBitsLeft/m_remainingCostIntra );
    }
    else
    {
      avgBits = int( m_bitsLeft / m_LCULeft );
    }
    m_remainingCostIntra -= MAD;
  }
  else
  {
    double totalWeight = 0;
    for ( int i=LCUIdx; i<m_numberOfLCU; i++ )
    {
      totalWeight += m_LCUs[i].m_bitWeight;
    }
    int realInfluenceLCU = min( g_RCLCUSmoothWindowSize, getLCULeft() );
    avgBits = (int)( m_LCUs[LCUIdx].m_bitWeight - ( totalWeight - m_bitsLeft ) / realInfluenceLCU + 0.5 );
  }

  if ( avgBits < 1 )
  {
    avgBits = 1;
  }

  bpp = ( double )avgBits/( double )m_LCUs[ LCUIdx ].m_numberOfPixel;
  m_LCUs[ LCUIdx ].m_targetBits = avgBits;

  return bpp;
}


#if USE_R_Lambda_INTRA
double EncRCPic::getLCUEstLambda(double bpp)
{
  int   LCUIdx = getLCUCoded();
  double a;
  double b;
  if (m_encRCSeq->getUseLCUSeparateModel())
  {
    a = m_encRCSeq->getLCUPara(m_frameLevel, LCUIdx).m_a;
    b = m_encRCSeq->getLCUPara(m_frameLevel, LCUIdx).m_b;
  }
  else
  {
    a = m_encRCSeq->getPicPara(m_frameLevel).m_a;
    b = m_encRCSeq->getPicPara(m_frameLevel).m_b;
  }

  double estLambda = -2*a * log(bpp/3) / bpp - b / bpp;
  //for Lambda clip, picture level clip
  double clipPicLambda = m_estPicLambda;

  //for Lambda clip, LCU level clip
  double clipNeighbourLambda = -1.0;
  for (int i = LCUIdx - 1; i >= 0; i--)
  {
    if (m_LCUs[i].m_lambda > 0)
    {
      clipNeighbourLambda = m_LCUs[i].m_lambda;
      break;
    }
  }

  if (clipNeighbourLambda > 0.0)
  {
    estLambda = Clip3(clipNeighbourLambda * pow(2.0, -1.0 / 3.0), clipNeighbourLambda * pow(2.0, 1.0 / 3.0), estLambda);
  }

  if (clipPicLambda > 0.0)
  {
    estLambda = Clip3(clipPicLambda * pow(2.0, -2.0 / 3.0), clipPicLambda * pow(2.0, 2.0 / 3.0), estLambda);
  }
  else
  {
    estLambda = Clip3(10.0, 1000.0, estLambda);
  }

  if (estLambda < 0.1)
  {
    estLambda = 0.1;
  }

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  return estLambda;
}
#else
double EncRCPic::getLCUEstLambda( double bpp )
{
  int   LCUIdx = getLCUCoded();
  double alpha;
  double beta;
  if ( m_encRCSeq->getUseLCUSeparateModel() )
  {
    alpha = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_alpha;
    beta  = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_beta;
  }
  else
  {
    alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
    beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;
  }

  double estLambda = alpha * pow( bpp, beta );
  //for Lambda clip, picture level clip
  double clipPicLambda = m_estPicLambda;

  //for Lambda clip, LCU level clip
  double clipNeighbourLambda = -1.0;
  for ( int i=LCUIdx - 1; i>=0; i-- )
  {
    if ( m_LCUs[i].m_lambda > 0 )
    {
      clipNeighbourLambda = m_LCUs[i].m_lambda;
      break;
    }
  }

  if ( clipNeighbourLambda > 0.0 )
  {
    estLambda = Clip3( clipNeighbourLambda * pow( 2.0, -1.0/3.0 ), clipNeighbourLambda * pow( 2.0, 1.0/3.0 ), estLambda );
  }

  if ( clipPicLambda > 0.0 )
  {
    estLambda = Clip3( clipPicLambda * pow( 2.0, -2.0/3.0 ), clipPicLambda * pow( 2.0, 2.0/3.0 ), estLambda );
  }
  else
  {
    estLambda = Clip3( 10.0, 1000.0, estLambda );
  }

  if ( estLambda < 0.1 )
  {
    estLambda = 0.1;
  }

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  return estLambda;
}
#endif
int EncRCPic::getLCUEstQP( double lambda, int clipPicQP )
{
  int LCUIdx = getLCUCoded();
  int bitdepth_luma_scale =
    2
    * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  int estQP = int(4.2005 * log(lambda / pow(2.0, bitdepth_luma_scale)) + 13.7122 + 0.5);

  //for Lambda clip, LCU level clip
  int clipNeighbourQP = g_RCInvalidQPValue;
  for ( int i=LCUIdx - 1; i>=0; i-- )
  {
    if ( (getLCU(i)).m_QP > g_RCInvalidQPValue )
    {
      clipNeighbourQP = getLCU(i).m_QP;
      break;
    }
  }

  if ( clipNeighbourQP > g_RCInvalidQPValue )
  {
    estQP = Clip3( clipNeighbourQP - 1, clipNeighbourQP + 1, estQP );
  }

  estQP = Clip3( clipPicQP - 2, clipPicQP + 2, estQP );

  return estQP;
}


#if USE_R_Lambda_INTRA
void EncRCPic::updateAfterCTU(int LCUIdx, int bits, int QP, double lambda, bool updateLCUParameter)
{
  m_LCUs[LCUIdx].m_actualBits = bits;
  m_LCUs[LCUIdx].m_QP = QP;
  m_LCUs[LCUIdx].m_lambda = lambda;
  m_LCUs[LCUIdx].m_actualSSE = m_LCUs[LCUIdx].m_actualMSE * m_LCUs[LCUIdx].m_numberOfPixel;

  m_LCULeft--;
  m_bitsLeft -= bits;
  m_pixelsLeft -= m_LCUs[LCUIdx].m_numberOfPixel;

  if (!updateLCUParameter)
  {
    return;
  }

  if (!m_encRCSeq->getUseLCUSeparateModel())
  {
    return;
  }

  double a = m_encRCSeq->getLCUPara(m_frameLevel, LCUIdx).m_a;
  double b = m_encRCSeq->getLCUPara(m_frameLevel, LCUIdx).m_b;
  double c = m_encRCSeq->getLCUPara(m_frameLevel, LCUIdx).m_c;

  int LCUActualBits = m_LCUs[LCUIdx].m_actualBits;
  int LCUTotalPixels = m_LCUs[LCUIdx].m_numberOfPixel;
  double bpp = (double)LCUActualBits / (double)LCUTotalPixels;
  double calLambda = -2*a * log(bpp/3) / bpp - b / bpp;
  double inputLambda = m_LCUs[LCUIdx].m_lambda;

  ///////////////////////////////////////////////////////TODO
  if (inputLambda < 0.01 || calLambda < 0.01 || bpp < 0.0001)
  {
    //////////////////////////////////////////////////// TODO:HOW TO UPDATE
#if USE_R_Lambda_INTRA
    a *= (1.0 - m_encRCSeq->getaUpdate() / 2.0);
    b *= (1.0 - m_encRCSeq->getbUpdate() / 2.0);
#else
    a *= (1.0 - m_encRCSeq->getAlphaUpdate() / 2.0);
    b *= (1.0 - m_encRCSeq->getBetaUpdate() / 2.0);
#endif
    a = Clip3(g_RCAlphaMinValue, g_RCAlphaMaxValue, a);
    b = Clip3(g_RCBetaMinValue, g_RCBetaMaxValue, b);
    
    TRCParameter rcPara;
    rcPara.m_a = a;
    rcPara.m_b = b;
    if (QP == g_RCInvalidQPValue && m_encRCSeq->getAdaptiveBits() == 1)
    {
      rcPara.m_validPix = 0;
    }
    else
    {
      rcPara.m_validPix = LCUTotalPixels;
    }

    double MSE = m_LCUs[LCUIdx].m_actualMSE;
    double updatedK = bpp * inputLambda / MSE;
    double updatedC = MSE / pow(bpp, -updatedK);
    rcPara.m_a = updatedC * updatedK;
    rcPara.m_b = -updatedK - 1.0;

    if (bpp > 0 && updatedK > 0.0001)
    {
      m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
    }
    else
    {
      rcPara.m_a = Clip3(0.0001, g_RCAlphaMaxValue, rcPara.m_a);
      m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
    }

    return;
  }

  calLambda = Clip3(inputLambda / 10.0, inputLambda * 10.0, calLambda);
  //////////////////////////////////TODO



  //a += m_encRCSeq->getaUpdate() * (log(inputLambda) - log(calLambda)) * a;
  double lnbpp = log(bpp);
  lnbpp = Clip3(-5.0, -0.1, lnbpp);
  //b += m_encRCSeq->getbUpdate() * (log(inputLambda) - log(calLambda)) * lnbpp;



  a = Clip3(g_RCAlphaMinValue, g_RCAlphaMaxValue, a);
  b = Clip3(g_RCBetaMinValue, g_RCBetaMaxValue, b);

  TRCParameter rcPara;
  rcPara.m_a = a;
  rcPara.m_b = b;
  rcPara.m_c = c;
  if (QP == g_RCInvalidQPValue && m_encRCSeq->getAdaptiveBits() == 1)
  {
    rcPara.m_validPix = 0;
  }
  else
  {
    rcPara.m_validPix = LCUTotalPixels;
  }

  //double MSE = m_LCUs[LCUIdx].m_actualMSE;
  //double updatedK = bpp * inputLambda / MSE;
  //double updatedC = MSE / pow(bpp, -updatedK);
  //rcPara.m_a = updatedC * updatedK;
  //rcPara.m_b = -updatedK - 1.0;
  ////////// mse=a*lnbpp^2+b*lnbpp+c, update a first and then update b
  double MSE = m_LCUs[LCUIdx].m_actualMSE;
  rcPara.m_a = (MSE - rcPara.m_c - rcPara.m_b*log(bpp)) / pow(log(bpp), 2.0);
  rcPara.m_b = (MSE - rcPara.m_c - rcPara.m_a*pow(log(bpp), 2.0)) / log(bpp);
  rcPara.m_c = MSE - rcPara.m_b*log(bpp) - rcPara.m_a*pow(log(bpp), 2.0);
  //if (bpp > 0 && updatedK > 0.0001)
  ////////// TODO
  if (1)
  {
    m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
  }
  else
  {
    rcPara.m_a = Clip3(0.0001, g_RCAlphaMaxValue, rcPara.m_a);
    m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
  }

}
#else
void EncRCPic::updateAfterCTU( int LCUIdx, int bits, int QP, double lambda, bool updateLCUParameter )
{
  m_LCUs[LCUIdx].m_actualBits = bits;
  m_LCUs[LCUIdx].m_QP         = QP;
  m_LCUs[LCUIdx].m_lambda     = lambda;
  m_LCUs[LCUIdx].m_actualSSE  = m_LCUs[LCUIdx].m_actualMSE * m_LCUs[LCUIdx].m_numberOfPixel;

  m_LCULeft--;
  m_bitsLeft   -= bits;
  m_pixelsLeft -= m_LCUs[LCUIdx].m_numberOfPixel;

  if ( !updateLCUParameter )
  {
    return;
  }

  if ( !m_encRCSeq->getUseLCUSeparateModel() )
  {
    return;
  }

  double alpha = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_alpha;
  double beta  = m_encRCSeq->getLCUPara( m_frameLevel, LCUIdx ).m_beta;

  int LCUActualBits   = m_LCUs[LCUIdx].m_actualBits;
  int LCUTotalPixels  = m_LCUs[LCUIdx].m_numberOfPixel;
  double bpp         = ( double )LCUActualBits/( double )LCUTotalPixels;
  double calLambda   = alpha * pow( bpp, beta );
  double inputLambda = m_LCUs[LCUIdx].m_lambda;

  if( inputLambda < 0.01 || calLambda < 0.01 || bpp < 0.0001 )
  {
    alpha *= ( 1.0 - m_encRCSeq->getAlphaUpdate() / 2.0 );
    beta  *= ( 1.0 - m_encRCSeq->getBetaUpdate() / 2.0 );

    alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
    beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );

    TRCParameter rcPara;
    rcPara.m_alpha = alpha;
    rcPara.m_beta  = beta;
    if (QP == g_RCInvalidQPValue && m_encRCSeq->getAdaptiveBits() == 1)
    {
      rcPara.m_validPix = 0;
    }
    else
    {
      rcPara.m_validPix = LCUTotalPixels;
    }

    double MSE = m_LCUs[LCUIdx].m_actualMSE;
    double updatedK = bpp * inputLambda / MSE;
    double updatedC = MSE / pow(bpp, -updatedK);
    rcPara.m_alpha = updatedC * updatedK;
    rcPara.m_beta = -updatedK - 1.0;

    if (bpp > 0 && updatedK > 0.0001)
    {
      m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
    }
    else
    {
      rcPara.m_alpha = Clip3(0.0001, g_RCAlphaMaxValue, rcPara.m_alpha);
      m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
    }

    return;
  }

  calLambda = Clip3( inputLambda / 10.0, inputLambda * 10.0, calLambda );
  alpha += m_encRCSeq->getAlphaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * alpha;
  double lnbpp = log( bpp );
  lnbpp = Clip3( -5.0, -0.1, lnbpp );
  beta  += m_encRCSeq->getBetaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * lnbpp;

  alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
  beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );

  TRCParameter rcPara;
  rcPara.m_alpha = alpha;
  rcPara.m_beta  = beta;
  if (QP == g_RCInvalidQPValue && m_encRCSeq->getAdaptiveBits() == 1)
  {
    rcPara.m_validPix = 0;
  }
  else
  {
    rcPara.m_validPix = LCUTotalPixels;
  }

  double MSE = m_LCUs[LCUIdx].m_actualMSE;
  double updatedK = bpp * inputLambda / MSE;
  double updatedC = MSE / pow(bpp, -updatedK);
  rcPara.m_alpha = updatedC * updatedK;
  rcPara.m_beta = -updatedK - 1.0;

  if (bpp > 0 && updatedK > 0.0001)
  {
    m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
  }
  else
  {
    rcPara.m_alpha = Clip3(0.0001, g_RCAlphaMaxValue, rcPara.m_alpha);
    m_encRCSeq->setLCUPara(m_frameLevel, LCUIdx, rcPara);
  }

}
#endif


double EncRCPic::calAverageQP()
{
  int totalQPs = 0;
  int numTotalLCUs = 0;

  int i;
  for ( i=0; i<m_numberOfLCU; i++ )
  {
    if ( m_LCUs[i].m_QP > 0 )
    {
      totalQPs += m_LCUs[i].m_QP;
      numTotalLCUs++;
    }
  }

  double avgQP = 0.0;
  if ( numTotalLCUs == 0 )
  {
    avgQP = g_RCInvalidQPValue;
  }
  else
  {
    avgQP = ((double)totalQPs) / ((double)numTotalLCUs);
  }
  return avgQP;
}

double EncRCPic::calAverageLambda()
{
  double totalLambdas = 0.0;
  int numTotalLCUs = 0;

  double totalSSE = 0.0;
  int totalPixels = 0;
  int i;
  for ( i=0; i<m_numberOfLCU; i++ )
  {
    if ( m_LCUs[i].m_lambda > 0.01 )
    {
      if (m_LCUs[i].m_QP > 0 || m_encRCSeq->getAdaptiveBits() != 1)
      {
        m_validPixelsInPic += m_LCUs[i].m_numberOfPixel;
        
        totalLambdas += log(m_LCUs[i].m_lambda);
        numTotalLCUs++;
      }

      if (m_LCUs[i].m_QP > 0 || m_encRCSeq->getAdaptiveBits() != 1)
      {
        totalSSE += m_LCUs[i].m_actualSSE;
        totalPixels += m_LCUs[i].m_numberOfPixel;
       }
    }
  }

  setPicMSE(totalPixels > 0 ? totalSSE / (double)totalPixels : 1.0); //1.0 is useless in the following process, just to make sure the divisor not be 0
  double avgLambda;
  if( numTotalLCUs == 0 )
  {
    avgLambda = -1.0;
  }
  else
  {
    avgLambda = pow( 2.7183, totalLambdas / numTotalLCUs );
  }
  return avgLambda;
}



#if USE_R_Lambda_INTRA
void EncRCPic::updateAfterPicture(int actualHeaderBits, int actualTotalBits, double averageQP, double averageLambda, bool isIRAP)
{
  m_picActualHeaderBits = actualHeaderBits;
  m_picActualBits = actualTotalBits;
  if (averageQP > 0.0)
  {
    m_picQP = int(averageQP + 0.5);
  }
  else
  {
    m_picQP = g_RCInvalidQPValue;
  }
  m_picLambda = averageLambda;

  double a = m_encRCSeq->getPicPara(m_frameLevel).m_a;
  double b = m_encRCSeq->getPicPara(m_frameLevel).m_b;
  double c = m_encRCSeq->getPicPara(m_frameLevel).m_c;
  
  if (isIRAP)
  {
    updateAlphaBetaIntra(&a, &b, &c);
  }
  else
  {
    // update parameters
    double picActualBits = (double)m_picActualBits;
    double picActualBpp = picActualBits / (double)m_validPixelsInPic;
    double calLambda = -2*a * log(picActualBpp/3) / picActualBpp - b / picActualBpp;
    double inputLambda = m_picLambda;



    if (inputLambda < 0.01 || calLambda < 0.01 || picActualBpp < 0.0001)
    {
      a *= (1.0 - m_encRCSeq->getaUpdate() / 2.0);
      b *= (1.0 - m_encRCSeq->getbUpdate() / 2.0);

      a = Clip3(g_RCAlphaMinValue, g_RCAlphaMaxValue, a);
      b = Clip3(g_RCBetaMinValue, g_RCBetaMaxValue, b);

      TRCParameter rcPara;
      rcPara.m_a = a;
      rcPara.m_b = b;
      rcPara.m_c = c;
      //////////
      double avgMSE = getPicMSE();
      //double updatedK = picActualBpp * averageLambda / avgMSE;
      //double updatedC = avgMSE / pow(picActualBpp, -updatedK);
      rcPara.m_a = (avgMSE - rcPara.m_c - rcPara.m_b*log(picActualBpp/3)) / pow(log(picActualBpp/3), 2.0);
      rcPara.m_b = (avgMSE - rcPara.m_c - rcPara.m_a*pow(log(picActualBpp/3), 2.0)) / log(picActualBpp/3);
      rcPara.m_c = avgMSE - rcPara.m_b*log(picActualBpp/3) - rcPara.m_a*pow(log(picActualBpp/3), 2.0);


      /* TODO:::: different tid
      if (m_frameLevel > 0)  //only use for level > 0
      {
        rcPara.m_a = updatedC * updatedK;
        rcPara.m_b = -updatedK - 1.0;
      }
      */
      rcPara.m_validPix = m_validPixelsInPic;

      if (m_validPixelsInPic > 0)
      {
        m_encRCSeq->setPicPara(m_frameLevel, rcPara);
      }
      
      return;
    }

    calLambda = Clip3(inputLambda / 10.0, inputLambda * 10.0, calLambda);
    //////////////////////////////////TODO
    a += m_encRCSeq->getaUpdate() * (log(inputLambda) - log(calLambda)) * a;
    double lnbpp = log(picActualBpp);
    lnbpp = Clip3(-5.0, -0.1, lnbpp);

    b += m_encRCSeq->getbUpdate() * (log(inputLambda) - log(calLambda)) * lnbpp;

    ////////// TODO:RANGHE FOR A B C
    a = Clip3(g_RCAlphaMinValue, g_RCAlphaMaxValue, a);
    b = Clip3(g_RCBetaMinValue, g_RCBetaMaxValue, b);
  }
#if PrintTemporalResult  
  printf("\t %f\t %f\t %f\n", a, b, c);
#endif
  TRCParameter rcPara;
  rcPara.m_a = a;
  rcPara.m_b = b;
  rcPara.m_c = c;
  //double picActualBpp = (double)m_picActualBits / (double)m_validPixelsInPic;

  //double avgMSE = getPicMSE();
  //double updatedK = picActualBpp * averageLambda / avgMSE;
  //double updatedC = avgMSE / pow(picActualBpp, -updatedK);

  //double MSE = m_LCUs[LCUIdx].m_actualMSE;

  //rcPara.m_a = (avgMSE - rcPara.m_c - rcPara.m_b*log(picActualBpp)) / pow(log(picActualBpp), 2.0);
  //rcPara.m_b = (avgMSE - rcPara.m_c - rcPara.m_a*pow(log(picActualBpp), 2.0)) / log(picActualBpp);
  //rcPara.m_c = avgMSE - rcPara.m_b*log(picActualBpp) - rcPara.m_a*pow(log(picActualBpp), 2.0);


  ////////// TODO tid
  
  /*
  if (m_frameLevel > 0)  //only use for level > 0
  {
    rcPara.m_a = updatedC * updatedK;
    rcPara.m_b = -updatedK - 1.0;
  }
  */
  rcPara.m_validPix = m_validPixelsInPic;

  if (m_validPixelsInPic > 0)
  {
    m_encRCSeq->setPicPara(m_frameLevel, rcPara);
  }

  if (m_frameLevel == 1)
  {
    double currLambda = Clip3(0.1, 10000.0, m_picLambda);
    double updateLastLambda = g_RCWeightHistoryLambda * m_encRCSeq->getLastLambda() + g_RCWeightCurrentLambda * currLambda;
    m_encRCSeq->setLastLambda(updateLastLambda);
  }
}
#else
void EncRCPic::updateAfterPicture( int actualHeaderBits, int actualTotalBits, double averageQP, double averageLambda, bool isIRAP)
{
  m_picActualHeaderBits = actualHeaderBits;
  m_picActualBits       = actualTotalBits;
  if ( averageQP > 0.0 )
  {
    m_picQP             = int( averageQP + 0.5 );
  }
  else
  {
    m_picQP             = g_RCInvalidQPValue;
  }
  m_picLambda           = averageLambda;

  double alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  double beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;

  if (isIRAP)
  {
    updateAlphaBetaIntra(&alpha, &beta);
  }
  else
  {
    // update parameters
    double picActualBits = ( double )m_picActualBits;
    double picActualBpp = picActualBits / (double)m_validPixelsInPic;
    double calLambda     = alpha * pow( picActualBpp, beta );
    double inputLambda   = m_picLambda;

    if ( inputLambda < 0.01 || calLambda < 0.01 || picActualBpp < 0.0001 )
    {
      alpha *= ( 1.0 - m_encRCSeq->getAlphaUpdate() / 2.0 );
      beta  *= ( 1.0 - m_encRCSeq->getBetaUpdate() / 2.0 );

      alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
      beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );

      TRCParameter rcPara;
      rcPara.m_alpha = alpha;
      rcPara.m_beta  = beta;
      double avgMSE = getPicMSE();
      double updatedK = picActualBpp * averageLambda / avgMSE;
      double updatedC = avgMSE / pow(picActualBpp, -updatedK);

      if (m_frameLevel > 0)  //only use for level > 0
      {
        rcPara.m_alpha = updatedC * updatedK;
        rcPara.m_beta = -updatedK - 1.0;
      }

      rcPara.m_validPix = m_validPixelsInPic;

      if (m_validPixelsInPic > 0)
      {
        m_encRCSeq->setPicPara(m_frameLevel, rcPara);
      }

      return;
    }

    calLambda = Clip3( inputLambda / 10.0, inputLambda * 10.0, calLambda );
    alpha += m_encRCSeq->getAlphaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * alpha;
    double lnbpp = log( picActualBpp );
    lnbpp = Clip3( -5.0, -0.1, lnbpp );

    beta  += m_encRCSeq->getBetaUpdate() * ( log( inputLambda ) - log( calLambda ) ) * lnbpp;

    alpha = Clip3( g_RCAlphaMinValue, g_RCAlphaMaxValue, alpha );
    beta  = Clip3( g_RCBetaMinValue,  g_RCBetaMaxValue,  beta  );
  }

  TRCParameter rcPara;
  rcPara.m_alpha = alpha;
  rcPara.m_beta  = beta;
  double picActualBpp = (double)m_picActualBits / (double)m_validPixelsInPic;

  double avgMSE = getPicMSE();
  double updatedK = picActualBpp * averageLambda / avgMSE;
  double updatedC = avgMSE / pow(picActualBpp, -updatedK);
  if (m_frameLevel > 0)  //only use for level > 0
  {
    rcPara.m_alpha = updatedC * updatedK;
    rcPara.m_beta = -updatedK - 1.0;
  }

  rcPara.m_validPix = m_validPixelsInPic;

  if (m_validPixelsInPic > 0)
  {
    m_encRCSeq->setPicPara(m_frameLevel, rcPara);
  }

  if ( m_frameLevel == 1 )
  {
    double currLambda = Clip3( 0.1, 10000.0, m_picLambda );
    double updateLastLambda = g_RCWeightHistoryLambda * m_encRCSeq->getLastLambda() + g_RCWeightCurrentLambda * currLambda;
    m_encRCSeq->setLastLambda( updateLastLambda );
  }
}
#endif


int EncRCPic::getRefineBitsForIntra( int orgBits )
{
  double alpha=0.25, beta=0.5582;
  int iIntraBits;

  if (orgBits*40 < m_numberOfPixel)
  {
    alpha=0.25;
  }
  else
  {
    alpha=0.30;
  }

  iIntraBits = (int)(alpha* pow(m_totalCostIntra*4.0/(double)orgBits, beta)*(double)orgBits+0.5);

  return iIntraBits;
}

#if USE_R_Lambda_INTRA
double EncRCPic::calculateLambdaIntra(double a, double b, double MADPerPixel, double bitsPerPixel)
{
  /////////////////////////////////////TODO
  //return ((alpha / 256.0) * pow(MADPerPixel / bitsPerPixel, beta));


  double bpp = MADPerPixel / bitsPerPixel/25;
  //double bpp = exp(-b/a)/MADPerPixel * bitsPerPixel*3;
#if PrintTemporalResult 
  printf("%f\t%f\t%f\t", MADPerPixel, bitsPerPixel,bpp);
#endif
  return  -2*a * log(bpp/3) / bpp - b / bpp ;
  //return exp((6 * log2(bpp) + 4 - 13.7122) / 4.2005);
  //return  (-2 * a * log(bitsPerPixel) / bitsPerPixel - b / bitsPerPixel)* ;
  

}
#else
double EncRCPic::calculateLambdaIntra(double alpha, double beta, double MADPerPixel, double bitsPerPixel)
{
  /////////////////////////////////////TODO
#if PrintTemporalResult 
  printf("%f\t%f\t", MADPerPixel, bitsPerPixel);
#endif
  return ( (alpha/256.0) * pow( MADPerPixel/bitsPerPixel, beta ) );
}
#endif


#if USE_R_Lambda_INTRA
void EncRCPic::updateAlphaBetaIntra(double *a, double *b, double *c)
{
  ////////////////////////////////////TODO
  ////////// solve quadratic equation, use the + solution,,m_picMSE  m_totalCostIntra
  /*
  double lnbpp = (-(*b) + sqrt(pow(*b, 2) - 4 * (*a)*((*c) - m_picMSE )))
    / (2 * (*a));
  //double lnbpp = log(pow(m_totalCostIntra / (double)m_numberOfPixel, BETA1));
  double diffLambda = log(m_targetBits)-log(m_picActualBits)
    +log((*a)*log(m_picActualBits)+ (*b) - (*a)*log((double)m_numberOfPixel))
    - log((*a)*log(m_targetBits) + (*b) - (*a) * log((double)m_numberOfPixel));

  ////////// TODO
  diffLambda = Clip3(-0.125, 0.125, 0.25*diffLambda);
  ////////// alpha is updated by lambda ratio
  ////////// beta is updated by accuracy of lnbpp
  */


  /*
  (*a) = (*a) * exp(diffLambda);
  (*b) = (*b) + diffLambda / lnbpp;
  (*c) = m_picMSE  - (*a)*pow(lnbpp, 2.0) - (*b)*lnbpp;
  */
  
  
  //double adjust = exp(-(*b) / (*a)) / (double) m_totalCostIntra * m_numberOfPixel;
  double bpp_real = (double)m_picActualBits / (double)m_numberOfPixel;
  double bpp_comp = (double)m_targetBits / (double)m_numberOfPixel;
 
  /*
  printf("\t%f\t%f\t%f\t%f", bpp_comp, bpp_real, 
    ((2 * (*a)* log(bpp_comp) + (*b)) / (bpp_comp)-(2 * (*a)* log(bpp_real) + (*b)) / (bpp_real))
    * (2 * log(bpp_comp) / (bpp_comp)-2 * log(bpp_real) / (bpp_real)), ((2 * (*a)* log(bpp_comp) + (*b)) / (bpp_comp)
      -(2 * (*a)* log(bpp_real) + (*b)) / (bpp_real))
    * (1 / (bpp_comp)-1 / (bpp_real)));
  */
#if PrintTemporalResult  
  printf("\t%f\t%f\t", bpp_comp, bpp_real);
  printf("\t%f\t%f\t", (-2 * (*a)* log(bpp_comp/3) - (*b)) / (bpp_comp), (-2 * (*a)* log(bpp_real/3)- (*b)) / (bpp_real));
#endif
  
  // k:symmetry axis
  double k = (*b) / 2 / (*a);
  printf("%f\t", k);
  (*a) = ((*a)*((log(bpp_comp/3) + k) / bpp_comp) / ((log(bpp_real/3) + k) / bpp_real));
  (*b) = 2 * (*a)*(k*bpp_real / bpp_comp + (bpp_real*log(bpp_comp) - bpp_comp * log(bpp_real)) / bpp_comp);
  (*c) = m_picMSE - (*a)*pow(log(bpp_real/3), 2) - (*b)*log(bpp_real/3);




  //(*a) = ((2 * bpp_real*(*a)*log(bpp_comp / 3) + (*b)*(bpp_real - bpp_comp)) / (2 * bpp_comp*log(bpp_real / 3)));
  ////(*a) = ((2 * bpp_comp*log(bpp_real / 3)) + (*b)*(bpp_real - bpp_comp)) / (2 * bpp_real*(*a)*log(bpp_comp / 3));
  //(*b) = (*b) *bpp_real / bpp_comp +
  //  (2 * bpp_real*(*a)*(bpp_real*log(bpp_comp / 3) - bpp_comp * log(bpp_real / 3))) / bpp_comp;
  //(*c) = m_picMSE - (*a)*pow(log(bpp_real), 2) - (*b)*log(bpp_real);


  
  
  //(*a) = (*a) - 2 * ((2 * (*a)* log(bpp_comp/3) + (*b)) / (bpp_comp)
  //  -(2 * (*a)* log(bpp_real/3) + (*b)) / (bpp_real))
  //  * (2 * log(bpp_comp/3) / (bpp_comp)) * 0.01;
  //(*b) = (*b) - 2 * ((2 * (*a)* log(bpp_comp) + (*b)) / (bpp_comp) -(2 * (*a)* log(bpp_real) )* (1 / (bpp_comp))) * 0.01 ;
  //(*c) = m_picMSE - (*a)*pow(log(bpp_real), 2) - (*b)*log(bpp_real);
  //

  

  
  //m_encRCSeq->m_MSEprepre = m_encRCSeq->m_MSEpre;
  //m_encRCSeq->m_MSEpre = m_encRCSeq->m_MSEcur;
  //m_encRCSeq->m_MSEcur = m_picMSE;
  //m_encRCSeq->m_bitsprepre = m_encRCSeq->m_bitspre;
  //m_encRCSeq->m_bitspre = m_encRCSeq->m_bitscur;
  //m_encRCSeq->m_bitscur = (double)m_picActualBits / m_numberOfPixel;
  //m_encRCSeq->m_update_times += 1;

  ////double bpp_real = (double)m_picActualBits / (double)m_numberOfPixel;
  ////double bpp_comp = (double)m_targetBits / (double)m_numberOfPixel;
  ////printf("\t %f\t %f\t %f\n", *a, *b, *c);
  //if (m_encRCSeq->m_update_times==3)
  //{
  //  double x1 = log(m_encRCSeq->m_bitscur);
  //  double x2 = log(m_encRCSeq->m_bitspre);
  //  double x3 = log(m_encRCSeq->m_bitsprepre);
  //  double D1 = m_encRCSeq->m_MSEcur;
  //  double D2 = m_encRCSeq->m_MSEpre;
  //  double D3 = m_encRCSeq->m_MSEprepre;
  //  (*b) = ((D1 - D2)*(pow(x1, 2) - pow(x3, 2)) / (pow(x1, 2) - pow(x2, 2)) - (D1 - D3)) 
  //    / ((pow(x1, 2) - pow(x3, 2)) / (x1 + x2) - (x1 - x3));
  //  (*a) = ((D1 - D2) - (*b)*(x1 - x2)) / (pow(x1, 2) - pow(x2, 2));
  //  (*c) = D1 - (*a)*pow(x1, 2) - (*b)*x1;
  //}
  //else
  //{
  //  /*
  //  printf("\t %f\t %f\t %f\n", *a, *b, *c);
  //  (*a) = (*a) - 2 * ((2 * (*a)* log(bpp_comp) + (*b)) / (bpp_comp)
  //    -(2 * (*a)* log(bpp_real) + (*b)) / (bpp_real))
  //    * (2 * log(bpp_comp) / (bpp_comp)-2 * log(bpp_real) / (bpp_real)) * 10;
  //  (*b) = (*b) - 2 * ((2 * (*a)* log(bpp_comp) + (*b)) / (bpp_comp)
  //    -(2 * (*a)* log(bpp_real) + (*b)) / (bpp_real))
  //    * (1 / (bpp_comp)-1 / (bpp_real)) * 2.5;
  //  (*c) = m_picMSE - (*a)*pow(log(bpp_real), 2) - (*b)*log(bpp_real); 
  //  */
  //  (*a) = ((2 * bpp_real*(*a)*log(bpp_comp / 3) + (*b)*(bpp_real - bpp_comp)) / (2 * bpp_comp*log(bpp_real / 3)));
  //  //(*a) = ((2 * bpp_comp*log(bpp_real / 3)) + (*b)*(bpp_real - bpp_comp)) / (2 * bpp_real*(*a)*log(bpp_comp / 3));
  //  (*b) = (*b) *bpp_real / bpp_comp +
  //    (2 * bpp_real*(*a)*(bpp_real*log(bpp_comp / 3) - bpp_comp * log(bpp_real / 3))) / bpp_comp;
  //  (*c) = m_picMSE - (*a)*pow(log(bpp_real), 2) - (*b)*log(bpp_real);
  //}
  ////printf("\n %f\t %f\t %f\t %f\t %f\t %f", m_encRCSeq->m_bitscur*m_numberOfPixel, m_encRCSeq->m_bitspre*m_numberOfPixel, m_encRCSeq->m_bitsprepre*m_numberOfPixel, m_encRCSeq->m_MSEcur, m_encRCSeq->m_MSEpre, m_encRCSeq->m_MSEprepre);
  ////printf("\t %f\t %f\t %f\n", *a, *b, *c);
  //

}
#else

void EncRCPic::updateAlphaBetaIntra(double *alpha, double *beta)
{
  ////////////////////////////////////TODO
  double lnbpp = log(pow(m_totalCostIntra / (double)m_numberOfPixel, BETA1));
  double diffLambda = (*beta)*(log((double)m_picActualBits)-log((double)m_targetBits));

  ////////// TODO
  diffLambda = Clip3(-0.125, 0.125, 0.25*diffLambda);
  double bpp_real = (double)m_picActualBits / (double)m_numberOfPixel;
  double bpp_comp = (double)m_targetBits / (double)m_numberOfPixel;
#if PrintTemporalResult  
  printf("\t%f\t%f\t", (*alpha)*pow(bpp_comp,*beta), (*alpha)*pow(bpp_real, *beta));
#endif
  ////////// alpha is updated by lambda ratio
  ////////// beta is updated by accuracy of lnbpp
  *alpha    =  (*alpha) * exp(diffLambda);
  *beta     =  (*beta) + diffLambda / lnbpp;
#if PrintTemporalResult  
  printf("%f\t%f\t", *alpha, *beta);
#endif
  
}
#endif

void EncRCPic::getLCUInitTargetBits()
{
  int iAvgBits     = 0;

  m_remainingCostIntra = m_totalCostIntra;
  for (int i=m_numberOfLCU-1; i>=0; i--)
  {
    iAvgBits += int(m_targetBits * getLCU(i).m_costIntra/m_totalCostIntra);
    getLCU(i).m_targetBitsLeft = iAvgBits;
  }
}


#if USE_R_Lambda_INTRA
double EncRCPic::getLCUEstLambdaAndQP(double bpp, int clipPicQP, int *estQP)
{
  int   LCUIdx = getLCUCoded();

  double   a = m_encRCSeq->getPicPara(m_frameLevel).m_a;
  double   b = m_encRCSeq->getPicPara(m_frameLevel).m_b;

  double costPerPixel = getLCU(LCUIdx).m_costIntra / (double)getLCU(LCUIdx).m_numberOfPixel;
  //////////////////////////////?????
  costPerPixel = pow(costPerPixel, BETA1);
  double estLambda = calculateLambdaIntra(a, b, costPerPixel, bpp);

  int clipNeighbourQP = g_RCInvalidQPValue;
  for (int i = LCUIdx - 1; i >= 0; i--)
  {
    if ((getLCU(i)).m_QP > g_RCInvalidQPValue)
    {
      clipNeighbourQP = getLCU(i).m_QP;
      break;
    }
  }

  int minQP = clipPicQP - 2;
  int maxQP = clipPicQP + 2;

  if (clipNeighbourQP > g_RCInvalidQPValue)
  {
    maxQP = min(clipNeighbourQP + 1, maxQP);
    minQP = max(clipNeighbourQP - 1, minQP);
  }

  int bitdepth_luma_scale =
    2
    * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  double maxLambda = exp(((double)(maxQP + 0.49) - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);
  double minLambda = exp(((double)(minQP - 0.49) - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);

  estLambda = Clip3(minLambda, maxLambda, estLambda);

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  *estQP = int(4.2005 * log(estLambda / pow(2.0, bitdepth_luma_scale)) + 13.7122 + 0.5);
  *estQP = Clip3(minQP, maxQP, *estQP);

  return estLambda;
}
#else
double EncRCPic::getLCUEstLambdaAndQP(double bpp, int clipPicQP, int *estQP)
{
  int   LCUIdx = getLCUCoded();

  double   alpha = m_encRCSeq->getPicPara( m_frameLevel ).m_alpha;
  double   beta  = m_encRCSeq->getPicPara( m_frameLevel ).m_beta;

  double costPerPixel = getLCU(LCUIdx).m_costIntra/(double)getLCU(LCUIdx).m_numberOfPixel;
  costPerPixel = pow(costPerPixel, BETA1);
  double estLambda = calculateLambdaIntra(alpha, beta, costPerPixel, bpp);

  int clipNeighbourQP = g_RCInvalidQPValue;
  for (int i=LCUIdx-1; i>=0; i--)
  {
    if ((getLCU(i)).m_QP > g_RCInvalidQPValue)
    {
      clipNeighbourQP = getLCU(i).m_QP;
      break;
    }
  }

  int minQP = clipPicQP - 2;
  int maxQP = clipPicQP + 2;

  if ( clipNeighbourQP > g_RCInvalidQPValue )
  {
    maxQP = min(clipNeighbourQP + 1, maxQP);
    minQP = max(clipNeighbourQP - 1, minQP);
  }

  int bitdepth_luma_scale =
    2
    * (m_encRCSeq->getbitDepth() - 8
      - DISTORTION_PRECISION_ADJUSTMENT(m_encRCSeq->getbitDepth()));

  double maxLambda = exp(((double)(maxQP + 0.49) - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);
  double minLambda = exp(((double)(minQP - 0.49) - 13.7122) / 4.2005) * pow(2.0, bitdepth_luma_scale);

  estLambda = Clip3(minLambda, maxLambda, estLambda);

  //Avoid different results in different platforms. The problem is caused by the different results of pow() in different platforms.
  estLambda = double(int64_t(estLambda * (double)LAMBDA_PREC + 0.5)) / (double)LAMBDA_PREC;
  *estQP = int(4.2005 * log(estLambda / pow(2.0, bitdepth_luma_scale)) + 13.7122 + 0.5);
  *estQP = Clip3(minQP, maxQP, *estQP);

  return estLambda;
}
#endif



RateCtrl::RateCtrl()
{
  m_encRCSeq = NULL;
  m_encRCGOP = NULL;
  m_encRCPic = NULL;
}

RateCtrl::~RateCtrl()
{
  destroy();
}

void RateCtrl::destroy()
{
  if ( m_encRCSeq != NULL )
  {
    delete m_encRCSeq;
    m_encRCSeq = NULL;
  }
  if ( m_encRCGOP != NULL )
  {
    delete m_encRCGOP;
    m_encRCGOP = NULL;
  }
  while ( m_listRCPictures.size() > 0 )
  {
    EncRCPic* p = m_listRCPictures.front();
    m_listRCPictures.pop_front();
    delete p;
  }
}

void RateCtrl::init(int totalFrames, int targetBitrate, int frameRate, int GOPSize, int picWidth, int picHeight, int LCUWidth, int LCUHeight, int bitDepth, int keepHierBits, bool useLCUSeparateModel, GOPEntry  GOPList[MAX_GOP])
{
  destroy();

  bool isLowdelay = true;
  for ( int i=0; i<GOPSize-1; i++ )
  {
    if ( GOPList[i].m_POC > GOPList[i+1].m_POC )
    {
      isLowdelay = false;
      break;
    }
  }

  int numberOfLevel = 1;
  int adaptiveBit = 0;
  if ( keepHierBits > 0 )
  {
    numberOfLevel = int( log((double)GOPSize)/log(2.0) + 0.5 ) + 1;
  }
  if (!isLowdelay && (GOPSize == 16 || GOPSize == 8))
  {
    numberOfLevel = int( log((double)GOPSize)/log(2.0) + 0.5 ) + 1;
  }
  numberOfLevel++;    // intra picture
  numberOfLevel++;    // non-reference picture


  int* bitsRatio;
  bitsRatio = new int[ GOPSize ];
  for ( int i=0; i<GOPSize; i++ )
  {
    bitsRatio[i] = 10;
    if ( !GOPList[i].m_refPic )
    {
      bitsRatio[i] = 2;
    }
  }

  if ( keepHierBits > 0 )
  {
    double bpp = (double)( targetBitrate / (double)( frameRate*picWidth*picHeight ) );
    if ( GOPSize == 4 && isLowdelay )
    {
      if ( bpp > 0.2 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 6;
      }
      else if( bpp > 0.1 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 10;
      }
      else if ( bpp > 0.05 )
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 12;
      }
      else
      {
        bitsRatio[0] = 2;
        bitsRatio[1] = 3;
        bitsRatio[2] = 2;
        bitsRatio[3] = 14;
      }

      if ( keepHierBits == 2 )
      {
        adaptiveBit = 1;
      }
    }
    else if ( GOPSize == 8 && !isLowdelay )
    {
      if ( bpp > 0.2 )
      {
        bitsRatio[0] = 15;
        bitsRatio[1] = 5;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else if ( bpp > 0.1 )
      {
        bitsRatio[0] = 20;
        bitsRatio[1] = 6;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else if ( bpp > 0.05 )
      {
        bitsRatio[0] = 25;
        bitsRatio[1] = 7;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }
      else
      {
        bitsRatio[0] = 30;
        bitsRatio[1] = 8;
        bitsRatio[2] = 4;
        bitsRatio[3] = 1;
        bitsRatio[4] = 1;
        bitsRatio[5] = 4;
        bitsRatio[6] = 1;
        bitsRatio[7] = 1;
      }

      if ( keepHierBits == 2 )
      {
        adaptiveBit = 2;
      }
    }
    else if (GOPSize == 16 && !isLowdelay)
    {
      if (bpp > 0.2)
      {
        bitsRatio[0] = 10;
        bitsRatio[1] = 8;
        bitsRatio[2] = 4;
        bitsRatio[3] = 2;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 2;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 4;
        bitsRatio[10] = 2;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 2;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }
      else if (bpp > 0.1)
      {
        bitsRatio[0] = 15;
        bitsRatio[1] = 9;
        bitsRatio[2] = 4;
        bitsRatio[3] = 2;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 2;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 4;
        bitsRatio[10] = 2;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 2;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }
      else if (bpp > 0.05)
      {
        bitsRatio[0] = 40;
        bitsRatio[1] = 17;
        bitsRatio[2] = 7;
        bitsRatio[3] = 2;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 2;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 7;
        bitsRatio[10] = 2;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 2;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }
      else
      {
        bitsRatio[0] = 40;
        bitsRatio[1] = 15;
        bitsRatio[2] = 6;
        bitsRatio[3] = 3;
        bitsRatio[4] = 1;
        bitsRatio[5] = 1;
        bitsRatio[6] = 3;
        bitsRatio[7] = 1;
        bitsRatio[8] = 1;
        bitsRatio[9] = 6;
        bitsRatio[10] = 3;
        bitsRatio[11] = 1;
        bitsRatio[12] = 1;
        bitsRatio[13] = 3;
        bitsRatio[14] = 1;
        bitsRatio[15] = 1;
      }

      if (keepHierBits == 2)
      {
        adaptiveBit = 3;
      }
    }
    else
    {
      msg( WARNING, "\n hierarchical bit allocation is not support for the specified coding structure currently.\n" );
    }
  }

  int* GOPID2Level = new int[ GOPSize ];
  for ( int i=0; i<GOPSize; i++ )
  {
    GOPID2Level[i] = 1;
    if ( !GOPList[i].m_refPic )
    {
      GOPID2Level[i] = 2;
    }
  }

  if ( keepHierBits > 0 )
  {
    if ( GOPSize == 4 && isLowdelay )
    {
      GOPID2Level[0] = 3;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 1;
    }
    else if ( GOPSize == 8 && !isLowdelay )
    {
      GOPID2Level[0] = 1;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 4;
      GOPID2Level[4] = 4;
      GOPID2Level[5] = 3;
      GOPID2Level[6] = 4;
      GOPID2Level[7] = 4;
    }
    else if (GOPSize == 16 && !isLowdelay)
    {
      GOPID2Level[0] = 1;
      GOPID2Level[1] = 2;
      GOPID2Level[2] = 3;
      GOPID2Level[3] = 4;
      GOPID2Level[4] = 5;
      GOPID2Level[5] = 5;
      GOPID2Level[6] = 4;
      GOPID2Level[7] = 5;
      GOPID2Level[8] = 5;
      GOPID2Level[9] = 3;
      GOPID2Level[10] = 4;
      GOPID2Level[11] = 5;
      GOPID2Level[12] = 5;
      GOPID2Level[13] = 4;
      GOPID2Level[14] = 5;
      GOPID2Level[15] = 5;
    }
  }

  if ( !isLowdelay && GOPSize == 8 )
  {
    GOPID2Level[0] = 1;
    GOPID2Level[1] = 2;
    GOPID2Level[2] = 3;
    GOPID2Level[3] = 4;
    GOPID2Level[4] = 4;
    GOPID2Level[5] = 3;
    GOPID2Level[6] = 4;
    GOPID2Level[7] = 4;
  }
  else if (GOPSize == 16 && !isLowdelay)
  {
    GOPID2Level[0] = 1;
    GOPID2Level[1] = 2;
    GOPID2Level[2] = 3;
    GOPID2Level[3] = 4;
    GOPID2Level[4] = 5;
    GOPID2Level[5] = 5;
    GOPID2Level[6] = 4;
    GOPID2Level[7] = 5;
    GOPID2Level[8] = 5;
    GOPID2Level[9] = 3;
    GOPID2Level[10] = 4;
    GOPID2Level[11] = 5;
    GOPID2Level[12] = 5;
    GOPID2Level[13] = 4;
    GOPID2Level[14] = 5;
    GOPID2Level[15] = 5;
  }

  m_encRCSeq = new EncRCSeq;
  m_encRCSeq->create( totalFrames, targetBitrate, frameRate, GOPSize, picWidth, picHeight, LCUWidth, LCUHeight, numberOfLevel, useLCUSeparateModel, adaptiveBit );
  m_encRCSeq->initBitsRatio( bitsRatio );
  m_encRCSeq->initGOPID2Level( GOPID2Level );
  m_encRCSeq->setBitDepth(bitDepth);
  m_encRCSeq->initPicPara();
  if ( useLCUSeparateModel )
  {
    m_encRCSeq->initLCUPara();
  }
#if U0132_TARGET_BITS_SATURATION
  m_CpbSaturationEnabled = false;
  m_cpbSize              = targetBitrate;
  m_cpbState             = (uint32_t)(m_cpbSize*0.5f);
  m_bufferingRate        = (int)(targetBitrate / frameRate);
#endif

  delete[] bitsRatio;
  delete[] GOPID2Level;
}

void RateCtrl::initRCPic( int frameLevel )
{
  m_encRCPic = new EncRCPic;
  m_encRCPic->create( m_encRCSeq, m_encRCGOP, frameLevel, m_listRCPictures );
}

void RateCtrl::initRCGOP( int numberOfPictures )
{
  m_encRCGOP = new EncRCGOP;
  m_encRCGOP->create( m_encRCSeq, numberOfPictures );
}

#if U0132_TARGET_BITS_SATURATION
int  RateCtrl::updateCpbState(int actualBits)
{
  int cpbState = 1;

  m_cpbState -= actualBits;
  if (m_cpbState < 0)
  {
    cpbState = -1;
  }

  m_cpbState += m_bufferingRate;
  if (m_cpbState > m_cpbSize)
  {
    cpbState = 0;
  }

  return cpbState;
}

void RateCtrl::initHrdParam(const HRD* pcHrd, int iFrameRate, double fInitialCpbFullness)
{
  m_CpbSaturationEnabled = true;
  m_cpbSize = (pcHrd->getCpbSizeValueMinus1(0, 0, 0) + 1) << (4 + pcHrd->getCpbSizeScale());
  m_cpbState = (uint32_t)(m_cpbSize*fInitialCpbFullness);
  m_bufferingRate = (uint32_t)(((pcHrd->getBitRateValueMinus1(0, 0, 0) + 1) << (6 + pcHrd->getBitRateScale())) / iFrameRate);
  msg( NOTICE, "\nHRD - [Initial CPB state %6d] [CPB Size %6d] [Buffering Rate %6d]\n", m_cpbState, m_cpbSize, m_bufferingRate);
}
#endif

void RateCtrl::destroyRCGOP()
{
  delete m_encRCGOP;
  m_encRCGOP = NULL;
}
