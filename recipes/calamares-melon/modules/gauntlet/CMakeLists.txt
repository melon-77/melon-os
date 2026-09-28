# The melon gauntlet: a Calamares view module. 200 easy questions, a few seconds per question, a wrong
# answer sends you back 10. Calamares' Next button stays disabled until the last question is answered.
cmake_minimum_required(VERSION 3.16)
project(calamares-melon-gauntlet LANGUAGES CXX)
find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
# Calamares' exported target links KF6::CoreAddons, so that has to be known first
find_package(KF6 REQUIRED COMPONENTS CoreAddons)
find_package(Calamares 3.3 REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
calamares_add_plugin(gauntlet
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES GauntletViewStep.cpp
    RESOURCES gauntlet.qrc
    SHARED_LIB
)
