// Lz4Handler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../Common/ComTry.h"
#include "../../Common/Defs.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/Lz4Decoder.h"
#include "../Compress/Lz4Encoder.h"
#include "../Compress/CopyCoder.h"

#include "Common/DummyOutStream.h"
#include "Common/HandlerOut.h"

#include "../../../C/7zVersion.h"
#if MY_VER_MAJOR >= 23
#define OVERRIDE override
#define MY_UNKNOWN_IMP4 Z7_COM_UNKNOWN_IMP_4
#endif

using namespace NWindows;

namespace NArchive {
namespace NLZ4 {

class CHandler:
  public IInArchive,
  public IArchiveOpenSeq,
  public IOutArchive,
  public ISetProperties,
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _stream;
  CMyComPtr<ISequentialInStream> _seqStream;

  bool _isArc;
  bool _dataAfterEnd;
  bool _needMoreInput;

  bool _packSize_Defined;
  bool _unpackSize_Defined;

  UInt64 _packSize;
  UInt64 _unpackSize;
  UInt64 _numStreams;
  UInt64 _numBlocks;

  CSingleMethodProps _props;

public:
  MY_UNKNOWN_IMP4(
      IInArchive,
      IArchiveOpenSeq,
      IOutArchive,
      ISetProperties)
#if MY_VER_MAJOR >= 23
  Z7_IFACE_COM7_IMP(IInArchive)
  Z7_IFACE_COM7_IMP(IOutArchive)
#else
  INTERFACE_IInArchive(noexcept;)
  INTERFACE_IOutArchive(noexcept;)
#endif

public:
  STDMETHOD(OpenSeq)(ISequentialInStream *stream) noexcept OVERRIDE;
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps) noexcept OVERRIDE;

  CHandler() { }
  virtual ~CHandler() = default;
};

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize
};

static const Byte kArcProps[] =
{
  kpidNumStreams,
  kpidNumBlocks
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID /*propID*/, PROPVARIANT * /*value*/) noexcept
{
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems) noexcept
{
  *numItems = 1;
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value) noexcept
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPackSize: if (_packSize_Defined) prop = _packSize; break;
    case kpidSize: if (_unpackSize_Defined) prop = _unpackSize; break;
  }
  prop.Detach(value);
  return S_OK;
}

static const unsigned kSignatureCheckSize = 4;

API_FUNC_static_IsArc IsArc_lz4(const Byte *p, size_t size)
{
  if (size < 4)
    return k_IsArc_Res_NEED_MORE;

  UInt32 magic = GetUi32(p);

  // skippable frames
  if (magic >= 0x184D2A50 && magic <= 0x184D2A5F) {
    if (size < 16)
      return k_IsArc_Res_NEED_MORE;
    magic = GetUi32(p+12);
  }

  // lz4 magic
  if (magic == 0x184D2204)
    return k_IsArc_Res_YES;

  return k_IsArc_Res_NO;
}
}

STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *) noexcept
{
  COM_TRY_BEGIN
  Close();
  {
    Byte buf[kSignatureCheckSize];
    RINOK(ReadStream_FALSE(stream, buf, kSignatureCheckSize));
    if (IsArc_lz4(buf, kSignatureCheckSize) == k_IsArc_Res_NO)
      return S_FALSE;
    _isArc = true;
    _stream = stream;
    _seqStream = stream;
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::OpenSeq(ISequentialInStream *stream) noexcept
{
  Close();
  _isArc = true;
  _seqStream = stream;
  return S_OK;
}

STDMETHODIMP CHandler::Close() noexcept
{
  _isArc = false;
  _dataAfterEnd = false;
  _needMoreInput = false;

  _packSize_Defined = false;
  _unpackSize_Defined = false;

  _packSize = 0;

  _seqStream.Release();
  _stream.Release();
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback) noexcept
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  if (_packSize_Defined)
    extractCallback->SetTotal(_packSize);

  CMyComPtr<ISequentialOutStream> realOutStream;
  Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode));
  if (!testMode && !realOutStream)
    return S_OK;

  extractCallback->PrepareOperation(askMode);

  Int32 opRes;

  {

  NCompress::NLZ4::CDecoder *decoderSpec = new NCompress::NLZ4::CDecoder;
  CMyComPtr<ICompressCoder> decoder = decoderSpec;
  decoderSpec->SetInStream(_seqStream);

  CDummyOutStream *outStreamSpec = new CDummyOutStream;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
  outStreamSpec->SetStream(realOutStream);
  outStreamSpec->Init();

  realOutStream.Release();

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, true);

  UInt64 packSize = 0;
  UInt64 unpackedSize = 0;

  HRESULT result = S_OK;

  for (;;)
  {
    lps->InSize = packSize;
    lps->OutSize = unpackedSize;

    RINOK(lps->SetCur());
    result = decoderSpec->CodeResume(outStream, &unpackedSize, progress);
    UInt64 streamSize = decoderSpec->GetInputProcessedSize();

    if (result != S_FALSE && result != S_OK)
      return result;

    if (unpackedSize == 0)
      break;

    if (streamSize == packSize)
    {
      // no new bytes in input stream, So it's good end of archive.
      result = S_OK;
      break;
    }

    if (packSize > streamSize)
      return E_FAIL;

    if (result != S_OK)
      break;
  }

  decoderSpec->ReleaseInStream();
  outStream.Release();

  if (!_isArc)
    opRes = NExtract::NOperationResult::kIsNotArc;
  else if (_needMoreInput)
    opRes = NExtract::NOperationResult::kUnexpectedEnd;
  else if (_dataAfterEnd)
    opRes = NExtract::NOperationResult::kDataAfterEnd;
  else if (result == S_FALSE)
    opRes = NExtract::NOperationResult::kDataError;
  else if (result == S_OK)
    opRes = NExtract::NOperationResult::kOK;
  else
    return result;

  }

  return extractCallback->SetOperationResult(opRes);

  COM_TRY_END
}

static HRESULT UpdateArchive(
    UInt64 unpackSize,
    ISequentialOutStream *outStream,
    const CProps &props,
    IArchiveUpdateCallback *updateCallback)
{
  RINOK(updateCallback->SetTotal(unpackSize));
  CMyComPtr<ISequentialInStream> fileInStream;
  RINOK(updateCallback->GetStream(0, &fileInStream));
  CLocalProgress *localProgressSpec = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> localProgress = localProgressSpec;
  localProgressSpec->Init(updateCallback, true);
  NCompress::NLZ4::CEncoder *encoderSpec = new NCompress::NLZ4::CEncoder;
  CMyComPtr<ICompressCoder> encoder = encoderSpec;
  RINOK(props.SetCoderProps(encoderSpec, NULL));
  RINOK(encoder->Code(fileInStream, outStream, NULL, NULL, localProgress));
  return updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK);
}

STDMETHODIMP CHandler::GetFileTimeType(UInt32 *type) noexcept
{
  *type = NFileTimeType::kUnix;
  return S_OK;
}

STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *updateCallback) noexcept
{
  COM_TRY_BEGIN

  if (numItems != 1)
    return E_INVALIDARG;

  Int32 newData, newProps;
  UInt32 indexInArchive;
  if (!updateCallback)
    return E_FAIL;
  RINOK(updateCallback->GetUpdateItemInfo(0, &newData, &newProps, &indexInArchive));
 
  if ((newProps))
  {
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidIsDir, &prop));
      if (prop.vt != VT_EMPTY)
        if (prop.vt != VT_BOOL || prop.boolVal != VARIANT_FALSE)
          return E_INVALIDARG;
    }
  }
  
  if ((newData))
  {
    UInt64 size;
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
    }
    return UpdateArchive(size, outStream, _props, updateCallback);
  }

  if (indexInArchive != 0)
    return E_INVALIDARG;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);

  CMyComPtr<IArchiveUpdateCallbackFile> opCallback;
  updateCallback->QueryInterface(IID_IArchiveUpdateCallbackFile, (void **)&opCallback);
  if (opCallback)
  {
    RINOK(opCallback->ReportOperation(
        NEventIndexType::kInArcIndex, 0,
        NUpdateNotifyOp::kReplicate))
  }

  if (_stream)
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));

  return NCompress::CopyStream(_stream, outStream, progress);

  COM_TRY_END
}

STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps) noexcept
{
  return _props.SetProperties(names, values, numProps);
}

static const Byte k_Signature[] = "0x184D2204";

REGISTER_ARC_IO(
  "lz4", "lz4 tlz4", "* .tar", 0x0f,
  k_Signature,
  0,
  NArcInfoFlags::kKeepName,
  0,
  IsArc_lz4)

}}
