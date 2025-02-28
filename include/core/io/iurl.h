/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iurl.h
/// @brief   provide a comprehensive and flexible way to represent and manipulate URLs.
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IURL_H
#define IURL_H

#include <core/global/iglobal.h>
#include <core/utils/ibytearray.h>
#include <core/utils/istring.h>

namespace iShell {

class iUrlPrivate;

template <typename E1, typename E2>
class iUrlTwoFlags
{
    int i;
    typedef int iUrlTwoFlags:: *Zero;
public:
    inline iUrlTwoFlags(E1 f) : i(f) {}
    inline iUrlTwoFlags(E2 f) : i(f) {}
    inline iUrlTwoFlags(int f) : i(f) {}
    inline iUrlTwoFlags(Zero = 0) : i(0) {}

    inline iUrlTwoFlags &operator&=(int mask) { i &= mask; return *this; }
    inline iUrlTwoFlags &operator&=(uint mask) { i &= mask; return *this; }
    inline iUrlTwoFlags &operator|=(iUrlTwoFlags f) { i |= f.i; return *this; }
    inline iUrlTwoFlags &operator|=(E1 f) { i |= f; return *this; }
    inline iUrlTwoFlags &operator|=(E2 f) { i |= f; return *this; }
    inline iUrlTwoFlags &operator^=(iUrlTwoFlags f) { i ^= f.i; return *this; }
    inline iUrlTwoFlags &operator^=(E1 f) { i ^= f; return *this; }
    inline iUrlTwoFlags &operator^=(E2 f) { i ^= f; return *this; }

    inline operator int() const { return i; }
    inline bool operator!() const { return !i; }

    inline iUrlTwoFlags operator|(iUrlTwoFlags f) const
    { return iUrlTwoFlags(uint(i | f.i)); }
    inline iUrlTwoFlags operator|(E1 f) const
    { return iUrlTwoFlags(uint(i | uint(f))); }
    inline iUrlTwoFlags operator|(E2 f) const
    { return iUrlTwoFlags(uint(i | uint(f))); }
    inline iUrlTwoFlags operator^(iUrlTwoFlags f) const
    { return iUrlTwoFlags(uint(i ^ f.i)); }
    inline iUrlTwoFlags operator^(E1 f) const
    { return iUrlTwoFlags(uint(i ^ f)); }
    inline iUrlTwoFlags operator^(E2 f) const
    { return iUrlTwoFlags(uint(i ^ f)); }
    inline iUrlTwoFlags operator&(int mask) const
    { return iUrlTwoFlags(uint(i & mask)); }
    inline iUrlTwoFlags operator&(uint mask) const
    { return iUrlTwoFlags(uint(i & mask)); }
    inline iUrlTwoFlags operator&(E1 f) const
    { return iUrlTwoFlags(uint(i & f)); }
    inline iUrlTwoFlags operator&(E2 f) const
    { return iUrlTwoFlags(uint(i & f)); }
    inline iUrlTwoFlags operator~() const
    { return iUrlTwoFlags(~i); }

    inline bool testFlag(E1 f) const { return (i & f) == f && (f != 0 || i == int(f)); }
    inline bool testFlag(E2 f) const { return (i & f) == f && (f != 0 || i == int(f)); }
};

class IX_CORE_EXPORT iUrl
{
public:
    enum ParsingMode {
        TolerantMode,
        StrictMode,
        DecodedMode
    };

    // encoding / toString values
    enum UrlFormattingOption {
        None = 0x0,
        RemoveScheme = 0x1,
        RemovePassword = 0x2,
        RemoveUserInfo = RemovePassword | 0x4,
        RemovePort = 0x8,
        RemoveAuthority = RemoveUserInfo | RemovePort | 0x10,
        RemovePath = 0x20,
        RemoveQuery = 0x40,
        RemoveFragment = 0x80,
        
        PreferLocalFile = 0x200,
        StripTrailingSlash = 0x400,
        RemoveFilename = 0x800,
        NormalizePathSegments = 0x1000
    };

    enum ComponentFormattingOption {
        PrettyDecoded = 0x000000,
        EncodeSpaces = 0x100000,
        EncodeUnicode = 0x200000,
        EncodeDelimiters = 0x400000 | 0x800000,
        EncodeReserved = 0x1000000,
        DecodeReserved = 0x2000000,
        // 0x4000000 used to indicate full-decode mode

        FullyEncoded = EncodeSpaces | EncodeUnicode | EncodeDelimiters | EncodeReserved,
        FullyDecoded = FullyEncoded | DecodeReserved | 0x4000000
    };
    typedef uint ComponentFormattingOptions;

    typedef iUrlTwoFlags<UrlFormattingOption, ComponentFormattingOption> FormattingOptions;

    iUrl();
    iUrl(const iUrl &copy);
    iUrl &operator =(const iUrl &copy);
    iUrl(const iString &url, ParsingMode mode = TolerantMode);
    iUrl &operator=(const iString &url);

    ~iUrl();

    inline void swap(iUrl &other) { std::swap(d, other.d); }

    void setUrl(const iString &url, ParsingMode mode = TolerantMode);
    iString url(FormattingOptions options = FormattingOptions(PrettyDecoded)) const;
    iString toString(FormattingOptions options = FormattingOptions(PrettyDecoded)) const;
    iString toDisplayString(FormattingOptions options = FormattingOptions(PrettyDecoded)) const;
    iUrl adjusted(FormattingOptions options) const;

    iByteArray toEncoded(FormattingOptions options = FullyEncoded) const;
    static iUrl fromEncoded(const iByteArray &url, ParsingMode mode = TolerantMode);

    bool isValid() const;
    iString errorString() const;

    bool isEmpty() const;
    void clear();

    void setScheme(const iString &scheme);
    iString scheme() const;

    void setAuthority(const iString &authority, ParsingMode mode = TolerantMode);
    iString authority(ComponentFormattingOptions options = PrettyDecoded) const;

    void setUserInfo(const iString &userInfo, ParsingMode mode = TolerantMode);
    iString userInfo(ComponentFormattingOptions options = PrettyDecoded) const;

    void setUserName(const iString &userName, ParsingMode mode = DecodedMode);
    iString userName(ComponentFormattingOptions options = FullyDecoded) const;

    void setPassword(const iString &password, ParsingMode mode = DecodedMode);
    iString password(ComponentFormattingOptions = FullyDecoded) const;

    void setHost(const iString &host, ParsingMode mode = DecodedMode);
    iString host(ComponentFormattingOptions = FullyDecoded) const;
    iString topLevelDomain(ComponentFormattingOptions options = FullyDecoded) const;

    void setPort(int port);
    int port(int defaultPort = -1) const;

    void setPath(const iString &path, ParsingMode mode = DecodedMode);
    iString path(ComponentFormattingOptions options = FullyDecoded) const;
    iString fileName(ComponentFormattingOptions options = FullyDecoded) const;

    bool hasQuery() const;
    void setQuery(const iString &query, ParsingMode mode = TolerantMode);
    iString query(ComponentFormattingOptions = PrettyDecoded) const;

    bool hasFragment() const;
    iString fragment(ComponentFormattingOptions options = PrettyDecoded) const;
    void setFragment(const iString &fragment, ParsingMode mode = TolerantMode);

    iUrl resolved(const iUrl &relative) const;

    bool isRelative() const;
    bool isParentOf(const iUrl &url) const;

    bool isLocalFile() const;
    iString toLocalFile() const;

    void detach();
    bool isDetached() const;

    bool operator <(const iUrl &url) const;
    bool operator ==(const iUrl &url) const;
    bool operator !=(const iUrl &url) const;

    bool matches(const iUrl &url, FormattingOptions options) const;

    static iString fromPercentEncoding(const iByteArray &);
    static iByteArray toPercentEncoding(const iString &,
                                        const iByteArray &exclude = iByteArray(),
                                        const iByteArray &include = iByteArray());

private:
    static iString fromEncodedComponent_helper(const iByteArray &ba);

public:
    static iString fromAce(const iByteArray &);
    static iByteArray toAce(const iString &);
    static std::list<iString> idnWhitelist();
    static std::list<iString> toStringList(const std::list<iUrl> &uris, FormattingOptions options = FormattingOptions(PrettyDecoded));
    static std::list<iUrl> fromStringList(const std::list<iString> &uris, ParsingMode mode = TolerantMode);

    static void setIdnWhitelist(const std::list<iString> &);

private:
    iUrlPrivate *d;
};

inline iUrl::FormattingOptions operator|(iUrl::UrlFormattingOption f1, iUrl::UrlFormattingOption f2)
{ return iUrl::FormattingOptions(f1) | f2; }
inline iUrl::FormattingOptions operator|(iUrl::UrlFormattingOption f1, iUrl::FormattingOptions f2)
{ return f2 | f1; }

// add operators for OR'ing the two types of flags
inline iUrl::FormattingOptions &operator|=(iUrl::FormattingOptions &i, iUrl::ComponentFormattingOptions f)
{ i |= iUrl::UrlFormattingOption(int(f)); return i; }
inline iUrl::FormattingOptions operator|(iUrl::UrlFormattingOption i, iUrl::ComponentFormattingOption f)
{ return i | iUrl::UrlFormattingOption(int(f)); }
inline iUrl::FormattingOptions operator|(iUrl::UrlFormattingOption i, iUrl::ComponentFormattingOptions f)
{ return i | iUrl::UrlFormattingOption(int(f)); }
inline iUrl::FormattingOptions operator|(iUrl::ComponentFormattingOption f, iUrl::UrlFormattingOption i)
{ return i | iUrl::UrlFormattingOption(int(f)); }
inline iUrl::FormattingOptions operator|(iUrl::ComponentFormattingOptions f, iUrl::UrlFormattingOption i)
{ return i | iUrl::UrlFormattingOption(int(f)); }
inline iUrl::FormattingOptions operator|(iUrl::FormattingOptions i, iUrl::ComponentFormattingOptions f)
{ return i | iUrl::UrlFormattingOption(int(f)); }
inline iUrl::FormattingOptions operator|(iUrl::ComponentFormattingOption f, iUrl::FormattingOptions i)
{ return i | iUrl::UrlFormattingOption(int(f)); }
inline iUrl::FormattingOptions operator|(iUrl::ComponentFormattingOptions f, iUrl::FormattingOptions i)
{ return i | iUrl::UrlFormattingOption(int(f)); }

} // namespace iShell

#endif // IURL_H
