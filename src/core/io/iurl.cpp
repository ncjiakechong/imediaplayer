/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iurl.cpp
/// @brief   provides a convenient interface for working with URLs
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

/*!
    \class iUrl

    \brief The iUrl class provides a convenient interface for working
    with URLs.

    \reentrant
    \ingroup io
    \ingroup network
    \ingroup shared


    It can parse and construct URLs in both encoded and unencoded
    form. iUrl also has support for internationalized domain names
    (IDNs).

    The most common way to use iUrl is to initialize it via the
    constructor by passing a iString. Otherwise, setUrl() can also
    be used.

    URLs can be represented in two forms: encoded or unencoded. The
    unencoded representation is suitable for showing to users, but
    the encoded representation is typically what you would send to
    a web server. For example, the unencoded URL
    "http://bühler.example.com/List of applicants.xml"
    would be sent to the server as
    "http://xn--bhler-kva.example.com/List%20of%20applicants.xml".

    A URL can also be constructed piece by piece by calling
    setScheme(), setUserName(), setPassword(), setHost(), setPort(),
    setPath(), setQuery() and setFragment(). Some convenience
    functions are also available: setAuthority() sets the user name,
    password, host and port. setUserInfo() sets the user name and
    password at once.

    Call isValid() to check if the URL is valid. This can be done at any point
    during the constructing of a URL. If isValid() returns \c false, you should
    clear() the URL before proceeding, or start over by parsing a new URL with
    setUrl().

    Constructing a query is particularly convenient through the use of the \l
    iUrlQuery class and its methods iUrlQuery::setQueryItems(),
    iUrlQuery::addQueryItem() and iUrlQuery::removeQueryItem(). Use
    iUrlQuery::setQueryDelimiters() to customize the delimiters used for
    generating the query string.

    For the convenience of generating encoded URL strings or query
    strings, there are two static functions called
    fromPercentEncoding() and toPercentEncoding() which deal with
    percent encoding and decoding of iString objects.

    fromLocalFile() constructs a iUrl by parsing a local
    file path. toLocalFile() converts a URL to a local file path.

    The human readable representation of the URL is fetched with
    toString(). This representation is appropriate for displaying a
    URL to a user in unencoded form. The encoded form however, as
    returned by toEncoded(), is for internal use, passing to web
    servers, mail clients and so on. Both forms are technically correct
    and represent the same URL unambiguously -- in fact, passing either
    form to iUrl's constructor or to setUrl() will yield the same iUrl
    object.

    iUrl conforms to the URI specification from
    \l{RFC 3986} (Uniform Resource Identifier: Generic Syntax), and includes
    scheme extensions from \l{RFC 1738} (Uniform Resource Locators). Case
    folding rules in iUrl conform to \l{RFC 3491} (Nameprep: A Stringprep
    Profile for Internationalized Domain Names (IDN)). It is also compatible with the
    \l{http://freedesktop.org/wiki/Specifications/file-uri-spec/}{file URI specification}
    from freedesktop.org, provided that the locale encodes file names using
    UTF-8 (required by IDN).

    \section2 Relative URLs vs Relative Paths

    Calling isRelative() will return whether or not the URL is relative.
    A relative URL has no \l {scheme}. For example:


    Notice that a URL can be absolute while containing a relative path, and
    vice versa:


    A relative URL can be resolved by passing it as an argument to resolved(),
    which returns an absolute URL. isParentOf() is used for determining whether
    one URL is a parent of another.

    \section2 Error checking

    iUrl is capable of detecting many errors in URLs while parsing it or when
    components of the URL are set with individual setter methods (like
    setScheme(), setHost() or setPath()). If the parsing or setter function is
    successful, any previously recorded error conditions will be discarded.

    By default, iUrl setter methods operate in iUrl::TolerantMode, which means
    they accept some common mistakes and mis-representation of data. An
    alternate method of parsing is iUrl::StrictMode, which applies further
    checks. See iUrl::ParsingMode for a description of the difference of the
    parsing modes.

    iUrl only checks for conformance with the URL specification. It does not
    try to verify that high-level protocol URLs are in the format they are
    expected to be by handlers elsewhere. For example, the following URIs are
    all considered valid by iUrl, even if they do not make sense when used:

    \list
      \li "http:/filename.html"
      \li "mailto://example.com"
    \endlist

    When the parser encounters an error, it signals the event by making
    isValid() return false and toString() / toEncoded() return an empty string.
    If it is necessary to show the user the reason why the URL failed to parse,
    the error condition can be obtained from iUrl by calling errorString().
    Note that this message is highly technical and may not make sense to
    end-users.

    iUrl is capable of recording only one error condition. If more than one
    error is found, it is undefined which error is reported.

    \section2 Character Conversions

    Follow these rules to avoid erroneous character conversion when
    dealing with URLs and strings:

    \list
    \li When creating a iString to contain a URL from a iByteArray or a
       char*, always use iString::fromUtf8().
    \endlist
*/

/*!
    \enum iUrl::ParsingMode

    The parsing mode controls the way iUrl parses strings.

    \value TolerantMode iUrl will try to correct some common errors in URLs.
                        This mode is useful for parsing URLs coming from sources
                        not known to be strictly standards-conforming.

    \value StrictMode Only valid URLs are accepted. This mode is useful for
                      general URL validation.

    \value DecodedMode iUrl will interpret the URL component in the fully-decoded form,
                       where percent characters stand for themselves, not as the beginning
                       of a percent-encoded sequence. This mode is only valid for the
                       setters setting components of a URL; it is not permitted in
                       the iUrl constructor, in fromEncoded() or in setUrl().
                       For more information on this mode, see the documentation for
                       \l {iUrl::ComponentFormattingOption}{iUrl::FullyDecoded}.

    In TolerantMode, the parser has the following behaviour:

    \list

    \li Spaces and "%20": unencoded space characters will be accepted and will
    be treated as equivalent to "%20".

    \li Single "%" characters: Any occurrences of a percent character "%" not
    followed by exactly two hexadecimal characters (e.g., "13% coverage.html")
    will be replaced by "%25". Note that one lone "%" character will trigger
    the correction mode for all percent characters.

    \li Reserved and unreserved characters: An encoded URL should only
    contain a few characters as literals; all other characters should
    be percent-encoded. In TolerantMode, these characters will be
    accepted if they are found in the URL:
            space / double-quote / "<" / ">" / "\" /
            "^" / "`" / "{" / "|" / "}"
    Those same characters can be decoded again by passing iUrl::DecodeReserved
    to toString() or toEncoded(). In the getters of individual components,
    those characters are often returned in decoded form.

    \endlist

    When in StrictMode, if a parsing error is found, isValid() will return \c
    false and errorString() will return a message describing the error.
    If more than one error is detected, it is undefined which error gets
    reported.

    Note that TolerantMode is not usually enough for parsing user input, which
    often contains more errors and expectations than the parser can deal with.
    When dealing with data coming directly from the user -- as opposed to data
    coming from data-transfer sources, such as other programs -- it is
    recommended to use fromUserInput().

    \sa fromUserInput(), setUrl(), toString(), toEncoded(), iUrl::FormattingOptions
*/

/*!
    \enum iUrl::UrlFormattingOption

    The formatting options define how the URL is formatted when written out
    as text.

    \value None The format of the URL is unchanged.
    \value RemoveScheme  The scheme is removed from the URL.
    \value RemovePassword  Any password in the URL is removed.
    \value RemoveUserInfo  Any user information in the URL is removed.
    \value RemovePort      Any specified port is removed from the URL.
    \value RemoveAuthority
    \value RemovePath   The URL's path is removed, leaving only the scheme,
                        host address, and port (if present).
    \value RemoveQuery  The query part of the URL (following a '?' character)
                        is removed.
    \value RemoveFragment
    \value RemoveFilename The filename (i.e. everything after the last '/' in the path) is removed.
            The trailing '/' is kept, unless StripTrailingSlash is set.
            Only valid if RemovePath is not set.
    \value PreferLocalFile If the URL is a local file according to isLocalFile()
     and contains no query or fragment, a local file path is returned.
    \value StripTrailingSlash  The trailing slash is removed from the path, if one is present.
    \value NormalizePathSegments  Modifies the path to remove redundant directory separators,
             and to resolve "."s and ".."s (as far as possible). For non-local paths, adjacent
             slashes are preserved.

    Note that the case folding rules in \l{RFC 3491}{Nameprep}, which iUrl
    conforms to, require host names to always be converted to lower case,
    regardless of the FormattingOptions used.

    The options from iUrl::ComponentFormattingOptions are also possible.

    \sa iUrl::ComponentFormattingOptions
*/

/*!
    \enum iUrl::ComponentFormattingOption

    The component formatting options define how the components of an URL will
    be formatted when written out as text. They can be combined with the
    options from iUrl::FormattingOptions when used in toString() and
    toEncoded().

    \value PrettyDecoded   The component is returned in a "pretty form", with
                           most percent-encoded characters decoded. The exact
                           behavior of PrettyDecoded varies from component to
                           component and may also change. This is the default.

    \value EncodeSpaces    Leave space characters in their encoded form ("%20").

    \value EncodeUnicode   Leave non-US-ASCII characters encoded in their UTF-8
                           percent-encoded form (e.g., "%C3%A9" for the U+00E9
                           codepoint, LATIN SMALL LETTER E WITH ACUTE).

    \value EncodeDelimiters Leave certain delimiters in their encoded form, as
                            would appear in the URL when the full URL is
                            represented as text. The delimiters are affected
                            by this option change from component to component.
                            This flag has no effect in toString() or toEncoded().

    \value EncodeReserved  Leave US-ASCII characters not permitted in the URL by
                           the specification in their encoded form. This is the
                           default on toString() and toEncoded().

    \value DecodeReserved  Decode the US-ASCII characters that the URL specification
                           does not allow to appear in the URL. This is the
                           default on the getters of individual components.

    \value FullyEncoded    Leave all characters in their properly-encoded form,
                           as this component would appear as part of a URL. When
                           used with toString(), this produces a fully-compliant
                           URL in iString form, exactly equal to the result of
                           toEncoded()

    \value FullyDecoded    Attempt to decode as much as possible. For individual
                           components of the URL, this decodes every percent
                           encoding sequence, including control characters (U+0000
                           to U+001F) and UTF-8 sequences found in percent-encoded form.
                           Use of this mode may cause data loss, see below for more information.

    The values of EncodeReserved and DecodeReserved should not be used together
    in one call. The behavior is undefined if that happens. They are provided
    as separate values because the behavior of the "pretty mode" with regards
    to reserved characters is different on certain components and specially on
    the full URL.

    \section2 Full decoding

    The FullyDecoded mode is similar to the behavior of the functions returning
    iString, in that every character represents itself and never has
    any special meaning. This is true even for the percent character ('%'),
    which should be interpreted to mean a literal percent, not the beginning of
    a percent-encoded sequence. The same actual character, in all other
    decoding modes, is represented by the sequence "%25".

    Whenever re-applying data obtained with iUrl::FullyDecoded into a iUrl,
    care must be taken to use the iUrl::DecodedMode parameter to the setters
    (like setPath() and setUserName()). Failure to do so may cause
    re-interpretation of the percent character ('%') as the beginning of a
    percent-encoded sequence.

    This mode is quite useful when portions of a URL are used in a non-URL
    context. For example, to extract the username, password or file paths in an
    FTP client application, the FullyDecoded mode should be used.

    This mode should be used with care, since there are two conditions that
    cannot be reliably represented in the returned iString. They are:

    \list
      \li \b{Non-UTF-8 sequences:} URLs may contain sequences of
      percent-encoded characters that do not form valid UTF-8 sequences. Since
      URLs need to be decoded using UTF-8, any decoder failure will result in
      the iString containing one or more replacement characters where the
      sequence existed.

      \li \b{Encoded delimiters:} URLs are also allowed to make a distinction
      between a delimiter found in its literal form and its equivalent in
      percent-encoded form. This is most commonly found in the query, but is
      permitted in most parts of the URL.
    \endlist

    The following example illustrates the problem:


    If the two URLs were used via HTTP GET, the interpretation by the web
    server would probably be different. In the first case, it would interpret
    as one parameter, with a key of "q" and value "a+=b&c". In the second
    case, it would probably interpret as two parameters, one with a key of "q"
    and value "a =b", and the second with a key "c" and no value.

    \sa iUrl::FormattingOptions
*/

/*!
    \enum iUrl::UserInputResolutionOption

    The user input resolution options define how fromUserInput() should
    interpret strings that could either be a relative path or the short
    form of a HTTP URL. For instance \c{file.pl} can be either a local file
    or the URL \c{http://file.pl}.

    \value DefaultResolution  The default resolution mechanism is to check
                              whether a local file exists, in the working
                              directory given to fromUserInput, and only
                              return a local path in that case. Otherwise a URL
                              is assumed.
    \value AssumeLocalFile    This option makes fromUserInput() always return
                              a local path unless the input contains a scheme, such as
                              \c{http://file.pl}. This is useful for applications
                              such as text editors, which are able to create
                              the file if it doesn't exist.

    \sa fromUserInput()
*/

/*!
    \fn iUrl::iUrl(iUrl &&other)

    Move-constructs a iUrl instance, making it point at the same
    object that \a other was pointing to.
*/

/*!
    \fn iUrl &iUrl::operator=(iUrl &&other)

    Move-assigns \a other to this iUrl instance.
*/

#include <core/utils/ivarlengtharray.h>
#include <core/utils/istring.h>
#include <core/io/iurl.h>
#include <core/io/ilog.h>
#include "iurl_p.h"
#include "iipaddress_p.h"
#include "utils/itools_p.h"

#define ILOG_TAG "ix_io"

namespace iShell {

using namespace iMiscUtils;

static inline iString ftpScheme()
{
    return iStringLiteral("ftp");
}

static inline iString fileScheme()
{
    return iStringLiteral("file");
}

static inline iString webDavScheme()
{
    return iStringLiteral("webdavs");
}

static inline iString webDavSslTag()
{
    return iStringLiteral("@SSL");
}


/*!
    This is a helper for the assignment operators of implicitly
    shared classes.
*/
template <typename T>
inline void iAtomicAssign(T *&d, T *x)
{
    if (d == x)
        return;
    x->ref.ref();
    if (!d->ref.deref())
        delete d;
    d = x;
}

/*!
    This is a helper for the detach method of implicitly shared
    classes. Your private class needs a copy constructor which copies
    the members and sets the refcount to 1. After that, your detach
    function should look like this:

    \snippet code/src.corelib.thread.qatomic.h 1
*/
template <typename T>
inline void iAtomicDetach(T *&d)
{
    if (d->ref.value() == 1)
        return;
    T *x = d;
    d = new T(*d);
    if (!x->ref.deref())
        delete x;
}


enum PathNormalization {
    DefaultNormalization = 0x00,
    UrlNormalizationMode = 0x01,
    RemotePath = 0x02
};
typedef uint PathNormalizations;
bool ix_normalizePathSegments(iString *path, PathNormalizations flags);

class iUrlPrivate
{
public:
    enum Section : uchar {
        Scheme = 0x01,
        UserName = 0x02,
        Password = 0x04,
        UserInfo = UserName | Password,
        Host = 0x08,
        Port = 0x10,
        Authority = UserInfo | Host | Port,
        Path = 0x20,
        Hierarchy = Authority | Path,
        Query = 0x40,
        Fragment = 0x80,
        FullUrl = 0xff
    };

    enum Flags : uchar {
        IsLocalFile = 0x01
    };

    enum ErrorCode {
        // the high byte of the error code matches the Section
        // the first item in each value must be the generic "Invalid xxx Error"
        InvalidSchemeError = Scheme << 8,

        InvalidUserNameError = UserName << 8,

        InvalidPasswordError = Password << 8,

        InvalidRegNameError = Host << 8,
        InvalidIPv4AddressError,
        InvalidIPv6AddressError,
        InvalidCharacterInIPv6Error,
        InvalidIPvFutureError,
        HostMissingEndBracket,

        InvalidPortError = Port << 8,
        PortEmptyError,

        InvalidPathError = Path << 8,

        InvalidQueryError = Query << 8,

        InvalidFragmentError = Fragment << 8,

        // the following three cases are only possible in combination with
        // presence/absence of the path, authority and scheme. See validityError().
        AuthorityPresentAndPathIsRelative = Authority << 8 | Path << 8 | 0x10000,
        AuthorityAbsentAndPathIsDoubleSlash,
        RelativeUrlPathContainsColonBeforeSlash = Scheme << 8 | Authority << 8 | Path << 8 | 0x10000,

        NoError = 0
    };

    struct Error {
        iString source;
        ErrorCode code;
        int position;
    };

    iUrlPrivate();
    iUrlPrivate(const iUrlPrivate &copy);
    ~iUrlPrivate();

    void parse(const iString &url, iUrl::ParsingMode parsingMode);
    bool isEmpty() const
    { return sectionIsPresent == 0 && port == -1 && path.isEmpty(); }

    Error *cloneError() const;
    void clearError();
    void setError(ErrorCode errorCode, const iString &source, xsizetype supplement = -1);
    ErrorCode validityError(iString *source = IX_NULLPTR, xsizetype *position = IX_NULLPTR) const;
    bool validateComponent(Section section, const iString &input, xsizetype begin, xsizetype end);
    bool validateComponent(Section section, const iString &input)
    { return validateComponent(section, input, 0, uint(input.length())); }

    // no iString scheme() const;
    void appendAuthority(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const;
    void appendUserInfo(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const;
    void appendUserName(iString &appendTo, iUrl::FormattingOptions options) const;
    void appendPassword(iString &appendTo, iUrl::FormattingOptions options) const;
    void appendHost(iString &appendTo, iUrl::FormattingOptions options) const;
    void appendPath(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const;
    void appendQuery(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const;
    void appendFragment(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const;

    // the "end" parameters are like STL iterators: they point to one past the last valid element
    bool setScheme(const iString &value, xsizetype len, bool doSetError);
    void setAuthority(const iString &auth, xsizetype from, xsizetype end, iUrl::ParsingMode mode);
    void setUserInfo(iStringView userInfo, iUrl::ParsingMode mode);
    void setUserName(iStringView value, iUrl::ParsingMode mode);
    void setPassword(iStringView value, iUrl::ParsingMode mode);
    bool setHost(const iString &value, xsizetype from, xsizetype end, iUrl::ParsingMode mode);
    void setPath(iStringView value, iUrl::ParsingMode mode);
    void setQuery(iStringView value, iUrl::ParsingMode mode);
    void setFragment(iStringView value, iUrl::ParsingMode mode);

    uint presentSections() const
    {
        uint s = sectionIsPresent;

        // We have to ignore the host-is-present flag for local files (the
        // "file" protocol), due to the requirements of the XDG file URI
        // specification.
        if (isLocalFile())
            s &= ~Host;

        // If the password was set, we must have a username too
        if (s & Password)
            s |= UserName;

        return s;
    }

    inline bool hasScheme() const { return sectionIsPresent & Scheme; }
    inline bool hasAuthority() const { return sectionIsPresent & Authority; }
    inline bool hasUserInfo() const { return sectionIsPresent & UserInfo; }
    inline bool hasUserName() const { return sectionIsPresent & UserName; }
    inline bool hasPassword() const { return sectionIsPresent & Password; }
    inline bool hasHost() const { return sectionIsPresent & Host; }
    inline bool hasPort() const { return port != -1; }
    inline bool hasPath() const { return !path.isEmpty(); }
    inline bool hasQuery() const { return sectionIsPresent & Query; }
    inline bool hasFragment() const { return sectionIsPresent & Fragment; }

    inline bool isLocalFile() const { return flags & IsLocalFile; }
    iString toLocalFile(iUrl::FormattingOptions options) const;

    bool normalizePathSegments(iString *path) const
    {
        PathNormalizations mode = UrlNormalizationMode;
        if (!isLocalFile())
            mode |= RemotePath;
        return ix_normalizePathSegments(path, mode);
    }

    iString mergePaths(const iString &relativePath) const;

    void clear()
    {
        clearError();
        scheme = userName = password = host = path = query = fragment = iString();
        port = -1;
        sectionIsPresent = 0;
        flags = 0;
    }

    iRefCount ref;
    int port;

    iString scheme;
    iString userName;
    iString password;
    iString host;
    iString path;
    iString query;
    iString fragment;

    Error *error;

    // not used for:
    //  - Port (port == -1 means absence)
    //  - Path (there's no path delimiter, so we optimize its use out of existence)
    // Schemes are never supposed to be empty, but we keep the flag anyway
    uchar sectionIsPresent;
    uchar flags;

    // 32-bit: 2 bytes tail padding available
    // 64-bit: 6 bytes tail padding available
};

inline iUrlPrivate::iUrlPrivate()
    : ref(1)
    , port(-1)
    , error(IX_NULLPTR)
    , sectionIsPresent(0)
    , flags(0)
{}

inline iUrlPrivate::iUrlPrivate(const iUrlPrivate &copy)
    : ref(1)
    , port(copy.port)
    , scheme(copy.scheme)
    , userName(copy.userName)
    , password(copy.password)
    , host(copy.host)
    , path(copy.path)
    , query(copy.query)
    , fragment(copy.fragment)
    , error(copy.cloneError())
    , sectionIsPresent(copy.sectionIsPresent)
    , flags(copy.flags)
{}

inline iUrlPrivate::~iUrlPrivate()
{
    delete error;
}

inline iUrlPrivate::Error *iUrlPrivate::cloneError() const
{
    return error ? new Error(*error) : IX_NULLPTR;
}

inline void iUrlPrivate::clearError()
{
    delete error;
    error = IX_NULLPTR;
}

inline void iUrlPrivate::setError(ErrorCode errorCode, const iString &source, xsizetype supplement)
{
    if (error) {
        // don't overwrite an error set in a previous section during parsing
        return;
    }
    error = new Error;
    error->code = errorCode;
    error->source = source;
    error->position = supplement;
}

// From RFC 3986, Appendix A Collected ABNF for URI
//    URI           = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
//[...]
//    scheme        = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
//    authority     = [ userinfo "@" ] host [ ":" port ]
//    userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
//    host          = IP-literal / IPv4address / reg-name
//    port          = *DIGIT
//[...]
//    reg-name      = *( unreserved / pct-encoded / sub-delims )
//[..]
//    pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
//
//    query         = *( pchar / "/" / "?" )
//
//    fragment      = *( pchar / "/" / "?" )
//
//    pct-encoded   = "%" HEXDIG HEXDIG
//
//    unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
//    reserved      = gen-delims / sub-delims
//    gen-delims    = ":" / "/" / "?" / "#" / "[" / "]" / "@"
//    sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
//                  / "*" / "+" / "," / ";" / "="
// the path component has a complex ABNF that basically boils down to
// slash-separated segments of "pchar"

// The above is the strict definition of the URL components and we mostly
// adhere to it, with few exceptions. iUrl obeys the following behavior:
//  - percent-encoding sequences always use uppercase HEXDIG;
//  - unreserved characters are *always* decoded, no exceptions;
//  - the space character and bytes with the high bit set are controlled by
//    the EncodeSpaces and EncodeUnicode bits;
//  - control characters, the percent sign itself, and bytes with the high
//    bit set that don't form valid UTF-8 sequences are always encoded,
//    except in FullyDecoded mode;
//  - sub-delims are always left alone, except in FullyDecoded mode;
//  - gen-delim change behavior depending on which section of the URL (or
//    the entire URL) we're looking at; see below;
//  - characters not mentioned above, like "<", and ">", are usually
//    decoded in individual sections of the URL, but encoded when the full
//    URL is put together (we can change on subjective definition of
//    "pretty").
//
// The behavior for the delimiters bears some explanation. The spec says in
// section 2.2:
//     URIs that differ in the replacement of a reserved character with its
//     corresponding percent-encoded octet are not equivalent.
// (note: iUrl API mistakenly uses the "reserved" term, so we will refer to
// them here as "delimiters").
//
// For that reason, we cannot encode delimiters found in decoded form and we
// cannot decode the ones found in encoded form if that would change the
// interpretation. Conversely, we *can* perform the transformation if it would
// not change the interpretation. From the last component of a URL to the first,
// here are the gen-delims we can unambiguously transform when the field is
// taken in isolation:
//  - fragment: none, since it's the last
//  - query: "#" is unambiguous
//  - path: "#" and "?" are unambiguous
//  - host: completely special but never ambiguous, see setHost() below.
//  - password: the "#", "?", "/", "[", "]" and "@" characters are unambiguous
//  - username: the "#", "?", "/", "[", "]", "@", and ":" characters are unambiguous
//  - scheme: doesn't accept any delimiter, see setScheme() below.
//
// Internally, iUrl stores each component in the format that corresponds to the
// default mode (PrettyDecoded). It deviates from the "strict" FullyEncoded
// mode in the following way:
//  - spaces are decoded
//  - valid UTF-8 sequences are decoded
//  - gen-delims that can be unambiguously transformed are decoded
//  - characters controlled by DecodeReserved are often decoded, though this behavior
//    can change depending on the subjective definition of "pretty"
//
// Note that the list of gen-delims that we can transform is different for the
// user info (user name + password) and the authority (user info + host +
// port).


// list the recoding table modifications to be used with the recodeFromUser and
// appendToUser functions, according to the rules above. Spaces and UTF-8
// sequences are handled outside the tables.

// the encodedXXX tables are run with the delimiters set to "leave" by default;
// the decodedXXX tables are run with the delimiters set to "decode" by default
// (except for the query, which doesn't use these functions)

#define decode(x) xuint16(x)
#define leave(x)  xuint16(0x100 | (x))
#define encode(x) xuint16(0x200 | (x))

static const xuint16 userNameInIsolation[] = {
    decode(':'), // 0
    decode('@'), // 1
    decode(']'), // 2
    decode('['), // 3
    decode('/'), // 4
    decode('?'), // 5
    decode('#'), // 6

    decode('"'), // 7
    decode('<'),
    decode('>'),
    decode('^'),
    decode('\\'),
    decode('|'),
    decode('{'),
    decode('}'),
    0
};
static const xuint16 * const passwordInIsolation = userNameInIsolation + 1;
static const xuint16 * const pathInIsolation = userNameInIsolation + 5;
static const xuint16 * const queryInIsolation = userNameInIsolation + 6;
static const xuint16 * const fragmentInIsolation = userNameInIsolation + 7;

static const xuint16 userNameInUserInfo[] =  {
    encode(':'), // 0
    decode('@'), // 1
    decode(']'), // 2
    decode('['), // 3
    decode('/'), // 4
    decode('?'), // 5
    decode('#'), // 6

    decode('"'), // 7
    decode('<'),
    decode('>'),
    decode('^'),
    decode('\\'),
    decode('|'),
    decode('{'),
    decode('}'),
    0
};
static const xuint16 * const passwordInUserInfo = userNameInUserInfo + 1;

static const xuint16 userNameInAuthority[] = {
    encode(':'), // 0
    encode('@'), // 1
    encode(']'), // 2
    encode('['), // 3
    decode('/'), // 4
    decode('?'), // 5
    decode('#'), // 6

    decode('"'), // 7
    decode('<'),
    decode('>'),
    decode('^'),
    decode('\\'),
    decode('|'),
    decode('{'),
    decode('}'),
    0
};
static const xuint16 * const passwordInAuthority = userNameInAuthority + 1;

static const xuint16 userNameInUrl[] = {
    encode(':'), // 0
    encode('@'), // 1
    encode(']'), // 2
    encode('['), // 3
    encode('/'), // 4
    encode('?'), // 5
    encode('#'), // 6

    // no need to list encode(x) for the other characters
    0
};
static const xuint16 * const passwordInUrl = userNameInUrl + 1;
static const xuint16 * const pathInUrl = userNameInUrl + 5;
static const xuint16 * const queryInUrl = userNameInUrl + 6;
static const xuint16 * const fragmentInUrl = userNameInUrl + 6;

static void recodeFromUser(iString &output, const iString &input, const ushort *actions, iUrl::ParsingMode mode)
{
    output.resize(0);
    xsizetype appended;
    if (mode == iUrl::DecodedMode)
        appended = ix_encodeFromUser(output, input, actions);
    else
        appended = ix_urlRecode(output, input, {}, actions);
    if (!appended)
        output = input;
}

static void recodeFromUser(iString &output, const iStringView &input, const ushort *actions, iUrl::ParsingMode mode)
{
    IX_ASSERT_X(mode != iUrl::DecodedMode, "This function should only be called when parsing encoded components");
    IX_UNUSED(mode);
    output.resize(0);
    if (ix_urlRecode(output, input, {}, actions))
        return;
    output.append(input);
}

// appendXXXX functions: copy from the internal form to the external, user form.
// the internal value is stored in its PrettyDecoded form, so that case is easy.
static inline void appendToUser(iString &appendTo, const iStringView &value, iUrl::FormattingOptions options,
                                const xuint16 *actions)
{
    // The stored value is already iUrl::PrettyDecoded, so there's nothing to
    // do if that's what the user asked for (test only
    // ComponentFormattingOptions, ignore FormattingOptions).
    if ((options & 0xFFFF0000) == iUrl::PrettyDecoded ||
            !ix_urlRecode(appendTo, value, options, actions))
        appendTo += value;

    // copy nullness, if necessary, because iString::operator+=(iStringView) doesn't
    if (appendTo.isNull() && !value.isNull())
        appendTo.detach();
}

inline void iUrlPrivate::appendAuthority(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const
{
    if ((options & iUrl::RemoveUserInfo) != iUrl::RemoveUserInfo) {
        appendUserInfo(appendTo, options, appendingTo);

        // add '@' only if we added anything
        if (hasUserName() || (hasPassword() && (options & iUrl::RemovePassword) == 0))
            appendTo += iLatin1Char('@');
    }
    appendHost(appendTo, options);
    if (!(options & iUrl::RemovePort) && port != -1)
        appendTo += iLatin1Char(':') + iString::number(port);
}

inline void iUrlPrivate::appendUserInfo(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const
{
    if (!hasUserInfo())
        return;

    const xuint16 *userNameActions;
    const xuint16 *passwordActions;
    if (options & iUrl::EncodeDelimiters) {
        userNameActions = userNameInUrl;
        passwordActions = passwordInUrl;
    } else {
        switch (appendingTo) {
        case UserInfo:
            userNameActions = userNameInUserInfo;
            passwordActions = passwordInUserInfo;
            break;

        case Authority:
            userNameActions = userNameInAuthority;
            passwordActions = passwordInAuthority;
            break;

        case FullUrl:
            userNameActions = userNameInUrl;
            passwordActions = passwordInUrl;
            break;

        default:
            // can't happen
            break;
        }
    }

    if (!ix_urlRecode(appendTo, userName, options, userNameActions))
        appendTo += userName;
    if (options & iUrl::RemovePassword || !hasPassword()) {
        return;
    } else {
        appendTo += iLatin1Char(':');
        if (!ix_urlRecode(appendTo, password, options, passwordActions))
            appendTo += password;
    }
}

inline void iUrlPrivate::appendUserName(iString &appendTo, iUrl::FormattingOptions options) const
{
    // only called from iUrl::userName()
    appendToUser(appendTo, userName, options,
                 options & iUrl::EncodeDelimiters ? userNameInUrl : userNameInIsolation);
    if (appendTo.isNull() && hasPassword())
        appendTo.detach();      // the presence of password implies presence of username
}

inline void iUrlPrivate::appendPassword(iString &appendTo, iUrl::FormattingOptions options) const
{
    // only called from iUrl::password()
    appendToUser(appendTo, password, options,
                 options & iUrl::EncodeDelimiters ? passwordInUrl : passwordInIsolation);
}


// Return the length of the root part of an absolute path, for use by cleanPath(), cd().
static xsizetype rootLength(iStringView name, PathNormalizations flags)
{
    const bool UseWindowsRules = false
#if defined(IX_OS_WIN)
            || true
#endif
            ;
    const xsizetype len = name.size();
    xuint16 firstChar = len > 0 ? name.at(0).unicode() : u'\0';
    xuint16 secondChar = len > 1 ? name.at(1).unicode() : u'\0';
    if (UseWindowsRules) {
        // Handle possible UNC paths which start with double slash
        bool urlMode = flags & UrlNormalizationMode;
        if (firstChar == u'/' && secondChar == u'/' && !urlMode) {
            // Server name '//server/path' is part of the prefix.
            const xsizetype nextSlash = name.indexOf(u'/', 2);
            return nextSlash >= 0 ? nextSlash + 1 : len;
        }

        // Handle a possible drive letter
        xsizetype driveLength = 2;
        if (firstChar == u'/' && urlMode && len > 2 && name.at(2) == u':') {
            // Drive-in-URL-Path mode, e.g. "/c:" or "/c:/autoexec.bat"
            ++driveLength;
            secondChar = u':';
        }
        if (secondChar == u':') {
            if (len > driveLength && name.at(driveLength) == u'/')
                return driveLength + 1;     // absolute drive path, e.g. "c:/config.sys"
            return driveLength;             // relative drive path, e.g. "c:" or "d:swapfile.sys"
        }
    }

    return firstChar == u'/' ? 1 : 0;
}

/*!
    \internal

    Updates \a path with redundant directory separators removed, and "."s and
    ".."s resolved (as far as possible). It returns \c false if there were ".."
    segments left over, attempt to go up past the root (only applies to
    absolute paths), or \c true otherwise.

    This method is shared with iUrl, so it doesn't deal with iDir::separator(),
    nor does it remove the trailing slash, if any.

    When dealing with URLs, we are following the "Remove dot segments"
    algorithm from https://www.ietf.org/rfc/rfc3986.html#section-5.2.4
    URL mode differs from local path mode in these ways:
    1) it can set *path to empty ("." becomes "")
    2) directory path outputs end in / ("a/.." becomes "a/" instead of "a")
    3) a sequence of "//" is treated as multiple path levels ("a/b//.." becomes
       "a/b/" and "a/b//../.." becomes "a/"), which matches the behavior
       observed in web browsers.

    As an extension, for local URLs we treat multiple slashes as one slash.
*/
bool ix_normalizePathSegments(iString *path, PathNormalizations flags)
{
    const bool isRemote = flags & RemotePath;
    const xsizetype prefixLength = rootLength(*path, flags);

    // RFC 3986 says: "The input buffer is initialized with the now-appended
    // path components and the output buffer is initialized to the empty
    // string."
    const iChar *in = path->constBegin();

    // Scan the input for a "." or ".." segment. If there isn't any, we may not
    // need to modify this path at all. Also scan for "//" segments, which
    // will be normalized if the path is local.
    xsizetype i = prefixLength;
    xsizetype n = path->size();
    for (bool lastWasSlash = true; i < n; ++i) {
        if (lastWasSlash && in[i] == u'.') {
            if (i + 1 == n || in[i + 1] == u'/')
                break;
            if (in[i + 1] == u'.' && (i + 2 == n || in[i + 2] == u'/'))
                break;
        }
        if (!isRemote && lastWasSlash && in[i] == u'/' && i > 0) {
            // Found consecutive slashes in local path (e.g., "path//to/file")
            // Move back one position so the normalization loop below will remove
            // all consecutive slashes in one pass
            --i;
            break;
        }
        lastWasSlash = in[i] == u'/';
    }
    if (i == n)
        return true;

    iChar *out = path->data();  // Triggers copy-on-write detachment for modification
    const iChar *start = out + prefixLength;
    const iChar *end = out + path->size();
    out += i;
    in = out;

    // We implement a modified algorithm compared to RFC 3986, for efficiency.
    bool ok = true;
    do {
        // First, copy the preceding slashes, so we can look at the segment's
        // content. If the path is part of a URL, we copy all slashes, otherwise
        // just one.
        if (in[0] == u'/') {
            *out++ = *in++;
            while (in < end && in[0] == u'/') {
                if (isRemote)
                    *out++ = *in++;
                else
                    ++in; // Skip multiple slashes for local URLs

                // Note: we may exit this loop with in == end, in which case we
                // *shouldn't* dereference *in. But since we are pointing to a
                // detached, non-empty string, we know there's a u'\0' at the
                // end, so dereferencing is safe.
            }
        }

        // Is this path segment either "." or ".."?
        enum { Nothing, Dot, DotDot } type = Nothing;
        if (in[0] == u'.') {
            if (in + 1 == end || in[1] == u'/')
                type = Dot;
            else if (in[1] == u'.' && (in + 2 == end || in[2] == u'/'))
                type = DotDot;
        }
        if (type == Nothing) {
            // If it is neither, then we copy this segment.
            while (in < end && in[0] != u'/')
                *out++ = *in++;
            continue;
        }

        // Otherwise, we skip it and remove preceding slashes (if
        // any, exactly one if part of a URL, all otherwise) from the
        // output. If it is "..", we remove the segment before that and
        // preceding slashes too in a similar fashion, if they are there.
        if (type == DotDot) {
            if (out == start) {
                // we can't go further up from here, so we "re-root"
                // without cleaning this segment
                ok = false;
                if (!isRemote) {
                    *out++ = u'.';
                    *out++ = u'.';
                    if (in + 2 != end) {
                        IX_ASSERT(in[2] == u'/');
                        *out++ = u'/';
                        ++in;
                    }
                    start = out;
                    in += 2;
                    continue;
                }
            }

            if (out > start)
                --out; // backtrack the first dot
            // backtrack the previous path segment
            while (out > start && out[-1] != u'/')
                --out;
            in += 2;    // the two dots
        } else {
            ++in;       // the one dot
        }

        // Not at 'end' yet, prepare for the next loop iteration by backtracking one slash.
        // E.g.: /a/b/../c    >>> /a/b/../c
        //          ^out            ^out
        // the next iteration will copy '/c' to the output buffer >>> /a/c
        if (in != end && out > start && out[-1] == u'/')
            --out;
        if (out == start) {
            // We've reached the root. Make sure we don't turn a relative path
            // to absolute or, in the case of local paths that are already
            // absolute, into UNC.
            // Note: this will turn ".//a" into "a" even for URLs!
            if (in != end && in[0] == u'/')
                ++in;
            while (prefixLength == 0 && in != end && in[0] == u'/')
                ++in;
        }
    } while (in < end);

    path->truncate(out - path->constBegin());
    if (!isRemote && path->isEmpty())
        *path = iLatin1StringView(".");

    // we return false only if the path was absolute
    return ok || prefixLength == 0;
}

inline void iUrlPrivate::appendPath(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const
{
    iString thePath = path;
    if (options & iUrl::NormalizePathSegments)
        normalizePathSegments(&thePath);

    iStringView thePathView(thePath);
    if (options & iUrl::RemoveFilename) {
        const xsizetype slash = path.lastIndexOf(iLatin1Char('/'));
        if (slash == -1)
            return;
        thePathView = path.left(slash + 1);
    }
    // check if we need to remove trailing slashes
    if (options & iUrl::StripTrailingSlash) {
        while (thePathView.length() > 1 && thePathView.endsWith(iLatin1Char('/')))
            thePathView.chop(1);
    }

    appendToUser(appendTo, thePathView, options,
                 appendingTo == FullUrl || options & iUrl::EncodeDelimiters ? pathInUrl : pathInIsolation);
}

inline void iUrlPrivate::appendFragment(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const
{
    appendToUser(appendTo, fragment, options,
                 options & iUrl::EncodeDelimiters ? fragmentInUrl :
                 appendingTo == FullUrl ? IX_NULLPTR : fragmentInIsolation);
}

inline void iUrlPrivate::appendQuery(iString &appendTo, iUrl::FormattingOptions options, Section appendingTo) const
{
    appendToUser(appendTo, query, options,
                 appendingTo == FullUrl || options & iUrl::EncodeDelimiters ? queryInUrl : queryInIsolation);
}

// setXXX functions

inline bool iUrlPrivate::setScheme(const iString &value, xsizetype len, bool doSetError)
{
    // schemes are strictly RFC-compliant:
    //    scheme        = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    // we also lowercase the scheme

    // schemes in URLs are not allowed to be empty, but they can be in
    // "Relative URIs" which iUrl also supports. iUrl::setScheme does
    // not call us with len == 0, so this can only be from parse()
    scheme.clear();
    if (len == 0)
        return false;

    sectionIsPresent |= Scheme;

    // validate it:
    xsizetype needsLowercasing = -1;
    const xuint16 *p = reinterpret_cast<const xuint16 *>(value.constData());
    for (xsizetype i = 0; i < len; ++i) {
        if (isAsciiLower(p[i]))
            continue;
        if (isAsciiUpper(p[i])) {
            needsLowercasing = i;
            continue;
        }
        if (i) {
            if (isAsciiDigit(p[i]))
                continue;
            if (p[i] == '+' || p[i] == '-' || p[i] == '.')
                continue;
        }

        // found something else
        // don't call setError needlessly:
        // if we've been called from parse(), it will try to recover
        if (doSetError)
            setError(InvalidSchemeError, value, i);
        return false;
    }

    scheme = value.left(len);

    if (needsLowercasing != -1) {
        // schemes are ASCII only, so we don't need the full Unicode toLower
        iChar *schemeData = scheme.data(); // force detaching here
        for (xsizetype i = needsLowercasing; i >= 0; --i) {
            xuint16 c = schemeData[i].unicode();
            if (isAsciiUpper(c))
                schemeData[i] = iChar(c + 0x20);
        }
    }

    // did we set to the file protocol?
    if (scheme == fileScheme()
        #ifdef IX_OS_WIN
        || scheme == webDavScheme()
        #endif
       ) {
        flags |= IsLocalFile;
    } else {
        flags &= ~IsLocalFile;
    }
    return true;
}

inline void iUrlPrivate::setAuthority(const iString &auth, xsizetype from, xsizetype end, iUrl::ParsingMode mode)
{
    sectionIsPresent &= ~Authority;
    port = -1;
    if (from == end && !auth.isNull())
        sectionIsPresent |= Host;   // empty but not null authority implies host

    // we never actually _loop_
    while (from != end) {
        xsizetype userInfoIndex = auth.indexOf(iLatin1Char('@'), from);
        if (size_t(userInfoIndex) < size_t(end)) {
            setUserInfo(iStringView(auth).sliced(from, userInfoIndex - from), mode);
            if (mode == iUrl::StrictMode && !validateComponent(UserInfo, auth, from, userInfoIndex))
                break;
            from = userInfoIndex + 1;
        }

        xsizetype colonIndex = auth.lastIndexOf(iLatin1Char(':'), end - 1);
        if (colonIndex < from)
            colonIndex = -1;

        if (size_t(colonIndex) < size_t(end)) {
            if (auth.at(from).unicode() == '[') {
                // check if colonIndex isn't inside the "[...]" part
                xsizetype closingBracket = auth.indexOf(iLatin1Char(']'), from);
                if (size_t(closingBracket) > size_t(colonIndex))
                    colonIndex = -1;
            }
        }

        if (size_t(colonIndex) < size_t(end) - 1) {
            // found a colon with digits after it
            unsigned long x = 0;
            for (xsizetype i = colonIndex + 1; i < end; ++i) {
                xuint16 c = auth.at(i).unicode();
                if (isAsciiDigit(c)) {
                    x *= 10;
                    x += c - '0';
                } else {
                    x = ulong(-1); // x != xuint16(x)
                    break;
                }
            }
            if (x == xuint16(x)) {
                port = xuint16(x);
            } else {
                setError(InvalidPortError, auth, colonIndex + 1);
                if (mode == iUrl::StrictMode)
                    break;
            }
        }

        setHost(auth, from, std::min<uint>(end, colonIndex), mode);
        if (mode == iUrl::StrictMode && !validateComponent(Host, auth, from, std::min<uint>(end, colonIndex))) {
            // Validation failed: clear entire authority section (not just host)
            sectionIsPresent &= ~Authority;
            break;
        }

        // Authority section successfully parsed and validated
        return;
    }
    // clear all sections but host
    sectionIsPresent &= ~Authority | Host;
    userName.clear();
    password.clear();
    host.clear();
    port = -1;
}

inline void iUrlPrivate::setUserInfo(iStringView userInfo, iUrl::ParsingMode mode)
{
    xsizetype delimIndex = userInfo.indexOf(iLatin1Char(':'));
    if (delimIndex < 0) {
        // no password
        setUserName(userInfo, mode);
        password.clear();
        sectionIsPresent &= ~Password;
    } else {
        setUserName(userInfo.first(delimIndex), mode);
        setPassword(userInfo.sliced(delimIndex + 1), mode);
    }
}

inline void iUrlPrivate::setUserName(iStringView value, iUrl::ParsingMode mode)
{
    sectionIsPresent |= UserName;
    recodeFromUser(userName, value, userNameInIsolation, mode);
}

inline void iUrlPrivate::setPassword(iStringView value, iUrl::ParsingMode mode)
{
    sectionIsPresent |= Password;
    recodeFromUser(password, value, passwordInIsolation, mode);
}

inline void iUrlPrivate::setPath(iStringView value, iUrl::ParsingMode mode)
{
    // sectionIsPresent |= Path; // not used, save some cycles
    recodeFromUser(path, value, pathInIsolation, mode);
}

inline void iUrlPrivate::setFragment(iStringView value, iUrl::ParsingMode mode)
{
    sectionIsPresent |= Fragment;
    recodeFromUser(fragment, value, fragmentInIsolation, mode);
}

inline void iUrlPrivate::setQuery(iStringView value, iUrl::ParsingMode mode)
{
    sectionIsPresent |= Query;
    recodeFromUser(query, value, queryInIsolation, mode);
}

// Host handling
// The RFC says the host is:
//    host          = IP-literal / IPv4address / reg-name
//    IP-literal    = "[" ( IPv6address / IPvFuture  ) "]"
//    IPvFuture     = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
//  [a strict definition of IPv6Address and IPv4Address]
//     reg-name      = *( unreserved / pct-encoded / sub-delims )
//
// We deviate from the standard in all but IPvFuture. For IPvFuture we accept
// and store only exactly what the RFC says we should. No percent-encoding is
// permitted in this field, so Unicode characters and space aren't either.
//
// For IPv4 addresses, we accept broken addresses like inet_aton does (that is,
// less than three dots). However, we correct the address to the proper form
// and store the corrected address. After correction, we comply to the RFC and
// it's exclusively composed of unreserved characters.
//
// For IPv6 addresses, we accept addresses including trailing (embedded) IPv4
// addresses, the so-called v4-compat and v4-mapped addresses. We also store
// those addresses like that in the hostname field, which violates the spec.
// IPv6 hosts are stored with the square brackets in the iString. It also
// requires no transformation in any way.
//
// As for registered names, it's the other way around: we accept only valid
// hostnames as specified by STD 3 and IDNA. That means everything we accept is
// valid in the RFC definition above, but there are many valid reg-names
// according to the RFC that we do not accept in the name of security. Since we
// do accept IDNA, reg-names are subject to ACE encoding and decoding, which is
// specified by the DecodeUnicode flag. The hostname is stored in its Unicode form.

inline void iUrlPrivate::appendHost(iString &appendTo, iUrl::FormattingOptions options) const
{
    if (host.isEmpty()) {
        if ((sectionIsPresent & Host) && appendTo.isNull())
            appendTo.detach();
        return;
    }
    if (host.at(0).unicode() == '[') {
        // IPv6 addresses might contain a zone-id which needs to be recoded
        if (options != 0)
            if (ix_urlRecode(appendTo, host, options, IX_NULLPTR))
                return;
        appendTo += host;
    } else {
        // this is either an IPv4Address or a reg-name
        // if it is a reg-name, it is already stored in Unicode form
        if (options & iUrl::EncodeUnicode && !(options & 0x4000000))
            appendTo += ix_ACE_do(host, ToAceOnly, AllowLeadingDot);
        else
            appendTo += host;
    }
}

// the whole IPvFuture is passed and parsed here, including brackets;
// returns null if the parsing was successful, or the iChar of the first failure
static const iChar *parseIpFuture(iString &host, const iChar *begin, const iChar *end, iUrl::ParsingMode mode)
{
    //    IPvFuture     = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
    static const char acceptable[] =
            "!$&'()*+,;=" // sub-delims
            ":"           // ":"
            "-._~";       // unreserved

    // the brackets and the "v" have been checked
    const iChar *const origBegin = begin;
    if (begin[3].unicode() != '.')
        return &begin[3];
    if (isHexDigit(begin[2].unicode())) {
        // this is so unlikely that we'll just go down the slow path
        // decode the whole string, skipping the "[vH." and "]" which we already know to be there
        host += iStringView(begin, 4);

        // uppercase the version, if necessary
        if (begin[2].unicode() >= 'a')
            host[host.length() - 2] = iChar(begin[2].unicode() - 0x20);

        begin += 4;
        --end;

        iString decoded;
        if (mode == iUrl::TolerantMode && ix_urlRecode(decoded, iStringView(begin, end), iUrl::FullyDecoded, IX_NULLPTR)) {
            begin = decoded.constBegin();
            end = decoded.constEnd();
        }

        for ( ; begin != end; ++begin) {
            if (isAsciiLetterOrNumber(begin->unicode()))
                host += *begin;
            else if (begin->unicode() < 0x80 && strchr(acceptable, begin->unicode()) != IX_NULLPTR)
                host += *begin;
            else
                return decoded.isEmpty() ? begin : &origBegin[2];
        }
        host += iLatin1Char(']');
        return IX_NULLPTR;
    }
    return &origBegin[2];
}

// ONLY the IPv6 address is parsed here, WITHOUT the brackets
static const iChar *parseIp6(iString &host, const iChar *begin, const iChar *end, iUrl::ParsingMode mode)
{
    iStringView decoded(begin, end);
    iString decodedBuffer;
    if (mode == iUrl::TolerantMode) {
        // this struct is kept in automatic storage because it's only 4 bytes
        const xuint16 decodeColon[] = { decode(':'), 0 };
        if (ix_urlRecode(decodedBuffer, decoded, iUrl::ComponentFormattingOption::PrettyDecoded, decodeColon))
            decoded = decodedBuffer;
    }

    const iStringView zoneIdIdentifier(u"%25");
    iIPAddressUtils::IPv6Address address;
    iStringView zoneId;

    xsizetype zoneIdPosition = decoded.indexOf(zoneIdIdentifier);
    if ((zoneIdPosition != -1) && (decoded.lastIndexOf(zoneIdIdentifier) == zoneIdPosition)) {
        zoneId = decoded.mid(zoneIdPosition + zoneIdIdentifier.size());
        decoded.truncate(zoneIdPosition);

        // was there anything after the zone ID separator?
        if (zoneId.isEmpty())
            return end;
    }

    // did the address become empty after removing the zone ID?
    // (it might have always been empty)
    if (decoded.isEmpty())
        return end;

    const iChar *ret = iIPAddressUtils::parseIp6(address, decoded.begin(), decoded.end());
    if (ret)
        return begin + (ret - decoded.begin());

    host.reserve(host.size() + (end - begin) + 2);  // +2 for the brackets
    host += iLatin1Char('[');
    iIPAddressUtils::toString(host, address);

    if (!zoneId.isEmpty()) {
        host += zoneIdIdentifier;
        host += zoneId;
    }
    host += iLatin1Char(']');
    return IX_NULLPTR;
}

inline bool iUrlPrivate::setHost(const iString &value, xsizetype from, xsizetype iend, iUrl::ParsingMode mode)
{
    const iChar *begin = value.constData() + from;
    const iChar *end = value.constData() + iend;

    const xsizetype len = end - begin;
    host.clear();
    sectionIsPresent &= ~Host;
    if (!value.isNull() || (sectionIsPresent & Authority))
        sectionIsPresent |= Host;
    if (len == 0)
        return true;

    if (begin[0].unicode() == '[') {
        // IPv6Address or IPvFuture
        // smallest IPv6 address is      "[::]"   (len = 4)
        // smallest IPvFuture address is "[v7.X]" (len = 6)
        if (end[-1].unicode() != ']') {
            setError(HostMissingEndBracket, value);
            return false;
        }

        if (len > 5 && begin[1].unicode() == 'v') {
            const iChar *c = parseIpFuture(host, begin, end, mode);
            if (c)
                setError(InvalidIPvFutureError, value, c - value.constData());
            return !c;
        } else if (begin[1].unicode() == 'v') {
            setError(InvalidIPvFutureError, value, from);
        }

        const iChar *c = parseIp6(host, begin + 1, end - 1, mode);
        if (!c)
            return true;

        if (c == end - 1)
            setError(InvalidIPv6AddressError, value, from);
        else
            setError(InvalidCharacterInIPv6Error, value, c - value.constData());
        return false;
    }

    // check if it's an IPv4 address
    iIPAddressUtils::IPv4Address ip4;
    if (iIPAddressUtils::parseIp4(ip4, begin, end)) {
        // yes, it was
        iIPAddressUtils::toString(host, ip4);
        return true;
    }

    // This is probably a reg-name.
    // But it can also be an encoded string that, when decoded becomes one
    // of the types above.
    //
    // Two types of encoding are possible:
    //  percent encoding (e.g., "%31%30%2E%30%2E%30%2E%31" -> "10.0.0.1")
    //  Unicode encoding (some non-ASCII characters case-fold to digits
    //                    when nameprepping is done)
    //
    // The ix_ACE_do function below applies nameprepping and the STD3 check.
    // That means a Unicode string may become an IPv4 address, but it cannot
    // produce a '[' or a '%'.

    // check for percent-encoding first
    iString s;
    if (mode == iUrl::TolerantMode && ix_urlRecode(s, iStringView{begin, end}, { }, IX_NULLPTR)) {
        // Host was percent-decoded successfully
        // Verify no illegal percent sequences remain (should all be decoded by now)
        xsizetype pos = s.indexOf(iChar(0x25)); // '%'
        if (pos != -1) {
            setError(InvalidRegNameError, s, pos);
            return false;
        }

        // Recursively validate the decoded host in strict mode
        return setHost(s, 0, s.length(), iUrl::StrictMode);
    }

    s = ix_ACE_do(value.mid(from, iend - from), NormalizeAce, ForbidLeadingDot, {});
    if (s.isEmpty()) {
        setError(InvalidRegNameError, value);
        return false;
    }

    // check IPv4 again
    if (iIPAddressUtils::parseIp4(ip4, s.constBegin(), s.constEnd())) {
        iIPAddressUtils::toString(host, ip4);
    } else {
        host = s;
    }
    return true;
}

inline void iUrlPrivate::parse(const iString &url, iUrl::ParsingMode parsingMode)
{
    //   URI-reference = URI / relative-ref
    //   URI           = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
    //   relative-ref  = relative-part [ "?" query ] [ "#" fragment ]
    //   hier-part     = "//" authority path-abempty
    //                 / other path types
    //   relative-part = "//" authority path-abempty
    //                 /  other path types here

    IX_ASSERT_X(parsingMode != iUrl::DecodedMode, "This function should only be called when parsing encoded URLs");
    IX_ASSERT(sectionIsPresent == 0);
    IX_ASSERT(!error);

    // find the important delimiters
    xsizetype colon = -1;
    xsizetype question = -1;
    xsizetype hash = -1;
    const xsizetype len = url.length();
    const iChar *const begin = url.constData();
    const xuint16 *const data = reinterpret_cast<const xuint16 *>(begin);

    for (xsizetype i = 0; i < len; ++i) {
        xuint32 uc = data[i];
        if (uc == '#' && hash == -1) {
            hash = i;

            // nothing more to be found
            break;
        }

        if (question == -1) {
            if (uc == ':' && colon == -1)
                colon = i;
            else if (uc == '?')
                question = i;
        }
    }

    // check if we have a scheme
    xsizetype hierStart;
    if (colon != -1 && setScheme(url, colon, /* don't set error */ false)) {
        hierStart = colon + 1;
    } else {
        // recover from a failed scheme: it might not have been a scheme at all
        scheme.clear();
        sectionIsPresent = 0;
        hierStart = 0;
    }

    xsizetype pathStart;
    xsizetype hierEnd = std::min<size_t>(std::min<size_t>(question, hash), len);
    if (hierEnd - hierStart >= 2 && data[hierStart] == '/' && data[hierStart + 1] == '/') {
        // we have an authority, it ends at the first slash after these
        xsizetype authorityEnd = hierEnd;
        for (xsizetype i = hierStart + 2; i < authorityEnd ; ++i) {
            if (data[i] == '/') {
                authorityEnd = i;
                break;
            }
        }

        setAuthority(url, hierStart + 2, authorityEnd, parsingMode);

        // even if we failed to set the authority properly, let's try to recover
        pathStart = authorityEnd;
        setPath(iStringView(url).sliced(pathStart, hierEnd - pathStart), parsingMode);
    } else {
        userName.clear();
        password.clear();
        host.clear();
        port = -1;
        pathStart = hierStart;

        if (hierStart < hierEnd)
            setPath(iStringView(url).sliced(hierStart, hierEnd - hierStart), parsingMode);
        else
            path.clear();
    }

    IX_ASSERT(query.isNull());
    if (size_t(question) < size_t(hash))
        setQuery(iStringView(url).sliced(question + 1, std::min<size_t>(hash, len) - question - 1),
                 parsingMode);

    IX_ASSERT(fragment.isNull());
    if (hash != -1)
        setFragment(iStringView(url).sliced(hash + 1, len - hash - 1), parsingMode);

    if (error || parsingMode == iUrl::TolerantMode)
        return;

    // The parsing so far was partially tolerant of errors, except for the
    // scheme parser (which is always strict) and the authority (which was
    // executed in strict mode).
    // If we haven't found any errors so far, continue the strict-mode parsing
    // from the path component onwards.

    if (!validateComponent(Path, url, pathStart, hierEnd))
        return;
    if (size_t(question) < size_t(hash) && !validateComponent(Query, url, question + 1, std::min<size_t>(hash, len)))
        return;
    if (hash != -1)
        validateComponent(Fragment, url, hash + 1, len);
}

iString iUrlPrivate::toLocalFile(iUrl::FormattingOptions options) const
{
    iString tmp;
    iString ourPath;
    appendPath(ourPath, options, iUrlPrivate::Path);

    // magic for shared drive on windows
    if (!host.isEmpty()) {
        tmp = iLatin1StringView("//") + host;
        #ifdef IX_OS_WIN // WebDAV is visible as local file on Windows only.
        if (scheme == webDavScheme())
            tmp += webDavSslTag();
        #endif
        if (!ourPath.isEmpty() && !ourPath.startsWith(iLatin1Char('/')))
            tmp += iLatin1Char('/');
        tmp += ourPath;
    } else {
        tmp = ourPath;
        #ifdef IX_OS_WIN
        // magic for drives on windows
        if (ourPath.length() > 2 && ourPath.at(0) == iLatin1Char('/') && ourPath.at(2) == iLatin1Char(':'))
            tmp.remove(0, 1);
        #endif
    }
    return tmp;
}

/*
    From http://www.ietf.org/rfc/rfc3986.txt, 5.2.3: Merge paths

    Returns a merge of the current path with the relative path passed
    as argument.

    Note: \a relativePath is relative (does not start with '/').
*/
inline iString iUrlPrivate::mergePaths(const iString &relativePath) const
{
    // If the base URI has a defined authority component and an empty
    // path, then return a string consisting of "/" concatenated with
    // the reference's path; otherwise,
    if (!host.isEmpty() && path.isEmpty())
        return iLatin1Char('/') + relativePath;

    // Return a string consisting of the reference's path component
    // appended to all but the last segment of the base URI's path
    // (i.e., excluding any characters after the right-most "/" in the
    // base URI path, or excluding the entire base URI path if it does
    // not contain any "/" characters).
    iString newPath;
    if (!path.contains(iLatin1Char('/')))
        newPath = relativePath;
    else
        newPath = iStringView(path).left(path.lastIndexOf(iLatin1Char('/')) + 1).toString() + relativePath;

    return newPath;
}

// Authority-less URLs cannot have paths starting with double slashes (see
// iUrlPrivate::validityError). We refuse to turn a valid URL into invalid by
// way of iUrl::resolved().
static void fixupNonAuthorityPath(iString *path)
{
    if (path->isEmpty() || path->at(0) != u'/')
        return;

    // Find the first non-slash character, because its position is equal to the
    // number of slashes. We'll remove all but one of them.
    xsizetype i = 0;
    while (i + 1 < path->size() && path->at(i + 1) == u'/')
        ++i;
    if (i)
        path->remove(0, i);
}

inline iUrlPrivate::ErrorCode iUrlPrivate::validityError(iString *source, xsizetype *position) const
{
    IX_ASSERT(!source == !position);
    if (error) {
        if (source) {
            *source = error->source;
            *position = error->position;
        }
        return error->code;
    }

    // There are three more cases of invalid URLs that iUrl recognizes and they
    // are only possible with constructed URLs (setXXX methods), not with
    // parsing. Therefore, they are tested here.
    //
    // Two cases are a non-empty path that doesn't start with a slash and:
    //  - with an authority
    //  - without an authority, without scheme but the path with a colon before
    //    the first slash
    // The third case is an empty authority and a non-empty path that starts
    // with "//".
    // Those cases are considered invalid because toString() would produce a URL
    // that wouldn't be parsed back to the same iUrl.

    if (path.isEmpty())
        return NoError;
    if (path.at(0) == iLatin1Char('/')) {
        if (hasAuthority() || path.length() == 1 || path.at(1) != iLatin1Char('/'))
            return NoError;
        if (source) {
            *source = path;
            *position = 0;
        }
        return AuthorityAbsentAndPathIsDoubleSlash;
    }

    if (sectionIsPresent & iUrlPrivate::Host) {
        if (source) {
            *source = path;
            *position = 0;
        }
        return AuthorityPresentAndPathIsRelative;
    }
    if (sectionIsPresent & iUrlPrivate::Scheme)
        return NoError;

    // check for a path of "text:text/"
    for (xsizetype i = 0; i < path.length(); ++i) {
        xuint16 c = path.at(i).unicode();
        if (c == '/') {
            // found the slash before the colon
            return NoError;
        }
        if (c == ':') {
            // found the colon before the slash, it's invalid
            if (source) {
                *source = path;
                *position = i;
            }
            return RelativeUrlPathContainsColonBeforeSlash;
        }
    }
    return NoError;
}

bool iUrlPrivate::validateComponent(iUrlPrivate::Section section, const iString &input, xsizetype begin, xsizetype end)
{
    // What we need to look out for, that the regular parser tolerates:
    //  - percent signs not followed by two hex digits
    //  - forbidden characters, which should always appear encoded
    //    '"' / '<' / '>' / '\' / '^' / '`' / '{' / '|' / '}' / BKSP
    //    control characters
    //  - delimiters not allowed in certain positions
    //    . scheme: parser is already strict
    //    . user info: gen-delims except ":" disallowed ("/" / "?" / "#" / "[" / "]" / "@")
    //    . host: parser is stricter than the standard
    //    . port: parser is stricter than the standard
    //    . path: all delimiters allowed
    //    . fragment: all delimiters allowed
    //    . query: all delimiters allowed
    static const char forbidden[] = "\"<>\\^`{|}\x7F";
    static const char forbiddenUserInfo[] = ":/?#[]@";

    IX_ASSERT(section != Authority && section != Hierarchy && section != FullUrl);

    const xuint16 *const data = reinterpret_cast<const xuint16 *>(input.constData());
    for (size_t i = size_t(begin); i < size_t(end); ++i) {
        xuint32 uc = data[i];
        if (uc >= 0x80)
            continue;

        bool error = false;
        if ((uc == '%' && (size_t(end) < i + 2 || !isHexDigit(data[i + 1]) || !isHexDigit(data[i + 2])))
                || uc <= 0x20 || strchr(forbidden, uc)) {
            // found an error
            error = true;
        } else if (section & UserInfo) {
            if (section == UserInfo && strchr(forbiddenUserInfo + 1, uc))
                error = true;
            else if (section != UserInfo && strchr(forbiddenUserInfo, uc))
                error = true;
        }

        if (!error)
            continue;

        ErrorCode errorCode = ErrorCode(int(section) << 8);
        if (section == UserInfo) {
            // is it the user name or the password?
            errorCode = InvalidUserNameError;
            for (size_t j = size_t(begin); j < i; ++j)
                if (data[j] == ':') {
                    errorCode = InvalidPasswordError;
                    break;
                }
        }

        setError(errorCode, input, i);
        return false;
    }

    // no errors
    return true;
}

/*!
    \relates iUrl

    Disables automatic conversions from iString (or char *) to iUrl.

    Compiling your code with this define is useful when you have a lot of
    code that uses iString for file names and you wish to convert it to
    use iUrl for network transparency. In any code that uses iUrl, it can
    help avoid missing iUrl::resolved() calls, and other misuses of
    iString to iUrl conversions.

    \oldcode
        url = filename; // probably not what you want
    \newcode
        url = iUrl::fromLocalFile(filename);
        url = baseurl.resolved(iUrl(filename));
    \endcode
*/


/*!
    Constructs a URL by parsing \a url. iUrl will automatically percent encode
    all characters that are not allowed in a URL and decode the percent-encoded
    sequences that represent an unreserved character (letters, digits, hyphens,
    undercores, dots and tildes). All other characters are left in their
    original forms.

    Parses the \a url using the parser mode \a parsingMode. In TolerantMode
    (the default), iUrl will correct certain mistakes, notably the presence of
    a percent character ('%') not followed by two hexadecimal digits, and it
    will accept any character in any position. In StrictMode, encoding mistakes
    will not be tolerated and iUrl will also check that certain forbidden
    characters are not present in unencoded form. If an error is detected in
    StrictMode, isValid() will return false. The parsing mode DecodedMode is not
    permitted in this context.
    To construct a URL from an encoded string, you can also use fromEncoded():


    Both functions are equivalent and, both functions accept encoded
    data. Usually, the choice of the iUrl constructor or setUrl() versus
    fromEncoded() will depend on the source data: the constructor and setUrl()
    take a iString, whereas fromEncoded takes a iByteArray.

    \sa setUrl(), fromEncoded(), TolerantMode
*/
iUrl::iUrl(const iString &url, ParsingMode parsingMode) : d(IX_NULLPTR)
{
    setUrl(url, parsingMode);
}

/*!
    Constructs an empty iUrl object.
*/
iUrl::iUrl() : d(IX_NULLPTR)
{
}

/*!
    Constructs a copy of \a other.
*/
iUrl::iUrl(const iUrl &other) : d(other.d)
{
    if (d)
        d->ref.ref();
}

/*!
    Destructor; called immediately before the object is deleted.
*/
iUrl::~iUrl()
{
    if (d && !d->ref.deref())
        delete d;
}

/*!
    Returns \c true if the URL is non-empty and valid; otherwise returns \c false.

    The URL is run through a conformance test. Every part of the URL
    must conform to the standard encoding rules of the URI standard
    for the URL to be reported as valid.

*/
bool iUrl::isValid() const
{
    if (isEmpty()) {
        // also catches d == 0
        return false;
    }
    return d->validityError() == iUrlPrivate::NoError;
}

/*!
    Returns \c true if the URL has no data; otherwise returns \c false.

    \sa clear()
*/
bool iUrl::isEmpty() const
{
    if (!d) return true;
    return d->isEmpty();
}

/*!
    Resets the content of the iUrl. After calling this function, the
    iUrl is equal to one that has been constructed with the default
    empty constructor.

    \sa isEmpty()
*/
void iUrl::clear()
{
    if (d && !d->ref.deref())
        delete d;
    d = IX_NULLPTR;
}

/*!
    Parses \a url and sets this object to that value. iUrl will automatically
    percent encode all characters that are not allowed in a URL and decode the
    percent-encoded sequences that represent an unreserved character (letters,
    digits, hyphens, undercores, dots and tildes). All other characters are
    left in their original forms.

    Parses the \a url using the parser mode \a parsingMode. In TolerantMode
    (the default), iUrl will correct certain mistakes, notably the presence of
    a percent character ('%') not followed by two hexadecimal digits, and it
    will accept any character in any position. In StrictMode, encoding mistakes
    will not be tolerated and iUrl will also check that certain forbidden
    characters are not present in unencoded form. If an error is detected in
    StrictMode, isValid() will return false. The parsing mode DecodedMode is
    not permitted in this context and will produce a run-time warning.

    \sa url(), toString()
*/
void iUrl::setUrl(const iString &url, ParsingMode parsingMode)
{
    if (parsingMode == DecodedMode) {
        ilog_warn("iUrl::DecodedMode is not permitted when parsing a full URL");
    } else {
        detachToClear();
        d->parse(url, parsingMode);
    }
}

/*!
    \fn void iUrl::setEncodedUrl(const iByteArray &encodedUrl, ParsingMode parsingMode)
    \deprecated
    Constructs a URL by parsing the contents of \a encodedUrl.

    \a encodedUrl is assumed to be a URL string in percent encoded
    form, containing only ASCII characters.

    The parsing mode \a parsingMode is used for parsing \a encodedUrl.

    \obsolete Use setUrl(iString::fromUtf8(encodedUrl), parsingMode)

    \sa setUrl()
*/

/*!
    Sets the scheme of the URL to \a scheme. As a scheme can only
    contain ASCII characters, no conversion or decoding is done on the
    input. It must also start with an ASCII letter.

    The scheme describes the type (or protocol) of the URL. It's
    represented by one or more ASCII characters at the start the URL.

    A scheme is strictly \l {http://www.ietf.org/rfc/rfc3986.txt} {RFC 3986}-compliant:
        \tt {scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )}

    The following example shows a URL where the scheme is "ftp":

    \image iurl-authority2.png

    To set the scheme, the following call is used:

    The scheme can also be empty, in which case the URL is interpreted
    as relative.

    \sa scheme(), isRelative()
*/
void iUrl::setScheme(const iString &scheme)
{
    detach();
    d->clearError();
    if (scheme.isEmpty()) {
        // schemes are not allowed to be empty
        d->sectionIsPresent &= ~iUrlPrivate::Scheme;
        d->flags &= ~iUrlPrivate::IsLocalFile;
        d->scheme.clear();
    } else {
        d->setScheme(scheme, scheme.length(), /* do set error */ true);
    }
}

/*!
    Returns the scheme of the URL. If an empty string is returned,
    this means the scheme is undefined and the URL is then relative.

    The scheme can only contain US-ASCII letters or digits, which means it
    cannot contain any character that would otherwise require encoding.
    Additionally, schemes are always returned in lowercase form.

    \sa setScheme(), isRelative()
*/
iString iUrl::scheme() const
{
    if (!d) return iString();

    return d->scheme;
}

/*!
    Sets the authority of the URL to \a authority.

    The authority of a URL is the combination of user info, a host
    name and a port. All of these elements are optional; an empty
    authority is therefore valid.

    The user info and host are separated by a '@', and the host and
    port are separated by a ':'. If the user info is empty, the '@'
    must be omitted; although a stray ':' is permitted if the port is
    empty.

    The following example shows a valid authority string:

    \image iurl-authority.png

    The \a authority data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode (the default), all characters are accepted in undecoded form
    and the tolerant parser will correct stray '%' not followed by two hex
    characters.

    This function does not allow \a mode to be iUrl::DecodedMode. To set fully
    decoded data, call setUserName(), setPassword(), setHost() and setPort()
    individually.

    \sa setUserInfo(), setHost(), setPort()
*/
void iUrl::setAuthority(const iString &authority, ParsingMode mode)
{
    detach();
    d->clearError();

    if (mode == DecodedMode) {
        ilog_warn("iUrl::DecodedMode is not permitted in this function");
        return;
    }

    d->setAuthority(authority, 0, authority.length(), mode);
}

/*!
    Returns the authority of the URL if it is defined; otherwise
    an empty string is returned.

    This function returns an unambiguous value, which may contain that
    characters still percent-encoded, plus some control sequences not
    representable in decoded form in iString.

    The \a options argument controls how to format the user info component. The
    value of iUrl::FullyDecoded is not permitted in this function. If you need
    to obtain fully decoded data, call userName(), password(), host() and
    port() individually.

    \sa setAuthority(), userInfo(), userName(), password(), host(), port()
*/
iString iUrl::authority(ComponentFormattingOptions options) const
{
    iString result;
    if (!d)
        return result;

    if (options == iUrl::FullyDecoded) {
        ilog_warn("iUrl::FullyDecoded is not permitted in this function");
        return result;
    }

    d->appendAuthority(result, options, iUrlPrivate::Authority);
    return result;
}

/*!
    Sets the user info of the URL to \a userInfo. The user info is an
    optional part of the authority of the URL, as described in
    setAuthority().

    The user info consists of a user name and optionally a password,
    separated by a ':'. If the password is empty, the colon must be
    omitted. The following example shows a valid user info string:

    \image iurl-authority3.png

    The \a userInfo data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode (the default), all characters are accepted in undecoded form
    and the tolerant parser will correct stray '%' not followed by two hex
    characters.

    This function does not allow \a mode to be iUrl::DecodedMode. To set fully
    decoded data, call setUserName() and setPassword() individually.

    \sa userInfo(), setUserName(), setPassword(), setAuthority()
*/
void iUrl::setUserInfo(const iString &userInfo, ParsingMode mode)
{
    detach();
    d->clearError();
    iString trimmed = userInfo.trimmed();
    if (mode == DecodedMode) {
        ilog_warn("iUrl::DecodedMode is not permitted in this function");
        return;
    }

    d->setUserInfo(trimmed, mode);
    if (userInfo.isNull()) {
        // iUrlPrivate::setUserInfo cleared almost everything
        // but it leaves the UserName bit set
        d->sectionIsPresent &= ~iUrlPrivate::UserInfo;
    } else if (mode == StrictMode && !d->validateComponent(iUrlPrivate::UserInfo, userInfo)) {
        d->sectionIsPresent &= ~iUrlPrivate::UserInfo;
        d->userName.clear();
        d->password.clear();
    }
}

/*!
    Returns the user info of the URL, or an empty string if the user
    info is undefined.

    This function returns an unambiguous value, which may contain that
    characters still percent-encoded, plus some control sequences not
    representable in decoded form in iString.

    The \a options argument controls how to format the user info component. The
    value of iUrl::FullyDecoded is not permitted in this function. If you need
    to obtain fully decoded data, call userName() and password() individually.

    \sa setUserInfo(), userName(), password(), authority()
*/
iString iUrl::userInfo(ComponentFormattingOptions options) const
{
    iString result;
    if (!d)
        return result;

    if (options == iUrl::FullyDecoded) {
        ilog_warn("iUrl::FullyDecoded is not permitted in this function");
        return result;
    }

    d->appendUserInfo(result, options, iUrlPrivate::UserInfo);
    return result;
}

/*!
    Sets the URL's user name to \a userName. The \a userName is part
    of the user info element in the authority of the URL, as described
    in setUserInfo().

    The \a userName data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode (the default), all characters are accepted in undecoded form
    and the tolerant parser will correct stray '%' not followed by two hex
    characters. In DecodedMode, '%' stand for themselves and encoded characters
    are not possible.

    iUrl::DecodedMode should be used when setting the user name from a data
    source which is not a URL, such as a password dialog shown to the user or
    with a user name obtained by calling userName() with the iUrl::FullyDecoded
    formatting option.

    \sa userName(), setUserInfo()
*/
void iUrl::setUserName(const iString &userName, ParsingMode mode)
{
    detach();
    d->clearError();

    d->setUserName(userName, mode);
    if (userName.isNull())
        d->sectionIsPresent &= ~iUrlPrivate::UserName;
    else if (mode == StrictMode && !d->validateComponent(iUrlPrivate::UserName, userName))
        d->userName.clear();
}

/*!
    Returns the user name of the URL if it is defined; otherwise
    an empty string is returned.

    The \a options argument controls how to format the user name component. All
    values produce an unambiguous result. With iUrl::FullyDecoded, all
    percent-encoded sequences are decoded; otherwise, the returned value may
    contain some percent-encoded sequences for some control sequences not
    representable in decoded form in iString.

    Note that iUrl::FullyDecoded may cause data loss if those non-representable
    sequences are present. It is recommended to use that value when the result
    will be used in a non-URL context, such as setting in iAuthenticator or
    negotiating a login.

    \sa setUserName(), userInfo()
*/
iString iUrl::userName(ComponentFormattingOptions options) const
{
    iString result;
    if (d)
        d->appendUserName(result, options);
    return result;
}

/*!
    \fn void iUrl::setEncodedUserName(const iByteArray &userName)
    \deprecated

    Sets the URL's user name to the percent-encoded \a userName. The \a
    userName is part of the user info element in the authority of the
    URL, as described in setUserInfo().

    \obsolete Use setUserName(iString::fromUtf8(userName))

    \sa setUserName(), encodedUserName(), setUserInfo()
*/

/*!
    \fn iByteArray iUrl::encodedUserName() const
    \deprecated

    Returns the user name of the URL if it is defined; otherwise
    an empty string is returned. The returned value will have its
    non-ASCII and other control characters percent-encoded, as in
    toEncoded().

    \obsolete Use userName(iUrl::FullyEncoded).toLatin1()

    \sa setEncodedUserName()
*/

/*!
    Sets the URL's password to \a password. The \a password is part of
    the user info element in the authority of the URL, as described in
    setUserInfo().

    The \a password data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode, all characters are accepted in undecoded form and the
    tolerant parser will correct stray '%' not followed by two hex characters.
    In DecodedMode, '%' stand for themselves and encoded characters are not
    possible.

    iUrl::DecodedMode should be used when setting the password from a data
    source which is not a URL, such as a password dialog shown to the user or
    with a password obtained by calling password() with the iUrl::FullyDecoded
    formatting option.

    \sa password(), setUserInfo()
*/
void iUrl::setPassword(const iString &password, ParsingMode mode)
{
    detach();
    d->clearError();

    d->setPassword(password, mode);
    if (password.isNull())
        d->sectionIsPresent &= ~iUrlPrivate::Password;
    else if (mode == StrictMode && !d->validateComponent(iUrlPrivate::Password, password))
        d->password.clear();
}

/*!
    Returns the password of the URL if it is defined; otherwise
    an empty string is returned.

    The \a options argument controls how to format the user name component. All
    values produce an unambiguous result. With iUrl::FullyDecoded, all
    percent-encoded sequences are decoded; otherwise, the returned value may
    contain some percent-encoded sequences for some control sequences not
    representable in decoded form in iString.

    Note that iUrl::FullyDecoded may cause data loss if those non-representable
    sequences are present. It is recommended to use that value when the result
    will be used in a non-URL context, such as setting in iAuthenticator or
    negotiating a login.

    \sa setPassword()
*/
iString iUrl::password(ComponentFormattingOptions options) const
{
    iString result;
    if (d)
        d->appendPassword(result, options);
    return result;
}

/*!
    \fn void iUrl::setEncodedPassword(const iByteArray &password)
    \deprecated

    Sets the URL's password to the percent-encoded \a password. The \a
    password is part of the user info element in the authority of the
    URL, as described in setUserInfo().

    \obsolete Use setPassword(iString::fromUtf8(password));

    \sa setPassword(), encodedPassword(), setUserInfo()
*/

/*!
    \fn iByteArray iUrl::encodedPassword() const
    \deprecated

    Returns the password of the URL if it is defined; otherwise an
    empty string is returned. The returned value will have its
    non-ASCII and other control characters percent-encoded, as in
    toEncoded().

    \obsolete Use password(iUrl::FullyEncoded).toLatin1()

    \sa setEncodedPassword(), toEncoded()
*/

/*!
    Sets the host of the URL to \a host. The host is part of the
    authority.

    The \a host data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode, all characters are accepted in undecoded form and the
    tolerant parser will correct stray '%' not followed by two hex characters.
    In DecodedMode, '%' stand for themselves and encoded characters are not
    possible.

    Note that, in all cases, the result of the parsing must be a valid hostname
    according to STD 3 rules, as modified by the Internationalized Resource
    Identifiers specification (RFC 3987). Invalid hostnames are not permitted
    and will cause isValid() to become false.

    \sa host(), setAuthority()
*/
void iUrl::setHost(const iString &host, ParsingMode mode)
{
    detach();
    d->clearError();

    iString data = host;
    if (mode == DecodedMode) {
        data.replace(u'%', "%25");
        mode = TolerantMode;
    }

    if (d->setHost(data, 0, data.length(), mode)) {
        return;
    } else if (!data.startsWith(iLatin1Char('['))) {
        // setHost failed, it might be IPv6 or IPvFuture in need of bracketing
        IX_ASSERT(d->error);

        data.prepend(iLatin1Char('['));
        data.append(iLatin1Char(']'));
        if (!d->setHost(data, 0, data.length(), mode)) {
            // Host validation failed even after percent-decoding
            if (data.contains(iLatin1Char(':'))) {
                // Presence of ':' indicates this was likely an IPv6 address parse error
                d->error->code = iUrlPrivate::InvalidIPv6AddressError;
            }
            d->sectionIsPresent &= ~iUrlPrivate::Host;
        } else {
            // Host successfully validated and set
            d->clearError();
        }
    }
}

/*!
    Returns the host of the URL if it is defined; otherwise
    an empty string is returned.

    The \a options argument controls how the hostname will be formatted. The
    iUrl::EncodeUnicode option will cause this function to return the hostname
    in the ASCII-Compatible Encoding (ACE) form, which is suitable for use in
    channels that are not 8-bit clean or that require the legacy hostname (such
    as DNS requests or in HTTP request headers). If that flag is not present,
    this function returns the International Domain Name (IDN) in Unicode form,
    according to the list of permissible top-level domains (see
    idnWhitelist()).

    All other flags are ignored. Host names cannot contain control or percent
    characters, so the returned value can be considered fully decoded.

    \sa setHost(), idnWhitelist(), setIdnWhitelist(), authority()
*/
iString iUrl::host(ComponentFormattingOptions options) const
{
    iString result;
    if (d) {
        d->appendHost(result, options);
        if (result.startsWith(iLatin1Char('[')))
            result = result.mid(1, result.length() - 2);
    }
    return result;
}

/*!
    \fn void iUrl::setEncodedHost(const iByteArray &host)
    \deprecated

    Sets the URL's host to the ACE- or percent-encoded \a host. The \a
    host is part of the user info element in the authority of the
    URL, as described in setAuthority().

    \obsolete Use setHost(iString::fromUtf8(host)).

    \sa setHost(), encodedHost(), setAuthority(), fromAce()
*/

/*!
    \fn iByteArray iUrl::encodedHost() const
    \deprecated

    Returns the host part of the URL if it is defined; otherwise
    an empty string is returned.

    Note: encodedHost() does not return percent-encoded hostnames. Instead,
    the ACE-encoded (bare ASCII in Punycode encoding) form will be
    returned for any non-ASCII hostname.

    This function is equivalent to calling iUrl::toAce() on the return
    value of host().

    \obsolete Use host(iUrl::FullyEncoded).toLatin1() or toAce(host()).

    \sa setEncodedHost()
*/

/*!
    Sets the port of the URL to \a port. The port is part of the
    authority of the URL, as described in setAuthority().

    \a port must be between 0 and 65535 inclusive. Setting the
    port to -1 indicates that the port is unspecified.
*/
void iUrl::setPort(int port)
{
    detach();
    d->clearError();

    if (port < -1 || port > 65535) {
        d->setError(iUrlPrivate::InvalidPortError, iString::number(port), 0);
        port = -1;
    }

    d->port = port;
    if (port != -1)
        d->sectionIsPresent |= iUrlPrivate::Host;
}

/*!
    Returns the port of the URL, or \a defaultPort if the port is
    unspecified.*/
int iUrl::port(int defaultPort) const
{
    if (!d) return defaultPort;
    return d->port == -1 ? defaultPort : d->port;
}

/*!
    Sets the path of the URL to \a path. The path is the part of the
    URL that comes after the authority but before the query string.

    \image iurl-ftppath.png

    For non-hierarchical schemes, the path will be everything
    following the scheme declaration, as in the following example:

    \image iurl-mailtopath.png

    The \a path data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode, all characters are accepted in undecoded form and the
    tolerant parser will correct stray '%' not followed by two hex characters.
    In DecodedMode, '%' stand for themselves and encoded characters are not
    possible.

    iUrl::DecodedMode should be used when setting the path from a data source
    which is not a URL, such as a dialog shown to the user or with a path
    obtained by calling path() with the iUrl::FullyDecoded formatting option.

    \sa path()
*/
void iUrl::setPath(const iString &path, ParsingMode mode)
{
    detach();
    d->clearError();

    d->setPath(path, mode);

    // Path null check removed - optimization since path has no delimiter marker
    // No need to clear sectionIsPresent flag as path is always considered present
    if (mode == StrictMode && !d->validateComponent(iUrlPrivate::Path, path))
        d->path.clear();
}

/*!
    Returns the path of the URL.


    The \a options argument controls how to format the path component. All
    values produce an unambiguous result. With iUrl::FullyDecoded, all
    percent-encoded sequences are decoded; otherwise, the returned value may
    contain some percent-encoded sequences for some control sequences not
    representable in decoded form in iString.

    Note that iUrl::FullyDecoded may cause data loss if those non-representable
    sequences are present. It is recommended to use that value when the result
    will be used in a non-URL context, such as sending to an FTP server.

    An example of data loss is when you have non-Unicode percent-encoded sequences
    and use FullyDecoded (the default):


    In this example, there will be some level of data loss because the \c %FF cannot
    be converted.

    Data loss can also occur when the path contains sub-delimiters (such as \c +):


    Other decoding examples:


    \sa setPath()
*/
iString iUrl::path(ComponentFormattingOptions options) const
{
    iString result;
    if (d)
        d->appendPath(result, options, iUrlPrivate::Path);
    return result;
}

/*!
    \fn void iUrl::setEncodedPath(const iByteArray &path)
    \deprecated

    Sets the URL's path to the percent-encoded \a path.  The path is
    the part of the URL that comes after the authority but before the
    query string.

    \image iurl-ftppath.png

    For non-hierarchical schemes, the path will be everything
    following the scheme declaration, as in the following example:

    \image iurl-mailtopath.png

    \obsolete Use setPath(iString::fromUtf8(path)).

    \sa setPath(), encodedPath(), setUserInfo()
*/

/*!
    \fn iByteArray iUrl::encodedPath() const
    \deprecated

    Returns the path of the URL if it is defined; otherwise an
    empty string is returned. The returned value will have its
    non-ASCII and other control characters percent-encoded, as in
    toEncoded().

    \obsolete Use path(iUrl::FullyEncoded).toLatin1().

    \sa setEncodedPath(), toEncoded()
*/

/*!
    Returns the name of the file, excluding the directory path.

    Note that, if this iUrl object is given a path ending in a slash, the name of the file is considered empty.

    If the path doesn't contain any slash, it is fully returned as the fileName.
    The \a options argument controls how to format the file name component. All
    values produce an unambiguous result. With iUrl::FullyDecoded, all
    percent-encoded sequences are decoded; otherwise, the returned value may
    contain some percent-encoded sequences for some control sequences not
    representable in decoded form in iString.

    \sa path()
*/
iString iUrl::fileName(ComponentFormattingOptions options) const
{
    const iString ourPath = path(options);
    const xsizetype slash = ourPath.lastIndexOf(iLatin1Char('/'));
    if (slash == -1)
        return ourPath;
    return ourPath.mid(slash + 1);
}

/*!
    Returns \c true if this URL contains a Query (i.e., if ? was seen on it).

    \sa setQuery(), query(), hasFragment()
*/
bool iUrl::hasQuery() const
{
    if (!d) return false;
    return d->hasQuery();
}

/*!
    Sets the query string of the URL to \a query.

    This function is useful if you need to pass a query string that
    does not fit into the key-value pattern, or that uses a different
    scheme for encoding special characters than what is suggested by
    iUrl.

    Passing a value of iString() to \a query (a null iString) unsets
    the query completely. However, passing a value of iString("")
    will set the query to an empty value, as if the original URL
    had a lone "?".

    The \a query data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode, all characters are accepted in undecoded form and the
    tolerant parser will correct stray '%' not followed by two hex characters.
    In DecodedMode, '%' stand for themselves and encoded characters are not
    possible.

    Query strings often contain percent-encoded sequences, so use of
    DecodedMode is discouraged. One special sequence to be aware of is that of
    the plus character ('+'). iUrl does not convert spaces to plus characters,
    even though HTML forms posted by web browsers do. In order to represent an
    actual plus character in a query, the sequence "%2B" is usually used. This
    function will leave "%2B" sequences untouched in TolerantMode or
    StrictMode.

    \sa query(), hasQuery()
*/
void iUrl::setQuery(const iString &query, ParsingMode mode)
{
    detach();
    d->clearError();

    d->setQuery(query, mode);
    if (query.isNull())
        d->sectionIsPresent &= ~iUrlPrivate::Query;
    else if (mode == StrictMode && !d->validateComponent(iUrlPrivate::Query, query))
        d->query.clear();
}

/*!
    \fn void iUrl::setEncodedQuery(const iByteArray &query)
    \deprecated

    Sets the query string of the URL to \a query. The string is
    inserted as-is, and no further encoding is performed when calling
    toEncoded().

    This function is useful if you need to pass a query string that
    does not fit into the key-value pattern, or that uses a different
    scheme for encoding special characters than what is suggested by
    iUrl.

    Passing a value of iByteArray() to \a query (a null iByteArray) unsets
    the query completely. However, passing a value of iByteArray("")
    will set the query to an empty value, as if the original URL
    had a lone "?".

    \obsolete Use setQuery, which has the same null / empty behavior.

    \sa encodedQuery(), hasQuery()
*/

/*!
    \fn void iUrl::setQueryItems(const std::list<std::pair<iString, iString> > &query)
    \deprecated

    Sets the query string of the URL to an encoded version of \a
    query. The contents of \a query are converted to a string
    internally, each pair delimited by the character returned by
    \l {iUrlQuery::queryPairDelimiter()}{queryPairDelimiter()}, and the key and value are delimited by
    \l {iUrlQuery::queryValueDelimiter()}{queryValueDelimiter()}

    \note This method does not encode spaces (ASCII 0x20) as plus (+) signs,
    like HTML forms do. If you need that kind of encoding, you must encode
    the value yourself and use iUrl::setEncodedQueryItems.

    \obsolete Use iUrlQuery and setQuery().

    \sa queryItems(), setEncodedQueryItems()
*/

/*!
    \fn void iUrl::setEncodedQueryItems(const std::list<std::pair<iByteArray, iByteArray> > &query)
    \deprecated

    Sets the query string of the URL to the encoded version of \a
    query. The contents of \a query are converted to a string
    internally, each pair delimited by the character returned by
    \l {iUrlQuery::queryPairDelimiter()}{queryPairDelimiter()}, and the key and value are delimited by
    \l {iUrlQuery::queryValueDelimiter()}{queryValueDelimiter()}.

    \obsolete Use iUrlQuery and setQuery().

    \sa encodedQueryItems(), setQueryItems()
*/

/*!
    \fn void iUrl::addQueryItem(const iString &key, const iString &value)
    \deprecated

    Inserts the pair \a key = \a value into the query string of the
    URL.

    The key-value pair is encoded before it is added to the query. The
    pair is converted into separate strings internally. The \a key and
    \a value is first encoded into UTF-8 and then delimited by the
    character returned by \l {iUrlQuery::queryValueDelimiter()}{queryValueDelimiter()}.
    Each key-value pair is delimited by the character returned by
    \l {iUrlQuery::queryPairDelimiter()}{queryPairDelimiter()}

    \note This method does not encode spaces (ASCII 0x20) as plus (+) signs,
    like HTML forms do. If you need that kind of encoding, you must encode
    the value yourself and use iUrl::addEncodedQueryItem.

    \obsolete Use iUrlQuery and setQuery().

    \sa addEncodedQueryItem()
*/

/*!
    \fn void iUrl::addEncodedQueryItem(const iByteArray &key, const iByteArray &value)
    \deprecated

    Inserts the pair \a key = \a value into the query string of the
    URL.

    \obsolete Use iUrlQuery and setQuery().

    \sa addQueryItem()
*/

/*!
    \fn std::list<std::pair<iString, iString> > iUrl::queryItems() const
    \deprecated

    Returns the query string of the URL, as a map of keys and values.

    \note This method does not decode spaces plus (+) signs as spaces (ASCII
    0x20), like HTML forms do. If you need that kind of decoding, you must
    use iUrl::encodedQueryItems and decode the data yourself.

    \obsolete Use iUrlQuery.

    \sa setQueryItems(), setEncodedQuery()
*/

/*!
    \fn std::list<std::pair<iByteArray, iByteArray> > iUrl::encodedQueryItems() const
    \deprecated

    Returns the query string of the URL, as a map of encoded keys and values.

    \obsolete Use iUrlQuery.

    \sa setEncodedQueryItems(), setQueryItems(), setEncodedQuery()
*/

/*!
    \fn bool iUrl::hasQueryItem(const iString &key) const
    \deprecated

    Returns \c true if there is a query string pair whose key is equal
    to \a key from the URL.

    \obsolete Use iUrlQuery.

    \sa hasEncodedQueryItem()
*/

/*!
    \fn bool iUrl::hasEncodedQueryItem(const iByteArray &key) const
    \deprecated

    Returns \c true if there is a query string pair whose key is equal
    to \a key from the URL.

    \obsolete Use iUrlQuery.

    \sa hasQueryItem()
*/

/*!
    \fn iString iUrl::queryItemValue(const iString &key) const
    \deprecated

    Returns the first query string value whose key is equal to \a key
    from the URL.

    \note This method does not decode spaces plus (+) signs as spaces (ASCII
    0x20), like HTML forms do. If you need that kind of decoding, you must
    use iUrl::encodedQueryItemValue and decode the data yourself.

    \obsolete Use iUrlQuery.

    \sa allQueryItemValues()
*/

/*!
    \fn iByteArray iUrl::encodedQueryItemValue(const iByteArray &key) const
    \deprecated

    Returns the first query string value whose key is equal to \a key
    from the URL.

    \obsolete Use iUrlQuery.

    \sa queryItemValue(), allQueryItemValues()
*/

/*!
    \fn std::list<iString> iUrl::allQueryItemValues(const iString &key) const
    \deprecated

    Returns the a list of query string values whose key is equal to
    \a key from the URL.

    \note This method does not decode spaces plus (+) signs as spaces (ASCII
    0x20), like HTML forms do. If you need that kind of decoding, you must
    use iUrl::allEncodedQueryItemValues and decode the data yourself.

    \obsolete Use iUrlQuery.

    \sa queryItemValue()
*/

/*!
    \fn std::list<iByteArray> iUrl::allEncodedQueryItemValues(const iByteArray &key) const
    \deprecated

    Returns the a list of query string values whose key is equal to
    \a key from the URL.

    \obsolete Use iUrlQuery.

    \sa allQueryItemValues(), queryItemValue(), encodedQueryItemValue()
*/

/*!
    \fn void iUrl::removeQueryItem(const iString &key)
    \deprecated

    Removes the first query string pair whose key is equal to \a key
    from the URL.

    \obsolete Use iUrlQuery.

    \sa removeAllQueryItems()
*/

/*!
    \fn void iUrl::removeEncodedQueryItem(const iByteArray &key)
    \deprecated

    Removes the first query string pair whose key is equal to \a key
    from the URL.

    \obsolete Use iUrlQuery.

    \sa removeQueryItem(), removeAllQueryItems()
*/

/*!
    \fn void iUrl::removeAllQueryItems(const iString &key)
    \deprecated

    Removes all the query string pairs whose key is equal to \a key
    from the URL.

    \obsolete Use iUrlQuery.

   \sa removeQueryItem()
*/

/*!
    \fn void iUrl::removeAllEncodedQueryItems(const iByteArray &key)
    \deprecated

    Removes all the query string pairs whose key is equal to \a key
    from the URL.

    \obsolete Use iUrlQuery.

   \sa removeQueryItem()
*/

/*!
    \fn iByteArray iUrl::encodedQuery() const
    \deprecated

    Returns the query string of the URL in percent encoded form.

    \obsolete Use query(iUrl::FullyEncoded).toLatin1()

    \sa setEncodedQuery(), query()
*/

/*!
    Returns the query string of the URL if there's a query string, or an empty
    result if not. To determine if the parsed URL contained a query string, use
    hasQuery().

    The \a options argument controls how to format the query component. All
    values produce an unambiguous result. With iUrl::FullyDecoded, all
    percent-encoded sequences are decoded; otherwise, the returned value may
    contain some percent-encoded sequences for some control sequences not
    representable in decoded form in iString.

    Note that use of iUrl::FullyDecoded in queries is discouraged, as queries
    often contain data that is supposed to remain percent-encoded, including
    the use of the "%2B" sequence to represent a plus character ('+').

    \sa setQuery(), hasQuery()
*/
iString iUrl::query(ComponentFormattingOptions options) const
{
    iString result;
    if (d) {
        d->appendQuery(result, options, iUrlPrivate::Query);
        if (d->hasQuery() && result.isNull())
            result.detach();
    }
    return result;
}

/*!
    Sets the fragment of the URL to \a fragment. The fragment is the
    last part of the URL, represented by a '#' followed by a string of
    characters. It is typically used in HTTP for referring to a
    certain link or point on a page:

    \image iurl-fragment.png

    The fragment is sometimes also referred to as the URL "reference".

    Passing an argument of iString() (a null iString) will unset the fragment.
    Passing an argument of iString("") (an empty but not null iString) will set the
    fragment to an empty string (as if the original URL had a lone "#").

    The \a fragment data is interpreted according to \a mode: in StrictMode,
    any '%' characters must be followed by exactly two hexadecimal characters
    and some characters (including space) are not allowed in undecoded form. In
    TolerantMode, all characters are accepted in undecoded form and the
    tolerant parser will correct stray '%' not followed by two hex characters.
    In DecodedMode, '%' stand for themselves and encoded characters are not
    possible.

    iUrl::DecodedMode should be used when setting the fragment from a data
    source which is not a URL or with a fragment obtained by calling
    fragment() with the iUrl::FullyDecoded formatting option.

    \sa fragment(), hasFragment()
*/
void iUrl::setFragment(const iString &fragment, ParsingMode mode)
{
    detach();
    d->clearError();

    d->setFragment(fragment, mode);
    if (fragment.isNull())
        d->sectionIsPresent &= ~iUrlPrivate::Fragment;
    else if (mode == StrictMode && !d->validateComponent(iUrlPrivate::Fragment, fragment))
        d->fragment.clear();
}

/*!
    Returns the fragment of the URL. To determine if the parsed URL contained a
    fragment, use hasFragment().

    The \a options argument controls how to format the fragment component. All
    values produce an unambiguous result. With iUrl::FullyDecoded, all
    percent-encoded sequences are decoded; otherwise, the returned value may
    contain some percent-encoded sequences for some control sequences not
    representable in decoded form in iString.

    Note that iUrl::FullyDecoded may cause data loss if those non-representable
    sequences are present. It is recommended to use that value when the result
    will be used in a non-URL context.

    \sa setFragment(), hasFragment()
*/
iString iUrl::fragment(ComponentFormattingOptions options) const
{
    iString result;
    if (d) {
        d->appendFragment(result, options, iUrlPrivate::Fragment);
        if (d->hasFragment() && result.isNull())
            result.detach();
    }
    return result;
}

/*!
    \fn void iUrl::setEncodedFragment(const iByteArray &fragment)
    \deprecated

    Sets the URL's fragment to the percent-encoded \a fragment. The fragment is the
    last part of the URL, represented by a '#' followed by a string of
    characters. It is typically used in HTTP for referring to a
    certain link or point on a page:

    \image iurl-fragment.png

    The fragment is sometimes also referred to as the URL "reference".

    Passing an argument of iByteArray() (a null iByteArray) will unset the fragment.
    Passing an argument of iByteArray("") (an empty but not null iByteArray)
    will set the fragment to an empty string (as if the original URL
    had a lone "#").

    \obsolete Use setFragment(), which has the same behavior of null / empty.

    \sa setFragment(), encodedFragment()
*/

/*!
    \fn iByteArray iUrl::encodedFragment() const
    \deprecated

    Returns the fragment of the URL if it is defined; otherwise an
    empty string is returned. The returned value will have its
    non-ASCII and other control characters percent-encoded, as in
    toEncoded().

    \obsolete Use query(iUrl::FullyEncoded).toLatin1().

    \sa setEncodedFragment(), toEncoded()
*/

/*!
    Returns \c true if this URL contains a fragment (i.e., if # was seen on it).

    \sa fragment(), setFragment()
*/
bool iUrl::hasFragment() const
{
    if (!d) return false;
    return d->hasFragment();
}

/*!
    Returns the result of the merge of this URL with \a relative. This
    URL is used as a base to convert \a relative to an absolute URL.

    If \a relative is not a relative URL, this function will return \a
    relative directly. Otherwise, the paths of the two URLs are
    merged, and the new URL returned has the scheme and authority of
    the base URL, but with the merged path, as in the following
    example:


    Calling resolved() with ".." returns a iUrl whose directory is
    one level higher than the original. Similarly, calling resolved()
    with "../.." removes two levels from the path. If \a relative is
    "/", the path becomes "/".

    \sa isRelative()
*/
iUrl iUrl::resolved(const iUrl &relative) const
{
    if (!d) return relative;
    if (!relative.d) return *this;

    iUrl t;
    if (!relative.d->scheme.isEmpty()) {
        t = relative;
        t.detach();
    } else {
        if (relative.d->hasAuthority()) {
            t = relative;
            t.detach();
        } else {
            t.d = new iUrlPrivate;

            // copy the authority
            t.d->userName = d->userName;
            t.d->password = d->password;
            t.d->host = d->host;
            t.d->port = d->port;
            t.d->sectionIsPresent = d->sectionIsPresent & iUrlPrivate::Authority;

            if (relative.d->path.isEmpty()) {
                t.d->path = d->path;
                if (relative.d->hasQuery()) {
                    t.d->query = relative.d->query;
                    t.d->sectionIsPresent |= iUrlPrivate::Query;
                } else if (d->hasQuery()) {
                    t.d->query = d->query;
                    t.d->sectionIsPresent |= iUrlPrivate::Query;
                }
            } else {
                t.d->path = relative.d->path.startsWith(iLatin1Char('/'))
                            ? relative.d->path
                            : d->mergePaths(relative.d->path);
                if (relative.d->hasQuery()) {
                    t.d->query = relative.d->query;
                    t.d->sectionIsPresent |= iUrlPrivate::Query;
                }
            }
        }
        t.d->scheme = d->scheme;
        if (d->hasScheme())
            t.d->sectionIsPresent |= iUrlPrivate::Scheme;
        else
            t.d->sectionIsPresent &= ~iUrlPrivate::Scheme;
        t.d->flags |= d->flags & iUrlPrivate::IsLocalFile;
    }
    t.d->fragment = relative.d->fragment;
    if (relative.d->hasFragment())
        t.d->sectionIsPresent |= iUrlPrivate::Fragment;
    else
        t.d->sectionIsPresent &= ~iUrlPrivate::Fragment;

    t.d->normalizePathSegments(&t.d->path);
    if (!t.d->hasAuthority()) {
        if (t.d->isLocalFile() && t.d->path.startsWith(u'/'))
            t.d->sectionIsPresent |= iUrlPrivate::Host;
        else
            fixupNonAuthorityPath(&t.d->path);
    }

    ilog_debug("iUrl(",url(), ").resolved(", relative.url(), ") = ", t.url());
    return t;
}

/*!
    Returns \c true if the URL is relative; otherwise returns \c false. A URL is
    relative reference if its scheme is undefined; this function is therefore
    equivalent to calling scheme().isEmpty().

    Relative references are defined in RFC 3986 section 4.2.

    \sa {Relative URLs vs Relative Paths}
*/
bool iUrl::isRelative() const
{
    if (!d) return true;
    return !d->hasScheme();
}

/*!
    Returns a string representation of the URL. The output can be customized by
    passing flags with \a options. The option iUrl::FullyDecoded is not
    permitted in this function since it would generate ambiguous data.

    The resulting iString can be passed back to a iUrl later on.

    Synonym for toString(options).

    \sa FormattingOptions, toEncoded(), toString()
*/
iString iUrl::url(FormattingOptions options) const
{
    return toString(options);
}

/*!
    Returns a string representation of the URL. The output can be customized by
    passing flags with \a options. The option iUrl::FullyDecoded is not
    permitted in this function since it would generate ambiguous data.

    The default formatting option is \l{iUrl::FormattingOptions}{PrettyDecoded}.

    \sa FormattingOptions, url(), setUrl()
*/
iString iUrl::toString(FormattingOptions options) const
{
    iString url;
    if (!isValid()) {
        // also catches isEmpty()
        return url;
    }
    if ((options & iUrl::FullyDecoded) == iUrl::FullyDecoded) {
        ilog_warn("iUrl::FullyDecoded is not permitted when reconstructing the full URL");
        options &= ~iUrl::FullyDecoded;
    }

    // return just the path if:
    //  - iUrl::PreferLocalFile is passed
    //  - iUrl::RemovePath isn't passed (rather stupid if the user did...)
    //  - there's no query or fragment to return
    //    that is, either they aren't present, or we're removing them
    //  - it's a local file
    if (options.testFlag(iUrl::PreferLocalFile) && !options.testFlag(iUrl::RemovePath)
            && (!d->hasQuery() || options.testFlag(iUrl::RemoveQuery))
            && (!d->hasFragment() || options.testFlag(iUrl::RemoveFragment))
            && isLocalFile()) {
        url = d->toLocalFile(options);
        return url;
    }

    // for the full URL, we consider that the reserved characters are prettier if encoded
    if (options & DecodeReserved)
        options &= ~EncodeReserved;
    else
        options |= EncodeReserved;

    if (!(options & iUrl::RemoveScheme) && d->hasScheme())
        url += d->scheme + iLatin1Char(':');

    bool pathIsAbsolute = d->path.startsWith(iLatin1Char('/'));
    if (!((options & iUrl::RemoveAuthority) == iUrl::RemoveAuthority) && d->hasAuthority()) {
        url += iLatin1StringView("//");
        d->appendAuthority(url, options, iUrlPrivate::FullUrl);
    } else if (isLocalFile() && pathIsAbsolute) {
        // Comply with the XDG file URI spec, which requires triple slashes.
        url += iLatin1StringView("//");
    }

    if (!(options & iUrl::RemovePath))
        d->appendPath(url, options, iUrlPrivate::FullUrl);

    if (!(options & iUrl::RemoveQuery) && d->hasQuery()) {
        url += iLatin1Char('?');
        d->appendQuery(url, options, iUrlPrivate::FullUrl);
    }
    if (!(options & iUrl::RemoveFragment) && d->hasFragment()) {
        url += iLatin1Char('#');
        d->appendFragment(url, options, iUrlPrivate::FullUrl);
    }

    return url;
}

/*!
    Returns a human-displayable string representation of the URL.
    The output can be customized by passing flags with \a options.
    The option RemovePassword is always enabled, since passwords
    should never be shown back to users.

    With the default options, the resulting iString can be passed back
    to a iUrl later on, but any password that was present initially will
    be lost.

    \sa FormattingOptions, toEncoded(), toString()
*/

iString iUrl::toDisplayString(FormattingOptions options) const
{
    return toString(options | RemovePassword);
}

/*!
    Returns an adjusted version of the URL.
    The output can be customized by passing flags with \a options.

    The encoding options from iUrl::ComponentFormattingOption don't make
    much sense for this method, nor does iUrl::PreferLocalFile.

    This is always equivalent to iUrl(url.toString(options)).

    \sa FormattingOptions, toEncoded(), toString()
*/
iUrl iUrl::adjusted(iUrl::FormattingOptions options) const
{
    if (!isValid()) {
        // also catches isEmpty()
        return iUrl();
    }
    iUrl that = *this;
    if (options & RemoveScheme)
        that.setScheme(iString());
    if ((options & RemoveAuthority) == RemoveAuthority) {
        that.setAuthority(iString());
    } else {
        if ((options & RemoveUserInfo) == RemoveUserInfo)
            that.setUserInfo(iString());
        else if (options & RemovePassword)
            that.setPassword(iString());
        if (options & RemovePort)
            that.setPort(-1);
    }
    if (options & RemoveQuery)
        that.setQuery(iString());
    if (options & RemoveFragment)
        that.setFragment(iString());
    if (options & RemovePath) {
        that.setPath(iString());
    } else if (options & (StripTrailingSlash | RemoveFilename | NormalizePathSegments)) {
        that.detach();
        that.d->path.resize(0);
        d->appendPath(that.d->path, options | FullyEncoded, iUrlPrivate::Path);
    }
    if (that.d->isLocalFile() && that.d->path.startsWith(u'/')) {
        // ensure absolute file URLs have an empty authority to comply with the
        // XDG file spec (note this may undo a RemoveAuthority)
        that.d->sectionIsPresent |= iUrlPrivate::Host;
    }
    return that;
}

/*!
    Returns the encoded representation of the URL if it's valid;
    otherwise an empty iByteArray is returned. The output can be
    customized by passing flags with \a options.

    The user info, path and fragment are all converted to UTF-8, and
    all non-ASCII characters are then percent encoded. The host name
    is encoded using Punycode.
*/
iByteArray iUrl::toEncoded(FormattingOptions options) const
{
    options &= ~(uint(FullyDecoded) | uint(FullyEncoded));
    return toString(options | FullyEncoded).toLatin1();
}

/*!
    \fn iUrl iUrl::fromEncoded(const iByteArray &input, ParsingMode parsingMode)

    Parses \a input and returns the corresponding iUrl. \a input is
    assumed to be in encoded form, containing only ASCII characters.

    Parses the URL using \a parsingMode. See setUrl() for more information on
    this parameter. iUrl::DecodedMode is not permitted in this context.

    \sa toEncoded(), setUrl()
*/
iUrl iUrl::fromEncoded(iByteArrayView input, ParsingMode mode)
{
    return iUrl(iString::fromUtf8(input), mode);
}

/*!
    Returns a decoded copy of \a input. \a input is first decoded from
    percent encoding, then converted from UTF-8 to unicode.

    \note Given invalid input (such as a string containing the sequence "%G5",
    which is not a valid hexadecimal number) the output will be invalid as
    well. As an example: the sequence "%G5" could be decoded to 'W'.
*/
iString iUrl::fromPercentEncoding(const iByteArray &input)
{
    iByteArray ba = iByteArray::fromPercentEncoding(input);
    return iString::fromUtf8(ba.constData(), ba.size());
}

/*!
    Returns an encoded copy of \a input. \a input is first converted
    to UTF-8, and all ASCII-characters that are not in the unreserved group
    are percent encoded. To prevent characters from being percent encoded
    pass them to \a exclude. To force characters to be percent encoded pass
    them to \a include.

    Unreserved is defined as:
       \tt {ALPHA / DIGIT / "-" / "." / "_" / "~"}

*/
iByteArray iUrl::toPercentEncoding(const iString &input, const iByteArray &exclude, const iByteArray &include)
{
    return input.toUtf8().toPercentEncoding(exclude, include);
}

/*!
    \fn iByteArray iUrl::toPunycode(const iString &uc)
    \obsolete
    Returns a \a uc in Punycode encoding.

    Punycode is a Unicode encoding used for internationalized domain
    names, as defined in RFC3492. If you want to convert a domain name from
    Unicode to its ASCII-compatible representation, use toAce().
*/

/*!
    \fn iString iUrl::fromPunycode(const iByteArray &pc)
    \obsolete
    Returns the Punycode decoded representation of \a pc.

    Punycode is a Unicode encoding used for internationalized domain
    names, as defined in RFC3492. If you want to convert a domain from
    its ASCII-compatible encoding to the Unicode representation, use
    fromAce().
*/

/*!
    Returns the Unicode form of the given domain name
    \a domain, which is encoded in the ASCII Compatible Encoding (ACE).
    The result of this function is considered equivalent to \a domain.

    If the value in \a domain cannot be encoded, it will be converted
    to iString and returned.

    The ASCII Compatible Encoding (ACE) is defined by RFC 3490, RFC 3491
    and RFC 3492. It is part of the Internationalizing Domain Names in
    Applications (IDNA) specification, which allows for domain names
    (like \c "example.com") to be written using international
    characters.
*/
iString iUrl::fromAce(const iByteArray &domain, AceProcessingOptions options)
{
    return ix_ACE_do(iString::fromLatin1(domain), NormalizeAce, ForbidLeadingDot /*FIXME: make configurable*/, options);
}

/*!
    Returns the ASCII Compatible Encoding of the given domain name \a domain.
    The result of this function is considered equivalent to \a domain.

    The ASCII-Compatible Encoding (ACE) is defined by RFC 3490, RFC 3491
    and RFC 3492. It is part of the Internationalizing Domain Names in
    Applications (IDNA) specification, which allows for domain names
    (like \c "example.com") to be written using international
    characters.

    This function returns an empty iByteArray if \a domain is not a valid
    hostname. Note, in particular, that IPv6 literals are not valid domain
    names.
*/
iByteArray iUrl::toAce(const iString &domain, AceProcessingOptions options)
{
    return ix_ACE_do(domain, ToAceOnly, ForbidLeadingDot /*FIXME: make configurable*/, options).toLatin1();
}

/*!
    \internal

    Returns \c true if this URL is "less than" the given \a url. This
    provides a means of ordering URLs.
*/
bool iUrl::operator <(const iUrl &url) const
{
    if (!d || !url.d) {
        bool thisIsEmpty = !d || d->isEmpty();
        bool thatIsEmpty = !url.d || url.d->isEmpty();

        // sort an empty URL first
        return thisIsEmpty && !thatIsEmpty;
    }

    int cmp;
    cmp = d->scheme.compare(url.d->scheme);
    if (cmp != 0)
        return cmp < 0;

    cmp = d->userName.compare(url.d->userName);
    if (cmp != 0)
        return cmp < 0;

    cmp = d->password.compare(url.d->password);
    if (cmp != 0)
        return cmp < 0;

    cmp = d->host.compare(url.d->host);
    if (cmp != 0)
        return cmp < 0;

    if (d->port != url.d->port)
        return d->port < url.d->port;

    cmp = d->path.compare(url.d->path);
    if (cmp != 0)
        return cmp < 0;

    if (d->hasQuery() != url.d->hasQuery())
        return url.d->hasQuery();

    cmp = d->query.compare(url.d->query);
    if (cmp != 0)
        return cmp < 0;

    if (d->hasFragment() != url.d->hasFragment())
        return url.d->hasFragment();

    cmp = d->fragment.compare(url.d->fragment);
    return cmp < 0;
}

/*!
    Returns \c true if this URL and the given \a url are equal;
    otherwise returns \c false.
*/
bool iUrl::operator ==(const iUrl &url) const
{
    if (!d && !url.d)
        return true;
    if (!d)
        return url.d->isEmpty();
    if (!url.d)
        return d->isEmpty();

    // First, compare which sections are present, since it speeds up the
    // processing considerably. We just have to ignore the host-is-present flag
    // for local files (the "file" protocol), due to the requirements of the
    // XDG file URI specification.
    int mask = iUrlPrivate::FullUrl;
    if (isLocalFile())
        mask &= ~iUrlPrivate::Host;
    return (d->sectionIsPresent & mask) == (url.d->sectionIsPresent & mask) &&
            d->scheme == url.d->scheme &&
            d->userName == url.d->userName &&
            d->password == url.d->password &&
            d->host == url.d->host &&
            d->port == url.d->port &&
            d->path == url.d->path &&
            d->query == url.d->query &&
            d->fragment == url.d->fragment;
}

/*!
    Returns \c true if this URL and the given \a url are equal after
    applying \a options to both; otherwise returns \c false.

    This is equivalent to calling adjusted(options) on both URLs
    and comparing the resulting urls, but faster.
*/
bool iUrl::matches(const iUrl &url, FormattingOptions options) const
{
    if (!d && !url.d)
        return true;
    if (!d)
        return url.d->isEmpty();
    if (!url.d)
        return d->isEmpty();

    // First, compare which sections are present, since it speeds up the
    // processing considerably. We just have to ignore the host-is-present flag
    // for local files (the "file" protocol), due to the requirements of the
    // XDG file URI specification.
    int mask = iUrlPrivate::FullUrl;
    if (isLocalFile())
        mask &= ~iUrlPrivate::Host;

    if (options.testFlag(iUrl::RemoveScheme))
        mask &= ~iUrlPrivate::Scheme;
    else if (d->scheme != url.d->scheme)
        return false;

    if (options.testFlag(iUrl::RemovePassword))
        mask &= ~iUrlPrivate::Password;
    else if (d->password != url.d->password)
        return false;

    if (options.testFlag(iUrl::RemoveUserInfo))
        mask &= ~iUrlPrivate::UserName;
    else if (d->userName != url.d->userName)
        return false;

    if (options.testFlag(iUrl::RemovePort))
        mask &= ~iUrlPrivate::Port;
    else if (d->port != url.d->port)
        return false;

    if (options.testFlag(iUrl::RemoveAuthority))
        mask &= ~iUrlPrivate::Host;
    else if (d->host != url.d->host)
        return false;

    if (options.testFlag(iUrl::RemoveQuery))
        mask &= ~iUrlPrivate::Query;
    else if (d->query != url.d->query)
        return false;

    if (options.testFlag(iUrl::RemoveFragment))
        mask &= ~iUrlPrivate::Fragment;
    else if (d->fragment != url.d->fragment)
        return false;

    if ((d->sectionIsPresent & mask) != (url.d->sectionIsPresent & mask))
        return false;

    if (options.testFlag(iUrl::RemovePath))
        return true;

    // Compare paths, after applying path-related options
    iString path1;
    d->appendPath(path1, options, iUrlPrivate::Path);
    iString path2;
    url.d->appendPath(path2, options, iUrlPrivate::Path);
    return path1 == path2;
}

/*!
    Returns \c true if this URL and the given \a url are not equal;
    otherwise returns \c false.
*/
bool iUrl::operator !=(const iUrl &url) const
{
    return !(*this == url);
}

/*!
    Assigns the specified \a url to this object.
*/
iUrl &iUrl::operator =(const iUrl &url)
{
    if (!d) {
        if (url.d) {
            url.d->ref.ref();
            d = url.d;
        }
    } else {
        if (url.d)
            iAtomicAssign(d, url.d);
        else
            clear();
    }
    return *this;
}

/*!
    Assigns the specified \a url to this object.
*/
iUrl &iUrl::operator =(const iString &url)
{
    if (url.isEmpty()) {
        clear();
    } else {
        detach();
        d->parse(url, TolerantMode);
    }
    return *this;
}

/*!
    \fn void iUrl::swap(iUrl &other)

    Swaps URL \a other with this URL. This operation is very
    fast and never fails.
*/

void iUrl::detach()
{
    if (!d)
        d = new iUrlPrivate;
    else
        iAtomicDetach(d);
}

void iUrl::detachToClear()
{
    if (d && (d->ref.value() == 1 || !d->ref.deref())) {
        // we had the only copy
        d->ref.initializeOwned();
        d->clear();
    } else {
        d = new iUrlPrivate;
    }
}

/*!
    \internal
*/
bool iUrl::isDetached() const
{
    return !d || d->ref.value() == 1;
}

/*!
    Returns the path of this URL formatted as a local file path. The path
    returned will use forward slashes, even if it was originally created
    from one with backslashes.

    If this URL contains a non-empty hostname, it will be encoded in the
    returned value in the form found on SMB networks (for example,
    "//servername/path/to/file.txt").


    Note: if the path component of this URL contains a non-UTF-8 binary
    sequence (such as %80), the behaviour of this function is undefined.

    \sa fromLocalFile(), isLocalFile()
*/
iString iUrl::toLocalFile() const
{
    // the call to isLocalFile() also ensures that we're parsed
    if (!isLocalFile())
        return iString();

    return d->toLocalFile(iUrl::FullyDecoded);
}

/*!
    Returns \c true if this URL is pointing to a local file path. A URL is a
    local file path if the scheme is "file".

    Note that this function considers URLs with hostnames to be local file
    paths, even if the eventual file path cannot be opened with
    iFile::open().

    \sa fromLocalFile(), toLocalFile()
*/
bool iUrl::isLocalFile() const
{
    return d && d->isLocalFile();
}

/*!
    Returns \c true if this URL is a parent of \a childUrl. \a childUrl is a child
    of this URL if the two URLs share the same scheme and authority,
    and this URL's path is a parent of the path of \a childUrl.
*/
bool iUrl::isParentOf(const iUrl &childUrl) const
{
    iString childPath = childUrl.path();

    if (!d)
        return ((childUrl.scheme().isEmpty())
            && (childUrl.authority().isEmpty())
            && childPath.length() > 0 && childPath.at(0) == iLatin1Char('/'));

    iString ourPath = path();

    return ((childUrl.scheme().isEmpty() || d->scheme == childUrl.scheme())
            && (childUrl.authority().isEmpty() || authority() == childUrl.authority())
            &&  childPath.startsWith(ourPath)
            && ((ourPath.endsWith(iLatin1Char('/')) && childPath.length() > ourPath.length())
                || (!ourPath.endsWith(iLatin1Char('/'))
                    && childPath.length() > ourPath.length() && childPath.at(ourPath.length()) == iLatin1Char('/'))));
}

static iString errorMessage(iUrlPrivate::ErrorCode errorCode, const iString &errorSource, int errorPosition)
{
    iChar c = uint(errorPosition) < uint(errorSource.length()) ?
                errorSource.at(errorPosition) : iChar(iChar::Null);

    switch (errorCode) {
    case iUrlPrivate::NoError:
        IX_ASSERT_X(false, "Impossible: iUrl::errorString should have treated this condition");
        return iString();

    case iUrlPrivate::InvalidSchemeError: {
        iString msg = iStringLiteral("Invalid scheme (character '%1' not permitted)");
        return msg.arg(c);
    }

    case iUrlPrivate::InvalidUserNameError:
        return iString(iStringLiteral("Invalid user name (character '%1' not permitted)"))
                .arg(c);

    case iUrlPrivate::InvalidPasswordError:
        return iString(iStringLiteral("Invalid password (character '%1' not permitted)"))
                .arg(c);

    case iUrlPrivate::InvalidRegNameError:
        if (errorPosition != -1)
            return iString(iStringLiteral("Invalid hostname (character '%1' not permitted)"))
                    .arg(c);
        else
            return iStringLiteral("Invalid hostname (contains invalid characters)");
    case iUrlPrivate::InvalidIPv4AddressError:
        return iString(); // doesn't happen yet
    case iUrlPrivate::InvalidIPv6AddressError:
        return iStringLiteral("Invalid IPv6 address");
    case iUrlPrivate::InvalidCharacterInIPv6Error:
        return iStringLiteral("Invalid IPv6 address (character '%1' not permitted)").arg(c);
    case iUrlPrivate::InvalidIPvFutureError:
        return iStringLiteral("Invalid IPvFuture address (character '%1' not permitted)").arg(c);
    case iUrlPrivate::HostMissingEndBracket:
        return iStringLiteral("Expected ']' to match '[' in hostname");

    case iUrlPrivate::InvalidPortError:
        return iStringLiteral("Invalid port or port number out of range");
    case iUrlPrivate::PortEmptyError:
        return iStringLiteral("Port field was empty");

    case iUrlPrivate::InvalidPathError:
        return iString(iStringLiteral("Invalid path (character '%1' not permitted)"))
                .arg(c);

    case iUrlPrivate::InvalidQueryError:
        return iString(iStringLiteral("Invalid query (character '%1' not permitted)"))
                .arg(c);

    case iUrlPrivate::InvalidFragmentError:
        return iString(iStringLiteral("Invalid fragment (character '%1' not permitted)"))
                .arg(c);

    case iUrlPrivate::AuthorityPresentAndPathIsRelative:
        return iStringLiteral("Path component is relative and authority is present");
    case iUrlPrivate::AuthorityAbsentAndPathIsDoubleSlash:
        return iStringLiteral("Path component starts with '//' and authority is absent");
    case iUrlPrivate::RelativeUrlPathContainsColonBeforeSlash:
        return iStringLiteral("Relative URL's path component contains ':' before any '/'");
    }

    IX_ASSERT_X(false, "iUrl::errorString Cannot happen, unknown error");
    return iString();
}

static inline void appendComponentIfPresent(iString &msg, bool present, const char *componentName,
                                            const iString &component)
{
    if (present) {
        msg += iLatin1StringView(componentName);
        msg += iLatin1Char('"');
        msg += component;
        msg += iLatin1StringView("\",");
    }
}

/*!
    Returns an error message if the last operation that modified this iUrl
    object ran into a parsing error. If no error was detected, this function
    returns an empty string and isValid() returns \c true.

    The error message returned by this function is technical in nature and may
    not be understood by end users. It is mostly useful to developers trying to
    understand why iUrl will not accept some input.

    \sa iUrl::ParsingMode
*/
iString iUrl::errorString() const
{
    iString msg;
    if (!d)
        return msg;

    iString errorSource;
    xsizetype errorPosition = 0;
    iUrlPrivate::ErrorCode errorCode = d->validityError(&errorSource, &errorPosition);
    if (errorCode == iUrlPrivate::NoError)
        return msg;

    msg += errorMessage(errorCode, errorSource, errorPosition);
    msg += iLatin1StringView("; source was \"");
    msg += errorSource;
    msg += iLatin1StringView("\";");
    appendComponentIfPresent(msg, d->sectionIsPresent & iUrlPrivate::Scheme,
                             " scheme = ", d->scheme);
    appendComponentIfPresent(msg, d->sectionIsPresent & iUrlPrivate::UserInfo,
                             " userinfo = ", userInfo());
    appendComponentIfPresent(msg, d->sectionIsPresent & iUrlPrivate::Host,
                             " host = ", d->host);
    appendComponentIfPresent(msg, d->port != -1,
                             " port = ", iString::number(d->port));
    appendComponentIfPresent(msg, !d->path.isEmpty(),
                             " path = ", d->path);
    appendComponentIfPresent(msg, d->sectionIsPresent & iUrlPrivate::Query,
                             " query = ", d->query);
    appendComponentIfPresent(msg, d->sectionIsPresent & iUrlPrivate::Fragment,
                             " fragment = ", d->fragment);
    if (msg.endsWith(iLatin1Char(',')))
        msg.chop(1);
    return msg;
}

/*!
    Converts a list of \a urls into a list of iString objects, using toString(\a options).
*/
std::list<iString> iUrl::toStringList(const std::list<iUrl> &urls, FormattingOptions options)
{
    std::list<iString> lst;
    for (const iUrl &url : urls)
        lst.push_back(url.toString(options));
    return lst;

}

/*!
    Converts a list of strings representing \a urls into a list of urls, using iUrl(str, \a mode).
    Note that this means all strings must be urls, not for instance local paths.
*/
std::list<iUrl> iUrl::fromStringList(const std::list<iString> &urls, ParsingMode mode)
{
    std::list<iUrl> lst;
    for (const iString &str : urls)
        lst.push_back(iUrl(str, mode));
    return lst;
}

} // namespace iShell
