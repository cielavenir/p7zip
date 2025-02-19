// (C) 2016 Tino Reichardt

#define LZ5_STATIC_LINKING_ONLY
#include "../../../C/Alloc.h"
#include "../../../C/Threads.h"
#include "../../../Codecs/lz5/lib/lz5.h"
#include "../../../Codecs/zstdmt/lib/lz5-mt.h"

#include "../../Windows/System.h"
#include "../../Common/Common.h"
#include "../../Common/MyCom.h"
#include "../ICoder.h"
#include "../Common/StreamUtils.h"
#include "../Common/RegisterCodec.h"
#include "../Common/ProgressMt.h"

#include "../../../C/7zVersion.h"
#if MY_VER_MAJOR >= 23
#define OVERRIDE override
#define MY_QUERYINTERFACE_BEGIN2 Z7_COM_QI_BEGIN2
#define MY_QUERYINTERFACE_ENTRY Z7_COM_QI_ENTRY
#define MY_QUERYINTERFACE_END Z7_COM_QI_END
#define MY_ADDREF_RELEASE Z7_COM_ADDREF_RELEASE
#else
#define OVERRIDE
#endif

struct Lz5Stream
{
  ISequentialInStream *inStream;
  ISequentialOutStream *outStream;
  ICompressProgressInfo *progress;
  UInt64 *processedIn;
  UInt64 *processedOut;
};

extern int Lz5Read(void *Stream, LZ5MT_Buffer *in);
extern int Lz5Write(void *Stream, LZ5MT_Buffer *in);

namespace NCompress
{
  namespace NLZ5
  {

    struct DProps
    {
      DProps() { clear(); }
      void clear()
      {
        memset(this, 0, sizeof(*this));
        _ver_major = LZ5_VERSION_MAJOR;
        _ver_minor = LZ5_VERSION_MINOR;
        _level = 1;
      }

      Byte _ver_major;
      Byte _ver_minor;
      Byte _level;
      Byte _reserved[2];
    };

    class CDecoder : public ICompressCoder,
                     public ICompressSetDecoderProperties2,
                     public ICompressSetOutStreamSize,
                     #ifndef NO_READ_FROM_CODER
                     public ICompressSetInStream,
                     #endif
                     public ICompressSetCoderMt,
                     public CMyUnknownImp
    {
      CMyComPtr<ISequentialInStream> _inStream;

      DProps _props;

      UInt64 _processedIn;
      UInt64 _processedOut;
      UInt32 _inputSize;
      UInt32 _numThreads;

      HRESULT CodeSpec(ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress);
      HRESULT SetOutStreamSizeResume(const UInt64 *outSize);

    public:
      MY_QUERYINTERFACE_BEGIN2(ICompressCoder)
      MY_QUERYINTERFACE_ENTRY(ICompressSetDecoderProperties2)
#ifndef NO_READ_FROM_CODER
      MY_QUERYINTERFACE_ENTRY(ICompressSetInStream)
#endif
      MY_QUERYINTERFACE_ENTRY(ICompressSetCoderMt)
      MY_QUERYINTERFACE_END
      MY_ADDREF_RELEASE

    public:
      STDMETHOD(Code)
      (ISequentialInStream *inStream, ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress) noexcept OVERRIDE;
      STDMETHOD(SetDecoderProperties2)
      (const Byte *data, UInt32 size) noexcept OVERRIDE;
      STDMETHOD(SetOutStreamSize)
      (const UInt64 *outSize) noexcept OVERRIDE;
      STDMETHOD(SetNumberOfThreads)
      (UInt32 numThreads) noexcept OVERRIDE;

#ifndef NO_READ_FROM_CODER
      STDMETHOD(SetInStream)
      (ISequentialInStream *inStream) noexcept OVERRIDE;
      STDMETHOD(ReleaseInStream)
      () noexcept OVERRIDE;
      UInt64 GetInputProcessedSize() const { return _processedIn; }
#endif
      HRESULT CodeResume(ISequentialOutStream *outStream, const UInt64 *outSize, ICompressProgressInfo *progress);

      CDecoder();
      virtual ~CDecoder();
    };

  }
}
