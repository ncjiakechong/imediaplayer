/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilocale.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <cmath>


#include "core/utils/ilocale.h"
#include "utils/ilocale_p.h"
#include "utils/ilocale_tools_p.h"
#include "global/inumeric_p.h"
#include "utils/ilocale_data_p.h"
#include "core/io/ilog.h"
#include "core/global/iglobalstatic.h"

#define ILOG_TAG "ix_utils"

namespace iShell {

iLocale::Language iLocalePrivate::codeToLanguage(iStringView code)
{
    const xsizetype len = code.size();
    if (len != 2 && len != 3)
        return iLocale::C;
    xuint16 uc1 = code[0].toLower().unicode();
    xuint16 uc2 = code[1].toLower().unicode();
    xuint16 uc3 = len > 2 ? code[2].toLower().unicode() : 0;

    const unsigned char *c = language_code_list;
    for (; *c != 0; c += 3) {
        if (uc1 == c[0] && uc2 == c[1] && uc3 == c[2])
            return iLocale::Language((c - language_code_list)/3);
    }

    if (uc3 == 0) {
        // legacy codes
        if (uc1 == 'n' && uc2 == 'o') { // no -> nb
            IX_COMPILER_VERIFY(iLocale::Norwegian == iLocale::NorwegianBokmal);
            return iLocale::Norwegian;
        }
        if (uc1 == 't' && uc2 == 'l') { // tl -> fil
            IX_COMPILER_VERIFY(iLocale::Tagalog == iLocale::Filipino);
            return iLocale::Tagalog;
        }
        if (uc1 == 's' && uc2 == 'h') { // sh -> sr[_Latn]
            IX_COMPILER_VERIFY(iLocale::SerboCroatian == iLocale::Serbian);
            return iLocale::SerboCroatian;
        }
        if (uc1 == 'm' && uc2 == 'o') { // mo -> ro
            IX_COMPILER_VERIFY(iLocale::Moldavian == iLocale::Romanian);
            return iLocale::Moldavian;
        }
        // Android uses the following deprecated codes
        if (uc1 == 'i' && uc2 == 'w') // iw -> he
            return iLocale::Hebrew;
        if (uc1 == 'i' && uc2 == 'n') // in -> id
            return iLocale::Indonesian;
        if (uc1 == 'j' && uc2 == 'i') // ji -> yi
            return iLocale::Yiddish;
    }
    return iLocale::C;
}

iLocale::Script iLocalePrivate::codeToScript(iStringView code)
{
    const xsizetype len = code.size();
    if (len != 4)
        return iLocale::AnyScript;

    // script is titlecased in our data
    unsigned char c0 = code[0].toUpper().toLatin1();
    unsigned char c1 = code[1].toLower().toLatin1();
    unsigned char c2 = code[2].toLower().toLatin1();
    unsigned char c3 = code[3].toLower().toLatin1();

    const unsigned char *c = script_code_list;
    for (int i = 0; i < iLocale::LastScript; ++i, c += 4) {
        if (c0 == c[0] && c1 == c[1] && c2 == c[2] && c3 == c[3])
            return iLocale::Script(i);
    }
    return iLocale::AnyScript;
}

iLocale::Country iLocalePrivate::codeToCountry(iStringView code)
{
    const xsizetype len = code.size();
    if (len != 2 && len != 3)
        return iLocale::AnyCountry;

    xuint16 uc1 = code[0].toUpper().unicode();
    xuint16 uc2 = code[1].toUpper().unicode();
    xuint16 uc3 = len > 2 ? code[2].toUpper().unicode() : 0;

    const unsigned char *c = country_code_list;
    for (; *c != 0; c += 3) {
        if (uc1 == c[0] && uc2 == c[1] && uc3 == c[2])
            return iLocale::Country((c - country_code_list)/3);
    }

    return iLocale::AnyCountry;
}

iLatin1String iLocalePrivate::languageToCode(iLocale::Language language)
{
    if (language == iLocale::AnyLanguage)
        return iLatin1String();
    if (language == iLocale::C)
        return iLatin1String("C");

    const unsigned char *c = language_code_list + 3*(uint(language));

    return iLatin1String(reinterpret_cast<const char*>(c), c[2] == 0 ? 2 : 3);

}

iLatin1String iLocalePrivate::scriptToCode(iLocale::Script script)
{
    if (script == iLocale::AnyScript || script > iLocale::LastScript)
        return iLatin1String();
    const unsigned char *c = script_code_list + 4*(uint(script));
    return iLatin1String(reinterpret_cast<const char *>(c), 4);
}

iLatin1String iLocalePrivate::countryToCode(iLocale::Country country)
{
    if (country == iLocale::AnyCountry)
        return iLatin1String();

    const unsigned char *c = country_code_list + 3*(uint(country));

    return iLatin1String(reinterpret_cast<const char*>(c), c[2] == 0 ? 2 : 3);
}

// http://www.unicode.org/reports/tr35/#Likely_Subtags
static bool addLikelySubtags(iLocaleId &localeId)
{
    // ### optimize with bsearch
    const int likely_subtags_count = sizeof(likely_subtags) / sizeof(likely_subtags[0]);
    const iLocaleId *p = likely_subtags;
    const iLocaleId *const e = p + likely_subtags_count;
    for ( ; p < e; p += 2) {
        if (localeId == p[0]) {
            localeId = p[1];
            return true;
        }
    }
    return false;
}

iLocaleId iLocaleId::withLikelySubtagsAdded() const
{
    // language_script_region
    if (language_id || script_id || country_id) {
        iLocaleId id = iLocaleId::fromIds(language_id, script_id, country_id);
        if (addLikelySubtags(id))
            return id;
    }
    // language_region
    if (script_id) {
        iLocaleId id = iLocaleId::fromIds(language_id, 0, country_id);
        if (addLikelySubtags(id)) {
            id.script_id = script_id;
            return id;
        }
    }
    // language_script
    if (country_id) {
        iLocaleId id = iLocaleId::fromIds(language_id, script_id, 0);
        if (addLikelySubtags(id)) {
            id.country_id = country_id;
            return id;
        }
    }
    // language
    if (script_id && country_id) {
        iLocaleId id = iLocaleId::fromIds(language_id, 0, 0);
        if (addLikelySubtags(id)) {
            id.script_id = script_id;
            id.country_id = country_id;
            return id;
        }
    }
    // und_script
    if (language_id) {
        iLocaleId id = iLocaleId::fromIds(0, script_id, 0);
        if (addLikelySubtags(id)) {
            id.language_id = language_id;
            return id;
        }
    }
    return *this;
}

iLocaleId iLocaleId::withLikelySubtagsRemoved() const
{
    iLocaleId max = withLikelySubtagsAdded();
    // language
    {
        iLocaleId id = iLocaleId::fromIds(language_id, 0, 0);
        if (id.withLikelySubtagsAdded() == max)
            return id;
    }
    // language_region
    if (country_id) {
        iLocaleId id = iLocaleId::fromIds(language_id, 0, country_id);
        if (id.withLikelySubtagsAdded() == max)
            return id;
    }
    // language_script
    if (script_id) {
        iLocaleId id = iLocaleId::fromIds(language_id, script_id, 0);
        if (id.withLikelySubtagsAdded() == max)
            return id;
    }
    return max;
}

iByteArray iLocaleId::name(char separator) const
{
    if (language_id == iLocale::AnyLanguage)
        return iByteArray();
    if (language_id == iLocale::C)
        return iByteArrayLiteral("C");

    const unsigned char *lang = language_code_list + 3 * language_id;
    const unsigned char *script =
            (script_id != iLocale::AnyScript ? script_code_list + 4 * script_id : 0);
    const unsigned char *country =
            (country_id != iLocale::AnyCountry ? country_code_list + 3 * country_id : 0);
    char len = (lang[2] != 0 ? 3 : 2) + (script ? 4+1 : 0) + (country ? (country[2] != 0 ? 3 : 2)+1 : 0);
    iByteArray name(len, iShell::Uninitialized);
    char *uc = name.data();
    *uc++ = lang[0];
    *uc++ = lang[1];
    if (lang[2] != 0)
        *uc++ = lang[2];
    if (script) {
        *uc++ = separator;
        *uc++ = script[0];
        *uc++ = script[1];
        *uc++ = script[2];
        *uc++ = script[3];
    }
    if (country) {
        *uc++ = separator;
        *uc++ = country[0];
        *uc++ = country[1];
        if (country[2] != 0)
            *uc++ = country[2];
    }
    return name;
}

iByteArray iLocalePrivate::bcp47Name(char separator) const
{
    if (m_data->m_language_id == iLocale::AnyLanguage)
        return iByteArray();
    if (m_data->m_language_id == iLocale::C)
        return iByteArrayLiteral("en");

    iLocaleId localeId = iLocaleId::fromIds(m_data->m_language_id, m_data->m_script_id, m_data->m_country_id);
    return localeId.withLikelySubtagsRemoved().name(separator);
}

static const iLocaleData *findLocaleDataById(const iLocaleId &localeId)
{
    const uint idx = locale_index[localeId.language_id];

    const iLocaleData *data = locale_data + idx;

    if (idx == 0) // default language has no associated script or country
        return data;

    IX_ASSERT(data->m_language_id == localeId.language_id);

    if (localeId.script_id == iLocale::AnyScript && localeId.country_id == iLocale::AnyCountry)
        return data;

    if (localeId.script_id == iLocale::AnyScript) {
        do {
            if (data->m_country_id == localeId.country_id)
                return data;
            ++data;
        } while (data->m_language_id && data->m_language_id == localeId.language_id);
    } else if (localeId.country_id == iLocale::AnyCountry) {
        do {
            if (data->m_script_id == localeId.script_id)
                return data;
            ++data;
        } while (data->m_language_id && data->m_language_id == localeId.language_id);
    } else {
        do {
            if (data->m_script_id == localeId.script_id && data->m_country_id == localeId.country_id)
                return data;
            ++data;
        } while (data->m_language_id && data->m_language_id == localeId.language_id);
    }

    return 0;
}

const iLocaleData *iLocaleData::findLocaleData(iLocale::Language language, iLocale::Script script, iLocale::Country country)
{
    iLocaleId localeId = iLocaleId::fromIds(language, script, country);
    iLocaleId likelyId = localeId.withLikelySubtagsAdded();

    const uint idx = locale_index[likelyId.language_id];

    // Try a straight match with the likely data:
    if (const iLocaleData *const data = findLocaleDataById(likelyId))
        return data;
    std::list<iLocaleId> tried;
    tried.push_back(likelyId);

    // No match; try again with raw data:
    if (std::find(tried.begin(), tried.end(), localeId) == tried.end()) {
        if (const iLocaleData *const data = findLocaleDataById(localeId))
            return data;
        tried.push_back(localeId);
    }

    // No match; try again with likely country
    if (country != iLocale::AnyCountry
        && (language != iLocale::AnyLanguage || script != iLocale::AnyScript)) {
        localeId = iLocaleId::fromIds(language, script, iLocale::AnyCountry);
        likelyId = localeId.withLikelySubtagsAdded();
        if (std::find(tried.begin(), tried.end(), likelyId) == tried.end()) {
            if (const iLocaleData *const data = findLocaleDataById(likelyId))
                return data;
            tried.push_back(likelyId);
        }

        // No match; try again with any country
        if (std::find(tried.begin(), tried.end(), localeId) == tried.end()) {
            if (const iLocaleData *const data = findLocaleDataById(localeId))
                return data;
            tried.push_back(localeId);
        }
    }

    // No match; try again with likely script
    if (script != iLocale::AnyScript
        && (language != iLocale::AnyLanguage || country != iLocale::AnyCountry)) {
        localeId = iLocaleId::fromIds(language, iLocale::AnyScript, country);
        likelyId = localeId.withLikelySubtagsAdded();
        if (std::find(tried.begin(), tried.end(), likelyId) == tried.end()) {
            if (const iLocaleData *const data = findLocaleDataById(likelyId))
                return data;
            tried.push_back(likelyId);
        }

        // No match; try again with any script
        if (std::find(tried.begin(), tried.end(), localeId) == tried.end()) {
            if (const iLocaleData *const data = findLocaleDataById(localeId))
                return data;
            tried.push_back(localeId);
        }
    }

    // No match; return data at original index
    return locale_data + idx;
}

static bool parse_locale_tag(const iString &input, int &i, iString *result, const iString &separators)
{
    *result = iString(8, iShell::Uninitialized); // worst case according to BCP47
    iChar *pch = result->data();
    const iChar *uc = input.data() + i;
    const int l = input.length();
    int size = 0;
    for (; i < l && size < 8; ++i, ++size) {
        if (separators.contains(*uc))
            break;
        if (! ((uc->unicode() >= 'a' && uc->unicode() <= 'z') ||
               (uc->unicode() >= 'A' && uc->unicode() <= 'Z') ||
               (uc->unicode() >= '0' && uc->unicode() <= '9')) ) // latin only
            return false;
        *pch++ = *uc++;
    }
    result->truncate(size);
    return true;
}

bool ix_splitLocaleName(const iString &name, iString &lang, iString &script, iString &cntry)
{
    const int length = name.length();

    lang = script = cntry = iString();

    const iString separators = iStringLiteral("_-.@");
    enum ParserState { NoState, LangState, ScriptState, CountryState };
    ParserState state = LangState;
    for (int i = 0; i < length && state != NoState; ) {
        iString value;
        if (!parse_locale_tag(name, i, &value, separators) ||value.isEmpty())
            break;
        iChar sep = i < length ? name.at(i) : iChar();
        switch (state) {
        case LangState:
            if (!sep.isNull() && !separators.contains(sep)) {
                state = NoState;
                break;
            }
            lang = value;
            if (i == length) {
                // just language was specified
                state = NoState;
                break;
            }
            state = ScriptState;
            break;
        case ScriptState: {
            iString scripts = iString::fromLatin1((const char *)script_code_list, sizeof(script_code_list) - 1);
            if (value.length() == 4 && scripts.indexOf(value) % 4 == 0) {
                // script name is always 4 characters
                script = value;
                state = CountryState;
            } else {
                // it wasn't a script, maybe it is a country then?
                cntry = value;
                state = NoState;
            }
            break;
        }
        case CountryState:
            cntry = value;
            state = NoState;
            break;
        case NoState:
            // shouldn't happen
            ilog_warn("This should never happen");
            break;
        }
        ++i;
    }
    return lang.length() == 2 || lang.length() == 3;
}

void iLocalePrivate::getLangAndCountry(const iString &name, iLocale::Language &lang,
                                       iLocale::Script &script, iLocale::Country &cntry)
{
    lang = iLocale::C;
    script = iLocale::AnyScript;
    cntry = iLocale::AnyCountry;

    iString lang_code;
    iString script_code;
    iString cntry_code;
    if (!ix_splitLocaleName(name, lang_code, script_code, cntry_code))
        return;

    lang = iLocalePrivate::codeToLanguage(lang_code);
    if (lang == iLocale::C)
        return;
    script = iLocalePrivate::codeToScript(script_code);
    cntry = iLocalePrivate::codeToCountry(cntry_code);
}

static const iLocaleData *findLocaleData(const iString &name)
{
    iLocale::Language lang;
    iLocale::Script script;
    iLocale::Country cntry;
    iLocalePrivate::getLangAndCountry(name, lang, script, cntry);

    return iLocaleData::findLocaleData(lang, script, cntry);
}

iString ix_readEscapedFormatString(iStringView format, int *idx)
{
    int &i = *idx;

    IX_ASSERT(format.at(i) == iLatin1Char('\''));
    ++i;
    if (i == format.size())
        return iString();
    if (format.at(i).unicode() == '\'') { // "''" outside of a quoted stirng
        ++i;
        return iLatin1String("'");
    }

    iString result;

    while (i < format.size()) {
        if (format.at(i).unicode() == '\'') {
            if (i + 1 < format.size() && format.at(i + 1).unicode() == '\'') {
                // "''" inside of a quoted string
                result.append(iLatin1Char('\''));
                i += 2;
            } else {
                break;
            }
        } else {
            result.append(format.at(i++));
        }
    }
    if (i < format.size())
        ++i;

    return result;
}

/*!
    \internal

    Counts the number of identical leading characters in \a s.

    If \a s is empty, returns 0.

    Otherwise, returns the number of consecutive \c{s.front()}
    characters at the start of \a s.

    \code
    ix_repeatCount(u"a");   // == 1
    ix_repeatCount(u"ab");  // == 1
    ix_repeatCount(u"aab"); // == 2
    \endcode
*/
int ix_repeatCount(iStringView s)
{
    if (s.isEmpty())
        return 0;
    const iChar c = s.front();
    xsizetype j = 1;
    while (j < s.size() && s.at(j) == c)
        ++j;
    return int(j);
}

static const iLocaleData *default_data = 0;
static iLocale::NumberOptions default_number_options = iLocale::DefaultNumberOptions;

static const iLocaleData *const c_data = locale_data;
static iLocalePrivate *c_private()
{
    static iLocalePrivate c_locale = { c_data, {iRefCount(1)}, iLocale::OmitGroupSeparator };
    return &c_locale;
}

static const iLocaleData *systemData()
{
    return locale_data;
}

static const iLocaleData *defaultData()
{
    if (!default_data)
        default_data = systemData();
    return default_data;
}

const iLocaleData *iLocaleData::c()
{
    IX_ASSERT(locale_index[iLocale::C] == 0);
    return c_data;
}

static inline iString getLocaleData(const xuint16 *data, int size)
{
    return size > 0 ? iString::fromRawData(reinterpret_cast<const iChar *>(data), size) : iString();
}

static iString getLocaleListData(const xuint16 *data, int size, int index)
{
    static const xuint16 separator = ';';
    while (index && size > 0) {
        while (*data != separator)
            ++data, --size;
        --index;
        ++data;
        --size;
    }
    const xuint16 *end = data;
    while (size > 0 && *end != separator)
        ++end, --size;
    return getLocaleData(data, end - data);
}

static const int locale_data_size = sizeof(locale_data)/sizeof(iLocaleData) - 1;

IX_GLOBAL_STATIC_WITH_ARGS(iSharedDataPointer<iLocalePrivate>, defaultLocalePrivate,
                          (iLocalePrivate::create(defaultData(), default_number_options)))
IX_GLOBAL_STATIC_WITH_ARGS(iExplicitlySharedDataPointer<iLocalePrivate>, systemLocalePrivate,
                          (iLocalePrivate::create(systemData())))

static iLocalePrivate *localePrivateByName(const iString &name)
{
    if (name == iLatin1String("C"))
        return c_private();
    const iLocaleData *data = findLocaleData(name);
    return iLocalePrivate::create(data, data->m_language_id == iLocale::C ?
                                      iLocale::OmitGroupSeparator : iLocale::DefaultNumberOptions);
}

static iLocalePrivate *findLocalePrivate(iLocale::Language language, iLocale::Script script,
                                         iLocale::Country country)
{
    if (language == iLocale::C)
        return c_private();

    const iLocaleData *data = iLocaleData::findLocaleData(language, script, country);

    iLocale::NumberOptions numberOptions = iLocale::DefaultNumberOptions;

    // If not found, should default to system
    if (data->m_language_id == iLocale::C && language != iLocale::C) {
        numberOptions = default_number_options;
        data = defaultData();
    }
    return iLocalePrivate::create(data, numberOptions);
}


/*!
 \internal
*/
iLocale::iLocale(iLocalePrivate &dd)
    : d(&dd)
{}


/*!
    Constructs a iLocale object with the specified \a name,
    which has the format
    "language[_script][_country][.codeset][@modifier]" or "C", where:

    \list
    \li language is a lowercase, two-letter, ISO 639 language code (also some three-letter codes),
    \li script is a titlecase, four-letter, ISO 15924 script code,
    \li country is an uppercase, two-letter, ISO 3166 country code (also "419" as defined by United Nations),
    \li and codeset and modifier are ignored.
    \endlist

    The separator can be either underscore or a minus sign.

    If the string violates the locale format, or language is not
    a valid ISO 639 code, the "C" locale is used instead. If country
    is not present, or is not a valid ISO 3166 code, the most
    appropriate country is chosen for the specified language.

    The language, script and country codes are converted to their respective
    \c Language, \c Script and \c Country enums. After this conversion is
    performed, the constructor behaves exactly like iLocale(Country, Script,
    Language).

    This constructor is much slower than iLocale(Country, Script, Language).

    \sa bcp47Name()
*/

iLocale::iLocale(const iString &name)
    : d(localePrivateByName(name))
{
}

/*!
    Constructs a iLocale object initialized with the default locale. If
    no default locale was set using setDefault(), this locale will
    be the same as the one returned by system().

    \sa setDefault()
*/

iLocale::iLocale()
    : d(*defaultLocalePrivate)
{
    // Make sure system data is up to date
    systemData();
}

/*!
    Constructs a iLocale object with the specified \a language and \a
    country.

    \list
    \li If the language/country pair is found in the database, it is used.
    \li If the language is found but the country is not, or if the country
       is \c AnyCountry, the language is used with the most
       appropriate available country (for example, Germany for German),
    \li If neither the language nor the country are found, iLocale
       defaults to the default locale (see setDefault()).
    \endlist

    The language and country that are actually used can be queried
    using language() and country().

    \sa setDefault(), language(), country()
*/

iLocale::iLocale(Language language, Country country)
    : d(findLocalePrivate(language, iLocale::AnyScript, country))
{
}

/*!


    Constructs a iLocale object with the specified \a language, \a script and
    \a country.

    \list
    \li If the language/script/country is found in the database, it is used.
    \li If both \a script is AnyScript and \a country is AnyCountry, the
       language is used with the most appropriate available script and country
       (for example, Germany for German),
    \li If either \a script is AnyScript or \a country is AnyCountry, the
       language is used with the first locale that matches the given \a script
       and \a country.
    \li If neither the language nor the country are found, iLocale
       defaults to the default locale (see setDefault()).
    \endlist

    The language, script and country that are actually used can be queried
    using language(), script() and country().

    \sa setDefault(), language(), script(), country()
*/

iLocale::iLocale(Language language, Script script, Country country)
    : d(findLocalePrivate(language, script, country))
{
}

/*!
    Constructs a iLocale object as a copy of \a other.
*/

iLocale::iLocale(const iLocale &other)
{
    d = other.d;
}

/*!
    Destructor
*/

iLocale::~iLocale()
{
}

/*!
    Assigns \a other to this iLocale object and returns a reference
    to this iLocale object.
*/

iLocale &iLocale::operator=(const iLocale &other)
{
    d = other.d;
    return *this;
}

bool iLocale::operator==(const iLocale &other) const
{
    return d->m_data == other.d->m_data && d->m_numberOptions == other.d->m_numberOptions;
}

bool iLocale::operator!=(const iLocale &other) const
{
    return d->m_data != other.d->m_data || d->m_numberOptions != other.d->m_numberOptions;
}

/*!
    \fn void iLocale::swap(iLocale &other)


    Swaps locale \a other with this locale. This operation is very fast and
    never fails.
*/

/*!


    Sets the \a options related to number conversions for this
    iLocale instance.
*/
void iLocale::setNumberOptions(NumberOptions options)
{
    d->m_numberOptions = options;
}

/*!


    Returns the options related to number conversions for this
    iLocale instance.

    By default, no options are set for the standard locales.
*/
iLocale::NumberOptions iLocale::numberOptions() const
{
    return static_cast<NumberOptions>(d->m_numberOptions);
}

/*!


    Returns a string that represents a join of a given \a list of strings with
    a separator defined by the locale.
*/
iString iLocale::createSeparatedList(const std::list<iString> &list) const
{
    const int size = list.size();
    if (size == 1) {
        return list.front();
    } else if (size == 2) {
        iString format = getLocaleData(list_pattern_part_data + d->m_data->m_list_pattern_part_two_idx, d->m_data->m_list_pattern_part_two_size);

        std::list<iString>::const_iterator it_arg1 = list.cbegin();
        std::list<iString>::const_iterator it_arg2 = list.cbegin();
        std::advance(it_arg1, 0);
        std::advance(it_arg2, 1);
        return format.arg(*it_arg1, *it_arg2);
    } else if (size > 2) {
        iString formatStart = getLocaleData(list_pattern_part_data + d->m_data->m_list_pattern_part_start_idx, d->m_data->m_list_pattern_part_start_size);
        iString formatMid = getLocaleData(list_pattern_part_data + d->m_data->m_list_pattern_part_mid_idx, d->m_data->m_list_pattern_part_mid_size);
        iString formatEnd = getLocaleData(list_pattern_part_data + d->m_data->m_list_pattern_part_end_idx, d->m_data->m_list_pattern_part_end_size);

        std::list<iString>::const_iterator it_arg1 = list.cbegin();
        std::list<iString>::const_iterator it_arg2 = list.cbegin();
        std::advance(it_arg1, 0);
        std::advance(it_arg2, 1);
        iString result = formatStart.arg(*it_arg1, *it_arg2);
        for (int i = 2; i < size - 1; ++i) {
            it_arg1 = list.cbegin();
            std::advance(it_arg1, i);
            result = formatMid.arg(result, *it_arg1);
        }

        it_arg1 = list.cbegin();
        std::advance(it_arg1, size - 1);
        result = formatEnd.arg(result, *it_arg1);
        return result;
    }

    return iString();
}

/*!
    \nonreentrant

    Sets the global default locale to \a locale. These
    values are used when a iLocale object is constructed with
    no arguments. If this function is not called, the system's
    locale is used.

    \warning In a multithreaded application, the default locale
    should be set at application startup, before any non-GUI threads
    are created.

    \sa system(), c()
*/

void iLocale::setDefault(const iLocale &locale)
{
    default_data = locale.d->m_data;
    default_number_options = locale.numberOptions();

    if (defaultLocalePrivate.exists()) {
        // update the cached private
        *defaultLocalePrivate = locale.d;
    }
}

/*!
    Returns the language of this locale.

    \sa script(), country(), languageToString(), bcp47Name()
*/
iLocale::Language iLocale::language() const
{
    return Language(d->languageId());
}

/*!


    Returns the script of this locale.

    \sa language(), country(), languageToString(), scriptToString(), bcp47Name()
*/
iLocale::Script iLocale::script() const
{
    return Script(d->m_data->m_script_id);
}

/*!
    Returns the country of this locale.

    \sa language(), script(), countryToString(), bcp47Name()
*/
iLocale::Country iLocale::country() const
{
    return Country(d->countryId());
}

/*!
    Returns the language and country of this locale as a
    string of the form "language_country", where
    language is a lowercase, two-letter ISO 639 language code,
    and country is an uppercase, two- or three-letter ISO 3166 country code.

    Note that even if iLocale object was constructed with an explicit script,
    name() will not contain it for compatibility reasons. Use bcp47Name() instead
    if you need a full locale name.

    \sa iLocale(), language(), script(), country(), bcp47Name()
*/

iString iLocale::name() const
{
    Language l = language();
    if (l == C)
        return d->languageCode();

    Country c = country();
    if (c == AnyCountry)
        return d->languageCode();

    return d->languageCode() + iLatin1Char('_') + d->countryCode();
}

static xlonglong toIntegral_helper(const iLocaleData *d, iStringView str, bool *ok,
                                   iLocale::NumberOptions mode, xlonglong)
{
    return d->stringToLongLong(str, 10, ok, mode);
}

static xulonglong toIntegral_helper(const iLocaleData *d, iStringView str, bool *ok,
                                    iLocale::NumberOptions mode, xulonglong)
{
    return d->stringToUnsLongLong(str, 10, ok, mode);
}

template <typename T> static inline
T toIntegral_helper(const iLocalePrivate *d, iStringView str, bool *ok)
{
    using Int64 = typename std::conditional<std::is_unsigned<T>::value, xulonglong, xlonglong>::type;

    // we select the right overload by the last, unused parameter
    Int64 val = toIntegral_helper(d->m_data, str, ok, d->m_numberOptions, Int64());
    if (T(val) != val) {
        if (ok != IX_NULLPTR)
            *ok = false;
        val = 0;
    }
    return T(val);
}


/*!


    Returns the dash-separated language, script and country (and possibly other BCP47 fields)
    of this locale as a string.

    Unlike the uiLanguages() the returned value of the bcp47Name() represents
    the locale name of the iLocale data but not the language the user-interface
    should be in.

    This function tries to conform the locale name to BCP47.

    \sa language(), country(), script(), uiLanguages()
*/
iString iLocale::bcp47Name() const
{
    return iString::fromLatin1(d->bcp47Name());
}

/*!
    Returns a iString containing the name of \a language.

    \sa countryToString(), scriptToString(), bcp47Name()
*/

iString iLocale::languageToString(Language language)
{
    if (uint(language) > uint(iLocale::LastLanguage))
        return iLatin1String("Unknown");
    return iLatin1String(language_name_list + language_name_index[language]);
}

/*!
    Returns a iString containing the name of \a country.

    \sa languageToString(), scriptToString(), country(), bcp47Name()
*/

iString iLocale::countryToString(Country country)
{
    if (uint(country) > uint(iLocale::LastCountry))
        return iLatin1String("Unknown");
    return iLatin1String(country_name_list + country_name_index[country]);
}

/*!


    Returns a iString containing the name of \a script.

    \sa languageToString(), countryToString(), script(), bcp47Name()
*/
iString iLocale::scriptToString(iLocale::Script script)
{
    if (uint(script) > uint(iLocale::LastScript))
        return iLatin1String("Unknown");
    return iLatin1String(script_name_list + script_name_index[script]);
}

/*!
    Returns the short int represented by the localized string \a s.

    If the conversion fails the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toUShort(), toString()
*/

short iLocale::toShort(const iString &s, bool *ok) const
{
    return toIntegral_helper<short>(d.data(), s, ok);
}

/*!
    Returns the unsigned short int represented by the localized string \a s.

    If the conversion fails the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toShort(), toString()
*/

ushort iLocale::toUShort(const iString &s, bool *ok) const
{
    return toIntegral_helper<ushort>(d.data(), s, ok);
}

/*!
    Returns the int represented by the localized string \a s.

    If the conversion fails the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toUInt(), toString()
*/

int iLocale::toInt(const iString &s, bool *ok) const
{
    return toIntegral_helper<int>(d.data(), s, ok);
}

/*!
    Returns the unsigned int represented by the localized string \a s.

    If the conversion fails the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toInt(), toString()
*/

uint iLocale::toUInt(const iString &s, bool *ok) const
{
    return toIntegral_helper<uint>(d.data(), s, ok);
}

/*!
 Returns the long int represented by the localized string \a s.

 If the conversion fails the function returns 0.

 If \a ok is not \c IX_NULLPTR, failure is reported by setting *\a{ok}
 to \c false, and success by setting *\a{ok} to \c true.

 This function ignores leading and trailing whitespace.

 \sa toInt(), toULong(), toDouble(), toString()


 */


long iLocale::toLong(const iString &s, bool *ok) const
{
    return toIntegral_helper<long>(d.data(), s, ok);
}

/*!
 Returns the unsigned long int represented by the localized
 string \a s.

 If the conversion fails the function returns 0.

 If \a ok is not \c IX_NULLPTR, failure is reported by setting *\a{ok}
 to \c false, and success by setting *\a{ok} to \c true.

 This function ignores leading and trailing whitespace.

 \sa toLong(), toInt(), toDouble(), toString()


*/

ulong iLocale::toULong(const iString &s, bool *ok) const
{
    return toIntegral_helper<ulong>(d.data(), s, ok);
}

/*!
    Returns the long long int represented by the localized string \a s.

    If the conversion fails the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toInt(), toULongLong(), toDouble(), toString()
*/


xlonglong iLocale::toLongLong(const iString &s, bool *ok) const
{
    return toIntegral_helper<xlonglong>(d.data(), s, ok);
}

/*!
    Returns the unsigned long long int represented by the localized
    string \a s.

    If the conversion fails the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toLongLong(), toInt(), toDouble(), toString()
*/

xulonglong iLocale::toULongLong(const iString &s, bool *ok) const
{
    return toIntegral_helper<xulonglong>(d.data(), s, ok);
}

/*!
    Returns the float represented by the localized string \a s.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for any other reason (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function does not fall back to the 'C' locale if the string
    cannot be interpreted in this locale.

    This function ignores leading and trailing whitespace.

    \sa toDouble(), toInt(), toString()
*/

float iLocale::toFloat(const iString &s, bool *ok) const
{
    return iLocaleData::convertDoubleToFloat(toDouble(s, ok), ok);
}

/*!
    Returns the double represented by the localized string \a s.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for any other reason (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function does not fall back to the 'C' locale if the string
    cannot be interpreted in this locale.

    \snippet code/src_corelib_tools_qlocale.cpp 3

    Notice that the last conversion returns 1234.0, because '.' is the
    thousands group separator in the German locale.

    This function ignores leading and trailing whitespace.

    \sa toFloat(), toInt(), toString()
*/

double iLocale::toDouble(const iString &s, bool *ok) const
{
    return d->m_data->stringToDouble(s, ok, d->m_numberOptions);
}

/*!
    Returns the short int represented by the localized string \a s.

    If the conversion fails, the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toUShort(), toString()


*/

short iLocale::toShort(iStringView s, bool *ok) const
{
    return toIntegral_helper<short>(d.data(), s, ok);
}

/*!
    Returns the unsigned short int represented by the localized string \a s.

    If the conversion fails, the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toShort(), toString()


*/

ushort iLocale::toUShort(iStringView s, bool *ok) const
{
    return toIntegral_helper<ushort>(d.data(), s, ok);
}

/*!
    Returns the int represented by the localized string \a s.

    If the conversion fails, the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toUInt(), toString()


*/

int iLocale::toInt(iStringView s, bool *ok) const
{
    return toIntegral_helper<int>(d.data(), s, ok);
}

/*!
    Returns the unsigned int represented by the localized string \a s.

    If the conversion fails, the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toInt(), toString()


*/

uint iLocale::toUInt(iStringView s, bool *ok) const
{
    return toIntegral_helper<uint>(d.data(), s, ok);
}

/*!
 Returns the long int represented by the localized string \a s.

 If the conversion fails the function returns 0.

 If \a ok is not \c IX_NULLPTR, failure is reported by setting *\a{ok}
 to \c false, and success by setting *\a{ok} to \c true.

 This function ignores leading and trailing whitespace.

 \sa toInt(), toULong(), toDouble(), toString()


 */


long iLocale::toLong(iStringView s, bool *ok) const
{
    return toIntegral_helper<long>(d.data(), s, ok);
}

/*!
 Returns the unsigned long int represented by the localized
 string \a s.

 If the conversion fails the function returns 0.

 If \a ok is not \c IX_NULLPTR, failure is reported by setting *\a{ok}
 to \c false, and success by setting *\a{ok} to \c true.

 This function ignores leading and trailing whitespace.

 \sa toLong(), toInt(), toDouble(), toString()


 */

ulong iLocale::toULong(iStringView s, bool *ok) const
{
    return toIntegral_helper<ulong>(d.data(), s, ok);
}

/*!
    Returns the long long int represented by the localized string \a s.

    If the conversion fails, the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toInt(), toULongLong(), toDouble(), toString()


*/


xlonglong iLocale::toLongLong(iStringView s, bool *ok) const
{
    return toIntegral_helper<xlonglong>(d.data(), s, ok);
}

/*!
    Returns the unsigned long long int represented by the localized
    string \a s.

    If the conversion fails, the function returns 0.

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toLongLong(), toInt(), toDouble(), toString()


*/

xulonglong iLocale::toULongLong(iStringView s, bool *ok) const
{
    return toIntegral_helper<xulonglong>(d.data(), s, ok);
}

/*!
    Returns the float represented by the localized string \a s.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for any other reason (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    This function ignores leading and trailing whitespace.

    \sa toDouble(), toInt(), toString()


*/

float iLocale::toFloat(iStringView s, bool *ok) const
{
    return iLocaleData::convertDoubleToFloat(toDouble(s, ok), ok);
}

/*!
    Returns the double represented by the localized string \a s.

    Returns an infinity if the conversion overflows or 0.0 if the
    conversion fails for any other reason (e.g. underflow).

    If \a ok is not \IX_NULLPTR, failure is reported by setting *\a{ok}
    to \c false, and success by setting *\a{ok} to \c true.

    Unlike iString::toDouble(), this function does not fall back to
    the "C" locale if the string cannot be interpreted in this
    locale.

    \snippet code/src_corelib_tools_qlocale.cpp 3-qstringview

    Notice that the last conversion returns 1234.0, because '.' is the
    thousands group separator in the German locale.

    This function ignores leading and trailing whitespace.

    \sa toFloat(), toInt(), toString()


*/

double iLocale::toDouble(iStringView s, bool *ok) const
{
    return d->m_data->stringToDouble(s, ok, d->m_numberOptions);
}

/*!
    Returns a localized string representation of \a i.

    \sa toLongLong()
*/

iString iLocale::toString(xlonglong i) const
{
    int flags = d->m_numberOptions & OmitGroupSeparator
                    ? 0
                    : iLocaleData::ThousandsGroup;

    return d->m_data->longLongToString(i, -1, 10, -1, flags);
}

/*!
    \overload

    \sa toULongLong()
*/

iString iLocale::toString(xulonglong i) const
{
    int flags = d->m_numberOptions & OmitGroupSeparator
                    ? 0
                    : iLocaleData::ThousandsGroup;

    return d->m_data->unsLongLongToString(i, -1, 10, -1, flags);
}

/*!


    Returns the date format used for the current locale.

    If \a format is LongFormat the format will be a long version.
    Otherwise it uses a shorter version.
*/

iString iLocale::dateFormat(FormatType format) const
{
    xuint32 idx, size;
    switch (format) {
    case LongFormat:
        idx = d->m_data->m_long_date_format_idx;
        size = d->m_data->m_long_date_format_size;
        break;
    default:
        idx = d->m_data->m_short_date_format_idx;
        size = d->m_data->m_short_date_format_size;
        break;
    }
    return getLocaleData(date_format_data + idx, size);
}

/*!


    Returns the time format used for the current locale.

    If \a format is LongFormat the format will be a long version.
    Otherwise it uses a shorter version.
*/

iString iLocale::timeFormat(FormatType format) const
{
    xuint32 idx, size;
    switch (format) {
    case LongFormat:
        idx = d->m_data->m_long_time_format_idx;
        size = d->m_data->m_long_time_format_size;
        break;
    default:
        idx = d->m_data->m_short_time_format_idx;
        size = d->m_data->m_short_time_format_size;
        break;
    }
    return getLocaleData(time_format_data + idx, size);
}

/*!


    Returns the date time format used for the current locale.

    If \a format is ShortFormat the format will be a short version.
    Otherwise it uses a longer version.
*/

iString iLocale::dateTimeFormat(FormatType format) const
{
    return dateFormat(format) + iLatin1Char(' ') + timeFormat(format);
}

/*!


    Returns the decimal point character of this locale.
*/
iChar iLocale::decimalPoint() const
{
    return d->decimal();
}

/*!


    Returns the group separator character of this locale.
*/
iChar iLocale::groupSeparator() const
{
    return d->group();
}

/*!


    Returns the percent character of this locale.
*/
iChar iLocale::percent() const
{
    return d->percent();
}

/*!


    Returns the zero digit character of this locale.
*/
iChar iLocale::zeroDigit() const
{
    return d->zero();
}

/*!


    Returns the negative sign character of this locale.
*/
iChar iLocale::negativeSign() const
{
    return d->minus();
}

/*!


    Returns the positive sign character of this locale.
*/
iChar iLocale::positiveSign() const
{
    return d->plus();
}

/*!


    Returns the exponential character of this locale.
*/
iChar iLocale::exponential() const
{
    return d->exponential();
}

static bool iIsUpper(char c)
{
    return c >= 'A' && c <= 'Z';
}

static char iToLower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}

/*!
    \overload

    \a f and \a prec have the same meaning as in iString::number(double, char, int).

    \sa toDouble()
*/

iString iLocale::toString(double i, char f, int prec) const
{
    iLocaleData::DoubleForm form = iLocaleData::DFDecimal;
    uint flags = 0;

    if (iIsUpper(f))
        flags = iLocaleData::CapitalEorX;
    f = iToLower(f);

    switch (f) {
        case 'f':
            form = iLocaleData::DFDecimal;
            break;
        case 'e':
            form = iLocaleData::DFExponent;
            break;
        case 'g':
            form = iLocaleData::DFSignificantDigits;
            break;
        default:
            break;
    }

    if (!(d->m_numberOptions & OmitGroupSeparator))
        flags |= iLocaleData::ThousandsGroup;
    if (!(d->m_numberOptions & OmitLeadingZeroInExponent))
        flags |= iLocaleData::ZeroPadExponent;
    if (d->m_numberOptions & IncludeTrailingZeroesAfterDot)
        flags |= iLocaleData::AddTrailingZeroes;
    return d->m_data->doubleToString(i, prec, form, -1, flags);
}

/*!
    \fn iLocale iLocale::c()

    Returns a iLocale object initialized to the "C" locale.

    This locale is based on en_US but with various quirks of its own, such as
    simplified number formatting and its own date formatting. It implements the
    POSIX standards that describe the behavior of standard library functions of
    the "C" programming language.

    Among other things, this means its collation order is based on the ASCII
    values of letters, so that (for case-sensitive sorting) all upper-case
    letters sort before any lower-case one (rather than each letter's upper- and
    lower-case forms sorting adjacent to one another, before the next letter's
    two forms).

    \sa system()
*/

/*!
    Returns a iLocale object initialized to the system locale.

    On Windows and Mac, this locale will use the decimal/grouping characters and date/time
    formats specified in the system configuration panel.

    \sa c()
*/

iLocale iLocale::system()
{
    if (systemLocalePrivate.isDestroyed())
        return iLocale(iLocale::C);
    return iLocale(*systemLocalePrivate->data());
}


/*!


    Returns the localized name of \a month, in the format specified
    by \a type.

    \sa dayName(), standaloneMonthName()
*/
iString iLocale::monthName(int month, FormatType type) const
{
    if (month < 1 || month > 12)
        return iString();

    xuint32 idx, size;
    switch (type) {
    case iLocale::LongFormat:
        idx = d->m_data->m_long_month_names_idx;
        size = d->m_data->m_long_month_names_size;
        break;
    case iLocale::ShortFormat:
        idx = d->m_data->m_short_month_names_idx;
        size = d->m_data->m_short_month_names_size;
        break;
    case iLocale::NarrowFormat:
        idx = d->m_data->m_narrow_month_names_idx;
        size = d->m_data->m_narrow_month_names_size;
        break;
    default:
        return iString();
    }
    return getLocaleListData(months_data + idx, size, month - 1);
}

/*!


    Returns the localized name of \a month that is used as a
    standalone text, in the format specified by \a type.

    If the locale information doesn't specify the standalone month
    name then return value is the same as in monthName().

    \sa monthName(), standaloneDayName()
*/
iString iLocale::standaloneMonthName(int month, FormatType type) const
{
    if (month < 1 || month > 12)
        return iString();

    xuint32 idx, size;
    switch (type) {
    case iLocale::LongFormat:
        idx = d->m_data->m_standalone_long_month_names_idx;
        size = d->m_data->m_standalone_long_month_names_size;
        break;
    case iLocale::ShortFormat:
        idx = d->m_data->m_standalone_short_month_names_idx;
        size = d->m_data->m_standalone_short_month_names_size;
        break;
    case iLocale::NarrowFormat:
        idx = d->m_data->m_standalone_narrow_month_names_idx;
        size = d->m_data->m_standalone_narrow_month_names_size;
        break;
    default:
        return iString();
    }
    iString name = getLocaleListData(months_data + idx, size, month - 1);
    if (name.isEmpty())
        return monthName(month, type);
    return name;
}

/*!


    Returns the localized name of the \a day (where 1 represents
    Monday, 2 represents Tuesday and so on), in the format specified
    by \a type.

    \sa monthName(), standaloneDayName()
*/
iString iLocale::dayName(int day, FormatType type) const
{
    if (day < 1 || day > 7)
        return iString();

    if (day == 7)
        day = 0;

    xuint32 idx, size;
    switch (type) {
    case iLocale::LongFormat:
        idx = d->m_data->m_long_day_names_idx;
        size = d->m_data->m_long_day_names_size;
        break;
    case iLocale::ShortFormat:
        idx = d->m_data->m_short_day_names_idx;
        size = d->m_data->m_short_day_names_size;
        break;
    case iLocale::NarrowFormat:
        idx = d->m_data->m_narrow_day_names_idx;
        size = d->m_data->m_narrow_day_names_size;
        break;
    default:
        return iString();
    }
    return getLocaleListData(days_data + idx, size, day);
}

/*!


    Returns the localized name of the \a day (where 1 represents
    Monday, 2 represents Tuesday and so on) that is used as a
    standalone text, in the format specified by \a type.

    If the locale information does not specify the standalone day
    name then return value is the same as in dayName().

    \sa dayName(), standaloneMonthName()
*/
iString iLocale::standaloneDayName(int day, FormatType type) const
{
    if (day < 1 || day > 7)
        return iString();

    if (day == 7)
        day = 0;

    xuint32 idx, size;
    switch (type) {
    case iLocale::LongFormat:
        idx = d->m_data->m_standalone_long_day_names_idx;
        size = d->m_data->m_standalone_long_day_names_size;
        break;
    case iLocale::ShortFormat:
        idx = d->m_data->m_standalone_short_day_names_idx;
        size = d->m_data->m_standalone_short_day_names_size;
        break;
    case iLocale::NarrowFormat:
        idx = d->m_data->m_standalone_narrow_day_names_idx;
        size = d->m_data->m_standalone_narrow_day_names_size;
        break;
    default:
        return iString();
    }
    iString name = getLocaleListData(days_data + idx, size, day);
    if (name.isEmpty())
        return dayName(day == 0 ? 7 : day, type);
    return name;
}

iLocale::MeasurementSystem iLocalePrivate::measurementSystem() const
{
    for (int i = 0; i < ImperialMeasurementSystemsCount; ++i) {
        if (ImperialMeasurementSystems[i].languageId == m_data->m_language_id
            && ImperialMeasurementSystems[i].countryId == m_data->m_country_id) {
            return ImperialMeasurementSystems[i].system;
        }
    }
    return iLocale::MetricSystem;
}

/*!


    Returns the measurement system for the locale.
*/
iLocale::MeasurementSystem iLocale::measurementSystem() const
{
    return d->measurementSystem();
}

/*!


  Returns the text direction of the language.
*/
iShell::LayoutDirection iLocale::textDirection() const
{
    switch (script()) {
    case iLocale::AdlamScript:
    case iLocale::ArabicScript:
    case iLocale::AvestanScript:
    case iLocale::CypriotScript:
    case iLocale::HatranScript:
    case iLocale::HebrewScript:
    case iLocale::ImperialAramaicScript:
    case iLocale::InscriptionalPahlaviScript:
    case iLocale::InscriptionalParthianScript:
    case iLocale::KharoshthiScript:
    case iLocale::LydianScript:
    case iLocale::MandaeanScript:
    case iLocale::ManichaeanScript:
    case iLocale::MendeKikakuiScript:
    case iLocale::MeroiticCursiveScript:
    case iLocale::MeroiticScript:
    case iLocale::NabataeanScript:
    case iLocale::NkoScript:
    case iLocale::OldHungarianScript:
    case iLocale::OldNorthArabianScript:
    case iLocale::OldSouthArabianScript:
    case iLocale::OrkhonScript:
    case iLocale::PalmyreneScript:
    case iLocale::PhoenicianScript:
    case iLocale::PsalterPahlaviScript:
    case iLocale::SamaritanScript:
    case iLocale::SyriacScript:
    case iLocale::ThaanaScript:
        return iShell::RightToLeft;
    default:
        break;
    }
    return iShell::LeftToRight;
}

/*!


  Returns an uppercase copy of \a str.

  If iShell Core is using the ICU libraries, they will be used to perform
  the transformation according to the rules of the current locale.
  Otherwise the conversion may be done in a platform-dependent manner,
  with iString::toUpper() as a generic fallback.

  \sa iString::toUpper()
*/
iString iLocale::toUpper(const iString &str) const
{
    return str.toUpper();
}

/*!


  Returns a lowercase copy of \a str.

  If iShell Core is using the ICU libraries, they will be used to perform
  the transformation according to the rules of the current locale.
  Otherwise the conversion may be done in a platform-dependent manner,
  with iString::toLower() as a generic fallback.

  \sa iString::toLower()
*/
iString iLocale::toLower(const iString &str) const
{
    return str.toLower();
}


/*!


    Returns the localized name of the "AM" suffix for times specified using
    the conventions of the 12-hour clock.

    \sa pmText()
*/
iString iLocale::amText() const
{
    return getLocaleData(am_data + d->m_data->m_am_idx, d->m_data->m_am_size);
}

/*!


    Returns the localized name of the "PM" suffix for times specified using
    the conventions of the 12-hour clock.

    \sa amText()
*/
iString iLocale::pmText() const
{
    return getLocaleData(pm_data + d->m_data->m_pm_idx, d->m_data->m_pm_size);
}

iString iLocaleData::doubleToString(double d, int precision, DoubleForm form,
                                    int width, unsigned flags) const
{
    return doubleToString(m_zero, m_plus, m_minus, m_exponential, m_group, m_decimal,
                          d, precision, form, width, flags);
}

iString iLocaleData::doubleToString(const iChar _zero, const iChar plus, const iChar minus,
                                    const iChar exponential, const iChar group, const iChar decimal,
                                    double d, int precision, DoubleForm form, int width, unsigned flags)
{
    if (precision != iLocale::FloatingPointShortest && precision < 0)
        precision = 6;
    if (width < 0)
        width = 0;

    bool negative = false;
    iString num_str;

    int decpt;
    int bufSize = 1;
    if (precision == iLocale::FloatingPointShortest)
        bufSize += DoubleMaxSignificant;
    else if (form == DFDecimal) // optimize for numbers between -512k and 512k
        bufSize += ((d > (1 << 19) || d < -(1 << 19)) ? DoubleMaxDigitsBeforeDecimal : 6) +
                precision;
    else // Add extra digit due to different interpretations of precision. Also, "nan" has to fit.
        bufSize += std::max(2, precision) + 1;

    iVarLengthArray<char> buf(bufSize);
    int length;

    ix_doubleToAscii(d, form, precision, buf.data(), bufSize, negative, length, decpt);

    if (istrncmp(buf.data(), "inf", 3) == 0 || istrncmp(buf.data(), "nan", 3) == 0) {
        num_str = iString::fromLatin1(buf.data(), length);
    } else { // Handle normal numbers
        iString digits = iString::fromLatin1(buf.data(), length);

        if (_zero.unicode() != '0') {
            xuint16 z = _zero.unicode() - '0';
            for (int i = 0; i < digits.length(); ++i)
                reinterpret_cast<xuint16 *>(digits.data())[i] += z;
        }

        bool always_show_decpt = (flags & ForcePoint);
        switch (form) {
            case DFExponent: {
                num_str = exponentForm(_zero, decimal, exponential, group, plus, minus,
                                       digits, decpt, precision, PMDecimalDigits,
                                       always_show_decpt, flags & ZeroPadExponent);
                break;
            }
            case DFDecimal: {
                num_str = decimalForm(_zero, decimal, group,
                                      digits, decpt, precision, PMDecimalDigits,
                                      always_show_decpt, flags & ThousandsGroup);
                break;
            }
            case DFSignificantDigits: {
                PrecisionMode mode = (flags & AddTrailingZeroes) ?
                            PMSignificantDigits : PMChopTrailingZeros;

                int cutoff = precision < 0 ? 6 : precision;
                // Find out which representation is shorter
                if (precision == iLocale::FloatingPointShortest && decpt > 0) {
                    cutoff = digits.length() + 4; // 'e', '+'/'-', one digit exponent
                    if (decpt <= 10) {
                        ++cutoff;
                    } else {
                        cutoff += decpt > 100 ? 2 : 1;
                    }
                    if (!always_show_decpt && digits.length() > decpt)
                        ++cutoff; // decpt shown in exponent form, but not in decimal form
                }

                if (decpt != digits.length() && (decpt <= -4 || decpt > cutoff))
                    num_str = exponentForm(_zero, decimal, exponential, group, plus, minus,
                                           digits, decpt, precision, mode,
                                           always_show_decpt, flags & ZeroPadExponent);
                else
                    num_str = decimalForm(_zero, decimal, group,
                                          digits, decpt, precision, mode,
                                          always_show_decpt, flags & ThousandsGroup);
                break;
            }
        }

        if (isZero(d))
            negative = false;

        // pad with zeros. LeftAdjusted overrides this flag). Also, we don't
        // pad special numbers
        if (flags & iLocaleData::ZeroPadded && !(flags & iLocaleData::LeftAdjusted)) {
            int num_pad_chars = width - num_str.length();
            // leave space for the sign
            if (negative
                    || flags & iLocaleData::AlwaysShowSign
                    || flags & iLocaleData::BlankBeforePositive)
                --num_pad_chars;

            for (int i = 0; i < num_pad_chars; ++i)
                num_str.prepend(_zero);
        }
    }

    // add sign
    if (negative)
        num_str.prepend(minus);
    else if (flags & iLocaleData::AlwaysShowSign)
        num_str.prepend(plus);
    else if (flags & iLocaleData::BlankBeforePositive)
        num_str.prepend(iLatin1Char(' '));

    if (flags & iLocaleData::CapitalEorX)
        num_str = num_str.toUpper();

    return num_str;
}

iString iLocaleData::longLongToString(xlonglong l, int precision,
                                            int base, int width,
                                            unsigned flags) const
{
    return longLongToString(m_zero, m_group, m_plus, m_minus,
                                            l, precision, base, width, flags);
}

iString iLocaleData::longLongToString(const iChar zero, const iChar group,
                                         const iChar plus, const iChar minus,
                                         xlonglong l, int precision,
                                         int base, int width,
                                         unsigned flags)
{
    bool precision_not_specified = false;
    if (precision == -1) {
        precision_not_specified = true;
        precision = 1;
    }

    bool negative = l < 0;
    if (base != 10) {
        // these are not supported by sprintf for octal and hex
        flags &= ~AlwaysShowSign;
        flags &= ~BlankBeforePositive;
        negative = false; // neither are negative numbers
    }

    /*
      Negating std::numeric_limits<xlonglong>::min() hits undefined behavior, so
      taking an absolute value has to cast to unsigned to change sign.
     */
    iString num_str = iulltoa(negative ? -xulonglong(l) : xulonglong(l), base, zero);

    uint cnt_thousand_sep = 0;
    if (flags & ThousandsGroup && base == 10) {
        for (int i = num_str.length() - 3; i > 0; i -= 3) {
            num_str.insert(i, group);
            ++cnt_thousand_sep;
        }
    }

    for (int i = num_str.length()/* - cnt_thousand_sep*/; i < precision; ++i)
        num_str.prepend(base == 10 ? zero : iChar::fromLatin1('0'));

    if ((flags & ShowBase)
            && base == 8
            && (num_str.isEmpty() || num_str[0].unicode() != iLatin1Char('0')))
        num_str.prepend(iLatin1Char('0'));

    // LeftAdjusted overrides this flag ZeroPadded. sprintf only padds
    // when precision is not specified in the format string
    bool zero_padded = flags & ZeroPadded
                        && !(flags & LeftAdjusted)
                        && precision_not_specified;

    if (zero_padded) {
        int num_pad_chars = width - num_str.length();

        // leave space for the sign
        if (negative
                || flags & AlwaysShowSign
                || flags & BlankBeforePositive)
            --num_pad_chars;

        // leave space for optional '0x' in hex form
        if (base == 16 && (flags & ShowBase))
            num_pad_chars -= 2;
        // leave space for optional '0b' in binary form
        else if (base == 2 && (flags & ShowBase))
            num_pad_chars -= 2;

        for (int i = 0; i < num_pad_chars; ++i)
            num_str.prepend(base == 10 ? zero : iChar::fromLatin1('0'));
    }

    if (flags & CapitalEorX)
        num_str = num_str.toUpper();

    if (base == 16 && (flags & ShowBase))
        num_str.prepend(iLatin1String(flags & UppercaseBase ? "0X" : "0x"));
    if (base == 2 && (flags & ShowBase))
        num_str.prepend(iLatin1String(flags & UppercaseBase ? "0B" : "0b"));

    // add sign
    if (negative)
        num_str.prepend(minus);
    else if (flags & AlwaysShowSign)
        num_str.prepend(plus);
    else if (flags & BlankBeforePositive)
        num_str.prepend(iLatin1Char(' '));

    return num_str;
}

iString iLocaleData::unsLongLongToString(xulonglong l, int precision,
                                            int base, int width,
                                            unsigned flags) const
{
    return unsLongLongToString(m_zero, m_group, m_plus,
                                               l, precision, base, width, flags);
}

iString iLocaleData::unsLongLongToString(const iChar zero, const iChar group,
                                            const iChar plus,
                                            xulonglong l, int precision,
                                            int base, int width,
                                            unsigned flags)
{
    const iChar resultZero = base == 10 ? zero : iChar(iLatin1Char('0'));
    iString num_str = l ? iulltoa(l, base, zero) : iString(resultZero);

    bool precision_not_specified = false;
    if (precision == -1) {
        if (flags == NoFlags)
            return num_str; // fast-path: nothing below applies, so we're done.

        precision_not_specified = true;
        precision = 1;
    }

    uint cnt_thousand_sep = 0;
    if (flags & ThousandsGroup && base == 10) {
        for (int i = num_str.length() - 3; i > 0; i -=3) {
            num_str.insert(i, group);
            ++cnt_thousand_sep;
        }
    }

    const int zeroPadding = precision - num_str.length()/* + cnt_thousand_sep*/;
    if (zeroPadding > 0)
        num_str.prepend(iString(zeroPadding, resultZero));

    if ((flags & ShowBase)
            && base == 8
            && (num_str.isEmpty() || num_str.at(0).unicode() != iLatin1Char('0')))
        num_str.prepend(iLatin1Char('0'));

    // LeftAdjusted overrides this flag ZeroPadded. sprintf only padds
    // when precision is not specified in the format string
    bool zero_padded = flags & ZeroPadded
                        && !(flags & LeftAdjusted)
                        && precision_not_specified;

    if (zero_padded) {
        int num_pad_chars = width - num_str.length();

        // leave space for optional '0x' in hex form
        if (base == 16 && flags & ShowBase)
            num_pad_chars -= 2;
        // leave space for optional '0b' in binary form
        else if (base == 2 && flags & ShowBase)
            num_pad_chars -= 2;

        if (num_pad_chars > 0)
            num_str.prepend(iString(num_pad_chars, resultZero));
    }

    if (flags & CapitalEorX)
        num_str = num_str.toUpper();

    if (base == 16 && flags & ShowBase)
        num_str.prepend(iLatin1String(flags & UppercaseBase ? "0X" : "0x"));
    else if (base == 2 && flags & ShowBase)
        num_str.prepend(iLatin1String(flags & UppercaseBase ? "0B" : "0b"));

    // add sign
    if (flags & AlwaysShowSign)
        num_str.prepend(plus);
    else if (flags & BlankBeforePositive)
        num_str.prepend(iLatin1Char(' '));

    return num_str;
}

/*
    Converts a number in locale to its representation in the C locale.
    Only has to guarantee that a string that is a correct representation of
    a number will be converted. If junk is passed in, junk will be passed
    out and the error will be detected during the actual conversion to a
    number. We can't detect junk here, since we don't even know the base
    of the number.
*/
bool iLocaleData::numberToCLocale(iStringView s, iLocale::NumberOptions number_options,
                                  CharBuff *result) const
{
    const iChar *uc = s.data();
    xsizetype l = s.size();
    decltype(l) idx = 0;

    // Skip whitespace
    while (idx < l && uc[idx].isSpace())
        ++idx;
    if (idx == l)
        return false;

    // Check trailing whitespace
    for (; idx < l; --l) {
        if (!uc[l - 1].isSpace())
            break;
    }

    int group_cnt = 0; // counts number of group chars
    int decpt_idx = -1;
    int last_separator_idx = -1;
    int start_of_digits_idx = -1;
    int exponent_idx = -1;

    while (idx < l) {
        const iChar in = uc[idx];

        char out = digitToCLocale(in);
        if (out == 0) {
            if (in == m_list)
                out = ';';
            else if (in == m_percent)
                out = '%';
            // for handling base-x numbers
            else if (in.unicode() >= 'A' && in.unicode() <= 'Z')
                out = in.toLower().toLatin1();
            else if (in.unicode() >= 'a' && in.unicode() <= 'z')
                out = in.toLatin1();
            else
                break;
        } else if (out == '.') {
            // Fail if more than one decimal point or point after e
            if (decpt_idx != -1 || exponent_idx != -1)
                return false;
            decpt_idx = idx;
        } else if (out == 'e' || out == 'E') {
            exponent_idx = idx;
        }

        if (number_options & iLocale::RejectLeadingZeroInExponent) {
            if (exponent_idx != -1 && out == '0' && idx < l - 1) {
                // After the exponent there can only be '+', '-' or digits.
                // If we find a '0' directly after some non-digit, then that is a leading zero.
                if (result->last() < '0' || result->last() > '9')
                    return false;
            }
        }

        if (number_options & iLocale::RejectTrailingZeroesAfterDot) {
            // If we've seen a decimal point and the last character after the exponent is 0, then
            // that is a trailing zero.
            if (decpt_idx >= 0 && idx == exponent_idx && result->last() == '0')
                    return false;
        }

        if (!(number_options & iLocale::RejectGroupSeparator)) {
            if (start_of_digits_idx == -1 && out >= '0' && out <= '9') {
                start_of_digits_idx = idx;
            } else if (out == ',') {
                // Don't allow group chars after the decimal point or exponent
                if (decpt_idx != -1 || exponent_idx != -1)
                    return false;

                // check distance from the last separator or from the beginning of the digits
                // ### FIXME: Some locales allow other groupings! See https://en.wikipedia.org/wiki/Thousands_separator
                if (last_separator_idx != -1 && idx - last_separator_idx != 4)
                    return false;
                if (last_separator_idx == -1 && (start_of_digits_idx == -1 || idx - start_of_digits_idx > 3))
                    return false;

                last_separator_idx = idx;
                ++group_cnt;

                // don't add the group separator
                ++idx;
                continue;
            } else if (out == '.' || out == 'e' || out == 'E') {
                // check distance from the last separator
                // ### FIXME: Some locales allow other groupings! See https://en.wikipedia.org/wiki/Thousands_separator
                if (last_separator_idx != -1 && idx - last_separator_idx != 4)
                    return false;

                // stop processing separators
                last_separator_idx = -1;
            }
        }

        result->append(out);

        ++idx;
    }

    if (!(number_options & iLocale::RejectGroupSeparator)) {
        // group separator post-processing
        // did we end in a separator?
        if (last_separator_idx + 1 == idx)
            return false;
        // were there enough digits since the last separator?
        if (last_separator_idx != -1 && idx - last_separator_idx != 4)
            return false;
    }

    if (number_options & iLocale::RejectTrailingZeroesAfterDot) {
        // In decimal form, the last character can be a trailing zero if we've seen a decpt.
        if (decpt_idx != -1 && exponent_idx == -1 && result->last() == '0')
            return false;
    }

    result->append('\0');
    return idx == l;
}

bool iLocaleData::validateChars(iStringView str, NumberMode numMode, iByteArray *buff,
                                int decDigits, iLocale::NumberOptions number_options) const
{
    buff->clear();
    buff->reserve(str.length());

    const bool scientific = numMode == DoubleScientificMode;
    bool lastWasE = false;
    bool lastWasDigit = false;
    int eCnt = 0;
    int decPointCnt = 0;
    bool dec = false;
    int decDigitCnt = 0;

    for (xsizetype i = 0; i < str.size(); ++i) {
        char c = digitToCLocale(str.at(i));

        if (c >= '0' && c <= '9') {
            if (numMode != IntegerMode) {
                // If a double has too many digits after decpt, it shall be Invalid.
                if (dec && decDigits != -1 && decDigits < ++decDigitCnt)
                    return false;
            }

            // The only non-digit character after the 'e' can be '+' or '-'.
            // If a zero is directly after that, then the exponent is zero-padded.
            if ((number_options & iLocale::RejectLeadingZeroInExponent) && c == '0' && eCnt > 0 &&
                    !lastWasDigit)
                return false;

            lastWasDigit = true;
        } else {
            switch (c) {
                case '.':
                    if (numMode == IntegerMode) {
                        // If an integer has a decimal point, it shall be Invalid.
                        return false;
                    } else {
                        // If a double has more than one decimal point, it shall be Invalid.
                        if (++decPointCnt > 1)
                            return false;
                        #if 0
                        // If a double with no decimal digits has a decimal point, it shall be
                        // Invalid.
                        if (decDigits == 0)
                            return false;
                        #endif // On second thoughts, it shall be Valid.

                        dec = true;
                    }
                    break;

                case '+':
                case '-':
                    if (scientific) {
                        // If a scientific has a sign that's not at the beginning or after
                        // an 'e', it shall be Invalid.
                        if (i != 0 && !lastWasE)
                            return false;
                    } else {
                        // If a non-scientific has a sign that's not at the beginning,
                        // it shall be Invalid.
                        if (i != 0)
                            return false;
                    }
                    break;

                case ',':
                    //it can only be placed after a digit which is before the decimal point
                    if ((number_options & iLocale::RejectGroupSeparator) || !lastWasDigit ||
                            decPointCnt > 0)
                        return false;
                    break;

                case 'e':
                    if (scientific) {
                        // If a scientific has more than one 'e', it shall be Invalid.
                        if (++eCnt > 1)
                            return false;
                        dec = false;
                    } else {
                        // If a non-scientific has an 'e', it shall be Invalid.
                        return false;
                    }
                    break;

                default:
                    // If it's not a valid digit, it shall be Invalid.
                    return false;
            }
            lastWasDigit = false;
        }

        lastWasE = c == 'e';
        if (c != ',')
            buff->append(c);
    }

    return true;
}

double iLocaleData::stringToDouble(iStringView str, bool *ok,
                                   iLocale::NumberOptions number_options) const
{
    CharBuff buff;
    if (!numberToCLocale(str, number_options, &buff)) {
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0.0;
    }
    int processed = 0;
    bool nonNullOk = false;
    double d = ix_asciiToDouble(buff.constData(), buff.length() - 1, nonNullOk, processed);
    if (ok != IX_NULLPTR)
        *ok = nonNullOk;
    return d;
}

xlonglong iLocaleData::stringToLongLong(iStringView str, int base, bool *ok,
                                        iLocale::NumberOptions number_options) const
{
    CharBuff buff;
    if (!numberToCLocale(str, number_options, &buff)) {
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0;
    }

    return bytearrayToLongLong(buff.constData(), base, ok);
}

xulonglong iLocaleData::stringToUnsLongLong(iStringView str, int base, bool *ok,
                                            iLocale::NumberOptions number_options) const
{
    CharBuff buff;
    if (!numberToCLocale(str, number_options, &buff)) {
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0;
    }

    return bytearrayToUnsLongLong(buff.constData(), base, ok);
}

double iLocaleData::bytearrayToDouble(const char *num, bool *ok)
{
    bool nonNullOk = false;
    int len = static_cast<int>(strlen(num));
    IX_ASSERT(len >= 0);
    int processed = 0;
    double d = ix_asciiToDouble(num, len, nonNullOk, processed);
    if (ok != IX_NULLPTR)
        *ok = nonNullOk;
    return d;
}

xlonglong iLocaleData::bytearrayToLongLong(const char *num, int base, bool *ok)
{
    bool _ok;
    const char *endptr;

    if (*num == '\0') {
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0;
    }

    xlonglong l = istrtoll(num, &endptr, base, &_ok);

    if (!_ok) {
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0;
    }

    if (*endptr != '\0') {
        while (ascii_isspace(*endptr))
            ++endptr;
    }

    if (*endptr != '\0') {
        // we stopped at a non-digit character after converting some digits
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0;
    }

    if (ok != IX_NULLPTR)
        *ok = true;
    return l;
}

xulonglong iLocaleData::bytearrayToUnsLongLong(const char *num, int base, bool *ok)
{
    bool _ok;
    const char *endptr;
    xulonglong l = istrtoull(num, &endptr, base, &_ok);

    if (!_ok) {
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0;
    }

    if (*endptr != '\0') {
        while (ascii_isspace(*endptr))
            ++endptr;
    }

    if (*endptr != '\0') {
        if (ok != IX_NULLPTR)
            *ok = false;
        return 0;
    }

    if (ok != IX_NULLPTR)
        *ok = true;
    return l;
}

/*!


    \enum iLocale::CurrencySymbolFormat

    Specifies the format of the currency symbol.

    \value CurrencyIsoCode a ISO-4217 code of the currency.
    \value CurrencySymbol a currency symbol.
    \value CurrencyDisplayName a user readable name of the currency.
*/

/*!

    Returns a currency symbol according to the \a format.
*/
iString iLocale::currencySymbol(iLocale::CurrencySymbolFormat format) const
{
    xuint32 idx, size;
    switch (format) {
    case CurrencySymbol:
        idx = d->m_data->m_currency_symbol_idx;
        size = d->m_data->m_currency_symbol_size;
        return getLocaleData(currency_symbol_data + idx, size);
    case CurrencyDisplayName:
        idx = d->m_data->m_currency_display_name_idx;
        size = d->m_data->m_currency_display_name_size;
        return getLocaleListData(currency_display_name_data + idx, size, 0);
    case CurrencyIsoCode: {
        int len = 0;
        const iLocaleData *data = this->d->m_data;
        for (; len < 3; ++len)
            if (!data->m_currency_iso_code[len])
                break;
        return len ? iString::fromLatin1(data->m_currency_iso_code, len) : iString();
    }
    }
    return iString();
}

/*!


    Returns a localized string representation of \a value as a currency.
    If the \a symbol is provided it is used instead of the default currency symbol.

    \sa currencySymbol()
*/
iString iLocale::toCurrencyString(xlonglong value, const iString &symbol) const
{
    const iLocalePrivate *d = this->d.data();
    xuint8 idx = d->m_data->m_currency_format_idx;
    xuint8 size = d->m_data->m_currency_format_size;
    if (d->m_data->m_currency_negative_format_size && value < 0) {
        idx = d->m_data->m_currency_negative_format_idx;
        size = d->m_data->m_currency_negative_format_size;
        value = -value;
    }
    iString str = toString(value);
    iString sym = symbol.isNull() ? currencySymbol() : symbol;
    if (sym.isEmpty())
        sym = currencySymbol(iLocale::CurrencyIsoCode);
    iString format = getLocaleData(currency_format_data + idx, size);
    return format.arg(str, sym);
}

/*!

    \overload
*/
iString iLocale::toCurrencyString(xulonglong value, const iString &symbol) const
{
    const iLocaleData *data = this->d->m_data;
    xuint8 idx = data->m_currency_format_idx;
    xuint8 size = data->m_currency_format_size;
    iString str = toString(value);
    iString sym = symbol.isNull() ? currencySymbol() : symbol;
    if (sym.isEmpty())
        sym = currencySymbol(iLocale::CurrencyIsoCode);
    iString format = getLocaleData(currency_format_data + idx, size);
    return format.arg(str, sym);
}

/*!

    \overload toCurrencyString()

    Returns a localized string representation of \a value as a currency.
    If the \a symbol is provided it is used instead of the default currency symbol.
    If the \a precision is provided it is used to set the precision of the currency value.

    \sa currencySymbol()
 */
iString iLocale::toCurrencyString(double value, const iString &symbol, int precision) const
{
    const iLocaleData *data = this->d->m_data;
    xuint8 idx = data->m_currency_format_idx;
    xuint8 size = data->m_currency_format_size;
    if (data->m_currency_negative_format_size && value < 0) {
        idx = data->m_currency_negative_format_idx;
        size = data->m_currency_negative_format_size;
        value = -value;
    }
    iString str = toString(value, 'f', precision == -1 ? d->m_data->m_currency_digits : precision);
    iString sym = symbol.isNull() ? currencySymbol() : symbol;
    if (sym.isEmpty())
        sym = currencySymbol(iLocale::CurrencyIsoCode);
    iString format = getLocaleData(currency_format_data + idx, size);
    return format.arg(str, sym);
}

/*!
  \fn iString iLocale::toCurrencyString(float i, const iString &symbol) const
  \fn iString iLocale::toCurrencyString(float i, const iString &symbol, int precision) const
  \overload toCurrencyString()
*/

/*!


    \enum iLocale::DataSizeFormat

    Specifies the format for representation of data quantities.

    \omitvalue DataSizeBase1000
    \omitvalue DataSizeSIQuantifiers
    \value DataSizeIecFormat            format using base 1024 and IEC prefixes: KiB, MiB, GiB, ...
    \value DataSizeTraditionalFormat    format using base 1024 and SI prefixes: kB, MB, GB, ...
    \value DataSizeSIFormat             format using base 1000 and SI prefixes: kB, MB, GB, ...

    \sa formattedDataSize()
*/


/*!


    Converts a size in bytes to a human-readable localized string, comprising a
    number and a quantified unit. The quantifier is chosen such that the number
    is at least one, and as small as possible. For example if \a bytes is
    16384, \a precision is 2, and \a format is \l DataSizeIecFormat (the
    default), this function returns "16.00 KiB"; for 1330409069609 bytes it
    returns "1.21 GiB"; and so on. If \a format is \l DataSizeIecFormat or
    \l DataSizeTraditionalFormat, the given number of bytes is divided by a
    power of 1024, with result less than 1024; for \l DataSizeSIFormat, it is
    divided by a power of 1000, with result less than 1000.
    \c DataSizeIecFormat uses the new IEC standard quantifiers Ki, Mi and so on,
    whereas \c DataSizeSIFormat uses the older SI quantifiers k, M, etc., and
    \c DataSizeTraditionalFormat abuses them.
*/
iString iLocale::formattedDataSize(xint64 bytes, int precision, DataSizeFormats format) const
{
    int power, base = 1000;
    if (!bytes) {
        power = 0;
    } else if (format & DataSizeBase1000) {
        power = int(std::log10(std::abs(bytes)) / 3);
    } else { // Compute log2(bytes) / 10:
        power = int((63 - iCountLeadingZeroBits(xuint64(std::abs(bytes)))) / 10);
        base = 1024;
    }
    // Only go to doubles if we'll be using a quantifier:
    const iString number = power
        ? toString(bytes / std::pow(double(base), power), 'f', std::min(precision, 3 * power))
        : toString(bytes);

    // We don't support sizes in units larger than exbibytes because
    // the number of bytes would not fit into xint64.
    IX_ASSERT(power <= 6 && power >= 0);
    iString unit;
    if (power > 0) {
        xuint16 index, size;
        if (format & DataSizeSIQuantifiers) {
            index = d->m_data->m_byte_si_quantified_idx;
            size = d->m_data->m_byte_si_quantified_size;
        } else {
            index = d->m_data->m_byte_iec_quantified_idx;
            size = d->m_data->m_byte_iec_quantified_size;
        }
        unit = getLocaleListData(byte_unit_data + index, size, power - 1);
    } else {
        unit = getLocaleData(byte_unit_data + d->m_data->m_byte_idx, d->m_data->m_byte_size);
    }

    return number + iLatin1Char(' ') + unit;
}

/*!


    Returns a native name of the language for the locale. For example
    "Schwiizertüütsch" for Swiss-German locale.

    \sa nativeCountryName(), languageToString()
*/
iString iLocale::nativeLanguageName() const
{
    return getLocaleData(endonyms_data + d->m_data->m_language_endonym_idx, d->m_data->m_language_endonym_size);
}

/*!


    Returns a native name of the country for the locale. For example
    "España" for Spanish/Spain locale.

    \sa nativeLanguageName(), countryToString()
*/
iString iLocale::nativeCountryName() const
{
    return getLocaleData(endonyms_data + d->m_data->m_country_endonym_idx, d->m_data->m_country_endonym_size);
}

} // namespace iShell
