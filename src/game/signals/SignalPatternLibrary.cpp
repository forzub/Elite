#include "SignalPatternLibrary.h"

static SignalPattern makeBeacon()
{
    return {
        {
            {0.2f, true},
            {0.8f, false}
        },
        true
    };
}

static SignalPattern makeTransponder()
{
    return {
        {
            {0.1f, true},
            {0.1f, false},
            {0.1f, true},
            {0.6f, false}
        },
        true
    };
}

static SignalPattern makeSOS()
{
    return {
        {
            {3.3f, true}, {3.3f, false},
            {3.3f, true}, {3.3f, false},
            {3.3f, true}, {3.6f, false},
        },
        true
    };
}

static SignalPattern makeConstant()
{
    return {
        {
            {1.0f, true}
        },
        true
    };
}

const SignalPattern& getDefaultSignalPattern(SignalType type)
{
    static SignalPattern beacon      = makeBeacon();
    static SignalPattern transponder = makeTransponder();
    static SignalPattern sos          = makeSOS();
    static SignalPattern constant    = makeConstant();

    switch (type)
    {
        case SignalType::Beacon:       return beacon;
        case SignalType::Transponder:  return transponder;
        case SignalType::SOSModern:
        case SignalType::SOSAntic:     return sos;
        case SignalType::Planets:
        case SignalType::StationClass: return constant;
        default:                       return beacon;
    }
}
