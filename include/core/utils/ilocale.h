/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ilocale.h
/// @brief   provides a comprehensive set of tools for handling locale-specific data, 
///          such as language, country, script, number formatting, date and time formatting,
///          currency formatting, and text direction.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ILOCALE_H
#define ILOCALE_H

#include <core/kernel/ivariant.h>
#include <core/utils/istring.h>
#include <core/utils/ishareddata.h>

namespace iShell {

class iLocalePrivate;

class IX_CORE_EXPORT iLocale
{
    friend class iString;
    friend class iByteArray;
    friend class iIntValidator;

public:
// GENERATED PART STARTS HERE
// see ilocale_data_p.h for more info on generated data
    enum Language {
        AnyLanguage = 0,
        C = 1,
        Abkhazian = 2,
        Oromo = 3,
        Afar = 4,
        Afrikaans = 5,
        Albanian = 6,
        Amharic = 7,
        Arabic = 8,
        Armenian = 9,
        Assamese = 10,
        Aymara = 11,
        Azerbaijani = 12,
        Bashkir = 13,
        Basque = 14,
        Bengali = 15,
        Dzongkha = 16,
        Bihari = 17,
        Bislama = 18,
        Breton = 19,
        Bulgarian = 20,
        Burmese = 21,
        Belarusian = 22,
        Khmer = 23,
        Catalan = 24,
        Chinese = 25,
        Corsican = 26,
        Croatian = 27,
        Czech = 28,
        Danish = 29,
        Dutch = 30,
        English = 31,
        Esperanto = 32,
        Estonian = 33,
        Faroese = 34,
        Fijian = 35,
        Finnish = 36,
        French = 37,
        WesternFrisian = 38,
        Gaelic = 39,
        Galician = 40,
        Georgian = 41,
        German = 42,
        Greek = 43,
        Greenlandic = 44,
        Guarani = 45,
        Gujarati = 46,
        Hausa = 47,
        Hebrew = 48,
        Hindi = 49,
        Hungarian = 50,
        Icelandic = 51,
        Indonesian = 52,
        Interlingua = 53,
        Interlingue = 54,
        Inuktitut = 55,
        Inupiak = 56,
        Irish = 57,
        Italian = 58,
        Japanese = 59,
        Javanese = 60,
        Kannada = 61,
        Kashmiri = 62,
        Kazakh = 63,
        Kinyarwanda = 64,
        Kirghiz = 65,
        Korean = 66,
        Kurdish = 67,
        Rundi = 68,
        Lao = 69,
        Latin = 70,
        Latvian = 71,
        Lingala = 72,
        Lithuanian = 73,
        Macedonian = 74,
        Malagasy = 75,
        Malay = 76,
        Malayalam = 77,
        Maltese = 78,
        Maori = 79,
        Marathi = 80,
        Marshallese = 81,
        Mongolian = 82,
        NauruLanguage = 83,
        Nepali = 84,
        NorwegianBokmal = 85,
        Occitan = 86,
        Oriya = 87,
        Pashto = 88,
        Persian = 89,
        Polish = 90,
        Portuguese = 91,
        Punjabi = 92,
        Quechua = 93,
        Romansh = 94,
        Romanian = 95,
        Russian = 96,
        Samoan = 97,
        Sango = 98,
        Sanskrit = 99,
        Serbian = 100,
        Ossetic = 101,
        SouthernSotho = 102,
        Tswana = 103,
        Shona = 104,
        Sindhi = 105,
        Sinhala = 106,
        Swati = 107,
        Slovak = 108,
        Slovenian = 109,
        Somali = 110,
        Spanish = 111,
        Sundanese = 112,
        Swahili = 113,
        Swedish = 114,
        Sardinian = 115,
        Tajik = 116,
        Tamil = 117,
        Tatar = 118,
        Telugu = 119,
        Thai = 120,
        Tibetan = 121,
        Tigrinya = 122,
        Tongan = 123,
        Tsonga = 124,
        Turkish = 125,
        Turkmen = 126,
        Tahitian = 127,
        Uighur = 128,
        Ukrainian = 129,
        Urdu = 130,
        Uzbek = 131,
        Vietnamese = 132,
        Volapuk = 133,
        Welsh = 134,
        Wolof = 135,
        Xhosa = 136,
        Yiddish = 137,
        Yoruba = 138,
        Zhuang = 139,
        Zulu = 140,
        NorwegianNynorsk = 141,
        Bosnian = 142,
        Divehi = 143,
        Manx = 144,
        Cornish = 145,
        Akan = 146,
        Konkani = 147,
        Ga = 148,
        Igbo = 149,
        Kamba = 150,
        Syriac = 151,
        Blin = 152,
        Geez = 153,
        Koro = 154,
        Sidamo = 155,
        Atsam = 156,
        Tigre = 157,
        Jju = 158,
        Friulian = 159,
        Venda = 160,
        Ewe = 161,
        Walamo = 162,
        Hawaiian = 163,
        Tyap = 164,
        Nyanja = 165,
        Filipino = 166,
        SwissGerman = 167,
        SichuanYi = 168,
        Kpelle = 169,
        LowGerman = 170,
        SouthNdebele = 171,
        NorthernSotho = 172,
        NorthernSami = 173,
        Taroko = 174,
        Gusii = 175,
        Taita = 176,
        Fulah = 177,
        Kikuyu = 178,
        Samburu = 179,
        Sena = 180,
        NorthNdebele = 181,
        Rombo = 182,
        Tachelhit = 183,
        Kabyle = 184,
        Nyankole = 185,
        Bena = 186,
        Vunjo = 187,
        Bambara = 188,
        Embu = 189,
        Cherokee = 190,
        Morisyen = 191,
        Makonde = 192,
        Langi = 193,
        Ganda = 194,
        Bemba = 195,
        Kabuverdianu = 196,
        Meru = 197,
        Kalenjin = 198,
        Nama = 199,
        Machame = 200,
        Colognian = 201,
        Masai = 202,
        Soga = 203,
        Luyia = 204,
        Asu = 205,
        Teso = 206,
        Saho = 207,
        KoyraChiini = 208,
        Rwa = 209,
        Luo = 210,
        Chiga = 211,
        CentralMoroccoTamazight = 212,
        KoyraboroSenni = 213,
        Shambala = 214,
        Bodo = 215,
        Avaric = 216,
        Chamorro = 217,
        Chechen = 218,
        Church = 219,
        Chuvash = 220,
        Cree = 221,
        Haitian = 222,
        Herero = 223,
        HiriMotu = 224,
        Kanuri = 225,
        Komi = 226,
        Kongo = 227,
        Kwanyama = 228,
        Limburgish = 229,
        LubaKatanga = 230,
        Luxembourgish = 231,
        Navaho = 232,
        Ndonga = 233,
        Ojibwa = 234,
        Pali = 235,
        Walloon = 236,
        Aghem = 237,
        Basaa = 238,
        Zarma = 239,
        Duala = 240,
        JolaFonyi = 241,
        Ewondo = 242,
        Bafia = 243,
        MakhuwaMeetto = 244,
        Mundang = 245,
        Kwasio = 246,
        Nuer = 247,
        Sakha = 248,
        Sangu = 249,
        CongoSwahili = 250,
        Tasawaq = 251,
        Vai = 252,
        Walser = 253,
        Yangben = 254,
        Avestan = 255,
        Asturian = 256,
        Ngomba = 257,
        Kako = 258,
        Meta = 259,
        Ngiemboon = 260,
        Aragonese = 261,
        Akkadian = 262,
        AncientEgyptian = 263,
        AncientGreek = 264,
        Aramaic = 265,
        Balinese = 266,
        Bamun = 267,
        BatakToba = 268,
        Buginese = 269,
        Buhid = 270,
        Carian = 271,
        Chakma = 272,
        ClassicalMandaic = 273,
        Coptic = 274,
        Dogri = 275,
        EasternCham = 276,
        EasternKayah = 277,
        Etruscan = 278,
        Gothic = 279,
        Hanunoo = 280,
        Ingush = 281,
        LargeFloweryMiao = 282,
        Lepcha = 283,
        Limbu = 284,
        Lisu = 285,
        Lu = 286,
        Lycian = 287,
        Lydian = 288,
        Mandingo = 289,
        Manipuri = 290,
        Meroitic = 291,
        NorthernThai = 292,
        OldIrish = 293,
        OldNorse = 294,
        OldPersian = 295,
        OldTurkish = 296,
        Pahlavi = 297,
        Parthian = 298,
        Phoenician = 299,
        PrakritLanguage = 300,
        Rejang = 301,
        Sabaean = 302,
        Samaritan = 303,
        Santali = 304,
        Saurashtra = 305,
        Sora = 306,
        Sylheti = 307,
        Tagbanwa = 308,
        TaiDam = 309,
        TaiNua = 310,
        Ugaritic = 311,
        Akoose = 312,
        Lakota = 313,
        StandardMoroccanTamazight = 314,
        Mapuche = 315,
        CentralKurdish = 316,
        LowerSorbian = 317,
        UpperSorbian = 318,
        Kenyang = 319,
        Mohawk = 320,
        Nko = 321,
        Prussian = 322,
        Kiche = 323,
        SouthernSami = 324,
        LuleSami = 325,
        InariSami = 326,
        SkoltSami = 327,
        Warlpiri = 328,
        ManichaeanMiddlePersian = 329,
        Mende = 330,
        AncientNorthArabian = 331,
        LinearA = 332,
        HmongNjua = 333,
        Ho = 334,
        Lezghian = 335,
        Bassa = 336,
        Mono = 337,
        TedimChin = 338,
        Maithili = 339,
        Ahom = 340,
        AmericanSignLanguage = 341,
        ArdhamagadhiPrakrit = 342,
        Bhojpuri = 343,
        HieroglyphicLuwian = 344,
        LiteraryChinese = 345,
        Mazanderani = 346,
        Mru = 347,
        Newari = 348,
        NorthernLuri = 349,
        Palauan = 350,
        Papiamento = 351,
        Saraiki = 352,
        TokelauLanguage = 353,
        TokPisin = 354,
        TuvaluLanguage = 355,
        UncodedLanguages = 356,
        Cantonese = 357,
        Osage = 358,
        Tangut = 359,
        Ido = 360,
        Lojban = 361,
        Sicilian = 362,
        SouthernKurdish = 363,
        WesternBalochi = 364,

        Afan = Oromo,
        Bhutani = Dzongkha,
        Byelorussian = Belarusian,
        Cambodian = Khmer,
        Chewa = Nyanja,
        Frisian = WesternFrisian,
        Kurundi = Rundi,
        Moldavian = Romanian,
        Norwegian = NorwegianBokmal,
        RhaetoRomance = Romansh,
        SerboCroatian = Serbian,
        Tagalog = Filipino,
        Twi = Akan,
        Uigur = Uighur,

        LastLanguage = WesternBalochi
    };

    enum Script {
        AnyScript = 0,
        ArabicScript = 1,
        CyrillicScript = 2,
        DeseretScript = 3,
        GurmukhiScript = 4,
        SimplifiedHanScript = 5,
        TraditionalHanScript = 6,
        LatinScript = 7,
        MongolianScript = 8,
        TifinaghScript = 9,
        ArmenianScript = 10,
        BengaliScript = 11,
        CherokeeScript = 12,
        DevanagariScript = 13,
        EthiopicScript = 14,
        GeorgianScript = 15,
        GreekScript = 16,
        GujaratiScript = 17,
        HebrewScript = 18,
        JapaneseScript = 19,
        KhmerScript = 20,
        KannadaScript = 21,
        KoreanScript = 22,
        LaoScript = 23,
        MalayalamScript = 24,
        MyanmarScript = 25,
        OriyaScript = 26,
        TamilScript = 27,
        TeluguScript = 28,
        ThaanaScript = 29,
        ThaiScript = 30,
        TibetanScript = 31,
        SinhalaScript = 32,
        SyriacScript = 33,
        YiScript = 34,
        VaiScript = 35,
        AvestanScript = 36,
        BalineseScript = 37,
        BamumScript = 38,
        BatakScript = 39,
        BopomofoScript = 40,
        BrahmiScript = 41,
        BugineseScript = 42,
        BuhidScript = 43,
        CanadianAboriginalScript = 44,
        CarianScript = 45,
        ChakmaScript = 46,
        ChamScript = 47,
        CopticScript = 48,
        CypriotScript = 49,
        EgyptianHieroglyphsScript = 50,
        FraserScript = 51,
        GlagoliticScript = 52,
        GothicScript = 53,
        HanScript = 54,
        HangulScript = 55,
        HanunooScript = 56,
        ImperialAramaicScript = 57,
        InscriptionalPahlaviScript = 58,
        InscriptionalParthianScript = 59,
        JavaneseScript = 60,
        KaithiScript = 61,
        KatakanaScript = 62,
        KayahLiScript = 63,
        KharoshthiScript = 64,
        LannaScript = 65,
        LepchaScript = 66,
        LimbuScript = 67,
        LinearBScript = 68,
        LycianScript = 69,
        LydianScript = 70,
        MandaeanScript = 71,
        MeiteiMayekScript = 72,
        MeroiticScript = 73,
        MeroiticCursiveScript = 74,
        NkoScript = 75,
        NewTaiLueScript = 76,
        OghamScript = 77,
        OlChikiScript = 78,
        OldItalicScript = 79,
        OldPersianScript = 80,
        OldSouthArabianScript = 81,
        OrkhonScript = 82,
        OsmanyaScript = 83,
        PhagsPaScript = 84,
        PhoenicianScript = 85,
        PollardPhoneticScript = 86,
        RejangScript = 87,
        RunicScript = 88,
        SamaritanScript = 89,
        SaurashtraScript = 90,
        SharadaScript = 91,
        ShavianScript = 92,
        SoraSompengScript = 93,
        CuneiformScript = 94,
        SundaneseScript = 95,
        SylotiNagriScript = 96,
        TagalogScript = 97,
        TagbanwaScript = 98,
        TaiLeScript = 99,
        TaiVietScript = 100,
        TakriScript = 101,
        UgariticScript = 102,
        BrailleScript = 103,
        HiraganaScript = 104,
        CaucasianAlbanianScript = 105,
        BassaVahScript = 106,
        DuployanScript = 107,
        ElbasanScript = 108,
        GranthaScript = 109,
        PahawhHmongScript = 110,
        KhojkiScript = 111,
        LinearAScript = 112,
        MahajaniScript = 113,
        ManichaeanScript = 114,
        MendeKikakuiScript = 115,
        ModiScript = 116,
        MroScript = 117,
        OldNorthArabianScript = 118,
        NabataeanScript = 119,
        PalmyreneScript = 120,
        PauCinHauScript = 121,
        OldPermicScript = 122,
        PsalterPahlaviScript = 123,
        SiddhamScript = 124,
        KhudawadiScript = 125,
        TirhutaScript = 126,
        VarangKshitiScript = 127,
        AhomScript = 128,
        AnatolianHieroglyphsScript = 129,
        HatranScript = 130,
        MultaniScript = 131,
        OldHungarianScript = 132,
        SignWritingScript = 133,
        AdlamScript = 134,
        BhaiksukiScript = 135,
        MarchenScript = 136,
        NewaScript = 137,
        OsageScript = 138,
        TangutScript = 139,
        HanWithBopomofoScript = 140,
        JamoScript = 141,

        SimplifiedChineseScript = SimplifiedHanScript,
        TraditionalChineseScript = TraditionalHanScript,

        LastScript = JamoScript
    };
    enum Country {
        AnyCountry = 0,
        Afghanistan = 1,
        Albania = 2,
        Algeria = 3,
        AmericanSamoa = 4,
        Andorra = 5,
        Angola = 6,
        Anguilla = 7,
        Antarctica = 8,
        AntiguaAndBarbuda = 9,
        Argentina = 10,
        Armenia = 11,
        Aruba = 12,
        Australia = 13,
        Austria = 14,
        Azerbaijan = 15,
        Bahamas = 16,
        Bahrain = 17,
        Bangladesh = 18,
        Barbados = 19,
        Belarus = 20,
        Belgium = 21,
        Belize = 22,
        Benin = 23,
        Bermuda = 24,
        Bhutan = 25,
        Bolivia = 26,
        BosniaAndHerzegowina = 27,
        Botswana = 28,
        BouvetIsland = 29,
        Brazil = 30,
        BritishIndianOceanTerritory = 31,
        Brunei = 32,
        Bulgaria = 33,
        BurkinaFaso = 34,
        Burundi = 35,
        Cambodia = 36,
        Cameroon = 37,
        Canada = 38,
        CapeVerde = 39,
        CaymanIslands = 40,
        CentralAfricanRepublic = 41,
        Chad = 42,
        Chile = 43,
        China = 44,
        ChristmasIsland = 45,
        CocosIslands = 46,
        Colombia = 47,
        Comoros = 48,
        CongoKinshasa = 49,
        CongoBrazzaville = 50,
        CookIslands = 51,
        CostaRica = 52,
        IvoryCoast = 53,
        Croatia = 54,
        Cuba = 55,
        Cyprus = 56,
        CzechRepublic = 57,
        Denmark = 58,
        Djibouti = 59,
        Dominica = 60,
        DominicanRepublic = 61,
        EastTimor = 62,
        Ecuador = 63,
        Egypt = 64,
        ElSalvador = 65,
        EquatorialGuinea = 66,
        Eritrea = 67,
        Estonia = 68,
        Ethiopia = 69,
        FalklandIslands = 70,
        FaroeIslands = 71,
        Fiji = 72,
        Finland = 73,
        France = 74,
        Guernsey = 75,
        FrenchGuiana = 76,
        FrenchPolynesia = 77,
        FrenchSouthernTerritories = 78,
        Gabon = 79,
        Gambia = 80,
        Georgia = 81,
        Germany = 82,
        Ghana = 83,
        Gibraltar = 84,
        Greece = 85,
        Greenland = 86,
        Grenada = 87,
        Guadeloupe = 88,
        Guam = 89,
        Guatemala = 90,
        Guinea = 91,
        GuineaBissau = 92,
        Guyana = 93,
        Haiti = 94,
        HeardAndMcDonaldIslands = 95,
        Honduras = 96,
        HongKong = 97,
        Hungary = 98,
        Iceland = 99,
        India = 100,
        Indonesia = 101,
        Iran = 102,
        Iraq = 103,
        Ireland = 104,
        Israel = 105,
        Italy = 106,
        Jamaica = 107,
        Japan = 108,
        Jordan = 109,
        Kazakhstan = 110,
        Kenya = 111,
        Kiribati = 112,
        NorthKorea = 113,
        SouthKorea = 114,
        Kuwait = 115,
        Kyrgyzstan = 116,
        Laos = 117,
        Latvia = 118,
        Lebanon = 119,
        Lesotho = 120,
        Liberia = 121,
        Libya = 122,
        Liechtenstein = 123,
        Lithuania = 124,
        Luxembourg = 125,
        Macau = 126,
        Macedonia = 127,
        Madagascar = 128,
        Malawi = 129,
        Malaysia = 130,
        Maldives = 131,
        Mali = 132,
        Malta = 133,
        MarshallIslands = 134,
        Martinique = 135,
        Mauritania = 136,
        Mauritius = 137,
        Mayotte = 138,
        Mexico = 139,
        Micronesia = 140,
        Moldova = 141,
        Monaco = 142,
        Mongolia = 143,
        Montserrat = 144,
        Morocco = 145,
        Mozambique = 146,
        Myanmar = 147,
        Namibia = 148,
        NauruCountry = 149,
        Nepal = 150,
        Netherlands = 151,
        CuraSao = 152,
        NewCaledonia = 153,
        NewZealand = 154,
        Nicaragua = 155,
        Niger = 156,
        Nigeria = 157,
        Niue = 158,
        NorfolkIsland = 159,
        NorthernMarianaIslands = 160,
        Norway = 161,
        Oman = 162,
        Pakistan = 163,
        Palau = 164,
        PalestinianTerritories = 165,
        Panama = 166,
        PapuaNewGuinea = 167,
        Paraguay = 168,
        Peru = 169,
        Philippines = 170,
        Pitcairn = 171,
        Poland = 172,
        Portugal = 173,
        PuertoRico = 174,
        Qatar = 175,
        Reunion = 176,
        Romania = 177,
        Russia = 178,
        Rwanda = 179,
        SaintKittsAndNevis = 180,
        SaintLucia = 181,
        SaintVincentAndTheGrenadines = 182,
        Samoa = 183,
        SanMarino = 184,
        SaoTomeAndPrincipe = 185,
        SaudiArabia = 186,
        Senegal = 187,
        Seychelles = 188,
        SierraLeone = 189,
        Singapore = 190,
        Slovakia = 191,
        Slovenia = 192,
        SolomonIslands = 193,
        Somalia = 194,
        SouthAfrica = 195,
        SouthGeorgiaAndTheSouthSandwichIslands = 196,
        Spain = 197,
        SriLanka = 198,
        SaintHelena = 199,
        SaintPierreAndMiquelon = 200,
        Sudan = 201,
        Suriname = 202,
        SvalbardAndJanMayenIslands = 203,
        Swaziland = 204,
        Sweden = 205,
        Switzerland = 206,
        Syria = 207,
        Taiwan = 208,
        Tajikistan = 209,
        Tanzania = 210,
        Thailand = 211,
        Togo = 212,
        TokelauCountry = 213,
        Tonga = 214,
        TrinidadAndTobago = 215,
        Tunisia = 216,
        Turkey = 217,
        Turkmenistan = 218,
        TurksAndCaicosIslands = 219,
        TuvaluCountry = 220,
        Uganda = 221,
        Ukraine = 222,
        UnitedArabEmirates = 223,
        UnitedKingdom = 224,
        UnitedStates = 225,
        UnitedStatesMinorOutlyingIslands = 226,
        Uruguay = 227,
        Uzbekistan = 228,
        Vanuatu = 229,
        VaticanCityState = 230,
        Venezuela = 231,
        Vietnam = 232,
        BritishVirginIslands = 233,
        UnitedStatesVirginIslands = 234,
        WallisAndFutunaIslands = 235,
        WesternSahara = 236,
        Yemen = 237,
        CanaryIslands = 238,
        Zambia = 239,
        Zimbabwe = 240,
        ClippertonIsland = 241,
        Montenegro = 242,
        Serbia = 243,
        SaintBarthelemy = 244,
        SaintMartin = 245,
        LatinAmerica = 246,
        AscensionIsland = 247,
        AlandIslands = 248,
        DiegoGarcia = 249,
        CeutaAndMelilla = 250,
        IsleOfMan = 251,
        Jersey = 252,
        TristanDaCunha = 253,
        SouthSudan = 254,
        Bonaire = 255,
        SintMaarten = 256,
        Kosovo = 257,
        EuropeanUnion = 258,
        OutlyingOceania = 259,
        World = 260,
        Europe = 261,

        DemocraticRepublicOfCongo = CongoKinshasa,
        DemocraticRepublicOfKorea = NorthKorea,
        LatinAmericaAndTheCaribbean = LatinAmerica,
        PeoplesRepublicOfCongo = CongoBrazzaville,
        RepublicOfKorea = SouthKorea,
        RussianFederation = Russia,
        SyrianArabRepublic = Syria,
        Tokelau = TokelauCountry,
        Tuvalu = TuvaluCountry,

        LastCountry = Europe
    };

    enum MeasurementSystem {
        MetricSystem,
        ImperialUSSystem,
        ImperialUKSystem,
        ImperialSystem = ImperialUSSystem // compatibility
    };

    enum FormatType { LongFormat, ShortFormat, NarrowFormat };
    enum NumberOption {
        DefaultNumberOptions = 0x0,
        OmitGroupSeparator = 0x01,
        RejectGroupSeparator = 0x02,
        OmitLeadingZeroInExponent = 0x04,
        RejectLeadingZeroInExponent = 0x08,
        IncludeTrailingZeroesAfterDot = 0x10,
        RejectTrailingZeroesAfterDot = 0x20
    };
    typedef uint NumberOptions;

    enum FloatingPointPrecisionOption {
        FloatingPointShortest = -128
    };

    enum CurrencySymbolFormat {
        CurrencyIsoCode,
        CurrencySymbol,
        CurrencyDisplayName
    };

    enum DataSizeFormat {
        // Single-bit values, for internal use.
        DataSizeBase1000 = 1, // use factors of 1000 instead of IEC's 1024;
        DataSizeSIQuantifiers = 2, // use SI quantifiers instead of IEC ones.

        // Flags values for use in API:
        DataSizeIecFormat = 0, // base 1024, KiB, MiB, GiB, ...
        DataSizeTraditionalFormat = DataSizeSIQuantifiers, // base 1024, kB, MB, GB, ...
        DataSizeSIFormat = DataSizeBase1000 | DataSizeSIQuantifiers // base 1000, kB, MB, GB, ...
    };
    typedef uint DataSizeFormats;

    iLocale();
    iLocale(const iString &name);
    iLocale(Language language, Country country = AnyCountry);
    iLocale(Language language, Script script, Country country);
    iLocale(const iLocale &other);
    iLocale &operator=(const iLocale &other);
    ~iLocale();

    void swap(iLocale &other) { std::swap(d, other.d); }

    Language language() const;
    Script script() const;
    Country country() const;
    iString name() const;

    iString bcp47Name() const;
    iString nativeLanguageName() const;
    iString nativeCountryName() const;

    short toShort(const iString &s, bool *ok = IX_NULLPTR) const;
    ushort toUShort(const iString &s, bool *ok = IX_NULLPTR) const;
    int toInt(const iString &s, bool *ok = IX_NULLPTR) const;
    uint toUInt(const iString &s, bool *ok = IX_NULLPTR) const;
    long toLong(const iString &s, bool *ok = IX_NULLPTR) const;
    ulong toULong(const iString &s, bool *ok = IX_NULLPTR) const;
    xlonglong toLongLong(const iString &s, bool *ok = IX_NULLPTR) const;
    xulonglong toULongLong(const iString &s, bool *ok = IX_NULLPTR) const;
    float toFloat(const iString &s, bool *ok = IX_NULLPTR) const;
    double toDouble(const iString &s, bool *ok = IX_NULLPTR) const;

    short toShort(iStringView s, bool *ok = IX_NULLPTR) const;
    ushort toUShort(iStringView s, bool *ok = IX_NULLPTR) const;
    int toInt(iStringView s, bool *ok = IX_NULLPTR) const;
    uint toUInt(iStringView s, bool *ok = IX_NULLPTR) const;
    long toLong(iStringView s, bool *ok = IX_NULLPTR) const;
    ulong toULong(iStringView s, bool *ok = IX_NULLPTR) const;
    xlonglong toLongLong(iStringView s, bool *ok = IX_NULLPTR) const;
    xulonglong toULongLong(iStringView s, bool *ok = IX_NULLPTR) const;
    float toFloat(iStringView s, bool *ok = IX_NULLPTR) const;
    double toDouble(iStringView s, bool *ok = IX_NULLPTR) const;

    iString toString(xlonglong i) const;
    iString toString(xulonglong i) const;
    inline iString toString(short i) const;
    inline iString toString(ushort i) const;
    inline iString toString(int i) const;
    inline iString toString(uint i) const;
    iString toString(double i, char f = 'g', int prec = 6) const;
    inline iString toString(float i, char f = 'g', int prec = 6) const;

    iString dateFormat(FormatType format = LongFormat) const;
    iString timeFormat(FormatType format = LongFormat) const;
    iString dateTimeFormat(FormatType format = LongFormat) const;

    // ### We need to return iString from these function since
    //     unicode data contains several characters for these fields.
    iChar decimalPoint() const;
    iChar groupSeparator() const;
    iChar percent() const;
    iChar zeroDigit() const;
    iChar negativeSign() const;
    iChar positiveSign() const;
    iChar exponential() const;

    iString monthName(int, FormatType format = LongFormat) const;
    iString standaloneMonthName(int, FormatType format = LongFormat) const;
    iString dayName(int, FormatType format = LongFormat) const;
    iString standaloneDayName(int, FormatType format = LongFormat) const;

    iString amText() const;
    iString pmText() const;

    MeasurementSystem measurementSystem() const;

    iShell::LayoutDirection textDirection() const;

    iString toUpper(const iString &str) const;
    iString toLower(const iString &str) const;

    iString currencySymbol(CurrencySymbolFormat = CurrencySymbol) const;
    iString toCurrencyString(xlonglong, const iString &symbol = iString()) const;
    iString toCurrencyString(xulonglong, const iString &symbol = iString()) const;
    inline iString toCurrencyString(short, const iString &symbol = iString()) const;
    inline iString toCurrencyString(ushort, const iString &symbol = iString()) const;
    inline iString toCurrencyString(int, const iString &symbol = iString()) const;
    inline iString toCurrencyString(uint, const iString &symbol = iString()) const;
    iString toCurrencyString(double, const iString &symbol = iString(), int precision = -1) const;
    inline iString toCurrencyString(float i, const iString &symbol = iString(), int precision = -1) const
    { return toCurrencyString(double(i), symbol, precision); }

    iString formattedDataSize(xint64 bytes, int precision = 2, DataSizeFormats format = DataSizeIecFormat) const;

    bool operator==(const iLocale &other) const;
    bool operator!=(const iLocale &other) const;

    static iString languageToString(Language language);
    static iString countryToString(Country country);
    static iString scriptToString(Script script);
    static void setDefault(const iLocale &locale);

    static iLocale c() { return iLocale(C); }
    static iLocale system();

    void setNumberOptions(NumberOptions options);
    NumberOptions numberOptions() const;

    iString createSeparatedList(const std::list<iString> &strl) const;

private:
    iLocale(iLocalePrivate &dd);
    friend class iLocalePrivate;

    iSharedDataPointer<iLocalePrivate> d;
};

inline iString iLocale::toString(short i) const
{ return toString(xlonglong(i)); }
inline iString iLocale::toString(ushort i) const
{ return toString(xulonglong(i)); }
inline iString iLocale::toString(int i) const
{ return toString(xlonglong(i)); }
inline iString iLocale::toString(uint i) const
{ return toString(xulonglong(i)); }
inline iString iLocale::toString(float i, char f, int prec) const
{ return toString(double(i), f, prec); }

inline iString iLocale::toCurrencyString(short i, const iString &symbol) const
{ return toCurrencyString(xlonglong(i), symbol); }
inline iString iLocale::toCurrencyString(ushort i, const iString &symbol) const
{ return toCurrencyString(xulonglong(i), symbol); }
inline iString iLocale::toCurrencyString(int i, const iString &symbol) const
{ return toCurrencyString(xlonglong(i), symbol); }
inline iString iLocale::toCurrencyString(uint i, const iString &symbol) const
{ return toCurrencyString(xulonglong(i), symbol); }

} // namespace iShell

#endif // ILOCALE_H
