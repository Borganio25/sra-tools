/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include "sra-info.hpp"

#include <functional>

#include <klib/out.h>
#include <klib/log.h>
#include <klib/rc.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/args-conv.h>

#include "formatter.hpp"


using namespace std;
#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = nullptr; } while (false)

#define OPTION_PLATFORM     "platform"
#define OPTION_FORMAT       "format"
#define OPTION_ISALIGNED    "is-aligned"
#define OPTION_QUALITY      "quality"
#define OPTION_SPOTLAYOUT   "spot-layout"
#define OPTION_LIMIT        "limit"
#define OPTION_DETAIL       "detail"
#define OPTION_SEQUENCE     "sequence"

#define ALIAS_PLATFORM      "P"
#define ALIAS_FORMAT        "f"
#define ALIAS_ISALIGNED     "A"
#define ALIAS_QUALITY       "Q"
#define ALIAS_SPOTLAYOUT    "S"
#define ALIAS_LIMIT         "l"
#define ALIAS_DETAIL        "D"
#define ALIAS_SEQUENCE      "s"

static const char * platform_usage[]    = { "print platform(s)", nullptr };
static const char * format_usage[]      = { "output format:", nullptr };
static const char * isaligned_usage[]   = { "is data aligned", nullptr };
static const char * quality_usage[]     = { "are quality scores stored or generated", nullptr };
static const char * spot_layout_usage[] = { "print spot layout(s). Uses CONSENSUS table if present, SEQUENCE table otherwise", nullptr };
static const char * limit_usage[]       = { "limit output to <N> elements, e.g. <N> most popular spot layouts; <N> must be positive", nullptr };
static const char * detail_usage[]      = { "detail level, <0> the least detailed output; <N> must be 0 or greater", nullptr };
static const char * sequence_usage[]    = { "use SEQUENCE table for spot layouts, even if CONSENSUS table is present", nullptr };

OptDef InfoOptions[] =
{
    { OPTION_PLATFORM,      ALIAS_PLATFORM,     nullptr, platform_usage,    1, false,   false, nullptr },
    { OPTION_FORMAT,        ALIAS_FORMAT,       nullptr, format_usage,      1, true,    false, nullptr },
    { OPTION_ISALIGNED,     ALIAS_ISALIGNED,    nullptr, isaligned_usage,   1, false,   false, nullptr },
    { OPTION_QUALITY,       ALIAS_QUALITY,      nullptr, quality_usage,     1, false,   false, nullptr },
    { OPTION_SPOTLAYOUT,    ALIAS_SPOTLAYOUT,   nullptr, spot_layout_usage, 1, false,   false, nullptr },
    { OPTION_LIMIT,         ALIAS_LIMIT,        nullptr, limit_usage,       1, true,    false, nullptr },
    { OPTION_DETAIL,        ALIAS_DETAIL,       nullptr, detail_usage,      1, true,    false, nullptr },
    { OPTION_SEQUENCE,      ALIAS_SEQUENCE,     nullptr, sequence_usage,    1, false,   false, nullptr },
};

const char UsageDefaultName[] = "sra-info";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <accession> [options]\n"
                    "\n", progname);
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == nullptr )
    {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    }
    else
    {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }

    if ( rc )
    {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );

    HelpOptionLine ( ALIAS_PLATFORM,    OPTION_PLATFORM,    nullptr, platform_usage );
    HelpOptionLine ( ALIAS_QUALITY,     OPTION_QUALITY,     nullptr, quality_usage );
    HelpOptionLine ( ALIAS_ISALIGNED,   OPTION_ISALIGNED,   nullptr, isaligned_usage );
    HelpOptionLine ( ALIAS_SPOTLAYOUT,  OPTION_SPOTLAYOUT,  nullptr, spot_layout_usage );

    HelpOptionLine ( ALIAS_FORMAT,   OPTION_FORMAT,     "format",   format_usage );
    KOutMsg( "      csv ..... comma separated values on one line\n" );
    KOutMsg( "      xml ..... xml-style without complete xml-frame\n" );
    KOutMsg( "      json .... json-style\n" );
    KOutMsg( "      tab ..... tab-separated values on one line\n" );

    HelpOptionLine ( ALIAS_LIMIT,  OPTION_LIMIT, "N", limit_usage );
    HelpOptionLine ( ALIAS_DETAIL, OPTION_DETAIL, "N", detail_usage );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

static
void
Output( const string & text )
{
    KOutMsg ( "%s\n", text.c_str() );
}

int
GetNumber( Args * args, const char * option, std::function<bool(int)> condition )
{
    const char* res = nullptr;
    rc_t rc = ArgsOptionValue( args, option, 0, ( const void** )&res );
    DISP_RC( rc, "ArgsOptionValue() failed" );

    try
    {
        int d = std::stoi(res);
        if ( ! condition( d ) )
        {
            throw VDB::Error("");
        }
        return d;
    }
    catch(...)
    {
        throw VDB::Error("");
    }
}

unsigned int
GetPositiveNumber( Args * args, const char * option )
{
    try
    {
        return GetNumber( args, option, [](int x) -> bool { return x > 0; } );
    }
    catch(...)
    {
        throw VDB::Error( string("invalid value for --") + option + "(not a positive number)");
    }
}
unsigned int
GetNonNegativeNumber( Args * args, const char * option )
{
    try
    {
        return GetNumber( args, option, [](int x) -> bool { return x >= 0; } );
    }
    catch(...)
    {
        throw VDB::Error( string("invalid value for --") + option + "(not a non-negative number)");
    }
}

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle( &args, argc, argv,
        1, InfoOptions, sizeof InfoOptions / sizeof InfoOptions [ 0 ] );
    DISP_RC( rc, "ArgsMakeAndHandle() failed" );
    if ( rc == 0)
    {
        SraInfo info;

        uint32_t param_count = 0;
        rc = ArgsParamCount(args, &param_count);
        DISP_RC( rc, "ArgsParamCount() failed" );
        if ( rc == 0 )
        {
            if ( param_count == 0 || param_count > 1 ) {
                MiniUsage(args);
                DESTRUCT(Args, args);
                exit(1);
            }

            const char * accession = nullptr;
            rc = ArgsParamValue( args, 0, ( const void ** )&( accession ) );
            DISP_RC( rc, "ArgsParamValue() failed" );

            try
            {
                info.SetAccession( accession );

                uint32_t opt_count;
                uint32_t limit = 0;

                rc = ArgsOptionCount( args, OPTION_LIMIT, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    limit = GetPositiveNumber( args, OPTION_LIMIT );
                }

                // formatting
                Formatter::Format fmt = Formatter::Default;
                rc = ArgsOptionCount( args, OPTION_FORMAT, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    const char* res = nullptr;
                    rc = ArgsOptionValue( args, OPTION_FORMAT, 0, ( const void** )&res );
                    fmt = Formatter::StringToFormat( res );
                }
                Formatter formatter( fmt, limit );

                rc = ArgsOptionCount( args, OPTION_PLATFORM, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    Output( formatter.format( info.GetPlatforms() ) );
                }

                rc = ArgsOptionCount( args, OPTION_ISALIGNED, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    Output( formatter.format( info.IsAligned() ? "ALIGNED" : "UNALIGNED" ) );
                }

                rc = ArgsOptionCount( args, OPTION_QUALITY, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    Output( formatter.format( info.HasPhysicalQualities() ? "STORED" : "GENERATED" ) );
                }

                rc = ArgsOptionCount( args, OPTION_SPOTLAYOUT, &opt_count );
                DISP_RC( rc, "ArgsOptionCount() failed" );
                if ( opt_count > 0 )
                {
                    SraInfo::Detail detail = SraInfo::Verbose;

                    // detail level
                    rc = ArgsOptionCount( args, OPTION_DETAIL, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                    {
                        switch( GetNonNegativeNumber( args, OPTION_DETAIL ) )
                        {
                        case 0: detail = SraInfo::Short; break;
                        case 1: detail = SraInfo::Abbreviated; break;
                        case 2: detail = SraInfo::Full; break;
                        default: break; // anything higher than 2 is Verbose
                        }
                    }

                    bool useConsensus = true;
                    rc = ArgsOptionCount( args, OPTION_SEQUENCE, &opt_count );
                    DISP_RC( rc, "ArgsOptionCount() failed" );
                    if ( opt_count > 0 )
                    {
                        useConsensus = false;
                    }
                    Output ( formatter.format( info.GetSpotLayouts( detail, useConsensus ), detail ) );
                }

            }
            catch( const exception& ex )
            {
                //KOutMsg( "%s\n", ex.what() ); ? - should be in stderr already, at least for VDB::Error
                rc = 3;
            }
        }

        DESTRUCT(Args, args);
    }

    return rc;
}
