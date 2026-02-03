#include "SignalPatternLibrary.h"

static SignalPattern makeBeacon()
{
    return {
        {
            {2.0f, true},
            {4.0f, false}
        },
        true
    };
}

static SignalPattern makeTransponder()
{
    return {
        {
            {1.1f, true},
            {1.1f, false},
            {1.1f, true},
            {6.0f, false}
        },
        true
    };
}

static SignalPattern makeSOS()
{
    return {
        {
            {3.3f, true}, {1.0f, false},
            {3.3f, true}, {1.0f, false},
            {3.3f, true}, {1.0f, false},

            {1.3f, true}, {1.0f, false},
            {1.3f, true}, {1.0f, false},
            {1.3f, true}, {1.0f, false},

            {3.3f, true}, {1.0f, false},
            {3.3f, true}, {1.0f, false},
            {3.3f, true}, {1.0f, false},
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
        case SignalType::Beacon:        return beacon;
        case SignalType::Transponder:   return transponder;
        case SignalType::SOSModern:     return sos;
        case SignalType::SOSAntic:    
        case SignalType::Planets:
        case SignalType::StationClass:  return constant;
        default:                        return beacon;
    }
}
