#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTimer>
#include "GauntletViewStep.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Variant.h"

void
GauntletConfig::setProgress( int p )
{
    if ( p < 0 )
    {
        p = 0;
    }
    if ( p != m_progress )
    {
        m_progress = p;
        emit progressChanged();
    }
}

void
GauntletConfig::setPassed( bool p )
{
    if ( p == m_passed )
    {
        return;
    }
    m_passed = p;
    if ( auto* gs = Calamares::JobQueue::instance() ? Calamares::JobQueue::instance()->globalStorage() : nullptr )
    {
        // the finishing job reads this to hand out the rewards
        gs->insert( QStringLiteral( "melonGauntletPassed" ), p );
        gs->insert( QStringLiteral( "melonGauntletMistakes" ), m_mistakes );
    }
    emit passedChanged( p );
}

void
GauntletConfig::setAlpine( bool a )
{
    if ( a == m_alpine )
    {
        return;
    }
    m_alpine = a;
    if ( auto* gs = Calamares::JobQueue::instance() ? Calamares::JobQueue::instance()->globalStorage() : nullptr )
    {
        // read by the contextualprocess job "melon-alpine" (opt-in @alpine repo)
        gs->insert( QStringLiteral( "melonAlpine" ), a ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) );
    }
    emit alpineChanged();
}

void
GauntletConfig::setNoRust( bool r )
{
    if ( r == m_noRust )
    {
        return;
    }
    m_noRust = r;
    // cal-finish removes everything built with Rust (melon-remove-rust) and hands out its reward when this file exists
    QDir().mkpath( QStringLiteral( "/run/melon" ) );
    QFile f( QStringLiteral( "/run/melon/.remove-rust" ) );
    if ( r )
    {
        f.open( QIODevice::WriteOnly );
    }
    else
    {
        f.remove();
    }
    emit noRustChanged();
}

void
GauntletConfig::setNiri( bool n )
{
    if ( n == m_niri )
    {
        return;
    }
    m_niri = n;
    // cal-finish installs the niri profile (niri + Noctalia from the online repository) instead of Plasma
    QDir().mkpath( QStringLiteral( "/run/melon" ) );
    QFile f( QStringLiteral( "/run/melon/.desktop-niri" ) );
    if ( n )
    {
        f.open( QIODevice::WriteOnly );
    }
    else
    {
        f.remove();
    }
    emit niriChanged();
}

void
GauntletConfig::checkNiri()
{
    auto setState = [this]( int s )
    {
        m_niriState = s;
        if ( s != 1 )
        {
            setNiri( false );
        }
        emit niriStateChanged();
    };
    if ( !QFile::exists( QStringLiteral( "/usr/share/melon/profiles/niri" ) ) )
    {
        setState( 3 );
        return;
    }
    setState( 0 );
    // melon-profile-online (melon-base): the online repository answers and has every package of the niri profile
    auto* p = new QProcess( this );
    connect( p,
             qOverload< int, QProcess::ExitStatus >( &QProcess::finished ),
             this,
             [p, setState]( int code, QProcess::ExitStatus status )
             {
                 setState( status == QProcess::NormalExit && code == 0 ? 1 : 2 );
                 p->deleteLater();
             } );
    // a helper that can't start never sends finished()
    connect( p,
             &QProcess::errorOccurred,
             this,
             [p, setState]( QProcess::ProcessError e )
             {
                 if ( e == QProcess::FailedToStart )
                 {
                     setState( 2 );
                     p->deleteLater();
                 }
             } );
    p->start( QStringLiteral( "/usr/libexec/melon/melon-profile-online" ), { QStringLiteral( "niri" ) } );
}

void
GauntletConfig::recordMistake()
{
    ++m_mistakes;
    emit progressChanged();
}

GauntletViewStep::GauntletViewStep( QObject* parent )
    : Calamares::QmlViewStep( parent )
    , m_config( new GauntletConfig( this ) )
{
    connect( m_config, &GauntletConfig::passedChanged, this, &GauntletViewStep::nextStatusChanged );
    // a choice from an earlier Calamares run in this live session must not outlive its unticked checkbox
    QFile::remove( QStringLiteral( "/run/melon/.remove-rust" ) );
    QFile::remove( QStringLiteral( "/run/melon/.desktop-niri" ) );
    // a hidden command can let the owner through without answering (to look at the rest of the installer):
    // it leaves this file behind, and Next unlocks as soon as it appears. No rewards then (see cal-finish).
    auto* skipCheck = new QTimer( this );
    connect( skipCheck, &QTimer::timeout, this, [this]() { if ( skipped() ) emit nextStatusChanged( true ); } );
    skipCheck->start( 1000 );
}

bool
GauntletViewStep::skipped() const
{
    return QFile::exists( QStringLiteral( "/run/melon/.gauntlet-skip" ) );
}

GauntletViewStep::~GauntletViewStep() {}

QString
GauntletViewStep::prettyName() const
{
    return tr( "Gauntlet" );
}

bool
GauntletViewStep::isNextEnabled() const
{
    return m_config->passed() || skipped();
}

void
GauntletConfig::setTrialPassed( bool t )
{
    if ( t == m_trialPassed )
    {
        return;
    }
    m_trialPassed = t;
    if ( t )
    {
        // cal-finish hands out the survivor rewards only when this file exists
        QDir().mkpath( QStringLiteral( "/run/melon" ) );
        QFile f( QStringLiteral( "/run/melon/.trial-passed" ) );
        if ( f.open( QIODevice::WriteOnly ) )
        {
            f.write( QByteArray::number( m_mistakes ) + '\n' );
        }
    }
    emit trialPassedChanged();
}

bool
GauntletViewStep::isBackEnabled() const
{
    // going back to the earlier pages is fine; progress is kept
    return true;
}

void
GauntletViewStep::setConfigurationMap( const QVariantMap& map )
{
    m_config->configure( Calamares::getInteger( map, "delay", 3 ), Calamares::getInteger( map, "setback", 10 ) );
    Calamares::QmlViewStep::setConfigurationMap( map );  // parent last: it loads the QML
}

QObject*
GauntletViewStep::getConfig()
{
    return m_config;
}

CALAMARES_PLUGIN_FACTORY_DEFINITION( GauntletViewStepFactory, registerPlugin< GauntletViewStep >(); )
