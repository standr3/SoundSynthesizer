#include <iostream>
#include "olcNoiseMaker.h"
using namespace std;

double w(double dHertz)
{
    return dHertz * 2.0 * PI;
}

// General purpose oscillator
#define OSC_SINE 0
#define OSC_SQUARE 1
#define OSC_TRIANGLE 2
#define OSC_SAW_ANA 3
#define OSC_SAW_DIG 4
#define OSC_NOISE 5

double osc(double dHertz, double dTime, int nType = OSC_SINE)
{
    switch (nType)
    {
    case OSC_SINE: // Sine wave bewteen -1 and +1
        return sin(w(dHertz) * dTime);

    case OSC_SQUARE: // Square wave between -1 and +1
        return sin(w(dHertz) * dTime) > 0 ? 1.0 : -1.0;

    case OSC_TRIANGLE: // Triangle wave between -1 and +1
        return asin(sin(w(dHertz) * dTime)) * (2.0 / PI);

    case OSC_SAW_ANA: // Saw wave (analogue / warm / slow)
    {
        double dOutput = 0.0;

        for (double n = 1.0; n < 40.0; n++)
            dOutput += (sin(n * w(dHertz) * dTime)) / n;

        return dOutput * (2.0 / PI);
    }

    case OSC_SAW_DIG: // Saw Wave (optimised / harsh / fast)
        return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));


    case OSC_NOISE: // Pseudorandom noise
        return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;

    default:
        return 0.0;
    }
}

// Amplitude (Attack, Decay, Sustain, Release) Envelope
struct sEnvelopeADSR
{
    double dAttackTime;
    double dDecayTime;
    double dSustainAmplitude;
    double dReleaseTime;
    double dStartAmplitude;
    double dTriggerOffTime;
    double dTriggerOnTime;
    bool bNoteOn;

    sEnvelopeADSR()
    {
        dAttackTime = 0.10;
        dDecayTime = 0.01;
        dStartAmplitude = 1.0;
        dSustainAmplitude = 0.8;
        dReleaseTime = 0.20;
        bNoteOn = false;
        dTriggerOffTime = 0.0;
        dTriggerOnTime = 0.0;
    }

    // Call when key is pressed
    void NoteOn(double dTimeOn)
    {
        dTriggerOnTime = dTimeOn;
        bNoteOn = true;
    }

    // Call when key is released
    void NoteOff(double dTimeOff)
    {
        dTriggerOffTime = dTimeOff;
        bNoteOn = false;
    }

    // Get the correct amplitude at the requested point in time
    double GetAmplitude(double dTime)
    {
        double dAmplitude = 0.0;
        double dLifeTime = dTime - dTriggerOnTime;

        if (bNoteOn)
        {
            if (dLifeTime <= dAttackTime)
            {
                // In attack Phase - approach max amplitude
                dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;
            }

            if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
            {
                // In decay phase - reduce to sustained amplitude
                dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;
            }

            if (dLifeTime > (dAttackTime + dDecayTime))
            {
                // In sustain phase - dont change until note released
                dAmplitude = dSustainAmplitude;
            }
        }
        else
        {
            // Note has been released, so in release phase
            dAmplitude = ((dTime - dTriggerOffTime) / dReleaseTime) * (0.0 - dSustainAmplitude) + dSustainAmplitude;
        }

        // Amplitude should not be negative
        if (dAmplitude <= 0.0001)
            dAmplitude = 0.0;

        return dAmplitude;
    }
};


// Global synthesizer variables
atomic<double> dFrequencyOutput = 0.0;			// dominant output frequency of instrument, i.e. the note
sEnvelopeADSR envelope;							// amplitude modulation of output to give texture, i.e. the timbre
double dOctaveBaseFrequency = 110.0; // A2		// frequency of octave represented by keyboard
double d12thRootOf2 = pow(2.0, 1.0 / 12.0);		// assuming western 12 notes per ocatve

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
double MakeNoise(double dTime)
{
    // Mix together a little sine and square waves
    double dOutput = envelope.GetAmplitude(dTime) *
        (
            +1.0 * osc(dFrequencyOutput * 0.5, dTime, OSC_SINE)
            + 1.0 * osc(dFrequencyOutput, dTime, OSC_SINE)
            );

    return dOutput * 0.4; // Master Volume
}

int main()
{
    wcout << "Synthesizer" << endl;

    // Get all sound hardware
    vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

    // Display findings
    for (auto d : devices) wcout << "Found Output Device: " << d << endl;
    wcout << "Using Device: " << devices[0] << endl;

    // Display a keyboard
    wcout << endl <<
        "|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << endl <<
        "|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << endl <<
        "|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << endl <<
        "|     |     |     |     |     |     |     |     |     |     |" << endl <<
        "|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << endl <<
        "|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << endl << endl;

    // Create sound machine !!
    olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

    // Link noise function with sound machine
    sound.SetUserFunction(MakeNoise);


    double dOctaveBaseFrequency = 110.0;
    double d12thRootOf2 = pow(2.0, 1.0 / 12.0);

    // Sit in loop, capturing keyboard state changes and modify
    // synthesizer output accordingly
    int nCurrentKey = -1;
    bool bKeyPressed = false;
    while (1)
    {
        bKeyPressed = false;
        for (int k = 0; k < 16; k++)
        {
            if (GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k])) & 0x8000)
            {
                if (nCurrentKey != k)
                {
                    dFrequencyOutput = dOctaveBaseFrequency * pow(d12thRootOf2, k);
                    envelope.NoteOn(sound.GetTime());
                    wcout << "\rNote On : " << sound.GetTime() << "s " << dFrequencyOutput << "Hz";
                    nCurrentKey = k;
                }

                bKeyPressed = true;
            }
        }

        if (!bKeyPressed)
        {
            if (nCurrentKey != -1)
            {
                wcout << "\rNote Off: " << sound.GetTime() << "s                        ";
                envelope.NoteOff(sound.GetTime());
                nCurrentKey = -1;
            }
        }

        
    }

    return 0;

}
