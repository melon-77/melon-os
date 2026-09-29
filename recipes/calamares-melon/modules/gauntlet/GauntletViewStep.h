// The melon gauntlet view step for Calamares.
#ifndef MELON_GAUNTLETVIEWSTEP_H
#define MELON_GAUNTLETVIEWSTEP_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/QmlViewStep.h"

#include <QObject>

/// State shared with gauntlet.qml (as the context property "config").
class GauntletConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY( int progress READ progress WRITE setProgress NOTIFY progressChanged )
    Q_PROPERTY( bool passed READ passed WRITE setPassed NOTIFY passedChanged )
    Q_PROPERTY( int delay READ delay CONSTANT )
    Q_PROPERTY( int setback READ setback CONSTANT )
    Q_PROPERTY( int mistakes READ mistakes NOTIFY progressChanged )
    Q_PROPERTY( bool alpine READ alpine WRITE setAlpine NOTIFY alpineChanged )
    Q_PROPERTY( bool noRust READ noRust WRITE setNoRust NOTIFY noRustChanged )
    Q_PROPERTY( bool trialPassed READ trialPassed WRITE setTrialPassed NOTIFY trialPassedChanged )
    Q_PROPERTY( bool niri READ niri WRITE setNiri NOTIFY niriChanged )
    Q_PROPERTY( int niriState READ niriState NOTIFY niriStateChanged )

public:
    explicit GauntletConfig( QObject* parent = nullptr ) : QObject( parent ) {}

    int progress() const { return m_progress; }
    bool passed() const { return m_passed; }
    int delay() const { return m_delay; }
    int setback() const { return m_setback; }
    int mistakes() const { return m_mistakes; }
    bool alpine() const { return m_alpine; }
    bool noRust() const { return m_noRust; }
    bool trialPassed() const { return m_trialPassed; }
    bool niri() const { return m_niri; }
    /// the niri desktop comes from the online repository: 0 checking, 1 available, 2 unreachable, 3 not on this ISO
    int niriState() const { return m_niriState; }
    void setNiri( bool n );
    Q_INVOKABLE void checkNiri();
    void setTrialPassed( bool t );
    void setAlpine( bool a );
    void setNoRust( bool r );

    void setProgress( int p );
    void setPassed( bool p );
    void configure( int delay, int setback )
    {
        m_delay = delay;
        m_setback = setback;
    }

    Q_INVOKABLE void recordMistake();

signals:
    void progressChanged();
    void passedChanged( bool );
    void alpineChanged();
    void noRustChanged();
    void trialPassedChanged();
    void niriChanged();
    void niriStateChanged();

private:
    int m_progress = 0;
    bool m_passed = false;
    int m_delay = 3;
    int m_setback = 10;
    int m_mistakes = 0;
    bool m_alpine = false;
    bool m_noRust = false;
    bool m_trialPassed = false;
    bool m_niri = false;
    int m_niriState = 0;
};

class PLUGINDLLEXPORT GauntletViewStep : public Calamares::QmlViewStep
{
    Q_OBJECT

public:
    explicit GauntletViewStep( QObject* parent = nullptr );
    ~GauntletViewStep() override;

    QString prettyName() const override;
    bool skipped() const;
    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    void setConfigurationMap( const QVariantMap& configurationMap ) override;
    QObject* getConfig() override;

private:
    GauntletConfig* m_config;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION( GauntletViewStepFactory )

#endif
