// ImplodeEncoder.h

#ifndef __COMPRESS_PKIMPLODE_ENCODER_H
#define __COMPRESS_PKIMPLODE_ENCODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/InBuffer.h"

extern "C" {
#include "../../../Codecs/StormLib/src/pklib/pklib.h"
}

#ifndef MY_VER_MAJOR
#include "../../../C/7zVersion.h"
#endif
#if MY_VER_MAJOR >= 23
#define OVERRIDE override
#define MY_UNKNOWN_IMP2 Z7_COM_UNKNOWN_IMP_2
#else
#define OVERRIDE
#endif

namespace NCompress {
namespace NPKImplode {
namespace NEncoder {

class CEncoder:
  public ICompressCoder,
  public ICompressGetInStreamProcessedSize,
  public ICompressSetCoderProperties,
  public CMyUnknownImp
{
  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

public:
  UInt64 processedIn;
  UInt64 processedOut;
  ISequentialInStream *inS;
  ISequentialOutStream *outS;
  ICompressProgressInfo *progr;

  unsigned int typ;
  unsigned int dsize;
  TCmpStruct CmpStruct;

  MY_UNKNOWN_IMP2(ICompressGetInStreamProcessedSize, ICompressSetCoderProperties)

public:
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress) noexcept OVERRIDE;
  STDMETHOD (SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps) noexcept OVERRIDE;
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value) noexcept OVERRIDE;

  CEncoder();
  virtual ~CEncoder() = default;
};

}}}

#endif
